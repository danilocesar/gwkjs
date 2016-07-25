/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __GWKJS_UTIL_LOG_H__
#define __GWKJS_UTIL_LOG_H__

#include <glib.h>

G_BEGIN_DECLS

/* The idea of this is to be able to have one big log file for the entire
 * environment, and grep out what you care about. So each module or app
 * should have its own entry in the enum. Be sure to add new enum entries
 * to the switch in log.c
 */
typedef enum {
    GWKJS_DEBUG_STRACE_TIMESTAMP,
    GWKJS_DEBUG_GI_USAGE,
    GWKJS_DEBUG_MEMORY,
    GWKJS_DEBUG_CONTEXT,
    GWKJS_DEBUG_IMPORTER,
    GWKJS_DEBUG_NATIVE,
    GWKJS_DEBUG_KEEP_ALIVE,
    GWKJS_DEBUG_GREPO,
    GWKJS_DEBUG_GNAMESPACE,
    GWKJS_DEBUG_GOBJECT,
    GWKJS_DEBUG_GFUNCTION,
    GWKJS_DEBUG_GCLOSURE,
    GWKJS_DEBUG_GBOXED,
    GWKJS_DEBUG_GENUM,
    GWKJS_DEBUG_GPARAM,
    GWKJS_DEBUG_DATABASE,
    GWKJS_DEBUG_RESULTSET,
    GWKJS_DEBUG_WEAK_HASH,
    GWKJS_DEBUG_MAINLOOP,
    GWKJS_DEBUG_PROPS,
    GWKJS_DEBUG_SCOPE,
    GWKJS_DEBUG_HTTP,
    GWKJS_DEBUG_BYTE_ARRAY,
    GWKJS_DEBUG_GERROR,
    GWKJS_DEBUG_GFUNDAMENTAL,
} GwkjsDebugTopic;

/* These defines are because we have some pretty expensive and
 * extremely verbose debug output in certain areas, that's useful
 * sometimes, but just too much to compile in by default. The areas
 * tend to be broader and less focused than the ones represented by
 * GwkjsDebugTopic.
 *
 * Don't use these special "disabled by default" log macros to print
 * anything that's an abnormal or error situation.
 *
 * Don't use them for one-time events, either. They are for routine
 * stuff that happens over and over and would deluge the logs, so
 * should be off by default.
 */

/* Whether to be verbose about JavaScript property access and resolution */
#ifndef GWKJS_VERBOSE_ENABLE_PROPS
#define GWKJS_VERBOSE_ENABLE_PROPS 1
#endif

/* Whether to be verbose about JavaScript function arg and closure marshaling */
#ifndef GWKJS_VERBOSE_ENABLE_MARSHAL
#define GWKJS_VERBOSE_ENABLE_MARSHAL 0
#endif

/* Whether to be verbose about constructing, destroying, and gc-rooting
 * various kinds of JavaScript thingy
 */
#ifndef GWKJS_VERBOSE_ENABLE_LIFECYCLE
#define GWKJS_VERBOSE_ENABLE_LIFECYCLE 1
#endif

/* Whether to log all gobject-introspection types and methods we use
 */
#ifndef GWKJS_VERBOSE_ENABLE_GI_USAGE
#define GWKJS_VERBOSE_ENABLE_GI_USAGE 1
#endif

/* Whether to log all callback GClosure debugging (finalizing, invalidating etc)
 */
#ifndef GWKJS_VERBOSE_ENABLE_GCLOSURE
#define GWKJS_VERBOSE_ENABLE_GCLOSURE 0
#endif

/* Whether to log all GObject signal debugging
 */
#ifndef GWKJS_VERBOSE_ENABLE_GSIGNAL
#define GWKJS_VERBOSE_ENABLE_GSIGNAL 0
#endif

#if GWKJS_VERBOSE_ENABLE_PROPS
#define gwkjs_debug_jsprop(topic, format...) \
    do { gwkjs_debug(topic, format); } while(0)
#else
#define gwkjs_debug_jsprop(topic, format...)
#endif

#if GWKJS_VERBOSE_ENABLE_MARSHAL
#define gwkjs_debug_marshal(topic, format...) \
    do { gwkjs_debug(topic, format); } while(0)
#else
#define gwkjs_debug_marshal(topic, format...)
#endif

#if GWKJS_VERBOSE_ENABLE_LIFECYCLE
#define gwkjs_debug_lifecycle(topic, format...) \
    do { gwkjs_debug(topic, format); } while(0)
#else
#define gwkjs_debug_lifecycle(topic, format...)
#endif

#if GWKJS_VERBOSE_ENABLE_GI_USAGE
#define gwkjs_debug_gi_usage(format...) \
    do { gwkjs_debug(GWKJS_DEBUG_GI_USAGE, format); } while(0)
#else
#define gwkjs_debug_gi_usage(format...)
#endif

#if GWKJS_VERBOSE_ENABLE_GCLOSURE
#define gwkjs_debug_closure(format...) \
    do { gwkjs_debug(GWKJS_DEBUG_GCLOSURE, format); } while(0)
#else
#define gwkjs_debug_closure(format, ...)
#endif

#if GWKJS_VERBOSE_ENABLE_GSIGNAL
#define gwkjs_debug_gsignal(format...) \
    do { gwkjs_debug(GWKJS_DEBUG_GOBJECT, format); } while(0)
#else
#define gwkjs_debug_gsignal(format...)
#endif

void gwkjs_debug(GwkjsDebugTopic topic,
               const char   *format,
               ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif  /* __GWKJS_UTIL_LOG_H__ */
