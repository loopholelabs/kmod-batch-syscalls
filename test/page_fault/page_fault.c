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

#include "../../module.h"

int main() {
    const size_t page_size = getpagesize() * 64;
    const size_t total_size = page_size * 1024 * 8;
    printf("Using pagesize %lu with total size %lu\n", page_size, total_size);

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

    if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
        printf("ERROR: could not measure 'after' time for base mmap\n");
        munmap(baseMap, total_size);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"base.bin\") took %lins (%lims)\n", after.tv_nsec - before.tv_nsec, (after.tv_nsec - before.tv_nsec)/1000000);

    struct mmap _mmap;
    _mmap.base_addr = *(unsigned long*)(&baseMap);
    _mmap.path = "overlay.bin";
    _mmap.mode = O_RDONLY;
    _mmap.size = total_size / (page_size * 2);
    _mmap.elements = malloc(sizeof(struct mmap_element) * _mmap.size);
    memset(_mmap.elements, 0, sizeof(struct mmap_element) * _mmap.size);

    printf("requesting %u operations and sending %lu bytes worth of mmap elements\n", _mmap.size, sizeof(struct mmap_element) * _mmap.size);

    int i = 0;
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        _mmap.elements[i].length = page_size;
        _mmap.elements[i].offset = offset;
        i++;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
        printf("ERROR: could not measure 'before' time for overlay mmap\n");
        free(_mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int ret = ioctl(syscall_dev, IOCTL_MMAP_CMD, &_mmap);
    if(ret) {
        printf("ERROR: could not call 'IOCTL_MMAP_CMD': %d\n", ret);
        free(_mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
        printf("ERROR: could not measure 'after' time for overlay mmap\n");
        free(_mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"overlay.bin\") (%i pages) took %lins (%lims)\n", i, after.tv_nsec - before.tv_nsec, (after.tv_nsec - before.tv_nsec)/1000000);

    printf("checking mmap buffer against file contents\n");

    char *buffer = malloc(page_size);
    memset(buffer, 0, page_size);

//    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
//        lseek(overlayFd, offset, SEEK_SET);
//        read(overlayFd, buffer, page_size);
//        if (memcmp(baseMap + offset, buffer, page_size)) {
//            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
//            free(buffer);
//            free(mmap.elements);
//            close(syscall_dev);
//            close(overlayFd);
//            close(baseFd);
//            return EXIT_FAILURE;
//        }
//        memset(buffer, 0, page_size);
//    }

    for (size_t offset = page_size; offset < total_size; offset += page_size) {
        lseek(baseFd, offset, SEEK_SET);
        read(baseFd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
            free(buffer);
            free(_mmap.elements);
            close(syscall_dev);
            close(overlayFd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
    }

    printf("successfully checked mmap buffer against file contents\n");

    sleep(30);

    char *baseMap2 = mmap(NULL, total_size, PROT_READ, MAP_SHARED, baseFd, 0);
    _mmap.base_addr = *(unsigned long*)(&baseMap2);

    ioctl(syscall_dev, IOCTL_MMAP_CMD, &_mmap);

    for (size_t offset = page_size; offset < total_size; offset += page_size) {
        lseek(baseFd, offset, SEEK_SET);
        read(baseFd, buffer, page_size);
        if (memcmp(baseMap2 + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
            free(buffer);
            free(_mmap.elements);
            close(syscall_dev);
            close(overlayFd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
    }

    printf("Sleeping\n");
    sleep(60);

    ret = munmap(baseMap, total_size);
    if(ret) {
        printf("error during munmap: %d\n", ret);
        free(buffer);
        free(_mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    ret = munmap(baseMap2, total_size);
    if(ret) {
        printf("error during munmap: %d\n", ret);
        free(buffer);
        free(_mmap.elements);
        close(syscall_dev);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    free(buffer);
    free(_mmap.elements);
    close(syscall_dev);
    close(overlayFd);
    close(baseFd);
    return EXIT_SUCCESS;
}