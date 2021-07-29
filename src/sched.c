#include "sched.h"
#include <string.h>

struct task {
	const char *name;		/* task name */
	task_func_t func;		/* task function to run */
	void *data;			/* user private data; passed to func */
};

/* Contains status of each task (1 - data is ready; 0 - blocked) */
static volatile uint32_t sched_ready;
static struct task task_list[TASK_NR];
static int current;			/* current pos in task_list[] */

/**
 * Set "Blocked" state for specified task (waiting for new data).
 *
 * @param task_id Task ID, starting from 0
 */
static void sched_set_blocked(int task_id)
{
	unsigned long flags;

	enter_critical(flags);
	sched_ready &= ~BIT(task_id);
	exit_critical(flags);
}

static int sched_find_empty_slot(void)
{
	int i;

	for (i = 0; i < TASK_NR; ++i) {
		if (!task_list[i].func)
			return i;
	}

	return -1;
}

static int sched_slot_by_name(const char *name)
{
	int i;

	for (i = 0; i < TASK_NR; ++i) {
		if (strcmp(name, task_list[i].name) == 0)
			return i;
	}

	return -1;
}

/**
 * Look for the next ready to run task.
 *
 * @return Index of task or -1 if no tasks found
 */
static int sched_find_next(void)
{
	uint32_t tasks_ready = sched_ready;
	int i;

	/*
	 * Iterate over all tasks, including current, but starting from next one
	 * (fair scheduling).
	 */
	for (i = current + 1; i <= current + TASK_NR; ++i) {
		const int idx = i % TASK_NR;
		int ready = tasks_ready & (1 << idx);

		if (ready)
			return idx;
	}

	return -1;
}

/**
 * Run next task from scheduler list.
 *
 * @return Index of executed task or -1 if no task was executed
 */
static int sched_run_next(void)
{
	unsigned long irq_flags;
	int next;

	enter_critical(irq_flags);
	if (!sched_ready) {
		exit_critical(irq_flags);
		return -1;
	}
	exit_critical(irq_flags);

	next = sched_find_next();
	if (next == -1)
		return -1;
	current = next;

	/*
	 * Clear the task flag (put it in "Blocked" state) before running task
	 * function, to avoid race conditions.
	 */
	sched_set_blocked(current);
	task_list[current].func(task_list[current].data);

	return current;
}

/**
 * Initialize the scheduler.
 *
 * @return 0 on success or negative value on error
 */
int sched_init(void)
{
	return 0;
}

/**
 * Initiate the heart beat.
 *
 * Run the super loop, which executes registered tasks.
 *
 * @note Does not return.
 */
int sched_start(void)
{
	for (;;)
		sched_run_next();
}

/**
 * Add new scheduler task.
 *
 * Create new task and add it to scheduler list. Created task will have
 * "Blocked" state by default, and will be only executed when new data arrives
 * for this task.
 *
 * When new data is available, use @ref sched_set_ready() to let scheduler know
 * it should run corresponding task. After running the task, scheduler sets its
 * state to "Blocked" automatically.
 *
 * @param name Task name, must be unique
 * @param func Task function (callback) to be executed in schedule loop
 * @param data Pointer to data to be passed to task function
 * @param task_id If not null, will contain ID of created task, starting from 1
 * @return 0 on success or negative value on error
 *
 * @note This function can be called before sched_init() and sched_start()
 */
int sched_add_task(const char *name, task_func_t func, void *data,
		   int *task_id)
{
	int slot;		/* next empty slot index for new task */

	slot = sched_find_empty_slot();
	if (slot < 0)
		return slot;
	if (sched_slot_by_name(name) >= 0)
		return -1;
	if (strlen(name) == 0)
		return -2;
	if (func == NULL)
		return -3;

	/* Add new task to task list */
	memset(&task_list[slot], 0, sizeof(struct task));
	task_list[slot].name = name;
	task_list[slot].func = func;
	task_list[slot].data = data;

	if (task_id)
		*task_id = slot + 1;

	return 0;
}

/**
 * Remove task.
 *
 * @param task_id Task ID obtained on sched_add_task()
 * @return 0 on success or negative value on error
 */
int sched_del_task(int task_id)
{
	int idx = task_id - 1;

	cm3_assert(idx >= 0 && idx < TASK_NR);

	if (!task_list[idx].func)
		return -1;

	sched_set_blocked(idx);
	memset(&task_list[idx], 0, sizeof(struct task));

	return 0;
}

/**
 * Set "Ready" state for specified task (new data is available).
 *
 * Report to scheduler that task has new data and ready to be executed.
 *
 * @param task_id Task ID (obtained in sched_add_task())
 */
void sched_set_ready(int task_id)
{
	unsigned long flags;

	enter_critical(flags);
	sched_ready |= BIT(task_id - 1);
	exit_critical(flags);
}
