/**
# Copyright (c) 2016 Open Wide
#		2016 Institut de Recherche Technologique SystemX
#
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
 * @file printf.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief runtime (stack protection) display tests
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"Stack Protection test"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			wb_stackprotection_init
#define	MODULE_EXIT			wb_stackprotection_exit

static int wb_stackprotection_run(struct wboxtest *test, struct vmm_chardev *cdev)
{
   volatile int start_frame;
   int rc = VMM_OK;
   volatile unsigned int i;
   volatile unsigned char *ptr = (unsigned char *)(&start_frame);

#ifndef CONFIG_STACK_PROTECTOR
   vmm_cprintf(cdev, "Stack protection is not enabled! Expect system failure!\n");
#warning Meh
#endif

   for (i = 0; i < 128; ++i) {
      ptr[i] = 'X';
   }


   return rc;
}

static struct wboxtest wb_stackprotection = {
        .name = "stackprotection",
        .run = wb_stackprotection_run,
};

static int __init wb_stackprotection_init(void)
{
        return wboxtest_register("runtime", &wb_stackprotection);
}

static void __exit wb_stackprotection_exit(void)
{
        wboxtest_unregister(&wb_stackprotection);
}

VMM_DECLARE_MODULE(MODULE_DESC,
                   MODULE_AUTHOR,
                   MODULE_LICENSE,
                   MODULE_IPRIORITY,
                   MODULE_INIT,
                   MODULE_EXIT);

