///* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
///*
// * Copyright (c) 2010  litl, LLC
// *
// * Permission is hereby granted, free of charge, to any person obtaining a copy
// * of this software and associated documentation files (the "Software"), to
// * deal in the Software without restriction, including without limitation the
// * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// * sell copies of the Software, and to permit persons to whom the Software is
// * furnished to do so, subject to the following conditions:
// *
// * The above copyright notice and this permission notice shall be included in
// * all copies or substantial portions of the Software.
// *
// * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// * IN THE SOFTWARE.
// */
//
//#ifndef __GWKJS_OVERRIDE_H__
//#define __GWKJS_OVERRIDE_H__
//
//#include <girepository.h>
//#include <gwkjs/gwkjs.h>
//#include "arg.h"
//
//typedef JSBool (*GwkjsArgOverrideToGArgumentFunc) (JSContextRef      context,
//                                                 jsval           value,
//                                                 const char     *arg_name,
//                                                 GwkjsArgumentType argument_type,
//                                                 GITransfer      transfer,
//                                                 gboolean        may_be_null,
//                                                 GArgument      *arg);
//
//typedef JSBool (*GwkjsArgOverrideFromGArgumentFunc) (JSContextRef  context,
//                                                   jsval      *value_p,
//                                                   GArgument  *arg);
//typedef JSBool (*GwkjsArgOverrideReleaseGArgumentFunc) (JSContextRef  context,
//                                                      GITransfer  transfer,
//                                                      GArgument  *arg);
//
//typedef struct {
//    GwkjsArgOverrideToGArgumentFunc to_func;
//    GwkjsArgOverrideFromGArgumentFunc from_func;
//    GwkjsArgOverrideReleaseGArgumentFunc release_func;
//} GwkjsForeignInfo;
//
//JSBool  gwkjs_struct_foreign_register                (const char         *gi_namespace,
//                                                    const char         *type_name,
//                                                    GwkjsForeignInfo *info);
//
//JSBool  gwkjs_struct_foreign_convert_to_g_argument   (JSContextRef          context,
//                                                    jsval               value,
//                                                    GIBaseInfo         *interface_info,
//                                                    const char         *arg_name,
//                                                    GwkjsArgumentType     argument_type,
//                                                    GITransfer          transfer,
//                                                    gboolean            may_be_null,
//                                                    GArgument          *arg);
//JSBool  gwkjs_struct_foreign_convert_from_g_argument (JSContextRef          context,
//                                                    jsval              *value_p,
//                                                    GIBaseInfo         *interface_info,
//                                                    GArgument          *arg);
//JSBool  gwkjs_struct_foreign_release_g_argument      (JSContextRef          context,
//                                                    GITransfer          transfer,
//                                                    GIBaseInfo         *interface_info,
//                                                    GArgument          *arg);
//
//#endif /* __GWKJS_OVERRIDE_H__ */
