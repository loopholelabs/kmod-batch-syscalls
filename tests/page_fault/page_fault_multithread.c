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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../../common.h"

const static int nr_threads = 10;
size_t page_size, total_size;
char *base_mmap;

char base_file[] = "baseXL.bin";
char overlay_file[] = "overlayXL.bin";

void *page_fault()
{
	printf("verifying base memory\n");

	char *buffer = malloc(page_size);
	memset(buffer, 0, page_size);

	int base_fd = open(base_file, O_RDONLY);
	if (base_fd < 0) {
		printf("ERROR: could not open base file: %d\n", base_fd);
		goto out;
	}

	int overlay_fd = open(overlay_file, O_RDONLY);
	if (overlay_fd < 0) {
		printf("ERROR: could not open overlay file: %d\n", overlay_fd);
		goto out;
	}

	int fd;
	for (unsigned long pgoff = 0; pgoff < total_size / page_size; pgoff++) {
		size_t offset = pgoff * page_size;

		fd = base_fd;
		if (pgoff % 2 == 0) {
			fd = overlay_fd;
		}
		lseek(fd, offset, SEEK_SET);
		read(fd, buffer, page_size);

		if (memcmp(base_mmap + offset, buffer, page_size)) {
			printf("== ERROR: base memory does not match the file contents at page %lu\n",
			       pgoff);
			goto out;
		}
		memset(buffer, 0, page_size);
	}
	printf("== OK: base memory verification complete\n");

out:
	free(buffer);
	return NULL;
}

int main()
{
	int res = EXIT_SUCCESS;
	page_size = sysconf(_SC_PAGESIZE);
	total_size = page_size * 1024 * 1024;
	pthread_t tid[nr_threads];

	int base_fd = open(base_file, O_RDONLY);
	if (base_fd < 0) {
		printf("ERROR: could not open base file: %d\n", base_fd);
		return EXIT_FAILURE;
	}
	printf("base file %s opened\n", base_file);

	base_mmap = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
			 base_fd, 0);
	if (base_mmap == MAP_FAILED) {
		printf("ERROR: could not mmap base file\n");
		res = EXIT_FAILURE;
		goto close_base;
	}
	printf("base file %s mapped\n", base_file);

	int overlay_fd = open(overlay_file, O_RDONLY);
	if (overlay_fd < 0) {
		printf("ERROR: could not open overlay file: %d\n", overlay_fd);
		res = EXIT_FAILURE;
		goto unmap_base;
	}
	printf("overlay file %s opened\n", overlay_file);

	char *overlay_map =
		mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, overlay_fd, 0);
	if (overlay_map == MAP_FAILED) {
		printf("ERROR: could not mmap overlay file\n");
		res = EXIT_FAILURE;
		goto close_overlay;
	}
	printf("overlay file %s mapped\n", overlay_file);

	struct mem_overlay_req req;
	req.base_addr = *(unsigned long *)(&base_mmap);
	req.overlay_addr = *(unsigned long *)(&overlay_map);
	req.segments_size = total_size / (page_size * 2);
	req.segments = malloc(sizeof(struct mem_overlay_segment_req) *
			      req.segments_size);
	memset(req.segments, 0,
	       sizeof(struct mem_overlay_segment_req) * req.segments_size);

	// Overlay half of the pages.
	for (int i = 0; i < req.segments_size; i++) {
		req.segments[i].start_pgoff = 2 * i;
		req.segments[i].end_pgoff = 2 * i;
	}
	printf("generated memory overlay request\n");

	int syscall_dev = open("/dev/batch_syscalls", O_WRONLY);
	if (syscall_dev < 0) {
		printf("ERROR: could not open /dev/batch_syscalls: %d\n",
		       syscall_dev);
		res = EXIT_FAILURE;
		goto free_elements;
	}
	printf("opened /dev/batch_syscalls device\n");

	int ret = ioctl(syscall_dev, IOCTL_MEM_OVERLAY_REQ_CMD, &req);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %d\n", ret);
		res = EXIT_FAILURE;
		goto close_syscall_dev;
	}
	printf("called IOCTL_MEM_OVERLAY_REQ_CMD\n");

	for (int i = 0; i < nr_threads; i++) {
		pthread_create(&tid[i], NULL, page_fault, NULL);
	}

	for (int i = 0; i < nr_threads; i++) {
		pthread_join(tid[i], NULL);
	}

	struct mem_overlay_cleanup_req cleanup_req;
	memcpy(cleanup_req.id, req.id, sizeof(unsigned char) * UUID_SIZE);
	ret = ioctl(syscall_dev, IOCTL_MEM_OVERLAY_CLEANUP_CMD, &cleanup_req);
	if (ret) {
		printf("ERROR: could not call 'IOCTL_MMAP_CMD': %d\n", ret);
		res = EXIT_FAILURE;
	}
	printf("called IOCTL_MEM_OVERLAY_CLEANUP_CMD\n");

close_syscall_dev:
	close(syscall_dev);
free_elements:
	free(req.segments);
	munmap(overlay_map, total_size);
close_overlay:
	close(overlay_fd);
unmap_base:
	munmap(base_mmap, total_size);
close_base:
	close(base_fd);

	printf("done\n");
	return res;
}
