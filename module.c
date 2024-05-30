/*
    Copyright (C) 2024 Loophole Labs

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/xarray.h>
#include <linux/percpu_counter.h>

#include <asm/io.h>

#include "module.h"
#include "common.h"
#include "hashtable.h"
#include "log.h"

MODULE_AUTHOR("Loophole Labs (Shivansh Vij)");
MODULE_DESCRIPTION("Batch Syscalls");
MODULE_LICENSE("GPL");

static struct hashtable *mem_overlays;

static void cleanup_mem_overlay_segments(struct xarray segments)
{
	unsigned long i;
	struct mem_overlay_segment *seg;
	xa_for_each(&segments, i, seg) {
		kvfree(seg);
	}
	xa_destroy(&segments);
}

static void cleanup_mem_overlay(void *data)
{
	struct mem_overlay *mem_overlay = (struct mem_overlay *)data;

	// Revert base VMA vm_ops to its original value in case the VMA is used
	// after cleanup (usually to call ->close() on unmap).
	// TODO: support per-VMA locking (https://lwn.net/Articles/937943/).
	mmap_write_lock(current->mm);
	mem_overlay->base_vma->vm_ops = mem_overlay->original_vm_ops;
	kvfree(mem_overlay->hijacked_vm_ops);
	mmap_write_unlock(current->mm);

	cleanup_mem_overlay_segments(mem_overlay->segments);
	kvfree(mem_overlay);
}

static vm_fault_t hijacked_map_pages(struct vm_fault *vmf, pgoff_t start_pgoff,
				     pgoff_t end_pgoff)
{
	void *id = vmf->vma->vm_private_data;
	log_debug("page fault page=%lu start=%lu end=%lu uuid=%pUB ",
		  vmf->pgoff, start_pgoff, end_pgoff, id);

	struct mem_overlay *mem_overlay = hashtable_lookup(mem_overlays, id);
	if (!mem_overlay) {
		log_error("unable to find memory overlay uuid=%pUB", id);
		return VM_FAULT_SIGBUS;
	}

	spin_lock(&mem_overlay->vma_file_lock);

	XA_STATE(xas, &mem_overlay->segments, start_pgoff);
	struct file *base_vm_file = vmf->vma->vm_file;
	struct mem_overlay_segment *seg;
	vm_fault_t ret;
	pgoff_t end;

	rcu_read_lock();
	for (pgoff_t start = start_pgoff; start <= end_pgoff; start = end + 1) {
		do {
			seg = xas_find(&xas, end_pgoff);
		} while (xas_retry(&xas, seg));

		// The range doesn't overlap with any segment, so handle it like a
		// normal page fault.
		if (seg == NULL) {
			end = end_pgoff;
			log_debug(
				"handling base page fault start=%lu end=%lu uuid=%pUB",
				start, end, id);

			ret = filemap_map_pages(vmf, start, end);
			break;
		}

		// Handle any non-overlay range before the next segment.
		if (start < seg->start_pgoff) {
			end = seg->start_pgoff - 1;
			log_debug(
				"handling base page fault start=%lu end=%lu uuid=%pUB",
				start, end, id);

			ret = filemap_map_pages(vmf, start, end);
			if (ret & VM_FAULT_ERROR)
				break;
		}

		// Handle fault over overlay range.
		start = seg->start_pgoff;
		end = min(seg->end_pgoff, end_pgoff);
		log_debug(
			"handling overlay page fault start=%lu end=%lu uuid=%pUB",
			start, end, id);

		// TODO: acquire write lock on vmf->vma->vm_mm.
		// A read lock may have already been acquired.
		// https://docs.kernel.org/filesystems/locking.html#vm-operations-struct
		vmf->vma->vm_file = seg->overlay_vma->vm_file;
		ret = filemap_map_pages(vmf, start, end);
		vmf->vma->vm_file = base_vm_file;
		if (ret & VM_FAULT_ERROR)
			break;
	}
	rcu_read_unlock();
	spin_unlock(&mem_overlay->vma_file_lock);
	return ret;
}

static int device_open(struct inode *device_file, struct file *instance)
{
	log_debug("called device_open");
	log_info("device opened");
	return 0;
}

static int device_close(struct inode *device_file, struct file *instance)
{
	log_debug("called device_close");
	log_info("device closed");
	return 0;
}

static long int unlocked_ioctl_handle_mem_overlay_req(unsigned long arg)
{
	long int res = 0;

	// Read request data from userspace.
	struct mem_overlay_req req;
	unsigned long ret = copy_from_user(&req, (struct mem_overlay_req *)arg,
					   sizeof(struct mem_overlay_req));
	if (ret) {
		log_error(
			"failed to copy memory overlay request from user: %lu",
			ret);
		return -EFAULT;
	}

	// Acquire mm write lock since we expect to mutate the base VMA.
	// TODO: support per-VMA locking (https://lwn.net/Articles/937943/).
	mmap_write_lock(current->mm);

	// Find overlay VMA to store it into each segment.
	struct vm_area_struct *overlay_vma =
		find_vma(current->mm, req.overlay_addr);
	if (overlay_vma == NULL || overlay_vma->vm_start > req.overlay_addr) {
		log_error("failed to find overlay VMA");
		res = -EFAULT;
		goto out;
	}

	// Find base VMA and validate we can use it.
	struct vm_area_struct *base_vma = find_vma(current->mm, req.base_addr);
	if (base_vma == NULL || base_vma->vm_start > req.base_addr) {
		log_error("failed to find base VMA");
		res = -EFAULT;
		goto out;
	}

	if (base_vma->vm_private_data != NULL) {
		log_crit("base VMA private data already set");
		res = -EFAULT;
		goto out;
	}

	// Read overlay segments from request.
	struct mem_overlay_segment_req *segs =
		kvzalloc(sizeof(struct mem_overlay_segment) * req.segments_size,
			 GFP_KERNEL);
	if (!segs) {
		log_error("failed to allocate segments");
		res = -ENOMEM;
		goto out;
	}
	ret = copy_from_user(segs, req.segments,
			     sizeof(struct mem_overlay_segment_req) *
				     req.segments_size);
	if (ret) {
		log_error(
			"failed to copy memory overlay segments request from user: %lu",
			ret);
		res = -EFAULT;
		goto free_segs;
	}

	log_debug(
		"received memory overlay request base_addr=%lu overlay_addr=%lu",
		req.base_addr, req.overlay_addr);

	// Create new memory overlay instance.
	struct mem_overlay *mem_overlay =
		kvzalloc(sizeof(struct mem_overlay), GFP_KERNEL);
	if (!mem_overlay) {
		log_error("failed to allocate memory for memory overlay");
		res = -ENOMEM;
		goto free_segs;
	}

	mem_overlay->base_addr = req.base_addr;
	spin_lock_init(&mem_overlay->vma_file_lock);
	xa_init(&(mem_overlay->segments));

	struct mem_overlay_segment *seg;
	for (int i = 0; i < req.segments_size; i++) {
		unsigned long start = segs[i].start_pgoff;
		unsigned long end = segs[i].end_pgoff;

		seg = kvzalloc(sizeof(struct mem_overlay_segment), GFP_KERNEL);
		if (!seg) {
			log_error(
				"failed to allocate memory for memory overlay segment start=%lu end=%lu",
				start, end);
			res = -ENOMEM;
			goto cleanup_segments;
		}

		seg->start_pgoff = start;
		seg->end_pgoff = end;
		seg->overlay_addr = req.overlay_addr;
		seg->overlay_vma = overlay_vma;

		log_debug("inserting segment to overlay start=%lu end=%lu",
			  start, end);
		xa_store_range(&mem_overlay->segments, start, end, seg,
			       GFP_KERNEL);
	}

	// Generate request UUID and send it back to userspace so it can be used
	// in future user commands to reference this memory overlay instance.
	generate_random_uuid(mem_overlay->id);
	memcpy(req.id, mem_overlay->id, sizeof(unsigned char) * UUID_SIZE);
	ret = copy_to_user((struct mem_overlay_req *)arg, &req,
			   sizeof(struct mem_overlay_req));
	if (ret) {
		log_error("failed to copy memory overlay ID to user: %lu", ret);
		res = -EFAULT;
		goto cleanup_segments;
	}

	// Hijack page fault handler for base VMA.
	log_info("hijacking vm_ops for base VMA addr=0x%lu", req.base_addr);
	mem_overlay->hijacked_vm_ops =
		kvzalloc(sizeof(struct vm_operations_struct), GFP_KERNEL);
	if (!mem_overlay->hijacked_vm_ops) {
		log_error("failed to allocate memory for hijacked vm_ops");
		res = -ENOMEM;
		goto cleanup_segments;
	}

	// Store base VMA and original vm_ops so we can restore it on cleanup.
	mem_overlay->base_vma = base_vma;
	mem_overlay->original_vm_ops = base_vma->vm_ops;

	memcpy(mem_overlay->hijacked_vm_ops, base_vma->vm_ops,
	       sizeof(struct vm_operations_struct));
	mem_overlay->hijacked_vm_ops->map_pages = hijacked_map_pages;
	base_vma->vm_ops = mem_overlay->hijacked_vm_ops;
	log_info("done hijacking vm_ops addr=0x%lu", req.base_addr);

	// Store memory overlay ID on base VMA private data so we can retrieve it
	// when handling a page fault.
	base_vma->vm_private_data = mem_overlay->id;

	// Save memory overlay into hashtable.
	int iret = hashtable_insert(mem_overlays, mem_overlay->id, mem_overlay);
	if (iret) {
		log_error("failed to insert memory overlay into hashtable: %d",
			  iret);
		res = -EFAULT;
		goto revert_vm_ops;
	}

	log_info("memory overlay created successfully uuid=%pUB",
		 mem_overlay->id);
	goto out;

revert_vm_ops:
	base_vma->vm_ops = mem_overlay->original_vm_ops;
	mmap_write_unlock(current->mm);
	kvfree(mem_overlay->hijacked_vm_ops);
cleanup_segments:
	cleanup_mem_overlay_segments(mem_overlay->segments);
	kvfree(mem_overlay);
free_segs:
	kvfree(segs);
out:
	mmap_write_unlock(current->mm);
	return res;
}

static long int unlocked_ioctl_handle_mem_overlay_cleanup_req(unsigned long arg)
{
	struct mem_overlay_cleanup_req req;
	unsigned long ret =
		copy_from_user(&req, (struct mem_overlay_cleanup_req *)arg,
			       sizeof(struct mem_overlay_cleanup_req));
	if (ret) {
		log_error(
			"failed to copy memory overlay cleanup request from user: %lu",
			ret);
		return -EFAULT;
	}

	struct mem_overlay *mem_overlay =
		hashtable_delete(mem_overlays, req.id);
	if (!mem_overlay) {
		log_error("failed to cleanup memory overlay uuid=%pUb", req.id);
		return -EFAULT;
	}
	cleanup_mem_overlay(mem_overlay);
	return 0;
}

static long int unlocked_ioctl(struct file *file, unsigned cmd,
			       unsigned long arg)
{
	switch (cmd) {
	case IOCTL_MEM_OVERLAY_REQ_CMD:
		log_debug("called IOCTL_MEM_OVERLAY_REQ_CMD");
		return unlocked_ioctl_handle_mem_overlay_req(arg);
	case IOCTL_MEM_OVERLAY_CLEANUP_CMD:
		log_debug("called IOCTL_MEM_OVERLAY_CLEANUP_CMD");
		return unlocked_ioctl_handle_mem_overlay_cleanup_req(arg);
	default:
		log_error("unknown ioctl cmd %x", cmd);
	}
	return -EINVAL;
}

static struct file_operations file_ops = { .owner = THIS_MODULE,
					   .open = device_open,
					   .release = device_close,
					   .unlocked_ioctl = unlocked_ioctl };

static unsigned int major;
static dev_t device_number;
static struct class *device_class;

static int __init init_mod(void)
{
	log_debug("called init_module");

	mem_overlays = hashtable_setup(&cleanup_mem_overlay);

	log_info("registering device with major %u and ID '%s'",
		 (unsigned int)MAJOR_DEV, DEVICE_ID);
	int ret = register_chrdev(MAJOR_DEV, DEVICE_ID, &file_ops);
	if (!ret) {
		major = MAJOR_DEV;
		log_info("registered device (major %d, minor %d)", major, 0);
		device_number = MKDEV(major, 0);
	} else if (ret > 0) {
		major = ret >> 20;
		log_info("registered device (major %d, minor %d)", major,
			 ret & 0xfffff);
		device_number = MKDEV(major, ret & 0xfffff);
	} else {
		log_error("unable to register device: %d", ret);
		return ret;
	}

	log_debug("creating device class with ID '%s'", DEVICE_ID);
	device_class = class_create(DEVICE_ID);
	if (IS_ERR(device_class)) {
		log_error("unable to create device class");
		unregister_chrdev(major, DEVICE_ID);
		return -EINVAL;
	}

	log_debug("creating device with id '%s'", DEVICE_ID);
	struct device *device = device_create(device_class, NULL, device_number,
					      NULL, DEVICE_ID);
	if (IS_ERR(device)) {
		log_error("unable to create device");
		class_destroy(device_class);
		unregister_chrdev(major, DEVICE_ID);
		return -EINVAL;
	}

	return 0;
}

static void __exit exit_mod(void)
{
	log_debug("called exit_module");

	if (mem_overlays) {
		log_info("cleaning up mem_overlays hashtable");
		hashtable_cleanup(mem_overlays);
		mem_overlays = NULL;
	}

	log_info("unregistering device with major %u and ID '%s'",
		 (unsigned int)major, DEVICE_ID);
	device_destroy(device_class, device_number);
	class_destroy(device_class);
	unregister_chrdev(major, DEVICE_ID);
}

module_init(init_mod);
module_exit(exit_mod);
