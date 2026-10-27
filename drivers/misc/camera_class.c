// SPDX-License-Identifier: GPL-2.0-only
/*
 * Owner of the single "camera" class (/sys/class/camera).
 */
#include <linux/camera_class.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/mutex.h>

static DEFINE_MUTEX(camera_class_lock);
static struct class *camera_class;

struct class *camera_class_get(void)
{
	struct class *cls;

	mutex_lock(&camera_class_lock);
	if (!camera_class) {
		cls = class_create(THIS_MODULE, "camera");
		if (IS_ERR(cls)) {
			mutex_unlock(&camera_class_lock);
			return cls;
		}
		camera_class = cls;
	}
	cls = camera_class;
	mutex_unlock(&camera_class_lock);

	return cls;
}
EXPORT_SYMBOL_GPL(camera_class_get);
