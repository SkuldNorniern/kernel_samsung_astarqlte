/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/sec_param.h
 *
 * Shim for the Samsung sec_param block until it is ported. Writes fail with
 * -ENODEV and reads return -ENODEV, so callers fall back to defaults.
 */
#ifndef _LINUX_SEC_PARAM_H
#define _LINUX_SEC_PARAM_H

#include <linux/errno.h>

enum sec_param_index {
	param_index_wireless_charging_mode = 0,
	param_index_afc_disable,
};

static inline int sec_set_param(enum sec_param_index index, void *value)
{
	return -ENODEV;
}

static inline int sec_get_param(enum sec_param_index index, void *value)
{
	return -ENODEV;
}

#endif /* _LINUX_SEC_PARAM_H */
