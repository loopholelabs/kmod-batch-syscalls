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
#include <string.h>
#include <unistd.h>

#include <bits/time.h>


#include <sys/mman.h>


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

    struct timespec before, after;

    if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
        printf("ERROR: could not measure 'before' time for base mmap\n");
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    char *baseMap = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, baseFd, 0);
    if (baseMap == MAP_FAILED) {
        printf("ERROR: could not mmap base file\n");
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
        printf("ERROR: could not measure 'after' time for base mmap\n");
        munmap(baseMap, total_size);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"base.bin\") took %lins (%lims)\n", after.tv_nsec - before.tv_nsec, (after.tv_nsec - before.tv_nsec)/1000000);

    if (clock_gettime(CLOCK_MONOTONIC, &before) < 0) {
        printf("ERROR: could not measure 'before' time for overlay mmap\n");
        munmap(baseMap, total_size);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int i = 0;
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        char *overlayMap = mmap(baseMap + offset, page_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, overlayFd, offset);
        if (overlayMap == MAP_FAILED) {
            printf("ERROR: could not mmap overlay file\n");
            munmap(baseMap, total_size);
            close(baseFd);
            close(overlayFd);
            return EXIT_FAILURE;
        }
        i++;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &after) < 0) {
        printf("ERROR: could not measure 'after' time for overlay mmap\n");
        munmap(baseMap, total_size);
        close(baseFd);
        close(overlayFd);

        return EXIT_FAILURE;
    }

    printf("mmap(\"overlay.bin\") (%i pages) took %lins (%lims)\n", i, after.tv_nsec - before.tv_nsec, (after.tv_nsec - before.tv_nsec)/1000000);

    printf("checking mmap buffer against file contents\n");

    char *buffer = malloc(page_size);
    memset(buffer, 0, page_size);

    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        lseek(overlayFd, offset, SEEK_SET);
        read(overlayFd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the file contents at offset %lu\n", offset);
            free(buffer);
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
            close(overlayFd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
    }

    printf("successfully checked mmap buffer against file contents\n");

    int ret = munmap(baseMap, total_size);
    if(ret) {
        printf("error during munmap: %d\n", ret);
        free(buffer);
        close(overlayFd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    free(buffer);
    close(overlayFd);
    close(baseFd);
    return EXIT_SUCCESS;
}