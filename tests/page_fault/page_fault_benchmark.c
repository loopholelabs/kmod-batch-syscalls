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

#include <errno.h>
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

#include "../../common.h"

size_t page_size, total_size;

static const char base_file[] = "baseXL.bin";
static const char clean_base_file[] = "baseXL2.bin";
static const char overlay_file[] = "overlayXL.bin";
static const int page_size_factor = 1024 * 1024;

bool verify_test_cases(int overlay_fd, int base_fd, char *base_map)
{
	char *buffer = malloc(page_size);
	memset(buffer, 0, page_size);
	bool valid = true;

	struct timespec before, after;
	if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
		printf("ERROR: could not measure 'before' time for base mmap: %s\n",
		       strerror(errno));
		valid = false;
		goto out;
	}

	printf("%ld.%.9ld: starting memory check\n", before.tv_sec,
	       before.tv_nsec);

	for (unsigned long pgoff = 0; pgoff < total_size / page_size; pgoff++) {
		size_t offset = pgoff * page_size;

		int fd = base_fd;
		if (overlay_fd > 0 && pgoff % 2 == 0) {
			fd = overlay_fd;
		}
		lseek(fd, offset, SEEK_SET);
		read(fd, buffer, page_size);

		if (memcmp(base_map + offset, buffer, page_size)) {
			printf("== ERROR: base memory does not match the file contents at page %lu\n",
			       pgoff);
			valid = false;
			break;
		}
		memset(buffer, 0, page_size);
	}

	if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
		printf("ERROR: could not measure 'after' time for base mmap: %s\n",
		       strerror(errno));
		valid = false;
		goto out;
	}

	printf("%ld.%.9ld: finished memory check\n", after.tv_sec,
	       after.tv_nsec);

	long secs_diff = after.tv_sec - before.tv_sec;
	long nsecs_diff = after.tv_nsec;
	if (secs_diff == 0) {
		nsecs_diff = after.tv_nsec - before.tv_nsec;
	}
	printf("test verification took %ld.%.9lds\n", secs_diff, nsecs_diff);

out:
	free(buffer);
	return valid;
}

int main()
{
	int res = EXIT_SUCCESS;

	page_size = sysconf(_SC_PAGESIZE);
	total_size = page_size * page_size_factor;
	printf("Using pagesize %lu with total size %lu\n", page_size,
	       total_size);

	// Read base.bin test file and mmap it into memory.
	int base_fd = open(base_file, O_RDONLY);
	if (base_fd < 0) {
		printf("ERROR: could not open base file %s: %s\n", base_file,
		       strerror(errno));
		return EXIT_FAILURE;
	}

	int clean_base_fd = open(clean_base_file, O_RDONLY);
	if (clean_base_fd < 0) {
		printf("ERROR: could not open clean base file %s: %s\n",
		       clean_base_file, strerror(errno));
		close(base_fd);
		return EXIT_FAILURE;
	}

	char *base_mmap = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
			       MAP_PRIVATE, base_fd, 0);
	if (base_mmap == MAP_FAILED) {
		printf("ERROR: could not mmap base file %s: %s\n", base_file,
		       strerror(errno));
		res = EXIT_FAILURE;
		goto close_base;
	}

	char *clean_base_mmap = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE,
				     clean_base_fd, 0);
	if (clean_base_mmap == MAP_FAILED) {
		printf("ERROR: could not mmap second base file %s: %s\n",
		       clean_base_file, strerror(errno));
		res = EXIT_FAILURE;
		goto close_base;
	}

	// Read overlay test file and create memory overlay request.
	int overlay_fd = open(overlay_file, O_RDONLY);
	if (overlay_fd < 0) {
		printf("ERROR: could not open overlay file %s: %s\n",
		       overlay_file, strerror(errno));
		res = EXIT_FAILURE;
		goto unmap_base;
	}

	char *overlay_map =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, overlay_fd, 0);
	if (overlay_map == MAP_FAILED) {
		printf("ERROR: could not mmap overlay file %s: %s\n",
		       overlay_file, strerror(errno));
		res = EXIT_FAILURE;
		goto close_overlay;
	}

	struct mem_overlay_req req;
	req.base_addr = *(unsigned long *)(&base_mmap);
	req.overlay_addr = *(unsigned long *)(&overlay_map);
	req.segments_size = total_size / page_size / 2;
	req.segments = malloc(sizeof(struct mem_overlay_segment_req) *
			      req.segments_size);
	memset(req.segments, 0,
	       sizeof(struct mem_overlay_segment_req) * req.segments_size);

	printf("requesting %u operations and sending %lu bytes worth of mmap segments\n",
	       req.segments_size,
	       sizeof(struct mem_overlay_segment_req) * req.segments_size);

	for (int i = 0; i < req.segments_size; i++) {
		req.segments[i].start_pgoff = 2 * i;
		req.segments[i].end_pgoff = 2 * i;
	}

	// Call kernel module with ioctl call to the character device.
	int syscall_dev = open(kmod_device_path, O_WRONLY);
	if (syscall_dev < 0) {
		printf("ERROR: could not open %s: %s\n", kmod_device_path,
		       strerror(errno));
		res = EXIT_FAILURE;
		goto free_segments;
	}

	int ret;
	ret = ioctl(syscall_dev, IOCTL_MEM_OVERLAY_REQ_CMD, &req);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %s\n",
		       strerror(errno));
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}

	printf("= TEST: checking memory contents with overlay\n");
	if (!verify_test_cases(overlay_fd, base_fd, base_mmap)) {
		res = EXIT_FAILURE;
		goto cleanup;
	}
	printf("== OK: overlay memory verification completed successfully!\n");

	printf("= TEST: checking memory contents without overlay\n");
	if (!verify_test_cases(-1, clean_base_fd, clean_base_mmap)) {
		res = EXIT_FAILURE;
		goto cleanup;
	}
	printf("== OK: non-overlay memory verification completed successfully!\n");

cleanup:
	// Clean up memory overlay.
	printf("calling IOCTL_MEM_OVERLAY_CLEANUP_CMD\n");
	struct mem_overlay_cleanup_req cleanup_req = {
		.id = req.id,
	};
	ret = ioctl(syscall_dev, IOCTL_MEM_OVERLAY_CLEANUP_CMD, &cleanup_req);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %s\n",
		       strerror(errno));
		res = EXIT_FAILURE;
	}
close_syscall_dev:
	close(syscall_dev);
free_segments:
	free(req.segments);
	munmap(overlay_map, total_size);
close_overlay:
	close(overlay_fd);
unmap_base:
	munmap(clean_base_mmap, total_size);
	munmap(base_mmap, total_size);
close_base:
	close(clean_base_fd);
	close(base_fd);

	printf("done\n");
	return res;
}
