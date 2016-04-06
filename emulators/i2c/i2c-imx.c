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
 * @file i2c_imx.c
 * @author Mickaël TANSORIER (mickael.tansorier@openwide.fr)
 * @brief i2c emulator.
 * @details This source file implements the i2c emulator.
 *
 */


/***** includes *****/
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <libs/mathlib.h>
#include <linux/i2c.h>
#include <vmm_threads.h>

/***** define ******/
#define MODULE_DESC			"I2C emulator"
#define MODULE_AUTHOR			"Mickaël Tansorier"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			i2c_imx_emulator_init
#define	MODULE_EXIT			i2c_imx_emulator_exit


/***** define register ******/
#define IMX_I2C_IADR	0x00	/* i2c slave address */
#define IMX_I2C_IFDR	0x01	/* i2c frequency divider */
#define IMX_I2C_I2CR	0x02	/* i2c control */
#define IMX_I2C_I2SR	0x03	/* i2c status */
#define IMX_I2C_I2DR	0x04	/* i2c transfer data */

#define IMX_I2C_REGSHIFT	2

/* Bits of IMX I2C registers */
#define I2CR_RSTA	(1 << 2)
#define I2CR_TXAK	(1 << 3)
#define I2CR_MTX	(1 << 4)
#define I2CR_MSTA	(1 << 5)
#define I2CR_IIEN	(1 << 6)
#define I2CR_IEN	(1 << 7)

#define I2SR_RXAK	(1 << 0)
#define I2SR_IIF	(1 << 1)
#define I2SR_SRW	(1 << 2)
#define I2SR_IAL	(1 << 4)
#define I2SR_IBB	(1 << 5)
#define I2SR_IAAS	(1 << 6)
#define I2SR_ICF	(1 << 7)

/* mask write */
#define IADR_WR_MASK	0xFE			/* 0000 0000 1111 1110 */
#define IFDR_WR_MASK	0x3F			/* 0000 0000 0011 1111 */
#define I2CR_WR_MASK	( I2CR_RSTA | I2CR_TXAK | I2CR_MTX | I2CR_MSTA | \
			I2CR_IIEN | I2CR_IEN )	/* 0000 0000 1111 1100 */
#define I2SR_WR_MASK	( I2SR_IIF | I2SR_IAL )	/* 0000 0000 0001 0010 */
#define I2DR_WR_MASK	0xFF			/* 0000 0000 1111 1111 */

/* mask read */
#define IADR_RD_MASK	0xFFFF			/* 1111 1111 1111 1110 */
#define IFDR_RD_MASK	0xFFFF			/* 1111 1111 1111 1111 */
#define I2CR_RD_MASK	0xFFFB			/* 1111 1111 1111 1011 */
#define I2SR_RD_MASK	0xFFFF			/* 1111 1111 1111 1111 */
#define I2DR_RD_MASK	0xFFFF			/* 1111 1111 1111 1111 */


/**************** state *****************/
struct i2c_imx_state {
	struct vmm_guest *guest;
	struct vmm_emudev *edev;
	vmm_spinlock_t lock;
	u32 irq;
	int irq_level;

	u32 i2c_IADR;	/* i2c slave address */
	u32 i2c_IFDR;	/* i2c frequency divider */
	u32 i2c_I2CR;	/* i2c control */
	u32 i2c_I2SR;	/* i2c status */
	u32 i2c_I2DR;	/* i2c transfer data */

	bool slave_addr_request;
	u32 slave_addr;

	bool data_request;
	u32 data;

	int num; //nb data trans; start set 0, i2dr num++; stop call fonc
	u32 tab_data[256];

	struct i2c_adapter *adapter;

	u8 buf[256];
	int nb_buf;
	struct i2c_msg msg;

	u32 * ret_val;

//	struct vmm_thread *thread;
};



/*************************************************/
/********************* FONC **********************/
/*************************************************/

/* @dummy : struct vmm_emudev *edev */
static int i2cimx_attach_adapter(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
	struct vmm_emudev *edev;
	struct i2c_imx_state *i2c_imx;

	/* get adapter */
	if (dev->type != &i2c_adapter_type)
		return VMM_EINVALID;
	adap = to_i2c_adapter(dev);

	/* set right adapter to i2c_imx_state */
	edev = dummy;
	i2c_imx = edev->priv;

	if (strcmp(adap->name, edev->node->name) == 0)
	{
		i2c_imx->adapter = adap;
		/* __DEBUG__ */ vmm_printf("**** %s: adapter is set \n",__func__);
	}

	return VMM_OK;
}
/* ------------------------------------------------------------------------- */

static void call_read(struct vmm_guest * guest, void *param){
        vmm_printf("=>|call_read|\n");
        struct i2c_imx_state *i2c_imx = param;

	struct i2c_msg msgs[2];
	u8 buf[1];

	msgs[0].addr = i2c_imx->msg.addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = i2c_imx->msg.buf;
	msgs[1].addr = i2c_imx->msg.addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = buf;
	/* __DEBUG__ */ vmm_printf("**** %s: i2c_transfer: call read \n",__func__);
	int ret = i2c_transfer(i2c_imx->adapter, msgs, 2);
	/* __DEBUG__ */ vmm_printf("**** %s: i2c_transfer: done read \n",__func__);

	if (ret == i2c_imx->num) {
		i2c_imx->ret_val = (u32)msgs[1].buf; // *val = (buf[0] << 8) | buf[1];
		//return 0;
	}
        //return VMM_OK;
//	vmm_manager_guest_resume(i2c_imx->guest);
}

static void call_write(struct vmm_guest * guest, void *param){
	vmm_printf("=>|call_write|\n");
	struct i2c_imx_state *i2c_imx = param;

	struct i2c_msg msgs[1];
	u8 buf[1];

	buf[0] = i2c_imx->data; //tab_data[0];
	msgs[0].addr = i2c_imx->slave_addr >> 1;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;

	/* __DEBUG__ */ vmm_printf("**** %s: i2c_transfer: call write\n",__func__);
	i2c_transfer(i2c_imx->adapter, msgs, 1);
	/* __DEBUG__ */ vmm_printf("**** %s: i2c_transfer: done write \n",__func__);

//	vmm_manager_guest_resume(i2c_imx->guest);
}

/***************************/
//	__u16 flags;
//#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
//#define I2C_M_RD		0x0001	/* read data, from slave to master */
//#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
//#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
//#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
//#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
//#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
//#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */

/****************************************/
static int call_i2c_transfer_read(struct i2c_imx_state *i2c_imx, u32 *val)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);


	/* __DEBUG__ */ vmm_printf("**** %s: vmm_manager call read\n",__func__);
	vmm_manager_guest_request(i2c_imx->guest, call_read, i2c_imx);
	/* __DEBUG__ */ vmm_printf("**** %s: vmm_manager done read\n",__func__);

//	vmm_manager_guest_pause(i2c_imx->guest);
	if (i2c_imx->num != 2) {
		*val = i2c_imx->ret_val;
		//*val = (buf[0] << 8) | buf[1];
		return 0;
	}
	return VMM_OK;
}


static int call_i2c_transfer_write(struct i2c_imx_state *i2c_imx)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);

	/* __DEBUG__ */ vmm_printf("**** %s: vmm_manager call write\n",__func__);
	vmm_manager_guest_request(i2c_imx->guest, call_write, i2c_imx);
	/* __DEBUG__ */ vmm_printf("**** %s: vmm_manager done write\n",__func__);

//	vmm_manager_guest_pause(i2c_imx->guest);
}



/*************************************************/
/********************* READ **********************/
/*************************************************/

static int i2c_imx_reg_read(struct i2c_imx_state *i2c_imx,
			  u32 offset,
			  u32 *dst)
{
	int ret = VMM_OK;

	vmm_spin_lock(&i2c_imx->lock);

	/* lower the interrupt level */
	vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0);

	switch (offset >> IMX_I2C_REGSHIFT) {
	case IMX_I2C_IADR:
		*dst = (i2c_imx->i2c_IADR & IADR_RD_MASK);
		break;
	
	case IMX_I2C_IFDR:
		*dst = (i2c_imx->i2c_IFDR & IFDR_RD_MASK);
		break;

	case IMX_I2C_I2CR:
		*dst = (i2c_imx->i2c_I2CR & I2CR_RD_MASK);
		break;

	case IMX_I2C_I2SR:
		*dst = (i2c_imx->i2c_I2SR & I2SR_RD_MASK);
		break;

	case IMX_I2C_I2DR:
		if (i2c_imx->data_request)
		{
			i2c_imx->msg.flags = I2C_M_RD;
			//i2c_imx->nb_buf++;
			call_i2c_transfer_read(i2c_imx, dst);
		}

		/* Set IRQ */
		if (i2c_imx->irq_level)
		{
			i2c_imx->i2c_I2SR |= I2SR_IIF;
			/* deux read sont normalement appeler dans le fonction xfer, du coup au premier read
			 * aller faire la demande au hard de la valeur pour la donner au dexième read */
			/* mettre le ACK IRQ en mode read dans la fonction call_read, car cela permet de
			 * gerer un ordonnancement avec le "thread de vmm_manager_guest_request */
			vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 1);
		}
		else
		{
			i2c_imx->i2c_I2SR |= I2SR_IIF;
		}
		//*dst = (i2c_imx->i2c_I2DR & I2DR_RD_MASK);
		*dst = *(i2c_imx->ret_val);
		/* __DEBUG__ */ vmm_printf("**** %s: read val=0x%08x \n",__func__,*dst);
		break;

	default:
		ret = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&i2c_imx->lock);

	return ret;
}

static int i2c_imx_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	u32 val = 0;
	int ret = VMM_OK;

	ret = i2c_imx_reg_read(edev->priv, offset, &val);
	if (VMM_OK == ret) {
		*dst = val & 0xFF;
	}

	return ret;
}

static int i2c_imx_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	u32 val = 0;
	int ret = VMM_OK;

	ret = i2c_imx_reg_read(edev->priv, offset, &val);
	if (VMM_OK == ret) {
		*dst = val & 0xFFFF;
	}

	return ret;
}

static int i2c_imx_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	return i2c_imx_reg_read(edev->priv, offset, dst);
}







/*************************************************/
/********************* WRITE *********************/
/*************************************************/
static int i2c_imx_reg_write(struct i2c_imx_state *i2c_imx,
				u32 offset,
				u32 mask,
			   	u32 val)
{
	int ret = VMM_OK; 
	u32 prev_I2CR;
	vmm_spin_lock(&i2c_imx->lock);

	/* lower the interrupt level */
	vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0);

	switch (offset >> IMX_I2C_REGSHIFT) {
	case IMX_I2C_IADR: /* i2c slave address */ 
		i2c_imx->i2c_IADR = (i2c_imx->i2c_IADR & ~mask) | \
				    (val & mask & IADR_WR_MASK);
		break;

	case IMX_I2C_IFDR: /* i2c frequency divider */
		i2c_imx->i2c_IFDR = (i2c_imx->i2c_IFDR & ~mask) | \
				    (val & mask & IFDR_WR_MASK);
		/* eventuellement mettre un warning si la valeur écrite est différente
		 * de celle présente en hardware. Car elle est censé être en dur dans le dts */
		break;

	case IMX_I2C_I2CR: /* i2c control */
		prev_I2CR = i2c_imx->i2c_I2CR;
		i2c_imx->i2c_I2CR = (i2c_imx->i2c_I2CR & ~mask) | \
				    (val & mask & I2CR_WR_MASK);
		vmm_printf("**** %s: i2c_I2CR=0x%08x\n", __func__, i2c_imx->i2c_I2CR);

		/* Si changement dans MSTA pour définir slave mode, mettre warning expliquant qu'on
		 * ne gere pas le mode slave. Mais seulement le mode master */

		/* Change MSTA register */
		if ((i2c_imx->i2c_I2CR & I2CR_MSTA)!=(prev_I2CR & I2CR_MSTA)){
			if (i2c_imx->i2c_I2CR & I2CR_MSTA)
			{
				vmm_printf("**** %s: |START| \n", __func__);
				i2c_imx->slave_addr_request = true;
				i2c_imx->data_request = false;
				i2c_imx->i2c_I2SR |= I2SR_IBB;
				/* clear msg */
				i2c_imx->nb_buf = 0;
			}
			else
			{
				vmm_printf("**** %s: |STOP| \n", __func__);
				i2c_imx->slave_addr_request = false;
				i2c_imx->data_request = false;
				i2c_imx->i2c_I2SR &= ~I2SR_IBB;
				/* if R/W=0 */
				if (!(i2c_imx->i2c_I2SR & I2SR_SRW))
					call_i2c_transfer_write(i2c_imx);
				vmm_printf("**** %s: |SEND msg [%d]| \n", __func__, i2c_imx->nb_buf);
			}
		}

		/* if Transmit mode */
		if(i2c_imx->i2c_I2CR & I2CR_MTX)
		{
			i2c_imx->i2c_I2SR |= I2SR_SRW;
		}
		else
		{
			i2c_imx->i2c_I2SR &= ~I2SR_SRW;
		}

		/* repeat start */
		if(i2c_imx->i2c_I2CR & I2CR_RSTA)
		{
			vmm_printf("**** %s: |REPEAT START| \n", __func__);
			i2c_imx->slave_addr_request = true;
			i2c_imx->data_request = false;
			i2c_imx->num = 0;
			vmm_printf("**** %s: |SEND msg [%d]| \n", __func__, i2c_imx->nb_buf);
			/* if R/W=0 */
			if (!(i2c_imx->i2c_I2SR & I2SR_SRW))
				call_i2c_transfer_write(i2c_imx);
			/* clear msg */
			i2c_imx->nb_buf = 0;
		}

		/* IEN: if I2C enable bit is set to 0 */
		if ( !(i2c_imx->i2c_I2CR & I2CR_IEN) )
		{
//			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_I2CR: reset request \n", __func__);
			i2c_imx->i2c_I2CR = 0x00000000;
			i2c_imx->i2c_I2SR = 0x00000000;
			i2c_imx->i2c_I2DR = 0x00000000;

			i2c_imx->slave_addr_request = false;
			i2c_imx->slave_addr = 0x00000000;
			i2c_imx->data_request = false;
			i2c_imx->data = 0x00000000;

			i2c_imx->irq_level = 0;
			vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq,0);
		}

		/* IIEN */
		if (i2c_imx->i2c_I2CR & I2CR_IIEN)
		{
			/* Enabling IRQ */
			i2c_imx->irq_level = 1;
		}
		else
		{
			/* Disabling IRQ */
			i2c_imx->irq_level = 0;
		}


		/* MSTA: _/ set start signal: master | \_ set stop signal: slave */
		/* MTX:  0: receive | 1: transmit */
		/* TXAK */
		/* RSTA */
		break;

	case IMX_I2C_I2SR: /* i2c status */
		i2c_imx->i2c_I2SR = (i2c_imx->i2c_I2SR & \
				    (~mask | ~I2SR_WR_MASK) )\
				     | (val & mask & I2SR_WR_MASK);
		vmm_printf("**** %s: i2c_I2SR=0x%08x\n", __func__, i2c_imx->i2c_I2SR);
		/* IAL */
		/* IIF */
		break;

	case IMX_I2C_I2DR: /* i2c transfer data */
		/* __DEBUG__ */ vmm_printf("**** %s: Write val=0x%08x \n", \
					__func__, val & mask & I2DR_WR_MASK);
		i2c_imx->i2c_I2DR = (i2c_imx->i2c_I2DR & ~mask) | \
				    (val & mask & I2DR_WR_MASK);

		/* Recept slave addr */
		if (i2c_imx->slave_addr_request)
		{
	/* if 0x1 => read */
			i2c_imx->slave_addr_request = false;
			i2c_imx->data_request = true;
			i2c_imx->slave_addr = (val & mask & I2DR_WR_MASK);
			i2c_imx->msg.addr = (val & mask & I2DR_WR_MASK) >> 1;
			i2c_imx->nb_buf=0;
			i2c_imx->msg.len = 0;
			i2c_imx->msg.buf = i2c_imx->buf;
			/* __DEBUG__ */ vmm_printf("**** %s: slave_addr=0x%08x \n",__func__,i2c_imx->slave_addr);
		}
		else if (i2c_imx->data_request)
		{
			i2c_imx->tab_data[i2c_imx->num] = (val & mask & I2DR_WR_MASK);
			(i2c_imx->num)++;
			i2c_imx->data = (val & mask & I2DR_WR_MASK);

			i2c_imx->buf[i2c_imx->nb_buf] = (val & mask & I2DR_WR_MASK);

			i2c_imx->nb_buf++;
			if ( i2c_imx->nb_buf == 0) /* command */
			{
				vmm_printf("**** %s: command=0x%08x \n",__func__,i2c_imx->data);
			}
			else /* write data */
			{
				i2c_imx->msg.flags = 0;
				vmm_printf("**** %s: data=0x%08x \n",__func__,i2c_imx->data);	
			}
		}

		/* Set IRQ */
		if (i2c_imx->irq_level)
		{
			i2c_imx->i2c_I2SR |= I2SR_IIF;
			vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 1);
		}
		else
		{
			i2c_imx->i2c_I2SR |= I2SR_IIF;
		}
		break;

	default:
		ret = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&i2c_imx->lock);

	return ret;
}

static int i2c_imx_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	return i2c_imx_reg_write(edev->priv, offset, 0x000000FF, src);
}

static int i2c_imx_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	return i2c_imx_reg_write(edev->priv, offset, 0x0000FFFF, src);
}

static int i2c_imx_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	return i2c_imx_reg_write(edev->priv, offset, 0xFFFFFFFF, src);
}



/*************************************************/
/********************* RESET *********************/
/*************************************************/

static int i2c_imx_emulator_reset(struct vmm_emudev *edev)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
	struct i2c_imx_state *i2c_imx = edev->priv;

	vmm_spin_lock(&i2c_imx->lock);

	i2c_imx->i2c_I2CR = 0x00000000;
	i2c_imx->i2c_I2SR = 0x00000000;
	i2c_imx->i2c_I2DR = 0x00000000;

	i2c_imx->slave_addr_request = false;
	i2c_imx->slave_addr = 0x00000000;
	i2c_imx->data_request = false;
	i2c_imx->data = 0x00000000;

	i2c_imx->irq_level = 0;
	vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0);

	vmm_spin_unlock(&i2c_imx->lock);

	return VMM_OK;
}


/*************************************************/
/****************** PROB REMOVE ******************/
/*************************************************/

static int i2c_imx_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
	
	int rc;
	struct i2c_imx_state *i2c_imx = NULL;

	/* allocated structure */
	i2c_imx = vmm_zalloc(sizeof(struct i2c_imx_state));
	if (!i2c_imx) {
		return VMM_EFAIL;
	}

	i2c_imx->edev = edev;
	i2c_imx->guest = guest;

	i2c_imx->i2c_IADR = 0x00000000;
	i2c_imx->i2c_IFDR = 0x00000000;
	i2c_imx->i2c_I2CR = 0x00000000;
	i2c_imx->i2c_I2SR = 0x00000000;
	i2c_imx->i2c_I2DR = 0x00000000;

	i2c_imx->slave_addr_request = false;
	i2c_imx->slave_addr = 0x00000000;
	i2c_imx->data_request = false;
	i2c_imx->data = 0x00000000;

	i2c_imx->num = 0;
	//tab_data[]
	i2c_imx->adapter = NULL;

	/* init irq */
	rc = vmm_devtree_irq_get(edev->node, &i2c_imx->irq, 0);
	if (rc) {
		vmm_free(i2c_imx);
		return rc;
	}
	i2c_imx->irq_level = 0;

	edev->priv = i2c_imx;

	i2c_for_each_dev(edev, i2cimx_attach_adapter);

//	/* __DEBUG__ */ vmm_printf("**** %s: init thread \n",__func__);
//	i2c_imx->thread = vmm_threads_create(edev->node->name,
//                                fonc_thread,
//                                i2c_imx,
//                                VMM_THREAD_MAX_PRIORITY,
//                                VMM_VCPU_DEF_TIME_SLICE);
//
//        if (i2c_imx->thread == NULL) {
//                vmm_free(i2c_imx);
//                return VMM_EFAIL;
//        }
//vmm_threads_start(i2c_imx->thread);
	INIT_SPIN_LOCK(&i2c_imx->lock);


	return VMM_OK;
}

static int i2c_imx_emulator_remove(struct vmm_emudev *edev)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
	struct i2c_imx_state *i2c_imx = edev->priv;

	vmm_free(i2c_imx);
	edev->priv = NULL;

	return VMM_OK;
}


/*************************************************/
/******************* INIT EXIT *******************/
/*************************************************/

static struct vmm_devtree_nodeid i2c_imx_emuid_table[] = {
	{
		.type = "i2c-imx",
		.compatible = "fsl,imx21-i2c",
	},
	{ /* end of list */ },
};

static struct vmm_emulator i2c_imx_emulator = {
	.name = "i2c_imx",
	.match_table = i2c_imx_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = i2c_imx_emulator_probe,
	.read8 = i2c_imx_emulator_read8,
	.write8 = i2c_imx_emulator_write8,
	.read16 = i2c_imx_emulator_read16,
	.write16 = i2c_imx_emulator_write16,
	.read32 = i2c_imx_emulator_read32,
	.write32 = i2c_imx_emulator_write32,
	.reset = i2c_imx_emulator_reset,
	.remove = i2c_imx_emulator_remove,
};

static int __init i2c_imx_emulator_init(void)
{
	/* __DEBUG__ */ vmm_printf("**** init: i2c_imx\n");
	return vmm_devemu_register_emulator(&i2c_imx_emulator);
}

static void __exit i2c_imx_emulator_exit(void)
{
	/* __DEBUG__ */ vmm_printf("**** exit: i2c_imx\n");
	vmm_devemu_unregister_emulator(&i2c_imx_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
