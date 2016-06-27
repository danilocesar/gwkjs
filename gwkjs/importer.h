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

#ifndef __GWKJS_IMPORTER_H__
#define __GWKJS_IMPORTER_H__

#if !defined (__GWKJS_GWKJS_MODULE_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs-module.h> can be included directly."
#endif

#include <glib.h>
#include "gwkjs/jsapi-util.h"

G_BEGIN_DECLS

JSObjectRef gwkjs_create_root_importer (GwkjsContext *context,
                                        const char **initial_search_path,
                                        gboolean     add_standard_search_path);

JSBool    gwkjs_define_root_importer (JSContextRef   context,
                                      JSObjectRef    in_object,
                                      JSValueRef     importer);

JSBool    gwkjs_define_root_importer_object (JSContextRef   context,
                                             JSObjectRef    in_object,
                                             JSValueRef     importer);

JSObjectRef gwkjs_define_importer      (JSContextRef   context,
                                    JSObjectRef    in_object,
                                    const char  *importer_name,
                                    const char **initial_search_path,
                                    gboolean     add_standard_search_path);

G_END_DECLS

#endif  /* __GWKJS_IMPORTER_H__ */
