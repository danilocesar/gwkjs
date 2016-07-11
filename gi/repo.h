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

#ifndef __GWKJS_REPO_H__
#define __GWKJS_REPO_H__

#include <glib.h>

#include <girepository.h>

#include <gwkjs/gwkjs-module.h>
#include <util/log.h>

G_BEGIN_DECLS

JSBool      gwkjs_define_repo                     (JSContextRef  context,
                                                   JSObjectRef   *module_out,
                                                   const char     *name);
const char* gwkjs_info_type_name                  (GIInfoType      type);
JSObjectRef  gwkjs_lookup_private_namespace        (JSContextRef  context);
JSObjectRef gwkjs_lookup_namespace_object         (JSContextRef  context,
                                                   GIBaseInfo     *info);

JSObjectRef gwkjs_lookup_namespace_object_by_name (JSContextRef  context,
                                                   const gchar  *name);

JSObjectRef gwkjs_lookup_function_object          (JSContextRef context,
                                                   GIFunctionInfo *info);

JSObjectRef gwkjs_lookup_generic_constructor      (JSContextRef context,
                                                   GIBaseInfo     *info);

JSObjectRef gwkjs_lookup_generic_prototype        (JSContextRef context,
                                                   GIBaseInfo     *info);

JSObjectRef gwkjs_define_info                     (JSContextRef context,
                                                   JSObjectRef    in_object,
                                                   GIBaseInfo     *info,
                                                   gboolean       *defined);

char*       gwkjs_camel_from_hyphen               (const char     *hyphen_name);
char*       gwkjs_hyphen_from_camel               (const char     *camel_name);


#if GWKJS_VERBOSE_ENABLE_GI_USAGE
void _gwkjs_log_info_usage(GIBaseInfo *info);
#endif

G_END_DECLS

#endif  /* __GWKJS_REPO_H__ */
