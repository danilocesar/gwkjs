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

#ifndef __GWKJS_MEM_H__
#define __GWKJS_MEM_H__

#if !defined (__GWKJS_GWKJS_MODULE_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs-module.h> can be included directly."
#endif

#include <glib.h>
#include "gwkjs/jsapi-util.h"

G_BEGIN_DECLS

typedef struct {
    volatile int value;
    const char *name;
} GwkjsMemCounter;

#define GWKJS_DECLARE_COUNTER(name) \
    extern GwkjsMemCounter gwkjs_counter_ ## name ;

GWKJS_DECLARE_COUNTER(everything)

GWKJS_DECLARE_COUNTER(boxed)
GWKJS_DECLARE_COUNTER(gerror)
GWKJS_DECLARE_COUNTER(closure)
GWKJS_DECLARE_COUNTER(database)
GWKJS_DECLARE_COUNTER(function)
GWKJS_DECLARE_COUNTER(fundamental)
GWKJS_DECLARE_COUNTER(importer)
GWKJS_DECLARE_COUNTER(ns)
GWKJS_DECLARE_COUNTER(object)
GWKJS_DECLARE_COUNTER(param)
GWKJS_DECLARE_COUNTER(repo)
GWKJS_DECLARE_COUNTER(resultset)
GWKJS_DECLARE_COUNTER(weakhash)
GWKJS_DECLARE_COUNTER(interface)

#define GWKJS_INC_COUNTER(name)                \
    do {                                        \
        g_atomic_int_add(&gwkjs_counter_everything.value, 1); \
        g_atomic_int_add(&gwkjs_counter_ ## name .value, 1); \
    } while (0)

#define GWKJS_DEC_COUNTER(name)                \
    do {                                        \
        g_atomic_int_add(&gwkjs_counter_everything.value, -1); \
        g_atomic_int_add(&gwkjs_counter_ ## name .value, -1); \
    } while (0)

#define GWKJS_GET_COUNTER(name) \
    g_atomic_int_get(&gwkjs_counter_ ## name .value)

void gwkjs_memory_report(const char *where,
                       gboolean    die_if_leaks);

G_END_DECLS

#endif  /* __GWKJS_MEM_H__ */
