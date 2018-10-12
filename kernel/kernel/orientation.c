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
#include <linux/orientation.h>
#include <linux/kernel.h>

/*Global events data structure*/
struct orientevts events_list = {
.num_evts = 0,
.evt_list.next = &events_list.evt_list,
.evt_list.prev = &events_list.evt_list
};
static unsigned int evts_count = 0; //Not atomic_t right now

DEFINE_SPINLOCK(evts_lock); /*Lock for events data structure
				maybe put inside structure?*/

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
	printk("azimuth=%d, pitch=%d, roll=%d.\n",
		ori.azimuth, ori.pitch, ori.roll); //Test code shows on klog
	//Inform event_list to block/unblock
	spin_lock(&evts_lock);
	list_for_each(list, &events_list.evt_list) {
		event = list_entry(list, struct orientevt, evt_list);
		event->status = orient_within_range(&ori, event->orient);
		if (event->status)
			wake_up(&event->blocked_queue); //Wakeup func here
	}
	spin_unlock(&evts_lock);

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
	//event->num_proc = 0;  //Not useful right now.
	event->orient = orient_rg;
	//Maybe alternatives to initialize lock in wait queue
	event->blocked_queue.lock
		= __SPIN_LOCK_UNLOCKED(event->blocked_queue.lock);
	INIT_LIST_HEAD(&event->blocked_queue.task_list);
	INIT_LIST_HEAD(&event->evt_list);

	spin_lock(&evts_lock);
	event->evt_id = evts_count++; //need lock & evts_count overflow & prefer spinlock
	events_list.num_evts++; //need lock
	list_add_tail(&event->evt_list, &events_list.evt_list); //need lock
	spin_unlock(&evts_lock);

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

	if (event_id < 0)
		return -EINVAL;
	if (events_list.num_evts == 0)
		return -EFAULT; //Other alternatives

	spin_lock(&evts_lock);
	list_for_each(list, &events_list.evt_list) { //need lock
		event = list_entry(list, struct orientevt, evt_list);
		if (event->evt_id == event_id) {
			//Wake up proc in blocked_queue
			event->status = 1;
			wake_up(&event->blocked_queue);
			while (!list_empty(&event->blocked_queue.task_list))
				;
			list_del(&event->evt_list);
			//Free kmalloc here, might need careful check
			kfree(event->orient);
			kfree(event);
			events_list.num_evts--;
			spin_unlock(&evts_lock);
			return 0;
		}
	}
	spin_unlock(&evts_lock);
	//Event not found
	return -EFAULT;
}
SYSCALL_DEFINE1(orientevt_wait, int, event_id)
{
	//Find event, add in wait queue, get sleep
	int condition;
	struct list_head *list;
	struct orientevt *event;
	/*wait queue defined here, though it's a local variable.
	  But it worked and might need remedy.*/
	struct __wait_queue wait; 

	condition = -1;
	spin_lock(&evts_lock);
	list_for_each(list, &events_list.evt_list) { //need lock
		event = list_entry(list, struct orientevt, evt_list);
		if (event->evt_id == event_id) {
			condition = event->status;
			break;
		}
	}

	if (condition < 0) {
		spin_unlock(&evts_lock);
		return -EFAULT; //Other alternatives
	}
	if (condition) {
		spin_unlock(&evts_lock);
		return 0; //Other alternatives
	}

	/*No lock for entire procedure,
	  though should not have race condition*/
	spin_unlock(&evts_lock);
	init_wait(&wait); //wait queue initialization
	add_wait_queue(&event->blocked_queue, &wait); //Wait_queue lock inside
	while (!condition) {
		prepare_to_wait(&event->blocked_queue, &wait, 
				TASK_INTERRUPTIBLE); //Wait_queue lock inside
		schedule();
		condition = event->status; //No lock, might need atomic
	}
	finish_wait(&event->blocked_queue, &wait);
	return 0;
}

