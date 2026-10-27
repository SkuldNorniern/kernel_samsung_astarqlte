/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_CAMERA_CLASS_H
#define _LINUX_CAMERA_CLASS_H

struct class;

/*
 * Shared /sys/class/camera. Created on first call. The flash LED driver and
 * the camera sensor sysfs both hang their devices off this one class.
 * Returns the class or an ERR_PTR.
 */
struct class *camera_class_get(void);

#endif
