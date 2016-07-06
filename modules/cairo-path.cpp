/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2010 Red Hat, Inc.
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

#include <config.h>

#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>
#include <cairo.h>
#include "cairo-private.h"

typedef struct {
    JSContextRef       context;
    JSObjectRef        object;
    cairo_path_t    *path;
} GwkjsCairoPath;

GWKJS_DEFINE_PROTO_ABSTRACT("CairoPath", cairo_path, JSCLASS_BACKGROUND_FINALIZE)
GWKJS_DEFINE_PRIV_FROM_JS(GwkjsCairoPath, gwkjs_cairo_path_class)

static void
gwkjs_cairo_path_finalize(JSFreeOp *fop,
                        JSObjectRef obj)
{
    GwkjsCairoPath *priv;
    priv = (GwkjsCairoPath*) JS_GetPrivate(obj);
    if (priv == NULL)
        return;
    cairo_path_destroy(priv->path);
    g_slice_free(GwkjsCairoPath, priv);
}

/* Properties */
JSPropertySpec gwkjs_cairo_path_proto_props[] = {
    { NULL }
};

JSFunctionSpec gwkjs_cairo_path_proto_funcs[] = {
    { NULL }
};

/**
 * gwkjs_cairo_path_from_path:
 * @context: the context
 * @path: cairo_path_t to attach to the object
 *
 * Constructs a pattern wrapper given cairo pattern.
 * NOTE: This function takes ownership of the path.
 */
JSObjectRef 
gwkjs_cairo_path_from_path(JSContextRef    context,
                         cairo_path_t *path)
{
    JSObjectRef object;
    GwkjsCairoPath *priv;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(path != NULL, NULL);

    object = JS_NewObject(context, &gwkjs_cairo_path_class, NULL, NULL);
    if (!object) {
        gwkjs_throw(context, "failed to create path");
        return NULL;
    }

    priv = g_slice_new0(GwkjsCairoPath);

    g_assert(priv_from_js(context, object) == NULL);
    JS_SetPrivate(object, priv);

    priv->context = context;
    priv->object = object;
    priv->path = path;

    return object;
}


/**
 * gwkjs_cairo_path_get_path:
 * @context: the context
 * @object: path wrapper
 *
 * Returns: the path attached to the wrapper.
 *
 */
cairo_path_t *
gwkjs_cairo_path_get_path(JSContextRef context,
                        JSObjectRef  object)
{
    GwkjsCairoPath *priv;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(object != NULL, NULL);

    priv = (GwkjsCairoPath*) JS_GetPrivate(object);
    if (priv == NULL)
        return NULL;
    return priv->path;
}
