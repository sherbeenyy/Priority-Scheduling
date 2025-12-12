# Priority Scheduling in C

This project implements both non-preemptive and preemptive Priority Scheduling with optional aging to reduce starvation. It includes a simple CLI runner and a clear separation of the scheduler core from the driver.

## Design

- **Process model:** Each process tracks `pid`, `arrival_time`, `burst_time`, `remaining_time`, `base_priority`, and `effective_priority` (which can increase via aging). Waiting and turnaround times are computed.
- **Ready queue:** A binary max-heap of process indices ordered by `effective_priority` (higher wins). Tie-breakers: earlier arrival, then smaller PID.
- **Dispatch rules:**
  - **Non-preemptive:** Once dispatched, a process runs until completion.
  - **Preemptive:** A newly ready higher-priority process preempts the current one at tick boundaries.
- **Aging:** At a configured interval, all waiting processes in the ready queue get `effective_priority += aging_increment` to reduce starvation.
- **Metrics:** Start time, finish time, waiting time, and turnaround time reported; averages computed.

## Build & Run

```sh
# Build
make -C /path/to/process_scheduler

# Run with built-in sample workload
/path/to/process_scheduler/bin/scheduler --preemptive --aging 5 1

# Or non-preemptive without aging
/path/to/process_scheduler/bin/scheduler --non-preemptive --quiet

# Run with an input file (PID ARRIVAL BURST PRIORITY per line)
/path/to/the/file --input processes.txt --preemptive
```

Example `processes.txt`:

```
1 0 7 2
2 2 4 4
3 4 1 6
4 5 4 3
5 6 6 1
```

## How It Interacts with an OS

There are two contexts:

- **Educational simulator (this project):** Models the kernel’s ready queue, dispatch, preemption, and aging in user-space to help understand behavior and measure metrics.
- **Real OS integration (Linux):** The actual CPU scheduler is in the kernel. You cannot replace it from user-space, but you can influence scheduling of processes/threads via system calls:
  - `nice(2)` / `setpriority(2)`: Adjust niceness for `SCHED_OTHER` (normal) tasks; higher nice means lower priority.
  - `sched_setscheduler(2)`: Set real-time policies (`SCHED_FIFO`, `SCHED_RR`, `SCHED_DEADLINE`). Requires CAP_SYS_NICE (root) for raising priority.
  - `pthread_setschedparam(3)`: Adjust thread-level policy/priority in the same ways.

If you wanted to implement a custom priority scheduler inside an OS, you would:

- Maintain a priority-ordered ready queue in the kernel’s runqueue.
- Hook into context-switch and timer interrupt paths to enforce preemption (preempt current if a higher priority task becomes runnable).
- Apply aging periodically to avoid starvation.
- Update per-task accounting (waiting/turnaround) for observability.

This simulator mirrors those responsibilities but runs entirely in user-space.

### Demo: influence scheduling priority on Linux

The following snippet increases the niceness (reduces priority) of the current process, which is allowed without root. Decreasing niceness (negative values) generally requires privileges.

```c
#include <sys/resource.h>
#include <stdio.h>

int main() {
    int ret = setpriority(PRIO_PROCESS, 0, 5); // make process nicer
    if (ret != 0) perror("setpriority");
    return 0;
}
```

For real-time priorities (fixed-priority scheduling), you can use `sched_setscheduler` with `SCHED_FIFO`/`SCHED_RR`, e.g.,

```c
#include <sched.h>
#include <stdio.h>

int main() {
    struct sched_param sp = { .sched_priority = 50 }; // 1..99
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) perror("sched_setscheduler");
    return 0;
}
```

Note: this typically requires root or `CAP_SYS_NICE`. Linux’s default scheduler is CFS (Completely Fair Scheduler), which is not fixed-priority; real-time policies allow priority-based preemption per POSIX.

## Files

- `include/scheduler.h`: Public types and APIs for the scheduler.
- `src/priority_scheduler.c`: Implementation of ready queue and scheduling logic.
- `src/main.c`: CLI runner with sample or file-driven workloads.
- `Makefile`: Build targets.

## Extending

- Add I/O (blocking) by moving processes between ready and waiting lists.
- Support time-sliced preemptive priority with quantum.
- Export a Gantt chart to a file for visualization.
- Add unit tests verifying tie-breakers and aging behavior.
