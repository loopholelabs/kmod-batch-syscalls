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
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <bits/time.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "../module.h"

int main() {
    const size_t page_size = 4096 * 64;
    const size_t total_size = page_size * 1024;

    int baseFd = open("base.bin", O_RDONLY);
    if (baseFd < 0) {
        printf("ERROR: could not open base file: %d\n", baseFd);
        return EXIT_FAILURE;
    }

    int overlayFd = open("overlay.bin", O_RDONLY);
    if (overlayFd < 0) {
        printf("ERROR: could not open overlay file: %d\n", overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int syscall_dev = open("/dev/batch_syscalls", O_WRONLY);
    if(syscall_dev < 0) {
        printf("ERROR: could not open /dev/batch_syscalls: %d\n", syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    struct timespec before, after;

    if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
        printf("ERROR: could not measure 'before' time for base mmap\n");
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    char *baseMap = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, baseFd, 0);

    if (baseMap == MAP_FAILED) {
        printf("ERROR: could not mmap base file\n");
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &after) == -1) {
        printf("ERROR: could not measure 'after' time for base mmap\n");
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"base.bin\") took %lins\n", after.tv_nsec - before.tv_nsec);

    struct mmap mmap;
    mmap.path = "overlay.bin";
    mmap.flag = MAP_PRIVATE | MAP_FIXED;
    mmap.prot = PROT_READ;
    mmap.mode = O_RDONLY;
    mmap.size = total_size / (page_size * 2);
    mmap.elements = malloc(sizeof(struct mmap_element) * mmap.size);
    memset(mmap.elements, 0, sizeof(struct mmap_element) * mmap.size);

    int i = 0;
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        mmap.elements[i].addr = *(unsigned long*)(&baseMap) + offset;
        mmap.elements[i].len = page_size;
        mmap.elements[i].offset = offset;
        i++;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &before) == -1) {
        printf("ERROR: could not measure 'before' time for overlay mmap\n");
        free(mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int ret = ioctl(syscall_dev, IOCTL_MMAP_CMD, &mmap);
    if(ret) {
        printf("ERROR: could not call 'IOCTL_MMAP_CMD': %d\n", ret);
        free(mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &after) == -1) {
        printf("ERROR: could not measure 'after' time for overlay mmap\n");
        free(mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"overlay.bin\") (%i pages) took %lins\n", i, after.tv_nsec - before.tv_nsec);

    printf("running sanity check to file contents\n");

    char *buffer = malloc(page_size);
    memset(buffer, 0, page_size);

    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        lseek(overlayFd, offset, SEEK_SET);
        read(overlayFd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
            free(buffer);
            free(mmap.elements);
            close(syscall_dev);
            close(overlayFd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
    }

    for (size_t offset = page_size; offset < total_size; offset += page_size * 2) {
        lseek(baseFd, offset, SEEK_SET);
        read(baseFd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
            free(buffer);
            free(mmap.elements);
            close(syscall_dev);
            close(overlayFd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
    }

    printf("successfully checked mmap buffer against file contents\n");

    ret = munmap(baseMap, total_size);
    if(ret) {
        printf("error during munmap: %d\n", ret);
        free(buffer);
        free(mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    free(buffer);
    free(mmap.elements);
    close(syscall_dev);
    close(overlayFd);
    close(baseFd);
    return EXIT_SUCCESS;
}