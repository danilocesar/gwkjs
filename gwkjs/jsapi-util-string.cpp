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

#include "jsapi-util.h"
#include "compat.h"

gboolean
gwkjs_string_to_utf8 (JSContextRef  context,
                    const jsval value,
                    char      **utf8_string_p)
{
    JSStringRef str;
    gsize len;
    char *bytes;


    if (!JSValueIsString(context, value)) {
        gwkjs_throw(context,
                  "Value is not a string, cannot convert to UTF-8");
        return JS_FALSE;
    }

    str = JSValueToStringCopy(context, value, NULL);
    len = JSStringGetMaximumUTF8CStringSize(str);
    if (len == (gsize)(-1)) {
        return JS_FALSE;
    }

    if (utf8_string_p) {
        bytes = (gchar *)g_malloc(len * sizeof(gchar));
        JSStringGetUTF8CString(str, bytes, len);
        *utf8_string_p = bytes;
    }

    return JS_TRUE;
}

JSBool
gwkjs_string_from_utf8(JSContextRef  context,
                     const char      *utf8_string,
                     gssize          n_bytes,
                     JSValueRef      *value_p)
{
    JSChar *u16_string;
    glong u16_string_length;
    JSStringRef str;
    GError *error;

    /* intentionally using n_bytes even though glib api suggests n_chars; with
    * n_chars (from g_utf8_strlen()) the result appears truncated
    */

    error = NULL;
    u16_string = g_utf8_to_utf16(utf8_string,
                                 n_bytes,
                                 NULL,
                                 &u16_string_length,
                                 &error);
    if (!u16_string) {
        gwkjs_throw(context,
                  "Failed to convert UTF-8 string to "
                  "JS string: %s",
                  error->message);
                  g_error_free(error);
        return JS_FALSE;
    }


    /* Avoid a copy - assumes that g_malloc == js_malloc == malloc */
    str = JSStringCreateWithCharacters(u16_string, u16_string_length);

    if (str && value_p)
        *value_p = JSValueMakeString(context, str);

    return str != NULL;
}

gboolean
gwkjs_string_to_filename(JSContextRef    context,
                       const jsval   filename_val,
                       char        **filename_string_p)
{
    GError *error;
    gchar *tmp, *filename_string;

    /* gwkjs_string_to_filename verifies that filename_val is a string */

    if (!gwkjs_string_to_utf8(context, filename_val, &tmp)) {
        /* exception already set */
        return JS_FALSE;
    }

    error = NULL;
    filename_string = g_filename_from_utf8(tmp, -1, NULL, NULL, &error);
    if (!filename_string) {
        gwkjs_throw_g_error(context, error);
        g_free(tmp);
        return FALSE;
    }

    *filename_string_p = filename_string;
    g_free(tmp);
    return TRUE;
}

JSBool
gwkjs_string_from_filename(JSContextRef  context,
                         const char *filename_string,
                         gssize      n_bytes,
                         jsval      *value_p)
{
    gsize written;
    GError *error;
    gchar *utf8_string;

    error = NULL;
    utf8_string = g_filename_to_utf8(filename_string, n_bytes, NULL,
                                     &written, &error);
    if (error) {
        gwkjs_throw(context,
                  "Could not convert UTF-8 string '%s' to a filename: '%s'",
                  filename_string,
                  error->message);
        g_error_free(error);
        g_free(utf8_string);
        return JS_FALSE;
    }

    if (!gwkjs_string_from_utf8(context, utf8_string, written, value_p))
        return JS_FALSE;

    g_free(utf8_string);

    return JS_TRUE;
}

/**
 * gwkjs_string_get_uint16_data:
 * @context: js context
 * @value: a jsval
 * @data_p: address to return allocated data buffer
 * @len_p: address to return length of data (number of 16-bit integers)
 *
 * Get the binary data (as a sequence of 16-bit integers) in the JSString
 * contained in @value.
 * Throws a JS exception if value is not a string.
 *
 * Returns: JS_FALSE if exception thrown
 **/
JSBool
gwkjs_string_get_uint16_data(JSContextRef       context,
                           jsval            value,
                           guint16        **data_p,
                           gsize           *len_p)
{
    const JSChar *js_data = NULL;
    JSStringRef str = NULL;
    JSBool retval = JS_FALSE;


    if (!JSValueIsString(context, value)) {
        gwkjs_throw(context,
                  "Value is not a string, can't return binary data from it");
        goto out;
    }
    str = JSValueToStringCopy(context, value, NULL);


    js_data = JSStringGetCharactersPtr(str);
// TODO: Check if this convertion is correct
    *len_p = JSStringGetLength(str);
    if (js_data == NULL)
        goto out;

    *data_p = (guint16*) g_memdup(js_data, sizeof(*js_data)*(*len_p));

    retval = JS_TRUE;
out:
    if (str)
        JSStringRelease(str);
    return retval;
}

///**
// * gwkjs_get_string_id:
// * @context: a #JSContext
// * @id: a jsid that is an object hash key (could be an int or string)
// * @name_p place to store ASCII string version of key
// *
// * If the id is not a string ID, return false and set *name_p to %NULL.
// * Otherwise, return true and fill in *name_p with ASCII name of id.
// *
// * Returns: true if *name_p is non-%NULL
// **/
//JSBool
//gwkjs_get_string_id (JSContextRef       context,
//                   jsid             id,
//                   char           **name_p)
//{
//    jsval id_val;
//
//    if (!JS_IdToValue(context, id, &id_val))
//        return JS_FALSE;
//
//    if (JSVAL_IS_STRING(id_val)) {
//        return gwkjs_string_to_utf8(context, id_val, name_p);
//    } else {
//        *name_p = NULL;
//        return JS_FALSE;
//    }
//}

/**
 * gwkjs_unichar_from_string:
 * @string: A string
 * @result: (out): A unicode character
 *
 * If successful, @result is assigned the Unicode codepoint
 * corresponding to the first full character in @string.  This
 * function handles characters outside the BMP.
 *
 * If @string is empty, @result will be 0.  An exception will
 * be thrown if @string can not be represented as UTF-8.
 */
gboolean
gwkjs_unichar_from_string (JSContextRef context,
                         jsval      value,
                         gunichar  *result)
{
    char *utf8_str;
    if (gwkjs_string_to_utf8(context, value, &utf8_str)) {
        *result = g_utf8_get_char(utf8_str);
        g_free(utf8_str);
        return TRUE;
    }
    return FALSE;
}

//jsid
//gwkjs_intern_string_to_id (JSContextRef  context,
//                         const char *string)
//{
//    JSString *str;
//    jsid id;
//    JS_BeginRequest(context);
//    str = JS_InternString(context, string);
//    id = INTERNED_STRING_TO_JSID(context, str);
//    JS_EndRequest(context);
//    return id;
//}
