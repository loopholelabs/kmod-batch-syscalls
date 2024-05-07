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
#include "log.h"

MODULE_AUTHOR("Loophole Labs (Shivansh Vij)");
MODULE_DESCRIPTION("Batch Syscalls");
MODULE_LICENSE("GPL");

static struct xarray elements;
static struct vm_operations_struct hijacked_vm_ops;

static vm_fault_t hijacked_map_pages(struct vm_fault *vmf, pgoff_t start_pgoff,
				     pgoff_t end_pgoff)
{
	// TODO: handle case where ->prealloc_pte is set.
	// This happens when a PMD entry does not exist for the faulting address,
	// such as when the faulted page is the first one in the address space.
	if (vmf->prealloc_pte)
		goto out;

#ifdef BENCHMARK
	ktime_t start = ktime_get();
#endif
	// Verify if reading memory from base or overlay.
	void *el = xa_load(&elements, vmf->pgoff);
	if (!el)
		goto out;

	log_debug(
		"page fault on overlay area page=%lu pud=0x%llu pmd=0x%llu addr=0x%lu",
		vmf->pgoff, pud_val(*(vmf->pud)), pmd_val(*(vmf->pmd)),
		vmf->address);

	// Find page from overlay memory area and load it into memory.
	struct page *src_page;
	struct vm_area_struct *overlay_vma = ((struct mmap_element *)el)->vma;
	unsigned long src_addr = overlay_vma->vm_start + vmf->pgoff * PAGE_SIZE;

	int nr_pages = get_user_pages_fast(src_addr, 1, 0, &src_page);
	if (nr_pages != 1) {
		log_error("expected 1 page, got %d", nr_pages);
		goto out;
	}

#ifdef DEBUG
	unsigned long pfn = page_to_pfn(src_page);
	log_debug("found overlay page addr=0x%lu pfn=0x%lu",
		  overlay_vma->vm_start, pfn);
#endif

	// Load folio for source page to increment its reference count.
	struct folio *folio = page_folio(src_page);
	folio_get(folio);

	// Create page table entry for faulted address.
	unsigned long dst_addr = vmf->vma->vm_start + vmf->pgoff * PAGE_SIZE;
	pte_t *ptep = pte_offset_kernel(vmf->pmd, dst_addr);
	pte_t pte = mk_pte(src_page, overlay_vma->vm_page_prot);

	log_debug(
		"setting PTE on base memory to overlay page addr=0x%lu pte=0x%llu",
		dst_addr, pte_val(pte));
	set_pte(ptep, pte);

#ifdef DEBUG
	struct page *dst_page = pte_page(pte);
	unsigned long dst_pfn = page_to_pfn(dst_page);
	log_debug("overlaid page on base memory area pfn=0x%lu", dst_pfn);
#endif

	// Increment MM_FILEPAGES counter to prevent "Bad rss-counter state" bug.
	percpu_counter_inc(&vmf->vma->vm_mm->rss_stat[MM_FILEPAGES]);

	// Don't call put_page() or folio_put() because the page and folio are
	// being reference by the new PTE.

#ifdef BENCHMARK
	s64 elapsed = ktime_to_ns(ktime_sub(ktime_get(), start));
	log_benchmark("page fault handling time: %lldns (%lldms)",
		      (long long)elapsed, (long long)elapsed / 1000000);
#endif

	return VM_FAULT_NOPAGE;

out:
	return filemap_map_pages(vmf, start_pgoff, end_pgoff);
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

static long int unlocked_ioctl(struct file *file, unsigned cmd,
			       unsigned long arg)
{
	log_debug("called unlocked_ioctl");
	switch (cmd) {
	case IOCTL_MMAP_CMD:
		static struct mmap mmap;
		unsigned long ret = copy_from_user(&mmap, (struct mmap *)arg,
						   sizeof(struct mmap));
		if (ret) {
			log_error(
				"unable to copy mmap struct from user during mmap ioctl: %lu",
				ret);
			break;
		}
		log_debug("base addr: %lu", mmap.base_addr);
		log_debug("overlay addr: %lu", mmap.overlay_addr);

		// Find overlay VMA to store it into each element.
		struct vm_area_struct *overlay_vma =
			find_vma(current->mm, mmap.overlay_addr);
		if (overlay_vma == NULL ||
		    overlay_vma->vm_start > mmap.overlay_addr) {
			log_error("invalid overlay_vma");
			return -EFAULT;
		}

		// Read elements from ioctl call.
		struct mmap_element *els = kvzalloc(
			sizeof(struct mmap_element) * mmap.size, GFP_KERNEL);
		if (!els) {
			log_crit(
				"unable to allocate elements variable during mmap ioctl");
			return -ENOMEM;
		}

		ret = copy_from_user(els, mmap.elements,
				     sizeof(struct mmap_element) * mmap.size);
		if (ret) {
			log_error(
				"unable to copy elements from user during mmap ioctl: %lu",
				ret);
			kvfree(els);
			break;
		}

		for (int i = 0; i < mmap.size; i++) {
			unsigned long start = els[i].pg_start;
			unsigned long end = els[i].pg_end;
			els[i].vma = overlay_vma;

			log_info("element to overlay start=%lu end=%lu", start,
				 end);
			xa_store_range(&elements, start, end, &els[i],
				       GFP_KERNEL);
		}

		// Hijack page fault handler of base file VMA.
		struct vm_area_struct *base_vma =
			find_vma(current->mm, mmap.base_addr);
		if (base_vma == NULL || base_vma->vm_start > mmap.base_addr) {
			log_error("invalid base_vma");
			kvfree(els);
			return -EFAULT;
		}

		if (!hijacked_vm_ops.fault) {
			log_info("hijacking vm_ops for base_addr %lu",
				 mmap.base_addr);
			memcpy(&hijacked_vm_ops, base_vma->vm_ops,
			       sizeof(struct vm_operations_struct));
			hijacked_vm_ops.map_pages = &hijacked_map_pages;
		}
		base_vma->vm_ops = &hijacked_vm_ops;
		log_info("done hijacking vm_ops");
		return 0;
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
	xa_init(&elements);

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
	log_info("unregistering device with major %u and ID '%s'",
		 (unsigned int)major, DEVICE_ID);
	device_destroy(device_class, device_number);
	class_destroy(device_class);
	unregister_chrdev(major, DEVICE_ID);
	xa_destroy(&elements);
}

module_init(init_mod);
module_exit(exit_mod);
