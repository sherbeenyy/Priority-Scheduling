#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_PROCS 256

static int load_processes_from_file(const char *path, Process *out, int maxn) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }
    int pid, at, bt, pr;
    int n = 0;
    while (n < maxn && fscanf(f, "%d %d %d %d", &pid, &at, &bt, &pr) == 4) {
        out[n].pid = pid;
        out[n].arrival_time = at;
        out[n].burst_time = bt;
        out[n].base_priority = pr;
        n++;
    }
    fclose(f);
    return n;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [--preemptive|--non-preemptive] [--aging <interval> <increment>] [--input <file>] [--quiet]\n", prog);
    printf("\nInput file format: each line => PID ARRIVAL BURST PRIORITY\n");
}

int main(int argc, char **argv) {
    int preemptive = 1;
    int aging = 0;
    int aging_interval = 5;
    int aging_increment = 1;
    const char *input = NULL;
    int verbose = 1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--preemptive") == 0) preemptive = 1;
        else if (strcmp(argv[i], "--non-preemptive") == 0) preemptive = 0;
        else if (strcmp(argv[i], "--aging") == 0) {
            aging = 1;
            if (i + 2 >= argc) { print_usage(argv[0]); return 1; }
            aging_interval = atoi(argv[++i]);
            aging_increment = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) { print_usage(argv[0]); return 1; }
            input = argv[++i];
        } else if (strcmp(argv[i], "--quiet") == 0) {
            verbose = 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    Process procs[MAX_PROCS];
    int n = 0;

    if (input) {
        n = load_processes_from_file(input, procs, MAX_PROCS);
        if (n < 0) return 1;
        if (n == 0) {
            fprintf(stderr, "No processes loaded from %s\n", input);
            return 1;
        }
    } else {
        // Default sample workload
        // PID, arrival, burst, priority
        Process sample[] = {
            { .pid=1, .arrival_time=0, .burst_time=7, .base_priority=2 },
            { .pid=2, .arrival_time=2, .burst_time=4, .base_priority=4 },
            { .pid=3, .arrival_time=4, .burst_time=1, .base_priority=6 },
            { .pid=4, .arrival_time=5, .burst_time=4, .base_priority=3 },
            { .pid=5, .arrival_time=6, .burst_time=6, .base_priority=1 },
        };
        n = (int)(sizeof(sample) / sizeof(sample[0]));
        memcpy(procs, sample, sizeof(sample));
    }

    Scheduler sch;
    scheduler_init(&sch, procs, n, preemptive, aging, aging_interval, aging_increment);

    if (verbose) {
        printf("Priority Scheduling (%s, aging=%s)\n",
               preemptive ? "preemptive" : "non-preemptive",
               aging ? "on" : "off");
    }

    scheduler_run(&sch, verbose);

    return 0;
}
