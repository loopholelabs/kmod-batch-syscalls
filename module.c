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
#include <linux/device.h>

#include <asm/io.h>

#include "module.h"
#include "log.h"

MODULE_AUTHOR("Loophole Labs (Shivansh Vij)");
MODULE_DESCRIPTION("Batch Syscalls");
MODULE_LICENSE("GPL");

static int device_open(struct inode *device_file, struct file *instance) {
    log_debug("called device_open");
    log_info("device opened");
    return 0;
}

static int device_close(struct inode *device_file, struct file *instance) {
    log_debug("called device_close");
    log_info("device closed");
    return 0;
}

static long int unlocked_ioctl(struct file *file, unsigned cmd, unsigned long arg){
    log_debug("called unlocked_ioctl");
    switch(cmd){
        case IOCTL_MMAP_CMD:
            struct mmap mmap;
            unsigned long ret = copy_from_user(&mmap, (struct mmap*)arg, sizeof(struct mmap));
            if(ret) {
                log_error("unable to copy mmap struct from user during mmap ioctl: %lu", ret);
                break;
            }
            long path_len = strnlen_user(mmap.path, PATH_MAX);
            if(!path_len) {
                log_error("invalid path length during mmap ioctl");
                break;
            } else if (path_len > PATH_MAX) {
                log_error("path length too large (%ld > %ld) during mmap ioctl", path_len, (long)PATH_MAX);
                break;
            }
            char* path = kmalloc(path_len, GFP_KERNEL);
            if(!path) {
                log_crit("unable to allocate path variable during mmap ioctl");
                return -ENOMEM;
            }
            ret = copy_from_user(path, mmap.path, path_len);
            if(ret) {
                log_error("unable to copy path from user during mmap ioctl: %lu", ret);
                kfree(path);
                break;
            }
            path[path_len-1] = '\0';
            log_info("performing %d mmap operations for '%s' with flag %lu and prot %lu", mmap.size, path, mmap.flag, mmap.prot);
            struct file* file_ptr = filp_open(path, mmap.mode, 0);
            if(!file_ptr) {
                log_error("unable to open file '%s' during mmap ioctl", path);
                kfree(path);
                break;
            }

            struct mmap_element* elements = kmalloc(sizeof(struct mmap_element) * mmap.size, GFP_KERNEL);
            for(unsigned int i = 0; i < mmap.size; i++) {
                ret = copy_from_user(&elements[i], &mmap.elements[i], sizeof(struct mmap_element));
                if(ret) {
                    log_error("unable to copy element %u from user during mmap ioctl: %lu", i, ret);
                    kfree(elements);
                    fput(file_ptr);
                    kfree(path);
                    break;
                }
                log_debug("batch mmap ioctl with path '%s': addr %lu, len %lu, mode %u, and offset %lu", path, elements[i].addr, elements[i].len, mmap.mode, elements[i].offset);
                elements[i].ret = vm_mmap(file_ptr, elements[i].addr, elements[i].len, mmap.prot, mmap.flag, elements[i].offset);
                log_info("successful mmap ioctl for element %u with path '%s' (addr %lu, len %lu, and offset %lu)", i, path, elements[i].addr, elements[i].len, elements[i].offset);
            }

            int close_ret = filp_close(file_ptr, NULL);
            if(close_ret) {
                log_error("unable to close file '%s' during mmap ioctl", path);
                kfree(elements);
                fput(file_ptr);
                kfree(path);
                break;
            }
            fput(file_ptr);
            kfree(path);

            for(unsigned int i = 0; i < mmap.size; i++) {
                ret = copy_to_user(&mmap.elements[i], &elements[i], sizeof(struct mmap_element));
                if(ret) {
                    log_error("unable to copy element %u to user during mmap ioctl: %lu", i, ret);
                    kfree(elements);
                    break;
                }
            }
            kfree(elements);

            return 0;
        default:
            log_error("unknown ioctl cmd %x", cmd);
    }
    return -EINVAL;
}

static struct file_operations file_ops = {
        .owner = THIS_MODULE,
        .open = device_open,
        .release = device_close,
        .unlocked_ioctl = unlocked_ioctl
};

static unsigned int major;
static dev_t device_number;
static struct class* device_class;

static int __init init_mod(void) {
    log_debug("called init_module");
    log_info("registering device with major %u and ID '%s'", (unsigned int)MAJOR_DEV, DEVICE_ID);
    int ret = register_chrdev(MAJOR_DEV, DEVICE_ID, &file_ops);
    if (!ret) {
        major = MAJOR_DEV;
        log_info("registered device (major %d, minor %d)", major, 0);
        device_number = MKDEV(major, 0);
    } else if (ret > 0) {
        major = ret>>20;
        log_info("registered device (major %d, minor %d)", major, ret&0xfffff);
        device_number = MKDEV(major, ret&0xfffff);
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
    struct device* device = device_create(device_class, NULL, device_number, NULL, DEVICE_ID);
    if(IS_ERR(device)) {
        log_error("unable to create device");
        class_destroy(device_class);
        unregister_chrdev(major, DEVICE_ID);
        return -EINVAL;
    }

    return 0;
}
static void __exit exit_mod(void) {
    log_debug("called exit_module");
    log_info("unregistering device with major %u and ID '%s'", (unsigned int)major, DEVICE_ID);
    device_destroy(device_class, device_number);
    class_destroy(device_class);
    unregister_chrdev(major, DEVICE_ID);
}

module_init(init_mod);
module_exit(exit_mod);