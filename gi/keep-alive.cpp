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

#include "keep-alive.h"

#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>

#include <util/log.h>
#include <util/glib.h>

typedef struct {
    GwkjsUnrootedFunc notify;
    JSObjectRef child;
    void *data;
} Child;

typedef struct {
    GHashTable *children;
    unsigned int inside_finalize : 1;
    unsigned int inside_trace : 1;
} KeepAlive;

extern JSClassDefinition gwkjs_keep_alive_class;
static JSClassRef gwkjs_keep_alive_class_ref = NULL;

GWKJS_DEFINE_PRIV_FROM_JS(KeepAlive, gwkjs_keep_alive_class)

static guint
child_hash(gconstpointer  v)
{
    const Child *child = (const Child *) v;

    return
        GPOINTER_TO_UINT(child->notify) ^
        GPOINTER_TO_UINT(child->child) ^
        GPOINTER_TO_UINT(child->data);
}

static gboolean
child_equal (gconstpointer  v1,
             gconstpointer  v2)
{
    const Child *child1 = (const Child *) v1;
    const Child *child2 = (const Child *) v2;

    /* notify is most likely to be equal, so check it last */
    return child1->data == child2->data &&
        child1->child == child2->child &&
        child1->notify == child2->notify;
}

static void
child_free(void *data)
{
    Child *child = (Child *) data;
    g_slice_free(Child, child);
}

GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(keep_alive)

static void
keep_alive_finalize(JSObjectRef obj)
{
    KeepAlive *priv;
    void *key;
    void *value;

    priv = (KeepAlive *) JSObjectGetPrivate(obj);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_KEEP_ALIVE,
                        "keep_alive finalizing, obj %p priv %p", obj, priv);

    if (priv == NULL)
        return; /* we are the prototype, not a real instance */

    priv->inside_finalize = TRUE;

    while (gwkjs_g_hash_table_steal_one(priv->children,
                                      &key, &value)) {
        Child *child = (Child *) value;
        if (child->notify)
            (* child->notify) (child->child, child->data);

        child_free(child);
    }

    g_hash_table_destroy(priv->children);
    g_slice_free(KeepAlive, priv);
}

//static void
//trace_foreach(void *key,
//              void *value,
//              void *data)
//{
//    Child *child = (Child *) value;
//    JSTracer *tracer = (JSTracer *) data;
//
//    if (child->child != NULL) {
//        jsval val;
//        JS_SET_TRACING_DETAILS(tracer, NULL, "keep-alive", 0);
//        val = OBJECT_TO_JSVAL(child->child);
//        g_assert (JSVAL_TO_TRACEABLE (val));
//        JS_CallValueTracer(tracer, &val, "keep-alive::val");
//    }
//}
//
//static void
//keep_alive_trace(JSTracer *tracer,
//                 JSObjectRef obj)
//{
//    KeepAlive *priv;
//
//    priv = (KeepAlive *) JS_GetPrivate(obj);
//
//    if (priv == NULL) /* prototype */
//        return;
//
//    g_assert(!priv->inside_trace);
//    priv->inside_trace = TRUE;
//    g_hash_table_foreach(priv->children, trace_foreach, tracer);
//    priv->inside_trace = FALSE;
//}
//
///* The bizarre thing about this vtable is that it applies to both
// * instances of the object, and to the prototype that instances of the
// * class have.
// */
//struct JSClass gwkjs_keep_alive_class = {
//    "__private_GwkjsKeepAlive", /* means "new __private_GwkjsKeepAlive()" works */
//    JSCLASS_HAS_PRIVATE,
//    JS_PropertyStub,
//    JS_DeletePropertyStub,
//    JS_PropertyStub,
//    JS_StrictPropertyStub,
//    JS_EnumerateStub,
//    JS_ResolveStub,
//    JS_ConvertStub,
//    keep_alive_finalize,
//    NULL,
//    NULL,
//    NULL,
//    NULL,
//    keep_alive_trace,
//};

JSClassDefinition gwkjs_keep_alive_class = {
    0,                           //     Version
    kJSPropertyAttributeNone,    //     JSClassAttributes
    "__private_GwkjsKeepAlive",  //     const char* className;
    NULL,                        //     JSClassRef parentClass;
    NULL,                        //     const JSStaticValue*                staticValues;
    NULL,                        //     const JSStaticFunction*             staticFunctions;
    NULL,                        //     JSObjectInitializeCallback          initialize;
    keep_alive_finalize,         //     JSObjectFinalizeCallback            finalize;
    NULL,                        //     JSObjectHasPropertyCallback         hasProperty;
    NULL,                        //     JSObjectGetPropertyCallback         getProperty;
    NULL,                        //     JSObjectSetPropertyCallback         setProperty;
    NULL,                        //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                        //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                        //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_keep_alive_constructor,//     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                        //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                        //     JSObjectConvertToTypeCallback       convertToType;
};



JSObjectRef
gwkjs_keep_alive_new(JSContextRef context)
{
    KeepAlive *priv;
    JSObjectRef keep_alive = NULL;
    JSObjectRef global;
    JSValueRef exception = NULL;
    JSBool found;
    JSObjectRef prototype;


    /* This function creates an unattached KeepAlive object; following our
     * general strategy, we have a single KeepAlive class with a constructor
     * stored on our single "load global" pseudo-global object, and we create
     * instances with the load global as parent.
     */

    g_assert(context != NULL);

    global = gwkjs_get_import_global(context);

    g_assert(global != NULL);

    if (!gwkjs_keep_alive_class_ref) {
        gwkjs_keep_alive_class_ref = JSClassCreate(&gwkjs_keep_alive_class);
    }

    found = gwkjs_object_has_property(context, global, gwkjs_keep_alive_class.className);

    if (!found) {
        gwkjs_debug(GWKJS_DEBUG_KEEP_ALIVE,
                  "Initializing keep-alive class in context %p global %p",
                  context, global);

        prototype = JSObjectMake(context, gwkjs_keep_alive_class_ref, NULL);
        gwkjs_object_set_property(context, global, gwkjs_keep_alive_class.className,
                                  prototype, GWKJS_PROTO_PROP_FLAGS, &exception);

        if (prototype == NULL || exception)
            g_error("Can't init class %s", gwkjs_keep_alive_class.className);

        gwkjs_debug(GWKJS_DEBUG_KEEP_ALIVE, "Initialized class %s prototype %p",
                  gwkjs_keep_alive_class.className, prototype);
    } else {
        JSValueRef proto_val = gwkjs_object_get_property(context, global,
                                                         gwkjs_keep_alive_class.className, &exception);

        if (exception || JSVAL_IS_NULL(context, proto_val) || !JSValueIsObject(context, proto_val))
            g_error("Can't get protoType for class %s", gwkjs_keep_alive_class.className);

        prototype = JSValueToObject(context, proto_val, NULL);

    }
    g_assert(prototype != NULL);

    gwkjs_debug(GWKJS_DEBUG_KEEP_ALIVE,
              "Creating new keep-alive object for context %p global %p",
              context, global);

    keep_alive = gwkjs_new_object(context, gwkjs_keep_alive_class_ref, prototype, global);
    if (keep_alive == NULL) {
// TODO: implement
//        gwkjs_log_exception(context);
        g_error("Failed to create keep_alive object");
    }

    priv = g_slice_new0(KeepAlive);
    priv->children = g_hash_table_new_full(child_hash, child_equal, NULL, child_free);

    g_assert(priv_from_js(keep_alive) == NULL);
    JSObjectSetPrivate(keep_alive, priv);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_KEEP_ALIVE,
                        "keep_alive constructor, obj %p priv %p", keep_alive, priv);

 out:

    return keep_alive;
}

void
gwkjs_keep_alive_add_child(JSObjectRef          keep_alive,
                         GwkjsUnrootedFunc    notify,
                         JSObjectRef          obj,
                         void              *data)
{
    KeepAlive *priv;
    Child *child;

    g_assert(keep_alive != NULL);
    priv = (KeepAlive *) JSObjectGetPrivate(keep_alive);
    g_assert(priv != NULL);

    g_return_if_fail(!priv->inside_trace);
    g_return_if_fail(!priv->inside_finalize);

    child = g_slice_new0(Child);
    child->notify = notify;
    child->child = obj;
    child->data = data;

    /* this is sort of an expensive check, probably */
    g_return_if_fail(g_hash_table_lookup(priv->children, child) == NULL);

    /* this overwrites any identical-by-value previous child,
     * but there should not be one.
     */
    g_hash_table_replace(priv->children, child, child);
}

void
gwkjs_keep_alive_remove_child(JSObjectRef          keep_alive,
                            GwkjsUnrootedFunc    notify,
                            JSObjectRef          obj,
                            void              *data)
{
    KeepAlive *priv;
    Child child;

    g_assert(keep_alive != NULL);
    priv = (KeepAlive *) JSObjectGetPrivate(keep_alive);
    g_assert(priv != NULL);

    g_return_if_fail(!priv->inside_trace);
    g_return_if_fail(!priv->inside_finalize);

    child.notify = notify;
    child.child = obj;
    child.data = data;

    g_hash_table_remove(priv->children, &child);
}

static JSObjectRef
gwkjs_keep_alive_create(JSContextRef context)
{
    JSObjectRef keep_alive;


    keep_alive = gwkjs_keep_alive_new(context);
    if (!keep_alive)
        g_error("could not create keep_alive on global object, no memory?");

    gwkjs_set_global_slot(context, GWKJS_GLOBAL_SLOT_KEEP_ALIVE, keep_alive);

    return keep_alive;
}

JSObjectRef
gwkjs_keep_alive_get_global_if_exists (JSContextRef context)
{
    jsval keep_alive;

    keep_alive = gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_KEEP_ALIVE);

    if (G_LIKELY (JSValueIsObject(context, keep_alive)))
        return JSValueToObject(context, keep_alive, NULL);

    return NULL;
}

JSObjectRef
gwkjs_keep_alive_get_global(JSContextRef context)
{
    JSObjectRef keep_alive = gwkjs_keep_alive_get_global_if_exists(context);

    if (G_LIKELY(keep_alive))
        return keep_alive;

    return gwkjs_keep_alive_create(context);
}

void
gwkjs_keep_alive_add_global_child(JSContextRef         context,
                                GwkjsUnrootedFunc  notify,
                                JSObjectRef          child,
                                void              *data)
{
    JSObjectRef keep_alive;

    keep_alive = gwkjs_keep_alive_get_global(context);

    gwkjs_keep_alive_add_child(keep_alive, notify, child, data);
}

void
gwkjs_keep_alive_remove_global_child(JSContextRef         context,
                                   GwkjsUnrootedFunc  notify,
                                   JSObjectRef          child,
                                   void              *data)
{
    JSObjectRef keep_alive;

    keep_alive = gwkjs_keep_alive_get_global(context);

    if (!keep_alive)
        g_error("no keep_alive property on the global object, have you "
                "previously added this child?");

    gwkjs_keep_alive_remove_child(keep_alive, notify, child, data);
}
//
//typedef struct {
//    GHashTableIter hashiter;
//} GwkjsRealKeepAliveIter;
//
//void
//gwkjs_keep_alive_iterator_init (GwkjsKeepAliveIter *iter,
//                              JSObjectRef         keep_alive)
//{
//    GwkjsRealKeepAliveIter *real = (GwkjsRealKeepAliveIter*)iter;
//    KeepAlive *priv = (KeepAlive *) JS_GetPrivate(keep_alive);
//    g_assert(priv != NULL);
//    g_hash_table_iter_init(&real->hashiter, priv->children);
//}
//
//gboolean
//gwkjs_keep_alive_iterator_next (GwkjsKeepAliveIter  *iter,
//                              GwkjsUnrootedFunc    notify_func,
//                              JSObjectRef         *out_child,
//                              void             **out_data)
//{
//    GwkjsRealKeepAliveIter *real = (GwkjsRealKeepAliveIter*)iter;
//    gpointer k, v;
//    gboolean ret = FALSE;
//
//    while (g_hash_table_iter_next(&real->hashiter, &k, &v)) {
//        Child *child = (Child*)k;
//
//        if (child->notify != notify_func)
//            continue;
//
//        ret = TRUE;
//        *out_child = child->child;
//        *out_data = child->data;
//        break;
//    }
//
//    return ret;
//}
