/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MSM_CAMERA_SYSFS_H
#define _MSM_CAMERA_SYSFS_H

#include <linux/stddef.h>
#include <linux/types.h>

#ifdef CONFIG_CAMERA_SYSFS_V2
/* id: module id bytes from eeprom; front selects /sys/class/camera/front */
void msm_camera_sysfs_set_module_id(bool front, const u8 *id, size_t len);
#else
static inline void msm_camera_sysfs_set_module_id(bool front, const u8 *id,
	size_t len)
{
}
#endif

#endif
