#ifndef _LINUX_ORIENTATION_H
#define _LINUX_ORIENTATION_H

struct dev_orientation {
	int azimuth; /* angle between the magnetic north and the Y axis, around the
		      * Z axis (-180<=azimuth<=180)
		      */
	int pitch;   /* rotation around the X-axis: -90<=pitch<=90 */
	int roll;    /* rotation around Y-axis: +Y == -roll, -180<=roll<=180 */
};

#endif
