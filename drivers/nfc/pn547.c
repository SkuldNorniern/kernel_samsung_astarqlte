/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 */
#define pr_fmt(fmt)     "[pn547] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include "pn547.h"
#include <linux/of_gpio.h>

static bool pn547_fw_download_allowed(void)
{
	if (IS_ENABLED(CONFIG_NFC_PN547_ALLOW_FW_WRITE))
		return true;

	pr_warn_once("pn547: firmware download mode blocked (NFC_PN547_ALLOW_FW_WRITE is off)\n");
	return false;
}
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/signal.h>

#define SIG_NFC 44
#define MAX_BUFFER_SIZE		512
#define NFC_DEBUG 0

#define NFC_LOG_INFO(fmt, ...) pr_info(fmt, ##__VA_ARGS__)
#define NFC_LOG_ERR(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define NFC_LOG_DBG(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)
#define NFC_LOG_REC(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#define FEATURE_NFC_TEST
#define FEATURE_PN80T

struct pn547_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct i2c_client *client;
	struct miscdevice pn547_device;
	unsigned int ven_gpio;
	unsigned int firm_gpio;
	unsigned int irq_gpio;
	int pvdd_gpio;
	struct regulator * nfc_pvdd;
	struct regulator * i2c_1p8;

	bool                spi_ven_enabled;
	bool                nfc_ven_enabled;

	atomic_t irq_enabled;
	atomic_t read_flag;
	bool cancel_read;
	struct wakeup_source *nfc_wake_lock;
	struct clk *nfc_clock;
	int i2c_probe;
	struct   clk *clk;


	long	nfc_service_pid; /*used to signal the nfc the nfc service */
};

static struct pn547_dev *pn547_dev;
static atomic_t s_Device_opened = ATOMIC_INIT(1);


static irqreturn_t pn547_dev_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
		NFC_LOG_ERR("irq_gpio = %d\n",
			gpio_get_value(pn547_dev->irq_gpio));
		return IRQ_HANDLED;
	}

	/* Wake up waiting readers */
	atomic_set(&pn547_dev->read_flag, 1);
	wake_up(&pn547_dev->read_wq);

	NFC_LOG_REC("irq handler called\n");

	__pm_wakeup_event(pn547_dev->nfc_wake_lock, 2000);
	return IRQ_HANDLED;
}


static ssize_t pn547_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE] = { 0, };
	int ret = 0;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	NFC_LOG_DBG("reading %zu bytes. irq=%s\n", count,
			gpio_get_value(pn547_dev->irq_gpio) ? "1" : "0");


	mutex_lock(&pn547_dev->read_mutex);

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
		atomic_set(&pn547_dev->read_flag, 0);
		if (filp->f_flags & O_NONBLOCK) {
			NFC_LOG_ERR("O_NONBLOCK\n");
			ret = -EAGAIN;
			goto fail;
		}

		if (!gpio_get_value(pn547_dev->irq_gpio))
			ret = wait_event_interruptible(pn547_dev->read_wq,
					atomic_read(&pn547_dev->read_flag));


		if (pn547_dev->cancel_read) {
			pn547_dev->cancel_read = false;
			ret = -1;
			goto fail;
		}

		if (ret)
			goto fail;
	}

	/* Read data */
	ret = i2c_master_recv(pn547_dev->client, tmp, count);
	NFC_LOG_REC("recv size : %d\n", ret);

	mutex_unlock(&pn547_dev->read_mutex);
	if (ret < 0) {
		NFC_LOG_ERR("i2c_master_recv returned (%d,%d)\n",
				ret, pn547_dev->i2c_probe);
		return ret;
	}

	if (ret > count) {
		NFC_LOG_ERR("received too many bytes from i2c (%d)\n",
				ret);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		NFC_LOG_ERR("failed to copy to user space\n");
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn547_dev->read_mutex);
	return ret;
}

static ssize_t pn547_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev;
	char tmp[MAX_BUFFER_SIZE] = { 0,};
	int ret = 0, retry = 2;

	pn547_dev = filp->private_data;


	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		NFC_LOG_ERR("failed to copy from user space\n");
		return -EFAULT;
	}

	NFC_LOG_REC("writing %zu bytes.\n", count);
	/* Write data */
	do {
		retry--;
		ret = i2c_master_send(pn547_dev->client, tmp, count);
		if (ret == count)
			break;
		usleep_range(6000, 10000); /* Retry, chip was in standby */
	} while (retry);


	if (ret != count) {
		NFC_LOG_ERR("i2c_master_send returned (%d,%d)\n",
				ret, pn547_dev->i2c_probe);
		ret = -EIO;
	}

	return ret;
}

static int pn547_dev_open(struct inode *inode, struct file *filp)
{
	struct pn547_dev *pn547_dev = container_of(filp->private_data,
						   struct pn547_dev,
						   pn547_device);

	if (!atomic_dec_and_test(&s_Device_opened)) {
		atomic_inc(&s_Device_opened);
		NFC_LOG_ERR("already opened!\n");
		return -EBUSY;
	}

	filp->private_data = pn547_dev;
	NFC_LOG_INFO("imajor:%d, iminor:%d (%d)\n", imajor(inode), iminor(inode),
			pn547_dev->i2c_probe);

	return 0;
}

static int pn547_dev_release(struct inode *inode, struct file *filp)
{

	NFC_LOG_INFO("release\n");
	atomic_inc(&s_Device_opened);

	return 0;
}


long pn547_dev_ioctl(struct file *filp,
			   unsigned int cmd, unsigned long arg)
{
	/*struct pn547_dev *pn547_dev = filp->private_data;*/
	switch (cmd) {
	case PN547_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			if (!pn547_fw_download_allowed())
				return -EPERM;
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 1);
			gpio_set_value(pn547_dev->firm_gpio, 1);
			usleep_range(4900, 5000);
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 0);
			usleep_range(4900, 5000);
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 1);
			usleep_range(4900, 5000);
			if (atomic_read(&pn547_dev->irq_enabled) == 0) {
				atomic_set(&pn547_dev->irq_enabled, 1);
				enable_irq(pn547_dev->client->irq);
				enable_irq_wake(pn547_dev->client->irq);
			}
			NFC_LOG_INFO("power on with firmware, irq=%d\n",
				atomic_read(&pn547_dev->irq_enabled));
		} else if (arg == 1) {
			/* power on */
			gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 1);
			usleep_range(4900, 5000);
			if (atomic_read(&pn547_dev->irq_enabled) == 0) {
				atomic_set(&pn547_dev->irq_enabled, 1);
				enable_irq(pn547_dev->client->irq);
				enable_irq_wake(pn547_dev->client->irq);
			}
			NFC_LOG_INFO("power on, irq=%d\n", atomic_read(&pn547_dev->irq_enabled));
		} else if (arg == 0) {
			/* power off */
			if (atomic_read(&pn547_dev->irq_enabled) == 1) {
				atomic_set(&pn547_dev->irq_enabled, 0);
				disable_irq_wake(pn547_dev->client->irq);
				disable_irq_nosync(pn547_dev->client->irq);
			}
			NFC_LOG_INFO("power off, irq=%d\n", atomic_read(&pn547_dev->irq_enabled));
			gpio_set_value(pn547_dev->firm_gpio, 0);
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 0);
			usleep_range(4900, 5000);
		} else if (arg == 3) {
			NFC_LOG_INFO("Read Cancel\n");
			pn547_dev->cancel_read = true;
			atomic_set(&pn547_dev->read_flag, 1);
			wake_up(&pn547_dev->read_wq);
		} else {
			NFC_LOG_ERR("bad arg %lu\n", arg);
			return -EINVAL;
		}
		break;
	default:
		NFC_LOG_ERR("bad ioctl %u\n", cmd);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(pn547_dev_ioctl);

static const struct file_operations pn547_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = pn547_dev_read,
	.write = pn547_dev_write,
	.open = pn547_dev_open,
	.release = pn547_dev_release,
	.unlocked_ioctl = pn547_dev_ioctl,
};

static int pn547_parse_dt(struct device *dev,
		struct pn547_dev *pdev)
{
	struct device_node *np = dev->of_node;
	int gpio;

	/*
	 * GPIOs routed through the PCAL6524 expander return -EPROBE_DEFER
	 * until the expander driver has registered; hand that to the caller.
	 */
	gpio = of_get_named_gpio(np, "pn547,irq-gpio", 0);
	if (gpio < 0)
		return gpio;
	pdev->irq_gpio = gpio;

	gpio = of_get_named_gpio(np, "pn547,ven-gpio", 0);
	if (gpio < 0)
		return gpio;
	pdev->ven_gpio = gpio;

	gpio = of_get_named_gpio(np, "pn547,firm-gpio", 0);
	if (gpio < 0)
		return gpio;
	pdev->firm_gpio = gpio;

	pdev->nfc_pvdd = devm_regulator_get_optional(dev, "pn547,nfc_pvdd");
	if (IS_ERR(pdev->nfc_pvdd)) {
		if (PTR_ERR(pdev->nfc_pvdd) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		NFC_LOG_INFO("nfc_pvdd ldo is not used\n");
		pdev->nfc_pvdd = NULL;
	}

	pdev->i2c_1p8 = devm_regulator_get_optional(dev, "pn547,i2c_1p8");
	if (IS_ERR(pdev->i2c_1p8)) {
		if (PTR_ERR(pdev->i2c_1p8) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		NFC_LOG_INFO("i2c_1p8 ldo is not used\n");
		pdev->i2c_1p8 = NULL;
	}

	gpio = of_get_named_gpio(np, "pn547,pvdd-gpio", 0);
	if (gpio == -EPROBE_DEFER)
		return gpio;
	if (gpio < 0) {
		NFC_LOG_INFO("pvdd-gpio is not used\n");
		pdev->pvdd_gpio = 0;
	} else {
		pdev->pvdd_gpio = gpio;
		NFC_LOG_INFO("pvdd gpio :%d\n", pdev->pvdd_gpio);
	}

	if (of_get_property(dev->of_node, "pn547,nfc_pm_clk", NULL)) {
		pdev->clk = clk_get(dev, "rf_clk");
		if (IS_ERR(pdev->clk)) {
			NFC_LOG_ERR("Couldn't get rf_clk\n");
		} else {
			NFC_LOG_INFO("got rf_clk\n");
			clk_prepare_enable(pdev->clk);
		}
	}

	NFC_LOG_INFO("irq : %d, ven : %d, firm : %d\n", pdev->irq_gpio,
		pdev->ven_gpio, pdev->firm_gpio);

	return 0;
}

static int pn547_regulator_onoff(struct device *dev,
		struct pn547_dev *pdev, struct regulator * regulator_nfc, int onoff)
{
	int rc = 0;

	NFC_LOG_INFO("onoff = %d\n", onoff);

	if (onoff == NFC_I2C_LDO_ON) {
		rc = regulator_enable(regulator_nfc);
		if (rc) {
			NFC_LOG_ERR("enable failed, rc=%d\n", rc);
			goto done;
		}
	} else {
		rc = regulator_disable(regulator_nfc);
		if (rc) {
			NFC_LOG_ERR("disable failed, rc=%d\n", rc);
			goto done;
		}
	}

	NFC_LOG_INFO("success\n");
done:
	return rc;
}

static ssize_t pn547_class_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int size;
	int ret = 0;
	int count = 4;
	char tmp[128] = {0x20, 0x00, 0x01, 0x00, };
	int retry;
	bool old_ven, old_irq;
	int old_read_value;

	NFC_LOG_INFO("start\n");


	old_ven = pn547_dev->nfc_ven_enabled;
	NFC_LOG_INFO("old_ven is %d\n", old_ven);
	retry = 20;
	if (!old_ven) {	/* if nfc status is off */
		pn547_dev->nfc_ven_enabled = true;

		if (pn547_dev->spi_ven_enabled == false)
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 1);
		usleep_range(4900, 5000);
	} else {	/* if nfc status is on */
		/*wake up device*/
		gpio_set_value_cansleep(pn547_dev->ven_gpio, 0);
		usleep_range(4900, 5000);
		gpio_set_value_cansleep(pn547_dev->ven_gpio, 1);
		usleep_range(4900, 5000);
		//intercept i2c_master_recv
		pn547_dev->cancel_read = 1;
		atomic_set(&pn547_dev->read_flag, 1);
	}
	wake_up_all(&pn547_dev->read_wq);
	while (!mutex_trylock(&pn547_dev->read_mutex) && --retry)
		usleep_range(15, 20);

	if (!retry) {
		NFC_LOG_ERR("mutex_trylock failed. check pn547_dev_read()\n");
		ret = sprintf(buf, "test failed : device in use\n");
		goto fail_lock;
	}

	NFC_LOG_INFO("read_mutex locked. retry : %d\n", retry);

	atomic_set(&pn547_dev->read_flag, 0);
	pn547_dev->cancel_read = 0;
	old_irq = atomic_read(&pn547_dev->irq_enabled);
	NFC_LOG_INFO("old_irq is %d\n", old_irq);
	if (!old_irq) {
		atomic_set(&pn547_dev->irq_enabled, 1);
		enable_irq(pn547_dev->client->irq);
		enable_irq_wake(pn547_dev->client->irq);
		NFC_LOG_INFO("power on, irq=%d\n", atomic_read(&pn547_dev->irq_enabled));
	}
	retry = 2;
	do {
		ret = i2c_master_send(pn547_dev->client, tmp, count);
		if (count == ret)
			break;

		NFC_LOG_INFO("i2c_master_send error. ret:%d, retry:%d\n", ret, retry);
		usleep_range(6000, 10000); /* Retry, chip was in standby */
	} while (retry--);

	if (ret != count) {
		NFC_LOG_ERR("failed. count error. send=%d, recv=%d\n", count, ret);
		ret = 0;
		goto fail;
	}

	NFC_LOG_INFO("send success. returned: %d\n", ret);

	//wait for reply
	ret = 0;
	old_read_value = atomic_read(&pn547_dev->read_flag);
	NFC_LOG_INFO("read_flag %d, cancel_read %d", old_read_value,
		pn547_dev->cancel_read);
	if (!old_read_value) {
		ret = wait_event_interruptible(pn547_dev->read_wq,
			atomic_read(&pn547_dev->read_flag));
	}


	if (pn547_dev->cancel_read) {
		pn547_dev->cancel_read = false;
		ret = 0;
		//todo : old_ven and old_irq rollback needed
		goto fail;
	}

	if (ret) {
		ret = 0;
		goto fail;
	}

	/* Read data */
	count = 6;
	ret = i2c_master_recv(pn547_dev->client, tmp, count);
	mutex_unlock(&pn547_dev->read_mutex);

	if (!old_ven) {	/* if nfc status is off */
		if (pn547_dev->spi_ven_enabled == false)
			gpio_set_value_cansleep(pn547_dev->ven_gpio, 0);
		usleep_range(4900, 5000);
		pn547_dev->nfc_ven_enabled = false;
	}
	if (!old_irq) {
		atomic_set(&pn547_dev->irq_enabled, 0);
		disable_irq_wake(pn547_dev->client->irq);
		disable_irq_nosync(pn547_dev->client->irq);
	}
	if (ret < 0 || ret > count) {
		NFC_LOG_ERR("i2c_master_recv returned %d. count : %d\n",
			ret, count);
		return 0;
	}

	size = sprintf(buf, "test completed!! size: %d, data: %X %X %X %X %X %X\n",
		ret, tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
	NFC_LOG_INFO("recv success.\n");
	usleep_range(10000, 10100);

	return size;

fail:
	mutex_unlock(&pn547_dev->read_mutex);
fail_lock:
	return ret;
}

static ssize_t pn547_class_store(struct class *class,
	struct class_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static struct class_attribute class_attr_test =
	__ATTR(test, 0664, pn547_class_show, pn547_class_store);

static ssize_t nfc_support_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	NFC_LOG_INFO("\n");
	return 0;
}

static struct class_attribute class_attr_nfc_support =
	__ATTR(nfc_support, 0444, nfc_support_show, NULL);

static int pn547_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int err;
	int addr;
	char tmp[4] = {0x20, 0x00, 0x01, 0x01};
	int addrcnt;
	struct class *nfc_class;


	NFC_LOG_INFO("entered\n");

	if (client->dev.of_node) {
		pn547_dev = devm_kzalloc(&client->dev, sizeof(struct pn547_dev), GFP_KERNEL);
		if (!pn547_dev)
			return -ENOMEM;

		err = pn547_parse_dt(&client->dev, pn547_dev);
		if (err)
			return err;
	} else {
		NFC_LOG_ERR("no dts\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		NFC_LOG_ERR("need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	ret = gpio_request(pn547_dev->irq_gpio, "nfc_irq");
	if (ret)
		return ret;
	ret = gpio_request(pn547_dev->ven_gpio, "nfc_ven");
	if (ret)
		goto err_ven;
	ret = gpio_request(pn547_dev->firm_gpio, "nfc_firm");
	if (ret)
		goto err_firm;

	if (pn547_dev->nfc_pvdd) {
		ret = pn547_regulator_onoff(&client->dev, pn547_dev, pn547_dev->nfc_pvdd, NFC_I2C_LDO_ON);
		if (ret < 0)
			NFC_LOG_ERR("NFC_PVDD regulator_on failed err:%d\n", ret);
	}

	if (pn547_dev->i2c_1p8) {
		ret = pn547_regulator_onoff(&client->dev, pn547_dev, pn547_dev->i2c_1p8, NFC_I2C_LDO_ON);
		if (ret < 0)
			NFC_LOG_ERR("I2C_1P8 regulator_on failed err:%d\n", ret);
	}

	if (pn547_dev->pvdd_gpio) {
		ret = gpio_request(pn547_dev->pvdd_gpio, "nfc_pvdd_gpio");
		if (ret < 0)
			NFC_LOG_ERR("pvdd-gpio gpio_request failed, ret : %d\n", ret);
		else
			gpio_direction_output(pn547_dev->pvdd_gpio, 1);
	}
	usleep_range(1000, 1100);


	client->irq = gpio_to_irq(pn547_dev->irq_gpio);
	NFC_LOG_INFO("IRQ num %d\n", client->irq);

	pn547_dev->client = client;

	/* init mutex and queues */
	init_waitqueue_head(&pn547_dev->read_wq);
	mutex_init(&pn547_dev->read_mutex);


	pn547_dev->nfc_wake_lock = wakeup_source_register(&client->dev, "nfc_wake_lock");
	if (!pn547_dev->nfc_wake_lock) {
		ret = -ENOMEM;
		goto err_misc_register;
	}

	pn547_dev->pn547_device.minor = MISC_DYNAMIC_MINOR;
	pn547_dev->pn547_device.name = "pn547";
	pn547_dev->pn547_device.fops = &pn547_dev_fops;

	ret = misc_register(&pn547_dev->pn547_device);
	if (ret) {
		NFC_LOG_ERR("misc_register failed\n");
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	NFC_LOG_INFO("requesting IRQ %d\n", client->irq);
	gpio_direction_input(pn547_dev->irq_gpio);
	gpio_direction_output(pn547_dev->ven_gpio, 0);
	gpio_direction_output(pn547_dev->firm_gpio, 0);

	i2c_set_clientdata(client, pn547_dev);
	ret = request_irq(client->irq, pn547_dev_irq_handler, IRQF_TRIGGER_RISING,
		"pn547", pn547_dev);
	if (ret) {
		NFC_LOG_ERR("request_irq failed\n");
		goto err_request_irq_failed;
	}
	disable_irq_nosync(pn547_dev->client->irq);
	atomic_set(&pn547_dev->irq_enabled, 0);


	if (!pn547_fw_download_allowed()) {
		/* no download mode entry: keep the default address, skip the scan */
		client->addr = 0x2B;
		ret = 0;
	} else {
		gpio_set_value(pn547_dev->ven_gpio, 1);
		gpio_set_value(pn547_dev->firm_gpio, 1); /* add firmware pin */
		usleep_range(4900, 5000);
		gpio_set_value(pn547_dev->ven_gpio, 0);
		usleep_range(4900, 5000);
		gpio_set_value(pn547_dev->ven_gpio, 1);
		usleep_range(4900, 5000);

		for (addr = 0x2B; addr > 0x27; addr--) {
			client->addr = addr;
			addrcnt = 2;
			do {
				ret = i2c_master_send(client, tmp, 4);
				if (ret > 0) {
					NFC_LOG_INFO("i2c addr(0x%X), ret(%d)\n",
						client->addr, ret);
					pn547_dev->i2c_probe = ret;
					break;
				}
			} while (addrcnt--);
			if (ret > 0)
				break;
		}

		if (ret <= 0) {
			NFC_LOG_INFO("ret(%d), i2c_probe(%d)\n", ret, pn547_dev->i2c_probe);
			client->addr = 0x2B;
		}
	}
	gpio_set_value(pn547_dev->ven_gpio, 0);
	gpio_set_value(pn547_dev->firm_gpio, 0); /* add */

	if (ret < 0)
		NFC_LOG_ERR("fail to get i2c addr\n");
	else
		NFC_LOG_INFO("success, i2c_probe(%d)\n", pn547_dev->i2c_probe);

	nfc_class = class_create(THIS_MODULE, "nfc_test");
	if (IS_ERR(nfc_class)) {
		NFC_LOG_ERR("failed to create nfc_test class\n");
	} else {
		ret = class_create_file(nfc_class, &class_attr_test);
		if (ret)
			NFC_LOG_ERR("failed to create attr_test file\n");
	}
	nfc_class = class_create(THIS_MODULE, "nfc");
	if (IS_ERR(nfc_class)) {
		NFC_LOG_ERR("failed to create nfc class\n");
	} else {
		ret = class_create_file(nfc_class, &class_attr_nfc_support);
		if (ret)
			NFC_LOG_ERR("failed to create nfc_support file\n");
	}

	return 0;

err_request_irq_failed:
	misc_deregister(&pn547_dev->pn547_device);
err_misc_register:
	wakeup_source_unregister(pn547_dev->nfc_wake_lock);
	mutex_destroy(&pn547_dev->read_mutex);
	gpio_free(pn547_dev->firm_gpio);
err_firm:
	gpio_free(pn547_dev->ven_gpio);
err_ven:
	gpio_free(pn547_dev->irq_gpio);
	devm_kfree(&client->dev, pn547_dev);
	NFC_LOG_ERR("failed!\n");
	return ret;
}

static int pn547_remove(struct i2c_client *client)
{
	struct pn547_dev *pn547_dev;

	NFC_LOG_INFO("removing pn547 driver\n");
	pn547_dev = i2c_get_clientdata(client);

	wakeup_source_unregister(pn547_dev->nfc_wake_lock);
	free_irq(client->irq, pn547_dev);
	misc_deregister(&pn547_dev->pn547_device);
	mutex_destroy(&pn547_dev->read_mutex);
	gpio_free(pn547_dev->irq_gpio);
	gpio_free(pn547_dev->ven_gpio);
	gpio_free(pn547_dev->firm_gpio);

	if (pn547_dev->i2c_1p8)
		devm_regulator_put(pn547_dev->i2c_1p8);
	if (pn547_dev->nfc_pvdd)
		devm_regulator_put(pn547_dev->nfc_pvdd);


	return 0;
}

static const struct i2c_device_id pn547_id[] = {
	{"pn547", 0},
	{}
};

static const struct of_device_id nfc_match_table[] = {
	{ .compatible = "pn547",},
	{},
};

static struct i2c_driver pn547_driver = {
	.id_table = pn547_id,
	.probe = pn547_probe,
	.remove = pn547_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "pn547",
		.of_match_table = nfc_match_table,
		.suppress_bind_attrs = true,
	},
};

/*
 * module load/unload record keeping
 */
static int __init pn547_dev_init(void)
{
	NFC_LOG_INFO("Loading pn547 driver\n");
	return i2c_add_driver(&pn547_driver);
}

module_init(pn547_dev_init);

static void __exit pn547_dev_exit(void)
{
	NFC_LOG_INFO("Unloading pn547 driver\n");
	i2c_del_driver(&pn547_driver);
}

module_exit(pn547_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN547 driver");
MODULE_LICENSE("GPL");
