#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/orientation.h>
#include <linux/kernel.h>

/*
 * Sets current device orientation in the kernel.
 * System call number 326.
 */
SYSCALL_DEFINE1(set_orientation, struct dev_orientation __user *, orient)
{
	struct dev_orientation test;
	
	if (orient == NULL)
		return -EINVAL;
	if (copy_from_user(&test, orient, sizeof(struct dev_orientation)))
		return -EFAULT;
	printk("azimuth=%d, pitch=%d, roll=%d.\n",
		test.azimuth, test.pitch, test.roll);
	return 0;
}
