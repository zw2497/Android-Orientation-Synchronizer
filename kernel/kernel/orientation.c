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

/*Global events data structure*/
struct orientevts events_list = {
.lock = __SPIN_LOCK_UNLOCKED(events_list.lock),
.evt_list.next = &events_list.evt_list,
.evt_list.prev = &events_list.evt_list
};

/*IDR for event id management*/
DEFINE_IDR(evts_id);

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
/*
 * Sets current device orientation in the kernel.
 * System call number 326.
 */
SYSCALL_DEFINE1(set_orientation, struct dev_orientation __user *, orient)
{
	struct dev_orientation ori;
	struct orientevt *event;
	struct list_head *list;
	
	if (orient == NULL)
		return -EINVAL;
	if (copy_from_user(&ori, orient, sizeof(struct dev_orientation)))
		return -EFAULT;
	/*Update status for each event*/
	spin_lock(&events_list.lock);
	list_for_each(list, &events_list.evt_list) {
		event = list_entry(list, struct orientevt, evt_list);
		event->status = orient_within_range(&ori, event->orient);
		/*Only wake up when is caused by orient update*/
		if (event->status && !event->close)
			wake_up(&event->blocked_queue);

	}
	spin_unlock(&events_list.lock);

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
	void *id_temp;
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
	INIT_LIST_HEAD(&event->evt_list);

	/*IDR event id allocation*/
	idr_preload(GFP_KERNEL);
	spin_lock(&evts_id.lock);
	unique_id = idr_alloc(&evts_id, id_temp, 1, 100, GFP_KERNEL);
	spin_unlock(&evts_id.lock);
	idr_preload_end();
	if (unique_id < 0)
		return -EFAULT;

	spin_lock(&events_list.lock);
	event->evt_id = unique_id;
	list_add_tail(&event->evt_list, &events_list.evt_list);
	spin_unlock(&events_list.lock);
 

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
	struct list_head *list;
	struct orientevt *event;
	int found;

	if (event_id < 0)
		return -EINVAL;

	found = 0;
	spin_lock(&events_list.lock);
	list_for_each(list, &events_list.evt_list) {
		event = list_entry(list, struct orientevt, evt_list);
		if (event->evt_id == event_id) {
			event->close = 1;
			/*Only deleted from events_list*/
			list_del(&event->evt_list);
			found = 1;
			break;
		}
	}
	spin_unlock(&events_list.lock);

	if (!found)
		return -EINVAL;

	spin_lock(&evts_id.lock);
	idr_remove(&evts_id, event_id);
	spin_unlock(&evts_id.lock);
	/*Make sure wait queue is empty*/
	while(atomic_read(&event->num_proc) != 0)
		wake_up(&event->blocked_queue);

	spin_lock(&events_list.lock);
	kfree(event->orient);
	kfree(event);
	spin_unlock(&events_list.lock);

	return 0;

}
SYSCALL_DEFINE1(orientevt_wait, int, event_id)
{
	int condition;
	struct list_head *list;
	struct orientevt *event;
	struct __wait_queue *wait; 

	condition = -1;
	spin_lock(&events_list.lock);
	list_for_each(list, &events_list.evt_list) {
		event = list_entry(list, struct orientevt, evt_list);
		if (event->evt_id == event_id) {
			condition = event->status;
			break;
		}
	}
	spin_unlock(&events_list.lock);


	if (condition < 0)
		return -EFAULT; // Event not found
	if (condition)
		return 0; //Event is active

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
	atomic_dec(&event->num_proc);

	return 0;
}

