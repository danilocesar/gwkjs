/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2009  litl, LLC
 * Copyright (c) 2010  Red Hat, Inc.
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

#if !defined (__GWKJS_GWKJS_MODULE_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs-module.h> can be included directly."
#endif

#ifndef __GWKJS_COMPAT_H__
#define __GWKJS_COMPAT_H__

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wstrict-prototypes\"")
_Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
#endif
#include <JavaScriptCore/JavaScript.h>
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
_Pragma("GCC diagnostic pop")
#endif
#include <glib.h>

// JSBool is now a gboolean
#define JSBool bool
#define jsval JSValueRef
#define JS_FALSE FALSE
#define JS_TRUE TRUE
#define jsid JSStringRef


#include <gwkjs/jsapi-util.h>

G_BEGIN_DECLS

/* This file inspects jsapi.h and attempts to provide a compatibility shim.
 * See https://bugzilla.gnome.org/show_bug.cgi?id=622896 for some initial discussion.
 */

// XXX: remove
#define JSVAL_IS_OBJECT(obj) (JSVAL_IS_NULL(obj) || !JSVAL_IS_PRIMITIVE(obj))

// XXX: remove eventually
#define JSVAL_IS_VOID(context, elem) (elem == NULL || JSValueIsUndefined(context, elem))
#define JSVAL_IS_NULL(context, elem) (elem == NULL || JSValueIsNull(context, elem))

#define JSVAL_IS_STRING(context, elem) (!JSVAL_IS_VOID(context, elem) && JSValueIsString(context, elem))

#define GWKJS_VALUE_IS_FUNCTION(context, elem) (elem != NULL && \
                                               JSValueIsObject(context, elem) && \
                                               JSObjectIsFunction(context, JSValueToObject(context, elem, NULL)))

#define JS_GetGlobalObject(cx) gwkjs_get_global_object(cx)

static JSBool G_GNUC_UNUSED JS_NewNumberValue(JSContextRef cx, double d, jsval *rval)
    {
        *rval = JSValueMakeNumber(cx, d);
        if (JSValueIsNumber(cx, *rval))
            return JS_TRUE;
        return JS_FALSE;
    }

/**
 * GWKJS_NATIVE_CONSTRUCTOR_DECLARE:
 * Prototype a constructor.
 */
#define GWKJS_NATIVE_CONSTRUCTOR_DECLARE(name)          \
static JSObjectRef                                      \
gwkjs_##name##_constructor(JSContextRef context,        \
                           JSObjectRef constructor,     \
                           size_t argc,                 \
                           const JSValueRef arguments[],\
                           JSValueRef* exception)

/**
 * GWKJS_NATIVE_CONSTRUCTOR_VARIABLES:
 * Declare variables necessary for the constructor; should
 * be at the very top.
 */
#define GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(name)          \
    JSObjectRef object = NULL;                                            \

/**
 * GWKJS_NATIVE_CONSTRUCTOR_PRELUDE:
 * Call after the initial variable declaration.
 */
#define GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(name)                         \
    {                                                                  \
        if (!gwkjs_##name##_class_ref) {                               \
            gwkjs_##name##_class_ref = JSClassCreate(&gwkjs_##name##_class);   \
            JSClassRetain(gwkjs_##name##_class_ref);                   \
        }                                                              \
        object = gwkjs_new_object_for_constructor(context, gwkjs_##name##_class_ref, constructor); \
        if (object == NULL) {                                          \
            g_warning("Couldn't create object");                       \
            return object;                                             \
        }                                                              \
    }

/**
 * GWKJS_NATIVE_CONSTRUCTOR_FINISH:
 * Call this at the end of a constructor when it's completed
 * successfully.
 */
#define GWKJS_NATIVE_CONSTRUCTOR_FINISH(name)             \
    return object;

/**
 * GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT:
 * Defines a constructor whose only purpose is to throw an error
 * and fail. To be used with classes that require a constructor (because they have
 * instances), but whose constructor cannot be used from JS code.
 */
#define GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(name)                          \
    GWKJS_NATIVE_CONSTRUCTOR_DECLARE(name)                                      \
    {                                                                           \
        gwkjs_throw_abstract_constructor_error(context, constructor, exception);\
        return JS_FALSE;                                                        \
    }

G_END_DECLS

#endif  /* __GWKJS_COMPAT_H__ */
