# f18-hmwk3-team18

## Data structure
```c
struct orientevt {
	bool status;	/*indicate if event is within range*/
	unsigned int evt_id;	/*event_id assigned by global variable*/
	struct orientation_range *orient;	/*orient range assigned by orientevt_create*/
	wait_queue_head_t blocked_queue;	/*blocked processes attached to this event*/
	struct list_head evt_list;	/*event linked list*/
};

struct orientevts {
	unsigned int num_evts;	/*number of events*/
	struct list_head evt_list;	/*event linked list*/
}
```
## Locks
A global lock for orientevts and locks inside **add_wait_queue**, **prepare_to_wait** and **finish_wait**:
```c
DEFINE_SPINLOCK(evts_lock);
```
## Test result
Lock seems work properly because I noticed the syscall order may change sometimes:
1:facing up!
2:facing up!
2:facing up!
1:facing up!
1:facing up!
2:facing up!
