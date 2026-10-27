/*
 * include/linux/ccic/ccic_notifier.h
 *
 * STUB until the S2MM005 CCIC driver is ported. Types match the stock
 * 4.4 header; the register/notify calls are no-ops so MUIC can build and
 * run with CCIC absent (CONFIG_CCIC_NOTIFIER off).
 */
#ifndef __CCIC_NOTIFIER_H__
#define __CCIC_NOTIFIER_H__

#include <linux/types.h>
#include <linux/notifier.h>


/* CCIC notifier call sequence,
 * largest priority number device will be called first. */
typedef enum {
	CCIC_NOTIFY_DEV_INITIAL = 0,
	CCIC_NOTIFY_DEV_USB,
	CCIC_NOTIFY_DEV_BATTERY,
	CCIC_NOTIFY_DEV_PDIC,
	CCIC_NOTIFY_DEV_MUIC,
	CCIC_NOTIFY_DEV_CCIC,
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	CCIC_NOTIFY_DEV_MANAGER,
#endif
	CCIC_NOTIFY_DEV_DP,
	CCIC_NOTIFY_DEV_USB_DP,
} ccic_notifier_device_t;

typedef enum {
	CCIC_NOTIFY_ID_INITIAL = 0,
	CCIC_NOTIFY_ID_ATTACH,
	CCIC_NOTIFY_ID_RID,
	CCIC_NOTIFY_ID_USB,
	CCIC_NOTIFY_ID_POWER_STATUS,
	CCIC_NOTIFY_ID_WATER,
	CCIC_NOTIFY_ID_VCONN,
	CCIC_NOTIFY_ID_DP_CONNECT,
	CCIC_NOTIFY_ID_DP_HPD,
	CCIC_NOTIFY_ID_DP_LINK_CONF,
	CCIC_NOTIFY_ID_USB_DP,
	CCIC_NOTIFY_ID_ROLE_SWAP,
	CCIC_NOTIFY_ID_FAC,
} ccic_notifier_id_t;

typedef struct {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t sub1:16;
	uint64_t sub2:16;
	uint64_t sub3:16;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	void *pd;
#endif
} CC_NOTI_TYPEDEF;

/* ID = 1 : Attach */
typedef struct {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t attach:16;
	uint64_t rprd:16;
	uint64_t cable_type:16;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	void *pd;
#endif
} CC_NOTI_ATTACH_TYPEDEF;

typedef enum {
	CCIC_NOTIFY_DETACH = 0,
	CCIC_NOTIFY_ATTACH,
} ccic_notifier_attach_t;

typedef enum {
	CCIC_NOTIFY_DEVICE = 0,
	CCIC_NOTIFY_HOST,
} ccic_notifier_attach_rprd_t;

typedef enum {
	CCIC_NOTIFY_LOW = 0,
	CCIC_NOTIFY_HIGH,
	CCIC_NOTIFY_IRQ,
} ccic_notifier_dp_hpd_t;

typedef enum {
	CCIC_NOTIFY_DP_PIN_UNKNOWN = 0,
	CCIC_NOTIFY_DP_PIN_A,
	CCIC_NOTIFY_DP_PIN_B,
	CCIC_NOTIFY_DP_PIN_C,
	CCIC_NOTIFY_DP_PIN_D,
	CCIC_NOTIFY_DP_PIN_E,
	CCIC_NOTIFY_DP_PIN_F,
} ccic_notifier_dp_pinconf_t;

/* ID = 2 : RID */
typedef struct {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t rid:16;
	uint64_t sub2:16;
	uint64_t sub3:16;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	void *pd;
#endif
} CC_NOTI_RID_TYPEDEF;

typedef enum {
	RID_UNDEFINED = 0,
	RID_000K,
	RID_001K,
	RID_255K,
	RID_301K,
	RID_523K,
	RID_619K,
	RID_OPEN,
} ccic_notifier_rid_t;

/* ID = 3 : USB status */
typedef struct {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t attach:16;
	uint64_t drp:16;
	uint64_t sub3:16;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	void *pd;
#endif
} CC_NOTI_USB_STATUS_TYPEDEF;

typedef struct {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t is_connect:16;
	uint64_t hs_connect:16;
	uint64_t reserved:16;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	void *pd;
#endif
} USB_DP_NOTI_TYPEDEF;

typedef enum {
	USB_STATUS_NOTIFY_DETACH = 0,
	USB_STATUS_NOTIFY_ATTACH_DFP = 1, // Host
	USB_STATUS_NOTIFY_ATTACH_UFP = 2, // Device
	USB_STATUS_NOTIFY_ATTACH_DRP = 3, // Dual role
	USB_STATUS_NOTIFY_ATTACH_HPD = 4, // DP : Hot Plugged Detect
} USB_STATUS;

/* TODO:  */
struct ccic_notifier_struct {
	CC_NOTI_TYPEDEF ccic_template;
	struct blocking_notifier_head notifier_call_chain;
};

static inline int ccic_notifier_notify(CC_NOTI_TYPEDEF *n, void *d, int s)
{
	return 0;
}

static inline int ccic_notifier_register(struct notifier_block *nb,
		notifier_fn_t notifier, ccic_notifier_device_t listener)
{
	return 0;
}

static inline int ccic_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

static inline int ccic_notifier_init(void)
{
	return 0;
}

#endif /* __CCIC_NOTIFIER_H__ */
