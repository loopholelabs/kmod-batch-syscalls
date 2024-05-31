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

#ifndef MEMORY_OVERLAY_COMMON_H
#define MEMORY_OVERLAY_COMMON_H

#define MAGIC 's'
#define IOCTL_MEM_OVERLAY_REQ_CMD _IOWR(MAGIC, 1, struct mem_overlay_req *)
#define IOCTL_MEM_OVERLAY_CLEANUP_CMD \
	_IOWR(MAGIC, 2, struct mem_overlay_cleanup_req *)

static const char kmod_device_path[] = "/dev/memory_overlay";

struct mem_overlay_segment_req {
	unsigned long start_pgoff;
	unsigned long end_pgoff;
};

struct mem_overlay_req {
	unsigned long id;

	unsigned long base_addr;
	unsigned long overlay_addr;

	unsigned int segments_size;
	struct mem_overlay_segment_req *segments;
};

struct mem_overlay_cleanup_req {
	unsigned long id;
};

#endif //MEMORY_OVERLAY_COMMON_H
