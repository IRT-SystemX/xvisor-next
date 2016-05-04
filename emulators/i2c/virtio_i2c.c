

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



//#define DEBUG_VIRTIO_I2C

#ifdef DEBUG_VIRTIO_I2C
#define DEBUG_PRINT(fmt, args...) vmm_printf(fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif


/********************************/
struct virtio_i2c_dev {
	struct virtio_device *vdev;

	struct virtio_queue vqs[VIRTIO_I2C_NUM_QUEUES];
	struct virtio_iovec iov[VIRTIO_I2C_QUEUE_SIZE];
//	u32 features;

	char name[VIRTIO_DEVICE_MAX_NAME_LEN];

	struct i2c_adapter *adapter;
	struct i2c_msg msg;
};

/*****************************************************************************/

/*************************************************/
/********************* FONC **********************/
/*************************************************/

static void print_msgs(const char * func, struct i2c_msg *msgs, int num)
{
	int i;

	for (i=0; i<num; i++)
	{
		DEBUG_PRINT("---- %s: msgs: addr=0x%016x | flags=0x%016x | len=0x%016x\n",
				func, msgs->addr, msgs->flags, msgs->len);
		for (i=0;i<msgs->len;i++)
			DEBUG_PRINT("---- %s: msgs->buf[%d] = 0x%08x\n",
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

	vmm_printf("---- %s: [%s] adapter %s is find \n",__func__, i2cdev->name, adap->name);
//const char *strstr(const char *string, const char *substring)
	if (	strcmp(adap->name, "i2c@021a0000") == 0 &&
		strcmp(i2cdev->name, "guest0/virtio-i2c0@021c4000")==0) {
			i2cdev->adapter = adap;
			vmm_printf("---- %s: adapter %s is set \n",__func__, adap->name);
		}
	else if (strcmp(adap->name, "i2c@021a4000") == 0 &&
		strcmp(i2cdev->name, "guest0/virtio-i2c1@021c8000")==0) {
		i2cdev->adapter = adap;
		vmm_printf("---- %s: adapter %s is set \n",__func__, adap->name);
	}
	else if (strcmp(adap->name, "i2c@021a8000") == 0 &&
		strcmp(i2cdev->name, "guest0/virtio-i2c2@021cc000")==0) {
		i2cdev->adapter = adap;
		vmm_printf("---- %s: adapter %s is set \n",__func__, adap->name);
	}

	return VMM_OK;
}

static void orphan_i2c_transfer(struct vmm_guest * guest, void *param){
        DEBUG_PRINT("=>|orphan_i2c_transfer|\n");

	int ret;
        struct virtio_i2c_dev *vi2cdev = param;
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0, len_write = 0, len_read = 0;;
	struct virtio_queue *vq = &vi2cdev->vqs[VIRTIO_I2C_QUEUE];
	struct virtio_device *vdev = vi2cdev->vdev;

//	DEBUG_PRINT("---- %s: name=%s\n", __func__,vi2cdev->name);
//	DEBUG_PRINT("---- %s: adapter.name=%s \n",__func__, vi2cdev->adapter->name);


	/* virtio queue available */
	if (virtio_queue_available(vq)) {

		head = virtio_queue_get_iovec(vq, vi2cdev->iov, &iov_cnt, &total_len);
//		DEBUG_PRINT("---- %s: iov_cnt=%d | total_len=%d | head=%d\n", __func__, iov_cnt, total_len,head);

		if (iov_cnt) {

			/* READ ADDR */
			len_read += virtio_iovec_to_buf_read(vi2cdev->vdev, &vi2cdev->iov[0], 1,
					&vi2cdev->msg.addr, sizeof(vi2cdev->msg.addr));

			/* READ FLAGS */
			len_read += virtio_iovec_to_buf_read(vi2cdev->vdev, &vi2cdev->iov[1], 1,
					&vi2cdev->msg.flags, sizeof(vi2cdev->msg.flags));

			if (vi2cdev->msg.flags & I2C_M_RD)
			{
				DEBUG_PRINT("---- %s: Read\n", __func__);
				/* do read sur len & buf dans le func orphan */
			}
			else
			{
				/* READ LEN */
				DEBUG_PRINT("---- %s: Write\n", __func__);
				len_read += virtio_iovec_to_buf_read(vi2cdev->vdev, &vi2cdev->iov[2], 1,
						&vi2cdev->msg.len, sizeof(vi2cdev->msg.len));

				/* READ BUF */
				len_read += virtio_iovec_to_buf_read(vi2cdev->vdev, &vi2cdev->iov[3], 1,
						vi2cdev->msg.buf, sizeof(vi2cdev->msg.buf));	
			}

			/* total lut */
//			DEBUG_PRINT("---- %s: len_read=%d | calcul_len_r=%d | calcul_len_w=%d\n",
//			 __func__,
//			 len_read,
//			 (sizeof(vi2cdev->msg.addr) + sizeof(vi2cdev->msg.flags)
//			 + sizeof(vi2cdev->msg.len) + sizeof(vi2cdev->msg.buf)),
//			 (sizeof(int)) );

			/* print msgs recv */
			print_msgs(__func__, &vi2cdev->msg, 1);

			/* send message to hardware */
//			DEBUG_PRINT("---- %s: before guest_request\n", __func__);
//			print_msgs(__func__, &vi2cdev->msg, 1);

			/* ---------- i2c transfer ------------ */
			DEBUG_PRINT("---- %s: i2c_transfer: call  \n",__func__);
			ret = i2c_transfer(vi2cdev->adapter, &vi2cdev->msg, 1);
			if (ret < 1)
				DEBUG_PRINT("---- %s: i2c_transfer: ERROR return=%d \n",__func__, ret);
			DEBUG_PRINT("---- %s: i2c_transfer: done  \n",__func__);

			/* read */
			if (vi2cdev->msg.flags & I2C_M_RD)
			{
				/* TODO: supprimer cette partie de test */
//				if(ret < 1)
//				{
//					vi2cdev->msg.len = 3;
//					vi2cdev->msg.buf[0] = 1;
//					vi2cdev->msg.buf[1] = 2;
//					vi2cdev->msg.buf[2] = 3;
//				}

				DEBUG_PRINT("---- %s: Read\n", __func__);
				/* WRITE LEN */
				len_write += virtio_buf_to_iovec_write(vdev, &vi2cdev->iov[2], 1, &vi2cdev->msg.len, sizeof(vi2cdev->msg.len));
				/* WRITE BUF */
				len_write += virtio_buf_to_iovec_write(vdev, &vi2cdev->iov[3], 1, vi2cdev->msg.buf, sizeof(vi2cdev->msg.buf));
			}

			/* print msgs recv */
			print_msgs(__func__, &vi2cdev->msg, 1);

//			ret = 12;
			/* WRITE RET */
			len_write += virtio_buf_to_iovec_write(vdev, &vi2cdev->iov[4], 1, &ret, sizeof(ret));

//			DEBUG_PRINT("---- %s: len_write=%d | calcul_len_w=%d | calcul_len_r=%d \n",
//				 __func__,
//				 len_write,
//				 sizeof(ret),
//				 (sizeof(ret) + sizeof(vi2cdev->msg.buf) + sizeof(vi2cdev->msg.addr)) );

//			DEBUG_PRINT("---- %s: len_read=%d | len_write=%d | len_total=%d \n",
//				 __func__,
//				 len_read,
//				 len_write,
//				 len_read + len_write );

			virtio_queue_set_used_elem(vq, head, len_read + len_write);

			/* signal */
//			DEBUG_PRINT("---- %s: signal \n",__func__);
			if (virtio_queue_should_signal(vq)) {
				vdev->tra->notify(vdev, VIRTIO_I2C_QUEUE);
			}
		}


	}

	DEBUG_PRINT("<=|orphan_i2c_transfer|\n");
}


static int i2c_recv_msgs(struct virtio_device *dev)
{
	DEBUG_PRINT("---- %s: \n", __func__);
	struct virtio_i2c_dev *vi2cdev = dev->emu_data;

	struct virtio_queue *vq = &vi2cdev->vqs[VIRTIO_I2C_QUEUE];

//	DEBUG_PRINT("---- %s: name=%s\n", __func__,vi2cdev->name);
//	DEBUG_PRINT("---- %s: vq->vring.avail->idx=%d | vq->last_avail_idx=%d\n", __func__,vq->vring.avail->idx,vq->last_avail_idx);


//	DEBUG_PRINT("---- %s: adapter.name=%s \n",__func__, vi2cdev->adapter->name);

//	DEBUG_PRINT("---- %s: vmm_manager_guest_request\n", __func__);
	vmm_manager_guest_request(dev->guest, orphan_i2c_transfer, vi2cdev);

	return VMM_OK;
}
/*************************************************/
/********************* OPS  **********************/
/*************************************************/

/* VirtIO operations */

static u32 virtio_i2c_get_host_features(struct virtio_device *dev)
{
	DEBUG_PRINT("---- %s: \n", __func__);
	return 0;
}

static void virtio_i2c_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	DEBUG_PRINT("---- %s: features=%u \n", __func__,features);
	/* No host features so, ignore it. */
}

static int virtio_i2c_init_vq(struct virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	DEBUG_PRINT("---- %s: \n", __func__);
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
	DEBUG_PRINT("---- %s: \n", __func__);
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
	DEBUG_PRINT("---- %s: \n", __func__);
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
	DEBUG_PRINT("---- %s: \n", __func__);
	/* FIXME: dynamic */
	return size;
}


static int virtio_i2c_notify_vq(struct virtio_device *dev, u32 vq)
{
	DEBUG_PRINT("---- %s: \n", __func__);

	int rc = VMM_OK;

	switch (vq) {
	case VIRTIO_I2C_QUEUE:
		DEBUG_PRINT("---- %s: VIRTIO_I2C_QUEUE: vq=%u\n", __func__,vq);
		rc = i2c_recv_msgs(dev);
		break;
	default:
		DEBUG_PRINT("---- %s: default: vq=%u\n", __func__,vq);
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}
/*****************************************************************************/
/* Emulator operations */

static int virtio_i2c_read_config(struct virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	DEBUG_PRINT("---- %s: not implemented\n", __func__);
	return 0;
}

static int virtio_i2c_write_config(struct virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	DEBUG_PRINT("---- %s: not implemented\n", __func__);
	return 0;
}

static int virtio_i2c_reset(struct virtio_device *dev)
{
	DEBUG_PRINT("---- %s: \n", __func__);
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
	DEBUG_PRINT("---- %s: \n", __func__);

	struct virtio_i2c_dev *i2cdev;

	i2cdev = vmm_zalloc(sizeof(struct virtio_i2c_dev));
	if (!i2cdev) {
		DEBUG_PRINT("Failed to allocate virtio i2c device....\n");
		return VMM_ENOMEM;
	}
	i2cdev->vdev = dev;

	vmm_snprintf(i2cdev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);
	DEBUG_PRINT("---- %s: name=%s\n", __func__, i2cdev->name);

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
	DEBUG_PRINT("---- %s: \n", __func__);

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
	DEBUG_PRINT("---- %s: Xvisor init i2c virtio\n", __func__);
	return virtio_register_emulator(&virtio_i2c);
}

static void __exit virtio_i2c_exit(void)
{
	DEBUG_PRINT("---- %s: \n", __func__);
	virtio_unregister_emulator(&virtio_i2c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
