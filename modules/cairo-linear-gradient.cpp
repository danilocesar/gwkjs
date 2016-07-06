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

GWKJS_DEFINE_PROTO("CairoLinearGradient", cairo_linear_gradient, JSCLASS_BACKGROUND_FINALIZE)

GWKJS_NATIVE_CONSTRUCTOR_DECLARE(cairo_linear_gradient)
{
    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(cairo_linear_gradient)
    double x0, y0, x1, y1;
    cairo_pattern_t *pattern;

    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(cairo_linear_gradient);

    if (!gwkjs_parse_call_args(context, "LinearGradient", "ffff", argv,
                        "x0", &x0,
                        "y0", &y0,
                        "x1", &x1,
                        "y1", &y1))
        return JS_FALSE;

    pattern = cairo_pattern_create_linear(x0, y0, x1, y1);

    if (!gwkjs_cairo_check_status(context, cairo_pattern_status(pattern), "pattern"))
        return JS_FALSE;

    gwkjs_cairo_pattern_construct(context, object, pattern);
    cairo_pattern_destroy(pattern);

    GWKJS_NATIVE_CONSTRUCTOR_FINISH(cairo_linear_gradient);

    return JS_TRUE;
}

static void
gwkjs_cairo_linear_gradient_finalize(JSFreeOp *fop,
                                   JSObjectRef obj)
{
    gwkjs_cairo_pattern_finalize_pattern(fop, obj);
}

JSPropertySpec gwkjs_cairo_linear_gradient_proto_props[] = {
    { NULL }
};

JSFunctionSpec gwkjs_cairo_linear_gradient_proto_funcs[] = {
    // getLinearPoints
    { NULL }
};

JSObjectRef 
gwkjs_cairo_linear_gradient_from_pattern(JSContextRef       context,
                                       cairo_pattern_t *pattern)
{
    JSObjectRef object;

    g_return_val_if_fail(context != NULL, NULL);
    g_return_val_if_fail(pattern != NULL, NULL);
    g_return_val_if_fail(cairo_pattern_get_type(pattern) == CAIRO_PATTERN_TYPE_LINEAR, NULL);

    object = JS_NewObject(context, &gwkjs_cairo_linear_gradient_class, NULL, NULL);
    if (!object) {
        gwkjs_throw(context, "failed to create linear gradient pattern");
        return NULL;
    }

    gwkjs_cairo_pattern_construct(context, object, pattern);

    return object;
}

