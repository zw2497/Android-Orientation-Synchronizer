#include <linux/syscalls.h>
#include <linux/compiler.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/idr.h>
#include <linux/orientation.h>
#include <linux/kernel.h>


/*IDR for event id management*/
DEFINE_IDR(event_idr);
struct dev_orientation ori;

/* Helper function to determine whether an orientation is within a range. */
static __always_inline bool orient_within_range(struct dev_orientation *orient,
						struct orientation_range *range)
{
	struct dev_orientation *target = &range->orient;
	unsigned int azimuth_diff = abs(target->azimuth - orient->azimuth);
	unsigned int pitch_diff = abs(target->pitch - orient->pitch);
	unsigned int roll_diff = abs(target->roll - orient->roll);

	return (!range->azimuth_range || azimuth_diff <= range->azimuth_range
			|| 360 - azimuth_diff <= range->azimuth_range)
		&& (!range->pitch_range || pitch_diff <= range->pitch_range)
		&& (!range->roll_range || roll_diff <= range->roll_range
			|| 360 - roll_diff <= range->roll_range);
}
static void do_notify(int event_id, struct orientevt *event)
{
	event->status = orient_within_range(&ori, event->orient);
	if (event->status && !event->close)
		wake_up(&event->blocked_queue);
}

static int event_notify(int event_id, void *event, void *data)
{
	do_notify(event_id, event);
	return 0;
}
/*
 * Sets current device orientation in the kernel.
 * System call number 326.
 */
SYSCALL_DEFINE1(set_orientation, struct dev_orientation __user *, orient)
{
	if (orient == NULL)
		return -EINVAL;
	if (copy_from_user(&ori, orient, sizeof(struct dev_orientation)))
		return -EFAULT;
	/*Update status for each event*/
	spin_lock(&event_idr.lock);
	idr_for_each(&event_idr, &event_notify, NULL);
	spin_unlock(&event_idr.lock);

	return 0;
}
/*
 * Create a new orientation event using the specified orientation range.
 * Return an event_id on success and appropriate error on failure.
 * System call number 327.
 */
SYSCALL_DEFINE1(orientevt_create, struct orientation_range __user *, orient)
{
	struct orientation_range *orient_rg;
	struct orientevt *event;
	int unique_id;

	if (orient == NULL)
		return -EINVAL;
	orient_rg = kmalloc(sizeof(struct orientation_range), GFP_KERNEL);
	event = kmalloc(sizeof(struct orientevt), GFP_KERNEL);

	if (!event || !orient_rg)
		return -ENOMEM;
	if (copy_from_user(orient_rg, orient,
			sizeof(struct orientation_range)))
		return -EFAULT;


	event->status = 0;
	event->close = 0;
	atomic_set(&event->num_proc, 0);
	event->orient = orient_rg;
	event->blocked_queue.lock
		= __SPIN_LOCK_UNLOCKED(event->blocked_queue.lock);
	INIT_LIST_HEAD(&event->blocked_queue.task_list);

	/*IDR event id allocation*/
	idr_preload(GFP_KERNEL);
	spin_lock(&event_idr.lock);
	unique_id = idr_alloc(&event_idr, event, 1, 100, GFP_KERNEL);
	spin_unlock(&event_idr.lock);
	idr_preload_end();

	if (unique_id < 0)
		return -EFAULT;

	spin_lock(&event_idr.lock);
	event->evt_id = unique_id;
	spin_unlock(&event_idr.lock);
	return event->evt_id;
}
/*
 * Destroy an orientation event and notify any processes which are
 * currently blocked on the event to leave the event.
 * Return 0 on success and appropriate error on failure.
 * System call number 328.
 */
SYSCALL_DEFINE1(orientevt_destroy, int, event_id)
{
	struct orientevt *event;
	int found;

	if (event_id < 0)
		return -EINVAL;

	found = 0;
	spin_lock(&event_idr.lock);
	event = idr_find(&event_idr, event_id);
	if (event == NULL) {
		spin_unlock(&event_idr.lock);
		/* Event not found */
		return -EFAULT;
	}
	event->close = 1;
	found = 1;
	spin_unlock(&event_idr.lock);

	if (!found)
		return -EINVAL;

	/* Make sure wait queue is empty */
	while (atomic_read(&event->num_proc) != 0)
		wake_up(&event->blocked_queue);

	spin_lock(&event_idr.lock);
	idr_remove(&event_idr, event_id);
	kfree(event->orient);
	kfree(event);
	spin_unlock(&event_idr.lock);

	return 0;

}
SYSCALL_DEFINE1(orientevt_wait, int, event_id)
{
	int condition;
	struct orientevt *event;
	struct __wait_queue *wait;

	spin_lock(&event_idr.lock);
	event = idr_find(&event_idr, event_id);
	if (event == NULL) {
		spin_unlock(&event_idr.lock);
		/* Event not found */
		return -EFAULT;
	}
	condition = event->status;
	spin_unlock(&event_idr.lock);

	/* Event is active */
	if (condition)
		return 0;

	wait = kmalloc(sizeof(struct __wait_queue), GFP_KERNEL);
	if (!wait)
		return -ENOMEM;

	init_wait(wait);
	add_wait_queue(&event->blocked_queue, wait);
	atomic_inc(&event->num_proc);

	while (!condition) {
		prepare_to_wait(&event->blocked_queue, wait,
				TASK_INTERRUPTIBLE);
		schedule();
		/*Orient update or event destroyed*/
		condition = (event->status || event->close);
	}
	finish_wait(&event->blocked_queue, wait);
	kfree(wait);

	condition = event->close;

	atomic_dec(&event->num_proc);
	if (condition)
		return -EFAULT;
	return 0;
}

