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
    const size_t page_size = getpagesize();
    const size_t total_size = page_size * 1024 * 1024;

    printf("using pagesize %lu with total size %lu\n", page_size, total_size);

    int baseFd = open("base.bin", O_RDONLY);
    if (baseFd < 0) {
        printf("ERROR: could not open base file: %d\n", baseFd);
        return EXIT_FAILURE;
    }

    int overlay1Fd = open("overlay1.bin", O_RDONLY);
    if (overlay1Fd < 0) {
        printf("ERROR: could not open overlay1 file: %d\n", overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int overlay2Fd = open("overlay2.bin", O_RDONLY);
    if (overlay2Fd < 0) {
        printf("ERROR: could not open overlay2 file: %d\n", overlay2Fd);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    clock_t time = clock();
    char *baseMap1 = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, baseFd, 0);
    time = clock() - time;
    if (baseMap1 == MAP_FAILED) {
        printf("ERROR: could not mmap base file (baseMap1)\n");
        close(overlay2Fd);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"base.bin\") for baseMap1 took %fms\n", (double)time/CLOCKS_PER_SEC*1000);

    time = clock();
    char *baseMap2 = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, baseFd, 0);
    time = clock() - time;
    if (baseMap2 == MAP_FAILED) {
        printf("ERROR: could not mmap base file (baseMap2)\n");
        munmap(baseMap1, total_size);
        close(overlay2Fd);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    printf("mmap(\"base.bin\") for baseMap2 took %fms\n", (double)time/CLOCKS_PER_SEC*1000);

    int i = 0;
    time = clock();
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        char *overlayMap = mmap(baseMap1 + offset, page_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, overlay1Fd, offset);
        if (overlayMap == MAP_FAILED) {
            printf("ERROR: could not mmap overlay1 file for index %d\n", i);
            munmap(baseMap2, total_size);
            munmap(baseMap1, total_size);
            close(overlay2Fd);
            close(overlay1Fd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        i++;
    }

    time = clock() - time;
    printf("mmap(\"overlay1.bin\") took %fms\n", (double)time/CLOCKS_PER_SEC*1000);

    char *buffer = malloc(page_size);
    memset(buffer, 0, page_size);

    time = clock();
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
#ifdef VERIFY
        lseek(overlay1Fd, offset, SEEK_SET);
        read(overlay1Fd, buffer, page_size);
        if (memcmp(baseMap1 + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the overlay1 file contents at offset %lu\n", offset);
            free(buffer);
            munmap(baseMap2, total_size);
            munmap(baseMap1, total_size);
            close(overlay2Fd);
            close(overlay1Fd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
#else
        memcpy(buffer, baseMap1 + offset, page_size);
#endif
    }

    for (size_t offset = page_size; offset < total_size; offset += page_size * 2) {
#ifdef VERIFY
        lseek(baseFd, offset, SEEK_SET);
        read(baseFd, buffer, page_size);
        if (memcmp(baseMap1 + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the base file contents at offset %lu (baseMap1)\n", offset);
            free(buffer);
            munmap(baseMap2, total_size);
            munmap(baseMap1, total_size);
            close(overlay2Fd);
            close(overlay1Fd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
#else
        memcpy(buffer, baseMap1 + offset, page_size);
#endif
    }

    time = clock() - time;
    printf("page faults for %d pages took %fms (baseMap1)\n", i, (double)time/CLOCKS_PER_SEC*1000);

#ifdef VERIFY
    printf("successfully verified mmap (baseMap1)\n");
#endif

    int ret = munmap(baseMap2, total_size);
    if(ret) {
        printf("error during munmap: %d (baseMap2)\n", ret);
        free(buffer);
        munmap(baseMap1, total_size);
        close(overlay2Fd);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    ret = munmap(baseMap1, total_size);
    if(ret) {
        printf("error during munmap: %d (baseMap1)\n", ret);
        free(buffer);
        close(overlay2Fd);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    free(buffer);
    close(overlay2Fd);
    close(overlay1Fd);
    close(baseFd);
    return EXIT_SUCCESS;
}