#include "memory.h"
#include "process.h"
#include "wait.h"

typedef struct
{
    list queue;
    process_t *proc;
} waiting_proc_t;

void __waitqueue_wait(waitqueue_t *waitq)
{
    waiting_proc_t *newwait = get_mem(sizeof(*newwait));
    newwait->proc = process_get_cur_task();
    list_add(&newwait->queue, waitq);
    process_wait();
}

void waitqueue_wakeup(waitqueue_t *waitq)
{
    list *i, *tmp;

    list_for_each_safe(i, tmp, waitq) {
        waiting_proc_t *waitproc =
            list_entry(i, waiting_proc_t, queue);

        process_wakeup(waitproc->proc);
        list_del(&waitproc->queue);
        free_mem(waitproc);
    }
}
