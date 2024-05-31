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

struct test_case {
	unsigned long pgoff;
	int fd;
	char *data;
};

size_t page_size, total_size;

static const char base_file[] = "base.bin";
static const char overlay_file[] = "overlay.bin";
static const int page_size_factor = 1024;

bool verify_test_cases(struct test_case *tcs, int tcs_nr, int base_fd,
		       char *base_map)
{
	char *buffer = malloc(page_size);
	memset(buffer, 0, page_size);
	bool valid = true;

	for (unsigned long pgoff = 0; pgoff < total_size / page_size; pgoff++) {
		size_t offset = pgoff * page_size;

		struct test_case *tc = NULL;
		for (int i = 0; i < tcs_nr; i++) {
			if (tcs[i].pgoff == pgoff) {
				tc = &tcs[i];
				break;
			}
		}

		int fd = base_fd;
		if (tc != NULL) {
			if (tc->fd > 0) {
				fd = tc->fd;
				printf("checking if page %lu is from overlay\n",
				       pgoff);
			} else if (tc->data) {
				printf("checking if page %lu has expected data\n",
				       pgoff);
				memcpy(buffer, tc->data, page_size);
				fd = -1;
			}
		}

		if (fd > 0) {
			lseek(fd, offset, SEEK_SET);
			read(fd, buffer, page_size);
		}

		if (memcmp(base_map + offset, buffer, page_size)) {
			printf("== ERROR: base memory does not match the file contents at page %lu\n",
			       pgoff);
			valid = false;
			break;
		}
		memset(buffer, 0, page_size);
	}

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
	printf("opened base file %s\n", base_file);

	char *base_mmap = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
			       MAP_PRIVATE, base_fd, 0);
	if (base_mmap == MAP_FAILED) {
		printf("ERROR: could not mmap base file %s: %s\n", base_file,
		       strerror(errno));
		res = EXIT_FAILURE;
		goto close_base;
	}
	printf("mapped base file %s\n", base_file);

	char *clean_base_mmap =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, base_fd, 0);
	if (clean_base_mmap == MAP_FAILED) {
		printf("ERROR: could not mmap clean base file %s: %s\n",
		       base_file, strerror(errno));
		res = EXIT_FAILURE;
		goto close_base;
	}
	printf("mapped clean base file %s\n", overlay_file);

	// Read overlay test file and create memory overlay request.
	int overlay_fd = open(overlay_file, O_RDONLY);
	if (overlay_fd < 0) {
		printf("ERROR: could not open overlay file %s: %s\n",
		       overlay_file, strerror(errno));
		res = EXIT_FAILURE;
		goto unmap_base;
	}
	printf("opened overlay file %s\n", overlay_file);

	char *overlay_map =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, overlay_fd, 0);
	if (overlay_map == MAP_FAILED) {
		printf("ERROR: could not mmap overlay file %s: %s\n",
		       overlay_file, strerror(errno));
		res = EXIT_FAILURE;
		goto close_overlay;
	}
	printf("mapped overlay file %s\n", overlay_file);

	struct mem_overlay_req req;
	req.base_addr = *(unsigned long *)(&base_mmap);
	req.overlay_addr = *(unsigned long *)(&overlay_map);
	req.segments_size = 5;
	req.segments = malloc(sizeof(struct mem_overlay_segment_req) *
			      req.segments_size);
	memset(req.segments, 0,
	       sizeof(struct mem_overlay_segment_req) * req.segments_size);

	printf("requesting %u operations and sending %lu bytes worth of mmap segments\n",
	       req.segments_size,
	       sizeof(struct mem_overlay_segment_req) * req.segments_size);

	// Overlay single page.
	req.segments[0].start_pgoff = 0;
	req.segments[0].end_pgoff = 0;

	// Overlay multiple pages.
	req.segments[1].start_pgoff = 4;
	req.segments[1].end_pgoff = 6;

	// Two overlays back-to-back.
	req.segments[2].start_pgoff = 20;
	req.segments[2].end_pgoff = 20;
	req.segments[3].start_pgoff = 21;
	req.segments[3].end_pgoff = 21;

	// Large overlay that crosses fault-around borders.
	req.segments[4].start_pgoff = 30;
	req.segments[4].end_pgoff = 50;

	// Call kernel module with ioctl call to the character device.
	int syscall_dev = open("/dev/batch_syscalls", O_WRONLY);
	if (syscall_dev < 0) {
		printf("ERROR: could not open /dev/batch_syscalls: %d\n",
		       syscall_dev);
		res = EXIT_FAILURE;
		goto free_elements;
	}

	int ret;
	ret = ioctl(syscall_dev, IOCTL_MEM_OVERLAY_REQ_CMD, &req);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %s\n",
		       strerror(errno));
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}

	// Verify reading memory.
	int tcs_nr = 27;
	struct test_case *tcs = malloc(sizeof(struct test_case) * tcs_nr);
	tcs[0].pgoff = 0;
	tcs[0].fd = overlay_fd;
	tcs[1].pgoff = 4;
	tcs[1].fd = overlay_fd;
	tcs[2].pgoff = 5;
	tcs[2].fd = overlay_fd;
	tcs[3].pgoff = 6;
	tcs[3].fd = overlay_fd;
	tcs[4].pgoff = 20;
	tcs[4].fd = overlay_fd;
	tcs[5].pgoff = 21;
	tcs[5].fd = overlay_fd;
	for (int i = 6, j = 30; j <= 50; i++, j++) {
		tcs[i].pgoff = j;
		tcs[i].fd = overlay_fd;
	}

	printf("= TEST: checking memory contents with overlay\n");
	if (!verify_test_cases(tcs, tcs_nr, base_fd, base_mmap)) {
		res = EXIT_FAILURE;
		free(tcs);
		goto cleanup;
	}
	printf("== OK: overlay memory verification completed successfully!\n");

	printf("= TEST: checking memory contents without overlay\n");
	if (!verify_test_cases(NULL, 0, base_fd, clean_base_mmap)) {
		res = EXIT_FAILURE;
		free(tcs);
		goto cleanup;
	}
	free(tcs);
	printf("== OK: non-overlay memory verification completed successfully!\n");

	// TODO: parse /proc/<pid>/smaps to verify memory sharing.

	// Verify writing to memory.
	int rand_fd = open("/dev/random", O_RDONLY);
	if (rand_fd < 0) {
		printf("ERROR: could not open /dev/random: %d\n", rand_fd);
		res = EXIT_FAILURE;
		goto cleanup;
	}

	char *rand = malloc(page_size);
	memset(rand, 0, page_size);
	read(rand_fd, rand, page_size);
	close(rand_fd);

	// Write to non-overlay page.
	memcpy(base_mmap + page_size * 10, rand, page_size);
	// Write to overlay page.
	memcpy(base_mmap + page_size * 4, rand, page_size);

	tcs_nr = 28;
	tcs = malloc(sizeof(struct test_case) * tcs_nr);
	tcs[0].pgoff = 0;
	tcs[0].fd = overlay_fd;
	tcs[1].pgoff = 4;
	tcs[1].data = rand;
	tcs[2].pgoff = 5;
	tcs[2].fd = overlay_fd;
	tcs[3].pgoff = 6;
	tcs[3].fd = overlay_fd;
	tcs[4].pgoff = 10;
	tcs[4].data = rand;
	tcs[5].pgoff = 20;
	tcs[5].fd = overlay_fd;
	tcs[6].pgoff = 21;
	tcs[6].fd = overlay_fd;
	for (int i = 7, j = 30; j <= 50; i++, j++) {
		tcs[i].pgoff = j;
		tcs[i].fd = overlay_fd;
	}

	printf("= TEST: checking memory write\n");
	if (!verify_test_cases(tcs, tcs_nr, base_fd, base_mmap)) {
		res = EXIT_FAILURE;
		free(tcs);
		free(rand);
		goto cleanup;
	}
	free(tcs);
	free(rand);
	printf("== OK: memory write verification completed successfully!\n");

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
	printf("closed device driver\n");
free_elements:
	free(req.segments);
	munmap(overlay_map, total_size);
	printf("unmapped overlay file\n");
close_overlay:
	close(overlay_fd);
	printf("closed overlay file\n");
unmap_base:
	munmap(clean_base_mmap, total_size);
	printf("unmapped clean base file\n");
	munmap(base_mmap, total_size);
	printf("unmapped base file\n");
close_base:
	close(base_fd);
	printf("closed base file\n");

	printf("done\n");
	return res;
}
