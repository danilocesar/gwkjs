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

#include "type-module.h"

struct _GwkjsTypeModule {
    GTypeModule parent;
};

struct _GwkjsTypeModuleClass {
    GTypeModuleClass parent_class;
};

G_DEFINE_TYPE (GwkjsTypeModule, gwkjs_type_module, G_TYPE_TYPE_MODULE)

static GwkjsTypeModule *global_type_module;

GwkjsTypeModule *
gwkjs_type_module_get ()
{
    if (global_type_module == NULL) {
        global_type_module = (GwkjsTypeModule *) g_object_new (GWKJS_TYPE_TYPE_MODULE, NULL);
    }

    return global_type_module;
}

static gboolean
gwkjs_type_module_load (GTypeModule *self)
{
    return TRUE;
}

static void
gwkjs_type_module_unload (GTypeModule *self)
{
    g_assert_not_reached ();
}

static void
gwkjs_type_module_class_init (GwkjsTypeModuleClass *klass)
{
    GTypeModuleClass *type_module_class;

    type_module_class = G_TYPE_MODULE_CLASS (klass);
    type_module_class->load = gwkjs_type_module_load;
    type_module_class->unload = gwkjs_type_module_unload;
}

static void
gwkjs_type_module_init (GwkjsTypeModule *self)
{
    /* Prevent the use count from ever dropping to zero */
    g_type_module_use (G_TYPE_MODULE (self));
}
