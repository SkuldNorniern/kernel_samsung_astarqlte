/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sec_compat.h - shims for Samsung services not ported to 4.19 yet.
 *
 * sec_param (persistent parameter block) and STI abc (event reporting)
 * are not present in this tree. Parameter writes fail with -ENODEV and
 * ABC events are dropped until the real services are brought over.
 *
 * MUIC types come from <linux/muic/muic.h> (CONFIG_USE_MUIC).
 */
#ifndef __SEC_COMPAT_H
#define __SEC_COMPAT_H

#include <linux/errno.h>

#include <linux/sec_param.h>

/* boot-mode query from the sec_bsp code that is not ported yet */
static inline unsigned int is_boot_recovery(void)
{
	return 0;
}

static inline void sec_abc_send_event(const char *str)
{
}


#endif /* __SEC_COMPAT_H */
