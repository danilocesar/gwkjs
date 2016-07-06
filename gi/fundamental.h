/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2013       Intel Corporation
 * Copyright (c) 2008-2010  litl, LLC
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

#ifndef __GWKJS_FUNDAMENTAL_H__
#define __GWKJS_FUNDAMENTAL_H__

#include <glib.h>
#include <girepository.h>
#include "gwkjs/jsapi-util.h"

G_BEGIN_DECLS

JSObjectRef gwkjs_define_fundamental_class          (JSContextRef context,
                                                     JSObjectRef in_object,
                                                     GIObjectInfo  *info,
                                                     JSObjectRef *constructor_p,
                                                     JSObjectRef *prototype_p);

JSObjectRef  gwkjs_object_from_g_fundamental        (JSContextRef context,
                                                     GIObjectInfo *info,
                                                     void         *fobj);

void*     gwkjs_g_fundamental_from_object           (JSContextRef context,
                                                     JSObjectRef  obj);

JSObjectRef gwkjs_fundamental_from_g_value          (JSContextRef context,
                                                     const GValue *value,
                                                     GType        gtype);

JSBool    gwkjs_typecheck_fundamental               (JSContextRef context,
                                                     JSObjectRef  object,
                                                     GType        expected_gtype,
                                                     JSBool       throw_error);

JSBool    gwkjs_typecheck_is_fundamental            (JSContextRef context,
                                                     JSObjectRef  object,
                                                     JSBool       throw_error);

void*     gwkjs_fundamental_ref                     (JSContextRef context,
                                                     void         *fobj);

void      gwkjs_fundamental_unref                   (JSContextRef context,
                                                     void         *fobj);

G_END_DECLS

#endif  /* __GWKJS_FUNDAMENTAL_H__ */
