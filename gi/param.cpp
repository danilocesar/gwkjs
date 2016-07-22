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

#include <string.h>

#include "param.h"
#include "arg.h"
#include "object.h"
#include "repo.h"
#include "gtype.h"
#include "function.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>

#include <util/log.h>

typedef struct {
    GParamSpec *gparam; /* NULL if we are the prototype and not an instance */
} Param;

extern JSClassDefinition gwkjs_param_class;
static JSClassRef gwkjs_param_class_ref = NULL;

GWKJS_DEFINE_PRIV_FROM_JS(Param, gwkjs_param_class)


static JSBool
param_new_resolve(JSContextRef context,
                  JSObjectRef obj,
                  const gchar *name,
                  JSObjectRef *objp,
                  JSValueRef *exception)
{
    GIObjectInfo *info = NULL;
    GIFunctionInfo *method_info;
    Param *priv;
    JSBool ret = JS_FALSE;

    priv = priv_from_js(obj);
    *objp = NULL;

    if (priv != NULL) {
        /* instance, not prototype */
        ret = JS_TRUE;
        goto out;
    }

    info = (GIObjectInfo*)g_irepository_find_by_gtype(g_irepository_get_default(), G_TYPE_PARAM);
    method_info = g_object_info_find_method(info, name);

    if (method_info == NULL) {
        ret = JS_TRUE;
        goto out;
    }
#if GWKJS_VERBOSE_ENABLE_GI_USAGE
    _gwkjs_log_info_usage((GIBaseInfo*) method_info);
#endif

    if (g_function_info_get_flags (method_info) & GI_FUNCTION_IS_METHOD) {
        gwkjs_debug(GWKJS_DEBUG_GOBJECT,
                  "Defining method %s in prototype for GObject.ParamSpec",
                  g_base_info_get_name( (GIBaseInfo*) method_info));

        *objp = gwkjs_define_function(context, obj, G_TYPE_PARAM, method_info);
        if (*objp == NULL) {
            g_base_info_unref( (GIBaseInfo*) method_info);
            goto out;
        }
    }

    g_base_info_unref( (GIBaseInfo*) method_info);

    ret = JS_TRUE;
 out:
    if (info != NULL)
        g_base_info_unref( (GIBaseInfo*)info);

    return ret;
}
static JSValueRef
param_get_prop(JSContextRef context,
               JSObjectRef obj,
               JSStringRef property_name,
               JSValueRef* exception)
{
    JSObjectRef ret = NULL;
    Param *priv = priv_from_js(obj);
    if (priv == NULL)
        return NULL;

    gchar *c_property_name = gwkjs_jsstring_to_cstring(property_name);
    JSBool did_resolve = param_new_resolve(context, obj, c_property_name, &ret, exception);

    g_free(c_property_name);

    return ret;
}



GWKJS_NATIVE_CONSTRUCTOR_DECLARE(param)
{
    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(param)
    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(param);
    GWKJS_INC_COUNTER(param);
    GWKJS_NATIVE_CONSTRUCTOR_FINISH(param);
}

static void
param_finalize(JSObjectRef obj)
{
    Param *priv;

    priv = (Param*) JSObjectGetPrivate(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GPARAM,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* wrong class? */

    if (priv->gparam) {
        g_param_spec_unref(priv->gparam);
        priv->gparam = NULL;
    }

    GWKJS_DEC_COUNTER(param);
    g_slice_free(Param, priv);
}

JSStaticValue gwkjs_param_proto_props[] = {
    { NULL, NULL, NULL, 0 }
};

JSStaticFunction gwkjs_param_proto_funcs[] = {
    { NULL }
};


JSClassDefinition gwkjs_param_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GObject_ParamSpec",       //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    gwkjs_param_proto_props,   //     const JSStaticValue*                staticValues;
    gwkjs_param_proto_funcs,   //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    param_finalize,            //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;
    param_get_prop,            //TODO: is this really resolve?
                               //     JSObjectGetPropertyCallback         getProperty;

    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_param_constructor,//     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;

};

static JSObjectRef
gwkjs_lookup_param_prototype(JSContextRef    context)
{
    JSObjectRef in_object;
    JSObjectRef constructor;
    jsval value = NULL;
    JSValueRef exception = NULL;

    in_object = gwkjs_lookup_namespace_object_by_name(context, "GObject");

    if (G_UNLIKELY (!in_object))
        return NULL;

    value = gwkjs_object_get_property(context, in_object, "ParamSpec", &exception);
    if (exception)
        return NULL;

    if (G_UNLIKELY (!JSValueIsObject(context, value) || JSVAL_IS_NULL(context, value)))
        return NULL;

    constructor = JSValueToObject(context, value, NULL);
    g_assert(constructor != NULL);

//TODO: Reusing the variable. I'm not sure if GJS
// clears the var internally. So I'm doing it here.
// Might be the root cause of issues
    value = NULL;
    if (!gwkjs_object_get_property_const(context, constructor,
                                         GWKJS_STRING_PROTOTYPE, &value))
        return NULL;

    if (G_UNLIKELY (!JSValueIsObject(context, value)))
        return NULL;

    return JSValueToObject(context, value, NULL);
}

JSObjectRef
gwkjs_define_param_class(JSContextRef    context,
                       JSObjectRef     in_object)
{
    const char *constructor_name;
    JSObjectRef prototype = NULL;
    JSObjectRef value = NULL;
    JSObjectRef constructor = NULL;
    GIObjectInfo *info = NULL;

    constructor_name = "ParamSpec";

    if (!gwkjs_param_class_ref) {
        gwkjs_param_class_ref = JSClassCreate(&gwkjs_param_class);
        JSClassRetain(gwkjs_param_class_ref);
    }

    if (!gwkjs_init_class_dynamic(context, in_object,
                                  NULL,
                                  "GObject",
                                  constructor_name,
                                  &gwkjs_param_class,
                                  gwkjs_param_class_ref,
                                  gwkjs_param_constructor, 0,
                                  &prototype,
                                  &constructor)) {
        g_error("Can't init class %s", constructor_name);
    }

    value = gwkjs_gtype_create_gtype_wrapper(context, G_TYPE_PARAM);
    gwkjs_object_set_property(context, constructor, "$gtype", value,
                      kJSPropertyAttributeDontDelete, NULL);

    info = (GIObjectInfo*)g_irepository_find_by_gtype(g_irepository_get_default(), G_TYPE_PARAM);
    gwkjs_object_define_static_methods(context, constructor, G_TYPE_PARAM, info);
    g_base_info_unref( (GIBaseInfo*) info);

    gwkjs_debug(GWKJS_DEBUG_GPARAM, "Defined class %s prototype is %p class %p in object %p",
                constructor_name, prototype, JSObjectGetClass(context, prototype), in_object);

//TODO: Should we return the constructor or the value?
    return value;
}

JSObjectRef
gwkjs_param_from_g_param(JSContextRef    context,
                       GParamSpec   *gparam)
{
    JSObjectRef obj = NULL;
    JSObjectRef proto = NULL;
    Param *priv = NULL;

    if (gparam == NULL)
        return NULL;

    gwkjs_debug(GWKJS_DEBUG_GPARAM,
              "Wrapping %s '%s' on %s with JSObject",
              g_type_name(G_TYPE_FROM_INSTANCE((GTypeInstance*) gparam)),
              gparam->name,
              g_type_name(gparam->owner_type));

    proto = gwkjs_lookup_param_prototype(context);

    obj = gwkjs_new_object(context,
                          JSObjectGetClass(context, proto), proto,
                          gwkjs_get_import_global (context));

    GWKJS_INC_COUNTER(param);
    priv = g_slice_new0(Param);
    JSObjectSetPrivate(obj, priv);
    priv->gparam = gparam;
    g_param_spec_ref (gparam);

    gwkjs_debug(GWKJS_DEBUG_GPARAM,
              "JSObject created with param instance %p type %s",
              priv->gparam, g_type_name(G_TYPE_FROM_INSTANCE((GTypeInstance*) priv->gparam)));

    return obj;
}

GParamSpec*
gwkjs_g_param_from_param(JSContextRef    context,
                       JSObjectRef     obj)
{
    Param *priv;

    if (obj == NULL)
        return NULL;

    priv = priv_from_js(obj);

    return priv->gparam;
}

JSBool
gwkjs_typecheck_param(JSContextRef     context,
                    JSObjectRef      object,
                    GType          expected_type,
                    JSBool         throw_error)
{
    Param *priv;
    JSBool result;

    if (!do_base_typecheck(context, object, throw_error))
        return JS_FALSE;

    priv = priv_from_js(object);

    if (priv->gparam == NULL) {
        if (throw_error) {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is GObject.ParamSpec.prototype, not an object instance - "
                             "cannot convert to a GObject.ParamSpec instance");
        }

        return JS_FALSE;
    }

    if (expected_type != G_TYPE_NONE)
        result = g_type_is_a (G_TYPE_FROM_INSTANCE (priv->gparam), expected_type);
    else
        result = JS_TRUE;

    if (!result && throw_error) {
        gwkjs_throw_custom(context, "TypeError",
                         "Object is of type %s - cannot convert to %s",
                         g_type_name(G_TYPE_FROM_INSTANCE (priv->gparam)),
                         g_type_name(expected_type));
    }

    return result;
}
