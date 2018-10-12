#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define __NR_set_orientation	326
#define __NR_orientevt_create	327
#define __NR_orientevt_destroy	328
#define __NR_orientevt_wait	329

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

static inline int orientevt_create(struct orientation_range *orient)
{
	return syscall(__NR_orientevt_create, orient);
}
static inline int orientevt_destroy(int event_id)
{
	return syscall(__NR_orientevt_destroy, event_id);
}
static inline int orientevt_wait(int event_id)
{
	return syscall(__NR_orientevt_wait, event_id);
}
int main(int argc, char **argv)
{
	int a, index;
	int b[3];
	struct orientation_range range;
	pid_t pid;

	range.orient.azimuth = 0;
	range.orient.pitch = 0;
	range.orient.roll = 0;
	range.azimuth_range = 5;
	range.pitch_range = 5;
	range.roll_range = 5;

	b[0] = orientevt_create(&range);
	
	range.orient.azimuth = 180;
	range.orient.pitch = 0;
	range.orient.roll = 180;
	range.azimuth_range = 5;
	range.pitch_range = 5;
	range.roll_range = 5;
	
	b[1] = orientevt_create(&range);

	pid = fork();
	if (pid == 0) {
		pid_t pid_up;
		pid_up = fork();
		if (pid_up == 0)
			index = 1;
		else
			index = 2;
		while (1) {
			a = orientevt_wait(b[0]);
			printf("%d:facing up!\n", index);
			usleep(1000000);
		}
		exit(0);
	} else {
		pid_t pid_down;
		pid_down = fork();
		if (pid_down == 0)
			index = 3;
		else
			index = 4;
		while (1) {
			a = orientevt_wait(b[1]);
			printf("%d:facing down!\n", index);
			usleep(1000000);
		}
	}
	orientevt_destroy(b[0]);
	return 0;
}
