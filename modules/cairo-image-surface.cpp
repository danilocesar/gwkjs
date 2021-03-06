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
#include <cairo.h>
#include "cairo-private.h"

GWKJS_DEFINE_PROTO("CairoImageSurface", cairo_image_surface, JSCLASS_BACKGROUND_FINALIZE)

GWKJS_NATIVE_CONSTRUCTOR_DECLARE(cairo_image_surface)
{
    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(cairo_image_surface)
    int format, width, height;
    cairo_surface_t *surface;

    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(cairo_image_surface);

    // create_for_data optional parameter
    if (!gwkjs_parse_call_args(context, "ImageSurface", "iii", argv,
                        "format", &format,
                        "width", &width,
                        "height", &height))
        return JS_FALSE;

    surface = cairo_image_surface_create((cairo_format_t) format, width, height);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    gwkjs_cairo_surface_construct(context, object, surface);
    cairo_surface_destroy(surface);

    GWKJS_NATIVE_CONSTRUCTOR_FINISH(cairo_image_surface);

    return JS_TRUE;
}

static void
gwkjs_cairo_image_surface_finalize(JSFreeOp *fop,
                                 JSObjectRef obj)
{
    gwkjs_cairo_surface_finalize_surface(fop, obj);
}

JSPropertySpec gwkjs_cairo_image_surface_proto_props[] = {
    { NULL }
};

static JSBool
createFromPNG_func(JSContextRef context,
                   unsigned   argc,
                   jsval     *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
    char *filename;
    cairo_surface_t *surface;
    JSObjectRef surface_wrapper;

    if (!gwkjs_parse_call_args(context, "createFromPNG", "s", argv,
                        "filename", &filename))
        return JS_FALSE;

    surface = cairo_image_surface_create_from_png(filename);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    surface_wrapper = JS_NewObject(context, &gwkjs_cairo_image_surface_class, NULL, NULL);
    if (!surface_wrapper) {
        gwkjs_throw(context, "failed to create surface");
        return JS_FALSE;
    }
    gwkjs_cairo_surface_construct(context, surface_wrapper, surface);
    cairo_surface_destroy(surface);

    argv.rval().set(OBJECT_TO_JSVAL(surface_wrapper));
    return JS_TRUE;
}

static JSBool
getFormat_func(JSContextRef context,
               unsigned   argc,
               jsval     *vp)
{
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(rec.thisv());

    cairo_surface_t *surface;
    cairo_format_t format;

    if (argc > 1) {
        gwkjs_throw(context, "ImageSurface.getFormat() takes no arguments");
        return JS_FALSE;
    }

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    format = cairo_image_surface_get_format(surface);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    rec.rval().setInt32(format);
    return JS_TRUE;
}

static JSBool
getWidth_func(JSContextRef context,
              unsigned   argc,
              jsval     *vp)
{
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(rec.thisv());

    cairo_surface_t *surface;
    int width;

    if (argc > 1) {
        gwkjs_throw(context, "ImageSurface.getWidth() takes no arguments");
        return JS_FALSE;
    }

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    width = cairo_image_surface_get_width(surface);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    rec.rval().setInt32(width);
    return JS_TRUE;
}

static JSBool
getHeight_func(JSContextRef context,
               unsigned   argc,
               jsval     *vp)
{
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(rec.thisv());

    cairo_surface_t *surface;
    int height;

    if (argc > 1) {
        gwkjs_throw(context, "ImageSurface.getHeight() takes no arguments");
        return JS_FALSE;
    }

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    height = cairo_image_surface_get_height(surface);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    rec.rval().setInt32(height);
    return JS_TRUE;
}

static JSBool
getStride_func(JSContextRef context,
               unsigned   argc,
               jsval     *vp)
{
    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
    JSObjectRef obj = JSVAL_TO_OBJECT(rec.thisv());

    cairo_surface_t *surface;
    int stride;

    if (argc > 1) {
        gwkjs_throw(context, "ImageSurface.getStride() takes no arguments");
        return JS_FALSE;
    }

    surface = gwkjs_cairo_surface_get_surface(context, obj);
    stride = cairo_image_surface_get_stride(surface);

    if (!gwkjs_cairo_check_status(context, cairo_surface_status(surface), "surface"))
        return JS_FALSE;

    rec.rval().setInt32(stride);
    return JS_TRUE;
}

JSFunctionSpec gwkjs_cairo_image_surface_proto_funcs[] = {
    { "createFromPNG", JSOP_WRAPPER((JSNative)createFromPNG_func), 0, 0},
    // getData
    { "getFormat", JSOP_WRAPPER((JSNative)getFormat_func), 0, 0 },
    { "getWidth", JSOP_WRAPPER((JSNative)getWidth_func), 0, 0 },
    { "getHeight", JSOP_WRAPPER((JSNative)getHeight_func), 0, 0 },
    { "getStride", JSOP_WRAPPER((JSNative)getStride_func), 0, 0 },
    { NULL }
};

JSObjectRef 
gwkjs_cairo_image_surface_from_surface(JSContextRef       context,
                                     cairo_surface_t *surface)
{
    JSObjectRef object;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(surface != NULL, NULL);
    g_return_val_if_fail(cairo_surface_get_type(surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);

    object = JS_NewObject(context, &gwkjs_cairo_image_surface_class, NULL, NULL);
    if (!object) {
        gwkjs_throw(context, "failed to create image surface");
        return NULL;
    }

    gwkjs_cairo_surface_construct(context, object, surface);

    return object;
}

void
gwkjs_cairo_image_surface_init(JSContextRef context, JSObjectRef module_obj)
{

    if (!JS_DefineFunction(context, module_obj,
                           "createFromPNG",
                           (JSNative)createFromPNG_func,
                           1, GWKJS_MODULE_PROP_FLAGS))
        return;
}
