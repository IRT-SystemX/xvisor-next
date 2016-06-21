/**
 * Copyright (c) 2016 Open Wide
 * Copyright (c) 2016 Institut de Recherche Technologique SystemX
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file egalax.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief EETI eGalax touchscreen driver
 *
 *
 * This is based on the linux-imx_3.10.17 kernel:
 *
 * Copyright (C) 2011-2013 Freescale Semiconductor, Inc.
 *
 * based on max11801_ts.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG
#define DEV_DEBUG

#include <vmm_modules.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

/*
 * Mouse Mode: some panel may configure the controller to mouse mode,
 * which can only report one point at a given time.
 * This driver will ignore events in this mode.
 */
#define REPORT_MODE_SINGLE		0x1
/*
 * Vendor Mode: this mode is used to transfer some vendor specific
 * messages.
 * This driver will ignore events in this mode.
 */
#define REPORT_MODE_VENDOR		0x3
/* Multiple Touch Mode */
#define REPORT_MODE_MTTOUCH		0x4

#define MAX_SUPPORT_POINTS		5

#define EVENT_MODE		0
#define EVENT_STATUS		1
#define EVENT_VALID_OFFSET	7
#define EVENT_VALID_MASK	(0x1 << EVENT_VALID_OFFSET)
#define EVENT_ID_OFFSET		2
#define EVENT_ID_MASK		(0xf << EVENT_ID_OFFSET)
#define EVENT_IN_RANGE		(0x1 << 1)
#define EVENT_DOWN_UP		(0X1 << 0)

#define MAX_I2C_DATA_LEN	10

#define EGALAX_MAX_X   32767
#define EGALAX_MAX_Y   32767
#define EGALAX_MAX_TRIES 100

#define MODULE_DESC			"EETI eGalax Touchscreen Driver"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		I2C_IPRIORITY
#define	MODULE_INIT		        egalax_init
#define	MODULE_EXIT		        egalax_exit

struct egalax_pointer {
	bool valid;
	bool status;
	u16 x;
	u16 y;
};

struct egalax {
	struct vmm_work job;
	struct vmm_mutex lock;

	struct i2c_client *client;
	struct input_dev *input_dev;
	struct egalax_pointer		events[MAX_SUPPORT_POINTS];
	int gpio;
};

static void egalax_i2c_job(struct vmm_work *work)
{
	struct egalax *const ts = container_of(work, struct egalax, job);

	vmm_printf("***** work = %p, egalax = %p\n", work, ts);

	mutex_lock(&ts->lock);

	struct input_dev *input_dev = ts->input_dev;
	struct i2c_client *client = ts->client;
	struct egalax_pointer *events = ts->events;
	char buf[MAX_I2C_DATA_LEN];
	int i, id, ret, x, y;
	int tries = 0;
	bool down, valid;
	u8 state;

	vmm_lcritical(NULL, "Driver egalax is orphan: %i\n",
		  vmm_scheduler_orphan_context());

	do {
		ret = i2c_master_recv(client, buf, MAX_I2C_DATA_LEN);
	} while ((ret == -EAGAIN) && (tries++ < EGALAX_MAX_TRIES));

	if (ret < 0) {
		vmm_lerror(NULL, "Did not receive anything from I2C master\n");
		goto unlock;
	}

	dev_dbg(&client->dev, "recv ret:%d", ret);
	for (i = 0; i < MAX_I2C_DATA_LEN; i++) {
		dev_dbg(&client->dev, " %x ", buf[i]);
	}

	if ((buf[0] != REPORT_MODE_VENDOR) &&
	    (buf[0] != REPORT_MODE_SINGLE) &&
	    (buf[0] != REPORT_MODE_MTTOUCH)) {
		/* invalid point */
		vmm_lerror(NULL, "Invlid point\n");
		goto unlock;
	}

	if (buf[0] == REPORT_MODE_VENDOR) {
		dev_dbg(&client->dev, "vendor message, ignored\n");
		goto unlock;
	}

	state = buf[1];
	x = (buf[3] << 8) | buf[2];
	y = (buf[5] << 8) | buf[4];

	/* Currently, the panel Freescale using on SMD board _NOT_
	 * support single pointer mode. All event are going to
	 * multiple pointer mode.  Add single pointer mode according
	 * to EETI eGalax I2C programming manual.
	 */
	if (buf[0] == REPORT_MODE_SINGLE) {
		input_report_abs(input_dev, ABS_X, x);
		input_report_abs(input_dev, ABS_Y, y);
		input_report_key(input_dev, BTN_TOUCH, !!state);
		input_sync(input_dev);
		goto unlock;
	}

	valid = state & EVENT_VALID_MASK;
	id = (state & EVENT_ID_MASK) >> EVENT_ID_OFFSET;
	down = state & EVENT_DOWN_UP;

	if (!valid || id > MAX_SUPPORT_POINTS) {
		dev_dbg(&client->dev, "point invalid\n");
		goto unlock;
	}

	if (down) {
		events[id].valid = valid;
		events[id].status = down;
		events[id].x = x;
		events[id].y = y;

//#ifdef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
//		input_report_abs(input_dev, ABS_X, x);
//		input_report_abs(input_dev, ABS_Y, y);
//		input_event(ts->input_dev, EV_KEY, BTN_TOUCH, 1);
//		input_report_abs(input_dev, ABS_PRESSURE, 1);
//#endif
	} else {
		dev_dbg(&client->dev, "release id:%d\n", id);
		events[id].valid = 0;
		events[id].status = 0;
//#ifdef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
//		input_report_key(input_dev, BTN_TOUCH, 0);
//		input_report_abs(input_dev, ABS_PRESSURE, 0);
//#else
		input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
		input_event(input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(input_dev);
//#endif
	}

//#ifndef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
	/* report all pointers */
	for (i = 0; i < MAX_SUPPORT_POINTS; i++) {
		if (!events[i].valid)
			continue;
		dev_dbg(&client->dev, "report id:%d valid:%d x:%d y:%d",
			i, valid, x, y);
			input_report_abs(input_dev,
				 ABS_MT_TRACKING_ID, i);
		input_report_abs(input_dev,
				 ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_X, events[i].x);
		input_report_abs(input_dev,
				 ABS_MT_POSITION_Y, events[i].y);
		input_mt_sync(input_dev);
	}
//#endif
	input_sync(input_dev);

unlock:;
	mutex_unlock(&ts->lock);
//	gpio_direction_input(ts->gpio);
//	vmm_host_irq_set_type(client->irq, VMM_IRQ_TYPE_LEVEL_LOW);
//	vmm_host_irq_unmask(client->irq);
}


static irqreturn_t egalax_isr(int irq, void *dev_id)
{
	struct egalax *const e = dev_id;
	vmm_lalert(NULL, "%s()\n", __func__);
	vmm_workqueue_schedule_work(NULL, &e->job);
//	vmm_host_irq_set_type(e->client->irq, VMM_IRQ_TYPE_LEVEL_HIGH);
//	gpio_direction_output(e->gpio, 1);
	return IRQ_HANDLED;
}

static int egalax_firmware_version(struct i2c_client *client)
{
	static const char cmd[MAX_I2C_DATA_LEN] = { 0x03, 0x03, 0xa, 0x01, 0x41 };
	int ret;

	ret = i2c_master_send(client, cmd, MAX_I2C_DATA_LEN);
	if (ret < 0) {
		return ret;
	}

	return VMM_OK;
}

/* wake up controller by an falling edge of interrupt gpio.  */
static int egalax_wake_up_device(struct egalax *e)
{
	struct i2c_client *const client = e->client;
	struct device_node *const np = client->dev.of_node;
	int gpio;
	int ret;

	if (!np) {
		return -ENODEV;
	}

	gpio = of_get_named_gpio(np, "wakeup-gpios", 0);
	if (!gpio_is_valid(gpio))
		return -ENODEV;

	ret = gpio_request(gpio, "egalax_irq");
	if (ret < 0) {
		dev_err(&client->dev,
			"request gpio failed, cannot wake up controller: %d\n",
			ret);
		return ret;
	}

	/* wake up controller via an falling edge on IRQ gpio. */
	gpio_direction_output(gpio, 0);
	gpio_set_value(gpio, 1);

	/* controller should be waken up, return irq.  */
	gpio_direction_input(gpio);

	e->gpio = gpio;

	return VMM_OK;
}

static int egalax_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct egalax *e = NULL;
	struct input_dev *input_dev;
	int rc = VMM_EFAIL;

	vmm_lcritical(NULL, "%s()\n", __func__);
	e = vmm_zalloc(sizeof(*e));
	if (NULL == e) {
		vmm_lerror("egalax", "Failed to allocate memory\n");
		rc = VMM_ENOMEM;
		goto fail;
	}

	input_dev = input_allocate_device();
	if (NULL == input_dev) {
		vmm_lerror("egalax", "Failed to create input device\n");
		rc = VMM_ENOMEM;
		goto fail;
	}

	e->input_dev = input_dev;
	e->client = client;

	rc = egalax_wake_up_device(e);
	if (rc) {
		vmm_lerror("egalax", "Failed to wake up the controller\n");
		goto fail;
	}

	rc = egalax_firmware_version(client);
	if (rc < 0) {
		vmm_lerror("egalax", "Failed to read firmware version\n");
		rc = -EIO;
		goto fail;
	}

	input_dev->name = "eGalax Touch Screen";
	input_dev->phys = "I2C",
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x0EEF;
	input_dev->id.product = 0x0020;
	input_dev->id.version = 0x0001;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	input_set_abs_params(input_dev, ABS_X, 0, EGALAX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, EGALAX_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 1, 0, 0);

//#ifndef CONFIG_TOUCHSCREEN_EGALAX_SINGLE_TOUCH
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				0, EGALAX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, EGALAX_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
			     MAX_SUPPORT_POINTS, 0, 0);
//#endif

	input_set_drvdata(input_dev, e);

	vmm_lcritical(NULL, "Driver egalax is orphan: %i\n",
		  vmm_scheduler_orphan_context());
	vmm_lnotice(NULL," client irq is %i\n", client->irq);


	rc = input_register_device(e->input_dev);
	if (rc) {
		vmm_lerror("egalax", "Failed to register input device\n");
		goto fail_unregister;
	}

	i2c_set_clientdata(client, e);

	INIT_WORK(&e->job, egalax_i2c_job);
	INIT_MUTEX(&e->lock);

	vmm_printf("**** work = %p, egalax = %p, irq = %i\n", &e->job, e, client->irq);
	rc = vmm_host_irq_set_type(client->irq, VMM_IRQ_TYPE_LEVEL_LOW);
	if (rc) {
		vmm_lerror(NULL, "Failed to set IRQ type\n");
		goto fail;
	}
//	rc = vmm_host_irq_register(client->irq, "eGalax", egalax_isr, e);
//	if (rc) {
//		vmm_lerror(NULL, "Failed to register IRQ %i\n", client->irq);
//		goto fail;
//	}

	return VMM_OK;

fail_unregister:
	vmm_host_irq_unregister(client->irq, e);
fail:
	if (e) {
		if (e->input_dev) {
			input_free_device(e->input_dev);
		}
		vmm_free(e);
	}
	return rc;
}

static int egalax_remove(struct i2c_client *client)
{
	struct egalax *const e = i2c_get_clientdata(client);
	input_unregister_device(e->input_dev);
	vmm_host_irq_unregister(e->client->irq, e);
	vmm_workqueue_stop_work(&e->job);
	vmm_free(e);
	return VMM_OK;
}

static const struct i2c_device_id egalax_id[] = {
	{ "egalax_ts", 0 },
	{ /* Sentinel */ }
};

static const struct vmm_devtree_nodeid egalax_dt_ids[] = {
	{ .compatible = "eeti,egalax_ts" },
	{ /* Sentinel */ }
};

static struct i2c_driver egalax_driver = {
	.driver = {
		.name = "EETI eGalax",
		.match_table = egalax_dt_ids,
	},
	.probe = egalax_probe,
	.remove = egalax_remove,
	.id_table = egalax_id,
};

static int __init egalax_init(void)
{
	return i2c_add_driver(&egalax_driver);
}

static void __exit egalax_exit(void)
{
	i2c_del_driver(&egalax_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
