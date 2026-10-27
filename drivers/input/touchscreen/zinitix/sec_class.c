// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal Samsung "sec" device class, created on first use.
 * Built into the same module as the zinitix driver.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sec_class.h>

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static DEFINE_MUTEX(sec_class_lock);

int sec_class_get(void)
{
	int ret = 0;

	mutex_lock(&sec_class_lock);
	if (!sec_class) {
		sec_class = class_create(THIS_MODULE, "sec");
		if (IS_ERR(sec_class)) {
			ret = PTR_ERR(sec_class);
			sec_class = NULL;
		}
	}
	mutex_unlock(&sec_class_lock);

	return ret;
}
EXPORT_SYMBOL(sec_class_get);

struct device *sec_device_create(u32 devt, void *drvdata, const char *name)
{
	int ret = sec_class_get();

	if (ret)
		return ERR_PTR(ret);

	return device_create(sec_class, NULL, MKDEV(0, devt), drvdata, "%s", name);
}
EXPORT_SYMBOL(sec_device_create);

void sec_device_destroy(u32 devt)
{
	if (sec_class)
		device_destroy(sec_class, MKDEV(0, devt));
}
EXPORT_SYMBOL(sec_device_destroy);
