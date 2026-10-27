/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Minimal Samsung "sec" class helper used by the zinitix/sec_cmd code.
 * The full Samsung sec_class core is not part of this tree.
 */
#ifndef _LINUX_SEC_CLASS_H
#define _LINUX_SEC_CLASS_H

#include <linux/device.h>
#include <linux/types.h>

extern struct class *sec_class;

int sec_class_get(void);
struct device *sec_device_create(u32 devt, void *drvdata, const char *name);
void sec_device_destroy(u32 devt);

#endif /* _LINUX_SEC_CLASS_H */
