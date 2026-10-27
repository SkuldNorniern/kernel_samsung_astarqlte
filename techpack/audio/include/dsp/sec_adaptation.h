/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Samsung sound adaptation (SEC_SND_ADAPTATION) ASM/ADM sound-effect
 * parameter layouts. Ported from stock 4.4 include/sound/sec_adaptation.h.
 * Maxim DSM, NXP TFADSP and voice (CVS/CVP) parts are not ported.
 */

#ifndef __SEC_ADAPTATION_H
#define __SEC_ADAPTATION_H

#include <linux/types.h>

#define ADM_MODULE_ID_PP_SS_REC             0x10001050
#define ADM_PARAM_ID_PP_SS_REC_GETPARAMS    0x10001052
#define ASM_MODULE_ID_PP_SA                 0x10001fa0
#define ASM_PARAM_ID_PP_SA_PARAMS           0x10001fa1
#define ASM_MODULE_ID_PP_SA_VSP             0x10001fb0
#define ASM_PARAM_ID_PP_SA_VSP_PARAMS       0x10001fb1
#define ASM_MODULE_ID_PP_ADAPTATION_SOUND		0x10001fc0
#define ASM_PARAM_ID_PP_ADAPTATION_SOUND_PARAMS	0x10001fc1
#define ASM_MODULE_ID_PP_LRSM               0x10001fe0
#define ASM_PARAM_ID_PP_LRSM_PARAMS         0x10001fe1
#define ASM_MODULE_ID_PP_SA_MSP             0x10001ff0
#define ASM_MODULE_ID_PP_SA_MSP_PARAM       0x10001ff1
#define ASM_MODULE_ID_PP_SB                 0x10001f01
#define ASM_PARAM_ID_PP_SB_PARAM            0x10001f04
#define ASM_PARAM_ID_PP_SB_ROTATION_PARAM	0x10001f02
#define ASM_MODULE_ID_PP_SA_UPSCALER_COLOR            0x10001f20
#define ASM_PARAM_ID_PP_SA_UPSCALER_COLOR_PARAMS      0x10001f21

struct asm_stream_cmd_set_pp_params_sa {
	int16_t OutDevice;
	int16_t Preset;
	int32_t EqLev[9];
	int16_t m3Dlevel;
	int16_t BElevel;
	int16_t CHlevel;
	int16_t CHRoomSize;
	int16_t Clalevel;
	int16_t volume;
	int16_t Sqrow;
	int16_t Sqcol;
	int16_t TabInfo;
	int16_t NewUI;
	int16_t m3DPositionOn;
	int16_t reserved;
	int32_t m3DPositionAngle[2];
	int32_t m3DPositionGain[2];
	int32_t AHDRonoff;
} __packed;

struct asm_stream_cmd_set_pp_params_vsp {
	uint32_t speed_int;
} __packed;

struct asm_stream_cmd_set_pp_params_adaptation_sound {
	int32_t enable;
	int16_t gain[2][6];
	int16_t device;
} __packed;

struct asm_stream_cmd_set_pp_params_lrsm {
	int16_t sm;
	int16_t lr;
} __packed;

struct asm_stream_cmd_set_pp_params_msp {
	uint32_t msp_int;
} __packed;

struct asm_stream_cmd_set_pp_params_sb {
	uint32_t sb_enable;
} __packed;

struct asm_stream_cmd_set_pp_params_upscaler {
	uint32_t upscaler_enable;
} __packed;

struct asm_stream_cmd_set_pp_params_sb_rotation {
	uint32_t sb_rotation;
} __packed;

#endif /* __SEC_ADAPTATION_H */
