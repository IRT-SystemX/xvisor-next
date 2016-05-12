/**
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file virtio_i2c.c
 * @author Mickaël TANSORIER (mickael.tansorier@openwide.fr)
 * @brief i2c virtio.
 * @details This source file implements the virtio for i2c.
 *
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <linux/types.h>
#include <emu/virtio.h>
#include <emu/virtio_i2c.h>
#include <linux/i2c.h>

#define MODULE_DESC			"VirtIO i2c Emulator"
#define MODULE_AUTHOR			"Mickaël Tansorier"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_i2c_init
#define MODULE_EXIT			virtio_i2c_exit

#define VIRTIO_I2C_QUEUE	0
#define VIRTIO_I2C_QUEUE_SIZE	128
#define VIRTIO_I2C_NUM_QUEUES	1


struct virtio_i2c_dev {
	struct virtio_device *vdev;

	struct virtio_queue vqs[VIRTIO_I2C_NUM_QUEUES];
	struct virtio_iovec iov[VIRTIO_I2C_QUEUE_SIZE];

	char name[VIRTIO_DEVICE_MAX_NAME_LEN];

	struct i2c_adapter *adapter;
	struct i2c_msg msg;
};

static int i2cimx_attach_adapter(struct device *dev, void *dummy)
{
	const char *attr;
	struct i2c_adapter *adap;
	struct virtio_device *virtiodev;
	struct virtio_i2c_dev *i2cdev;

	/* get adapter */
	if (dev->type != &i2c_adapter_type)
		return VMM_EINVALID;
	adap = to_i2c_adapter(dev);

	/* set right adapter to i2c_imx_state */
	virtiodev = dummy;
	i2cdev = virtiodev->emu_data;

	/* Attach i2c adapter */
	if (vmm_devtree_read_string(virtiodev->edev->node,
				    "i2c_attach", &attr) == VMM_OK) {
		if (strcmp(adap->name,attr)==0) {
			i2cdev->adapter = adap;
		}
	}

	return VMM_OK;
}

static int get_msg_buf(struct virtio_i2c_dev *vi2cdev)
{
	u32 len_read = 0;

	/* READ ADDR */
	len_read += virtio_iovec_to_buf_read(vi2cdev->vdev,
				&vi2cdev->iov[0], 1,
				&vi2cdev->msg.addr, sizeof(vi2cdev->msg.addr));

	/* READ FLAGS */
	len_read += virtio_iovec_to_buf_read(vi2cdev->vdev,
			&vi2cdev->iov[1], 1,
			&vi2cdev->msg.flags, sizeof(vi2cdev->msg.flags));

	if (!(vi2cdev->msg.flags & I2C_M_RD))
	{
		/* READ LEN */
		len_read += virtio_iovec_to_buf_read(vi2cdev->vdev,
				&vi2cdev->iov[2], 1,
				&vi2cdev->msg.len, sizeof(vi2cdev->msg.len));

		/* READ BUF */
		len_read += virtio_iovec_to_buf_read(vi2cdev->vdev,
				&vi2cdev->iov[3], 1,
				vi2cdev->msg.buf, sizeof(vi2cdev->msg.buf));
	}
	return len_read;
}

static int set_msg_buf(struct virtio_i2c_dev *vi2cdev, int ret)
{
	u32 len_write = 0;

	/* read */
	if (vi2cdev->msg.flags & I2C_M_RD)
	{
		/* WRITE LEN */
		len_write += virtio_buf_to_iovec_write(vi2cdev->vdev,
				&vi2cdev->iov[2], 1,
				&vi2cdev->msg.len, sizeof(vi2cdev->msg.len));
		/* WRITE BUF */
		len_write += virtio_buf_to_iovec_write(vi2cdev->vdev,
				&vi2cdev->iov[3], 1,
				vi2cdev->msg.buf, sizeof(vi2cdev->msg.buf));
	}

	/* WRITE RET */
	len_write += virtio_buf_to_iovec_write(vi2cdev->vdev,
					&vi2cdev->iov[4], 1,
					&ret, sizeof(ret));

	return len_write;
}

static void orphan_i2c_transfer(struct vmm_guest * guest, void *param){

	int ret;
        struct virtio_i2c_dev *vi2cdev = param;
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0, len_write = 0, len_read = 0;;
	struct virtio_queue *vq = &vi2cdev->vqs[VIRTIO_I2C_QUEUE];
	struct virtio_device *vdev = vi2cdev->vdev;

	/* virtio queue available */
	if (virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, vi2cdev->iov, &iov_cnt,
								&total_len);
		if (iov_cnt) {
			len_read += get_msg_buf(vi2cdev);

			/* i2c transfer */
			ret = i2c_transfer(vi2cdev->adapter, &vi2cdev->msg, 1);
			if (ret < 1)
				vmm_printf("virtio i2c: call i2c transfert \
fail with error %d\n", ret);

			len_write += set_msg_buf(vi2cdev, ret);

			virtio_queue_set_used_elem(vq, head,
							len_read + len_write);

			/* signal */
			if (virtio_queue_should_signal(vq)) {
				vdev->tra->notify(vdev, VIRTIO_I2C_QUEUE);
			}
		}
	}
}


static int i2c_recv_msgs(struct virtio_device *dev)
{
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	vmm_manager_guest_request(dev->guest, orphan_i2c_transfer, vi2cdev);

	return VMM_OK;
}

static u32 virtio_i2c_get_host_features(struct virtio_device *dev)
{
	return 0;
}

static void virtio_i2c_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	/* No host features so, ignore it. */
}

static int virtio_i2c_init_vq(struct virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		rc = virtio_queue_setup(&vi2cdev->vqs[vq], dev->guest,
			pfn, page_size, VIRTIO_I2C_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_i2c_get_pfn_vq(struct virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		rc = virtio_queue_guest_pfn(&vi2cdev->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_i2c_get_size_vq(struct virtio_device *dev, u32 vq)
{
	int rc;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		rc = VIRTIO_I2C_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_i2c_set_size_vq(struct virtio_device *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}


static int virtio_i2c_notify_vq(struct virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		rc = i2c_recv_msgs(dev);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

static int virtio_i2c_read_config(struct virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	return 0;
}

static int virtio_i2c_write_config(struct virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	return 0;
}

static int virtio_i2c_reset(struct virtio_device *dev)
{
	int rc;
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	rc = virtio_queue_cleanup(&vi2cdev->vqs[VIRTIO_I2C_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_i2c_connect(struct virtio_device *dev, 
				  struct virtio_emulator *emu)
{
	int ret;
	struct virtio_i2c_dev *i2cdev;

	i2cdev = vmm_zalloc(sizeof(struct virtio_i2c_dev));
	if (!i2cdev)
		goto out;

	i2cdev->vdev = dev;
	vmm_snprintf(i2cdev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);

	dev->emu_data = i2cdev;

	i2cdev->msg.buf = vmm_zalloc(sizeof(__u8)*VIRTIO_I2C_QUEUE_SIZE);
	if (!i2cdev->msg.buf)
		goto free_i2cdev;

	/* Get adapters */
	i2cdev->adapter = NULL;
	ret = i2c_for_each_dev(dev, i2cimx_attach_adapter);
	if (!ret)
		goto free_buf;

	rt_mutex_init(&i2cdev->adapter->bus_lock);

	return VMM_OK;

free_buf:
	vmm_free(i2cdev->msg.buf);
free_i2cdev:
	vmm_free(i2cdev);
out:
	return VMM_ENOMEM;
}

static void virtio_i2c_disconnect(struct virtio_device *dev)
{
	struct virtio_i2c_dev *i2cdev = dev->emu_data;

	vmm_free(i2cdev->msg.buf);
	vmm_free(i2cdev);
}

struct virtio_device_id virtio_i2c_emu_id[] = {
	{.type = VIRTIO_ID_I2C},
	{ },
};

struct virtio_emulator virtio_i2c = {
	.name = "virtio_i2c",
	.id_table = virtio_i2c_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_i2c_get_host_features,
	.set_guest_features     = virtio_i2c_set_guest_features,
	.init_vq                = virtio_i2c_init_vq,
	.get_pfn_vq             = virtio_i2c_get_pfn_vq,
	.get_size_vq            = virtio_i2c_get_size_vq,
	.set_size_vq            = virtio_i2c_set_size_vq,
	.notify_vq              = virtio_i2c_notify_vq,

	/* Emulator operations */
	.read_config = virtio_i2c_read_config,
	.write_config = virtio_i2c_write_config,
	.reset = virtio_i2c_reset,
	.connect = virtio_i2c_connect,
	.disconnect = virtio_i2c_disconnect,
};

static int __init virtio_i2c_init(void)
{
	return virtio_register_emulator(&virtio_i2c);
}

static void __exit virtio_i2c_exit(void)
{
	virtio_unregister_emulator(&virtio_i2c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
