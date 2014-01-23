/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef CGR_CONFIG
#define CGR_CONFIG

// For int types on POSIX systems
#include <stdlib.h>

#define CGR_PLUGIN_NAME V-Ray For Blender

#define CGR_USE_DEBUG        1
#define CGR_USE_CALL_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_TIME_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_DRAW_DEBUG  (0 && CGR_USE_DEBUG)

#define CGR_MAX_PLUGIN_NAME  1024

#ifndef WIN32
#  define COLOR_RED      "\033[0;31m"
#  define COLOR_GREEN    "\033[0;32m"
#  define COLOR_YELLOW   "\033[0;33m"
#  define COLOR_BLUE     "\033[0;34m"
#  define COLOR_MAGENTA  "\033[0;35m"
#  define COLOR_DEFAULT  "\033[0m"
#else
#  define COLOR_RED      ""
#  define COLOR_GREEN    ""
#  define COLOR_YELLOW   ""
#  define COLOR_BLUE     ""
#  define COLOR_MAGENTA  ""
#  define COLOR_DEFAULT  ""
#endif

#define NOT(x) !(x)

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)

#ifdef CGR_PLUGIN_NAME
#  define _OUTPUT_PROMPT(P) COLOR_MAGENTA STRINGIZE(P) COLOR_DEFAULT ": "
#  define OUTPUT_PROMPT _OUTPUT_PROMPT(CGR_PLUGIN_NAME)
#else
#  define OUTPUT_PROMPT COLOR_MAGENTA "Info" COLOR_DEFAULT ": "
#endif

#ifdef CGR_PLUGIN_NAME
#  define _OUTPUT_ERROR_PROMPT(P) COLOR_RED STRINGIZE(P) " Error" COLOR_DEFAULT ": "
#  define OUTPUT_ERROR_PROMPT _OUTPUT_ERROR_PROMPT(CGR_PLUGIN_NAME)
#else
#  define OUTPUT_ERROR_PROMPT COLOR_RED "Error" COLOR_DEFAULT ": "
#endif

#if CGR_USE_DEBUG == 0
#  define DEBUG_PRINT(use_debug, ...)
#else
#  define DEBUG_PRINT(use_debug, ...) \
    if(use_debug) { \
        fprintf(stdout, OUTPUT_PROMPT); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
    }
#endif

#define PRINT_ERROR(...) \
    fprintf(stdout, OUTPUT_ERROR_PROMPT); \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
	fflush(stdout);

#define PRINT_INFO(...) \
    fprintf(stdout, OUTPUT_PROMPT); \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
	fflush(stdout);

#define PRINT_INFO_LB(...) \
	fprintf(stdout, OUTPUT_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fflush(stdout);

#if CGR_USE_DEBUG == 0
#  define PRINT_TM4(label, tm) ()
#else
#  define PRINT_TM4(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[0][0], tm[0][1], tm[0][2], tm[0][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[1][0], tm[1][1], tm[1][2], tm[1][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[2][0], tm[2][1], tm[2][2], tm[2][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[3][0], tm[3][1], tm[3][2], tm[3][3]);
#endif

#define COPY_VECTOR(r, index, a) \
    r[index+0] = a[0]; \
    r[index+1] = a[1]; \
    r[index+2] = a[2]; \
    index += 3;

#define GEOM_TYPE(ob) ob->type == OB_MESH || ob->type == OB_CURVE || ob->type == OB_SURF  || ob->type == OB_FONT  || ob->type == OB_MBALL
#define EMPTY_TYPE(ob) ob->type == OB_EMPTY

#define	ANIM_CHECK_NONE    0
#define	ANIM_CHECK_SIMPLE  1
#define	ANIM_CHECK_HASH    2
#define	ANIM_CHECK_BOTH    3

#ifdef _WIN32
typedef unsigned __int8  u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
#endif

#endif // CGR_CONFIG
