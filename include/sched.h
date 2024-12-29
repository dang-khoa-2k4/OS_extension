#ifndef SCHED_H
#define SCHED_H

#include "common.h"

#ifndef MLQ_SCHED
#define MLQ_SCHED
#endif

//#define MAX_PRIO 139

int queue_empty(void);

void init_sched(void *timer_id);
void finish_scheduler(void);

/* Get the next process from ready queue */
struct pcb_t * get_proc(void);

/* Put a process back to run queue */
void put_proc(struct pcb_t * proc);

/* Add a new process to ready queue */
void add_proc(struct pcb_t * proc);
void * prio_boost_process(void * args);
void sche_thread_wait(void);
void sche_thread_create(void);
#endif


