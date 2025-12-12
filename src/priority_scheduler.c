#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- ReadyQueue (binary max-heap by effective_priority) ----------
static int compare_proc(const Scheduler *sch, int i, int j) {
    const Process *a = &sch->procs[i];
    const Process *b = &sch->procs[j];
    if (a->effective_priority != b->effective_priority)
        return (a->effective_priority > b->effective_priority) ? 1 : -1;
    // Tie-breaker: earlier arrival first
    if (a->arrival_time != b->arrival_time)
        return (a->arrival_time < b->arrival_time) ? 1 : -1;
    // Final tie-breaker: smaller pid first
    return (a->pid < b->pid) ? 1 : -1;
}

static void heap_swap(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

void rq_init(ReadyQueue *rq, int capacity) {
    rq->data = (int *)malloc(sizeof(int) * capacity);
    rq->size = 0;
    rq->capacity = capacity;
}

void rq_free(ReadyQueue *rq) {
    free(rq->data);
    rq->data = NULL;
    rq->size = rq->capacity = 0;
}

bool rq_is_empty(ReadyQueue *rq) { return rq->size == 0; }

static void sift_up(ReadyQueue *rq, const Scheduler *sch, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (compare_proc(sch, rq->data[idx], rq->data[parent]) > 0) {
            heap_swap(&rq->data[idx], &rq->data[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void sift_down(ReadyQueue *rq, const Scheduler *sch, int idx) {
    while (1) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int largest = idx;
        if (left < rq->size && compare_proc(sch, rq->data[left], rq->data[largest]) > 0)
            largest = left;
        if (right < rq->size && compare_proc(sch, rq->data[right], rq->data[largest]) > 0)
            largest = right;
        if (largest != idx) {
            heap_swap(&rq->data[idx], &rq->data[largest]);
            idx = largest;
        } else {
            break;
        }
    }
}

int rq_peek(ReadyQueue *rq) { return rq->size ? rq->data[0] : -1; }

int rq_pop(ReadyQueue *rq, Scheduler *sch) {
    if (rq->size == 0) return -1;
    int top = rq->data[0];
    rq->data[0] = rq->data[--rq->size];
    sift_down(rq, sch, 0);
    return top;
}

void rq_push(ReadyQueue *rq, Scheduler *sch, int proc_index) {
    if (rq->size >= rq->capacity) {
        rq->capacity = rq->capacity ? rq->capacity * 2 : 8;
        rq->data = (int *)realloc(rq->data, sizeof(int) * rq->capacity);
    }
    rq->data[rq->size++] = proc_index;
    sift_up(rq, sch, rq->size - 1);
}

void rq_reheapify(ReadyQueue *rq, Scheduler *sch) {
    // Rebuild heap using current priorities
    for (int i = (rq->size / 2) - 1; i >= 0; --i) {
        // custom sift_down that uses sch
        int idx = i;
        while (1) {
            int left = 2 * idx + 1;
            int right = 2 * idx + 2;
            int largest = idx;
            if (left < rq->size && compare_proc(sch, rq->data[left], rq->data[largest]) > 0)
                largest = left;
            if (right < rq->size && compare_proc(sch, rq->data[right], rq->data[largest]) > 0)
                largest = right;
            if (largest != idx) {
                heap_swap(&rq->data[idx], &rq->data[largest]);
                idx = largest;
            } else {
                break;
            }
        }
    }
}

// ---------- Scheduler ----------
static void maybe_age_waiting(Scheduler *sch) {
    if (!sch->aging_enabled || sch->aging_interval <= 0) return;
    if (sch->time > 0 && sch->time % sch->aging_interval == 0) {
        for (int i = 0; i < sch->rq.size; ++i) {
            int idx = sch->rq.data[i];
            sch->procs[idx].effective_priority += sch->aging_increment;
        }
        rq_reheapify(&sch->rq, sch);
    }
}

static void admit_arrivals(Scheduler *sch) {
    for (int i = 0; i < sch->nprocs; ++i) {
        Process *p = &sch->procs[i];
        if (p->arrival_time == sch->time) {
            p->effective_priority = p->base_priority; // reset on arrival
            p->last_ready_time = sch->time;
            rq_push(&sch->rq, sch, i);
        }
    }
}

static void dispatch_if_needed(Scheduler *sch) {
    if (sch->current == -1) {
        if (!rq_is_empty(&sch->rq)) {
            sch->current = rq_pop(&sch->rq, sch);
            Process *p = &sch->procs[sch->current];
            if (!p->started) {
                p->started = true;
                p->start_time = sch->time;
            }
            p->waiting_time_accum += (sch->time - p->last_ready_time);
        }
        return;
    }

    if (!sch->preemptive) return; // keep current until completion

    // Preempt if someone in ready queue has higher effective priority
    if (!rq_is_empty(&sch->rq)) {
        int cand = rq_peek(&sch->rq);
        if (compare_proc(sch, cand, sch->current) > 0) {
            // Preempt current
            Process *cur = &sch->procs[sch->current];
            cur->last_ready_time = sch->time; // re-enter ready queue now
            rq_push(&sch->rq, sch, sch->current);

            sch->current = rq_pop(&sch->rq, sch);
            Process *p = &sch->procs[sch->current];
            if (!p->started) {
                p->started = true;
                p->start_time = sch->time;
            }
            p->waiting_time_accum += (sch->time - p->last_ready_time);
        }
    }
}

void scheduler_init(Scheduler *sch, Process *procs, int nprocs,
                    bool preemptive, bool aging_enabled,
                    int aging_interval, int aging_increment) {
    sch->procs = procs;
    sch->nprocs = nprocs;
    sch->preemptive = preemptive;
    sch->aging_enabled = aging_enabled;
    sch->aging_interval = aging_interval;
    sch->aging_increment = aging_increment;
    sch->time = 0;
    sch->completed = 0;
    sch->current = -1;

    for (int i = 0; i < nprocs; ++i) {
        sch->procs[i].remaining_time = sch->procs[i].burst_time;
        sch->procs[i].effective_priority = sch->procs[i].base_priority;
        sch->procs[i].started = false;
        sch->procs[i].waiting_time_accum = 0;
        sch->procs[i].start_time = -1;
        sch->procs[i].finish_time = -1;
        sch->procs[i].last_ready_time = -1;
    }

    rq_init(&sch->rq, nprocs > 0 ? nprocs : 8);
}

void scheduler_run(Scheduler *sch, bool verbose) {
    if (verbose) {
        printf("Time | Running PID\n");
        printf("------------------\n");
    }
    while (sch->completed < sch->nprocs) {
        admit_arrivals(sch);
        maybe_age_waiting(sch);
        dispatch_if_needed(sch);

        if (verbose) {
            if (sch->current == -1) printf("%4d | idle\n", sch->time);
            else printf("%4d | %d\n", sch->time, sch->procs[sch->current].pid);
        }

        // Execute one tick
        if (sch->current != -1) {
            Process *p = &sch->procs[sch->current];
            p->remaining_time--;
            if (p->remaining_time == 0) {
                p->finish_time = sch->time + 1; // completes at end of this tick
                sch->completed++;
                sch->current = -1;
            }
        }

        sch->time++;
    }

    // Summary
    int total_wait = 0, total_turn = 0;
    if (verbose) printf("\nResults:\n");
    for (int i = 0; i < sch->nprocs; ++i) {
        Process *p = &sch->procs[i];
        int turnaround = p->finish_time - p->arrival_time;
        int waiting = p->waiting_time_accum;
        total_wait += waiting;
        total_turn += turnaround;
        if (verbose) {
            printf("PID %d: start=%d finish=%d wait=%d turnaround=%d priority=%d\n",
                   p->pid, p->start_time, p->finish_time, waiting, turnaround, p->base_priority);
        }
    }
    double avg_wait = (double)total_wait / sch->nprocs;
    double avg_turn = (double)total_turn / sch->nprocs;
    if (verbose) {
        printf("\nAvg waiting=%.2f, Avg turnaround=%.2f\n", avg_wait, avg_turn);
    }

    rq_free(&sch->rq);
}
