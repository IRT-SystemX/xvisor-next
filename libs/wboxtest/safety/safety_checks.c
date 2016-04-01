/**
 * Copyright (c) 2016 Open Wide
 *		2016 Institut de Recherche Technologique SystemX
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
 * @file safety_checks.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief safety checks tests
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_safety.h>
#include <libs/stringlib.h>
#include <libs/wboxtest.h>

#define MODULE_DESC			"Safety Checks Tests"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define	MODULE_INIT			wb_safety_checks_init
#define	MODULE_EXIT			wb_safety_checks_exit


union test {
        const void *ptr;
        bool cond;
};

enum test_type {
        TEST_TYPE_POINTER,
        TEST_TYPE_CONDITION
};

static int _test_ptr(const void *ptr)
{
        VMM_SAFETY_ON_NULL_GOTO(ptr, label);
        if ((ptr == NULL) && VMM_ENABLE_SAFETY_CHECKS) { /* Safety failed */
                return VMM_EFAIL;
        }
label:
        VMM_SAFETY_ON_NULL_RETURN_VAL(ptr, VMM_OK);
        if ((ptr == NULL) && VMM_ENABLE_SAFETY_CHECKS) { /* Safety failed */
                return VMM_EFAIL;
        }
        return VMM_OK;
}

static int _test_cond(bool cond)
{
        if (cond) {
                VMM_SAFETY_ON_TRUE_GOTO(cond, label);
        } else {
                VMM_SAFETY_ON_FALSE_GOTO(cond, label);
        }
        if (VMM_ENABLE_SAFETY_CHECKS) { /* Safety failed */
                return VMM_EFAIL;
        }

label:
        if (cond) {
                VMM_SAFETY_ON_TRUE_RETURN_VAL(cond, VMM_OK);
        } else {
                VMM_SAFETY_ON_FALSE_RETURN_VAL(cond, VMM_OK);
        }
        if (VMM_ENABLE_SAFETY_CHECKS) { /* Safety failed */
                return VMM_EFAIL;
        }
        return VMM_OK;
}

static int _safety = 0;
static const int _safety_expected = 3;
static const int _no_safety_expected = 6;

static void _func(void)
{
        /* Safeties shall not be trigerred */
        VMM_SAFETY_ON_NULL_RETURN(_func); /* _func != NULL */
        ++_safety;
        VMM_SAFETY_ON_TRUE_RETURN(FALSE);
        ++_safety;
        VMM_SAFETY_ON_FALSE_RETURN(TRUE);
        ++_safety;

        /* Safeties shall be trigerred */
        VMM_SAFETY_ON_NULL_RETURN(NULL);
        ++_safety;
        VMM_SAFETY_ON_TRUE_RETURN(TRUE);
        ++_safety;
        VMM_SAFETY_ON_FALSE_RETURN(FALSE);
        ++_safety;
}

static int _test_ret(void)
{
        int expected;

        _func();
        expected = VMM_ENABLE_SAFETY_CHECKS ? _safety_expected : _no_safety_expected;
        return (_safety == expected) ? VMM_OK : VMM_EFAIL;
}

static int wb_safety_checks_run(struct wboxtest *test, struct vmm_chardev *cdev)
{
        int rc = VMM_OK;

        rc &= _test_ret();
        rc &= _test_cond(TRUE);
        rc &= _test_cond(FALSE);
        rc &= _test_ptr(NULL);
        rc &= _test_ptr(test);

        vmm_cprintf(cdev, "Safety checks (%s)... %s\n",
                    VMM_ENABLE_SAFETY_CHECKS ? "enabled", "disabled",
                    (rc == VMM_OK) "pass" : "FAIL!");

        return rc;
}

static struct wboxtest wb_safety_checks = {
        .name = "safety_checks",
        .run = wb_safety_checks_run,
};

static int __init wb_safety_checks_init(void)
{
        return wboxtest_register("safety", &wb_safety_checks);
}

static void __exit wb_safety_checks_exit(void)
{
        wboxtest_unregister(&wb_safety_checks);
}

VMM_DECLARE_MODULE(MODULE_DESC,
                   MODULE_AUTHOR,
                   MODULE_LICENSE,
                   MODULE_IPRIORITY,
                   MODULE_INIT,
                   MODULE_EXIT);
