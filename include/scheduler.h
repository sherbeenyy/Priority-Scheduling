#ifndef PRIORITY_SCHEDULER_H
#define PRIORITY_SCHEDULER_H

#include <stdbool.h>

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int base_priority;           // static priority assigned initially
    int effective_priority;      // priority used for scheduling (with aging)
    int start_time;              // first time it got CPU
    int finish_time;             // completion time
    int last_ready_time;         // last time it entered ready queue
    int waiting_time_accum;      // total time spent waiting in ready queue
    bool started;
} Process;

typedef struct {
    int *data;        // heap of indices to processes array
    int size;         // number of items currently in heap
    int capacity;     // max capacity
} ReadyQueue;

typedef struct {
    Process *procs;
    int nprocs;
    bool preemptive;           // true => preemptive priority; false => non-preemptive
    bool aging_enabled;        // enable priority aging
    int aging_interval;        // ticks between aging applications
    int aging_increment;       // how much to boost waiting processes

    int time;                  // current time (tick)
    int completed;             // number of finished processes
    int current;               // index of running process (-1 if idle)

    ReadyQueue rq;             // ready queue (max-heap by effective priority)
} Scheduler;

// ReadyQueue API
void rq_init(ReadyQueue *rq, int capacity);
void rq_free(ReadyQueue *rq);
bool rq_is_empty(ReadyQueue *rq);
int rq_peek(ReadyQueue *rq);      // returns index of highest-priority process
int rq_pop(ReadyQueue *rq, Scheduler *sch);       // removes and returns index
void rq_push(ReadyQueue *rq, Scheduler *sch, int proc_index);
void rq_reheapify(ReadyQueue *rq, Scheduler *sch);

// Scheduler API
void scheduler_init(Scheduler *sch, Process *procs, int nprocs,
                    bool preemptive, bool aging_enabled,
                    int aging_interval, int aging_increment);
void scheduler_run(Scheduler *sch, bool verbose);

#endif // PRIORITY_SCHEDULER_H
