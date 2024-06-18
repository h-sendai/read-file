#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "drop-page-cache.h"
#include "get_num.h"

#define MAX_CHILD 1024

//int pipe_to_child[MAX_CHILD][2];
//int pipe_from_child[MAX_CHILD][2];
pid_t pid;
struct timeval start_time;

struct {
    int debug;
    int dont_drop_pagecache;
    int use_direct_io;
    int fadv_sequential;
    int fadv_random;
    int record_time;
    long target_read_bytes;
    int sleep_usec;
} opts = { 0, 0, 0, 0, 0, 0, 0, 0 };

struct time_record {
    struct timeval tv;
    int    cpu_num;
};

int usage()
{
    char msg[] = "Usage: read-file options filename\n"
                 "Options\n"
                 "    -d (debug)\n"
                 "    -b bufsize (64k)\n"
                 "    -i (O_DIRECT)\n"
                 "    -s (FADV_SEQUENTIAL)\n"
                 "    -r (FADV_RANDOM)\n"
                 "    -D (dont-drop-page-cache)\n"
                 "    -n total_read_bytes (exit after read this bytes)\n"
                 "    -t record_time (filename: time.<pid>)\n"
                 "    -u sleep_usec (default: don't sleep after every read())\n"
                 "If not -D option is not specified, drop page cache before read()\n"
                 "-s and -r are mutually exclusive\n";
    fprintf(stderr, "%s", msg);

    return 0;
}

off_t get_filesize(char *path)
{
    struct stat st;
    if (stat(path, &st) < 0) {
        warn("stat");
        return -1;
    }

    return st.st_size;
}

int child_proc(char *filename, int bufsize)
{
    int n;
    char *data_buf;
    //struct timeval *ts = NULL;
    struct time_record *time_record = NULL;

    //int index_ts = 0;
    int time_record_index = 0;

    if (opts.use_direct_io) {
        data_buf = aligned_alloc(512, bufsize);
    }
    else {
        data_buf = malloc(bufsize);
    }
    if (data_buf == NULL) {
        err(1, "malloc for %s", filename);
    }

    if (opts.record_time) {
        off_t filesize = get_filesize(filename);
        if (filesize < 0) {
            errx(1, "get_filesize()");
        }
        int n_time_record = filesize / bufsize;
        n_time_record += 1; /* in case of filesize/bufsize if not integer */
        //ts = (struct timeval *)malloc(sizeof(struct timeval)*n_ts);
        time_record = (struct time_record *)malloc(sizeof(struct time_record)*n_time_record);
        if (time_record == NULL) {
            err(1, "malloc for ts");
        }
    };

    int open_flags = O_RDONLY;
    if (opts.use_direct_io) {
        open_flags |= O_DIRECT;
    }
    int fd = open(filename, open_flags);
    if (fd < 0) {
        err(1, "open for %s", filename);
    }

    int advice = 0;
    if (opts.fadv_sequential) {
        advice = POSIX_FADV_SEQUENTIAL;
    }
    else if (opts.fadv_random) {
        advice = POSIX_FADV_RANDOM;
    }
    /* if more advise, write here */

    if (opts.debug) {
        fprintf(stderr, "advice: %d\n", advice);
    }

    if (advice != 0) {
        if (posix_fadvise(fd, 0, 0, advice) < 0) {
            err(1, "posix_fadvise()");
        }
    }

    struct timeval tv0, tv1, elapsed;
    gettimeofday(&tv0, NULL);
    if (opts.debug) {
        fprintf(stderr, "%ld.%06ld start\n", tv0.tv_sec, tv0.tv_usec);
    }

    long total_read_bytes = 0;
    for ( ; ; ) {
        n = read(fd, data_buf, bufsize);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            err(1, "read for %s", filename);
        }
        if (n != bufsize) {
            fprintf(stderr, "partial read\n");
        }

        total_read_bytes += n;
        if (opts.record_time) {
            gettimeofday(&time_record[time_record_index].tv, NULL);
            time_record[time_record_index].cpu_num = sched_getcpu();
            time_record_index ++;
        }
        if (opts.target_read_bytes > 0) {
            if (total_read_bytes >= opts.target_read_bytes) {
                break;
            }
        }
        
        if (opts.sleep_usec > 0) {
            usleep(opts.sleep_usec);
        }
    }
    gettimeofday(&tv1, NULL);
    timersub(&tv1, &tv0, &elapsed);

    double read_time = (double)elapsed.tv_sec + 0.000001*elapsed.tv_usec;
    double read_rate = (double)total_read_bytes / read_time / 1024.0 / 1024.0;
    printf("%.3f MB/s %ld bytes %ld.%06ld sec %s\n", 
        read_rate, total_read_bytes, elapsed.tv_sec, elapsed.tv_usec, filename);
    fflush(stdout);

    if (opts.record_time) {
        char output_filename[1024];
        snprintf(output_filename, sizeof(output_filename), "time.%d", pid);
        FILE *fp = fopen(output_filename, "w");
        if (fp == NULL) {
            err(1, "fopen");
        }
        struct timeval elapsed;
        for (int i = 0; i < time_record_index; ++i) {
            timersub(&time_record[i].tv, &start_time, &elapsed);
            fprintf(fp, "%ld.%06ld %d\n", elapsed.tv_sec, elapsed.tv_usec, i);
        }
    }
        
    return 0;
}

int main(int argc, char *argv[])
{
    long bufsize = 64*1024;
    int c;
    while ( (c = getopt(argc, argv, "hb:dDin:srtu:")) != -1) {
        switch (c) {
            case 'h':
                usage();
                exit(0);
                break;
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'd':
                opts.debug++;
                break;
            case 'D':
                opts.dont_drop_pagecache = 1;
                break;
            case 'i':
                opts.use_direct_io = 1;
                break;
            case 'n':
                opts.target_read_bytes = get_num(optarg);
                break;
            case 's':
                opts.fadv_sequential = 1;
                break;
            case 'r':
                opts.fadv_random = 1;
                break;
            case 't':
                opts.record_time = 1;
                break;
            case 'u':
                opts.sleep_usec = get_num(optarg);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc == 0) {
        usage();
        exit(1);
    }
    char *filename = argv[0];

    if (opts.fadv_sequential && opts.fadv_random) {
        fprintf(stderr, "-s and -r are exclusive\n");
        exit(1);
    }

    if (opts.debug) {
        fprintf(stderr, "argc: %d\n", argc);
    }

    /* used in record time files if record_time option is specified */
    pid = getpid();

    if (opts.dont_drop_pagecache) {
        ;
    }
    else {
        if (drop_page_cache(filename) < 0) {
            errx(1, "cannot drop pagecache: %s\n", filename);
        }
    }

    gettimeofday(&start_time, NULL);
    if (opts.debug >= 2) {
        fprintf(stderr, "parent done\n");
    }

    child_proc(filename, bufsize);

    return 0;
}
