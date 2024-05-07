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

#ifndef BATCH_SYSCALLS_MODULE_H
#define BATCH_SYSCALLS_MODULE_H

#define MAJOR_DEV 64
#define DEVICE_ID "batch_syscalls"
#define MAGIC 's'

struct mmap_element {
	unsigned long pg_start;
	unsigned long pg_end;
	struct vm_area_struct *vma;
};

struct mmap {
	unsigned long base_addr;
	unsigned long overlay_addr;
	unsigned int size;
	struct mmap_element *elements;
};

#define IOCTL_MMAP_CMD _IOWR(MAGIC, 1, struct mmap *)

#endif //BATCH_SYSCALLS_MODULE_H
