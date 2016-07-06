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
//#include <string.h>
//
//#include "param.h"
//#include "arg.h"
//#include "object.h"
//#include "repo.h"
//#include "gtype.h"
//#include "function.h"
//#include <gwkjs/gwkjs-module.h>
//#include <gwkjs/compat.h>
//
//#include <util/log.h>
//
//typedef struct {
//    GParamSpec *gparam; /* NULL if we are the prototype and not an instance */
//} Param;
//
//extern JSClassDefinition gwkjs_param_class;
//static JSClassRef gwkjs_paramclass_ref = NULL;
//
//GWKJS_DEFINE_PRIV_FROM_JS(Param, gwkjs_param_class)
//
///*
// * Like JSResolveOp, but flags provide contextual information as follows:
// *
// *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
// *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
// *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
// *  JSRESOLVE_DECLARING   var, const, or object prolog declaration opcode
// *  JSRESOLVE_CLASSNAME   class name used when constructing
// *
// * The *objp out parameter, on success, should be null to indicate that id
// * was not resolved; and non-null, referring to obj or one of its prototypes,
// * if id was resolved.
// */
//static JSBool
//param_new_resolve(JSContextRef context,
//                  JS::HandleObject obj,
//                  JS::HandleId id,
//                  unsigned flags,
//                  JS::MutableHandleObject objp)
//{
//    GIObjectInfo *info = NULL;
//    GIFunctionInfo *method_info;
//    Param *priv;
//    char *name;
//    JSBool ret = JS_FALSE;
//
//    if (!gwkjs_get_string_id(context, id, &name))
//        return JS_TRUE; /* not resolved, but no error */
//
//    priv = priv_from_js(context, obj);
//
//    if (priv != NULL) {
//        /* instance, not prototype */
//        ret = JS_TRUE;
//        goto out;
//    }
//
//    info = (GIObjectInfo*)g_irepository_find_by_gtype(g_irepository_get_default(), G_TYPE_PARAM);
//    method_info = g_object_info_find_method(info, name);
//
//    if (method_info == NULL) {
//        ret = JS_TRUE;
//        goto out;
//    }
//#if GWKJS_VERBOSE_ENABLE_GI_USAGE
//    _gwkjs_log_info_usage((GIBaseInfo*) method_info);
//#endif
//
//    if (g_function_info_get_flags (method_info) & GI_FUNCTION_IS_METHOD) {
//        gwkjs_debug(GWKJS_DEBUG_GOBJECT,
//                  "Defining method %s in prototype for GObject.ParamSpec",
//                  g_base_info_get_name( (GIBaseInfo*) method_info));
//
//        if (gwkjs_define_function(context, obj, G_TYPE_PARAM, method_info) == NULL) {
//            g_base_info_unref( (GIBaseInfo*) method_info);
//            goto out;
//        }
//
//        objp.set(obj); /* we defined the prop in obj */
//    }
//
//    g_base_info_unref( (GIBaseInfo*) method_info);
//
//    ret = JS_TRUE;
// out:
//    g_free(name);
//    if (info != NULL)
//        g_base_info_unref( (GIBaseInfo*)info);
//
//    return ret;
//}
//
//GWKJS_NATIVE_CONSTRUCTOR_DECLARE(param)
//{
//    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(param)
//    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(param);
//    GWKJS_INC_COUNTER(param);
//    GWKJS_NATIVE_CONSTRUCTOR_FINISH(param);
//    return JS_TRUE;
//}
//
//static void
//param_finalize(JSFreeOp *fop,
//               JSObjectRef obj)
//{
//    Param *priv;
//
//    priv = (Param*) JS_GetPrivate(obj);
//    gwkjs_debug_lifecycle(GWKJS_DEBUG_GPARAM,
//                        "finalize, obj %p priv %p", obj, priv);
//    if (priv == NULL)
//        return; /* wrong class? */
//
//    if (priv->gparam) {
//        g_param_spec_unref(priv->gparam);
//        priv->gparam = NULL;
//    }
//
//    GWKJS_DEC_COUNTER(param);
//    g_slice_free(Param, priv);
//}
//
//
///* The bizarre thing about this vtable is that it applies to both
// * instances of the object, and to the prototype that instances of the
// * class have.
// */
//struct JSClass gwkjs_param_class = {
//    "GObject_ParamSpec",
//    JSCLASS_HAS_PRIVATE |
//    JSCLASS_NEW_RESOLVE |
//    JSCLASS_BACKGROUND_FINALIZE,
//    JS_PropertyStub,
//    JS_DeletePropertyStub,
//    JS_PropertyStub,
//    JS_StrictPropertyStub,
//    JS_EnumerateStub,
//    (JSResolveOp) param_new_resolve,
//    JS_ConvertStub,
//    param_finalize,
//    NULL,
//    NULL,
//    NULL, NULL, NULL
//};
//
//JSPropertySpec gwkjs_param_proto_props[] = {
//    { NULL }
//};
//
//JSFunctionSpec gwkjs_param_proto_funcs[] = {
//    { NULL }
//};
//
//static JSFunctionSpec gwkjs_param_constructor_funcs[] = {
//    { NULL }
//};
//
//static JSObject*
//gwkjs_lookup_param_prototype(JSContextRef    context)
//{
//    JSObjectRef in_object;
//    JSObjectRef constructor;
//    jsid gobject_name;
//    jsval value;
//
//    gobject_name = gwkjs_intern_string_to_id(context, "GObject");
//    in_object = gwkjs_lookup_namespace_object_by_name(context, gobject_name);
//
//    if (G_UNLIKELY (!in_object))
//        return NULL;
//
//    if (!JS_GetProperty(context, in_object, "ParamSpec", &value))
//        return NULL;
//
//    if (G_UNLIKELY (!JSVAL_IS_OBJECT(value) || JSVAL_IS_NULL(value)))
//        return NULL;
//
//    constructor = JSVAL_TO_OBJECT(value);
//    g_assert(constructor != NULL);
//
//    if (!gwkjs_object_get_property_const(context, constructor,
//                                       GWKJS_STRING_PROTOTYPE, &value))
//        return NULL;
//
//    if (G_UNLIKELY (!JSVAL_IS_OBJECT(value)))
//        return NULL;
//
//    return JSVAL_TO_OBJECT(value);
//}
//
//void
//gwkjs_define_param_class(JSContextRef    context,
//                       JSObjectRef     in_object)
//{
//    const char *constructor_name;
//    JSObjectRef prototype;
//    jsval value;
//    JSObjectRef constructor;
//    GIObjectInfo *info;
//
//    constructor_name = "ParamSpec";
//
//    if (!gwkjs_init_class_dynamic(context, in_object,
//                                NULL,
//                                "GObject",
//                                constructor_name,
//                                &gwkjs_param_class,
//                                gwkjs_param_constructor, 0,
//                                /* props of prototype */
//                                &gwkjs_param_proto_props[0],
//                                /* funcs of prototype */
//                                &gwkjs_param_proto_funcs[0],
//                                /* props of constructor, MyConstructor.myprop */
//                                NULL,
//                                /* funcs of constructor, MyConstructor.myfunc() */
//                                gwkjs_param_constructor_funcs,
//                                &prototype,
//                                &constructor)) {
//        g_error("Can't init class %s", constructor_name);
//    }
//
//    value = OBJECT_TO_JSVAL(gwkjs_gtype_create_gtype_wrapper(context, G_TYPE_PARAM));
//    JS_DefineProperty(context, constructor, "$gtype", value,
//                      NULL, NULL, JSPROP_PERMANENT);
//
//    info = (GIObjectInfo*)g_irepository_find_by_gtype(g_irepository_get_default(), G_TYPE_PARAM);
//    gwkjs_object_define_static_methods(context, constructor, G_TYPE_PARAM, info);
//    g_base_info_unref( (GIBaseInfo*) info);
//
//    gwkjs_debug(GWKJS_DEBUG_GPARAM, "Defined class %s prototype is %p class %p in object %p",
//              constructor_name, prototype, JS_GetClass(prototype), in_object);
//}
//
//JSObject*
//gwkjs_param_from_g_param(JSContextRef    context,
//                       GParamSpec   *gparam)
//{
//    JSObjectRef obj;
//    JSObjectRef proto;
//    Param *priv;
//
//    if (gparam == NULL)
//        return NULL;
//
//    gwkjs_debug(GWKJS_DEBUG_GPARAM,
//              "Wrapping %s '%s' on %s with JSObject",
//              g_type_name(G_TYPE_FROM_INSTANCE((GTypeInstance*) gparam)),
//              gparam->name,
//              g_type_name(gparam->owner_type));
//
//    proto = gwkjs_lookup_param_prototype(context);
//
//    obj = JS_NewObjectWithGivenProto(context,
//                                     JS_GetClass(proto), proto,
//                                     gwkjs_get_import_global (context));
//
//    GWKJS_INC_COUNTER(param);
//    priv = g_slice_new0(Param);
//    JS_SetPrivate(obj, priv);
//    priv->gparam = gparam;
//    g_param_spec_ref (gparam);
//
//    gwkjs_debug(GWKJS_DEBUG_GPARAM,
//              "JSObject created with param instance %p type %s",
//              priv->gparam, g_type_name(G_TYPE_FROM_INSTANCE((GTypeInstance*) priv->gparam)));
//
//    return obj;
//}
//
//GParamSpec*
//gwkjs_g_param_from_param(JSContextRef    context,
//                       JSObjectRef     obj)
//{
//    Param *priv;
//
//    if (obj == NULL)
//        return NULL;
//
//    priv = priv_from_js(context, obj);
//
//    return priv->gparam;
//}
//
//JSBool
//gwkjs_typecheck_param(JSContextRef     context,
//                    JSObjectRef      object,
//                    GType          expected_type,
//                    JSBool         throw_error)
//{
//    Param *priv;
//    JSBool result;
//
//    if (!do_base_typecheck(context, object, throw_error))
//        return JS_FALSE;
//
//    priv = priv_from_js(context, object);
//
//    if (priv->gparam == NULL) {
//        if (throw_error) {
//            gwkjs_throw_custom(context, "TypeError",
//                             "Object is GObject.ParamSpec.prototype, not an object instance - "
//                             "cannot convert to a GObject.ParamSpec instance");
//        }
//
//        return JS_FALSE;
//    }
//
//    if (expected_type != G_TYPE_NONE)
//        result = g_type_is_a (G_TYPE_FROM_INSTANCE (priv->gparam), expected_type);
//    else
//        result = JS_TRUE;
//
//    if (!result && throw_error) {
//        gwkjs_throw_custom(context, "TypeError",
//                         "Object is of type %s - cannot convert to %s",
//                         g_type_name(G_TYPE_FROM_INSTANCE (priv->gparam)),
//                         g_type_name(expected_type));
//    }
//
//    return result;
//}
