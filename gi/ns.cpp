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

#include "ns.h"
#include "repo.h"
#include "param.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>

#include <util/log.h>
#include <girepository.h>

#include <string.h>

typedef struct {
    char *gi_namespace;
    GHashTable *modules;
} Ns;

extern JSClassDefinition gwkjs_ns_class;
static JSClassRef gwkjs_ns_class_ref = NULL;


GWKJS_DEFINE_PRIV_FROM_JS(Ns, gwkjs_ns_class)

static JSValueRef
ns_get_property(JSContextRef context,
                JSObjectRef obj,
                JSStringRef property_name,
                JSValueRef* exception)
{
    Ns *priv;
    char *name;
    GIRepository *repo;
    GIBaseInfo *info;
    JSValueRef ret = NULL;
    gboolean defined;

    name = gwkjs_jsstring_to_cstring(property_name);

    /* let Object.prototype resolve these */
    if (strcmp(name, "valueOf") == 0 ||
        strcmp(name, "toString") == 0) {
        goto out;
    }

    priv = priv_from_js(obj);
    gwkjs_debug_jsprop(GWKJS_DEBUG_GNAMESPACE,
                       "Resolve prop '%s' hook obj %p priv %p",
                       name, (void *)obj, priv);

    if (priv == NULL) {
        goto out;
    }

    // If the module was already included, the default proto system
    // should return the property, not this method.
    if (g_hash_table_contains(priv->modules, name))
        goto out;

    repo = g_irepository_get_default();

    info = g_irepository_find_by_name(repo, priv->gi_namespace, name);
    if (info == NULL) {
        /* No property defined, but no error either, so return TRUE */
        goto out;
    }

    gwkjs_debug(GWKJS_DEBUG_GNAMESPACE,
                "Found info type %s for '%s' in namespace '%s'",
                gwkjs_info_type_name(g_base_info_get_type(info)),
                g_base_info_get_name(info),
                g_base_info_get_namespace(info));


    g_hash_table_replace(priv->modules, g_strdup(name), NULL);
    if ((ret = gwkjs_define_info(context, obj, info, &defined))) {
        g_warning("Namespace imported: %s", name);
        g_base_info_unref(info);
//XXX: Does it return THIS?!
//        if (defined)
//            objp.set(obj); /* we defined the property in this object */
    } else {
        g_hash_table_remove(priv->modules, name);
        gwkjs_debug(GWKJS_DEBUG_GNAMESPACE,
                    "Failed to define info '%s'",
                    g_base_info_get_name(info));

        g_base_info_unref(info);
    }

 out:
    g_free(name);
    return ret;
}

static JSValueRef
get_name (JSContextRef context,
          JSObjectRef obj,
          JSStringRef propertyName,
          JSValueRef* exception)
{
    Ns *priv;
    JSValueRef retval = NULL;

    priv = priv_from_js(obj);

    if (priv == NULL)
        goto out;

    retval = gwkjs_cstring_to_jsvalue(context,  priv->gi_namespace);

 out:
    return retval;
}

GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(ns)

static void
ns_finalize(JSObjectRef obj)
{
    Ns *priv;

    priv = priv_from_js(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GNAMESPACE,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* we are the prototype, not a real instance */

    if (priv->gi_namespace)
        g_free(priv->gi_namespace);

    GWKJS_DEC_COUNTER(ns);
    g_slice_free(Ns, priv);
}

JSStaticValue gwkjs_ns_proto_props[] = {
    { "__name__", get_name, NULL, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
    { 0, 0, 0, 0 }
};

JSClassDefinition gwkjs_ns_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GIRepositoryNamespace",   //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    gwkjs_ns_proto_props,      //     const JSStaticValue*                staticValues;
    NULL,                      //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    ns_finalize,               //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;

    ns_get_property,           //     JSObjectGetPropertyCallback         getProperty;

    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_ns_constructor,      //     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};



static JSObjectRef
ns_new(JSContextRef context,
       const char   *ns_name)
{
    Ns *priv = NULL;
    JSObjectRef ret = NULL;
    JSObjectRef global = NULL;
    gboolean found = FALSE;
    JSObjectRef proto = NULL;
    JSValueRef exception = NULL;

    // XXX: NOT THREAD SAFE?
    if (!gwkjs_ns_class_ref) {
        gwkjs_ns_class_ref = JSClassCreate(&gwkjs_ns_class);
    }

    global = gwkjs_get_import_global(context);
    if (!(found = gwkjs_object_has_property(context,
                                            global,
                                            gwkjs_ns_class.className))) {
        proto = JSObjectMake(context,
                             gwkjs_ns_class_ref,
                             NULL);

        gwkjs_object_set_property(context, global,
                                  gwkjs_ns_class.className,
                                  proto,
                                  GWKJS_PROTO_PROP_FLAGS,
                                  &exception);

        if (exception)
           g_error("Can't init class %s", gwkjs_ns_class.className);

        gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Initialized class %s prototype %p",
                    gwkjs_ns_class.className, proto);
    } else {
        JSValueRef proto_val = gwkjs_object_get_property(context,
                                                         global,
                                                         gwkjs_ns_class.className,
                                                         &exception);

        if (exception || proto_val == NULL || !JSValueIsObject(context, proto_val))
            g_error("Can't get protoType for class %s", gwkjs_ns_class.className);

        proto = JSValueToObject(context, proto_val, NULL);
    }
    g_assert(proto != NULL);

    ret = JSObjectMake(context, gwkjs_ns_class_ref, NULL);

    if (ret == NULL)
        return ret;

    JSObjectSetPrototype(context, ret, proto);

    priv = g_slice_new0(Ns);
    GWKJS_INC_COUNTER(ns);
    
    g_assert(priv_from_js(ret) == NULL);
    JSObjectSetPrivate(ret, priv);


    gwkjs_debug_lifecycle(GWKJS_DEBUG_GNAMESPACE, "ns constructor, obj %p priv %p", ns, priv);

    priv = priv_from_js(ret);
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    priv->gi_namespace = g_strdup(ns_name);
    return ret;
}



JSObjectRef
gwkjs_create_ns(JSContextRef context,
                const char   *ns_name)
{
    return ns_new(context, ns_name);
}
