#ifndef _LINUX_ORIENTATION_H
#define _LINUX_ORIENTATION_H

struct dev_orientation {
	int azimuth; /* angle between the magnetic north and the Y axis, around the
		      * Z axis (-180<=azimuth<=180)
		      */
	int pitch;   /* rotation around the X-axis: -90<=pitch<=90 */
	int roll;    /* rotation around Y-axis: +Y == -roll, -180<=roll<=180 */
};

struct orientation_range {
	struct dev_orientation orient;  /* device orientation */
	unsigned int azimuth_range;     /* +/- degrees around Z-axis */
	unsigned int pitch_range;       /* +/- degrees around X-axis */
	unsigned int roll_range;        /* +/- degrees around Y-axis */
};

struct orientevt {
	bool status;
	unsigned int evt_id;
	//unsigned int num_proc; //Not useful right now.
	struct orientation_range *orient;
	wait_queue_head_t blocked_queue;
	struct list_head evt_list;
};

struct orientevts {
	unsigned int num_evts;
	struct list_head evt_list;
};
#endif
