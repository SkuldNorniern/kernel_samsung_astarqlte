/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung input codes that are not in this tree's uapi header.
 * Values follow stock 4.4 except SW_GLOVE, which clashes with
 * SW_PEN_INSERTED (0x0f) here and so uses the same value as the zinitix port.
 */
#ifndef _LINUX_INPUT_SEC_INPUT_CODES_H_
#define _LINUX_INPUT_SEC_INPUT_CODES_H_

#include <linux/input.h>

#ifndef KEY_RECENT
#define KEY_RECENT			254
#endif
#ifndef KEY_DUMMY_HOME
#define KEY_DUMMY_MENU			251
#define KEY_DUMMY_HOME			252
#define KEY_DUMMY_BACK			253
#endif
#ifndef KEY_BLACK_UI_GESTURE
#define KEY_BLACK_UI_GESTURE		0x1c7
#endif
#ifndef KEY_SIDE_GESTURE
#define KEY_SIDE_GESTURE		0x1c6
#endif
#ifndef KEY_SIDE_GESTURE_RIGHT
#define KEY_SIDE_GESTURE_RIGHT		0x1ca
#endif
#ifndef KEY_SIDE_GESTURE_LEFT
#define KEY_SIDE_GESTURE_LEFT		0x1cb
#endif
#ifndef SW_GLOVE
#define SW_GLOVE			0x15
#endif
#ifndef ABS_MT_PALM
#define ABS_MT_PALM			0x3e
#endif
#ifndef ABS_MT_CUSTOM
#define ABS_MT_CUSTOM			0x3e
#endif

#endif /* _LINUX_INPUT_SEC_INPUT_CODES_H_ */
