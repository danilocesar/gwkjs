/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef GWKJS_TYPE_MODULE_H
#define GWKJS_TYPE_MODULE_H

#include <glib-object.h>

typedef struct _GwkjsTypeModule GwkjsTypeModule;
typedef struct _GwkjsTypeModuleClass GwkjsTypeModuleClass;

#define GWKJS_TYPE_TYPE_MODULE              (gwkjs_type_module_get_type ())
#define GWKJS_TYPE_MODULE(module)           (G_TYPE_CHECK_INSTANCE_CAST ((module), GWKJS_TYPE_TYPE_MODULE, GwkjsTypeModule))
#define GWKJS_TYPE_MODULE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GWKJS_TYPE_TYPE_MODULE, GwkjsTypeModuleClass))
#define GWKJS_IS_TYPE_MODULE(module)        (G_TYPE_CHECK_INSTANCE_TYPE ((module), GWKJS_TYPE_TYPE_MODULE))
#define GWKJS_IS_TYPE_MODULE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GWKJS_TYPE_TYPE_MODULE))
#define GWKJS_TYPE_MODULE_GET_CLASS(module) (G_TYPE_INSTANCE_GET_CLASS ((module), GWKJS_TYPE_TYPE_MODULE, GwkjsTypeModuleClass))

GType gwkjs_type_module_get_type (void) G_GNUC_CONST;

GwkjsTypeModule *gwkjs_type_module_get (void);

#endif
