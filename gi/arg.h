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

#ifndef __GWKJS_ARG_H__
#define __GWKJS_ARG_H__

#include <glib.h>

#include "gwkjs/jsapi-util.h"

#include <girepository.h>

G_BEGIN_DECLS

/* Different roles for a GArgument */
typedef enum {
    GWKJS_ARGUMENT_ARGUMENT,
    GWKJS_ARGUMENT_RETURN_VALUE,
    GWKJS_ARGUMENT_FIELD,
    GWKJS_ARGUMENT_LIST_ELEMENT,
    GWKJS_ARGUMENT_HASH_ELEMENT,
    GWKJS_ARGUMENT_ARRAY_ELEMENT
} GwkjsArgumentType;

JSBool gwkjs_value_to_arg   (JSContextRef context,
                           jsval       value,
                           GIArgInfo  *arg_info,
                           GArgument  *arg);

JSBool gwkjs_value_to_explicit_array (JSContextRef context,
                                    jsval       value,
                                    GIArgInfo  *arg_info,
                                    GArgument  *arg,
                                    gsize      *length_p);

void gwkjs_g_argument_init_default (JSContextRef context,
                                  GITypeInfo     *type_info,
                                  GArgument      *arg);

JSBool gwkjs_value_to_g_argument (JSContextRef context,
                                jsval           value,
                                GITypeInfo     *type_info,
                                const char     *arg_name,
                                GwkjsArgumentType argument_type,
                                GITransfer      transfer,
                                gboolean        may_be_null,
                                GArgument      *arg);

JSBool gwkjs_value_from_g_argument (JSContextRef context,
                                  jsval      *value_p,
                                  GITypeInfo *type_info,
                                  GArgument  *arg,
                                  gboolean    copy_structs);
JSBool gwkjs_value_from_explicit_array (JSContextRef context,
                                      jsval      *value_p,
                                      GITypeInfo *type_info,
                                      GArgument  *arg,
                                      int         length);

JSBool gwkjs_g_argument_release    (JSContextRef context,
                                  GITransfer  transfer,
                                  GITypeInfo *type_info,
                                  GArgument  *arg);
JSBool gwkjs_g_argument_release_out_array (JSContextRef context,
                                         GITransfer  transfer,
                                         GITypeInfo *type_info,
                                         guint       length,
                                         GArgument  *arg);
JSBool gwkjs_g_argument_release_in_array (JSContextRef context,
                                        GITransfer  transfer,
                                        GITypeInfo *type_info,
                                        guint       length,
                                        GArgument  *arg);
JSBool gwkjs_g_argument_release_in_arg (JSContextRef context,
                                      GITransfer  transfer,
                                      GITypeInfo *type_info,
                                      GArgument  *arg);

JSBool _gwkjs_flags_value_is_valid (JSContextRef context,
                                  GType        gtype,
                                  gint64       value);

JSBool _gwkjs_enum_value_is_valid (JSContextRef context,
                                 GIEnumInfo *enum_info,
                                 gint64      value);

gint64 _gwkjs_enum_from_int (GIEnumInfo *enum_info,
                           int         int_value);

JSBool gwkjs_array_from_strv (JSContextRef context,
                            jsval       *value_p,
                            const char **strv);

JSBool gwkjs_array_to_strv (JSContextRef context,
                          jsval        array_value,
                          unsigned int length,
                          void       **arr_p);

G_END_DECLS

#endif  /* __GWKJS_ARG_H__ */
