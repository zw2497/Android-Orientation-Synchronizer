/*
 * Columbia University
 * COMS W4118 Fall 2018
 * Homework 3 - orientd.h
 * teamN: UNI, UNI, UNI
 */

#ifndef _ORIENTD_H
#define _ORIENTD_H

#include <unistd.h>

#define __NR_set_orientation 326

struct dev_orientation {
	int azimuth; /* angle between the magnetic north and the Y axis, around
		      * the Z axis (-180<=azimuth<180)
		      */
	int pitch;   /* rotation around the X-axis: -90<=pitch<=90 */
	int roll;    /* rotation around Y-axis: +Y == -roll, -180<=roll<=180 */
};

/* syscall wrapper */
static inline int set_orientation(struct dev_orientation *orient)
{
	return syscall(__NR_set_orientation, orient);
}

#endif /* _ORIENTD_H */
