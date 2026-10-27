/*
 * GPIO vibrator driver
 *
 * Drives a vibration motor that is switched on and off by a single GPIO
 * (no PWM and no regulator). The motor enable line may sit behind an I2C
 * GPIO expander, so the GPIO is always accessed with the sleeping API from
 * a work item.
 *
 * Only on/off is supported: any non-zero FF_RUMBLE magnitude turns the
 * motor on, zero turns it off.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define GPIO_VIBRATOR_NAME	"gpio-vibrator"

struct gpio_vibrator {
	struct input_dev *input;
	struct work_struct play_work;
	int gpio;
	u16 level;
};

static void gpio_vibrator_play_work(struct work_struct *work)
{
	struct gpio_vibrator *vib = container_of(work, struct gpio_vibrator,
						 play_work);

	gpio_set_value_cansleep(vib->gpio, vib->level ? 1 : 0);
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
	gpio_set_value_cansleep(vib->gpio, 0);
}

static void gpio_vibrator_cancel_work(void *data)
{
	struct gpio_vibrator *vib = data;

	cancel_work_sync(&vib->play_work);
	gpio_set_value_cansleep(vib->gpio, 0);
}

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

	INIT_WORK(&vib->play_work, gpio_vibrator_play_work);

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
