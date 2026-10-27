/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MSM_CAMERA_GUARD_H
#define _MSM_CAMERA_GUARD_H

#include <linux/kernel.h>
#include <linux/printk.h>

/*
 * Persistent chip writes (eeprom/OTP program, OIS firmware flash) can brick
 * a part. They are compiled out by default. Reads are never gated.
 */
static inline bool msm_camera_chip_write_allowed(const char *who)
{
#if defined(CONFIG_MSM_CAMERA_ALLOW_CHIP_WRITE)
	return true;
#else
	pr_warn_once("%s: chip write blocked (MSM_CAMERA_ALLOW_CHIP_WRITE off)\n",
		who);
	return false;
#endif
}

#endif
