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

#include "function.h"
#include "arg.h"
#include "object.h"
#include "fundamental.h"
#include "boxed.h"
#include "union.h"
#include "gerror.h"
#include "closure.h"
#include "gtype.h"
#include "param.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>
#include <gwkjs/jsapi-private.h>
#include <gwkjs/exceptions.h>

#include <util/log.h>

#include <girepository.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* We use guint8 for arguments; functions can't
 * have more than this.
 */
#define GWKJS_ARG_INDEX_INVALID G_MAXUINT8

typedef struct {
    GIFunctionInfo *info;

    GwkjsParamType *param_types;

    guint8 expected_js_argc;
    guint8 js_out_argc;
    GIFunctionInvoker invoker;
} Function;

extern JSClassDefinition gwkjs_function_class;
static JSClassRef gwkjs_function_class_ref = NULL;

/* Because we can't free the mmap'd data for a callback
 * while it's in use, this list keeps track of ones that
 * will be freed the next time we invoke a C function.
 */
static GSList *completed_trampolines = NULL;  /* GwkjsCallbackTrampoline */

GWKJS_DEFINE_PRIV_FROM_JS(Function, gwkjs_function_class)

void
gwkjs_callback_trampoline_ref(GwkjsCallbackTrampoline *trampoline)
{
    trampoline->ref_count++;
}

void
gwkjs_callback_trampoline_unref(GwkjsCallbackTrampoline *trampoline)
{
    /* Not MT-safe, like all the rest of GWKJS */

    trampoline->ref_count--;
    if (trampoline->ref_count == 0) {
        JSContextRef context = trampoline->context;

        if (!trampoline->is_vfunc) {
            JSValueUnprotect(context, trampoline->js_function);
        }

        g_callable_info_free_closure(trampoline->info, trampoline->closure);
        g_base_info_unref( (GIBaseInfo*) trampoline->info);
        g_free (trampoline->param_types);
        g_slice_free(GwkjsCallbackTrampoline, trampoline);
    }
}

static void
set_return_ffi_arg_from_giargument (GITypeInfo  *ret_type,
                                    void        *result,
                                    GIArgument  *return_value)
{
    switch (g_type_info_get_tag(ret_type)) {
    case GI_TYPE_TAG_INT8:
        *(ffi_sarg *) result = return_value->v_int8;
        break;
    case GI_TYPE_TAG_UINT8:
        *(ffi_arg *) result = return_value->v_uint8;
        break;
    case GI_TYPE_TAG_INT16:
        *(ffi_sarg *) result = return_value->v_int16;
        break;
    case GI_TYPE_TAG_UINT16:
        *(ffi_arg *) result = return_value->v_uint16;
        break;
    case GI_TYPE_TAG_INT32:
        *(ffi_sarg *) result = return_value->v_int32;
        break;
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_BOOLEAN:
    case GI_TYPE_TAG_UNICHAR:
        *(ffi_arg *) result = return_value->v_uint32;

        break;
    case GI_TYPE_TAG_INT64:
        *(ffi_sarg *) result = return_value->v_int64;
        break;
    case GI_TYPE_TAG_UINT64:
        *(ffi_arg *) result = return_value->v_uint64;
        break;
    case GI_TYPE_TAG_INTERFACE:
        {
            GIBaseInfo* interface_info;
            GIInfoType interface_type;

            interface_info = g_type_info_get_interface(ret_type);
            interface_type = g_base_info_get_type(interface_info);

            switch (interface_type) {
            case GI_INFO_TYPE_ENUM:
            case GI_INFO_TYPE_FLAGS:
                *(ffi_sarg *) result = return_value->v_long;
                break;
            default:
                *(ffi_arg *) result = (ffi_arg) return_value->v_pointer;
                break;
            }

            g_base_info_unref(interface_info);
        }
    default:
        *(ffi_arg *) result = (ffi_arg) return_value->v_uint64;
        break;
    }
}

/* This is our main entry point for ffi_closure callbacks.
 * ffi_prep_closure is doing pure magic and replaces the original
 * function call with this one which gives us the ffi arguments,
 * a place to store the return value and our use data.
 * In other words, everything we need to call the JS function and
 * getting the return value back.
 */
static void
gwkjs_callback_closure(ffi_cif *cif,
                     void *result,
                     void **args,
                     void *data)
{
    JSContextRef context = NULL;
    JSObjectRef func_obj = NULL;
    GwkjsCallbackTrampoline *trampoline;
    int i, n_args, n_jsargs, n_outargs;
    jsval *jsargs, rval;
    JSObjectRef rval_obj = NULL;
    JSObjectRef this_object = NULL;
    GITypeInfo ret_type;
    gboolean success = FALSE;
    gboolean ret_type_is_void;
    JSValueRef exception = NULL;

    trampoline = (GwkjsCallbackTrampoline *) data;
    g_assert(trampoline);
    gwkjs_callback_trampoline_ref(trampoline);

    context = trampoline->context;
//TODO: implement - Might not be necessary
//    runtime = JS_GetRuntime(context);
//    if (G_UNLIKELY (gwkjs_runtime_is_sweeping(runtime))) {
//        g_critical("Attempting to call back into JSAPI during the sweeping phase of GC. "
//                   "This is most likely caused by not destroying a Clutter actor or Gtk+ "
//                   "widget with ::destroy signals connected, but can also be caused by "
//                   "using the destroy() or dispose() vfuncs. Because it would crash the "
//                   "application, it has been blocked and the JS callback not invoked.");
//        /* A gwkjs_dumpstack() would be nice here, but we can't,
//           because that works by creating a new Error object and
//           reading the stack property, which is the worst possible
//           idea during a GC session.
//        */
//        gwkjs_callback_trampoline_unref(trampoline);
//        return;
//    }

    func_obj = JSValueToObject(context, trampoline->js_function, NULL);

    n_args = g_callable_info_get_n_args(trampoline->info);

    g_assert(n_args >= 0);

    n_outargs = 0;
    jsargs = (jsval*)g_newa(jsval, n_args);
    for (i = 0, n_jsargs = 0; i < n_args; i++) {
        GIArgInfo arg_info;
        GITypeInfo type_info;
        GwkjsParamType param_type;

        g_callable_info_load_arg(trampoline->info, i, &arg_info);
        g_arg_info_load_type(&arg_info, &type_info);

        /* Skip void * arguments */
        if (g_type_info_get_tag(&type_info) == GI_TYPE_TAG_VOID)
            continue;

        if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_OUT) {
            n_outargs++;
            continue;
        }

        if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_INOUT)
            n_outargs++;

        param_type = trampoline->param_types[i];

        switch (param_type) {
            case PARAM_SKIPPED:
                continue;
            case PARAM_ARRAY: {
                gint array_length_pos = g_type_info_get_array_length(&type_info);
                GIArgInfo array_length_arg;
                GITypeInfo arg_type_info;
                jsval length;

                g_callable_info_load_arg(trampoline->info, array_length_pos, &array_length_arg);
                g_arg_info_load_type(&array_length_arg, &arg_type_info);
                if (!gwkjs_value_from_g_argument(context, &length,
                                               &arg_type_info,
                                               (GArgument *) args[array_length_pos], TRUE))
                    goto out;

                if (!gwkjs_value_from_explicit_array(context, &jsargs[n_jsargs++],
                                                   &type_info, (GArgument*) args[i], gwkjs_jsvalue_to_int(context, length, NULL)))
                    goto out;
                break;
            }
            case PARAM_NORMAL:
                if (!gwkjs_value_from_g_argument(context,
                                               &jsargs[n_jsargs++],
                                               &type_info,
                                               (GArgument *) args[i], FALSE))
                    goto out;
                break;
            default:
                g_assert_not_reached();
        }
    }

    if (trampoline->is_vfunc) {
        g_assert(n_args > 0);
        this_object = JSValueToObject(context, jsargs[0], NULL);
        jsargs++;
        n_jsargs--;
    } else {
        this_object = NULL;
    }

    rval = JSObjectCallAsFunction(context,
                                  JSValueToObject(context, trampoline->js_function, NULL),
                                  this_object,
                                  n_jsargs, jsargs, &exception);

    rval_obj = JSValueToObject(context, rval, NULL);
    if (exception)
        goto out;

    g_callable_info_load_return_type(trampoline->info, &ret_type);
    ret_type_is_void = g_type_info_get_tag (&ret_type) == GI_TYPE_TAG_VOID;

    if (n_outargs == 0 && !ret_type_is_void) {
        GIArgument argument;
        GITransfer transfer;

        transfer = g_callable_info_get_caller_owns (trampoline->info);
        /* non-void return value, no out args. Should
         * be a single return value. */
        if (!gwkjs_value_to_g_argument(context,
                                     rval,
                                     &ret_type,
                                     "callback",
                                     GWKJS_ARGUMENT_RETURN_VALUE,
                                     transfer,
                                     TRUE,
                                     &argument))
            goto out;

        set_return_ffi_arg_from_giargument(&ret_type,
                                           result,
                                           &argument);
    } else if (n_outargs == 1 && ret_type_is_void) {
        /* void return value, one out args. Should
         * be a single return value. */
        for (i = 0; i < n_args; i++) {
            GIArgInfo arg_info;
            GITypeInfo type_info;
            g_callable_info_load_arg(trampoline->info, i, &arg_info);
            if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_IN)
                continue;

            g_arg_info_load_type(&arg_info, &type_info);
            if (!gwkjs_value_to_g_argument(context,
                                         rval,
                                         &type_info,
                                         "callback",
                                         GWKJS_ARGUMENT_ARGUMENT,
                                         GI_TRANSFER_NOTHING,
                                         TRUE,
                                         *(GArgument **)args[i]))
                goto out;

            break;
        }
    } else {
        jsval elem = NULL;
        gsize elem_idx = 0;
        /* more than one of a return value or an out argument.
         * Should be an array of output values. */

        JSObjectRef rval_obj = JSValueToObject(context, rval, NULL);
        if (!ret_type_is_void) {
            GIArgument argument;

            exception = NULL;
            elem = JSObjectGetPropertyAtIndex(context, rval_obj, elem_idx, &exception);
            if (!elem || exception)
                goto out;

            if (!gwkjs_value_to_g_argument(context,
                                         elem,
                                         &ret_type,
                                         "callback",
                                         GWKJS_ARGUMENT_ARGUMENT,
                                         GI_TRANSFER_NOTHING,
                                         TRUE,
                                         &argument))
                goto out;

            set_return_ffi_arg_from_giargument(&ret_type,
                                               result,
                                               &argument);

            elem_idx++;
        }

        for (i = 0; i < n_args; i++) {
            GIArgInfo arg_info;
            GITypeInfo type_info;
            g_callable_info_load_arg(trampoline->info, i, &arg_info);
            if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_IN)
                continue;

            g_arg_info_load_type(&arg_info, &type_info);

            exception = NULL;
            elem = JSObjectGetPropertyAtIndex(context, rval_obj, elem_idx, &exception);
            if (!elem || exception)
                goto out;

            if (!gwkjs_value_to_g_argument(context,
                                         elem,
                                         &type_info,
                                         "callback",
                                         GWKJS_ARGUMENT_ARGUMENT,
                                         GI_TRANSFER_NOTHING,
                                         TRUE,
                                         *(GArgument **)args[i]))
                goto out;

            elem_idx++;
        }
    }

    success = TRUE;

out:
    if (!success) {
        gwkjs_log_exception (context, exception);

        /* Fill in the result with some hopefully neutral value */
        g_callable_info_load_return_type(trampoline->info, &ret_type);
        gwkjs_g_argument_init_default (context, &ret_type, (GArgument *) result);
    }

    if (trampoline->scope == GI_SCOPE_TYPE_ASYNC) {
        completed_trampolines = g_slist_prepend(completed_trampolines, trampoline);
    }

    gwkjs_callback_trampoline_unref(trampoline);
    gwkjs_schedule_gc_if_needed(context);
}

/* The global entry point for any invocations of GDestroyNotify;
 * look up the callback through the user_data and then free it.
 */
static void
gwkjs_destroy_notify_callback(gpointer data)
{
    GwkjsCallbackTrampoline *trampoline = (GwkjsCallbackTrampoline *) data;

    g_assert(trampoline);
    gwkjs_callback_trampoline_unref(trampoline);
}

GwkjsCallbackTrampoline*
gwkjs_callback_trampoline_new(JSContextRef      context,
                            jsval           function_val,
                            GICallableInfo *callable_info,
                            GIScopeType     scope,
                            gboolean        is_vfunc)
{
    GwkjsCallbackTrampoline *trampoline;
    int n_args, i;

    if (JSVAL_IS_NULL(context, function_val)) {
        return NULL;
    }

    JSObjectRef function = JSValueToObject(context, function_val, NULL);
    g_assert(function && JSObjectIsFunction(context, function));

    trampoline = g_slice_new(GwkjsCallbackTrampoline);
    trampoline->ref_count = 1;
    trampoline->context = context;
    trampoline->info = callable_info;
    g_base_info_ref((GIBaseInfo*)trampoline->info);
    trampoline->js_function = function;
    if (!is_vfunc)
        JSValueProtect(context, trampoline->js_function);

    /* Analyze param types and directions, similarly to init_cached_function_data */
    n_args = g_callable_info_get_n_args(trampoline->info);
    trampoline->param_types = g_new0(GwkjsParamType, n_args);

    for (i = 0; i < n_args; i++) {
        GIDirection direction;
        GIArgInfo arg_info;
        GITypeInfo type_info;
        GITypeTag type_tag;

        if (trampoline->param_types[i] == PARAM_SKIPPED)
            continue;

        g_callable_info_load_arg(trampoline->info, i, &arg_info);
        g_arg_info_load_type(&arg_info, &type_info);

        direction = g_arg_info_get_direction(&arg_info);
        type_tag = g_type_info_get_tag(&type_info);

        if (direction != GI_DIRECTION_IN) {
            /* INOUT and OUT arguments are handled differently. */
            continue;
        }

        if (type_tag == GI_TYPE_TAG_INTERFACE) {
            GIBaseInfo* interface_info;
            GIInfoType interface_type;

            interface_info = g_type_info_get_interface(&type_info);
            interface_type = g_base_info_get_type(interface_info);
            if (interface_type == GI_INFO_TYPE_CALLBACK) {
                gwkjs_throw(context, "Callback accepts another callback as a parameter. This is not supported");
                g_base_info_unref(interface_info);
                return NULL;
            }
            g_base_info_unref(interface_info);
        } else if (type_tag == GI_TYPE_TAG_ARRAY) {
            if (g_type_info_get_array_type(&type_info) == GI_ARRAY_TYPE_C) {
                int array_length_pos = g_type_info_get_array_length(&type_info);

                if (array_length_pos >= 0 && array_length_pos < n_args) {
                    GIArgInfo length_arg_info;

                    g_callable_info_load_arg(trampoline->info, array_length_pos, &length_arg_info);
                    if (g_arg_info_get_direction(&length_arg_info) != direction) {
                        gwkjs_throw(context, "Callback has an array with different-direction length arg, not supported");
                        return NULL;
                    }

                    trampoline->param_types[array_length_pos] = PARAM_SKIPPED;
                    trampoline->param_types[i] = PARAM_ARRAY;
                }
            }
        }
    }

    trampoline->closure = g_callable_info_prepare_closure(callable_info, &trampoline->cif,
                                                          gwkjs_callback_closure, trampoline);

    trampoline->scope = scope;
    trampoline->is_vfunc = is_vfunc;

    return trampoline;
}

/* an helper function to retrieve array lengths from a GArgument
   (letting the compiler generate good instructions in case of
   big endian machines) */
static unsigned long
get_length_from_arg (GArgument *arg, GITypeTag tag)
{
    switch (tag) {
    case GI_TYPE_TAG_INT8:
        return arg->v_int8;
    case GI_TYPE_TAG_UINT8:
        return arg->v_uint8;
    case GI_TYPE_TAG_INT16:
        return arg->v_int16;
    case GI_TYPE_TAG_UINT16:
        return arg->v_uint16;
    case GI_TYPE_TAG_INT32:
        return arg->v_int32;
    case GI_TYPE_TAG_UINT32:
        return arg->v_uint32;
    case GI_TYPE_TAG_INT64:
        return arg->v_int64;
    case GI_TYPE_TAG_UINT64:
        return arg->v_uint64;
    default:
        g_assert_not_reached ();
    }
}

static JSBool
gwkjs_fill_method_instance (JSContextRef  context,
                          JSObjectRef   obj,
                          Function   *function,
                          GIArgument *out_arg)
{
    GIBaseInfo *container = g_base_info_get_container((GIBaseInfo *) function->info);
    GIInfoType type = g_base_info_get_type(container);
    GType gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *)container);
    GITransfer transfer = g_callable_info_get_instance_ownership_transfer (function->info);

    switch (type) {
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
        /* GError must be special cased */
        if (g_type_is_a(gtype, G_TYPE_ERROR)) {
            if (!gwkjs_typecheck_gerror(context, obj, JS_TRUE))
                return JS_FALSE;

            out_arg->v_pointer = gwkjs_gerror_from_error(context, obj);
            if (transfer == GI_TRANSFER_EVERYTHING)
                out_arg->v_pointer = g_error_copy ((GError*) out_arg->v_pointer);
        } else if (type == GI_INFO_TYPE_STRUCT &&
                   g_struct_info_is_gtype_struct((GIStructInfo*) container)) {
            /* And so do GType structures */
            GType gtype;
            gpointer klass;

            gtype = gwkjs_gtype_get_actual_gtype(context, obj);

            if (gtype == G_TYPE_NONE) {
                gwkjs_throw(context, "Invalid GType class passed for instance parameter");
                return JS_FALSE;
            }

            /* We use peek here to simplify reference counting (we just ignore
               transfer annotation, as GType classes are never really freed)
               We know that the GType class is referenced at least once when
               the JS constructor is initialized.
            */

            if (g_type_is_a(gtype, G_TYPE_INTERFACE))
                klass = g_type_default_interface_peek(gtype);
            else
                klass = g_type_class_peek(gtype);

            out_arg->v_pointer = klass;
        } else {
            if (!gwkjs_typecheck_boxed(context, obj,
                                     container, gtype,
                                     JS_TRUE))
                return JS_FALSE;

            out_arg->v_pointer = gwkjs_c_struct_from_boxed(context, obj);
            if (transfer == GI_TRANSFER_EVERYTHING) {
                if (gtype != G_TYPE_NONE)
                    out_arg->v_pointer = g_boxed_copy (gtype, out_arg->v_pointer);
                else {
                    gwkjs_throw (context, "Cannot transfer ownership of instance argument for non boxed structure");
                    return JS_FALSE;
                }
            }
        }
        break;

    case GI_INFO_TYPE_UNION:
        if (!gwkjs_typecheck_union(context, obj,
                                 container, gtype, JS_TRUE))
            return JS_FALSE;

        out_arg->v_pointer = gwkjs_c_union_from_union(context, obj);
        if (transfer == GI_TRANSFER_EVERYTHING)
            out_arg->v_pointer = g_boxed_copy (gtype, out_arg->v_pointer);
        break;

    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
        if (g_type_is_a(gtype, G_TYPE_OBJECT)) {
            if (!gwkjs_typecheck_object(context, obj, gtype, JS_TRUE))
                return JS_FALSE;
            out_arg->v_pointer = gwkjs_g_object_from_object(context, obj);
            if (transfer == GI_TRANSFER_EVERYTHING)
                g_object_ref (out_arg->v_pointer);
        } else if (g_type_is_a(gtype, G_TYPE_PARAM)) {
            if (!gwkjs_typecheck_param(context, obj, G_TYPE_PARAM, JS_TRUE))
                return JS_FALSE;
            out_arg->v_pointer = gwkjs_g_param_from_param(context, obj);
            if (transfer == GI_TRANSFER_EVERYTHING)
                g_param_spec_ref ((GParamSpec*) out_arg->v_pointer);
        } else if (G_TYPE_IS_INTERFACE(gtype)) {
            if (gwkjs_typecheck_is_object(context, obj, JS_FALSE)) {
                if (!gwkjs_typecheck_object(context, obj, gtype, JS_TRUE))
                    return JS_FALSE;
                out_arg->v_pointer = gwkjs_g_object_from_object(context, obj);
                if (transfer == GI_TRANSFER_EVERYTHING)
                    g_object_ref (out_arg->v_pointer);
            } else {
                if (!gwkjs_typecheck_fundamental(context, obj, gtype, JS_TRUE))
                    return JS_FALSE;
                out_arg->v_pointer = gwkjs_g_fundamental_from_object(context, obj);
                if (transfer == GI_TRANSFER_EVERYTHING)
                    gwkjs_fundamental_ref (context, out_arg->v_pointer);
            }
        } else if (G_TYPE_IS_INSTANTIATABLE(gtype)) {
            if (!gwkjs_typecheck_fundamental(context, obj, gtype, JS_TRUE))
                return JS_FALSE;
            out_arg->v_pointer = gwkjs_g_fundamental_from_object(context, obj);
            if (transfer == GI_TRANSFER_EVERYTHING)
                gwkjs_fundamental_ref (context, out_arg->v_pointer);
        } else {
            gwkjs_throw_custom(context, "TypeError",
                             "%s.%s is not an object instance neither a fundamental instance of a supported type",
                             g_base_info_get_namespace(container),
                             g_base_info_get_name(container));
            return JS_FALSE;
        }
        break;

    default:
        g_assert_not_reached();
    }

    return JS_TRUE;
}

/*
 * This function can be called in 2 different ways. You can either use
 * it to create javascript objects by providing a @js_rval argument or
 * you can decide to keep the return values in #GArgument format by
 * providing a @r_value argument.
 */
static JSBool
gwkjs_invoke_c_function(JSContextRef      context,
                        Function          *function,
                        JSObjectRef       obj, /* "this" object */
                        unsigned          js_argc,
                        const JSValueRef  js_argv[],
                        jsval             *js_rval,
                        GArgument         *r_value)
{
    /* These first four are arrays which hold argument pointers.
     * @in_arg_cvalues: C values which are passed on input (in or inout)
     * @out_arg_cvalues: C values which are returned as arguments (out or inout)
     * @inout_original_arg_cvalues: For the special case of (inout) args, we need to
     *  keep track of the original values we passed into the function, in case we
     *  need to free it.
     * @ffi_arg_pointers: For passing data to FFI, we need to create another layer
     *  of indirection; this array is a pointer to an element in in_arg_cvalues
     *  or out_arg_cvalues.
     * @return_value: The actual return value of the C function, i.e. not an (out) param
     */
    GArgument *in_arg_cvalues;
    GArgument *out_arg_cvalues;
    GArgument *inout_original_arg_cvalues;
    gpointer *ffi_arg_pointers;
    GIFFIReturnValue return_value;
    gpointer return_value_p; /* Will point inside the union return_value */
    GArgument return_gargument;

    guint8 processed_c_args = 0;
    guint8 gi_argc, gi_arg_pos;
    guint8 c_argc, c_arg_pos;
    guint8 js_arg_pos;
    gboolean can_throw_gerror;
    gboolean did_throw_gerror = FALSE;
    GError *local_error = NULL;
    gboolean failed, postinvoke_release_failed;

    gboolean is_method;
    GITypeInfo return_info;
    GITypeTag return_tag;
    jsval *return_values = NULL;
    guint8 next_rval = 0; /* index into return_values */
    GSList *iter;

    /* Because we can't free a closure while we're in it, we defer
     * freeing until the next time a C function is invoked.  What
     * we should really do instead is queue it for a GC thread.
     */
    if (completed_trampolines) {
        for (iter = completed_trampolines; iter; iter = iter->next) {
            GwkjsCallbackTrampoline *trampoline = (GwkjsCallbackTrampoline *) iter->data;
            gwkjs_callback_trampoline_unref(trampoline);
        }
        g_slist_free(completed_trampolines);
        completed_trampolines = NULL;
    }

    is_method = g_callable_info_is_method(function->info);
    can_throw_gerror = g_callable_info_can_throw_gerror(function->info);

    c_argc = function->invoker.cif.nargs;
    gi_argc = g_callable_info_get_n_args( (GICallableInfo*) function->info);

    /* @c_argc is the number of arguments that the underlying C
     * function takes. @gi_argc is the number of arguments the
     * GICallableInfo describes (which does not include "this" or
     * GError**). @function->expected_js_argc is the number of
     * arguments we expect the JS function to take (which does not
     * include PARAM_SKIPPED args).
     *
     * @js_argc is the number of arguments that were actually passed;
     * we allow this to be larger than @expected_js_argc for
     * convenience, and simply ignore the extra arguments. But we
     * don't allow too few args, since that would break.
     */

    if (js_argc < function->expected_js_argc) {
        gwkjs_throw(context, "Too few arguments to %s %s.%s expected %d got %d",
                  is_method ? "method" : "function",
                  g_base_info_get_namespace( (GIBaseInfo*) function->info),
                  g_base_info_get_name( (GIBaseInfo*) function->info),
                  function->expected_js_argc,
                  js_argc);
        return JS_FALSE;
    }

    g_callable_info_load_return_type( (GICallableInfo*) function->info, &return_info);
    return_tag = g_type_info_get_tag(&return_info);

    in_arg_cvalues = g_newa(GArgument, c_argc);
    ffi_arg_pointers = g_newa(gpointer, c_argc);
    out_arg_cvalues = g_newa(GArgument, c_argc);
    inout_original_arg_cvalues = g_newa(GArgument, c_argc);

    failed = FALSE;
    c_arg_pos = 0; /* index into in_arg_cvalues, etc */
    js_arg_pos = 0; /* index into argv */

    if (is_method) {
        if (!gwkjs_fill_method_instance(context, obj,
                                      function, &in_arg_cvalues[0]))
            return JS_FALSE;
        ffi_arg_pointers[0] = &in_arg_cvalues[0];
        ++c_arg_pos;
    }

    processed_c_args = c_arg_pos;
    for (gi_arg_pos = 0; gi_arg_pos < gi_argc; gi_arg_pos++, c_arg_pos++) {
        GIDirection direction;
        GIArgInfo arg_info;
        gboolean arg_removed = FALSE;

        /* gwkjs_debug(GWKJS_DEBUG_GFUNCTION, "gi_arg_pos: %d c_arg_pos: %d js_arg_pos: %d", gi_arg_pos, c_arg_pos, js_arg_pos); */

        g_callable_info_load_arg( (GICallableInfo*) function->info, gi_arg_pos, &arg_info);
        direction = g_arg_info_get_direction(&arg_info);

        g_assert_cmpuint(c_arg_pos, <, c_argc);
        ffi_arg_pointers[c_arg_pos] = &in_arg_cvalues[c_arg_pos];

        if (direction == GI_DIRECTION_OUT) {
            if (g_arg_info_is_caller_allocates(&arg_info)) {
                GITypeTag type_tag;
                GITypeInfo ainfo;

                g_arg_info_load_type(&arg_info, &ainfo);
                type_tag = g_type_info_get_tag(&ainfo);

                switch (type_tag) {
                case GI_TYPE_TAG_INTERFACE:
                    {
                        GIBaseInfo* interface_info;
                        GIInfoType interface_type;
                        gsize size;

                        interface_info = g_type_info_get_interface(&ainfo);
                        g_assert(interface_info != NULL);

                        interface_type = g_base_info_get_type(interface_info);

                        if (interface_type == GI_INFO_TYPE_STRUCT) {
                            size = g_struct_info_get_size((GIStructInfo*)interface_info);
                        } else if (interface_type == GI_INFO_TYPE_UNION) {
                            size = g_union_info_get_size((GIUnionInfo*)interface_info);
                        } else {
                            failed = TRUE;
                        }

                        g_base_info_unref((GIBaseInfo*)interface_info);

                        if (!failed) {
                            in_arg_cvalues[c_arg_pos].v_pointer = g_slice_alloc0(size);
                            out_arg_cvalues[c_arg_pos].v_pointer = in_arg_cvalues[c_arg_pos].v_pointer;
                        }
                        break;
                    }
                default:
                    failed = TRUE;
                }
                if (failed)
                    gwkjs_throw(context, "Unsupported type %s for (out caller-allocates)", g_type_tag_to_string(type_tag));
            } else {
                out_arg_cvalues[c_arg_pos].v_pointer = NULL;
                in_arg_cvalues[c_arg_pos].v_pointer = &out_arg_cvalues[c_arg_pos];
            }
        } else {
            GArgument *in_value;
            GITypeInfo ainfo;
            GwkjsParamType param_type;

            g_arg_info_load_type(&arg_info, &ainfo);

            in_value = &in_arg_cvalues[c_arg_pos];

            param_type = function->param_types[gi_arg_pos];

            switch (param_type) {
            case PARAM_CALLBACK: {
                GICallableInfo *callable_info;
                GIScopeType scope = g_arg_info_get_scope(&arg_info);
                GwkjsCallbackTrampoline *trampoline;
                ffi_closure *closure;
                jsval value = js_argv[js_arg_pos];

                if (JSVAL_IS_NULL(context, value) && g_arg_info_may_be_null(&arg_info)) {
                    closure = NULL;
                    trampoline = NULL;
                } else {
                    if (!GWKJS_VALUE_IS_FUNCTION(context, value)) {
                        gwkjs_throw(context, "Error invoking %s.%s: Expected function for callback argument %s, got %s",
                                  g_base_info_get_namespace( (GIBaseInfo*) function->info),
                                  g_base_info_get_name( (GIBaseInfo*) function->info),
                                  g_base_info_get_name( (GIBaseInfo*) &arg_info),
                                  gwkjs_get_type_name(context, value));
                        failed = TRUE;
                        break;
                    }

                    callable_info = (GICallableInfo*) g_type_info_get_interface(&ainfo);
                    trampoline = gwkjs_callback_trampoline_new(context,
                                                             value,
                                                             callable_info,
                                                             scope,
                                                             FALSE);
                    closure = trampoline->closure;
                    g_base_info_unref(callable_info);
                }

                gint destroy_pos = g_arg_info_get_destroy(&arg_info);
                gint closure_pos = g_arg_info_get_closure(&arg_info);
                if (destroy_pos >= 0) {
                    gint c_pos = is_method ? destroy_pos + 1 : destroy_pos;
                    g_assert (function->param_types[destroy_pos] == PARAM_SKIPPED);
                    in_arg_cvalues[c_pos].v_pointer = trampoline ? (gpointer) gwkjs_destroy_notify_callback : NULL;
                }
                if (closure_pos >= 0) {
                    gint c_pos = is_method ? closure_pos + 1 : closure_pos;
                    g_assert (function->param_types[closure_pos] == PARAM_SKIPPED);
                    in_arg_cvalues[c_pos].v_pointer = trampoline;
                }

                if (trampoline && scope != GI_SCOPE_TYPE_CALL) {
                    /* Add an extra reference that will be cleared when collecting
                       async calls, or when GDestroyNotify is called */
                    gwkjs_callback_trampoline_ref(trampoline);
                }
                in_value->v_pointer = closure;
                break;
            }
            case PARAM_SKIPPED:
                arg_removed = TRUE;
                break;
            case PARAM_ARRAY: {
                GIArgInfo array_length_arg;

                gint array_length_pos = g_type_info_get_array_length(&ainfo);
                gsize length;

                if (!gwkjs_value_to_explicit_array(context, js_argv[js_arg_pos], &arg_info,
                                                 in_value, &length)) {
                    failed = TRUE;
                    break;
                }

                g_callable_info_load_arg(function->info, array_length_pos, &array_length_arg);

                array_length_pos += is_method ? 1 : 0;
                if (!gwkjs_value_to_arg(context,
                                        gwkjs_int_to_jsvalue(context, length),
                                        &array_length_arg,
                                        in_arg_cvalues + array_length_pos)) {
                    failed = TRUE;
                    break;
                }
                /* Also handle the INOUT for the length here */
                if (direction == GI_DIRECTION_INOUT) {
                    if (in_value->v_pointer == NULL) {
                        /* Special case where we were given JS null to
                         * also pass null for length, and not a
                         * pointer to an integer that derefs to 0.
                         */
                        in_arg_cvalues[array_length_pos].v_pointer = NULL;
                        out_arg_cvalues[array_length_pos].v_pointer = NULL;
                        inout_original_arg_cvalues[array_length_pos].v_pointer = NULL;
                    } else {
                        out_arg_cvalues[array_length_pos] = inout_original_arg_cvalues[array_length_pos] = *(in_arg_cvalues + array_length_pos);
                        in_arg_cvalues[array_length_pos].v_pointer = &out_arg_cvalues[array_length_pos];
                    }
                }
                break;
            }
            case PARAM_NORMAL:
                /* Ok, now just convert argument normally */
                g_assert_cmpuint(js_arg_pos, <, js_argc);
                if (!gwkjs_value_to_arg(context, js_argv[js_arg_pos], &arg_info,
                                      in_value)) {
                    failed = TRUE;
                    break;
                }
            }

            if (direction == GI_DIRECTION_INOUT && !arg_removed && !failed) {
                out_arg_cvalues[c_arg_pos] = inout_original_arg_cvalues[c_arg_pos] = in_arg_cvalues[c_arg_pos];
                in_arg_cvalues[c_arg_pos].v_pointer = &out_arg_cvalues[c_arg_pos];
            }

            if (failed) {
                /* Exit from the loop */
                break;
            }

            if (!failed && !arg_removed)
                ++js_arg_pos;
        }

        if (failed)
            break;

        processed_c_args++;
    }

    /* Did argument conversion fail?  In that case, skip invocation and jump to release
     * processing. */
    if (failed) {
        did_throw_gerror = FALSE;
        goto release;
    }

    if (can_throw_gerror) {
        g_assert_cmpuint(c_arg_pos, <, c_argc);
        in_arg_cvalues[c_arg_pos].v_pointer = &local_error;
        ffi_arg_pointers[c_arg_pos] = &(in_arg_cvalues[c_arg_pos]);
        c_arg_pos++;

        /* don't update processed_c_args as we deal with local_error
         * separately */
    }

    g_assert_cmpuint(c_arg_pos, ==, c_argc);
    g_assert_cmpuint(gi_arg_pos, ==, gi_argc);

    /* See comment for GwkjsFFIReturnValue above */
    if (return_tag == GI_TYPE_TAG_FLOAT)
        return_value_p = &return_value.v_float;
    else if (return_tag == GI_TYPE_TAG_DOUBLE)
        return_value_p = &return_value.v_double;
    else if (return_tag == GI_TYPE_TAG_INT64 || return_tag == GI_TYPE_TAG_UINT64)
        return_value_p = &return_value.v_uint64;
    else
        return_value_p = &return_value.v_long;
    ffi_call(&(function->invoker.cif), FFI_FN(function->invoker.native_address), return_value_p, ffi_arg_pointers);

    /* Return value and out arguments are valid only if invocation doesn't
     * return error. In arguments need to be released always.
     */
    if (can_throw_gerror) {
        did_throw_gerror = local_error != NULL;
    } else {
        did_throw_gerror = FALSE;
    }

    if (js_rval)
        *js_rval = JSValueMakeUndefined(context);

    /* Only process return values if the function didn't throw */
    if (function->js_out_argc > 0 && !did_throw_gerror) {
        return_values = g_newa(jsval, function->js_out_argc);
        gwkjs_set_values(context, return_values, function->js_out_argc, kJSTypeUndefined);
// TODO: I guess the following is not necessary
//        gwkjs_root_value_locations(context, return_values, function->js_out_argc);

        if (return_tag != GI_TYPE_TAG_VOID) {
            GITransfer transfer = g_callable_info_get_caller_owns((GICallableInfo*) function->info);
            gboolean arg_failed = FALSE;
            gint array_length_pos;

            g_assert_cmpuint(next_rval, <, function->js_out_argc);

            gi_type_info_extract_ffi_return_value(&return_info, &return_value, &return_gargument);

            array_length_pos = g_type_info_get_array_length(&return_info);
            if (array_length_pos >= 0) {
                GIArgInfo array_length_arg;
                GITypeInfo arg_type_info;
                JSValueRef length = NULL;

                g_callable_info_load_arg(function->info, array_length_pos, &array_length_arg);
                g_arg_info_load_type(&array_length_arg, &arg_type_info);
                array_length_pos += is_method ? 1 : 0;
                arg_failed = !gwkjs_value_from_g_argument(context, &length,
                                                        &arg_type_info,
                                                        &out_arg_cvalues[array_length_pos],
                                                        TRUE);
                if (!arg_failed && js_rval) {
                    arg_failed = !gwkjs_value_from_explicit_array(context,
                                                                &return_values[next_rval],
                                                                &return_info,
                                                                &return_gargument,
                                                                gwkjs_jsvalue_to_int(context, length, NULL));
                }
                if (!arg_failed &&
                    !r_value &&
                    !gwkjs_g_argument_release_out_array(context,
                                                      transfer,
                                                      &return_info,
                                                      gwkjs_jsvalue_to_int(context, length, NULL),
                                                      &return_gargument))
                    failed = TRUE;
            } else {
                if (js_rval)
                    arg_failed = !gwkjs_value_from_g_argument(context, &return_values[next_rval],
                                                            &return_info, &return_gargument,
                                                            TRUE);
                /* Free GArgument, the jsval should have ref'd or copied it */
                if (!arg_failed &&
                    !r_value &&
                    !gwkjs_g_argument_release(context,
                                            transfer,
                                            &return_info,
                                            &return_gargument))
                    failed = TRUE;
            }
            if (arg_failed)
                failed = TRUE;

            ++next_rval;
        }
    }

release:
    /* We walk over all args, release in args (if allocated) and convert
     * all out args to JS
     */
    c_arg_pos = is_method ? 1 : 0;
    postinvoke_release_failed = FALSE;
    for (gi_arg_pos = 0; gi_arg_pos < gi_argc && c_arg_pos < processed_c_args; gi_arg_pos++, c_arg_pos++) {
        GIDirection direction;
        GIArgInfo arg_info;
        GITypeInfo arg_type_info;
        GwkjsParamType param_type;

        g_callable_info_load_arg( (GICallableInfo*) function->info, gi_arg_pos, &arg_info);
        direction = g_arg_info_get_direction(&arg_info);

        g_arg_info_load_type(&arg_info, &arg_type_info);
        param_type = function->param_types[gi_arg_pos];

        if (direction == GI_DIRECTION_IN || direction == GI_DIRECTION_INOUT) {
            GArgument *arg;
            GITransfer transfer;

            if (direction == GI_DIRECTION_IN) {
                arg = &in_arg_cvalues[c_arg_pos];
                transfer = g_arg_info_get_ownership_transfer(&arg_info);
            } else {
                arg = &inout_original_arg_cvalues[c_arg_pos];
                /* For inout, transfer refers to what we get back from the function; for
                 * the temporary C value we allocated, clearly we're responsible for
                 * freeing it.
                 */
                transfer = GI_TRANSFER_NOTHING;
            }
            if (param_type == PARAM_CALLBACK) {
                ffi_closure *closure = (ffi_closure *) arg->v_pointer;
                if (closure) {
                    GwkjsCallbackTrampoline *trampoline = (GwkjsCallbackTrampoline *) closure->user_data;
                    /* CallbackTrampolines are refcounted because for notified/async closures
                       it is possible to destroy it while in call, and therefore we cannot check
                       its scope at this point */
                    gwkjs_callback_trampoline_unref(trampoline);
                    arg->v_pointer = NULL;
                }
            } else if (param_type == PARAM_ARRAY) {
                gsize length;
                GIArgInfo array_length_arg;
                GITypeInfo array_length_type;
                gint array_length_pos = g_type_info_get_array_length(&arg_type_info);

                g_assert(array_length_pos >= 0);

                g_callable_info_load_arg(function->info, array_length_pos, &array_length_arg);
                g_arg_info_load_type(&array_length_arg, &array_length_type);

                array_length_pos += is_method ? 1 : 0;

                length = get_length_from_arg(in_arg_cvalues + array_length_pos,
                                             g_type_info_get_tag(&array_length_type));

                if (!gwkjs_g_argument_release_in_array(context,
                                                     transfer,
                                                     &arg_type_info,
                                                     length,
                                                     arg)) {
                    postinvoke_release_failed = TRUE;
                }
            } else if (param_type == PARAM_NORMAL) {
                if (!gwkjs_g_argument_release_in_arg(context,
                                                   transfer,
                                                   &arg_type_info,
                                                   arg)) {
                    postinvoke_release_failed = TRUE;
                }
            }
        }

        /* Don't free out arguments if function threw an exception or we failed
         * earlier - note "postinvoke_release_failed" is separate from "failed".  We
         * sync them up after this loop.
         */
        if (did_throw_gerror || failed)
            continue;

        if ((direction == GI_DIRECTION_OUT || direction == GI_DIRECTION_INOUT) && param_type != PARAM_SKIPPED) {
            GArgument *arg;
            gboolean arg_failed = FALSE;
            gint array_length_pos;
            JSValueRef array_length = NULL;
            GITransfer transfer;

            g_assert(next_rval < function->js_out_argc);

            arg = &out_arg_cvalues[c_arg_pos];

            array_length_pos = g_type_info_get_array_length(&arg_type_info);

            if (js_rval) {
                if (array_length_pos >= 0) {
                    GIArgInfo array_length_arg;
                    GITypeInfo array_length_type_info;

                    g_callable_info_load_arg(function->info, array_length_pos, &array_length_arg);
                    g_arg_info_load_type(&array_length_arg, &array_length_type_info);
                    array_length_pos += is_method ? 1 : 0;
                    arg_failed = !gwkjs_value_from_g_argument(context, &array_length,
                                                            &array_length_type_info,
                                                            &out_arg_cvalues[array_length_pos],
                                                            TRUE);
                    if (!arg_failed) {
                        arg_failed = !gwkjs_value_from_explicit_array(context,
                                                                    &return_values[next_rval],
                                                                    &arg_type_info,
                                                                    arg,
                                                                    gwkjs_jsvalue_to_int(context, array_length, NULL));
                    }
                } else {
                    arg_failed = !gwkjs_value_from_g_argument(context,
                                                            &return_values[next_rval],
                                                            &arg_type_info,
                                                            arg,
                                                            TRUE);
                }
            }

            if (arg_failed)
                postinvoke_release_failed = TRUE;

            /* For caller-allocates, what happens here is we allocate
             * a structure above, then gwkjs_value_from_g_argument calls
             * g_boxed_copy on it, and takes ownership of that.  So
             * here we release the memory allocated above.  It would be
             * better to special case this and directly hand JS the boxed
             * object and tell gwkjs_boxed it owns the memory, but for now
             * this works OK.  We could also alloca() the structure instead
             * of slice allocating.
             */
            if (g_arg_info_is_caller_allocates(&arg_info)) {
                GITypeTag type_tag;
                GIBaseInfo* interface_info;
                GIInfoType interface_type;
                gsize size;

                type_tag = g_type_info_get_tag(&arg_type_info);
                g_assert(type_tag == GI_TYPE_TAG_INTERFACE);
                interface_info = g_type_info_get_interface(&arg_type_info);
                interface_type = g_base_info_get_type(interface_info);
                if (interface_type == GI_INFO_TYPE_STRUCT) {
                    size = g_struct_info_get_size((GIStructInfo*)interface_info);
                } else if (interface_type == GI_INFO_TYPE_UNION) {
                    size = g_union_info_get_size((GIUnionInfo*)interface_info);
                } else {
                    g_assert_not_reached();
                }

                g_slice_free1(size, out_arg_cvalues[c_arg_pos].v_pointer);
                g_base_info_unref((GIBaseInfo*)interface_info);
            }

            /* Free GArgument, the jsval should have ref'd or copied it */
            transfer = g_arg_info_get_ownership_transfer(&arg_info);
            if (!arg_failed) {
                if (array_length_pos >= 0) {
                    gwkjs_g_argument_release_out_array(context,
                                                     transfer,
                                                     &arg_type_info,
                                                     gwkjs_jsvalue_to_int(context, array_length, NULL),
                                                     arg);
                } else {
                    gwkjs_g_argument_release(context,
                                           transfer,
                                           &arg_type_info,
                                           arg);
                }
            }

            ++next_rval;
        }
    }

    if (postinvoke_release_failed)
        failed = TRUE;

    g_assert(failed || did_throw_gerror || next_rval == (guint8)function->js_out_argc);
    g_assert_cmpuint(c_arg_pos, ==, processed_c_args);

    if (function->js_out_argc > 0 && (!failed && !did_throw_gerror)) {
        /* if we have 1 return value or out arg, return that item
         * on its own, otherwise return a JavaScript array with
         * [return value, out arg 1, out arg 2, ...]
         */
        if (js_rval) {
            if (function->js_out_argc == 1) {
                *js_rval = return_values[0];
            } else {
                JSObjectRef array;
                array = JSObjectMakeArray(context,
                                          function->js_out_argc,
                                          return_values, NULL);
                if (array == NULL) {
                    failed = TRUE;
                } else {
                    *js_rval = array;
                }
            }
        }

// TODO: I guess the following is not necessary
//        gwkjs_unroot_value_locations(context, return_values, function->js_out_argc);

// FIXME: Shouldn't we free return_values?!

        if (r_value) {
            *r_value = return_gargument;
        }
    }

    if (!failed && did_throw_gerror) {
// TODO: Error handling sucks here!
        gwkjs_throw_g_error(context, local_error);
        return JS_FALSE;
    } else if (failed) {
        return JS_FALSE;
    } else {
        return JS_TRUE;
    }
}

static JSValueRef
function_call(JSContextRef context,
              JSObjectRef callee,
              JSObjectRef object,
              size_t js_argc,
              const JSValueRef arguments[],
              JSValueRef* exception)
{
    JSBool success;
    Function *priv;
    JSValueRef retval = NULL;

    priv = priv_from_js(callee);
    gwkjs_debug_marshal(GWKJS_DEBUG_GFUNCTION,
                        "Call callee %p priv %p this obj %p %s", callee, priv,
                        object,
                        JS_GetTypeName(context, JS_TypeOfValue(context, OBJECT_TO_JSVAL(object))));

    if (priv == NULL)
        return NULL; /* we are the prototype, or have the wrong class */


    success = gwkjs_invoke_c_function(context, priv, object, js_argc, arguments, &retval, NULL);
    if (success)
        return retval;

    g_warning("Something went wrong on calling funciton_call");
    return NULL;
}

GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(function)

/* Does not actually free storage for structure, just
 * reverses init_cached_function_data
 */
static void
uninit_cached_function_data (Function *function)
{
    if (function->info)
        g_base_info_unref( (GIBaseInfo*) function->info);
    if (function->param_types)
        g_free(function->param_types);

    g_function_invoker_destroy(&function->invoker);
}

static void
function_finalize(JSObjectRef obj)
{
    Function *priv;

    priv = (Function *) JSObjectGetPrivate(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GFUNCTION,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* we are the prototype, not a real instance, so constructor never called */

    uninit_cached_function_data(priv);

    GWKJS_DEC_COUNTER(function);
    g_slice_free(Function, priv);
}

static JSValueRef
get_num_arguments (JSContextRef context,
                   JSObjectRef obj,
                   JSStringRef propertyName,
                   JSValueRef* exception)
{
    int n_args, n_jsargs, i;
    jsval retval;
    Function *priv = NULL;

    priv = priv_from_js(obj);

    if (priv == NULL)
        return NULL;

    n_args = g_callable_info_get_n_args(priv->info);
    n_jsargs = 0;
    for (i = 0; i < n_args; i++) {
        GIArgInfo arg_info;

        if (priv->param_types[i] == PARAM_SKIPPED)
            continue;

        g_callable_info_load_arg(priv->info, i, &arg_info);

        if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_OUT)
            continue;
    }

    retval = gwkjs_int_to_jsvalue(context, n_jsargs);
    return retval;
}

static JSValueRef
function_to_string (JSContextRef context,
                    JSObjectRef function,
                    JSObjectRef self,
                    size_t argumentCount,
                    const JSValueRef arguments[],
                    JSValueRef* exception)
{
    Function *priv;
    gchar *string;
    gboolean free;
    JSValueRef retval = NULL;
    JSBool ret = JS_FALSE;
    int i, n_args, n_jsargs;
    GString *arg_names_str;
    gchar *arg_names;

    if (!self) {
        gwkjs_throw(context, "this cannot be null");
        return NULL;
    }

    priv = priv_from_js (self);
    if (priv == NULL) {
        string = (gchar *) "function () {\n}";
        free = FALSE;
        goto out;
    }

    free = TRUE;

    n_args = g_callable_info_get_n_args(priv->info);
    n_jsargs = 0;
    arg_names_str = g_string_new("");
    for (i = 0; i < n_args; i++) {
        GIArgInfo arg_info;

        if (priv->param_types[i] == PARAM_SKIPPED)
            continue;

        g_callable_info_load_arg(priv->info, i, &arg_info);

        if (g_arg_info_get_direction(&arg_info) == GI_DIRECTION_OUT)
            continue;

        if (n_jsargs > 0)
            g_string_append(arg_names_str, ", ");

        n_jsargs++;
        g_string_append(arg_names_str, g_base_info_get_name(&arg_info));
    }
    arg_names = g_string_free(arg_names_str, FALSE);

    if (g_base_info_get_type(priv->info) == GI_INFO_TYPE_FUNCTION) {
        string = g_strdup_printf("function %s(%s) {\n\t/* proxy for native symbol %s(); */\n}",
                                 g_base_info_get_name ((GIBaseInfo *) priv->info),
                                 arg_names,
                                 g_function_info_get_symbol ((GIFunctionInfo *) priv->info));
    } else {
        string = g_strdup_printf("function %s(%s) {\n\t/* proxy for native symbol */\n}",
                                 g_base_info_get_name ((GIBaseInfo *) priv->info),
                                 arg_names);
    }

    g_free(arg_names);

 out:
    gwkjs_cstring_to_jsstring(string);
    gwkjs_string_from_utf8(context, string, -1, &retval);

    if (free)
        g_free(string);
    return retval;
}

JSStaticValue gwkjs_function_proto_props[] = {
    { "length",
      get_num_arguments,
      NULL,
      kJSPropertyAttributeReadOnly |
      kJSPropertyAttributeDontEnum |
      kJSPropertyAttributeDontDelete,
    },
    { 0,0,0,0 }
};

/* The original Function.prototype.toString complains when
   given a GIRepository function as an argument */
JSStaticFunction gwkjs_function_proto_funcs[] = {
    { "toString", function_to_string, kJSPropertyAttributeNone },
    {0,0,0}
};

/* The bizarre thing about this vtable is that it applies to both
 * instances of the object, and to the prototype that instances of the
 * class have.
 */

JSClassDefinition gwkjs_function_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GIRepositoryFunction",    //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    gwkjs_function_proto_props,//     const JSStaticValue*                staticValues;
    gwkjs_function_proto_funcs,//     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    function_finalize,         //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;
    NULL,                      //     JSObjectGetPropertyCallback         getProperty;
    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    function_call,             //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_function_constructor,//     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};



static gboolean
init_cached_function_data (JSContextRef      context,
                           Function       *function,
                           GType           gtype,
                           GICallableInfo *info)
{
    guint8 i, n_args;
    int array_length_pos;
    GError *error = NULL;
    GITypeInfo return_type;
    GIInfoType info_type;

    info_type = g_base_info_get_type((GIBaseInfo *)info);

    if (info_type == GI_INFO_TYPE_FUNCTION) {
        if (!g_function_info_prep_invoker((GIFunctionInfo *)info,
                                          &(function->invoker),
                                          &error)) {
            gwkjs_throw_g_error(context, error);
            return FALSE;
        }
    } else if (info_type == GI_INFO_TYPE_VFUNC) {
        gpointer addr;

        addr = g_vfunc_info_get_address((GIVFuncInfo *)info, gtype, &error);
        if (error != NULL) {
            if (error->code != G_INVOKE_ERROR_SYMBOL_NOT_FOUND)
                gwkjs_throw_g_error(context, error);

            g_clear_error(&error);
            return FALSE;
        }

        if (!g_function_invoker_new_for_address(addr, info,
                                                &(function->invoker),
                                                &error)) {
            gwkjs_throw_g_error(context, error);
            return FALSE;
        }
    }

    g_callable_info_load_return_type((GICallableInfo*)info, &return_type);
    if (g_type_info_get_tag(&return_type) != GI_TYPE_TAG_VOID)
        function->js_out_argc += 1;

    n_args = g_callable_info_get_n_args((GICallableInfo*) info);
    function->param_types = g_new0(GwkjsParamType, n_args);

    array_length_pos = g_type_info_get_array_length(&return_type);
    if (array_length_pos >= 0 && array_length_pos < n_args)
        function->param_types[array_length_pos] = PARAM_SKIPPED;

    for (i = 0; i < n_args; i++) {
        GIDirection direction;
        GIArgInfo arg_info;
        GITypeInfo type_info;
        int destroy = -1;
        int closure = -1;
        GITypeTag type_tag;

        if (function->param_types[i] == PARAM_SKIPPED)
            continue;

        g_callable_info_load_arg((GICallableInfo*) info, i, &arg_info);
        g_arg_info_load_type(&arg_info, &type_info);

        direction = g_arg_info_get_direction(&arg_info);
        type_tag = g_type_info_get_tag(&type_info);

        if (type_tag == GI_TYPE_TAG_INTERFACE) {
            GIBaseInfo* interface_info;
            GIInfoType interface_type;

            interface_info = g_type_info_get_interface(&type_info);
            interface_type = g_base_info_get_type(interface_info);
            if (interface_type == GI_INFO_TYPE_CALLBACK) {
                if (strcmp(g_base_info_get_name(interface_info), "DestroyNotify") == 0 &&
                    strcmp(g_base_info_get_namespace(interface_info), "GLib") == 0) {
                    /* Skip GDestroyNotify if they appear before the respective callback */
                    function->param_types[i] = PARAM_SKIPPED;
                } else {
                    function->param_types[i] = PARAM_CALLBACK;
                    function->expected_js_argc += 1;

                    destroy = g_arg_info_get_destroy(&arg_info);
                    closure = g_arg_info_get_closure(&arg_info);

                    if (destroy >= 0 && destroy < n_args)
                        function->param_types[destroy] = PARAM_SKIPPED;

                    if (closure >= 0 && closure < n_args)
                        function->param_types[closure] = PARAM_SKIPPED;

                    if (destroy >= 0 && closure < 0) {
                        gwkjs_throw(context, "Function %s.%s has a GDestroyNotify but no user_data, not supported",
                                  g_base_info_get_namespace( (GIBaseInfo*) info),
                                  g_base_info_get_name( (GIBaseInfo*) info));
                        g_base_info_unref(interface_info);
                        return JS_FALSE;
                    }
                }
            }
            g_base_info_unref(interface_info);
        } else if (type_tag == GI_TYPE_TAG_ARRAY) {
            if (g_type_info_get_array_type(&type_info) == GI_ARRAY_TYPE_C) {
                array_length_pos = g_type_info_get_array_length(&type_info);

                if (array_length_pos >= 0 && array_length_pos < n_args) {
                    GIArgInfo length_arg_info;

                    g_callable_info_load_arg((GICallableInfo*) info, array_length_pos, &length_arg_info);
                    if (g_arg_info_get_direction(&length_arg_info) != direction) {
                        gwkjs_throw(context, "Function %s.%s has an array with different-direction length arg, not supported",
                                  g_base_info_get_namespace( (GIBaseInfo*) info),
                                  g_base_info_get_name( (GIBaseInfo*) info));
                        return JS_FALSE;
                    }

                    function->param_types[array_length_pos] = PARAM_SKIPPED;
                    function->param_types[i] = PARAM_ARRAY;

                    if (array_length_pos < i) {
                        /* we already collected array_length_pos, remove it */
                        if (direction == GI_DIRECTION_IN || direction == GI_DIRECTION_INOUT)
                            function->expected_js_argc -= 1;
                        if (direction == GI_DIRECTION_OUT || direction == GI_DIRECTION_INOUT)
                            function->js_out_argc -= 1;
                    }
                }
            }
        }

        if (function->param_types[i] == PARAM_NORMAL ||
            function->param_types[i] == PARAM_ARRAY) {
            if (direction == GI_DIRECTION_IN || direction == GI_DIRECTION_INOUT)
                function->expected_js_argc += 1;
            if (direction == GI_DIRECTION_OUT || direction == GI_DIRECTION_INOUT)
                function->js_out_argc += 1;
        }
    }

    function->info = info;

    g_base_info_ref((GIBaseInfo*) function->info);

    return JS_TRUE;
}

static JSObjectRef
function_new(JSContextRef      context,
             GType           gtype,
             GICallableInfo *info)
{
    JSObjectRef function = NULL;
    JSObjectRef global = NULL;
    JSObjectRef prototype = NULL;
    Function *priv = NULL;
    JSBool found = FALSE;
    JSValueRef exception = NULL;

    /* put constructor for GIRepositoryFunction() in the global namespace */
    global = gwkjs_get_import_global(context);

    if (!gwkjs_function_class_ref)
        gwkjs_function_class_ref = JSClassCreate(&gwkjs_function_class);

    found = gwkjs_object_has_property(context, global, gwkjs_function_class.className);
    if (!found) {
        JSValueRef parent_proto_val = NULL;
        JSValueRef parent_proto = NULL;
        JSValueRef native_function = NULL;
        JSObjectRef native_function_obj = NULL;

        native_function = gwkjs_object_get_property(context, global, "Function", &exception);
        if (exception)
            g_error ("Getting Function prototype failed!");

        native_function_obj = JSValueToObject(context, native_function, &exception);
        if (exception)
            g_error ("Couldn't find native_function Object");

        /* We take advantage from that fact that Function.__proto__ is Function.prototype */
        parent_proto_val = JSObjectGetPrototype(context, native_function_obj);
        parent_proto = JSValueToObject(context, parent_proto_val, &exception);
        if (exception)
            g_error ("Couldn't find native_function Object");

        prototype = JSObjectMake(context, gwkjs_function_class_ref, NULL);

        gwkjs_object_set_property(context, global,
                                  gwkjs_function_class.className,
                                  prototype,
                                  GWKJS_PROTO_PROP_FLAGS,
                                  &exception);
        if (prototype == NULL || exception)
            g_error("Can't init class %s", gwkjs_function_class.className);

        gwkjs_debug(GWKJS_DEBUG_GFUNCTION, "Initialized class %s prototype %p",
                  gwkjs_function_class.className, prototype);
    } else {
        JSValueRef proto_val = gwkjs_object_get_property(context,
                                                         global,
                                                         gwkjs_function_class.className,
                                                         &exception);
        if (exception) {
            g_error("Can't get protoType for class %s", gwkjs_function_class.className);
        }
        prototype = JSValueToObject(context, proto_val, 0);

    }

    function = JSObjectMake(context, gwkjs_function_class_ref, NULL);
    if (function == NULL) {
        gwkjs_debug(GWKJS_DEBUG_GFUNCTION, "Failed to construct function");
        return NULL;
    }
    JSObjectSetPrototype(context, function, prototype);

    priv = g_slice_new0(Function);

    GWKJS_INC_COUNTER(function);

    g_assert(priv_from_js(function) == NULL);
    JSObjectSetPrivate(function, priv);
    g_assert(priv_from_js(function) == priv);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GFUNCTION,
                        "function constructor, obj %p priv %p", function, priv);

    if (!init_cached_function_data(context, priv, gtype, (GICallableInfo *)info))
      return NULL;

    return function;
}

JSObjectRef
gwkjs_define_function(JSContextRef      context,
                    JSObjectRef       in_object,
                    GType           gtype,
                    GICallableInfo *info)
{
    JSObjectRef function = NULL;
    JSValueRef exception = NULL;
    GIInfoType info_type;
    gchar *name = NULL;
    gboolean free_name;

    info_type = g_base_info_get_type((GIBaseInfo *)info);


    function = function_new(context, gtype, info);
    if (function == NULL) {
        gwkjs_move_exception(context, context);
        goto out;
    }

    if (info_type == GI_INFO_TYPE_FUNCTION) {
        name = (gchar *) g_base_info_get_name((GIBaseInfo*) info);
        free_name = FALSE;
    } else if (info_type == GI_INFO_TYPE_VFUNC) {
        name = g_strdup_printf("vfunc_%s", g_base_info_get_name((GIBaseInfo*) info));
        free_name = TRUE;
    } else {
        g_assert_not_reached ();
    }

    if (!gwkjs_object_set_property(context, in_object, name,
                           function,
                           kJSPropertyAttributeDontDelete, &exception)) {
        gwkjs_debug(GWKJS_DEBUG_GFUNCTION, "Failed to define function %s", gwkjs_exception_to_string(context, exception));
        function = NULL;
    }

    if (free_name)
        g_free(name);

 out:
    return function;
}


JSBool
gwkjs_invoke_c_function_uncached (JSContextRef      context,
                                GIFunctionInfo *info,
                                JSObjectRef       obj,
                                unsigned        argc,
                                jsval          *argv,
                                jsval          *rval)
{
  Function function;
  JSBool result;

  memset (&function, 0, sizeof (Function));
  if (!init_cached_function_data (context, &function, 0, info))
    return JS_FALSE;

  result = gwkjs_invoke_c_function (context, &function, obj, argc, argv, rval, NULL);
  uninit_cached_function_data (&function);
  return result;
}

JSBool
gwkjs_invoke_constructor_from_c (JSContextRef      context,
                                 JSObjectRef       constructor,
                                 JSObjectRef       obj,
                                 unsigned        argc,
                                 jsval          *argv,
                                 GArgument      *rvalue)
{
    Function *priv;

    priv = priv_from_js(constructor);

    return gwkjs_invoke_c_function(context, priv, obj, argc, argv, NULL, rvalue);
}
