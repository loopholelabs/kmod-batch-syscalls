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

void time_diff(struct timespec* before, struct timespec* after, struct timespec* diff) {
    if ((before->tv_nsec - after->tv_nsec) < 0) {
        diff->tv_sec = before->tv_sec - after->tv_sec - 1;
        diff->tv_nsec = 1000000000 + before->tv_nsec - after->tv_nsec;
    } else {
        diff->tv_sec = before->tv_sec - after->tv_sec;
        diff->tv_nsec = before->tv_nsec - after->tv_nsec;
    }
}

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

    struct timespec before, after, diff;
    memset(&before, 0, sizeof(struct timespec));
    memset(&after, 0, sizeof(struct timespec));
    memset(&diff, 0, sizeof(struct timespec));

    if (clock_gettime(CLOCK_REALTIME, &before) < 0) {
        printf("ERROR: could not measure 'before' time for base mmap\n");
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    char *baseMap = mmap(NULL, total_size, PROT_READ, MAP_PRIVATE, baseFd, 0);
    if (baseMap == MAP_FAILED) {
        printf("ERROR: could not mmap base file\n");
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_REALTIME, &after) < 0) {
        printf("ERROR: could not measure 'after' time for base mmap\n");
        munmap(baseMap, total_size);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    time_diff(&before, &after, &diff);
    printf("mmap(\"base.bin\") took %lins (%lims)\n", diff.tv_nsec, (diff.tv_nsec)/1000000);
    memset(&before, 0, sizeof(struct timespec));
    memset(&after, 0, sizeof(struct timespec));
    memset(&diff, 0, sizeof(struct timespec));

    if (clock_gettime(CLOCK_REALTIME, &before) < 0) {
        printf("ERROR: could not measure 'before' time for overlay1 mmap\n");
        munmap(baseMap, total_size);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    int i = 0;
    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
        char *overlayMap = mmap(baseMap + offset, page_size, PROT_READ, MAP_PRIVATE | MAP_FIXED, overlay1Fd, offset);
        if (overlayMap == MAP_FAILED) {
            printf("ERROR: could not mmap overlay1 file for index %d\n", i);
            munmap(baseMap, total_size);
            close(baseFd);
            close(overlay1Fd);
            return EXIT_FAILURE;
        }
        i++;
    }

    if (clock_gettime(CLOCK_REALTIME, &after) < 0) {
        printf("ERROR: could not measure 'after' time for overlay1 mmap\n");
        munmap(baseMap, total_size);
        close(baseFd);
        close(overlay1Fd);

        return EXIT_FAILURE;
    }

    time_diff(&before, &after, &diff);
    printf("mmap(\"overlay1.bin\") took %lins (%lims)\n", diff.tv_nsec, (diff.tv_nsec)/1000000);
    memset(&before, 0, sizeof(struct timespec));
    memset(&after, 0, sizeof(struct timespec));
    memset(&diff, 0, sizeof(struct timespec));

    char *buffer = malloc(page_size);
    memset(buffer, 0, page_size);

    if (clock_gettime(CLOCK_REALTIME, &before) < 0) {
        printf("ERROR: could not measure 'before' time for page faults\n");
        free(buffer);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    for (size_t offset = 0; offset < total_size; offset += page_size * 2) {
#ifdef VERIFY
        lseek(overlay1Fd, offset, SEEK_SET);
        read(overlay1Fd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the overlay file contents at offset %lu\n", offset);
            free(buffer);
            close(overlay1Fd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
#else
        memcpy(buffer, baseMap + offset, page_size);
#endif
    }

    for (size_t offset = page_size; offset < total_size; offset += page_size * 2) {
#ifdef VERIFY
        lseek(baseFd, offset, SEEK_SET);
        read(baseFd, buffer, page_size);
        if (memcmp(baseMap + offset, buffer, page_size)) {
            printf("ERROR: mmap buffer does not match the base file contents at offset %lu\n", offset);
            free(buffer);
            close(overlay1Fd);
            close(baseFd);
            return EXIT_FAILURE;
        }
        memset(buffer, 0, page_size);
#else
        memcpy(buffer, baseMap + offset, page_size);
#endif
    }

    if (clock_gettime(CLOCK_REALTIME, &after) < 0) {
        printf("ERROR: could not measure 'after' time for page faults\n");
        free(buffer);
        munmap(baseMap, total_size);
        close(baseFd);
        close(overlay1Fd);
        return EXIT_FAILURE;
    }

    time_diff(&before, &after, &diff);
    printf("page faults for %d pages took %lins (%lims)\n", i, diff.tv_nsec, (diff.tv_nsec)/1000000);

#ifdef VERIFY
    printf("successfully verified mmap\n");
#endif

    int ret = munmap(baseMap, total_size);
    if(ret) {
        printf("error during munmap: %d\n", ret);
        free(buffer);
        close(overlay1Fd);
        close(baseFd);
        return EXIT_FAILURE;
    }

    free(buffer);
    close(overlay1Fd);
    close(baseFd);
    return EXIT_SUCCESS;
}