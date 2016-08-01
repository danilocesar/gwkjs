/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 *               2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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
#include <util/glib.h>
#include <util/misc.h>

#include "jsapi-util.h"
#include "compat.h"
#include "jsapi-private.h"

#include <string.h>
#include <math.h>

/*
 * JS 1.8.5 has JS_NewObjectForConstructor, but it attempts
 * to retrieve the JSClass from private fields in the constructor function,
 * which fails for our "dynamic classes".
 * This is the version included in SpiderMonkey 1.9 and later, to be
 * used until we rebase on a newer libmozjs.
 */
JSObjectRef
gwkjs_new_object_for_constructor(JSContextRef context,
                               JSClassRef  clasp,
                               JSObjectRef constructor)
{
    JSValueRef parent_val = NULL;
    JSObjectRef parent = NULL;
    jsval     prototype = NULL;

    parent_val = gwkjs_object_get_property(context, constructor, "__parent__", NULL);
    if (parent_val) {
       parent = JSValueToObject(context, parent_val, NULL);
    }

    if (!gwkjs_object_get_property_const(context, constructor, GWKJS_STRING_PROTOTYPE, &prototype))
        return NULL;

    return gwkjs_new_object(context, clasp,
                            JSValueToObject(context, prototype, NULL), parent);
}

JSBool
gwkjs_init_class_dynamic(JSContextRef     context,
                       JSObjectRef        in_object,
                       JSObjectRef        parent_proto,
                       const char         *ns_name,
                       const char         *class_name,
                       JSClassDefinition  *clasp,
                       JSClassRef         clasp_ref,
                       JSObjectCallAsConstructorCallback      constructor_native,
                       unsigned           nargs,
                       JSStaticValue      *proto_ps,
                       JSStaticFunction   *proto_fs,
                       JSStaticValue      *static_ps,
                       JSStaticFunction   *static_fs,
                       JSObjectRef       *prototype_p,
                       JSObjectRef       *constructor_p)
{
    JSObjectRef global;
    /* Force these variables on the stack, so the conservative GC will
       find them */
    JSObjectRef prototype = NULL;
    JSObjectRef constructor = NULL;
    JSObjectRef constructor_fun = NULL;
    char *full_function_name = NULL;
    JSBool res = JS_FALSE;
    JSValueRef exception = NULL;
    JSObjectRef func = NULL;

    /* Without a name, JS_NewObject fails */
    g_assert (clasp->className != NULL);

    /* gwkjs_init_class_dynamic only makes sense for instantiable classes,
       use JS_InitClass for static classes like Math */
    g_assert (constructor_native != NULL);

    global = gwkjs_get_import_global(context);

    /* Class initalization consists of three parts:
       - building a prototype
       - defining prototype properties and functions
       - building a constructor and definining it on the right object
       - defining constructor properties and functions
       - linking the constructor and the prototype, so that
         JS_NewObjectForConstructor can find it
    */

    /*
     * JS_NewObject will try to search for clasp prototype in the global
     * object if parent_proto is NULL, which is wrong, but it's not
     * a problem because it will fallback to Object.prototype if the clasp's
     * constructor is not found (and it won't be found, because we never call
     * JS_InitClass).
     */
    prototype = gwkjs_new_object(context, clasp_ref, parent_proto, global);
    if (!prototype)
        goto out;

    if (proto_ps && !gwkjs_define_properties(context, prototype, proto_ps))
        goto out;
    if (proto_fs && !gwkjs_define_functions(context, prototype, proto_fs))
        goto out;

    full_function_name = g_strdup_printf("return \"[%s_%s constructor]\";", ns_name, class_name);

    constructor = gwkjs_new_object(context, clasp_ref, parent_proto, global);
    func = JSObjectMakeFunction(context, gwkjs_cstring_to_jsstring("toString"), 0, NULL, gwkjs_cstring_to_jsstring(full_function_name), NULL, 0, NULL);
    gwkjs_object_set_property(context, constructor, "toString", func, 0, NULL);

    if (static_ps && !gwkjs_define_properties(context, constructor, static_ps))
        goto out;
    if (static_fs && !gwkjs_define_functions(context, constructor, static_fs))
        goto out;

    gwkjs_object_set_property(context, constructor, "prototype", prototype,
                              kJSPropertyAttributeNone, &exception);
    if (exception) {
        g_assert(!exception);
        goto out;
    }

    gwkjs_object_set_property(context, prototype, "constructor", constructor,
                              kJSPropertyAttributeNone, &exception);
    if (exception) {
        g_assert(!exception);
        goto out;
    }

    /* The constructor defined by JS_InitClass has no property attributes, but this
       is a more useful default for gwkjs */
    gwkjs_object_set_property(context, in_object, class_name, constructor,
                              kJSPropertyAttributeDontDelete, &exception);

    if (exception) {
        g_assert(!exception);
        goto out;
    }

    if (constructor_p)
        *constructor_p = constructor;
    if (prototype_p)
        *prototype_p = prototype;

    res = JS_TRUE;

    prototype = NULL;
    constructor = NULL;

 out:
    g_free(full_function_name);

    return res;
}

static const char*
format_dynamic_class_name (const char *name)
{
    if (g_str_has_prefix(name, "_private_"))
        return name + strlen("_private_");
    else
        return name;
}

JSBool
gwkjs_typecheck_instance(JSContextRef context,
                       JSObjectRef  obj,
                       JSClassRef   classref,
                       JSClassDefinition classdef,
                       JSBool     throw_error)
{
    if (!JSValueIsObjectOfClass(context, obj, classref)) {
        if (throw_error) {
            gwkjs_throw_custom(context, "TypeError",
                             "Object %p is not a subclass of %s",
                             obj, classdef.className);
        }

        return JS_FALSE;
    }

    return JS_TRUE;
}

//JSObject*
//gwkjs_construct_object_dynamic(JSContextRef      context,
//                             JSObjectRef       proto,
//                             unsigned        argc,
//                             jsval          *argv)
//{
//    JSObjectRef constructor;
//    JSObjectRef result = NULL;
//    jsval value;
//    jsid constructor_name;
//
//    JS_BeginRequest(context);
//
//    constructor_name = gwkjs_context_get_const_string(context, GWKJS_STRING_CONSTRUCTOR);
//    if (!gwkjs_object_require_property(context, proto, "prototype",
//                                     constructor_name, &value))
//        goto out;
//
//    constructor = JSVAL_TO_OBJECT(value);
//    result = JS_New(context, constructor, argc, argv);
//
// out:
//    JS_EndRequest(context);
//    return result;
//}
