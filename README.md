# f18-hmwk3-team18

## Data structure
```c
struct orientevt {
	bool status;				/*indicate if event is within range*/
	bool close;				/*indicate if event is destroeyd*/
	unsigned int evt_id;			/*event_id managed by IDR*/
	atomic_t num_proc;			/*number of blocked processes*/
	struct orientation_range *orient;	/*orient range assigned by orientevt_create*/
	wait_queue_head_t blocked_queue;	/*blocked processes attached to this event*/
	struct list_head evt_list;	/*event linked list*/
};

struct orientevts {
	spinlock_t lock;		/*spinlock for struct*/
	unsigned int num_evts;		/*number of events*/
	struct list_head evt_list;	/*event linked list*/
}
```
## Locks
A global lock for orientevts and locks inside **add_wait_queue**, **prepare_to_wait** and **finish_wait**.
Local lock in struct orientevts.
```c
DEFINE_SPINLOCK(evts_lock);
```

## Mechanism
**set_orientation**:Update status in every struct orientevt and if it is active wake up wait queue.
<br><br>
**orientevt_create**:Create struct orientevt and add into struct orientevts by linked list.
<br><br>
**orientevt_destroy**:Remove corresponding struct orientevt from linked list. After making sure no process is blocked on this event(indicated by num_proc) it will free the space.
<br><br>
**orientect_wait**:If the event exists, create struct wait_queue and synchronize with orientevts. If event is active or event is destroyed it will leave wait queue.
## Test program
Test process will fork 4 child processes. Two of them follow facing up event while others follow facing down. After TEST_SEC it will close these two events to let child process exit.
## Test result
Lock seems work properly because I noticed the syscall order may change sometimes:
<br>
1:facing up!
<br>
2:facing up!
<br>
2:facing up!
<br>
1:facing up!
<br>
1:facing up!
<br>
2:facing up!
