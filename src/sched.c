
#include "sched.h"
#include "queue.h"
#include "cpu.h"
#include "timer.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;
static pthread_mutex_t boost_lock;

#define THRESHOLD 5

#ifdef MLQ_SCHED
struct sche_args
{
    struct timer_id_t *timer_id;
    int id;
};

static struct queue_t mlq_ready_queue[MAX_PRIO];
static int mlq_ready_queue_slot[MAX_PRIO];
static uint8_t mask_array[MAX_PRIO] = {0};

pthread_t sche;
struct sche_args *args;

static void init_slot()
{
    for (int i = 0; i < MAX_PRIO; i++)
    {
        mlq_ready_queue_slot[i] = MAX_PRIO - i;
    }
}

static void print_queue()
{
    for (int i = 0; i < MAX_PRIO; i++)
    {
        if (empty(&mlq_ready_queue[i]))
            continue;
        printf("Queue %d time %d: ", i,
        mlq_ready_queue[i].total_waiting_time);
        for (int j = 0; j < mlq_ready_queue[i].size; j++)
        {
            printf("(%d %d)", mlq_ready_queue[i].proc[j]->pid,
                   mlq_ready_queue[i].proc[j]->waiting_time);
        }
        printf("\n");
    }
}

void init_sched(void *timer_id)
{
#ifdef MLQ_SCHED
    int i;
    for (i = 0; i < MAX_PRIO; i++)
    {
        mlq_ready_queue[i].size = 0;
        mlq_ready_queue[i].total_waiting_time = 0;
    }
    init_slot();
    args = malloc(sizeof(struct sche_args));
    args->timer_id = (struct timer_id_t *)timer_id;
#endif
    ready_queue.size = 0;
    run_queue.size = 0;
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&boost_lock, NULL);
}

static int highest_suitable_queue()
{
    for (int i = 0; i < MAX_PRIO; i++)
    {
        if (mask_array[i] == 1)
        {
            if (mlq_ready_queue_slot[i] > 0)
            {
                return i;
            }
        }
    }
    return -1;
}

#endif

int queue_empty(void)
{
#ifdef MLQ_SCHED
    unsigned long prio;
    for (prio = 0; prio < MAX_PRIO; prio++)
        if (!empty(&mlq_ready_queue[prio]))
            return 0;
#endif
    return 1;
}

#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */

struct pcb_t *get_mlq_proc(void)
{
    struct pcb_t *proc = NULL;
    /*TODO: get a process from PRIORITY [ready_queue].
     * Remember to use lock to protect the queue.
     * */

    pthread_mutex_lock(&queue_lock);
    int prio_index = highest_suitable_queue();

    // traverse all queues and not found any suitable process
    if (prio_index == -1)
    {
        init_slot();
        prio_index = highest_suitable_queue();
    }

    if (prio_index != -1)
    {
        proc = dequeue(&mlq_ready_queue[prio_index]);
       
        
        mlq_ready_queue_slot[prio_index]--;

        if (empty(&mlq_ready_queue[prio_index]))
        {
           mlq_ready_queue[prio_index].total_waiting_time=0; 
           mask_array[prio_index] = 0;
        }
        else{
           mlq_ready_queue[prio_index].proc[0]->waiting_time
           +=proc->waiting_time;
        }
    }
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_mlq_proc(struct pcb_t *proc)
{
    pthread_mutex_lock(&queue_lock);
    proc->waiting_time = THRESHOLD -
                         mlq_ready_queue[proc->prio].total_waiting_time;
    mlq_ready_queue[proc->prio].total_waiting_time = THRESHOLD;
    proc->up_prio_time=0;
    enqueue(&mlq_ready_queue[proc->prio], proc);
    mask_array[proc->prio] = 1;
#ifdef SCHED_INFO
    print_queue();
#endif
    pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc, int prio)
{
    pthread_mutex_lock(&queue_lock);
    proc->waiting_time = THRESHOLD -
                         mlq_ready_queue[prio].total_waiting_time;
    mlq_ready_queue[prio].total_waiting_time = THRESHOLD;

    mask_array[prio] = 1;

    enqueue(&mlq_ready_queue[prio], proc);
#ifdef SCHED_INFO
    print_queue();
#endif
    pthread_mutex_unlock(&queue_lock);
}

void sche_thread_create(void)
{
    pthread_create(&sche, NULL, prio_boost_process, (void *)args);
}

void sche_thread_wait(void)
{
    pthread_join(sche, NULL);
}

void *prio_boost_process(void *args)
{
    struct timer_id_t *timer_id = ((struct sche_args *)args)->timer_id;
    while (1)
    {
        printf("Boosting process at %d:\n",current_time());
        if (done_load && queue_empty())
        {
            printf("\tBoost mechanism have been finished\n");
            break;
        }
        pthread_mutex_lock(&queue_lock);
        for (int i = 0; i < MAX_PRIO; i++)
        {
            if (!empty(&mlq_ready_queue[i]))
            {
                int wait_time;
                if (mlq_ready_queue[i].proc[0]->up_prio_time < 2)
                {
                    if (mlq_ready_queue[i].total_waiting_time > 0 
                    && mlq_ready_queue[i].proc[0]->waiting_time > 0)
                    {
                        mlq_ready_queue[i].total_waiting_time--;
                        mlq_ready_queue[i].proc[0]->waiting_time--;
                    }

                    printf("\tProcess %d in queue %d has waiting time %d %d\n",
                        mlq_ready_queue[i].proc[0]->pid, i,
                        mlq_ready_queue[i].total_waiting_time, 
                        mlq_ready_queue[i].proc[0]->waiting_time);
                        
                    if (mlq_ready_queue[i].proc[0]->waiting_time <= 0&&i-1>=0)
                    {
                            struct pcb_t *proc = dequeue(&mlq_ready_queue[i]);
                             

                            
                            if (empty(&mlq_ready_queue[i]))
                            {
                                mask_array[i] = 0;
                                mlq_ready_queue[i].
                                total_waiting_time=0;
    
                            }
                            else
                            {
                                mlq_ready_queue[i].proc[0]->waiting_time
                                +=proc->waiting_time;
                             }

                            int new_prio = i - 1;
                            proc->up_prio_time++;
                            proc->waiting_time = THRESHOLD -
                                                mlq_ready_queue[new_prio].total_waiting_time;
                            mlq_ready_queue[new_prio].total_waiting_time = THRESHOLD;

                            mask_array[new_prio] = 1;

                            enqueue(&mlq_ready_queue[new_prio], proc);
                            printf("\tBoosted process %d from %d to %d\n", proc->pid, i, new_prio);
                        }
                }
                else 
                    printf("\tProcess %d has already been boosted 2 times\n", mlq_ready_queue[i].proc[0]->pid);
            }
        }
        pthread_mutex_unlock(&queue_lock);
        next_slot(timer_id);
    }
    detach_event(timer_id);
    pthread_exit(NULL);
}

struct pcb_t *get_proc(void)
{
    return get_mlq_proc();
}

void put_proc(struct pcb_t *proc)
{
    return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc)
{
    return add_mlq_proc(proc, proc->prio);
}
#else
struct pcb_t *get_proc(void)
{
    struct pcb_t *proc = NULL;
    /*TODO: get a process from [ready_queue].
     * Remember to use lock to protect the queue.
     * */
    pthread_mutex_lock(&queue_lock);
    proc = dequeue(&ready_queue);
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

void put_proc(struct pcb_t *proc)
{
    pthread_mutex_lock(&queue_lock);
    enqueue(&run_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc)
{
    pthread_mutex_lock(&queue_lock);
    enqueue(&ready_queue, proc);
    pthread_mutex_unlock(&queue_lock);
}
#endif
