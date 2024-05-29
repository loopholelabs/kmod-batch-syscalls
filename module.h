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

#include <linux/xarray.h>

#ifndef BATCH_SYSCALLS_MODULE_H
#define BATCH_SYSCALLS_MODULE_H

#define MAJOR_DEV 64
#define DEVICE_ID "batch_syscalls"

struct mem_overlay_segment {
	unsigned long overlay_addr;
	struct vm_area_struct *overlay_vma;

	unsigned long start_pgoff;
	unsigned long end_pgoff;
};

struct mem_overlay {
	unsigned char id[UUID_SIZE];

	unsigned long base_addr;
	struct vm_area_struct *base_vma;
	spinlock_t vma_file_lock;

	struct xarray segments;

	struct vm_operations_struct *hijacked_vm_ops;
};

#endif //BATCH_SYSCALLS_MODULE_H
