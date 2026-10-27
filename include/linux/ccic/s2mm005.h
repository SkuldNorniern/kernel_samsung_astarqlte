/*
 * include/linux/ccic/s2mm005.h
 *
 * STUB until the S2MM005 CCIC driver is ported. Only the types MUIC reads.
 */
#ifndef __S2MM005_STUB_H__
#define __S2MM005_STUB_H__

#include <linux/ccic/ccic_notifier.h>

typedef enum {
	Rp_Sbu_check = 0,
	Rp_56K = 1,	/* 80uA */
	Rp_22K = 2,	/* 180uA */
	Rp_10K = 3,	/* 330uA */
	Rp_Abnormal = 4,
} CCIC_RP_CurrentLvl;

#endif /* __S2MM005_STUB_H__ */
