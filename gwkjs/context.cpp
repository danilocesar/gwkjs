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

#include <gio/gio.h>

#include "context-private.h"
#include "importer.h"
#include "jsapi-private.h"
#include "jsapi-util.h"
#include "native.h"
#include "byteArray.h"
#include "compat.h"
#include "runtime.h"

#include "gi.h"
#include "gi/object.h"

#include <modules/modules.h>

#include <util/log.h>
#include <util/glib.h>
#include <util/error.h>
#include <girepository.h>

#include <string.h>

static void     gwkjs_context_dispose           (GObject               *object);
static void     gwkjs_context_finalize          (GObject               *object);
static void     gwkjs_context_constructed       (GObject               *object);
static void     gwkjs_context_get_property      (GObject               *object,
                                                  guint                  prop_id,
                                                  GValue                *value,
                                                  GParamSpec            *pspec);
static void     gwkjs_context_set_property      (GObject               *object,
                                                  guint                  prop_id,
                                                  const GValue          *value,
                                                  GParamSpec            *pspec);
struct _GwkjsContext {
    GObject parent;

    JSContextRef context;
    JSObjectRef global;

    char *program_name;

    char **search_path;

    gboolean destroying;

//TODO: IMPLEMENT
//    JSRuntime *runtime;
//    guint    auto_gc_id;
//
//    jsid const_strings[GWKJS_STRING_LAST];
};

// XXX: Do we want only one context group?
static JSContextGroupRef ContextGroup = NULL;

/* Keep this consistent with GwkjsConstString */
static const char *const_strings[] = {
    "constructor", "prototype", "length",
    "imports", "__parentModule__", "__init__", "searchPath",
    "__gwkjsKeepAlive", "__gwkjsPrivateNS",
    "gi", "versions", "overrides",
    "_init", "_instance_init", "_new_internal", "new",
    "message", "code", "stack", "fileName", "lineNumber", "name",
    "x", "y", "width", "height",
};

G_STATIC_ASSERT(G_N_ELEMENTS(const_strings) == GWKJS_STRING_LAST);


struct _GwkjsContextClass {
    GObjectClass parent;
};


G_DEFINE_TYPE(GwkjsContext, gwkjs_context, G_TYPE_OBJECT);


enum {
    PROP_0,
    PROP_SEARCH_PATH,
    PROP_PROGRAM_NAME,
};

// TODO: Is this really necessary?
static GMutex contexts_lock;
static GList *all_contexts = NULL;

static JSValueRef
gwkjs_log(JSContextRef ctx,
          JSObjectRef function,
          JSObjectRef thisObject,
          size_t argumentCount,
          const JSValueRef arguments[],
          JSValueRef* exception)
{
    gchar *s = NULL;

    if (argumentCount != 1) {
        gwkjs_throw(ctx, "Must pass a single argument to log()");
        goto out;
    }
    s = gwkjs_jsvalue_to_cstring(ctx, arguments[0], exception);

    if (*exception != NULL) {
        g_message("JS LOG: <cannot convert value to string>");
    }

    g_message("JS LOG: %s", s);

out:
    g_free(s);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef
gwkjs_log_error(JSContextRef ctx,
                JSObjectRef function,
                JSObjectRef thisObject,
                size_t argumentCount,
                const JSValueRef arguments[],
                JSValueRef* exception)
{
    gchar *str = NULL;

    if ((argumentCount != 1 && argumentCount != 2) ||
        !JSValueIsObject (ctx, arguments[0])) {
        gwkjs_throw(ctx, "Must pass an exception and optionally a message to logError()");
        goto out;
    }

    if (argumentCount == 2) {
        /* JS_ValueToString might throw, in which we will only
         *log that the value could be converted to string */
        str = gwkjs_jsvalue_to_cstring(ctx, arguments[1], NULL);
    }

    gwkjs_log_exception_full(ctx, arguments[0], str);

out:
    g_free(str);
    return JSValueMakeUndefined(ctx);
}

static JSBool
gwkjs_print_parse_args(JSContextRef ctx,
                size_t argumentCount,
                const JSValueRef arguments[],
                char     **buffer,
                JSValueRef* exception)
{
    GString *str;
    gchar *s;
    guint n;

    str = g_string_new("");
    for (n = 0; n < argumentCount; ++n) {
        s = gwkjs_jsvalue_to_cstring(ctx, arguments[n], exception);

        if (!s)
            g_string_append(str, "<invalid string>");
        else
            g_string_append(str, s);
        g_free(s);

        if (n < (argumentCount-1))
            g_string_append_c(str, ' ');

    }
    *buffer = g_string_free(str, FALSE);

    return TRUE;
}

static JSValueRef
gwkjs_print(JSContextRef ctx,
          JSObjectRef function,
          JSObjectRef thisObject,
          size_t argumentCount,
          const JSValueRef arguments[],
          JSValueRef* exception)
{
    char *buffer;

    if (!gwkjs_print_parse_args(ctx, argumentCount, arguments, &buffer, exception)) {
        return FALSE;
    }

    g_print("%s\n", buffer);
    g_free(buffer);

out:
    return JSValueMakeUndefined(ctx);
}

static JSValueRef
gwkjs_printerr(JSContextRef ctx,
          JSObjectRef function,
          JSObjectRef thisObject,
          size_t argumentCount,
          const JSValueRef arguments[],
          JSValueRef* exception)
{
    char *buffer;

    if (!gwkjs_print_parse_args(ctx, argumentCount, arguments, &buffer, exception)) {
        return FALSE;
    }

    g_printerr("%s\n", buffer);
    g_free(buffer);

out:
    return JSValueMakeUndefined(ctx);
}


static void
gwkjs_context_init(GwkjsContext *js_context)
{
    gwkjs_context_make_current(js_context);
}

static void
gwkjs_context_class_init(GwkjsContextClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    // TODO: uncomment eventually
    //object_class->dispose = gwkjs_context_dispose;
    //object_class->finalize = gwkjs_context_finalize;

    object_class->constructed = gwkjs_context_constructed;
    object_class->get_property = gwkjs_context_get_property;
    object_class->set_property = gwkjs_context_set_property;

    pspec = g_param_spec_boxed("search-path",
                               "Search path",
                               "Path where modules to import should reside",
                               G_TYPE_STRV,
                               (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object_class,
                                    PROP_SEARCH_PATH,
                                    pspec);

    pspec = g_param_spec_string("program-name",
                                "Program Name",
                                "The filename of the launched JS program",
                                "",
                                (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(object_class,
                                    PROP_PROGRAM_NAME,
                                    pspec);

    /* For GwkjsPrivate */
    {
        char *priv_typelib_dir = g_build_filename (PKGLIBDIR, "girepository-1.0", NULL);
        g_irepository_prepend_search_path(priv_typelib_dir);
        g_free (priv_typelib_dir);
    }

    // TODO: Register Native MODULES
    //gwkjs_register_native_module("byteArray", gwkjs_define_byte_array_stuff);
    //gwkjs_register_native_module("_gi", gwkjs_define_private_gi_stuff);
    gwkjs_register_native_module("gi", gwkjs_define_gi_stuff);

    gwkjs_register_static_modules();
}
//
//static void
//gwkjs_context_dispose(GObject *object)
//{
//    GwkjsContext *js_context;
//
//    js_context = GWKJS_CONTEXT(object);
//
//    if (js_context->global != NULL) {
//        js_context->global = NULL;
//    }
//
//    if (js_context->context != NULL) {
//
//        gwkjs_debug(GWKJS_DEBUG_CONTEXT,
//                  "Destroying JS context");
//
//        JS_BeginRequest(js_context->context);
//        /* Do a full GC here before tearing down, since once we do
//         * that we may not have the JS_GetPrivate() to access the
//         * context
//         */
//        JS_GC(js_context->runtime);
//        JS_EndRequest(js_context->context);
//
//        js_context->destroying = TRUE;
//
//        /* Now, release all native objects, to avoid recursion between
//         * the JS teardown and the C teardown.  The JSObject proxies
//         * still exist, but point to NULL.
//         */
//        gwkjs_object_prepare_shutdown(js_context->context);
//
//        if (js_context->auto_gc_id > 0) {
//            g_source_remove (js_context->auto_gc_id);
//            js_context->auto_gc_id = 0;
//        }
//
//        /* Tear down JS */
//        JS_DestroyContext(js_context->context);
//        js_context->context = NULL;
//        js_context->runtime = NULL;
//    }
//
//    G_OBJECT_CLASS(gwkjs_context_parent_class)->dispose(object);
//}
//
//static void
//gwkjs_context_finalize(GObject *object)
//{
//    GwkjsContext *js_context;
//
//    js_context = GWKJS_CONTEXT(object);
//
//    if (js_context->search_path != NULL) {
//        g_strfreev(js_context->search_path);
//        js_context->search_path = NULL;
//    }
//
//    if (js_context->program_name != NULL) {
//        g_free(js_context->program_name);
//        js_context->program_name = NULL;
//    }
//
//    if (gwkjs_context_get_current() == (GwkjsContext*)object)
//        gwkjs_context_make_current(NULL);
//
//    g_mutex_lock(&contexts_lock);
//    all_contexts = g_list_remove(all_contexts, object);
//    g_mutex_unlock(&contexts_lock);
//
//    G_OBJECT_CLASS(gwkjs_context_parent_class)->finalize(object);
//}
//
static void
_gwkjs_create_global_function(JSContextRef context,
                              JSObjectRef global,
                              const gchar* name,
                              JSObjectCallAsFunctionCallback cb)
{
    JSObjectRef ref = JSObjectMakeFunctionWithCallback(context, NULL, cb);
    gwkjs_object_set_property(context, global, name, ref,
                                  kJSPropertyAttributeReadOnly |
                                  kJSPropertyAttributeDontEnum |
                                  kJSPropertyAttributeDontDelete,
                              NULL);
}

static void
gwkjs_context_constructed(GObject *object)
{
    GwkjsContext *js_context = GWKJS_CONTEXT(object);
    int i;

    G_OBJECT_CLASS(gwkjs_context_parent_class)->constructed(object);

    // NO RUNTIME in JSC
    //js_context->runtime = gwkjs_runtime_for_current_thread();

    if (ContextGroup == NULL)
        ContextGroup = JSContextGroupCreate();

    js_context->context = JSGlobalContextCreateInGroup(ContextGroup, NULL);
    if (js_context->context == NULL)
        g_error("Failed to create javascript context");

// TODO: sounds unecessary
//    for (i = 0; i < GWKJS_STRING_LAST; i++)
//        js_context->const_strings[i] = gwkjs_intern_string_to_id(js_context->context, const_strings[i]);

    /* set ourselves as the private data */
    // XXX: Works for now, but it might be the root of future issues:
    // SpiderMonkey sets the JSContext private to be the GwkjsContext. JSC doesn't
    // have a JSContextSetPrivate. So, as the global doesn't have a private object
    // we can set the GwkjsContext in the global object
    //JSObjectSetPrivate(js_context->context, js_context);

    js_context->global = JSContextGetGlobalObject(js_context->context);

    // XXX: ^^ here we're setting the private object mentioned before.
    JSObjectSetPrivate(js_context->global, js_context);

    JSValueRef exception = NULL;
    gwkjs_object_set_property(js_context->context, js_context->global,
                              "window", js_context->global,
                              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,
                              &exception);
    if (exception)
        g_error("No memory to export global object as 'window'");

    _gwkjs_create_global_function(js_context->context, js_context->global,
                                  "log", &gwkjs_log);

    _gwkjs_create_global_function(js_context->context, js_context->global,
                                  "logErrpr", &gwkjs_log_error);

    _gwkjs_create_global_function(js_context->context, js_context->global,
                                  "print", &gwkjs_print);

    _gwkjs_create_global_function(js_context->context, js_context->global,
                                  "printerr", &gwkjs_printerr);

    /* We create the global-to-runtime root importer with the
     * passed-in search path. If someone else already created
     * the root importer, this is a no-op.
     */
    JSValueRef importer = NULL;
    importer = gwkjs_create_root_importer(js_context,
                                          js_context->search_path ?
                                          (const char**) js_context->search_path :
                                          NULL,
                                          TRUE);
    if (importer == NULL)
        g_error("Failed to create root importer");

    /* Now copy the global root importer (which we just created,
     * if it didn't exist) to our global object
     */
    if (!gwkjs_define_root_importer(js_context->context,
                                    js_context->global,
                                    importer))
        g_error("Failed to point 'imports' property at root importer");

    g_mutex_lock (&contexts_lock);
    all_contexts = g_list_prepend(all_contexts, object);
    g_mutex_unlock (&contexts_lock);
}

static void
gwkjs_context_get_property (GObject     *object,
                          guint        prop_id,
                          GValue      *value,
                          GParamSpec  *pspec)
{
    GwkjsContext *js_context;

    js_context = GWKJS_CONTEXT (object);

    switch (prop_id) {
    case PROP_PROGRAM_NAME:
        g_value_set_string(value, js_context->program_name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gwkjs_context_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    GwkjsContext *js_context;

    js_context = GWKJS_CONTEXT (object);

    switch (prop_id) {
    case PROP_SEARCH_PATH:
        js_context->search_path = (char**) g_value_dup_boxed(value);
        break;
    case PROP_PROGRAM_NAME:
        js_context->program_name = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


GwkjsContext*
gwkjs_context_new(void)
{
    return (GwkjsContext*) g_object_new (GWKJS_TYPE_CONTEXT, NULL);
}

GwkjsContext*
gwkjs_context_new_with_search_path(char** search_path)
{
    return (GwkjsContext*) g_object_new (GWKJS_TYPE_CONTEXT,
                         "search-path", search_path,
                         NULL);
}

gboolean
_gwkjs_context_destroying (GwkjsContext *context)
{
    return context->destroying;
}

//static gboolean
//trigger_gc_if_needed (gpointer user_data)
//{
//    GwkjsContext *js_context = GWKJS_CONTEXT(user_data);
//    js_context->auto_gc_id = 0;
//    gwkjs_gc_if_needed(js_context->context);
//    return FALSE;
//}
//
//void
//_gwkjs_context_schedule_gc_if_needed (GwkjsContext *js_context)
//{
//    if (js_context->auto_gc_id > 0)
//        return;
//
//    js_context->auto_gc_id = g_idle_add_full(G_PRIORITY_LOW,
//                                             trigger_gc_if_needed,
//                                             js_context, NULL);
//}
//
///**
// * gwkjs_context_maybe_gc:
// * @context: a #GwkjsContext
// * 
// * Similar to the Spidermonkey JS_MaybeGC() call which
// * heuristically looks at JS runtime memory usage and
// * may initiate a garbage collection. 
// *
// * This function always unconditionally invokes JS_MaybeGC(), but
// * additionally looks at memory usage from the system malloc()
// * when available, and if the delta has grown since the last run
// * significantly, also initiates a full JavaScript garbage
// * collection.  The idea is that since GWKJS is a bridge between
// * JavaScript and system libraries, and JS objects act as proxies
// * for these system memory objects, GWKJS consumers need a way to
// * hint to the runtime that it may be a good idea to try a
// * collection.
// *
// * A good time to call this function is when your application
// * transitions to an idle state.
// */ 
//void
//gwkjs_context_maybe_gc (GwkjsContext  *context)
//{
//    gwkjs_maybe_gc(context->context);
//}
//
///**
// * gwkjs_context_gc:
// * @context: a #GwkjsContext
// * 
// * Initiate a full GC; may or may not block until complete.  This
// * function just calls Spidermonkey JS_GC().
// */ 
//void
//gwkjs_context_gc (GwkjsContext  *context)
//{
//    JS_GC(context->runtime);
//}
//
///**
// * gwkjs_context_get_all:
// *
// * Returns a newly-allocated list containing all known instances of #GwkjsContext.
// * This is useful for operating on the contexts from a process-global situation
// * such as a debugger.
// *
// * Return value: (element-type GwkjsContext) (transfer full): Known #GwkjsContext instances
// */
//GList*
//gwkjs_context_get_all(void)
//{
//  GList *result;
//  GList *iter;
//  g_mutex_lock (&contexts_lock);
//  result = g_list_copy(all_contexts);
//  for (iter = result; iter; iter = iter->next)
//    g_object_ref((GObject*)iter->data);
//  g_mutex_unlock (&contexts_lock);
//  return result;
//}

/**
 * gwkjs_context_get_native_context:
 *
 * Returns a pointer to the underlying native context.  For SpiderMonkey, this
 * is a JSContextRef
 */
gpointer
gwkjs_context_get_native_context (GwkjsContext *js_context)
{
    g_return_val_if_fail(GWKJS_IS_CONTEXT(js_context), NULL);
    return (gpointer) js_context->context;
}

gboolean
gwkjs_context_eval(GwkjsContext   *js_context,
                   const char   *script,
                   gssize        script_len,
                   const char   *filename,
                   int          *exit_status_p,
                   GError      **error)
{
    gboolean ret = FALSE;
    JSValueRef retval = NULL;
    JSValueRef exception = NULL;

    g_object_ref(G_OBJECT(js_context));

    if (!gwkjs_eval_with_scope(js_context->context, js_context->global, script, script_len, filename, &retval, NULL, &exception)) {
        gwkjs_log_exception(js_context->context, exception);
        g_set_error(error,
                    GWKJS_ERROR,
                    GWKJS_ERROR_FAILED,
                    "JS_EvaluateScript() failed");
        goto out;
    }

    if (exit_status_p) {
        if (JSValueIsNumber(js_context->context, retval)) {
            double code = JSValueToNumber(js_context->context, retval, NULL);
            *exit_status_p = (int)code;
        } else {
            /* Assume success if no integer was returned */
            *exit_status_p = 0;
        }
    }

    ret = TRUE;

 out:
    g_object_unref(G_OBJECT(js_context));
    return ret;
}

gboolean
gwkjs_context_eval_file(GwkjsContext    *js_context,
                      const char    *filename,
                      int           *exit_status_p,
                      GError       **error)
{
    char     *script = NULL;
    gsize    script_len;
    gboolean ret = TRUE;

    GFile *file = g_file_new_for_commandline_arg(filename);

    if (!g_file_query_exists(file, NULL)) {
        ret = FALSE;
        goto out;
    }

    if (!g_file_load_contents(file, NULL, &script, &script_len, NULL, error)) {
        ret = FALSE;
        goto out;
    }

    if (!gwkjs_context_eval(js_context, script, script_len, filename, exit_status_p, error)) {
        ret = FALSE;
        goto out;
    }

out:
    g_free(script);
    g_object_unref(file);
    return ret;
}

gboolean
gwkjs_context_define_string_array(GwkjsContext  *js_context,
                                  const char    *array_name,
                                  gssize        array_length,
                                  const char    **array_values,
                                  GError        **error)
{
    JSValueRef exception = NULL;

    if (!gwkjs_define_string_array(js_context->context,
                                   js_context->global,
                                   array_name, array_length, array_values,
                                   kJSPropertyAttributeDontDelete,
                                   &exception)) {
        gwkjs_log_exception(js_context->context, exception);
        g_set_error(error,
                    GWKJS_ERROR,
                    GWKJS_ERROR_FAILED,
                    "gwkjs_define_string_array() failed");
        return FALSE;
    }

    return TRUE;
}

static GwkjsContext *current_context;

GwkjsContext *
gwkjs_context_get_current (void)
{
    return current_context;
}


// TODO: probably drop this later
void
gwkjs_context_make_current (GwkjsContext *context)
{
    g_assert (context == NULL || current_context == NULL);

    current_context = context;
}

const gchar *
gwkjs_context_get_const_string(JSContextRef      context,
                               GwkjsConstString  name)
{
    return const_strings[name];
}

gboolean
gwkjs_object_get_property_const(JSContextRef      context,
                              JSObjectRef       obj,
                              GwkjsConstString  property_name,
                              jsval          *value_p)
{
    const char* pname = gwkjs_context_get_const_string(context, property_name);
    JSValueRef ret = NULL;
    JSValueRef exception = NULL;

    ret = gwkjs_object_get_property(context, obj, pname, &exception);
    if (!exception)
        *value_p = ret;

    return !exception;
}


/**
 * gwkjs_get_import_global:
 * @context: a #JSContext
 *
 * Gets the "import global" for the context's runtime. The import
 * global object is the global object for the context. It is used
 * as the root object for the scope of modules loaded by GWKJS in this
 * runtime, and should also be used as the globals 'obj' argument passed
 * to JS_InitClass() and the parent argument passed to JS_ConstructObject()
 * when creating a native classes that are shared between all contexts using
 * the runtime. (The standard JS classes are not shared, but we share
 * classes such as GObject proxy classes since objects of these classes can
 * easily migrate between contexts and having different classes depending
 * on the context where they were first accessed would be confusing.)
 *
 * Return value: the "import global" for the context's
 *  runtime. Will never return %NULL while GWKJS has an active context
 *  for the runtime.
 */
JSObjectRef
gwkjs_get_import_global(JSContextRef context)
{
    // TODO: Might want to check the logic here.
    //GwkjsContext *gwkjs_context = (GwkjsContext *) JS_GetContextPrivate(context);
    //return gwkjs_context->global;
    return JSContextGetGlobalObject(context);
}
