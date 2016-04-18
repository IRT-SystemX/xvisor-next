/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file virtio_console.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for Virtio Console serial port driver.
 */

#ifndef __VIRTIO_CONSOLE_H_
#define __VIRTIO_CONSOLE_H_

#include <arm_types.h>
#include <emu/virtio_mmio.h>
#include <emu/virtio_console.h>

void virtio_console_printch(physical_addr_t base, char ch);
char virtio_console_getch(physical_addr_t base);
int virtio_console_init(physical_addr_t base);

#endif /* __VIRTIO_CONSOLE_H_ */
