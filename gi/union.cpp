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

/* include first for logging related #define used in repo.h */
#include <util/log.h>

#include "union.h"
#include "arg.h"
#include "object.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>
#include "repo.h"
#include "proxyutils.h"
#include "function.h"
#include "gtype.h"
#include <girepository.h>

typedef struct {
    GIUnionInfo *info;
    void *gboxed; /* NULL if we are the prototype and not an instance */
    GType gtype;
} Union;

extern JSClassDefinition gwkjs_union_class;
static JSClassRef gwkjs_union_class_ref = NULL;

GWKJS_DEFINE_PRIV_FROM_JS(Union, gwkjs_union_class)

/*
 * Like JSResolveOp, but flags provide contextual information as follows:
 *
 *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
 *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
 *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
 *  JSRESOLVE_DECLARING   var, const, or boxed prolog declaration opcode
 *  JSRESOLVE_CLASSNAME   class name used when constructing
 *
 * The *objp out parameter, on success, should be null to indicate that id
 * was not resolved; and non-null, referring to obj or one of its prototypes,
 * if id was resolved.
 */
static JSValueRef
union_get_property(JSContextRef context,
                  JSObjectRef obj,
                  JSStringRef property_name,
                  JSValueRef* exception)
{
    Union *priv = NULL;
    JSValueRef ret = NULL;
    char *name = gwkjs_jsstring_to_cstring(property_name);

    priv = priv_from_js(obj);
    gwkjs_debug_jsprop(GWKJS_DEBUG_GBOXED, "Resolve prop '%s' hook obj %p priv %p",
                     name, (void *)obj, priv);

    if (priv == NULL) {
        ret = NULL; /* wrong class */
        goto out;
    }

    if (priv->gboxed == NULL) {
        /* We are the prototype, so look for methods and other class properties */
        GIFunctionInfo *method_info;

        method_info = g_union_info_find_method((GIUnionInfo*) priv->info,
                                               name);

        if (method_info != NULL) {
            JSObjectRef union_proto = NULL;
            const char *method_name;

#if GWKJS_VERBOSE_ENABLE_GI_USAGE
            _gwkjs_log_info_usage((GIBaseInfo*) method_info);
#endif
            if (g_function_info_get_flags (method_info) & GI_FUNCTION_IS_METHOD) {
                method_name = g_base_info_get_name( (GIBaseInfo*) method_info);

                gwkjs_debug(GWKJS_DEBUG_GBOXED,
                          "Defining method %s in prototype for %s.%s",
                          method_name,
                          g_base_info_get_namespace( (GIBaseInfo*) priv->info),
                          g_base_info_get_name( (GIBaseInfo*) priv->info));

                union_proto = obj;

                if (gwkjs_define_function(context, union_proto,
                                        g_registered_type_info_get_g_type(priv->info),
                                        method_info) == NULL) {
                    g_base_info_unref( (GIBaseInfo*) method_info);
                    ret = JS_FALSE;
                    goto out;
                }

                ret = union_proto; /* we defined the prop in object_proto */
            }

            g_base_info_unref( (GIBaseInfo*) method_info);
        }
    } else {
        /* We are an instance, not a prototype, so look for
         * per-instance props that we want to define on the
         * JSObject. Generally we do not want to cache these in JS, we
         * want to always pull them from the C object, or JS would not
         * see any changes made from C. So we use the get/set prop
         * hooks, not this resolve hook.
         */
    }

 out:
    g_free(name);
    return ret;
}

static void*
union_new(JSContextRef   context,
          JSObjectRef    obj, /* "this" for constructor */
          GIUnionInfo *info)
{
    int n_methods;
    int i;

    /* Find a zero-args constructor and call it */

    n_methods = g_union_info_get_n_methods(info);

    for (i = 0; i < n_methods; ++i) {
        GIFunctionInfo *func_info;
        GIFunctionInfoFlags flags;

        func_info = g_union_info_get_method(info, i);

        flags = g_function_info_get_flags(func_info);
        if ((flags & GI_FUNCTION_IS_CONSTRUCTOR) != 0 &&
            g_callable_info_get_n_args((GICallableInfo*) func_info) == 0) {

            JSValueRef rval = NULL;

            gwkjs_invoke_c_function_uncached(context, func_info, obj,
                                           0, NULL, &rval);

            g_base_info_unref((GIBaseInfo*) func_info);

            /* We are somewhat wasteful here; invoke_c_function() above
             * creates a JSObject wrapper for the union that we immediately
             * discard.
             */
            if (JSVAL_IS_NULL(context, rval))
                return NULL;
            else
                return gwkjs_c_union_from_union(context, JSValueToObject(context, rval, NULL));
        }

        g_base_info_unref((GIBaseInfo*) func_info);
    }

    gwkjs_throw(context, "Unable to construct union type %s since it has no zero-args <constructor>, can only wrap an existing one",
              g_base_info_get_name((GIBaseInfo*) info));

    return NULL;
}

GWKJS_NATIVE_CONSTRUCTOR_DECLARE(union)
{
    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(union)
    Union *priv = NULL;
    Union *proto_priv = NULL;
    JSValueRef proto_val = NULL;
    JSObjectRef proto = NULL;
    void *gboxed;

    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(union);

    priv = g_slice_new0(Union);

    GWKJS_INC_COUNTER(boxed);

    g_assert(priv_from_js(object) == NULL);
    JSObjectSetPrivate(object, priv);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GBOXED,
                        "union constructor, obj %p priv %p",
                        object, priv);

    proto_val = JSObjectGetPrototype(context, object);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GBOXED, "union instance __proto__ is %p", proto);

    /* If we're the prototype, then post-construct we'll fill in priv->info.
     * If we are not the prototype, though, then we'll get ->info from the
     * prototype and then create a GObject if we don't have one already.
     */
    proto = JSValueToObject(context, proto_val, NULL);
    if (!proto) {
        gwkjs_debug(GWKJS_DEBUG_GBOXED,
                  "Bad prototype set on union!");
    }

    proto_priv = priv_from_js(proto);
    if (proto_priv == NULL) {
        gwkjs_debug(GWKJS_DEBUG_GBOXED,
                  "Bad prototype set on union? Must match JSClass of object. JS error should have been reported.");
        return NULL;
    }

    priv->info = proto_priv->info;
    g_base_info_ref( (GIBaseInfo*) priv->info);
    priv->gtype = proto_priv->gtype;

    /* union_new happens to be implemented by calling
     * gwkjs_invoke_c_function(), which returns a jsval.
     * The returned "gboxed" here is owned by that jsval,
     * not by us.
     */
    gboxed = union_new(context, object, priv->info);

    if (gboxed == NULL) {
        return NULL;
    }

    /* Because "gboxed" is owned by a jsval and will
     * be garbage collected, we make a copy here to be
     * owned by us.
     */
    priv->gboxed = g_boxed_copy(priv->gtype, gboxed);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GBOXED,
                        "JSObject created with union instance %p type %s",
                        priv->gboxed, g_type_name(priv->gtype));

    GWKJS_NATIVE_CONSTRUCTOR_FINISH(union);
}

static void
union_finalize(JSObjectRef obj)
{
    Union *priv;

    priv = (Union*) JSObjectGetPrivate(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GBOXED,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* wrong class? */

    if (priv->gboxed) {
        g_boxed_free(g_registered_type_info_get_g_type( (GIRegisteredTypeInfo*) priv->info),
                     priv->gboxed);
        priv->gboxed = NULL;
    }

    if (priv->info) {
        g_base_info_unref( (GIBaseInfo*) priv->info);
        priv->info = NULL;
    }

    GWKJS_DEC_COUNTER(boxed);
    g_slice_free(Union, priv);
}

static JSValueRef
to_string_func(JSContextRef context,
               JSObjectRef function,
               JSObjectRef obj,
               size_t argumentCount,
               const JSValueRef arguments[],
               JSValueRef* exception)
{
    Union *priv = NULL;
    JSValueRef retval = NULL;

    if (!priv_from_js_with_typecheck(context, obj, &priv))
        goto out;

    if (!_gwkjs_proxy_to_string_func(context, obj, "union", (GIBaseInfo*)priv->info,
                                   priv->gtype, priv->gboxed, &retval))
        goto out;

 out:
    return retval;
}

/* The bizarre thing about this vtable is that it applies to both
 * instances of the object, and to the prototype that instances of the
 * class have.
 */
JSClassDefinition gwkjs_union_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GObject_Union",           //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    NULL,                      //     const JSStaticValue*                staticValues;
    NULL,                      //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    union_finalize,            //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;
    union_get_property,        //     JSObjectGetPropertyCallback         getProperty;
    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_union_constructor,   //     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};


JSStaticValue gwkjs_union_proto_props[] = {
    { 0,0,0,0 }
};

JSStaticFunction gwkjs_union_proto_funcs[] = {
    { "toString", to_string_func, 0 },
    { NULL }
};

JSObjectRef
gwkjs_define_union_class(JSContextRef    context,
                       JSObjectRef     in_object,
                       GIUnionInfo  *info)
{
    const char *constructor_name;
    JSObjectRef prototype = NULL;
    jsval value = NULL;
    Union *priv = NULL;
    GType gtype;
    JSObjectRef constructor = NULL;

    /* For certain unions, we may be able to relax this in the future by
     * directly allocating union memory, as we do for structures in boxed.c
     */
    gtype = g_registered_type_info_get_g_type( (GIRegisteredTypeInfo*) info);
    if (gtype == G_TYPE_NONE) {
        gwkjs_throw(context, "Unions must currently be registered as boxed types");
        return JS_FALSE;
    }

    /* See the comment in gwkjs_define_object_class() for an
     * explanation of how this all works; Union is pretty much the
     * same as Object.
     */

    constructor_name = g_base_info_get_name( (GIBaseInfo*) info);

// TODO: IMPLEMENT - Not sure what to do with the dynamic class
//    if (!gwkjs_init_class_dynamic(context, in_object,
//                                NULL,
//                                g_base_info_get_namespace( (GIBaseInfo*) info),
//                                constructor_name,
//                                &gwkjs_union_class,
//                                gwkjs_union_constructor, 0,
//                                /* props of prototype */
//                                &gwkjs_union_proto_props[0],
//                                /* funcs of prototype */
//                                &gwkjs_union_proto_funcs[0],
//                                /* props of constructor, MyConstructor.myprop */
//                                NULL,
//                                /* funcs of constructor, MyConstructor.myfunc() */
//                                NULL,
//                                &prototype,
//                                &constructor)) {
//        g_error("Can't init class %s", constructor_name);
//    }

    GWKJS_INC_COUNTER(boxed);
    priv = g_slice_new0(Union);
    priv->info = info;
    g_base_info_ref( (GIBaseInfo*) priv->info);
    priv->gtype = gtype;
    JSObjectSetPrivate(prototype, priv);

//    gwkjs_debug(GWKJS_DEBUG_GBOXED, "Defined class %s prototype is %p class %p in object %p",
//              constructor_name, prototype, JS_GetClass(prototype), in_object);

    value = gwkjs_gtype_create_gtype_wrapper(context, gtype);
    gwkjs_object_set_property(context, constructor, "$gtype", value,
                              kJSPropertyAttributeDontDelete, NULL);

    return constructor;
}

JSObjectRef
gwkjs_union_from_c_union(JSContextRef    context,
                       GIUnionInfo  *info,
                       void         *gboxed)
{
    gwkjs_throw(context, " gwkjs_union_from_c_union  not implemented");
    return NULL;
// TODO: implement
//    JSObjectRef obj;
//    JSObjectRef proto;
//    Union *priv;
//    GType gtype;
//
//    if (gboxed == NULL)
//        return NULL;
//
//    /* For certain unions, we may be able to relax this in the future by
//     * directly allocating union memory, as we do for structures in boxed.c
//     */
//    gtype = g_registered_type_info_get_g_type( (GIRegisteredTypeInfo*) info);
//    if (gtype == G_TYPE_NONE) {
//        gwkjs_throw(context, "Unions must currently be registered as boxed types");
//        return NULL;
//    }
//
//    gwkjs_debug_marshal(GWKJS_DEBUG_GBOXED,
//                      "Wrapping union %s %p with JSObject",
//                      g_base_info_get_name((GIBaseInfo *)info), gboxed);
//
//    proto = gwkjs_lookup_generic_prototype(context, (GIUnionInfo*) info);
//
//    obj = JS_NewObjectWithGivenProto(context,
//                                     JS_GetClass(proto), proto,
//                                     gwkjs_get_import_global (context));
//
//    GWKJS_INC_COUNTER(boxed);
//    priv = g_slice_new0(Union);
//    JSObjectSetPrivate(obj, priv);
//    priv->info = info;
//    g_base_info_ref( (GIBaseInfo *) priv->info);
//    priv->gtype = gtype;
//    priv->gboxed = g_boxed_copy(gtype, gboxed);
//
//    return obj;
}

void*
gwkjs_c_union_from_union(JSContextRef    context,
                       JSObjectRef     obj)
{
    Union *priv;

    if (obj == NULL)
        return NULL;

    priv = priv_from_js(obj);

    return priv->gboxed;
}

JSBool
gwkjs_typecheck_union(JSContextRef     context,
                    JSObjectRef      object,
                    GIStructInfo  *expected_info,
                    GType          expected_type,
                    JSBool         throw_error)
{
    Union *priv;
    JSBool result;

    if (!do_base_typecheck(context, object, throw_error))
        return JS_FALSE;

    priv = priv_from_js(object);

    if (priv->gboxed == NULL) {
        if (throw_error) {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is %s.%s.prototype, not an object instance - cannot convert to a union instance",
                             g_base_info_get_namespace( (GIBaseInfo*) priv->info),
                             g_base_info_get_name( (GIBaseInfo*) priv->info));
        }

        return JS_FALSE;
    }

    if (expected_type != G_TYPE_NONE)
        result = g_type_is_a (priv->gtype, expected_type);
    else if (expected_info != NULL)
        result = g_base_info_equal((GIBaseInfo*) priv->info, (GIBaseInfo*) expected_info);
    else
        result = JS_TRUE;

    if (!result && throw_error) {
        if (expected_info != NULL) {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is of type %s.%s - cannot convert to %s.%s",
                             g_base_info_get_namespace((GIBaseInfo*) priv->info),
                             g_base_info_get_name((GIBaseInfo*) priv->info),
                             g_base_info_get_namespace((GIBaseInfo*) expected_info),
                             g_base_info_get_name((GIBaseInfo*) expected_info));
        } else {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is of type %s.%s - cannot convert to %s",
                             g_base_info_get_namespace((GIBaseInfo*) priv->info),
                             g_base_info_get_name((GIBaseInfo*) priv->info),
                             g_type_name(expected_type));
        }
    }

    return result;
}
