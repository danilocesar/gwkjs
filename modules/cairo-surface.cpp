/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2010 litl, LLC.
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
#include <gi/foreign.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include "cairo-private.h"

typedef struct {
    void            *dummy;
    JSContextRef       context;
    JSObjectRef        object;
    cairo_surface_t *surface;
} GwkjsCairoSurface;

GWKJS_DEFINE_PROTO_ABSTRACT_WITH_GTYPE("CairoSurface", cairo_surface, CAIRO_GOBJECT_TYPE_SURFACE, JSCLASS_BACKGROUND_FINALIZE)
GWKJS_DEFINE_PRIV_FROM_JS(GwkjsCairoSurface, gwkjs_cairo_surface_class)

static void
gwkjs_cairo_surface_finalize(JSFreeOp *fop,
                           JSObjectRef obj)
{
    GwkjsCairoSurface *priv;
    priv = (GwkjsCairoSurface*) JS_GetPrivate(obj);
    if (priv == NULL)
        return;
    cairo_surface_destroy(priv->surface);
    g_slice_free(GwkjsCairoSurface, priv);
}

/* Properties */
JSPropertySpec gwkjs_cairo_surface_proto_props[] = {
    { NULL }
};

/* Methods */
static JSBool
writeToPNG_func(JSContextRef context,
                unsigned   argc,
                jsval     *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(argv.thisv());

    char *filename;
    cairo_surface_t *surface;

    if (!gwkjs_parse_call_args(context, "writeToPNG", "s", argv,
                        "filename", &filename))
        return JS_FALSE;

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    if (!surface) {
        g_free(filename);
        return JS_FALSE;
    }
    cairo_surface_write_to_png(surface, filename);
    g_free(filename);
    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface),
                                "surface"))
        return JS_FALSE;
    argv.rval().set(JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
getType_func(JSContextRef context,
             unsigned   argc,
             jsval     *vp)
{
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(rec.thisv());

    cairo_surface_t *surface;
    cairo_surface_type_t type;

    if (argc > 1) {
        gwkjs_throw(context, "Surface.getType() takes no arguments");
        return JS_FALSE;
    }

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    type = cairo_surface_get_type(surface);
    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface),
                                "surface"))
        return JS_FALSE;

    rec.rval().setInt32(type);
    return JS_TRUE;
}

JSFunctionSpec gwkjs_cairo_surface_proto_funcs[] = {
    // flush
    // getContent
    // getFontOptions
    { "getType", JSOP_WRAPPER((JSNative)getType_func), 0, 0},
    // markDirty
    // markDirtyRectangle
    // setDeviceOffset
    // getDeviceOffset
    // setFallbackResolution
    // getFallbackResolution
    // copyPage
    // showPage
    // hasShowTextGlyphs
    { "writeToPNG", JSOP_WRAPPER((JSNative)writeToPNG_func), 0, 0 },
    { NULL }
};

/* Public API */

/**
 * gwkjs_cairo_surface_construct:
 * @context: the context
 * @object: object to construct
 * @surface: cairo_surface to attach to the object
 *
 * Constructs a surface wrapper giving an empty JSObject and a
 * cairo surface. A reference to @surface will be taken.
 *
 * This is mainly used for subclasses where object is already created.
 */
void
gwkjs_cairo_surface_construct(JSContextRef       context,
                            JSObjectRef        object,
                            cairo_surface_t *surface)
{
    GwkjsCairoSurface *priv;

    g_return_if_fail(context != NULL);
    g_return_if_fail(object != NULL);
    g_return_if_fail(surface != NULL);

    priv = g_slice_new0(GwkjsCairoSurface);

    g_assert(priv_from_js(context, object) == NULL);
    JS_SetPrivate(object, priv);

    priv->context = context;
    priv->object = object;
    priv->surface = cairo_surface_reference(surface);
}

/**
 * gwkjs_cairo_surface_finalize:
 * @fop: the free op
 * @object: object to finalize
 *
 * Destroys the resources associated with a surface wrapper.
 *
 * This is mainly used for subclasses.
 */
void
gwkjs_cairo_surface_finalize_surface(JSFreeOp *fop,
                                   JSObjectRef object)
{
    g_return_if_fail(fop != NULL);
    g_return_if_fail(object != NULL);

    gwkjs_cairo_surface_finalize(fop, object);
}

/**
 * gwkjs_cairo_surface_from_surface:
 * @context: the context
 * @surface: cairo_surface to attach to the object
 *
 * Constructs a surface wrapper given cairo surface.
 * A reference to @surface will be taken.
 *
 */
JSObjectRef 
gwkjs_cairo_surface_from_surface(JSContextRef       context,
                               cairo_surface_t *surface)
{
    JSObjectRef object;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(surface != NULL, NULL);

    switch (cairo_surface_get_type(surface)) {
        case CAIRO_SURFACE_TYPE_IMAGE:
            return gwkjs_cairo_image_surface_from_surface(context, surface);
        case CAIRO_SURFACE_TYPE_PDF:
            return gwkjs_cairo_pdf_surface_from_surface(context, surface);
        case CAIRO_SURFACE_TYPE_PS:
            return gwkjs_cairo_ps_surface_from_surface(context, surface);
        case CAIRO_SURFACE_TYPE_SVG:
            return gwkjs_cairo_svg_surface_from_surface(context, surface);
        default:
            break;
    }

    object = JS_NewObject(context, &gwkjs_cairo_surface_class, NULL, NULL);
    if (!object) {
        gwkjs_throw(context, "failed to create surface");
        return NULL;
    }

    gwkjs_cairo_surface_construct(context, object, surface);

    return object;
}

/**
 * gwkjs_cairo_surface_get_surface:
 * @context: the context
 * @object: surface wrapper
 *
 * Returns: the surface attaches to the wrapper.
 *
 */
cairo_surface_t *
gwkjs_cairo_surface_get_surface(JSContextRef context,
                              JSObjectRef object)
{
    GwkjsCairoSurface *priv;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(object != NULL, NULL);

    priv = (GwkjsCairoSurface*) JS_GetPrivate(object);
    if (priv == NULL)
        return NULL;
    return priv->surface;
}

static JSBool
surface_to_g_argument(JSContextRef      context,
                      jsval           value,
                      const char     *arg_name,
                      GwkjsArgumentType argument_type,
                      GITransfer      transfer,
                      gboolean        may_be_null,
                      GArgument      *arg)
{
    JSObjectRef obj;
    cairo_surface_t *s;

    obj = JSVAL_TO_OBJECT(value);
    s = gwkjs_cairo_surface_get_surface(context, obj);
    if (!s)
        return JS_FALSE;
    if (transfer == GI_TRANSFER_EVERYTHING)
        cairo_surface_destroy(s);

    arg->v_pointer = s;
    return JS_TRUE;
}

static JSBool
surface_from_g_argument(JSContextRef  context,
                        jsval      *value_p,
                        GArgument  *arg)
{
    JSObjectRef obj;

    obj = gwkjs_cairo_surface_from_surface(context, (cairo_surface_t*)arg->v_pointer);
    if (!obj)
        return JS_FALSE;

    *value_p = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

static JSBool
surface_release_argument(JSContextRef  context,
                         GITransfer  transfer,
                         GArgument  *arg)
{
    cairo_surface_destroy((cairo_surface_t*)arg->v_pointer);
    return JS_TRUE;
}

static GwkjsForeignInfo foreign_info = {
    surface_to_g_argument,
    surface_from_g_argument,
    surface_release_argument
};

void
gwkjs_cairo_surface_init(JSContextRef context)
{
    gwkjs_struct_foreign_register("cairo", "Surface", &foreign_info);
}
