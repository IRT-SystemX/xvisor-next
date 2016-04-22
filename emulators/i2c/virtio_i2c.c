

#include <vmm_error.h>
//#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>

//#include <vio/vmm_vserial.h>
//#include <libs/fifo.h>
//#include <libs/stringlib.h>
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

/********************************/
struct virtio_i2c_dev {
	struct virtio_device *vdev;

	struct virtio_queue vqs[VIRTIO_I2C_NUM_QUEUES];
	struct virtio_iovec iov[VIRTIO_I2C_QUEUE_SIZE];
//	u32 features;

	char name[VIRTIO_DEVICE_MAX_NAME_LEN];
//	struct vmm_vserial *vser;
//	struct fifo *emerg_rd;

	struct i2c_adapter *adapter;
	struct i2c_msg msg;

};

struct msg_info
{
	__u16 addr;
	__u16 flags;
	__u16 len;
};
//__u8 *msg_buf;


/*************************************************/
/********************* FONC **********************/
/*************************************************/

static void print_msgs(const char * func, struct i2c_msg *msgs, int num)
{
	int i;

	for (i=0; i<num; i++)
	{
		vmm_printf("---- %s: msgs: addr=0x%016x | flags=0x%016x | len=0x%016x\n",
				func, msgs->addr, msgs->flags, msgs->len);
		for (i=0;i<msgs->len;i++)
			vmm_printf("---- %s: msgs->buf[%d] = 0x%08x\n",
				func, i, msgs->buf[i]);
	}
}


static int i2cimx_attach_adapter(struct device *dev, void *dummy)
{
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

	vmm_printf("---- %s: adapter %s is find \n",__func__, adap->name);
	if (strcmp(adap->name, "i2c@021a0000") == 0)
	{
		i2cdev->adapter = adap;
		vmm_printf("---- %s: adapter %s is set \n",__func__, adap->name);
	}

	return VMM_OK;
}

static void orphan_i2c_transfer(struct vmm_guest * guest, void *param){
        vmm_printf("=>|orphan_i2c_transfer|\n");

        struct virtio_i2c_dev *vi2cdev = param;
	u16 head = 0;
	u32 i = 0, iov_cnt = 0, total_len = 0;
	struct virtio_iovec *iov = vi2cdev->in_iov;
	struct virtio_queue *vq = &vi2cdev->vqs[VIRTIO_I2C_QUEUE];
	struct virtio_device *vdev = vi2cdev->vdev;
	struct msg_info msg_informaiton;
	__u8 *msg_buffer;

	vmm_printf("---- %s: name=%s\n", __func__,vi2cdev->name);

	vmm_printf("---- %s: i2c_transfer: call  \n",__func__);
	int ret = i2c_transfer(vi2cdev->adapter, &vi2cdev->msg, 1);
	if (ret < 1) vmm_printf("---- %s: i2c_transfer: ERROR return=%d \n",__func__, ret);
	vmm_printf("---- %s: i2c_transfer: done  \n",__func__);

	/* print msgs recv */
	vmm_printf("---- %s: msg_inf: addr=0x%016x | flags=0x%016x | len=0x%016x\n",__func__, vi2cdev->msg.addr, vi2cdev->msg.flags, vi2cdev->msg.len);
	for (i=0;i<vi2cdev->msg.len;i++)
		vmm_printf("---- %s: buf[%d] = 0x%08x\n",__func__, i, vi2cdev->msg.buf[i]);

	/* transfer new val */
	msg_informaiton.addr = vi2cdev->msg.addr;
	msg_informaiton.flags = vi2cdev->msg.flags;
	msg_informaiton.len = vi2cdev->msg.len;
	msg_buffer = vmm_zalloc(sizeof(__u8)*vi2cdev->msg.len);
	for(i=0; i < vi2cdev->msg.len; i++)
		msg_buffer[i] = vi2cdev->msg.buf[i];

	vmm_printf("---- %s: virtio_queue_available ?\n",__func__);
	if (virtio_queue_available(vq)) {
		vmm_printf("---- %s: virtio_queue_available OK\n",__func__);
		head = virtio_queue_get_iovec(vq, iov, &iov_cnt, &total_len);
		if (iov_cnt) {
			/* print msgs recv */
			print_msgs(__func__, &vi2cdev->msg, 1);

			/* print msgs recv */
			print_msgs(__func__, &vi2cdev->msg, 1);


			/* write */
			virtio_buf_to_iovec_write(vdev, &iov[0], 1, &msg_informaiton, sizeof(msg_informaiton));
			virtio_buf_to_iovec_write(vdev, &iov[1], 1, &msg_buffer, sizeof(msg_buffer));

			virtio_queue_set_used_elem(vq, head, total_len);

			vmm_printf("---- %s: signal \n",__func__);
			if (virtio_queue_should_signal(vq)) {
				vdev->tra->notify(vdev, VIRTIO_I2C_QUEUE);
			}
		}
	}
	vmm_printf("---- %s: end \n",__func__);

}


static int i2c_recv_msgs(struct virtio_device *dev)
{
	vmm_printf("---- %s: \n", __func__);
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	struct virtio_queue *vq = &vi2cdev->vqs[VIRTIO_I2C_QUEUE];
	u8 buf[8];
	u16 head = 0;
	u32 i, len, iov_cnt = 0, total_len = 0;
	struct virtio_iovec *iov = vi2cdev->in_iov;
	struct msg_info msg_informaiton;
	__u8 *msg_buffer;

	vmm_printf("---- %s: name=%s\n", __func__,vi2cdev->name);
	/*---------*/
/*	if (!vq || !vq->addr || !vq->vring.avail) {
		vmm_printf("---- %s: ERROR available !\n",__func__);
	}

	vring_avail_event(&vq->vring) = vq->last_avail_idx;
	vmm_printf("---- %s: A=%d | B=%d \n",__func__,vring_avail_event(&vq->vring),vq->last_avail_idx);

	vq->vring.avail->idx !=  vq->last_avail_idx;
	vmm_printf("---- %s: C=%d | D=%d\n",__func__, vq->vring.avail->idx,vq->last_avail_idx);
*/	/*---------*/

	/* recept data */
	while (virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, iov, &iov_cnt, &total_len);
		vmm_printf("---- %s: iov_cnt=%d | total_len=%d | head=%d\n", __func__, iov_cnt, total_len,head);
		for (i = 0; i < iov_cnt; i+=2) {
			virtio_iovec_to_buf_read(vi2cdev->vdev, &iov[i], 1,
				&msg_informaiton, sizeof(msg_informaiton));
			virtio_iovec_to_buf_read(vi2cdev->vdev, &iov[i+1], 1,
				&msg_buffer, sizeof(msg_buffer));
		}
		virtio_queue_set_used_elem(vq, head, total_len);
	}


	/* make struct msg */
	vmm_printf("---- %s: make struct msg\n", __func__);
	vi2cdev->msg.addr = msg_informaiton.addr;
	vi2cdev->msg.flags = msg_informaiton.flags;
	vi2cdev->msg.len = msg_informaiton.len;
	for(i=0; i < msg_informaiton.len; i++)
		vi2cdev->msg.buf[i] = msg_buffer[i];


	vmm_printf("---- %s: vmm_manager_guest_request\n", __func__);
	vmm_manager_guest_request(dev->guest, orphan_i2c_transfer, vi2cdev);

	return VMM_OK;
}
/*************************************************/
/********************* OPS  **********************/
/*************************************************/

/* VirtIO operations */

static u32 virtio_i2c_get_host_features(struct virtio_device *dev)
{
	vmm_printf("---- %s: \n", __func__);
	return 0;
}

static void virtio_i2c_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	vmm_printf("---- %s: features=%u \n", __func__,features);
	/* No host features so, ignore it. */
}

static int virtio_i2c_init_vq(struct virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	vmm_printf("---- %s: \n", __func__);
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
	vmm_printf("---- %s: \n", __func__);
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
	vmm_printf("---- %s: \n", __func__);
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
	vmm_printf("---- %s: \n", __func__);
	/* FIXME: dynamic */
	return size;
}


static int virtio_i2c_notify_vq(struct virtio_device *dev, u32 vq)
{
	vmm_printf("---- %s: \n", __func__);

	int rc = VMM_OK;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		vmm_printf("---- %s: VIRTIO_I2C_QUEUE: vq=%u\n", __func__,vq);
		rc = i2c_recv_msgs(dev);
		break;
	default:
		vmm_printf("---- %s: default: vq=%u\n", __func__,vq);
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

/********************************/
/* Emulator operations */

static int virtio_i2c_read_config(struct virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	vmm_printf("---- %s: not implemented\n", __func__);
	return 0;
}

static int virtio_i2c_write_config(struct virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	vmm_printf("---- %s: not implemented\n", __func__);
	return 0;
}

static int virtio_i2c_reset(struct virtio_device *dev)
{
	vmm_printf("---- %s: \n", __func__);
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
	vmm_printf("---- %s: \n", __func__);

	struct virtio_i2c_dev *i2cdev;

	i2cdev = vmm_zalloc(sizeof(struct virtio_i2c_dev));
	if (!i2cdev) {
		vmm_printf("Failed to allocate virtio i2c device....\n");
		return VMM_ENOMEM;
	}
	i2cdev->vdev = dev;

	vmm_snprintf(i2cdev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);
	vmm_printf("---- %s: name=%s\n", __func__, i2cdev->name);

	dev->emu_data = i2cdev;

	i2cdev->msg.buf = vmm_zalloc(sizeof(__u8)*VIRTIO_I2C_QUEUE_SIZE);

	/* Get adapters */
	i2cdev->adapter = NULL;
	i2c_for_each_dev(dev, i2cimx_attach_adapter);

	rt_mutex_init(&i2cdev->adapter->bus_lock);

	return VMM_OK;
}

static void virtio_i2c_disconnect(struct virtio_device *dev)
{
	vmm_printf("---- %s: \n", __func__);

	struct virtio_i2c_dev *i2cdev = dev->emu_data;

	vmm_free(i2cdev);
}

/*************************/
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
	vmm_printf("---- %s: Xvisor init i2c virtio\n", __func__);
	return virtio_register_emulator(&virtio_i2c);
}

static void __exit virtio_i2c_exit(void)
{
	vmm_printf("---- %s: \n", __func__);
	virtio_unregister_emulator(&virtio_i2c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
