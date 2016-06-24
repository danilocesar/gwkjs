/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2010  litl, LLC
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

#ifndef __GWKJS_BYTE_ARRAY_H__
#define __GWKJS_BYTE_ARRAY_H__

#if !defined (__GWKJS_GWKJS_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs.h> can be included directly."
#endif

#include <glib.h>
#include "gwkjs/jsapi-util.h"

G_BEGIN_DECLS

JSBool    gwkjs_typecheck_bytearray        (JSContext     *context,
                                          JSObject      *obj,
                                          JSBool         throw_error);

JSBool        gwkjs_define_byte_array_stuff    (JSContext  *context,
                                              JSObject  **module_out);

JSObject *    gwkjs_byte_array_from_byte_array (JSContext  *context,
                                              GByteArray *array);
JSObject *    gwkjs_byte_array_from_bytes (JSContext  *context,
                                         GBytes *bytes);

GByteArray *   gwkjs_byte_array_get_byte_array (JSContext  *context,
                                              JSObject   *object);

GBytes *      gwkjs_byte_array_get_bytes (JSContext  *context,
                                        JSObject   *object);

void          gwkjs_byte_array_peek_data (JSContext  *context,
                                        JSObject   *object,
                                        guint8    **out_data,
                                        gsize      *out_len);

G_END_DECLS

#endif  /* __GWKJS_BYTE_ARRAY_H__ */
