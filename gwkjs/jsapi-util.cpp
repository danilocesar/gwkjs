/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2009 Red Hat, Inc.
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
#include <util/error.h>

#include "jsapi-util.h"
#include "exceptions.h"
#include "compat.h"
#include "context-private.h"
#include "jsapi-private.h"
#include <gi/boxed.h>

#include <string.h>
#include <math.h>

static GMutex gc_lock;
static JSClassRef empty_class_ref = NULL;

static JSValueRef* global_slots = NULL;

JSObjectRef gwkjs_new_object(JSContextRef context,
                             JSClassRef clas,
                             JSObjectRef proto,
                             JSObjectRef parent)
{
    JSObjectRef ret = NULL;
    if (!empty_class_ref) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        empty_class_ref = JSClassCreate(&definition);
    }

    if (clas)
        ret = JSObjectMake(context, clas, NULL);
    else
        ret = JSObjectMake(context, empty_class_ref, NULL);

    if (parent)
        gwkjs_object_set_property(context, ret,
                                  "__parent__", parent,
                                  kJSPropertyAttributeDontDelete, NULL);

    if (proto)
        JSObjectSetPrototype(context, ret, proto);

    return ret;
}

JSValueRef
gwkjs_cstring_to_jsvalue(JSContextRef context, const gchar *str)
{
    return JSValueMakeString(context, gwkjs_cstring_to_jsstring(str));
}

gchar *
gwkjs_jsstring_to_cstring (JSStringRef property_name)
{
    gint length = JSStringGetMaximumUTF8CStringSize(property_name);
    gchar *cproperty_name = (gchar *) g_new(gchar, length + 1);
    JSStringGetUTF8CString(property_name, cproperty_name, length);

    return (cproperty_name);
}

JSStringRef
gwkjs_cstring_to_jsstring(const char* str)
{
    return JSStringCreateWithUTF8CString(str);
}

gboolean
gwkjs_object_set_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName,
                          JSValueRef value,
                          JSPropertyAttributes attributes,
                          JSValueRef* exception)
{
    JSValueRef localEx = NULL;
    JSStringRef prop = gwkjs_cstring_to_jsstring(propertyName);

    JSObjectSetProperty(ctx, object, prop, value, attributes, &localEx);
    if (localEx) {
        *exception = localEx;
        return FALSE;
    }

    return TRUE;
}

guint
gwkjs_jsvalue_to_uint(JSContextRef ctx, JSValueRef val, JSValueRef* exception)
{
    if (!JSValueIsNumber(ctx, val) && !JSValueIsBoolean(ctx, val)) {
        if (!JSValueIsNull(ctx, val)) {
            gwkjs_make_exception(ctx, exception, "ConversionError",
                                 "Can not convert Javascript value to"
                                 " boolean");
        }
        return 0;
    }

    return (guint) JSValueToNumber(ctx, val, NULL);
}

gint
gwkjs_jsvalue_to_int(JSContextRef ctx, JSValueRef val, JSValueRef* exception)
{
    if (!JSValueIsNumber(ctx, val) && !JSValueIsBoolean(ctx, val)) {
        if (!JSValueIsNull(ctx, val)) {
            gwkjs_make_exception(ctx, exception, "ConversionError",
                                 "Can not convert Javascript value to"
                                 " boolean");
        }
        return 0;
    }

    return (gint) JSValueToNumber(ctx, val, NULL);
}

JSValueRef
gwkjs_int_to_jsvalue(JSContextRef ctx, gint val)
{
    return JSValueMakeNumber(ctx, (gdouble) val);
}


JSValueRef
gwkjs_object_get_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName,
                          JSValueRef* exception)
{
    JSStringRef prop = gwkjs_cstring_to_jsstring(propertyName);
    return (JSObjectGetProperty(ctx, object, prop, exception));
}

const gboolean
gwkjs_object_has_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName)
{
    JSStringRef prop = gwkjs_cstring_to_jsstring(propertyName);
    return (JSObjectHasProperty(ctx, object, prop));
}

gchar*
gwkjs_jsvalue_to_cstring(JSContextRef ctx, JSValueRef val, JSValueRef* exception)
{
    JSStringRef jsstr = NULL;
    JSValueRef func;
    gchar* buf = NULL;
    gint length;

    if (val == NULL)
        return NULL;
    else if (JSValueIsUndefined(ctx, val)) {
        buf = g_strdup("[undefined]");
    } else if (JSValueIsNull(ctx, val)) {
        buf = g_strdup("[null]");
    } else if (JSValueIsBoolean(ctx, val) || JSValueIsNumber(ctx, val)) {
        buf = g_strdup_printf("%.15g", JSValueToNumber(ctx, val, NULL));
    } else {
        if (!JSValueIsString(ctx, val)) // In this case,
        // it's an object
        {
            func = gwkjs_object_get_property(ctx, (JSObjectRef) val, "toString", NULL);

            if (!JSValueIsNull(ctx, func) && JSValueIsObject(ctx, func)
                && JSObjectIsFunction(ctx, (JSObjectRef) func))
                // str = ... we dump the return value!?!
                JSObjectCallAsFunction(ctx, (JSObjectRef) func,
                                       (JSObjectRef) val, 0, NULL, NULL);
        }

        jsstr = JSValueToStringCopy(ctx, val, NULL);
        length = JSStringGetMaximumUTF8CStringSize(jsstr);
        if (length > 0) {
            buf = (gchar *)g_malloc(length * sizeof(gchar));
            JSStringGetUTF8CString(jsstr, buf, length);
        }
        if (jsstr)
            JSStringRelease(jsstr);
    }

    return buf;
}

JSObjectRef
gwkjs_make_function(JSContextRef ctx, const gchar *name, JSObjectCallAsFunctionCallback cb)
{
    JSObjectRef ret = NULL;
    JSStringRef str_name = gwkjs_cstring_to_jsstring(name);

    return JSObjectMakeFunctionWithCallback(ctx, str_name, cb);
}


//
//GQuark
//gwkjs_util_error_quark (void)
//{
//    return g_quark_from_static_string ("gwkjs-util-error-quark");
//}
//
//static JSClass global_class = {
//    "GwkjsGlobal", JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(GWKJS_GLOBAL_SLOT_LAST),
//    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
//    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, NULL,
//    NULL /* checkAccess */, NULL /* call */, NULL /* hasInstance */, NULL /* construct */, NULL,
//    { NULL }
//};
//
///**
// * gwkjs_init_context_standard:
// * @context: a #JSContext
// * @global_out: (out): The created global object.
//
// * This function creates a global object given the context,
// * and initializes it with the default API.
// *
// * Returns: %TRUE on success, %FALSE otherwise
// */
//gboolean
//gwkjs_init_context_standard (JSContextRef  context,
//                           JSObjectRef  *global_out)
//{
//    JSObjectRef global;
//    JS::CompartmentOptions options;
//    guint32 options_flags;
//
//    /* JSOPTION_DONT_REPORT_UNCAUGHT: Don't send exceptions to our
//     * error report handler; instead leave them set.  This allows us
//     * to get at the exception object.
//     *
//     * JSOPTION_STRICT: Report warnings to error reporter function.
//     */
//    options_flags = JSOPTION_DONT_REPORT_UNCAUGHT;
//
//    if (!g_getenv("GWKJS_DISABLE_JIT")) {
//        gwkjs_debug(GWKJS_DEBUG_CONTEXT, "Enabling JIT");
//        options_flags |= JSOPTION_TYPE_INFERENCE | JSOPTION_ION | JSOPTION_BASELINE | JSOPTION_ASMJS;
//    }
//
//    if (!g_getenv("GWKJS_DISABLE_EXTRA_WARNINGS")) {
//        gwkjs_debug(GWKJS_DEBUG_CONTEXT, "Enabling extra warnings");
//        options_flags |= JSOPTION_EXTRA_WARNINGS;
//    }
//
//    JS_SetOptions(context, JS_GetOptions(context) | options_flags);
//    JS_SetErrorReporter(context, gwkjs_error_reporter);
//
//    options.setVersion(JSVERSION_LATEST);
//    global = JS_NewGlobalObject(context, &global_class, NULL, options);
//    if (global == NULL)
//        return FALSE;
//
//    /* Set the context's global */
//    JSAutoCompartment ac(context, global);
//
//    if (!JS_InitStandardClasses(context, global))
//        return FALSE;
//
//    if (!JS_InitReflect(context, global))
//        return FALSE;
//
//    if (!JS_DefineDebuggerObject(context, global))
//        return FALSE;
//
//    *global_out = global;
//
//    return TRUE;
//}

void
inline init_global_slots()
{
    if (!global_slots) {
        global_slots = g_new0(JSValueRef, GWKJS_GLOBAL_SLOT_LAST);
        int i;
        for (i = 0; i< GWKJS_GLOBAL_SLOT_LAST; i++) {
            global_slots[i] = NULL;
        }
    }
}

void
gwkjs_set_global_slot (JSContextRef     context,
                     GwkjsGlobalSlot  slot,
                     JSValueRef          value)
{
    init_global_slots();
    if (global_slots[slot] != NULL) {
        JSValueUnprotect(context, global_slots[slot]);
    }

    global_slots[slot] = value;
    JSValueProtect(context, value);
}

jsval
gwkjs_get_global_slot (JSContextRef     context,
                     GwkjsGlobalSlot  slot)
{
    init_global_slots();
    if (global_slots[slot] != NULL) {
        return global_slots[slot];
    }
    return JSValueMakeUndefined(context);
}

/* Returns whether the object had the property; if the object did
 * not have the property, always sets an exception. Treats
 * "the property's value is JSVAL_VOID" the same as "no such property,".
 * Guarantees that *value_p is set to something, if only JSVAL_VOID,
 * even if an exception is set and false is returned.
 *
 * Requires request.
 */
gboolean
gwkjs_object_require_property(JSContextRef  context,
                              JSObjectRef   obj,
                              const char   *obj_description,
                              const char   *property_name,
                              JSValueRef   *value_p)
{
    JSValueRef value = NULL;

    if (value_p)
        *value_p = value;

    value = gwkjs_object_get_property(context, obj, property_name, NULL);
    if (G_UNLIKELY (value == NULL))
        return FALSE;

    if (G_LIKELY (!JSValueIsNull(context, value) && !JSValueIsUndefined(context, value))) {
        if (value_p)
            *value_p = value;
        return TRUE;
    }

    /* remember gwkjs_throw() is a no-op if JS_GetProperty()
     * already set an exception
     */

    if (obj_description)
        gwkjs_throw(context,
                  "No property '%s' in %s (or its value was undefined)",
                  property_name, obj_description);
    else
        gwkjs_throw(context,
                  "No property '%s' in object %p (or its value was undefined)",
                  property_name, obj);

    return JS_FALSE;
}

gboolean
gwkjs_array_get_length(JSContextRef context,
                       JSObjectRef array,
                       guint32 *result_out)
{
    gboolean ret = FALSE;
    JSValueRef exception = NULL;
    const gchar *lengthstr = gwkjs_context_get_const_string(context, GWKJS_STRING_LENGTH);

    JSValueRef num_val = gwkjs_object_get_property(context, array, lengthstr, &exception);
    if (exception) {
        gwkjs_throw(context,
                    "No length in array %p", array);
        ret = FALSE;
        goto out;
    }
    if (JSValueIsNumber(context, num_val)) {
        if (result_out)
            *result_out = gwkjs_jsvalue_to_uint(context, num_val, NULL);
        ret = TRUE;
    }

out:
    return ret;
}

gboolean
gwkjs_array_get_element(JSContextRef context,
                        JSObjectRef obj,
                        guint pos,
                        JSValueRef *out)
{
    JSValueRef exception = NULL;
    gboolean ret = FALSE;

    JSValueRef out_v = JSObjectGetPropertyAtIndex(context, obj, pos, &exception);
    if (exception) {
        // TODO: thow exception / error?
        goto out;
    }

    *out = out_v;
    ret = TRUE;

out:
    return ret;

}

//void
//gwkjs_throw_constructor_error(JSContextRef context)
//{
//    gwkjs_throw(context,
//              "Constructor called as normal method. Use 'new SomeObject()' not 'SomeObject()'");
//}

void
gwkjs_throw_abstract_constructor_error(JSContextRef context,
                                       JSObjectRef  object,
                                       JSValueRef   *exception)
{
    const char *name = "[anonymous]";
    gchar *s = NULL;
    JSValueRef proto = JSObjectGetPrototype(context, object);

    s = gwkjs_jsvalue_to_cstring(context, object, NULL);

    gwkjs_throw(context, "You cannot construct new instances of '%s'", name);

    gwkjs_make_exception(context, exception, "ConstructorError",
                         "You cannot construct new instances of '%s'", s? s : "[anonymous]");
}


JSObjectRef
gwkjs_build_string_array(JSContextRef  context,
                         gssize       array_length,
                         char       **array_values)
{
    JSObjectRef array = NULL;
    int i = 0;

    if (array_length == -1)
        array_length = g_strv_length(array_values);

    array = JSObjectMakeArray(context, 0, NULL, NULL);

    for (i = 0; i < array_length; ++i) {
        JSStringRef str = JSStringCreateWithUTF8CString(array_values[i]);
        JSValueRef element = JSValueMakeString(context, str);

        // TODO: add exception?
        JSObjectSetPropertyAtIndex(context, array, i, element, NULL);
    }

    return array;
}


JSObjectRef
gwkjs_define_string_array(JSContextRef context,
                          JSObjectRef  in_object,
                          const char  *array_name,
                          gssize       array_length,
                          const char **array_values,
                          unsigned attrs,
						  JSValueRef *exception)
{
    JSObjectRef array = NULL;

    array = gwkjs_build_string_array(context, array_length, (char **) array_values);

    // TODO: we might want to handle exceptions here?
    if (array != NULL) {
        JSObjectSetProperty(context, in_object, gwkjs_cstring_to_jsstring(array_name),
                            array, attrs, exception);
    }

    return array;
}

GwkjsContext *
gwkjs_get_private_context(JSContextRef ctx)
{
    JSObjectRef global = gwkjs_get_import_global(ctx);

    GwkjsContext *priv = (GwkjsContext*) JSObjectGetPrivate(global);
    if (priv != NULL)
        return priv;

    return NULL;
}
//
///**
// * gwkjs_string_readable:
// *
// * Return a string that can be read back by gwkjs-console; for
// * JS strings that contain valid Unicode, we return a UTF-8 formatted
// * string.  Otherwise, we return one where non-ASCII-printable bytes
// * are \x escaped.
// *
// */
//static char *
//gwkjs_string_readable (JSContextRef   context,
//                     JSString    *string)
//{
//    GString *buf = g_string_new("");
//    char *chars;
//
//    JS_BeginRequest(context);
//
//    g_string_append_c(buf, '"');
//
//    if (!gwkjs_string_to_utf8(context, STRING_TO_JSVAL(string), &chars)) {
//        size_t i, len;
//        const jschar *uchars;
//
//        uchars = JS_GetStringCharsAndLength(context, string, &len);
//
//        for (i = 0; i < len; i++) {
//            jschar c = uchars[i];
//            if (c >> 8 == 0 && g_ascii_isprint(c & 0xFF))
//                g_string_append_c(buf, c & 0xFF);
//            else
//                g_string_append_printf(buf, "\\u%04X", c);
//        }
//    } else {
//        g_string_append(buf, chars);
//        g_free(chars);
//    }
//
//    g_string_append_c(buf, '"');
//
//    JS_EndRequest(context);
//
//    return g_string_free(buf, FALSE);
//}
//
///**
// * gwkjs_value_debug_string:
// * @context:
// * @value: Any JavaScript value
// *
// * Returns: A UTF-8 encoded string describing @value
// */
//char*
//gwkjs_value_debug_string(JSContextRef      context,
//                       jsval           value)
//{
//    JSString *str;
//    char *bytes;
//    char *debugstr;
//
//    /* Special case debug strings for strings */
//    if (JSVAL_IS_STRING(value)) {
//        return gwkjs_string_readable(context, JSVAL_TO_STRING(value));
//    }
//
//    JS_BeginRequest(context);
//
//    str = JS_ValueToString(context, value);
//
//    if (str == NULL) {
//        if (JSVAL_IS_OBJECT(value)) {
//            /* Specifically the Call object (see jsfun.c in spidermonkey)
//             * does not have a toString; there may be others also.
//             */
//            JSClass *klass;
//
//            klass = JS_GetClass(JSVAL_TO_OBJECT(value));
//            if (klass != NULL) {
//                str = JS_NewStringCopyZ(context, klass->name);
//                JS_ClearPendingException(context);
//                if (str == NULL) {
//                    JS_EndRequest(context);
//                    return g_strdup("[out of memory copying class name]");
//                }
//            } else {
//                gwkjs_log_exception(context);
//                JS_EndRequest(context);
//                return g_strdup("[unknown object]");
//            }
//        } else {
//            JS_EndRequest(context);
//            return g_strdup("[unknown non-object]");
//        }
//    }
//
//    g_assert(str != NULL);
//
//    size_t len = JS_GetStringEncodingLength(context, str);
//    if (len != (size_t)(-1)) {
//        bytes = (char*) g_malloc((len + 1) * sizeof(char));
//        JS_EncodeStringToBuffer(context, str, bytes, len);
//        bytes[len] = '\0';
//    } else {
//        bytes = g_strdup("[invalid string]");
//    }
//    JS_EndRequest(context);
//
//    debugstr = _gwkjs_g_utf8_make_valid(bytes);
//    g_free(bytes);
//
//    return debugstr;
//}
//
//void
//gwkjs_log_object_props(JSContextRef      context,
//                     JSObjectRef       obj,
//                     GwkjsDebugTopic   topic,
//                     const char     *prefix)
//{
//    JSObjectRef props_iter;
//    jsid prop_id;
//
//    JS_BeginRequest(context);
//
//    props_iter = JS_NewPropertyIterator(context, obj);
//    if (props_iter == NULL) {
//        gwkjs_log_exception(context);
//        goto done;
//    }
//
//    prop_id = JSID_VOID;
//    if (!JS_NextProperty(context, props_iter, &prop_id))
//        goto done;
//
//    while (!JSID_IS_VOID(prop_id)) {
//        jsval propval;
//        char *debugstr;
//        char *name;
//
//        if (!JS_GetPropertyById(context, obj, prop_id, &propval))
//            goto next;
//
//        if (!gwkjs_get_string_id(context, prop_id, &name))
//            goto next;
//
//        debugstr = gwkjs_value_debug_string(context, propval);
//        gwkjs_debug(topic,
//                  "%s%s = '%s'",
//                  prefix, name,
//                  debugstr);
//        g_free(debugstr);
//
//    next:
//        g_free(name);
//        prop_id = JSID_VOID;
//        if (!JS_NextProperty(context, props_iter, &prop_id))
//            break;
//    }
//
// done:
//    JS_EndRequest(context);
//}
//
//void
//gwkjs_explain_scope(JSContextRef  context,
//                  const char *title)
//{
//    JSObjectRef global;
//    JSObjectRef parent;
//    GString *chain;
//    char *debugstr;
//
//    gwkjs_debug(GWKJS_DEBUG_SCOPE,
//              "=== %s ===",
//              title);
//
//    JS_BeginRequest(context);
//
//    gwkjs_debug(GWKJS_DEBUG_SCOPE,
//              "  Context: %p %s",
//              context,
//              "");
//
//    global = JS_GetGlobalObject(context);
//    debugstr = gwkjs_value_debug_string(context, OBJECT_TO_JSVAL(global));
//    gwkjs_debug(GWKJS_DEBUG_SCOPE,
//              "  Global: %p %s",
//              global, debugstr);
//    g_free(debugstr);
//
//    parent = JS_GetGlobalForScopeChain(context);
//    chain = g_string_new(NULL);
//    while (parent != NULL) {
//        char *debug;
//        debug = gwkjs_value_debug_string(context, OBJECT_TO_JSVAL(parent));
//
//        if (chain->len > 0)
//            g_string_append(chain, ", ");
//
//        g_string_append_printf(chain, "%p %s",
//                               parent, debug);
//        g_free(debug);
//        parent = JS_GetParent(parent);
//    }
//    gwkjs_debug(GWKJS_DEBUG_SCOPE,
//              "  Chain: %s",
//              chain->str);
//    g_string_free(chain, TRUE);
//
//    JS_EndRequest(context);
//}
//

JSBool
gwkjs_log_exception_full(JSContextRef context,
                         JSValueRef      exc,
                         const gchar  *message)
{
// TODO: make a real implementation
    g_warning("UNIMPLEMENTED: gwkjs_log_exception_full");

    return TRUE;


//    jsval stack;
//    JSString *exc_str;
//    char *utf8_exception, *utf8_message;
//    gboolean is_syntax;
//
//    JS_BeginRequest(context);
//
//    is_syntax = FALSE;
//
//    if (JSVAL_IS_OBJECT(exc) &&
//        gwkjs_typecheck_boxed(context, JSVAL_TO_OBJECT(exc), NULL, G_TYPE_ERROR, FALSE)) {
//        GError *gerror;
//
//        gerror = (GError*) gwkjs_c_struct_from_boxed(context, JSVAL_TO_OBJECT(exc));
//        utf8_exception = g_strdup_printf("GLib.Error %s: %s",
//                                         g_quark_to_string(gerror->domain),
//                                         gerror->message);
//    } else {
//        if (JSVAL_IS_OBJECT(exc)) {
//            jsval js_name;
//            char *utf8_name;
//
//            if (gwkjs_object_get_property_const(context, JSVAL_TO_OBJECT(exc),
//                                              GWKJS_STRING_NAME, &js_name) &&
//                JSVAL_IS_STRING(js_name) &&
//                gwkjs_string_to_utf8(context, js_name, &utf8_name)) {
//                is_syntax = strcmp("SyntaxError", utf8_name) == 0;
//            }
//        }
//
//        exc_str = JS_ValueToString(context, exc);
//        if (exc_str != NULL)
//            gwkjs_string_to_utf8(context, STRING_TO_JSVAL(exc_str), &utf8_exception);
//        else
//            utf8_exception = NULL;
//    }
//
//    if (message != NULL)
//        gwkjs_string_to_utf8(context, STRING_TO_JSVAL(message), &utf8_message);
//    else
//        utf8_message = NULL;
//
//    /* We log syntax errors differently, because the stack for those includes
//       only the referencing module, but we want to print out the filename and
//       line number from the exception.
//    */
//
//    if (is_syntax) {
//        jsval js_lineNumber, js_fileName;
//        unsigned lineNumber;
//        char *utf8_fileName;
//
//        gwkjs_object_get_property_const(context, JSVAL_TO_OBJECT(exc),
//                                      GWKJS_STRING_LINE_NUMBER, &js_lineNumber);
//        gwkjs_object_get_property_const(context, JSVAL_TO_OBJECT(exc),
//                                      GWKJS_STRING_FILENAME, &js_fileName);
//
//        if (JSVAL_IS_STRING(js_fileName))
//            gwkjs_string_to_utf8(context, js_fileName, &utf8_fileName);
//        else
//            utf8_fileName = g_strdup("unknown");
//
//        lineNumber = JSVAL_TO_INT(js_lineNumber);
//
//        if (utf8_message) {
//            g_critical("JS ERROR: %s: %s @ %s:%u", utf8_message, utf8_exception,
//                       utf8_fileName, lineNumber);
//        } else {
//            g_critical("JS ERROR: %s @ %s:%u", utf8_exception,
//                       utf8_fileName, lineNumber);
//        }
//
//        g_free(utf8_fileName);
//    } else {
//        char *utf8_stack;
//
//        if (JSVAL_IS_OBJECT(exc) &&
//            gwkjs_object_get_property_const(context, JSVAL_TO_OBJECT(exc),
//                                          GWKJS_STRING_STACK, &stack) &&
//            JSVAL_IS_STRING(stack))
//            gwkjs_string_to_utf8(context, stack, &utf8_stack);
//        else
//            utf8_stack = NULL;
//
//        if (utf8_message) {
//            if (utf8_stack)
//                g_warning("JS ERROR: %s: %s\n%s", utf8_message, utf8_exception, utf8_stack);
//            else
//                g_warning("JS ERROR: %s: %s", utf8_message, utf8_exception);
//        } else {
//            if (utf8_stack)
//                g_warning("JS ERROR: %s\n%s", utf8_exception, utf8_stack);
//            else
//                g_warning("JS ERROR: %s", utf8_exception);
//        }
//
//        g_free(utf8_stack);
//    }
//
//    g_free(utf8_exception);
//    g_free(utf8_message);
//
//    JS_EndRequest(context);
//
//    return JS_TRUE;
}

//
//static JSBool
//log_and_maybe_keep_exception(JSContextRef  context,
//                             gboolean    keep)
//{
//    jsval exc = JSVAL_VOID;
//    JSBool retval = JS_FALSE;
//
//    JS_BeginRequest(context);
//
//    JS_AddValueRoot(context, &exc);
//    if (!JS_GetPendingException(context, &exc))
//        goto out;
//
//    JS_ClearPendingException(context);
//
//    gwkjs_log_exception_full(context, exc, NULL);
//
//    /* We clear above and then set it back so any exceptions
//     * from the logging process don't overwrite the original
//     */
//    if (keep)
//        JS_SetPendingException(context, exc);
//
//    retval = JS_TRUE;
//
// out:
//    JS_RemoveValueRoot(context, &exc);
//
//    JS_EndRequest(context);
//
//    return retval;
//}
//
JSBool
gwkjs_log_exception(JSContextRef  context, JSValueRef exception)
{
	if (!JSValueIsObject(context, exception))
		return FALSE;

	gchar *str = gwkjs_exception_to_string(context, exception);
	g_warning("Exception: %s", str);

	g_free(str);

    return TRUE;

    // TODO: Make a real implementation
    // return log_and_maybe_keep_exception(context, FALSE);
}

//JSBool
//gwkjs_log_and_keep_exception(JSContextRef context)
//{
//    return log_and_maybe_keep_exception(context, TRUE);
//}
//
//static void
//try_to_chain_stack_trace(JSContextRef src_context, JSContextRef dst_context,
//                         jsval src_exc) {
//    /* append current stack of dst_context to stack trace for src_exc.
//     * we bail if anything goes wrong, just using the src_exc unmodified
//     * in that case. */
//    jsval chained, src_stack, dst_stack, new_stack;
//    JSString *new_stack_str;
//
//    JS_BeginRequest(src_context);
//    JS_BeginRequest(dst_context);
//
//    if (!JSVAL_IS_OBJECT(src_exc))
//        goto out; // src_exc doesn't have a stack trace
//
//    /* create a new exception in dst_context to get a stack trace */
//    gwkjs_throw_literal(dst_context, "Chained exception");
//    if (!(JS_GetPendingException(dst_context, &chained) &&
//          JSVAL_IS_OBJECT(chained)))
//        goto out; // gwkjs_throw_literal didn't work?!
//    JS_ClearPendingException(dst_context);
//
//    /* get stack trace for src_exc and chained */
//    if (!(JS_GetProperty(dst_context, JSVAL_TO_OBJECT(chained),
//                         "stack", &dst_stack) &&
//          JSVAL_IS_STRING(dst_stack)))
//        goto out; // couldn't get chained stack
//    if (!(JS_GetProperty(src_context, JSVAL_TO_OBJECT(src_exc),
//                         "stack", &src_stack) &&
//          JSVAL_IS_STRING(src_stack)))
//        goto out; // couldn't get source stack
//
//    /* add chained exception's stack trace to src_exc */
//    new_stack_str = JS_ConcatStrings
//        (dst_context, JSVAL_TO_STRING(src_stack), JSVAL_TO_STRING(dst_stack));
//    if (new_stack_str==NULL)
//        goto out; // couldn't concatenate src and dst stacks?!
//    new_stack = STRING_TO_JSVAL(new_stack_str);
//    JS_SetProperty(dst_context, JSVAL_TO_OBJECT(src_exc), "stack", &new_stack);
//
// out:
//    JS_EndRequest(dst_context);
//    JS_EndRequest(src_context);
//}

JSBool
gwkjs_move_exception(JSContextRef      src_context,
                   JSContextRef      dest_context)
{
    gwkjs_throw(src_context, "gwkjs_move_exception NOT IMPLEMENTED!");
    return TRUE;
//TODO: IMPLEMENT
//    JSBool success;
//
//    JS_BeginRequest(src_context);
//    JS_BeginRequest(dest_context);
//
//    /* NOTE: src and dest could be the same. */
//    jsval exc;
//    if (JS_GetPendingException(src_context, &exc)) {
//        if (src_context != dest_context) {
//            /* try to add the current stack of dest_context to the
//             * stack trace of exc */
//            try_to_chain_stack_trace(src_context, dest_context, exc);
//            /* move the exception to dest_context */
//            JS_SetPendingException(dest_context, exc);
//            JS_ClearPendingException(src_context);
//        }
//        success = JS_TRUE;
//    } else {
//        success = JS_FALSE;
//    }
//
//    JS_EndRequest(dest_context);
//    JS_EndRequest(src_context);
//
//    return success;
}

JSBool
gwkjs_call_function_value(JSContextRef      context,
                        JSObjectRef       obj,
                        jsval           fval,
                        unsigned        argc,
                        const JSValueRef argv[],
                        jsval          *rval)
{
    JSObjectRef fobj = NULL;
    JSValueRef ret = NULL;
    JSValueRef exception = NULL;

    if (!JSValueIsObject(context, fval)) {
        g_warning("Function %p cannot be called as it's not a object", fval);
        return FALSE;
    }
    fobj = JSValueToObject(context, fval, NULL);

    if (!JSObjectIsFunction(context, fobj)) {
        g_warning("Function %p cannot be called as it's not a object", fval);
        return FALSE;
    }


    ret = JSObjectCallAsFunction(context, obj, fobj,
                                 argc, argv, &exception);
    if (rval)
        *rval = ret;

    if (exception) {
        g_warning("*** EXCEPTION gwkjs_call_function_value: %s", gwkjs_exception_to_string(context, exception));
    }

    gwkjs_schedule_gc_if_needed(context);

    return TRUE;
}

//static JSBool
//log_prop(JSContextRef  context,
//         JSObjectRef   obj,
//         jsval       id,
//         jsval      *value_p,
//         const char *what)
//{
//    if (JSVAL_IS_STRING(id)) {
//        char *name;
//
//        gwkjs_string_to_utf8(context, id, &name);
//        gwkjs_debug(GWKJS_DEBUG_PROPS,
//                  "prop %s: %s",
//                  name, what);
//        g_free(name);
//    } else if (JSVAL_IS_INT(id)) {
//        gwkjs_debug(GWKJS_DEBUG_PROPS,
//                  "prop %d: %s",
//                  JSVAL_TO_INT(id), what);
//    } else {
//        gwkjs_debug(GWKJS_DEBUG_PROPS,
//                  "prop not-sure-what: %s",
//                  what);
//    }
//
//    return JS_TRUE;
//}
//
//JSBool
//gwkjs_get_prop_verbose_stub(JSContextRef context,
//                          JSObjectRef  obj,
//                          jsval      id,
//                          jsval     *value_p)
//{
//    return log_prop(context, obj, id, value_p, "get");
//}
//
//JSBool
//gwkjs_set_prop_verbose_stub(JSContextRef context,
//                          JSObjectRef  obj,
//                          jsval      id,
//                          jsval     *value_p)
//{
//    return log_prop(context, obj, id, value_p, "set");
//}
//
//JSBool
//gwkjs_add_prop_verbose_stub(JSContextRef context,
//                          JSObjectRef  obj,
//                          jsval      id,
//                          jsval     *value_p)
//{
//    return log_prop(context, obj, id, value_p, "add");
//}
//
//JSBool
//gwkjs_delete_prop_verbose_stub(JSContextRef context,
//                             JSObjectRef  obj,
//                             jsval      id,
//                             jsval     *value_p)
//{
//    return log_prop(context, obj, id, value_p, "delete");
//}
//
/* get a debug string for type tag in jsval */
const char*
gwkjs_get_type_name(JSContextRef context, JSValueRef value)
{
    JSType t = JSValueGetType(context, value);
    if (value == NULL) {
        return "NULL";
    }
    switch (t) {
        case kJSTypeUndefined:
            return "undefined";
        case kJSTypeNull:
            return "null";
        case kJSTypeBoolean:
            return "boolean";
        case kJSTypeNumber:
            return "number";
        case kJSTypeString:
            return "string";
        case kJSTypeObject:
            return "object";

        default:
            return "<unknown>";
    }
}

/**
 * gwkjs_value_to_int64:
 * @context: the Javascript context object
 * @val: Javascript value to convert
 * @gint64: location to store the return value
 *
 * Converts a Javascript value into the nearest 64 bit signed value.
 *
 * This function behaves indentically for rounding to JSValToInt32(), which
 * means that it rounds (0.5 toward positive infinity) rather than doing
 * a C-style truncation to 0. If we change to using JSValToEcmaInt32() then
 * this should be changed to match.
 *
 * Return value: If the javascript value converted to a number (see
 *   JS_ValueToNumber()) is NaN, or outside the range of 64-bit signed
 *   numbers, fails and sets an exception. Otherwise returns the value
 *   rounded to the nearest 64-bit integer. Like JS_ValueToInt32(),
 *   undefined throws, but null => 0, false => 0, true => 1.
 */
JSBool
gwkjs_value_to_int64  (JSContextRef  context,
                     const jsval val,
                     gint64     *result)
{
    JSValueRef exception = NULL;
    if (JSValueIsNumber (context, val)) {
        double value_double = JSValueToNumber(context, val, &exception);
        if (exception) {
            return JS_FALSE;

            gwkjs_throw(context,
                      "Value is not a valid 64-bit integer");
        }

        *result = (gint64)(value_double + 0.5);
        return JS_TRUE;
    }
    return FALSE;
}

//static JSBool
//gwkjs_parse_args_valist (JSContextRef  context,
//                       const char *function_name,
//                       const char *format,
//                       unsigned    argc,
//                       jsval      *argv,
//                       va_list     args)
//{
//    guint i;
//    const char *fmt_iter;
//    guint n_unwind = 0;
//#define MAX_UNWIND_STRINGS 16
//    gpointer unwind_strings[MAX_UNWIND_STRINGS];
//    gboolean ignore_trailing_args = FALSE;
//    guint n_required = 0;
//    guint n_total = 0;
//    guint consumed_args;
//
//    JS_BeginRequest(context);
//
//    if (*format == '!') {
//        ignore_trailing_args = TRUE;
//        format++;
//    }
//
//    for (fmt_iter = format; *fmt_iter; fmt_iter++) {
//        switch (*fmt_iter) {
//        case '|':
//            n_required = n_total;
//            continue;
//        case '?':
//            continue;
//        default:
//            break;
//        }
//
//        n_total++;
//    }
//
//    if (n_required == 0)
//        n_required = n_total;
//
//    if (argc < n_required || (argc > n_total && !ignore_trailing_args)) {
//        if (n_required == n_total) {
//            gwkjs_throw(context, "Error invoking %s: Expected %d arguments, got %d", function_name,
//                      n_required, argc);
//        } else {
//            gwkjs_throw(context, "Error invoking %s: Expected minimum %d arguments (and %d optional), got %d", function_name,
//                      n_required, n_total - n_required, argc);
//        }
//        goto error_unwind;
//    }
//
//    /* We have 3 iteration variables here.
//     * @i: The current integer position in fmt_args
//     * @fmt_iter: A pointer to the character in fmt_args
//     * @consumed_args: How many arguments we've taken from argv
//     *
//     * consumed_args can currently be different from 'i' because of the '|' character.
//     */
//    for (i = 0, consumed_args = 0, fmt_iter = format; *fmt_iter; fmt_iter++, i++) {
//        const char *argname;
//        gpointer arg_location;
//        jsval js_value;
//        const char *arg_error_message = NULL;
//
//        if (*fmt_iter == '|')
//            continue;
//
//        if (consumed_args == argc) {
//            break;
//        }
//
//        argname = va_arg (args, char *);
//        arg_location = va_arg (args, gpointer);
//
//        g_return_val_if_fail (argname != NULL, JS_FALSE);
//        g_return_val_if_fail (arg_location != NULL, JS_FALSE);
//
//        js_value = argv[consumed_args];
//
//        if (*fmt_iter == '?') {
//            fmt_iter++;
//
//            if (JSVAL_IS_NULL (js_value)) {
//                gpointer *arg = (gpointer*) arg_location;
//                *arg = NULL;
//                goto got_value;
//            }
//        }
//
//        switch (*fmt_iter) {
//        case 'b': {
//            if (!JSVAL_IS_BOOLEAN(js_value)) {
//                arg_error_message = "Not a boolean";
//            } else {
//                gboolean *arg = (gboolean*) arg_location;
//                *arg = JSVAL_TO_BOOLEAN(js_value);
//            }
//        }
//            break;
//        case 'o': {
//            if (!JSVAL_IS_OBJECT(js_value)) {
//                arg_error_message = "Not an object";
//            } else {
//                JSObjectRef *arg = (JSObject**) arg_location;
//                *arg = JSVAL_TO_OBJECT(js_value);
//            }
//        }
//            break;
//        case 's': {
//            char **arg = (char**) arg_location;
//
//            if (gwkjs_string_to_utf8 (context, js_value, arg)) {
//                unwind_strings[n_unwind++] = *arg;
//                g_assert(n_unwind < MAX_UNWIND_STRINGS);
//            } else {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to string";
//            }
//        }
//            break;
//        case 'F': {
//            char **arg = (char**) arg_location;
//
//            if (gwkjs_string_to_filename (context, js_value, arg)) {
//                unwind_strings[n_unwind++] = *arg;
//                g_assert(n_unwind < MAX_UNWIND_STRINGS);
//            } else {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to filename";
//            }
//        }
//            break;
//        case 'i': {
//            if (!JS_ValueToInt32(context, js_value, (gint32*) arg_location)) {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to integer";
//            }
//        }
//            break;
//        case 'u': {
//            gdouble num;
//            if (!JSVAL_IS_NUMBER(js_value) || !JS_ValueToNumber(context, js_value, &num)) {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to unsigned integer";
//            } else if (num > G_MAXUINT32 || num < 0) {
//                arg_error_message = "Value is out of range";
//            } else {
//                *((guint32*) arg_location) = num;
//            }
//        }
//            break;
//        case 't': {
//            if (!gwkjs_value_to_int64(context, js_value, (gint64*) arg_location)) {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to 64-bit integer";
//            }
//        }
//            break;
//        case 'f': {
//            double num;
//            if (!JS_ValueToNumber(context, js_value, &num)) {
//                /* Our error message is going to be more useful */
//                JS_ClearPendingException(context);
//                arg_error_message = "Couldn't convert to double";
//            } else {
//                *((double*) arg_location) = num;
//            }
//        }
//            break;
//        default:
//            g_assert_not_reached ();
//        }
//
//    got_value:
//        if (arg_error_message != NULL) {
//            gwkjs_throw(context, "Error invoking %s, at argument %d (%s): %s", function_name,
//                      consumed_args+1, argname, arg_error_message);
//            goto error_unwind;
//        }
//
//        consumed_args++;
//    }
//
//    JS_EndRequest(context);
//    return JS_TRUE;
//
// error_unwind:
//    /* We still own the strings in the error case, free any we converted */
//    for (i = 0; i < n_unwind; i++) {
//        g_free (unwind_strings[i]);
//    }
//    JS_EndRequest(context);
//    return JS_FALSE;
//}
//
///**
// * gwkjs_parse_args:
// * @context:
// * @function_name: The name of the function being called
// * @format: Printf-like format specifier containing the expected arguments
// * @argc: Number of JavaScript arguments
// * @argv: JavaScript argument array
// * @Varargs: for each character in @format, a pair of a char * which is the name
// * of the argument, and a pointer to a location to store the value. The type of
// * value stored depends on the format character, as described below.
// *
// * This function is inspired by Python's PyArg_ParseTuple for those
// * familiar with it.  It takes a format specifier which gives the
// * types of the expected arguments, and a list of argument names and
// * value location pairs.  The currently accepted format specifiers are:
// *
// * b: A boolean
// * s: A string, converted into UTF-8
// * F: A string, converted into "filename encoding" (i.e. active locale)
// * i: A number, will be converted to a C "gint32"
// * u: A number, converted into a C "guint32"
// * t: A 64-bit number, converted into a C "gint64" by way of gwkjs_value_to_int64()
// * o: A JavaScript object, as a "JSObjectRef "
// *
// * If the first character in the format string is a '!', then JS is allowed
// * to pass extra arguments that are ignored, to the function.
// *
// * The '|' character introduces optional arguments.  All format specifiers
// * after a '|' when not specified, do not cause any changes in the C
// * value location.
// *
// * A prefix character '?' means that the next value may be null, in
// * which case the C value %NULL is returned.
// */
//JSBool
//gwkjs_parse_args (JSContextRef  context,
//                const char *function_name,
//                const char *format,
//                unsigned    argc,
//                jsval      *argv,
//                ...)
//{
//    va_list args;
//    JSBool ret;
//    va_start (args, argv);
//    ret = gwkjs_parse_args_valist (context, function_name, format, argc, argv, args);
//    va_end (args);
//    return ret;
//}
//
//JSBool
//gwkjs_parse_call_args (JSContextRef    context,
//                     const char   *function_name,
//                     const char   *format,
//                     JS::CallArgs &call_args,
//                     ...)
//{
//    va_list args;
//    JSBool ret;
//    va_start (args, call_args);
//    ret = gwkjs_parse_args_valist (context, function_name, format, call_args.length(), call_args.array(), args);
//    va_end (args);
//    return ret;
//}
//
//#ifdef __linux__
//static void
//_linux_get_self_process_size (gulong *vm_size,
//                              gulong *rss_size)
//{
//    char *contents;
//    char *iter;
//    gsize len;
//    int i;
//
//    *vm_size = *rss_size = 0;
//
//    if (!g_file_get_contents ("/proc/self/stat", &contents, &len, NULL))
//        return;
//
//    iter = contents;
//    /* See "man proc" for where this 22 comes from */
//    for (i = 0; i < 22; i++) {
//        iter = strchr (iter, ' ');
//        if (!iter)
//            goto out;
//        iter++;
//    }
//    sscanf (iter, " %lu", vm_size);
//    iter = strchr (iter, ' ');
//    if (iter)
//        sscanf (iter, " %lu", rss_size);
//
// out:
//    g_free (contents);
//}
//
//static gulong linux_rss_trigger;
//static gint64 last_gc_time;
//#endif
//
//void
//gwkjs_gc_if_needed (JSContextRef context)
//{
//#ifdef __linux__
//    {
//        /* We initiate a GC if VM or RSS has grown by this much */
//        gulong vmsize;
//        gulong rss_size;
//        gint64 now;
//
//        /* We rate limit GCs to at most one per 5 frames.
//           One frame is 16666 microseconds (1000000/60)*/
//        now = g_get_monotonic_time();
//        if (now - last_gc_time < 5 * 16666)
//            return;
//
//        _linux_get_self_process_size (&vmsize, &rss_size);
//
//        /* linux_rss_trigger is initialized to 0, so currently
//         * we always do a full GC early.
//         *
//         * Here we see if the RSS has grown by 25% since
//         * our last look; if so, initiate a full GC.  In
//         * theory using RSS is bad if we get swapped out,
//         * since we may be overzealous in GC, but on the
//         * other hand, if swapping is going on, better
//         * to GC.
//         */
//        if (rss_size > linux_rss_trigger) {
//            linux_rss_trigger = (gulong) MIN(G_MAXULONG, rss_size * 1.25);
//            JS_GC(JS_GetRuntime(context));
//            last_gc_time = now;
//        } else if (rss_size < (0.75 * linux_rss_trigger)) {
//            /* If we've shrunk by 75%, lower the trigger */
//            linux_rss_trigger = (rss_size * 1.25);
//        }
//    }
//#endif
//}
//
///**
// * gwkjs_maybe_gc:
// *
// * Low level version of gwkjs_context_maybe_gc().
// */
//void
//gwkjs_maybe_gc (JSContextRef context)
//{
//    JS_MaybeGC(context);
//    gwkjs_gc_if_needed(context);
//}
//

void
gwkjs_schedule_gc_if_needed (JSContextRef context)
{

    JSGarbageCollect(context);
//TODO: Check if it's OK. we don't have a 
//      Maybe_GC in JSC
//
//
//    GwkjsContext *gwkjs_context;
//
//    /* We call JS_MaybeGC immediately, but defer a check for a full
//     * GC cycle to an idle handler.
//     */
//    JS_MaybeGC(context);
//
//    gwkjs_context = (GwkjsContext *) JS_GetContextPrivate(context);
//    if (gwkjs_context)
//        _gwkjs_context_schedule_gc_if_needed(gwkjs_context);
}


/**
 * gwkjs_strip_unix_shebang:
 *
 * @script: (in): A pointer to a JS script
 * @script_len: (inout): A pointer to the script length. The
 * pointer will be modified if a shebang is stripped.
 * @new_start_line_number: (out) (allow-none): A pointer to
 * write the start-line number to account for the offset
 * as a result of stripping the shebang.
 *
 * Returns a pointer to the beginning of a script with unix
 * shebangs removed. The outparams are useful to know the
 * new length of the script and on what line of the
 * original script we're executing from, so that any relevant
 * offsets can be applied to the results of an execution pass.
 */
const char *
gwkjs_strip_unix_shebang(const char  *script,
                       gssize      *script_len,
                       int         *start_line_number_out)
{
    g_assert(script_len);

    /* handle scripts with UNIX shebangs */
    if (strncmp(script, "#!", 2) == 0) {
        /* If we found a newline, advance the script by one line */
        const char *s = (const char *) strstr (script, "\n");
        if (s != NULL) {
            if (*script_len > 0)
                *script_len -= (s + 1 - script);
            script = s + 1;

            if (start_line_number_out)
                *start_line_number_out = 2;

            return script;
        } else {
            /* Just a shebang */
            if (start_line_number_out)
                *start_line_number_out = -1;

            *script_len = 0;

            return NULL;
        }
    }

    /* No shebang, return the original script */
    if (start_line_number_out)
        *start_line_number_out = 1;

    return script;
}

JSBool
gwkjs_eval_with_scope(JSContextRef context,
                      JSObjectRef  object,
                      const char   *script,
                      gssize       script_len,
                      const char   *filename,
                      JSValueRef   *retval_p,
					  JSValueRef   *exception)
{
    int start_line_number = 1;
    JSValueRef retval = NULL;
    JSValueRef locException = NULL;

    if (script_len < 0)
        script_len = strlen(script);

    script = gwkjs_strip_unix_shebang(script,
                                    &script_len,
                                    &start_line_number);

// TODO: check if it's necessary, I guess not
//    /* log and clear exception if it's set (should not be, normally...) */
//    if (JS_IsExceptionPending(context)) {
//        g_warning("gwkjs_eval_in_scope called with a pending exception");
//        return JS_FALSE;
//    }

    if (!object)
        object = JSObjectMake(context, NULL, NULL);

    JSStringRef jsscript = gwkjs_cstring_to_jsstring(script);
    JSStringRef jsfilename = NULL;
    if (filename)
        jsfilename = gwkjs_cstring_to_jsstring(filename);

    retval = JSEvaluateScript(context, jsscript, object, jsfilename, start_line_number, &locException);
	if (JSValueIsObject(context, locException)) {
	    if (exception)
	        *exception = locException;

		// There was a problem during script execution,
		// We will return an exception instead
		return FALSE;
	}

// TODO: check if needs implementation
//    gwkjs_schedule_gc_if_needed(context);
//
//    if (JS_IsExceptionPending(context)) {
//        g_warning("EvaluateScript returned JS_TRUE but exception was pending; "
//                  "did somebody call gwkjs_throw() without returning JS_FALSE?");
//        return JS_FALSE;
//    }
//
    gwkjs_debug(GWKJS_DEBUG_CONTEXT,
                "Script evaluation succeeded");

    if (retval_p)
        *retval_p = retval;

    return TRUE;
}
