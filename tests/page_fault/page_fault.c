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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#include <bits/time.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "../../module.h"

int main()
{
	int res = EXIT_SUCCESS;

	const size_t page_size = sysconf(_SC_PAGESIZE);
	const size_t total_size = page_size * 1024; // * 1024;
	printf("Using pagesize %lu with total size %lu\n", page_size,
	       total_size);

	// Read base.bin test file and mmap it into memory.
	int base_fd = open("base.bin", O_RDONLY);
	if (base_fd < 0) {
		printf("ERROR: could not open base file: %d\n", base_fd);
		return EXIT_FAILURE;
	}

	struct timespec before, after;
	if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
		printf("ERROR: could not measure 'before' time for base mmap\n");
		res = EXIT_FAILURE;
		goto close_base;
	}

	char *base_map =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, base_fd, 0);
	if (base_map == MAP_FAILED) {
		printf("ERROR: could not mmap base file\n");
		res = EXIT_FAILURE;
		goto close_base;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
		printf("ERROR: could not measure 'after' time for base mmap\n");
		res = EXIT_FAILURE;
		goto unmap_base;
	}

	printf("mmap(\"base.bin\") took %lins (%lims)\n",
	       after.tv_nsec - before.tv_nsec,
	       (after.tv_nsec - before.tv_nsec) / 1000000);

	// Read overlay test file and create mmap struct.
	char overlay_file[] = "overlay2.bin";
	int overlay_fd = open(overlay_file, O_RDONLY);
	if (overlay_fd < 0) {
		printf("ERROR: could not open overlay file: %d\n", overlay_fd);
		res = EXIT_FAILURE;
		goto close_overlay;
	}

	char *overlay_map =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, overlay_fd, 0);
	if (overlay_map == MAP_FAILED) {
		printf("ERROR: could not mmap overlay file\n");
		res = EXIT_FAILURE;
		goto close_overlay;
	}

	struct mmap _mmap;
	_mmap.base_addr = *(unsigned long *)(&base_map);
	_mmap.overlay_addr = *(unsigned long *)(&overlay_map);
	_mmap.size = 2;
	_mmap.elements = malloc(sizeof(struct mmap_element) * _mmap.size);
	memset(_mmap.elements, 0, sizeof(struct mmap_element) * _mmap.size);

	printf("requesting %u operations and sending %lu bytes worth of mmap elements\n",
	       _mmap.size, sizeof(struct mmap_element) * _mmap.size);

	// Mark the parts of the memory area as elements to be overlaid.
	// In this test, each element is two pages in size and skips one page
	// between them, creating a pattern like this:
	// _XX_XX_XX_XX_XX
	for (int i = 0; i < _mmap.size; i++) {
		// TODO: fix module to handle first page in address space.
		_mmap.elements[i].pg_start = 3 * i + 1;
		_mmap.elements[i].pg_end = 3 * i + 2;
	}

	// Call kernel module with ioctl call to the character device.
	int syscall_dev = open("/dev/batch_syscalls", O_WRONLY);
	if (syscall_dev < 0) {
		printf("ERROR: could not open /dev/batch_syscalls: %d\n",
		       syscall_dev);
		res = EXIT_FAILURE;
		goto free_elements;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
		printf("ERROR: could not measure 'before' time for overlay mmap\n");
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}

	int ret = ioctl(syscall_dev, IOCTL_MMAP_CMD, &_mmap);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %d\n", ret);
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
		printf("ERROR: could not measure 'after' time for overlay mmap\n");
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}

	printf("mmap(\"%s\") (%i pages) took %lins (%lims)\n", overlay_file,
	       _mmap.size, after.tv_nsec - before.tv_nsec,
	       (after.tv_nsec - before.tv_nsec) / 1000000);

	// Verify resulting memory.
	printf("checking mmap buffer against file contents\n");

	char *buffer = malloc(page_size);
	memset(buffer, 0, page_size);

	for (int pgoff = 0; pgoff < total_size / page_size; pgoff++) {
		int fd = base_fd;

		// Verify if reading page from base or overlay.
		for (int i = 0; i < _mmap.size; i++) {
			if (pgoff >= _mmap.elements[i].pg_start && pgoff <= _mmap.elements[i].pg_end) {
				fd = overlay_fd;
                break;
			}
		}
		if(fd == base_fd) {
            printf("checking page %d using base\n", pgoff);
        } else if(fd == overlay_fd) {
            printf("checking page %d using overlay\n", pgoff);
        } else {
            printf("not sure what fd we're using\n");
        }

		size_t offset = pgoff * page_size;
		lseek(fd, offset, SEEK_SET);
		read(fd, buffer, page_size);

		if (memcmp(base_map + offset, buffer, page_size)) {
			printf("ERROR: mmap buffer does not match the file contents at page %d\n", pgoff);
			res = EXIT_FAILURE;
			goto free_buffer;
		}
		memset(buffer, 0, page_size);
	}
	printf("verification completed successfully!\n");

free_buffer:
	free(buffer);
close_syscall_dev:
	close(syscall_dev);
free_elements:
	free(_mmap.elements);
unmap_overlay:
	munmap(overlay_map, total_size);
close_overlay:
	close(overlay_fd);
unmap_base:
	munmap(base_map, total_size);
close_base:
	close(base_fd);

	return res;
}
