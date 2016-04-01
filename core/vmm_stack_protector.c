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
 * @file vmm_stack_protector.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief source file of hypervisor stack protector.
 */

#include <vmm_stdio.h>
#include <vmm_types.h>
#include <vmm_modules.h>
#include <libs/stacktrace.h>

extern __noreturn void __stack_chk_fail(void);
extern uintptr_t __stack_chk_guard;

#ifndef CONFIG_CANARIES_RANDOM
# ifndef CONFIG_CANARIES /* Just a safety check */
#  error CONFIG_CANARIES is not defined
# endif

/*
 * Hard-code the canaries value.
 * Bad for security purposes, but well sufficient enough for
 * debugging.
 */
uintptr_t __stack_chk_guard = CONFIG_CANARIES;
VMM_EXPORT_SYMBOL(__stack_chk_guard);

#else
# error Random Canaries are not implemented
#endif

__noreturn void __stack_chk_fail(void)
{
        vmm_panic("Stack corruption detected in 0x%p",
                  __builtin_return_address(0));
}
VMM_EXPORT_SYMBOL(__stack_chk_fail);
