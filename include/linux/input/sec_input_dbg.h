/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung input logging helpers used by the zinitix and sec_cmd/sec_tclm
 * code. The stock sec_debug telemetry hooks are not part of this tree, so
 * the "mode" argument is accepted and ignored; every message goes to dev_*.
 */
#ifndef _LINUX_INPUT_SEC_INPUT_DBG_H_
#define _LINUX_INPUT_SEC_INPUT_DBG_H_

#include <linux/device.h>

#define SECLOG	"[sec_input]"

#define input_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ##__VA_ARGS__)
#define input_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ##__VA_ARGS__)
#define input_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ##__VA_ARGS__)
#define input_raw_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ##__VA_ARGS__)

/* raw-data log buffer hooks (sec_debug telemetry): no-ops without it */
static inline void input_raw_data_clear(void) { }
static inline void input_log_fix(void) { }

#endif /* _LINUX_INPUT_SEC_INPUT_DBG_H_ */
