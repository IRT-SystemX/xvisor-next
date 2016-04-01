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
 * @file vmm_safety.h
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief header file for safety checks
 *
 * Safety checks are mainly used to check input parameters of functions.
 * They can be enabled or disabled at compile time. Safety checks may use
 * GCC's prediction branch enforcement, so code is running slower when a
 * safety check is triggerred, but these checks have a few impact on the
 * execution time when not triggerred.
 *
 * The code below is strongly inspired from the Eina library (part of EFL).
 */

#ifndef __VMM_SAFETY_H__
#define __VMM_SAFETY_H__

/*
 * To disable/enable safety checks is specific places,
 * one can set VMM_ENABLE_SAFETY_CHECKS to 1 (enabled)
 * or 0 (disabled) regardless of the configuration.
 *
 * Use at your own risks!
 */
#ifdef CONFIG_ENABLE_SAFETY_CHECKS
# define VMM_ENABLE_SAFETY_CHECKS 1
#else
# define VMM_ENABLE_SAFETY_CHECKS 0
#endif

#if VMM_ENABLE_SAFETY_CHECKS

#define VMM_SAFETY_ON_NULL_RETURN(x)                                           \
        do {                                                                   \
                if (unlikely((x) == NULL)) {                                   \
                        vmm_printf("*** Safety check failed: " # x " == NULL\n"); \
                        return;                                                \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_TRUE_RETURN(x)                                           \
        do {                                                                   \
                if (unlikely(x)) {                                             \
                        vmm_printf("*** Safety check failed: " # x " is TRUE\n"); \
                        return;                                                \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_FALSE_RETURN(x)                                          \
        do {                                                                   \
                if (unlikely(!(x))) {                                          \
                        vmm_printf("*** Safety check failed: " # x " is FALSE\n"); \
                        return;                                                \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_NULL_RETURN_VAL(x, ret)                                  \
        do {                                                                   \
                if (unlikely((x) == NULL)) {                                   \
                        vmm_printf("*** Safety check failed: " # x " == NULL\n"); \
                        return (ret);                                          \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_TRUE_RETURN_VAL(x, ret)                                  \
        do {                                                                   \
                if (unlikely(x)) {                                             \
                        vmm_printf("*** Safety check failed: " # x " is TRUE\n"); \
                        return (ret);                                          \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_FALSE_RETURN_VAL(x)                                      \
        do {                                                                   \
                if (unlikely(!(x))) {                                          \
                        vmm_printf("*** Safety check failed: " # x " is FALSE\n"); \
                        return (ret);                                          \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_NULL_GOTO(x, label)                                      \
        do {                                                                   \
                if (unlikely((x) == NULL)) {                                   \
                        vmm_printf("*** Safety check failed: " # x " == NULL\n"); \
                        goto label;                                            \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_TRUE_GOTO(x, label)                                      \
        do {                                                                   \
                if (unlikely(x)) {                                             \
                        vmm_printf("*** Safety check failed: " # x " is TRUE\n"); \
                        goto label;                                            \
                }                                                              \
        } while (0)

#define VMM_SAFETY_ON_FALSE_GOTO(x, label)                                     \
        do {                                                                   \
                if (unlikely(!(x))) {                                          \
                        vmm_printf("*** Safety check failed: " # x " is FALSE\n"); \
                        goto label                                             \
                }                                                              \
        } while (0)

#else /* VMM_ENABLE_SAFETY_CHECKS == 0 */

#define VMM_SAFETY_ON_NULL_RETURN(x)                                           \
        do { (void)(!(x)); } while (0)

#define VMM_SAFETY_ON_TRUE_RETURN(x)                                           \
        do { (void)(x); } while (0)

#define VMM_SAFETY_ON_FALSE_RETURN(x)                                          \
        do { (void)(!(x)); } while (0)

#define VMM_SAFETY_ON_NULL_RETURN_VAL(x, ret)                                  \
        do { if (0 && !(x)) { (void)val; } } while (0)

#define VMM_SAFETY_ON_TRUE_RETURN_VAL(x, ret)                                  \
        do { if (0 && (x)) { (void)val; } } while (0)

#define VMM_SAFETY_ON_FALSE_RETURN_VAL(x)                                      \
        do { if (0 && !(x)) { (void)val; } } while (0)

#define VMM_SAFETY_ON_NULL_GOTO(x, label)                                      \
        do { if (0 && (x) == NULL) { goto label; } } while (0)

#define VMM_SAFETY_ON_TRUE_GOTO(x, label)                                      \
        do { if (0 && (x)) { goto label; } } while (0)

#define VMM_SAFETY_ON_FALSE_GOTO(x, label)                                     \
        do { if (0 && !(x)) { goto label; } } while (0)

#endif /* VMM_ENABLE_SAFETY_CHECKS */

#endif /* ! __VMM_SAFETY_H__ */
