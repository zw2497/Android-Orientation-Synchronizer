#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define __NR_set_orientation	326
#define __NR_orientevt_create	327
#define __NR_orientevt_destroy	328
#define __NR_orientevt_wait	329
/*Multiple of 2*/
#define CHILD_PROC		4
#define TEST_SEC		10

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
	int count, status, b[2], pids[CHILD_PROC];
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

	for (int i = 0; i < CHILD_PROC; i++) {
		pid = fork();
		if (pid < 0)
			return -1;
		if (pid == 0) {
			if (i < CHILD_PROC/2) {
				while (1) {
					if (orientevt_wait(b[0]) < 0)
						break;
					printf("%d:facing up!\n", i);
					usleep(1000000);
				}
				exit(0);
			} else {
				while (1) {
					if (orientevt_wait(b[1]) < 0)
						break;
					printf("%d:facing down!\n", i);
					usleep(1000000);
				}
				exit(0);
			}
		} else
			pids[i] = pid;
	}

	count = 0;
	while(1) {
		if (count >= TEST_SEC)
			break;
		count++;
		usleep(1000000);
        }

	orientevt_destroy(b[0]);
	orientevt_destroy(b[1]);
	for (int i = 0; i < CHILD_PROC; i++) {
		printf("Waiting for %d\n", i);
		waitpid(pids[i], &status, 0);
	}

	return 0;
}
