/*
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2012  Red Hat, Inc.
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

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include <gwkjs/gwkjs-module.h>
#include <gwkjs/exceptions.h>
#include <gi/object.h>
#include "system.h"

#define NUMARG_EXPECTED_EXCEPTION(name, argc)                                  \
    gwkjs_make_exception(ctx, exception, "ArgumentError",                       \
                        name " expected " argc " but got %zd", argumentCount); \
    return JSValueMakeUndefined(ctx);

static JSValueRef
gwkjs_address_of(JSContextRef ctx,
                 JSObjectRef function,
                 JSObjectRef this_object,
                 size_t argumentCount,
                 const JSValueRef arguments[],
                 JSValueRef* exception)
{
    if (argumentCount != 1) {
        NUMARG_EXPECTED_EXCEPTION("addressOf", "1 argument")
    }

    JSValueRef targetValue = arguments[0];
    if (!JSValueIsObject(ctx, targetValue)) {
        gwkjs_make_exception(ctx, exception, "ArgumentError",
                            "addressOf expects an object");
        return JSValueMakeUndefined(ctx);
    }

    gchar* pointer_string = g_strdup_printf("%p", targetValue);
    JSValueRef ret = gwkjs_cstring_to_jsvalue(ctx, pointer_string);
    g_free(pointer_string);

    return ret;
}

static JSValueRef
gwkjs_refcount(JSContextRef ctx,
               JSObjectRef function,
               JSObjectRef this_object,
               size_t argumentCount,
               const JSValueRef arguments[],
               JSValueRef* exception)
{
    if (argumentCount != 1) {
        NUMARG_EXPECTED_EXCEPTION("refcount", "1 argument");
    }

    JSValueRef targetValue = arguments[0];
    GObject* object = gwkjs_g_object_from_object(ctx, JSValueToObject(ctx, targetValue, NULL));
    if (!object)
        return (JSValueMakeUndefined(ctx));

    JSValueRef ret = JSValueMakeNumber(ctx, object->ref_count);
    return ret;
}

static JSValueRef
gwkjs_breakpoint(JSContextRef ctx,
             JSObjectRef function,
             JSObjectRef this_object,
             size_t argumentCount,
             const JSValueRef arguments[],
             JSValueRef* exception)
{
    if (argumentCount != 0) {
        NUMARG_EXPECTED_EXCEPTION("breakpoint", "0 arguments");
    }
    G_BREAKPOINT();
    return (JSValueMakeUndefined(ctx));
}

static JSValueRef
gwkjs_gc(JSContextRef ctx,
         JSObjectRef function,
         JSObjectRef this_object,
         size_t argumentCount,
         const JSValueRef arguments[],
         JSValueRef* exception)
{
    if (argumentCount != 0) {
        NUMARG_EXPECTED_EXCEPTION("gc", "0 arguments");
    }

    JSGarbageCollect(ctx);
    return JSValueMakeUndefined(ctx);
}

static JSValueRef
gwkjs_exit(JSContextRef ctx,
           JSObjectRef function,
           JSObjectRef this_object,
           size_t argumentCount,
           const JSValueRef arguments[],
           JSValueRef* exception)
{
    gint32 ret = EXIT_SUCCESS;
    if (argumentCount > 1) {
        NUMARG_EXPECTED_EXCEPTION("exit", "none or 1 argument");
    }

    if (argumentCount == 1) {
        JSValueRef target = arguments[0];
        if (JSValueIsNumber(ctx, target)) {
            ret = gwkjs_jsvalue_to_int(ctx, target, exception);
        } else {
            gwkjs_make_exception(ctx, exception, "ArgumentError",
                                "exit expects a number argument");
            ret = EXIT_FAILURE;
        }
    }

    exit(ret);
}

static JSValueRef
gwkjs_clear_date_caches(JSContextRef ctx,
         JSObjectRef function,
         JSObjectRef this_object,
         size_t argumentCount,
         const JSValueRef arguments[],
         JSValueRef* exception)
{
    // This is provided just for compatibility as javascriptcore doesn't provide
    // the same
    // feature.
    // I'm not even sure that this is needed for webkit.
    gwkjs_make_exception(ctx, exception, "ImplementationError",
                        "clearDateCache is not implemented.");
    return JSValueMakeUndefined(ctx);
}

static JSStaticFunction module_funcs[]
  = { { "addressOf", gwkjs_address_of, kJSPropertyAttributeDontDelete },
      { "refcount", gwkjs_refcount, kJSPropertyAttributeDontDelete },
      { "breakpoint", gwkjs_breakpoint, kJSPropertyAttributeDontDelete },
      { "gc", gwkjs_gc, kJSPropertyAttributeDontDelete },
      { "exit", gwkjs_exit, kJSPropertyAttributeDontDelete },
      { "clearDateCaches", gwkjs_clear_date_caches, kJSPropertyAttributeDontDelete },
      { 0, 0, 0 } };

static JSClassDefinition system_def = {
    0,                                        /* Version, always 0 */
    kJSClassAttributeNoAutomaticPrototype,    /* JSClassAttributes */
    "System",                                 /* Class Name */
    NULL,                                     /* Parent Class */
    NULL,                                     /* Static Values */
    module_funcs,                             /* Static Functions */
    NULL,
    NULL, /* Finalize */
    NULL, /* Has Property */
    NULL, /* Get Property */
    NULL, /* Set Property */
    NULL, /* Delete Property */
    NULL, /* Get Property Names */
    NULL, /* Call As Function */
    NULL, /* Call As Constructor */
    NULL, /* Has Instance */
    NULL  /* Convert To Type */
};

static JSClassRef system_class = NULL;

JSBool
gwkjs_js_define_system_stuff(JSContextRef  context,
                            JSObjectRef  *module_out)
{
    GwkjsContext *gwkjs_context;
    JSValueRef exception = NULL;
    jsval value;
    JSBool retval = FALSE;
    JSObjectRef module;
    JSClassRef classref = JSClassCreate(&system_def);

    // TODO: Check if it's necessary to include DATA later
    module = JSObjectMake (context, classref, NULL);

    // TODO: find a way to fix program-name
    //gwkjs_context = (GwkjsContext*) JS_GetContextPrivate(context);
    //g_object_get(gwkjs_context,
    //             "program-name", &program_name,
    //             NULL);

    //if (!gwkjs_string_from_utf8(context, program_name,
    //                          -1, &value))
    //    goto out;

    ///* The name is modeled after program_invocation_name,
    //   part of the glibc */
    //if (!JS_DefineProperty(context, module,
    //                       "programInvocationName",
    //                       value,
    //                       JS_PropertyStub,
    //                       JS_StrictPropertyStub,
    //                       GWKJS_MODULE_PROP_FLAGS | JSPROP_READONLY))
    //    goto out;

    gwkjs_object_set_property(context, module, "version",
                              JSValueMakeNumber(context, GWKJS_VERSION),
                              kJSPropertyAttributeReadOnly |
                              kJSPropertyAttributeDontDelete, &exception);
    if (exception)
        goto out;

    retval = JS_TRUE;

    *module_out = module;
 out:

    return retval;
}
