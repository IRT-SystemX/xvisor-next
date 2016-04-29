/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
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
 * @file iterate.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief vfs/iterate display tests
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"vfs_iterate Test"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			wb_vfs_iterate_init
#define	MODULE_EXIT			wb_vfs_iterate_exit



static int wb_vfs_iterate_run(struct wboxtest *test, struct vmm_chardev *cdev)
{
	int count, i;
	struct mount *m;

	count = vfs_mount_count();
	for (i = 0; i < count; ++i) {
		m = vfs_mount_get(i);
	}
        return rc;
}

static struct wboxtest wb_vfs_iterate = {
        .name = "iterate",
        .run = wb_vfs_iterate_run,
};

static int __init wb_vfs_iterate_init(void)
{
        return wboxtest_register("vfs", &wb_vfs_iterate);
}

static void __exit wb_vfs_iterate_exit(void)
{
        wboxtest_unregister(&wb_vfs_iterate);
}

VMM_DECLARE_MODULE(MODULE_DESC,
                   MODULE_AUTHOR,
                   MODULE_LICENSE,
                   MODULE_IPRIORITY,
                   MODULE_INIT,
                   MODULE_EXIT);
