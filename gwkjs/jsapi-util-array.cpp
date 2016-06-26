///* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
///*
// * Copyright (c) 2008  litl, LLC
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
//#include <config.h>
//
//#include "jsapi-util.h"
//#include "compat.h"
//
///* Maximum number of elements allowed in a GArray of rooted jsvals.
// * We pre-alloc that amount and then never allow the array to grow,
// * or we'd have invalid memory rooted if the internals of GArray decide
// * to move the contents to a new memory area
// */
//#define ARRAY_MAX_LEN 32
//
///**
// * gwkjs_rooted_array_new:
// *
// * Creates an opaque data type that holds jsvals and keeps
// * their location (NOT their value) GC-rooted.
// *
// * Returns: an opaque object prepared to hold GC root locations.
// **/
//GwkjsRootedArray*
//gwkjs_rooted_array_new()
//{
//    GArray *array;
//
//    /* we prealloc ARRAY_MAX_LEN to avoid realloc */
//    array = g_array_sized_new(FALSE,         /* zero-terminated */
//                              FALSE,         /* clear */
//                              sizeof(jsval), /* element size */
//                              ARRAY_MAX_LEN); /* reserved size */
//
//    return (GwkjsRootedArray*) array;
//}
//
///* typesafe wrapper */
//static void
//add_root_jsval(JSContext *context,
//               jsval     *value_p)
//{
//    JS_BeginRequest(context);
//    JS_AddValueRoot(context, value_p);
//    JS_EndRequest(context);
//}
//
///* typesafe wrapper */
//static void
//remove_root_jsval(JSContext *context,
//                  jsval     *value_p)
//{
//    JS_BeginRequest(context);
//    JS_RemoveValueRoot(context, value_p);
//    JS_EndRequest(context);
//}
//
///**
// * gwkjs_rooted_array_append:
// * @context: a #JSContext
// * @array: a #GwkjsRootedArray created by gwkjs_rooted_array_new()
// * @value: a jsval
// *
// * Appends @jsval to @array, calling JS_AddValueRoot on the location where it's stored.
// *
// **/
//void
//gwkjs_rooted_array_append(JSContext        *context,
//                        GwkjsRootedArray *array,
//                        jsval             value)
//{
//    GArray *garray;
//
//    g_return_if_fail(context != NULL);
//    g_return_if_fail(array != NULL);
//
//    garray = (GArray*) array;
//
//    if (garray->len >= ARRAY_MAX_LEN) {
//        gwkjs_throw(context, "Maximum number of values (%d)",
//                     ARRAY_MAX_LEN);
//        return;
//    }
//
//    g_array_append_val(garray, value);
//    add_root_jsval(context, & g_array_index(garray, jsval, garray->len - 1));
//}
//
///**
// * gwkjs_rooted_array_get:
// * @context: a #JSContext
// * @array: an array
// * @i: element to return
// * Returns: value of an element
// */
//jsval
//gwkjs_rooted_array_get(JSContext        *context,
//                     GwkjsRootedArray *array,
//                     int               i)
//{
//    GArray *garray;
//
//    g_return_val_if_fail(context != NULL, JSVAL_VOID);
//    g_return_val_if_fail(array != NULL, JSVAL_VOID);
//
//    garray = (GArray*) array;
//
//    if (i < 0 || i >= (int) garray->len) {
//        gwkjs_throw(context, "Index %d is out of range", i);
//        return JSVAL_VOID;
//    }
//
//    return g_array_index(garray, jsval, i);
//}
//
///**
// * gwkjs_rooted_array_get_data:
// *
// * @context: a #JSContext
// * @array: an array
// * Returns: the rooted jsval locations in the array
// */
//jsval*
//gwkjs_rooted_array_get_data(JSContext      *context,
//                          GwkjsRootedArray *array)
//{
//    GArray *garray;
//
//    g_return_val_if_fail(context != NULL, NULL);
//    g_return_val_if_fail(array != NULL, NULL);
//
//    garray = (GArray*) array;
//
//    return (jsval*) garray->data;
//}
//
///**
// * gwkjs_rooted_array_get_length:
// *
// * @context: a #JSContext
// * @array: an array
// * Returns: number of jsval in the rooted array
// */
//int
//gwkjs_rooted_array_get_length (JSContext        *context,
//                             GwkjsRootedArray *array)
//{
//    GArray *garray;
//
//    g_return_val_if_fail(context != NULL, 0);
//    g_return_val_if_fail(array != NULL, 0);
//
//    garray = (GArray*) array;
//
//    return garray->len;
//}
//
///**
// * gwkjs_root_value_locations:
// * @context: a #JSContext
// * @locations: contiguous locations in memory that store jsvals (must be initialized)
// * @n_locations: the number of locations to root
// *
// * Calls JS_AddValueRoot() on each address in @locations.
// *
// **/
//void
//gwkjs_root_value_locations(JSContext        *context,
//                         jsval            *locations,
//                         int               n_locations)
//{
//    int i;
//
//    g_return_if_fail(context != NULL);
//    g_return_if_fail(locations != NULL);
//    g_return_if_fail(n_locations >= 0);
//
//    JS_BeginRequest(context);
//    for (i = 0; i < n_locations; i++) {
//        add_root_jsval(context, ((jsval*)locations) + i);
//    }
//    JS_EndRequest(context);
//}
//
///**
// * gwkjs_unroot_value_locations:
// * @context: a #JSContext
// * @locations: contiguous locations in memory that store jsvals and have been added as GC roots
// * @n_locations: the number of locations to unroot
// *
// * Calls JS_RemoveValueRoot() on each address in @locations.
// *
// **/
//void
//gwkjs_unroot_value_locations(JSContext *context,
//                           jsval     *locations,
//                           int        n_locations)
//{
//    int i;
//
//    g_return_if_fail(context != NULL);
//    g_return_if_fail(locations != NULL);
//    g_return_if_fail(n_locations >= 0);
//
//    JS_BeginRequest(context);
//    for (i = 0; i < n_locations; i++) {
//        remove_root_jsval(context, ((jsval*)locations) + i);
//    }
//    JS_EndRequest(context);
//}
//
///**
// * gwkjs_set_values:
// * @context: a #JSContext
// * @locations: array of jsval
// * @n_locations: the number of elements to set
// * @initializer: what to set each element to
// *
// * Assigns initializer to each member of the given array.
// *
// **/
//void
//gwkjs_set_values(JSContext        *context,
//               jsval            *locations,
//               int               n_locations,
//               jsval             initializer)
//{
//    int i;
//
//    g_return_if_fail(context != NULL);
//    g_return_if_fail(locations != NULL);
//    g_return_if_fail(n_locations >= 0);
//
//    for (i = 0; i < n_locations; i++) {
//        locations[i] = initializer;
//    }
//}
//
///**
// * gwkjs_rooted_array_free:
// * @context: a #JSContext
// * @array: a #GwkjsRootedArray created with gwkjs_rooted_array_new()
// * @free_segment: whether or not to free and unroot the internal jsval array
// *
// * Frees the memory allocated for the #GwkjsRootedArray. If @free_segment is
// * %TRUE the internal memory block allocated for the jsval array will
// * be freed and unrooted also.
// *
// * Returns: the jsval array if it was not freed
// **/
//jsval*
//gwkjs_rooted_array_free(JSContext        *context,
//                      GwkjsRootedArray *array,
//                      gboolean          free_segment)
//{
//    GArray *garray;
//
//    g_return_val_if_fail(context != NULL, NULL);
//    g_return_val_if_fail(array != NULL, NULL);
//
//    garray = (GArray*) array;
//
//    if (free_segment)
//        gwkjs_unroot_value_locations(context, (jsval*) garray->data, garray->len);
//
//    return (jsval*) g_array_free(garray, free_segment);
//}
