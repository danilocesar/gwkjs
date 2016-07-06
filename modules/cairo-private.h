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

#ifndef __CAIRO_PRIVATE_H__
#define __CAIRO_PRIVATE_H__

#include "cairo-module.h"
#include <cairo.h>

JSBool           gwkjs_cairo_check_status                 (JSContextRef       context,
                                                         cairo_status_t   status,
                                                         const char      *name);

jsval            gwkjs_cairo_region_create_proto          (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
void             gwkjs_cairo_region_init                  (JSContextRef       context);


jsval            gwkjs_cairo_context_create_proto         (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
cairo_t *        gwkjs_cairo_context_get_context          (JSContextRef       context,
                                                         JSObjectRef        object);
JSObjectRef        gwkjs_cairo_context_from_context         (JSContextRef       context,
                                                         cairo_t         *cr);
void             gwkjs_cairo_context_init                 (JSContextRef       context);
void             gwkjs_cairo_surface_init                 (JSContextRef       context);


/* cairo_path_t */
jsval            gwkjs_cairo_path_create_proto            (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
JSObjectRef        gwkjs_cairo_path_from_path               (JSContextRef       context,
                                                         cairo_path_t    *path);
cairo_path_t *   gwkjs_cairo_path_get_path                (JSContextRef       context,
                                                         JSObjectRef        path_wrapper);

/* surface */
jsval            gwkjs_cairo_surface_create_proto         (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
void             gwkjs_cairo_surface_construct            (JSContextRef       context,
                                                         JSObjectRef        object,
                                                         cairo_surface_t *surface);
void             gwkjs_cairo_surface_finalize_surface     (JSFreeOp        *fop,
                                                         JSObjectRef        object);
JSObjectRef        gwkjs_cairo_surface_from_surface         (JSContextRef       context,
                                                         cairo_surface_t *surface);
cairo_surface_t* gwkjs_cairo_surface_get_surface          (JSContextRef       context,
                                                         JSObjectRef        object);

/* image surface */
jsval            gwkjs_cairo_image_surface_create_proto   (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
void             gwkjs_cairo_image_surface_init           (JSContextRef       context,
                                                         JSObjectRef        object);
JSObjectRef        gwkjs_cairo_image_surface_from_surface   (JSContextRef       context,
                                                         cairo_surface_t *surface);

/* postscript surface */
#ifdef CAIRO_HAS_PS_SURFACE
jsval            gwkjs_cairo_ps_surface_create_proto      (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
#endif
JSObjectRef        gwkjs_cairo_ps_surface_from_surface       (JSContextRef       context,
                                                          cairo_surface_t *surface);

/* pdf surface */
#ifdef CAIRO_HAS_PDF_SURFACE
jsval            gwkjs_cairo_pdf_surface_create_proto     (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
#endif
JSObjectRef        gwkjs_cairo_pdf_surface_from_surface     (JSContextRef       context,
                                                         cairo_surface_t *surface);

/* svg surface */
#ifdef CAIRO_HAS_SVG_SURFACE
jsval            gwkjs_cairo_svg_surface_create_proto     (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
#endif
JSObjectRef        gwkjs_cairo_svg_surface_from_surface     (JSContextRef       context,
                                                         cairo_surface_t *surface);

/* pattern */
jsval            gwkjs_cairo_pattern_create_proto         (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
void             gwkjs_cairo_pattern_construct            (JSContextRef       context,
                                                         JSObjectRef        object,
                                                         cairo_pattern_t *pattern);
void             gwkjs_cairo_pattern_finalize_pattern     (JSFreeOp        *fop,
                                                         JSObjectRef        object);
JSObject*        gwkjs_cairo_pattern_from_pattern         (JSContextRef       context,
                                                         cairo_pattern_t *pattern);
cairo_pattern_t* gwkjs_cairo_pattern_get_pattern          (JSContextRef       context,
                                                         JSObjectRef        object);

/* gradient */
jsval            gwkjs_cairo_gradient_create_proto        (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);

/* linear gradient */
jsval            gwkjs_cairo_linear_gradient_create_proto (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
JSObjectRef        gwkjs_cairo_linear_gradient_from_pattern (JSContextRef       context,
                                                         cairo_pattern_t *pattern);

/* radial gradient */
jsval            gwkjs_cairo_radial_gradient_create_proto (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
JSObjectRef        gwkjs_cairo_radial_gradient_from_pattern (JSContextRef       context,
                                                         cairo_pattern_t *pattern);

/* surface pattern */
jsval            gwkjs_cairo_surface_pattern_create_proto (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
JSObjectRef        gwkjs_cairo_surface_pattern_from_pattern (JSContextRef       context,
                                                         cairo_pattern_t *pattern);

/* solid pattern */
jsval            gwkjs_cairo_solid_pattern_create_proto   (JSContextRef       context,
                                                         JSObjectRef        module,
                                                         const char      *proto_name,
                                                         JSObjectRef        parent);
JSObjectRef        gwkjs_cairo_solid_pattern_from_pattern   (JSContextRef       context,
                                                         cairo_pattern_t *pattern);

#endif /* __CAIRO_PRIVATE_H__ */

