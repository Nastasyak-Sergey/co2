
#ifndef CORE_SCHED_H
#define CORE_SCHED_H

#include "common.h"
#include <stdbool.h>
#include <stdint.h>

#define TASK_NR			10	/* max. number of tasks can be added */

typedef void (*task_func_t)(void *data);

int sched_init(void);
int sched_start(void) __attribute__((__noreturn__));
int sched_add_task(const char *name, task_func_t func, void *data,
		   int *task_id);
int sched_del_task(int task_id);
void sched_set_ready(int task_id);

#endif /* CORE_SCHED_H */
