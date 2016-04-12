

#include <vmm_error.h>
//#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>

//#include <vio/vmm_vserial.h>
//#include <libs/fifo.h>
//#include <libs/stringlib.h>

#include <emu/virtio.h>
#include <emu/virtio_i2c.h>

#define MODULE_DESC			"VirtIO i2c Emulator"
#define MODULE_AUTHOR			"Mickaël Tansorier"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_i2c_init
#define MODULE_EXIT			virtio_i2c_exit


/********************************/
struct virtio_i2c_dev {
	struct virtio_device *vdev;

//	struct virtio_queue vqs[VIRTIO_CONSOLE_NUM_QUEUES];
//	struct virtio_iovec rx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
//	struct virtio_iovec tx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
//	struct virtio_i2c_config config;
//	u32 features;

	char name[VIRTIO_DEVICE_MAX_NAME_LEN];
//	struct vmm_vserial *vser;
//	struct fifo *emerg_rd;
};


/********************************/
/* VirtIO operations */

static u32 virtio_i2c_get_host_features(struct virtio_device *dev)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static void virtio_i2c_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	vmm_printf("**** %s: features=%u \n", __func__,features);
	/* No host features so, ignore it. */
}

static int virtio_i2c_init_vq(struct virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_get_pfn_vq(struct virtio_device *dev, u32 vq)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_get_size_vq(struct virtio_device *dev, u32 vq)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_set_size_vq(struct virtio_device *dev, u32 vq, int size)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_notify_vq(struct virtio_device *dev, u32 vq)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

/********************************/
/* Emulator operations */

static int virtio_i2c_read_config(struct virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_write_config(struct virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_reset(struct virtio_device *dev)
{
	vmm_printf("**** %s: \n", __func__);
	return 0;
}

static int virtio_i2c_connect(struct virtio_device *dev, 
				  struct virtio_emulator *emu)
{
	vmm_printf("**** %s: \n", __func__);

	struct virtio_i2c_dev *i2cdev;

	i2cdev = vmm_zalloc(sizeof(struct virtio_i2c_dev));
	if (!i2cdev) {
		vmm_printf("Failed to allocate virtio i2c device....\n");
		return VMM_ENOMEM;
	}
	i2cdev->vdev = dev;

	vmm_snprintf(i2cdev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);
	vmm_printf("**** %s: name=%s\n", __func__, i2cdev->name);

	dev->emu_data = i2cdev;

	return VMM_OK;
}

static void virtio_i2c_disconnect(struct virtio_device *dev)
{
	vmm_printf("**** %s: \n", __func__);

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
	vmm_printf("**** %s: Xvisor init i2c virtio\n", __func__);
	return virtio_register_emulator(&virtio_i2c);
}

static void __exit virtio_i2c_exit(void)
{
	vmm_printf("**** %s: \n", __func__);
	virtio_unregister_emulator(&virtio_i2c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
