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

/***** define ******/
#define MODULE_DESC			"I2C emulator"
#define MODULE_AUTHOR			"Mickaël Tansorier"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			i2c_imx_emulator_init
#define	MODULE_EXIT			i2c_imx_emulator_exit


/***** define register ******/
/* IMX I2C registers:
 * the I2C register offset is different between SoCs,
 * to provid support for all these chips, split the
 * register offset into a fixed base address and a
 * variable shift value, then the full register offset
 * will be calculated by
 * reg_off = ( reg_base_addr << reg_shift)
 */
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

	u32 i2c_IADR;	/* i2c slave address */ 	//u16
	u32 i2c_IFDR;	/* i2c frequency divider */	//u16
	u32 i2c_I2CR;	/* i2c control */
	u32 i2c_I2SR;	/* i2c status */
	u32 i2c_I2DR;	/* i2c transfer data */
};






/*************************************************/
/********************* READ **********************/
/*************************************************/

static int i2c_imx_reg_read(struct i2c_imx_state *i2c_imx,
			  u32 offset,
			  u32 *dst)
{
	/* __DEBUG__ */ vmm_printf("**** %s: offset= 0x%08x \n", __func__, offset >> IMX_I2C_REGSHIFT);
	int ret = VMM_OK;

	vmm_spin_lock(&i2c_imx->lock);

	/* lower the interrupt level */
	vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0);

	switch (offset >> IMX_I2C_REGSHIFT) {
		case IMX_I2C_IADR:
			*dst = (i2c_imx->i2c_IADR & IADR_RD_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: Read i2c_IADR: 0x%08x \n",__func__, *dst);
			break;
	
		case IMX_I2C_IFDR:
			*dst = (i2c_imx->i2c_IFDR & IFDR_RD_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: Read i2c_IFDR: 0x%08x \n",__func__, *dst);
			break;

		case IMX_I2C_I2CR:
			*dst = (i2c_imx->i2c_I2CR & I2CR_RD_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: Read i2c_I2CR: 0x%08x \n",__func__, *dst);
			break;

		case IMX_I2C_I2SR:
			*dst = (i2c_imx->i2c_I2SR & I2SR_RD_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: Read i2c_I2SR: 0x%08x \n",__func__, *dst);
			break;

		case IMX_I2C_I2DR:
			*dst = (i2c_imx->i2c_I2DR & I2DR_RD_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: Read i2c_I2DR: 0x%08x \n",__func__, *dst);
			break;	

		default:
			/* __DEBUG__ */ vmm_printf("**** %s: Read default: 0x%08x \n",__func__, *dst);
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
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
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
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
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
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
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
	/* __DEBUG__ */ vmm_printf("**** %s: offset=0x%08x | mask=0x%08x | val=0x%08x \n",\
					 __func__, offset >> IMX_I2C_REGSHIFT, mask, val); /* __DEBUG__ */
	int ret = VMM_OK; 

	vmm_spin_lock(&i2c_imx->lock);
	/* lower the interrupt level */
	vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0);


	switch (offset >> IMX_I2C_REGSHIFT) {

		case IMX_I2C_IADR: /* i2c slave address */ 
			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_IADR: i2c_IADR=0x%08x | val=0x%08x \n", \
					__func__, (i2c_imx->i2c_IADR & ~mask), (val & mask & IADR_WR_MASK) ); 
			i2c_imx->i2c_IADR = (i2c_imx->i2c_IADR & ~mask) | (val & mask & IADR_WR_MASK);
			break;

		case IMX_I2C_IFDR: /* i2c frequency divider */
			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_IFDR: i2c_IFDR=0x%08x | val=0x%08x \n", \
					__func__, (i2c_imx->i2c_IFDR & ~mask), (val & mask & IFDR_WR_MASK)); 
			i2c_imx->i2c_IFDR = (i2c_imx->i2c_IFDR & ~mask) | (val & mask & IFDR_WR_MASK);
			/* update clock */
			i2c_imx_set_clk(i2c_imx);
			/* start */
			i2c_imx->i2c_I2SR |= I2SR_IBB; //set busy when start
			/* __DEBUG__ */ vmm_printf("**** %s: Start: i2c_I2SR=0x%08x \n", \
					__func__, i2c_imx->i2c_I2SR);
			break;

		case IMX_I2C_I2CR: /* i2c control */
			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_I2CR: i2c_I2CR=0x%08x | val=0x%08x \n", \
					__func__, (i2c_imx->i2c_I2CR & ~mask), (val & mask & I2CR_WR_MASK)); 
			i2c_imx->i2c_I2CR = (i2c_imx->i2c_I2CR & ~mask) | (val & mask & I2CR_WR_MASK);
			if(i2c_imx->i2c_I2CR & (I2CR_IIEN | I2CR_MTX | I2CR_TXAK))
			{
				i2c_imx->i2c_I2SR &= ~I2SR_IBB;
			}
			/* __DEBUG__ */ vmm_printf("**** %s: Start: i2c_I2SR=0x%08x \n", \
					__func__, i2c_imx->i2c_I2SR);
			/* IEN: if I2C enable bit is set to 0 */
			if ( !(i2c_imx->i2c_I2CR & I2CR_IEN) )
			{
				/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_I2CR: reset request \n", __func__);
				i2c_imx->reset_request=1; /* TODO: faire un reset à la fin de transmission du bus,
					 voir page 1887, car ce n'est pas très claire comme fonctionnement */
			}
			/* IIEN */
			if ( (i2c_imx->i2c_I2CR & I2CR_IIEN) )
			{
				/* Enabling IRQ */
				i2c_imx->irq_level = 1;
				vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0); // lower the interrupt level
			}
			else
			{
				/* Disabling IRQ */
				i2c_imx->irq_level = 0;
				vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 0); // lower the interrupt level
			}
			/* MSTA: _/ set start signal: master | \_ set stop signal: slave */
			/* MTX:  0: receive | 1: transmit */
			/* TXAK */
			/* RSTA */
			break;

		case IMX_I2C_I2SR: /* i2c status */
			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_I2SR: i2c_I2SR=0x%08x | val=0x%08x \n", \
					__func__, (i2c_imx->i2c_I2SR & ~mask), (val & mask & I2SR_WR_MASK)); 
			i2c_imx->i2c_I2SR = (i2c_imx->i2c_I2SR & (~mask | ~I2SR_WR_MASK) ) | (val & mask & I2SR_WR_MASK);
			/* __DEBUG__ */ vmm_printf("**** %s: i2c_I2SR=0x%08x\n", \
					__func__, i2c_imx->i2c_I2SR); 
			/* IAL */
			/* IIF */
			break;

		case IMX_I2C_I2DR: /* i2c transfer data */
			/* __DEBUG__ */ vmm_printf("**** %s: Write i2c_I2DR: i2c_I2DR=0x%08x | val=0x%08x \n", \
					__func__, (i2c_imx->i2c_I2DR & ~mask), (val & mask & I2DR_WR_MASK)); 
			i2c_imx->i2c_I2DR = (i2c_imx->i2c_I2DR & ~mask) | (val & mask & I2DR_WR_MASK);
			/* DATA */
			/* Set IRQ */
			if (i2c_imx->irq_level)
			{
				i2c_imx->i2c_I2SR |= I2SR_IIF;
				/* __DEBUG__ */ vmm_printf("**** %s: IIF: i2c_I2SR=0x%08x \n",__func__,i2c_imx->i2c_I2SR);
				vmm_devemu_emulate_irq(i2c_imx->guest, i2c_imx->irq, 1);
			}
			break;

		default:
			/* __DEBUG__ */ vmm_printf("**** %s: Write default: val=0x%08x \n",__func__,(val & mask)); 
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
	/* __DEBUG__ */ 	vmm_printf("**** %s \n",__func__); 
	return i2c_imx_reg_write(edev->priv, offset, 0x000000FF, src);
}

static int i2c_imx_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	/* __DEBUG__ */ vmm_printf("**** %s \n",__func__);
	return i2c_imx_reg_write(edev->priv, offset, 0x0000FFFF, src);
}

static int i2c_imx_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	/* __DEBUG__ */ vmm_printf("**** %s: offset=0x%08x \n",__func__, offset);
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

	i2c_imx->i2c_IADR = 0x00000000;
	i2c_imx->i2c_IFDR = 0x00000000;
	i2c_imx->i2c_I2CR = 0x00000000;
	i2c_imx->i2c_I2SR = 0x00000000;
	i2c_imx->i2c_I2DR = 0x00000000;

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
	
	// init i2c_imx_state
	i2c_imx = vmm_zalloc(sizeof(struct i2c_imx_state));
	if (!i2c_imx) {
		return VMM_EFAIL;
	}

	i2c_imx->edev = edev;

	i2c_imx->guest = guest;
	i2c_imx->i2c_IADR = 0x00000000; // 0: reserved | 1-7: slace addr | 8-15: reserved
	i2c_imx->i2c_IFDR = 0x00000000; // 0-5: IC | 6-15: reserved
	i2c_imx->i2c_I2CR = 0x00000000; // 0-1: reserved | 2-7: multiple value | 8-15:reserved  (rsta: wo)
	i2c_imx->i2c_I2SR = 0x00000000; // 0-2: multy | 3:reserved | 4-7: miultiple value | 8-15: reserved   (ICF,IAAS,IBB,SRW,RXAK ro)
	i2c_imx->i2c_I2DR = 0x00000000; // 0-7: data | 8-15:reserved
	

	/* init irq */
	rc = vmm_devtree_irq_get(edev->node, &i2c_imx->irq, 0);
	if (rc) {
		vmm_free(i2c_imx);
		return rc;
	}
	i2c_imx->irq_level = 0;

	INIT_SPIN_LOCK(&i2c_imx->lock);
	edev->priv = i2c_imx;

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
