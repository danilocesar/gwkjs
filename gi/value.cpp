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

#include <config.h>

#include <util/log.h>

#include "foreign.h"
#include "value.h"
#include "closure.h"
#include "arg.h"
#include "param.h"
#include "object.h"
#include "fundamental.h"
#include "boxed.h"
#include "union.h"
#include "gtype.h"
#include "gerror.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>

#include <girepository.h>

static JSBool gwkjs_value_from_g_value_internal(JSContextRef    context,
                                              jsval        *value_p,
                                              const GValue *gvalue,
                                              gboolean      no_copy,
                                              GSignalQuery *signal_query,
                                              gint          arg_n);

/*
 * Gets signal introspection info about closure, or NULL if not found. Currently
 * only works for signals on introspected GObjects, not signals on GWKJS-defined
 * GObjects nor standalone closures. The return value must be unreffed.
 */
static GISignalInfo *
get_signal_info_if_available(GSignalQuery *signal_query)
{
    GIBaseInfo *obj;
    GIInfoType info_type;
    GISignalInfo *signal_info = NULL;

    if (!signal_query->itype)
        return NULL;

    obj = g_irepository_find_by_gtype(NULL, signal_query->itype);
    if (!obj)
        return NULL;

    info_type = g_base_info_get_type (obj);
    if (info_type == GI_INFO_TYPE_OBJECT)
      signal_info = g_object_info_find_signal((GIObjectInfo*)obj,
                                              signal_query->signal_name);
    else if (info_type == GI_INFO_TYPE_INTERFACE)
      signal_info = g_interface_info_find_signal((GIInterfaceInfo*)obj,
                                                 signal_query->signal_name);
    g_base_info_unref((GIBaseInfo*)obj);

    return signal_info;
}

/*
 * Fill in value_p with a JS array, converted from a C array stored as a pointer
 * in array_value, with its length stored in array_length_value.
 */
static JSBool
gwkjs_value_from_array_and_length_values(JSContextRef    context,
                                       jsval        *value_p,
                                       GITypeInfo   *array_type_info,
                                       const GValue *array_value,
                                       const GValue *array_length_value,
                                       gboolean      no_copy,
                                       GSignalQuery *signal_query,
                                       int           array_length_arg_n)
{
    jsval array_length;
    GArgument array_arg;

    g_assert(G_VALUE_HOLDS_POINTER(array_value));
    g_assert(G_VALUE_HOLDS_INT(array_length_value));

    if (!gwkjs_value_from_g_value_internal(context, &array_length,
                                         array_length_value, no_copy,
                                         signal_query, array_length_arg_n))
        return JS_FALSE;

    array_arg.v_pointer = g_value_get_pointer(array_value);

    return gwkjs_value_from_explicit_array(context, value_p, array_type_info,
                                         &array_arg, gwkjs_jsvalue_to_int(context, array_length, NULL));
}

//static void
//closure_marshal(GClosure        *closure,
//                GValue          *return_value,
//                guint            n_param_values,
//                const GValue    *param_values,
//                gpointer         invocation_hint,
//                gpointer         marshal_data)
//{
//    JSContextRef context;
//    JSRuntime *runtime;
//    JSObjectRef obj;
//    int argc;
//    jsval *argv;
//    jsval rval;
//    int i;
//    GSignalQuery signal_query = { 0, };
//    GISignalInfo *signal_info;
//    gboolean *skip;
//    int *array_len_indices_for;
//    GITypeInfo **type_info_for;
//    int argv_index;
//
//    gwkjs_debug_marshal(GWKJS_DEBUG_GCLOSURE,
//                      "Marshal closure %p",
//                      closure);
//
//    if (!gwkjs_closure_is_valid(closure)) {
//        /* We were destroyed; become a no-op */
//        return;
//    }
//
//    context = gwkjs_closure_get_context(closure);
//    runtime = JS_GetRuntime(context);
//    if (G_UNLIKELY (gwkjs_runtime_is_sweeping(runtime))) {
//        GSignalInvocationHint *hint = (GSignalInvocationHint*) invocation_hint;
//
//        g_critical("Attempting to call back into JSAPI during the sweeping phase of GC. "
//                   "This is most likely caused by not destroying a Clutter actor or Gtk+ "
//                   "widget with ::destroy signals connected, but can also be caused by "
//                   "using the destroy() or dispose() vfuncs. Because it would crash the "
//                   "application, it has been blocked and the JS callback not invoked.");
//        if (hint) {
//            gpointer instance;
//            g_signal_query(hint->signal_id, &signal_query);
//
//            instance = g_value_peek_pointer(&param_values[0]);
//            g_critical("The offending signal was %s on %s %p.", signal_query.signal_name,
//                       g_type_name(G_TYPE_FROM_INSTANCE(instance)), instance);
//        }
//        /* A gwkjs_dumpstack() would be nice here, but we can't,
//           because that works by creating a new Error object and
//           reading the stack property, which is the worst possible
//           idea during a GC session.
//        */
//        return;
//    }
//
//    obj = gwkjs_closure_get_callable(closure);
//    JS_BeginRequest(context);
//    JSAutoCompartment ac(context, obj);
//
//    argc = n_param_values;
//    rval = JSVAL_VOID;
//    if (argc > 0) {
//        argv = g_newa(jsval, n_param_values);
//
//        gwkjs_set_values(context, argv, argc, JSVAL_VOID);
//        gwkjs_root_value_locations(context, argv, argc);
//    } else {
//        /* squash a compiler warning */
//        argv = NULL;
//    }
//    JS_AddValueRoot(context, &rval);
//
//    if (marshal_data) {
//        /* we are used for a signal handler */
//        guint signal_id;
//
//        signal_id = GPOINTER_TO_UINT(marshal_data);
//
//        g_signal_query(signal_id, &signal_query);
//
//        if (!signal_query.signal_id) {
//            gwkjs_debug(GWKJS_DEBUG_GCLOSURE,
//                      "Signal handler being called on invalid signal");
//            goto cleanup;
//        }
//
//        if (signal_query.n_params + 1 != n_param_values) {
//            gwkjs_debug(GWKJS_DEBUG_GCLOSURE,
//                      "Signal handler being called with wrong number of parameters");
//            goto cleanup;
//        }
//    }
//
//    /* Check if any parameters, such as array lengths, need to be eliminated
//     * before we invoke the closure.
//     */
//    skip = g_newa(gboolean, argc);
//    memset(skip, 0, sizeof (gboolean) * argc);
//    array_len_indices_for = g_newa(int, argc);
//    for(i = 0; i < argc; i++)
//        array_len_indices_for[i] = -1;
//    type_info_for = g_newa(GITypeInfo *, argc);
//    memset(type_info_for, 0, sizeof (gpointer) * argc);
//
//    signal_info = get_signal_info_if_available(&signal_query);
//    if (signal_info) {
//        /* Start at argument 1, skip the instance parameter */
//        for (i = 1; i < argc; ++i) {
//            GIArgInfo *arg_info;
//            int array_len_pos;
//
//            arg_info = g_callable_info_get_arg(signal_info, i - 1);
//            type_info_for[i] = g_arg_info_get_type(arg_info);
//
//            array_len_pos = g_type_info_get_array_length(type_info_for[i]);
//            if (array_len_pos != -1) {
//                skip[array_len_pos + 1] = TRUE;
//                array_len_indices_for[i] = array_len_pos + 1;
//            }
//
//            g_base_info_unref((GIBaseInfo *)arg_info);
//        }
//
//        g_base_info_unref((GIBaseInfo *)signal_info);
//    }
//
//    argv_index = 0;
//    for (i = 0; i < argc; ++i) {
//        const GValue *gval = &param_values[i];
//        gboolean no_copy;
//        int array_len_index;
//        JSBool res;
//
//        if (skip[i])
//            continue;
//
//        no_copy = FALSE;
//
//        if (i >= 1 && signal_query.signal_id) {
//            no_copy = (signal_query.param_types[i - 1] & G_SIGNAL_TYPE_STATIC_SCOPE) != 0;
//        }
//
//        array_len_index = array_len_indices_for[i];
//        if (array_len_index != -1) {
//            const GValue *array_len_gval = &param_values[array_len_index];
//            res = gwkjs_value_from_array_and_length_values(context,
//                                                         &argv[argv_index],
//                                                         type_info_for[i],
//                                                         gval, array_len_gval,
//                                                         no_copy, &signal_query,
//                                                         array_len_index);
//        } else {
//            res = gwkjs_value_from_g_value_internal(context, &argv[argv_index],
//                                                  gval, no_copy, &signal_query,
//                                                  i);
//        }
//
//        if (!res) {
//            gwkjs_debug(GWKJS_DEBUG_GCLOSURE,
//                      "Unable to convert arg %d in order to invoke closure",
//                      i);
//            gwkjs_log_exception(context);
//            goto cleanup;
//        }
//
//        argv_index++;
//    }
//
//    for (i = 1; i < argc; i++)
//        if (type_info_for[i])
//            g_base_info_unref((GIBaseInfo *)type_info_for[i]);
//
//    gwkjs_closure_invoke(closure, argv_index, argv, &rval);
//
//    if (return_value != NULL) {
//        if (JSVAL_IS_VOID(rval)) {
//            /* something went wrong invoking, error should be set already */
//            goto cleanup;
//        }
//
//        if (!gwkjs_value_to_g_value(context, rval, return_value)) {
//            gwkjs_debug(GWKJS_DEBUG_GCLOSURE,
//                      "Unable to convert return value when invoking closure");
//            gwkjs_log_exception(context);
//            goto cleanup;
//        }
//    }
//
// cleanup:
//    if (argc > 0)
//        gwkjs_unroot_value_locations(context, argv, argc);
//    JS_RemoveValueRoot(context, &rval);
//    JS_EndRequest(context);
//}
//
//GClosure*
//gwkjs_closure_new_for_signal(JSContextRef  context,
//                           JSObjectRef   callable,
//                           const char *description,
//                           guint       signal_id)
//{
//    GClosure *closure;
//
//    closure = gwkjs_closure_new(context, callable, description, FALSE);
//
//    g_closure_set_meta_marshal(closure, GUINT_TO_POINTER(signal_id), closure_marshal);
//
//    return closure;
//}
//
GClosure*
gwkjs_closure_new_marshaled (JSContextRef    context,
                           JSObjectRef     callable,
                           const char   *description)
{
    gwkjs_throw(context, " gwkjs_closure_new_marshaled  not implemented");
    return NULL;
//TODO: implement
//    GClosure *closure;
//
//    closure = gwkjs_closure_new(context, callable, description, TRUE);
//
//    g_closure_set_marshal(closure, closure_marshal);
//
//    return closure;
}

// XXX: Hack-ish, as there's no way to know
// if a Number is an Integer or a Double. But this method is OK
// as wwe don't really care about precision issues
bool
_is_int(double d)
{
    return ( (gint64) d == d);
}

static GType
gwkjs_value_guess_g_type(JSContextRef context,
                       jsval      value)
{
    if (JSValueIsNull(context, value))
        return G_TYPE_POINTER;

    if (JSValueIsString(context, value))
        return G_TYPE_STRING;

    if (JSValueIsNumber(context, value)) {
        double n = JSValueToNumber(context, value, NULL);

        // TODO: I think it's OK to skip this code
        // and rely only in G_TYPE_DOUBLE
        if (_is_int(n))
            return G_TYPE_INT;
        else
            return G_TYPE_DOUBLE;
    }

    if (JSValueIsBoolean(context, value))
        return G_TYPE_BOOLEAN;

    if (JSValueIsObject(context, value))
        return gwkjs_gtype_get_actual_gtype(context, JSValueToObject(context, value, NULL));

    return G_TYPE_INVALID;
}

static JSBool
gwkjs_value_to_g_value_internal(JSContextRef    context,
                              jsval         value,
                              GValue       *gvalue,
                              gboolean      no_copy)
{
    GType gtype;
    JSValueRef exception = NULL;

    gtype = G_VALUE_TYPE(gvalue);

    if (gtype == 0) {
        gtype = gwkjs_value_guess_g_type(context, value);

        if (gtype == G_TYPE_INVALID) {
            gwkjs_throw(context, "Could not guess unspecified GValue type");
            return JS_FALSE;
        }

        gwkjs_debug_marshal(GWKJS_DEBUG_GCLOSURE,
                          "Guessed GValue type %s from JS Value",
                          g_type_name(gtype));

        g_value_init(gvalue, gtype);
    }

    gwkjs_debug_marshal(GWKJS_DEBUG_GCLOSURE,
                      "Converting jsval to gtype %s",
                      g_type_name(gtype));


    if (gtype == G_TYPE_STRING) {
        /* Don't use ValueToString since we don't want to just toString()
         * everything automatically
         */
        if (JSValueIsNull(context, value)) {
            g_value_set_string(gvalue, NULL);
        } else if (JSValueIsString(context, value)) {
            gchar *utf8_string;

            if (!gwkjs_string_to_utf8(context, value, &utf8_string))
                return JS_FALSE;

            g_value_take_string(gvalue, utf8_string);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; string expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_CHAR) {
        gint32 i = gwkjs_value_to_int32(context, value, &exception);
        if (!exception && i >= SCHAR_MIN && i <= SCHAR_MAX) {
            g_value_set_schar(gvalue, (signed char)i);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; char expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_UCHAR) {
        guint16 i = gwkjs_jsvalue_to_uint(context, value, &exception);
        if (!exception && i <= UCHAR_MAX) {
            g_value_set_uchar(gvalue, (unsigned char)i);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; unsigned char expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_INT) {
        gint32 i = gwkjs_jsvalue_to_int(context, value, &exception);
        if (!exception) {
            g_value_set_int(gvalue, i);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; integer expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_DOUBLE) {
        gdouble d = gwkjs_value_to_double(context, value, &exception);
        if (!exception) {
            g_value_set_double(gvalue, d);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; double expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_FLOAT) {
        gdouble d = gwkjs_value_to_float(context, value, &exception);
        if (!exception) {
            g_value_set_float(gvalue, d);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; float expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_UINT) {
        guint32 i = gwkjs_value_to_uint32(context, value, &exception);
        if (!exception) {
            g_value_set_uint(gvalue, i);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; unsigned integer expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (gtype == G_TYPE_BOOLEAN) {
        JSBool b = gwkjs_value_to_boolean(context, value, &exception);

        /* JS_ValueToBoolean() pretty much always succeeds,
         * which is maybe surprising sometimes, but could
         * be handy also...
         */
        if (!exception) {
            g_value_set_boolean(gvalue, b);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; boolean expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (g_type_is_a(gtype, G_TYPE_OBJECT) || g_type_is_a(gtype, G_TYPE_INTERFACE)) {
        GObject *gobj;

        gobj = NULL;
        if (JSVAL_IS_NULL(context, value)) {
            /* nothing to do */
        } else if (JSValueIsObject(context, value)) {
            JSObjectRef obj;
            obj = JSValueToObject(context, value, &exception);

            if (!gwkjs_typecheck_object(context, obj,
                                      gtype, JS_TRUE))
                return JS_FALSE;

            gobj = gwkjs_g_object_from_object(context, obj);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; object %s expected",
                      gwkjs_get_type_name(context, value),
                      g_type_name(gtype));
            return JS_FALSE;
        }

        g_value_set_object(gvalue, gobj);
    } else if (gtype == G_TYPE_STRV) {
        const gchar* length_name;
        JSBool found_length;

        length_name = gwkjs_context_get_const_string(context, GWKJS_STRING_LENGTH);
        if (JSVAL_IS_NULL(context, value)) {
            /* do nothing */
        } else if (gwkjs_object_has_property(context, JSValueToObject(context, value, NULL), length_name)) {
            jsval length_value;
            guint32 length;

            gboolean prop_exist = gwkjs_object_require_property(context,
                                          JSValueToObject(context, value, NULL), NULL,
                                          length_name,
                                          &length_value);
            length = gwkjs_value_to_uint32(context, length_value, &exception);
            if (!prop_exist ||
                exception) {
                gwkjs_throw(context,
                          "Wrong type %s; strv expected",
                          gwkjs_get_type_name(context, value));
                return JS_FALSE;
            } else {
                void *result;
                char **strv;

                if (!gwkjs_array_to_strv (context,
                                        value,
                                        length, &result))
                    return JS_FALSE;
                /* cast to strv in a separate step to avoid type-punning */
                strv = (char**) result;
                g_value_take_boxed (gvalue, strv);
            }
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; strv expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else if (g_type_is_a(gtype, G_TYPE_BOXED)) {
        void *gboxed;

        gboxed = NULL;
        if (JSVAL_IS_NULL(context, value)) {
            /* nothing to do */
        } else if (JSValueIsObject(context, value)) {
            JSObjectRef obj;
            obj = JSValueToObject(context, value, &exception);

            if (g_type_is_a(gtype, G_TYPE_ERROR)) {
                /* special case GError */
                if (!gwkjs_typecheck_gerror(context, obj, JS_TRUE))
                    return JS_FALSE;

                gboxed = gwkjs_gerror_from_error(context, obj);
            } else {
                GIBaseInfo *registered = g_irepository_find_by_gtype (NULL, gtype);

                /* We don't necessarily have the typelib loaded when
                   we first see the structure... */
                if (registered) {
                    GIInfoType info_type = g_base_info_get_type (registered);

                    if (info_type == GI_INFO_TYPE_STRUCT &&
                        g_struct_info_is_foreign ((GIStructInfo*)registered)) {
                        GArgument arg;

                        if (!gwkjs_struct_foreign_convert_to_g_argument (context, value,
                                                                       registered,
                                                                       NULL,
                                                                       GWKJS_ARGUMENT_ARGUMENT,
                                                                       GI_TRANSFER_NOTHING,
                                                                       TRUE, &arg))
                            return FALSE;

                        gboxed = arg.v_pointer;
                    }
                }

                /* First try a union, if that fails,
                   assume a boxed struct. Distinguishing
                   which one is expected would require checking
                   the associated GIBaseInfo, which is not necessary
                   possible, if e.g. we see the GType without
                   loading the typelib.
                */
                if (!gboxed) {
                    if (gwkjs_typecheck_union(context, obj,
                                            NULL, gtype, JS_FALSE)) {
                        gboxed = gwkjs_c_union_from_union(context, obj);
                    } else {
                        if (!gwkjs_typecheck_boxed(context, obj,
                                                 NULL, gtype, JS_TRUE))
                            return JS_FALSE;

                        gboxed = gwkjs_c_struct_from_boxed(context, obj);
                    }
                }
            }
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; boxed type %s expected",
                      gwkjs_get_type_name(context, value),
                      g_type_name(gtype));
            return JS_FALSE;
        }

        if (no_copy)
            g_value_set_static_boxed(gvalue, gboxed);
        else
            g_value_set_boxed(gvalue, gboxed);
    } else if (g_type_is_a(gtype, G_TYPE_VARIANT)) {
        GVariant *variant = NULL;

        if (JSVAL_IS_NULL(context, value)) {
            /* nothing to do */
        } else if (JSValueIsObject(context, value)) {
            JSObjectRef obj = JSValueToObject(context, value, &exception);

            if (!gwkjs_typecheck_boxed(context, obj,
                                     NULL, G_TYPE_VARIANT, JS_TRUE))
                return JS_FALSE;

            variant = (GVariant*) gwkjs_c_struct_from_boxed(context, obj);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; boxed type %s expected",
                      gwkjs_get_type_name(context, value),
                      g_type_name(gtype));
            return JS_FALSE;
        }

        g_value_set_variant (gvalue, variant);
    } else if (g_type_is_a(gtype, G_TYPE_ENUM)) {
        gint64 value_int64 = gwkjs_value_to_int64(context, value, &exception);

        if (!exception) {
            GEnumValue *v;
            gpointer gtype_class = g_type_class_ref(gtype);

            /* See arg.c:_gwkjs_enum_to_int() */
            v = g_enum_get_value(G_ENUM_CLASS(gtype_class),
                                 (int)value_int64);
            g_type_class_unref(gtype_class);
            if (v == NULL) {
                gwkjs_throw(context,
                          "%d is not a valid value for enumeration %s",
                          gwkjs_jsvalue_to_int(context, value, NULL), g_type_name(gtype));
                return JS_FALSE;
            }

            g_value_set_enum(gvalue, v->value);
        } else {
            gwkjs_throw(context,
                         "Wrong type %s; enum %s expected",
                         gwkjs_get_type_name(context, value),
                         g_type_name(gtype));
            return JS_FALSE;
        }
    } else if (g_type_is_a(gtype, G_TYPE_FLAGS)) {
        gint64 value_int64 = gwkjs_value_to_int64(context, value, &exception);

        if (!exception) {
            if (!_gwkjs_flags_value_is_valid(context, gtype, value_int64))
                return JS_FALSE;

            /* See arg.c:_gwkjs_enum_to_int() */
            g_value_set_flags(gvalue, (int)value_int64);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; flags %s expected",
                      gwkjs_get_type_name(context, value),
                      g_type_name(gtype));
            return JS_FALSE;
        }
    } else if (g_type_is_a(gtype, G_TYPE_PARAM)) {
        void *gparam;

        gparam = NULL;
        if (JSVAL_IS_NULL(context, value)) {
            /* nothing to do */
        } else if (JSValueIsObject(context, value)) {
            JSObjectRef obj;
            obj = JSValueToObject(context, value, &exception);

            if (!gwkjs_typecheck_param(context, obj, gtype, JS_TRUE))
                return JS_FALSE;

            gparam = gwkjs_g_param_from_param(context, obj);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; param type %s expected",
                      gwkjs_get_type_name(context, value),
                      g_type_name(gtype));
            return JS_FALSE;
        }

        g_value_set_param(gvalue, (GParamSpec*) gparam);
    } else if (g_type_is_a(gtype, G_TYPE_GTYPE)) {
        GType type;

        if (!JSValueIsObject(context, value)) {
            gwkjs_throw(context, "Wrong type %s; expect a GType object",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }

        type = gwkjs_gtype_get_actual_gtype(context, JSValueToObject(context, value, &exception));
        g_value_set_gtype(gvalue, type);
    } else if (g_type_is_a(gtype, G_TYPE_POINTER)) {
        if (JSVAL_IS_NULL(context, value)) {
            /* Nothing to do */
        } else {
            gwkjs_throw(context,
                      "Cannot convert non-null JS value to G_POINTER");
            return JS_FALSE;
        }
    } else if (JSValueIsNumber(context, value) &&
               g_value_type_transformable(G_TYPE_INT, gtype)) {
        /* Only do this crazy gvalue transform stuff after we've
         * exhausted everything else. Adding this for
         * e.g. ClutterUnit.
         */
        gint32 i = gwkjs_value_to_int32(context, value, &exception);
        if (!exception) {
            GValue int_value = { 0, };
            g_value_init(&int_value, G_TYPE_INT);
            g_value_set_int(&int_value, i);
            g_value_transform(&int_value, gvalue);
        } else {
            gwkjs_throw(context,
                      "Wrong type %s; integer expected",
                      gwkjs_get_type_name(context, value));
            return JS_FALSE;
        }
    } else {
        gwkjs_debug(GWKJS_DEBUG_GCLOSURE, "jsval is number %d gtype fundamental %d transformable to int %d from int %d",
                  JSValueIsNumber(context, value),
                  G_TYPE_IS_FUNDAMENTAL(gtype),
                  g_value_type_transformable(gtype, G_TYPE_INT),
                  g_value_type_transformable(G_TYPE_INT, gtype));

        gwkjs_throw(context,
                  "Don't know how to convert JavaScript object to GType %s",
                  g_type_name(gtype));
        return JS_FALSE;
    }

    return JS_TRUE;
}

JSBool
gwkjs_value_to_g_value(JSContextRef    context,
                     jsval         value,
                     GValue       *gvalue)
{
    return gwkjs_value_to_g_value_internal(context, value, gvalue, FALSE);
}

JSBool
gwkjs_value_to_g_value_no_copy(JSContextRef    context,
                             jsval         value,
                             GValue       *gvalue)
{
    return gwkjs_value_to_g_value_internal(context, value, gvalue, TRUE);
}

static JSBool
convert_int_to_enum (JSContextRef context,
                     jsval     *value_p,
                     GType      gtype,
                     int        v)
{
    double v_double;

    if (v > 0 && v < G_MAXINT) {
        /* Optimize the unambiguous case */
        v_double = v;
    } else {
        GIBaseInfo *info;

        /* Need to distinguish between negative integers and unsigned integers */

        info = g_irepository_find_by_gtype(g_irepository_get_default(), gtype);
        g_assert (info);

        v_double = _gwkjs_enum_from_int ((GIEnumInfo *)info, v);
        g_base_info_unref(info);
    }

    return JS_NewNumberValue(context, v_double, value_p);
}

static JSBool
gwkjs_value_from_g_value_internal(JSContextRef    context,
                                jsval        *value_p,
                                const GValue *gvalue,
                                gboolean      no_copy,
                                GSignalQuery *signal_query,
                                gint          arg_n)
{
    GType gtype;

    gtype = G_VALUE_TYPE(gvalue);

    gwkjs_debug_marshal(GWKJS_DEBUG_GCLOSURE,
                      "Converting gtype %s to jsval",
                      g_type_name(gtype));

    if (gtype == G_TYPE_STRING) {
        const char *v;
        v = g_value_get_string(gvalue);
        if (v == NULL) {
            gwkjs_debug_marshal(GWKJS_DEBUG_GCLOSURE,
                              "Converting NULL string to JSVAL_NULL");
            *value_p = JSValueMakeUndefined(context);
        } else {
            if (!gwkjs_string_from_utf8(context, v, -1, value_p))
                return JS_FALSE;
        }
    } else if (gtype == G_TYPE_CHAR) {
        char v;
        v = g_value_get_schar(gvalue);
        *value_p = gwkjs_int_to_jsvalue(context, v);
    } else if (gtype == G_TYPE_UCHAR) {
        unsigned char v;
        v = g_value_get_uchar(gvalue);
        *value_p = gwkjs_int_to_jsvalue(context, v);
    } else if (gtype == G_TYPE_INT) {
        int v;
        v = g_value_get_int(gvalue);
        return JS_NewNumberValue(context, v, value_p);
    } else if (gtype == G_TYPE_UINT) {
        guint v;
        v = g_value_get_uint(gvalue);
        return JS_NewNumberValue(context, v, value_p);
    } else if (gtype == G_TYPE_DOUBLE) {
        double d;
        d = g_value_get_double(gvalue);
        return JS_NewNumberValue(context, d, value_p);
    } else if (gtype == G_TYPE_FLOAT) {
        double d;
        d = g_value_get_float(gvalue);
        return JS_NewNumberValue(context, d, value_p);
    } else if (gtype == G_TYPE_BOOLEAN) {
        gboolean v;
        v = g_value_get_boolean(gvalue);
        *value_p = JSValueMakeBoolean(context, !!v);
    } else if (g_type_is_a(gtype, G_TYPE_OBJECT) || g_type_is_a(gtype, G_TYPE_INTERFACE)) {
        GObject *gobj;
        JSObjectRef obj;

        gobj = (GObject*) g_value_get_object(gvalue);

        obj = gwkjs_object_from_g_object(context, gobj);
        *value_p = obj;
    } else if (gtype == G_TYPE_STRV) {
        if (!gwkjs_array_from_strv (context,
                                  value_p,
                                  (const char**) g_value_get_boxed (gvalue))) {
            gwkjs_throw(context, "Failed to convert strv to array");
            return JS_FALSE;
        }
    } else if (g_type_is_a(gtype, G_TYPE_HASH_TABLE) ||
               g_type_is_a(gtype, G_TYPE_ARRAY) ||
               g_type_is_a(gtype, G_TYPE_BYTE_ARRAY) ||
               g_type_is_a(gtype, G_TYPE_PTR_ARRAY)) {
        gwkjs_throw(context,
                  "Unable to introspect element-type of container in GValue");
        return JS_FALSE;
    } else if (g_type_is_a(gtype, G_TYPE_BOXED) ||
               g_type_is_a(gtype, G_TYPE_VARIANT)) {
        GwkjsBoxedCreationFlags boxed_flags;
        GIBaseInfo *info;
        void *gboxed;
        JSObjectRef obj;

        if (g_type_is_a(gtype, G_TYPE_BOXED))
            gboxed = g_value_get_boxed(gvalue);
        else
            gboxed = g_value_get_variant(gvalue);
        boxed_flags = GWKJS_BOXED_CREATION_NONE;

        /* special case GError */
        if (g_type_is_a(gtype, G_TYPE_ERROR)) {
            obj = gwkjs_error_from_gerror(context, (GError*) gboxed, FALSE);
            *value_p = obj;

            return TRUE;
        }

        /* The only way to differentiate unions and structs is from
         * their g-i info as both GBoxed */
        info = g_irepository_find_by_gtype(g_irepository_get_default(),
                                           gtype);
        if (info == NULL) {
            gwkjs_throw(context,
                      "No introspection information found for %s",
                      g_type_name(gtype));
            return JS_FALSE;
        }

        if (g_base_info_get_type(info) == GI_INFO_TYPE_STRUCT &&
            g_struct_info_is_foreign((GIStructInfo*)info)) {
            JSBool ret;
            GIArgument arg;
            arg.v_pointer = gboxed;
            ret = gwkjs_struct_foreign_convert_from_g_argument(context, value_p, info, &arg);
            g_base_info_unref(info);
            return ret;
        }

        switch (g_base_info_get_type(info)) {
        case GI_INFO_TYPE_BOXED:
        case GI_INFO_TYPE_STRUCT:
            if (no_copy)
                boxed_flags = (GwkjsBoxedCreationFlags) (boxed_flags | GWKJS_BOXED_CREATION_NO_COPY);
            obj = gwkjs_boxed_from_c_struct(context, (GIStructInfo *)info, gboxed, boxed_flags);
            break;
        case GI_INFO_TYPE_UNION:
            obj = gwkjs_union_from_c_union(context, (GIUnionInfo *)info, gboxed);
            break;
        default:
            gwkjs_throw(context,
                      "Unexpected introspection type %d for %s",
                      g_base_info_get_type(info),
                      g_type_name(gtype));
            g_base_info_unref(info);
            return JS_FALSE;
        }

        *value_p = obj;
        g_base_info_unref(info);
    } else if (g_type_is_a(gtype, G_TYPE_ENUM)) {
        return convert_int_to_enum(context, value_p, gtype, g_value_get_enum(gvalue));
    } else if (g_type_is_a(gtype, G_TYPE_PARAM)) {
        GParamSpec *gparam;
        JSObjectRef obj;

        gparam = g_value_get_param(gvalue);

        obj = gwkjs_param_from_g_param(context, gparam);
        *value_p = obj;
    } else if (signal_query && g_type_is_a(gtype, G_TYPE_POINTER)) {
        JSBool res;
        GArgument arg;
        GIArgInfo *arg_info;
        GIBaseInfo *obj;
        GISignalInfo *signal_info;
        GITypeInfo type_info;

        obj = g_irepository_find_by_gtype(NULL, signal_query->itype);
        if (!obj) {
            gwkjs_throw(context, "Signal argument with GType %s isn't introspectable",
                      g_type_name(signal_query->itype));
            return JS_FALSE;
        }

        signal_info = g_object_info_find_signal((GIObjectInfo*)obj, signal_query->signal_name);

        if (!signal_info) {
            gwkjs_throw(context, "Unknown signal.");
            g_base_info_unref((GIBaseInfo*)obj);
            return JS_FALSE;
        }
        arg_info = g_callable_info_get_arg(signal_info, arg_n - 1);
        g_arg_info_load_type(arg_info, &type_info);

        g_assert(("Check gwkjs_value_from_array_and_length_values() before calling"
                  " gwkjs_value_from_g_value_internal()",
                  g_type_info_get_array_length(&type_info) == -1));

        arg.v_pointer = g_value_get_pointer(gvalue);

        res = gwkjs_value_from_g_argument(context, value_p, &type_info, &arg, TRUE);

        g_base_info_unref((GIBaseInfo*)arg_info);
        g_base_info_unref((GIBaseInfo*)signal_info);
        g_base_info_unref((GIBaseInfo*)obj);
        return res;
    } else if (g_type_is_a(gtype, G_TYPE_POINTER)) {
        gpointer pointer;

        pointer = g_value_get_pointer(gvalue);

        if (pointer == NULL) {
            *value_p = JSValueMakeUndefined(context);
        } else {
            gwkjs_throw(context,
                      "Can't convert non-null pointer to JS value");
            return JS_FALSE;
        }
    } else if (g_value_type_transformable(gtype, G_TYPE_DOUBLE)) {
        GValue double_value = { 0, };
        double v;
        g_value_init(&double_value, G_TYPE_DOUBLE);
        g_value_transform(gvalue, &double_value);
        v = g_value_get_double(&double_value);
        return JS_NewNumberValue(context, v, value_p);
    } else if (g_value_type_transformable(gtype, G_TYPE_INT)) {
        GValue int_value = { 0, };
        int v;
        g_value_init(&int_value, G_TYPE_INT);
        g_value_transform(gvalue, &int_value);
        v = g_value_get_int(&int_value);
        return JS_NewNumberValue(context, v, value_p);
    } else if (G_TYPE_IS_INSTANTIATABLE(gtype)) {
        /* The gtype is none of the above, it should be a custom
           fundamental type. */
        JSObjectRef obj;
        obj = gwkjs_fundamental_from_g_value(context, (const GValue*)gvalue, gtype);
        if (obj == NULL)
            return JS_FALSE;
        else
            *value_p = obj;
    } else {
        gwkjs_throw(context,
                  "Don't know how to convert GType %s to JavaScript object",
                  g_type_name(gtype));
        return JS_FALSE;
    }

    return JS_TRUE;
}

JSBool
gwkjs_value_from_g_value(JSContextRef    context,
                       jsval        *value_p,
                       const GValue *gvalue)
{
    return gwkjs_value_from_g_value_internal(context, value_p, gvalue, FALSE, NULL, 0);
}
