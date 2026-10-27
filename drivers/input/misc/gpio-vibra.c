/*
 * GPIO vibrator driver
 *
 * Drives a vibration motor that is switched on and off by a single GPIO
 * (no PWM and no regulator). The motor enable line may sit behind an I2C
 * GPIO expander, so the GPIO is always accessed with the sleeping API from
 * a work item.
 *
 * The motor is exposed three ways, all driving the same GPIO:
 *  - input force-feedback (FF_RUMBLE): any non-zero magnitude turns the
 *    motor on, zero turns it off;
 *  - timed_output class (/sys/class/timed_output/vibrator/enable), as used
 *    by the stock Samsung vibrator HAL: N > 0 runs for N ms, 0 stops,
 *    -1 runs until stopped;
 *  - LED class (/sys/class/leds/vibrator/{activate,duration,state}), as
 *    used by LineageOS style vibrator HALs.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "../../staging/android/timed_output.h"

#define GPIO_VIBRATOR_NAME	"gpio-vibrator"
#define GPIO_VIBRATOR_DEF_MS	15000
#define GPIO_VIBRATOR_MAX_MS	15000

struct gpio_vibrator {
	struct input_dev *input;
	struct work_struct play_work;
	struct hrtimer stop_timer;
	struct timed_output_dev tdev;
	struct led_classdev cdev;
	int gpio;
	u16 level;		/* force-feedback magnitude */
	bool timed_on;		/* timed_output / led activation */
	u32 duration_ms;	/* led "duration" attribute */
};

static void gpio_vibrator_play_work(struct work_struct *work)
{
	struct gpio_vibrator *vib = container_of(work, struct gpio_vibrator,
						 play_work);

	gpio_set_value_cansleep(vib->gpio,
				(vib->level || vib->timed_on) ? 1 : 0);
}

static int gpio_vibrator_play_effect(struct input_dev *dev, void *data,
				     struct ff_effect *effect)
{
	struct gpio_vibrator *vib = input_get_drvdata(dev);

	vib->level = effect->u.rumble.strong_magnitude;
	if (!vib->level)
		vib->level = effect->u.rumble.weak_magnitude;

	schedule_work(&vib->play_work);

	return 0;
}

static void gpio_vibrator_close(struct input_dev *input)
{
	struct gpio_vibrator *vib = input_get_drvdata(input);

	cancel_work_sync(&vib->play_work);
	vib->level = 0;
	gpio_set_value_cansleep(vib->gpio, vib->timed_on ? 1 : 0);
}

static void gpio_vibrator_cancel_work(void *data)
{
	struct gpio_vibrator *vib = data;

	hrtimer_cancel(&vib->stop_timer);
	cancel_work_sync(&vib->play_work);
	gpio_set_value_cansleep(vib->gpio, 0);
}

static enum hrtimer_restart gpio_vibrator_stop_timer(struct hrtimer *timer)
{
	struct gpio_vibrator *vib = container_of(timer, struct gpio_vibrator,
						 stop_timer);

	vib->timed_on = false;
	schedule_work(&vib->play_work);

	return HRTIMER_NORESTART;
}

/* ms > 0: run for ms; ms == 0: stop; ms < 0: run until stopped. */
static void gpio_vibrator_timed_start(struct gpio_vibrator *vib, int ms)
{
	hrtimer_cancel(&vib->stop_timer);

	if (ms == 0) {
		vib->timed_on = false;
	} else {
		vib->timed_on = true;
		if (ms > 0)
			hrtimer_start(&vib->stop_timer, ms_to_ktime(ms),
				      HRTIMER_MODE_REL);
	}

	schedule_work(&vib->play_work);
}

static void gpio_vibrator_tdev_enable(struct timed_output_dev *tdev, int value)
{
	struct gpio_vibrator *vib = container_of(tdev, struct gpio_vibrator,
						 tdev);

	gpio_vibrator_timed_start(vib, value);
}

static int gpio_vibrator_tdev_get_time(struct timed_output_dev *tdev)
{
	struct gpio_vibrator *vib = container_of(tdev, struct gpio_vibrator,
						 tdev);

	if (!hrtimer_active(&vib->stop_timer))
		return 0;

	return ktime_to_ms(hrtimer_get_remaining(&vib->stop_timer));
}

static void gpio_vibrator_unregister_input(void *data)
{
	input_unregister_device(data);
}

static void gpio_vibrator_unregister_timed(void *data)
{
	timed_output_dev_unregister(data);
}

static ssize_t duration_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct gpio_vibrator *vib = container_of(cdev, struct gpio_vibrator,
						 cdev);

	return sprintf(buf, "%u\n", vib->duration_ms);
}

static ssize_t duration_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct gpio_vibrator *vib = container_of(cdev, struct gpio_vibrator,
						 cdev);
	u32 val;
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret < 0)
		return ret;

	/* 0 is a NOP, as on the qpnp vibrator-ldo led */
	if (val == 0)
		return count;

	if (val > GPIO_VIBRATOR_MAX_MS)
		val = GPIO_VIBRATOR_MAX_MS;

	vib->duration_ms = val;

	return count;
}
static DEVICE_ATTR_RW(duration);

static ssize_t activate_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct gpio_vibrator *vib = container_of(cdev, struct gpio_vibrator,
						 cdev);

	return sprintf(buf, "%d\n", vib->timed_on ? 1 : 0);
}

static ssize_t activate_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct gpio_vibrator *vib = container_of(cdev, struct gpio_vibrator,
						 cdev);
	u32 val;
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return count;

	gpio_vibrator_timed_start(vib, val ? vib->duration_ms : 0);

	return count;
}
static DEVICE_ATTR_RW(activate);

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct gpio_vibrator *vib = container_of(cdev, struct gpio_vibrator,
						 cdev);

	return sprintf(buf, "%d\n", vib->timed_on ? 1 : 0);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	/* Nothing to do: state follows activate, as on the qpnp led */
	return count;
}
static DEVICE_ATTR_RW(state);

static struct attribute *gpio_vibrator_led_attrs[] = {
	&dev_attr_activate.attr,
	&dev_attr_duration.attr,
	&dev_attr_state.attr,
	NULL,
};
ATTRIBUTE_GROUPS(gpio_vibrator_led);

static int gpio_vibrator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_vibrator *vib;
	int err;

	vib = devm_kzalloc(dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	/* Returns -EPROBE_DEFER while the GPIO expander is not registered. */
	vib->gpio = of_get_named_gpio(dev->of_node, "motor-en", 0);
	if (vib->gpio < 0) {
		if (vib->gpio != -EPROBE_DEFER)
			dev_err(dev, "missing motor-en gpio: %d\n", vib->gpio);
		return vib->gpio;
	}

	err = devm_gpio_request_one(dev, vib->gpio, GPIOF_OUT_INIT_LOW,
				    "vibrator-motor-en");
	if (err) {
		dev_err(dev, "failed to request motor-en gpio: %d\n", err);
		return err;
	}

	vib->input = devm_input_allocate_device(dev);
	if (!vib->input)
		return -ENOMEM;

	vib->duration_ms = GPIO_VIBRATOR_DEF_MS;
	INIT_WORK(&vib->play_work, gpio_vibrator_play_work);
	hrtimer_init(&vib->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->stop_timer.function = gpio_vibrator_stop_timer;

	err = devm_add_action_or_reset(dev, gpio_vibrator_cancel_work, vib);
	if (err)
		return err;

	vib->input->name = GPIO_VIBRATOR_NAME;
	vib->input->dev.parent = dev;
	vib->input->close = gpio_vibrator_close;
	input_set_drvdata(vib->input, vib);

	input_set_capability(vib->input, EV_FF, FF_RUMBLE);

	err = input_ff_create_memless(vib->input, NULL,
				      gpio_vibrator_play_effect);
	if (err) {
		dev_err(dev, "failed to create force-feedback: %d\n", err);
		return err;
	}

	err = input_register_device(vib->input);
	if (err) {
		dev_err(dev, "failed to register input device: %d\n", err);
		return err;
	}
	err = devm_add_action_or_reset(dev, gpio_vibrator_unregister_input,
				       vib->input);
	if (err)
		return err;

	vib->tdev.name = "vibrator";
	vib->tdev.enable = gpio_vibrator_tdev_enable;
	vib->tdev.get_time = gpio_vibrator_tdev_get_time;
	err = timed_output_dev_register(&vib->tdev);
	if (err) {
		dev_err(dev, "failed to register timed_output: %d\n", err);
		return err;
	}
	err = devm_add_action_or_reset(dev, gpio_vibrator_unregister_timed,
				       &vib->tdev);
	if (err)
		return err;

	vib->cdev.name = "vibrator";
	vib->cdev.groups = gpio_vibrator_led_groups;
	err = devm_led_classdev_register(dev, &vib->cdev);
	if (err) {
		dev_err(dev, "failed to register vibrator led: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, vib);

	return 0;
}

static const struct of_device_id gpio_vibrator_of_match[] = {
	{ .compatible = GPIO_VIBRATOR_NAME, },
	{ }
};
MODULE_DEVICE_TABLE(of, gpio_vibrator_of_match);

static struct platform_driver gpio_vibrator_driver = {
	.probe	= gpio_vibrator_probe,
	.driver	= {
		.name		= GPIO_VIBRATOR_NAME,
		.of_match_table	= gpio_vibrator_of_match,
	},
};
module_platform_driver(gpio_vibrator_driver);

MODULE_DESCRIPTION("GPIO vibrator driver");
MODULE_LICENSE("GPL v2");
