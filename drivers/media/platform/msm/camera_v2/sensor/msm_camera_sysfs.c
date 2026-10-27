// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung camera sysfs: /sys/class/camera/rear and /sys/class/camera/front.
 *
 * Only the nodes the Samsung camera HAL reads at init are here: camera type,
 * firmware version strings and module id. The strings are stored here and
 * the module id is filled in by the eeprom code.
 */

#include <linux/camera_class.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "msm_camera_sysfs.h"

#define CAM_STR_LEN		40
#define CAM_MODULE_ID_LEN	16
#define CAM_INFO_LEN		100
#define CAM_CAL_CRC_LEN		37

/* Read-only constant strings, as stock prints for the default project. */
#define CAM_STR_RO(_attr, _str) \
static ssize_t _attr##_show(struct device *dev, \
	struct device_attribute *attr, char *buf) \
{ \
	return scnprintf(buf, PAGE_SIZE, "%s", _str); \
} \
static DEVICE_ATTR_RO(_attr)

/* Writable strings: the HAL stores the firmware version it found. */
#define CAM_STR_RW(_attr, _buf) \
static ssize_t _attr##_show(struct device *dev, \
	struct device_attribute *attr, char *buf) \
{ \
	return scnprintf(buf, PAGE_SIZE, "%s", _buf); \
} \
static ssize_t _attr##_store(struct device *dev, \
	struct device_attribute *attr, const char *buf, size_t count) \
{ \
	size_t n = min_t(size_t, count, sizeof(_buf) - 1); \
	memcpy(_buf, buf, n); \
	memset(_buf + n, 0, sizeof(_buf) - n); \
	return count; \
} \
static DEVICE_ATTR_RW(_attr)

static char rear_camfw[CAM_STR_LEN] = "NULL\n";
static char rear_checkfw_user[CAM_STR_LEN] = "NULL\n";
static char rear_checkfw_factory[CAM_STR_LEN] = "NULL\n";
static char rear_camfw_full[CAM_STR_LEN] = "NULL NULL NULL\n";
static char front_camfw[CAM_STR_LEN] = "NULL\n";
static char front_camfw_full[CAM_STR_LEN] = "NULL NULL NULL\n";
static char front_checkfw_user[CAM_STR_LEN] = "NULL\n";
static char front_checkfw_factory[CAM_STR_LEN] = "NULL\n";

/* Module ids, filled by msm_camera_sysfs_set_module_id(). */
static u8 rear_module_id[CAM_MODULE_ID_LEN + 1];
static u8 front_module_id[CAM_MODULE_ID_LEN + 1];

static struct device *rear_dev;
static struct device *front_dev;

CAM_STR_RO(rear_camtype, "SLSI_S5K2P6\n");
CAM_STR_RO(front_camtype, "SLSI_S5K2X7SP\n");
CAM_STR_RW(rear_camfw, rear_camfw);
CAM_STR_RW(rear_checkfw_user, rear_checkfw_user);
CAM_STR_RW(rear_checkfw_factory, rear_checkfw_factory);
CAM_STR_RW(rear_camfw_full, rear_camfw_full);
CAM_STR_RW(front_camfw, front_camfw);
CAM_STR_RW(front_camfw_full, front_camfw_full);
CAM_STR_RW(front_checkfw_user, front_checkfw_user);
CAM_STR_RW(front_checkfw_factory, front_checkfw_factory);

/* Read-only buffer string, filled by the sensor code (stock: no store). */
#define CAM_BUF_RO(_attr, _buf) static ssize_t _attr##_show(struct device *dev, 	struct device_attribute *attr, char *buf) { 	return scnprintf(buf, PAGE_SIZE, "%s", _buf); } static DEVICE_ATTR_RO(_attr)

static char rear_cam_info[CAM_INFO_LEN] = "NULL\n";
static char front_cam_info[CAM_INFO_LEN] = "NULL\n";
static char rear_fw_crc[CAM_STR_LEN] = "NULL\n";
static char rear_cal_crc[CAM_CAL_CRC_LEN] = "NULL NULL NULL\n";
static char isp_core_str[CAM_STR_LEN] = "NULL\n";

CAM_BUF_RO(rear_caminfo, rear_cam_info);
CAM_BUF_RO(front_caminfo, front_cam_info);
CAM_STR_RW(rear_fwcheck, rear_fw_crc);
CAM_STR_RW(rear_calcheck, rear_cal_crc);
CAM_STR_RW(isp_core, isp_core_str);

/* Stock prints the first 5 chars, then the next 5 as hex. */
static ssize_t module_id_show(const u8 *id, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
		"%c%c%c%c%c%02X%02X%02X%02X%02X\n",
		id[0], id[1], id[2], id[3], id[4],
		id[5], id[6], id[7], id[8], id[9]);
}

static ssize_t rear_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return module_id_show(rear_module_id, buf);
}
static DEVICE_ATTR_RO(rear_moduleid);

static ssize_t front_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return module_id_show(front_module_id, buf);
}
static DEVICE_ATTR_RO(front_moduleid);

void msm_camera_sysfs_set_module_id(bool front, const u8 *id, size_t len)
{
	u8 *dst = front ? front_module_id : rear_module_id;
	size_t n = min_t(size_t, len, CAM_MODULE_ID_LEN);

	memcpy(dst, id, n);
	memset(dst + n, 0, CAM_MODULE_ID_LEN + 1 - n);
}
EXPORT_SYMBOL_GPL(msm_camera_sysfs_set_module_id);

static struct attribute *rear_attrs[] = {
	&dev_attr_rear_camtype.attr,
	&dev_attr_rear_camfw.attr,
	&dev_attr_rear_checkfw_user.attr,
	&dev_attr_rear_checkfw_factory.attr,
	&dev_attr_rear_camfw_full.attr,
	&dev_attr_rear_moduleid.attr,
	&dev_attr_rear_caminfo.attr,
	&dev_attr_rear_fwcheck.attr,
	&dev_attr_rear_calcheck.attr,
	&dev_attr_isp_core.attr,
	NULL,
};

static const struct attribute_group rear_group = {
	.attrs = rear_attrs,
};

static const struct attribute_group *rear_groups[] = {
	&rear_group,
	NULL,
};

static struct attribute *front_attrs[] = {
	&dev_attr_front_camtype.attr,
	&dev_attr_front_camfw.attr,
	&dev_attr_front_camfw_full.attr,
	&dev_attr_front_checkfw_user.attr,
	&dev_attr_front_checkfw_factory.attr,
	&dev_attr_front_moduleid.attr,
	&dev_attr_front_caminfo.attr,
	NULL,
};

static const struct attribute_group front_group = {
	.attrs = front_attrs,
};

static const struct attribute_group *front_groups[] = {
	&front_group,
	NULL,
};

static int __init msm_camera_sysfs_init(void)
{
	struct class *cls;

	cls = camera_class_get();
	if (IS_ERR(cls))
		return PTR_ERR(cls);

	rear_dev = device_create_with_groups(cls, NULL, 0, NULL,
		rear_groups, "rear");
	if (IS_ERR(rear_dev)) {
		pr_err("%s: fail to create rear device\n", __func__);
		return PTR_ERR(rear_dev);
	}

	front_dev = device_create_with_groups(cls, NULL, 0, NULL,
		front_groups, "front");
	if (IS_ERR(front_dev)) {
		pr_err("%s: fail to create front device\n", __func__);
		device_unregister(rear_dev);
		return PTR_ERR(front_dev);
	}

	return 0;
}
module_init(msm_camera_sysfs_init);
MODULE_DESCRIPTION("Samsung camera sysfs");
MODULE_LICENSE("GPL v2");
