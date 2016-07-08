/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2010  litl, LLC
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
#include <glib.h>
#include <errno.h>
#include "byteArray.h"
#include "../gi/boxed.h"
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>
#include <girepository.h>
#include <util/log.h>

typedef struct {
    GByteArray *array;
    GBytes     *bytes;
} ByteArrayInstance;

extern JSClassDefinition gwkjs_byte_array_class;
static JSClassRef gwkjs_byte_array_class_ref = NULL;
GWKJS_DEFINE_PRIV_FROM_JS(ByteArrayInstance, gwkjs_byte_array_class)

//static JSValueRef byte_array_get_prop      (JSContextRef ctx,
//											JSObjectRef object,
//											JSStringRef propertyName,
//											JSValueRef* exception);
//
//static bool byte_array_set_prop      (JSContextRef ctx,
//                                      JSObjectRef object,
//                                      JSStringRef propertyName,
//                                      JSValueRef value,
//                                      JSValueRef* exception);
//GWKJS_NATIVE_CONSTRUCTOR_DECLARE(byte_array);
//
//static void   byte_array_finalize      (JSObjectRef  obj);
//
//
//JSClassDefinition gwkjs_byte_array_class = {
//    0,                            /* Version, always 0 */
//    0,                            // Flags: We're ignoring JSCLASS_HAS_PRIVATE and
//                                  // JSCLASS_BACKGROUND_FINALIZE,
//    "ByteArray",                  /* Class Name */
//    0,                            /* Parent Class */
//    NULL,                         /* Static Values */
//    NULL,                         /* Static Functions */
//	NULL,                         // Initialize
//	byte_array_finalize,          /* Finalize */
//    NULL,                         /* Has Property */
//    byte_array_get_prop,          /* Get Property */
//    byte_array_set_prop,          /* Set Property */
//    NULL,                         /* Delete Property */
//    NULL,                         /* Get Property Names */
//    NULL,                         /* Call As Function */
//    NULL,                         /* Call As Constructor */
//    NULL,                         /* Has Instance */
//    NULL                          /* Convert To Type */
//};
//
JSBool
gwkjs_typecheck_bytearray(JSContextRef context,
                          JSObjectRef      object,
                          JSBool         throw_error)
{
    gwkjs_throw(context, " gwkjs_typecheck_bytearray  not implemented");
    return FALSE;
//TODO: implement
//    return do_base_typecheck(context, object, throw_error);
}

///* Seems to be unecessary and not doable, as JSC doesn't provide a INT_MAX
//static JSBool
//gwkjs_value_from_gsize(JSContextRef context,
//	                   gsize v,
//                       JS::MutableHandleValue value_p)
//{
//    if (v > (gsize) JSVAL_INT_MAX) {
//        value_p.set(INT_TO_JSVAL(v));
//        return JS_TRUE;
//    } else {
//        return JS_NewNumberValue(context, v, value_p.address());
//    }
//}
//*/
//
//static void
//byte_array_ensure_array (ByteArrayInstance  *priv)
//{
//    if (priv->bytes) {
//        priv->array = g_bytes_unref_to_array(priv->bytes);
//        priv->bytes = NULL;
//    } else {
//        g_assert(priv->array);
//    }
//}
//
//static void
//byte_array_ensure_gbytes (ByteArrayInstance  *priv)
//{
//    if (priv->array) {
//        priv->bytes = g_byte_array_free_to_bytes(priv->array);
//        priv->array = NULL;
//    } else {
//        g_assert(priv->bytes);
//    }
//}
//
//static JSBool
//gwkjs_value_to_gsize(JSContextRef    ctx,
//					 JSValueRef      value,
//					 gsize           *v_p)
//{
//    guint32 val32;
//	double i = JSValueToNumber(ctx, value, NULL);
//	if (i < 0) {
//		gwkjs_throw(ctx, "Negative length or index %f is not allowed for ByteArray", i);
//		return FALSE;
//	}
//	val32 = i;
//	*v_p = val32;
//	return TRUE;
//}
//
//static JSBool
//gwkjs_value_to_byte(JSContextRef      context,
//                  	JSValueRef        value,
//                  	guint8            *v_p)
//{
//    gsize v;
//
//    if (!gwkjs_value_to_gsize(context, value, &v))
//        return JS_FALSE;
//
//    if (v >= 256) {
//        gwkjs_throw(context,
//                  "Value %" G_GSIZE_FORMAT " is not a valid byte; must be in range [0,255]",
//                  v);
//        return JS_FALSE;
//    }
//
//    *v_p = v;
//    return JS_TRUE;
//}
//
//static JSBool
//byte_array_get_index(JSContextRef     context,
//                     JSObjectRef obj,
//                     ByteArrayInstance *priv,
//                     gsize              idx,
//					 JSValueRef		   *ret)
//{
//    gsize len;
//    guint8 *data;
//    
//    gwkjs_byte_array_peek_data(context, obj, &data, &len);
//
//    if (idx >= len) {
//        gwkjs_throw(context,
//                  "Index %" G_GSIZE_FORMAT " is out of range for ByteArray length %lu",
//                  idx,
//                  (unsigned long)len);
//        return JS_FALSE;
//    }
//
//	*ret = JSValueMakeNumber(context, data[idx]);
//    return JS_TRUE;
//}
//
//static JSValueRef
//byte_array_get_prop(JSContextRef ctx,
//                    JSObjectRef object,
//                    JSStringRef propertyName,
//                    JSValueRef* exception)
//{
//    ByteArrayInstance *priv;
//
//    priv = priv_from_js(object);
//    if (priv == NULL)
//        return JSValueMakeUndefined(ctx); /* prototype, not an instance. */
//
//	char *prop = gwkjs_jsstring_to_cstring(propertyName);
//	char *err = NULL;
//	// TODO: might have problems here regarding locale
//	double index = g_ascii_strtod(prop, &err);
//	g_free(prop);
//
//	if (err)
//	    return JSValueMakeNull(ctx);
//
//	// TODO: proper error handling
//	gsize idx = index;
//	if (index >= 0) {
//		JSValueRef ret;
//		if (byte_array_get_index(ctx, object, priv, idx, &ret))
//			return ret;
//	}
//
//    return JSValueMakeNull(ctx);
//}
//
//static JSValueRef
//byte_array_length_getter(JSContextRef ctx,
//                         JSObjectRef obj,
//                         size_t argumentCount,
//                         const JSValueRef arguments[],
//                         JSValueRef* exception)
//{
//    ByteArrayInstance *priv;
//    gsize len = 0;
//
//    priv = priv_from_js(obj);
//
//    if (priv == NULL)
//        return JSValueMakeNumber(ctx, 0); /* prototype, not an instance. */
//
//    if (priv->array != NULL)
//        len = priv->array->len;
//    else if (priv->bytes != NULL)
//        len = g_bytes_get_size (priv->bytes);
//
//	JSValueRef ret;
//    if (JS_NewNumberValue(ctx, len, &ret)) {
//		return ret;
//	} else {
//		g_warning("TODO: Bad transformation of byteArray length");
//		return JSValueMakeUndefined(ctx);
//	}
//}
//
//static bool
//byte_array_length_setter(JSContextRef context,
//                         JSObjectRef obj,
//                         JSStringRef propertyName,
//                         JSValueRef value,
//                         JSValueRef* exception)
//{
//    ByteArrayInstance *priv;
//    gsize len = 0;
//
//    priv = priv_from_js(obj);
//
//    if (priv == NULL)
//        return FALSE; /* prototype, not instance */
//
//    byte_array_ensure_array(priv);
//
//    if (!gwkjs_value_to_gsize(context, value, &len)) {
//        gwkjs_throw(context,
//                  "Can't set ByteArray length to non-integer");
//        return FALSE;
//    }
//    g_byte_array_set_size(priv->array, len);
//    return TRUE;
//}
//
//static bool
//byte_array_set_index(JSContextRef       context,
//                     JSObjectRef        obj,
//                     ByteArrayInstance *priv,
//                     gsize              idx,
//                     JSValueRef         value)
//{
//    guint8 v;
//
//    if (!gwkjs_value_to_byte(context, value, &v)) {
//        return FALSE;
//    }
//
//    byte_array_ensure_array(priv);
//
//    /* grow the array if necessary */
//    if (idx >= priv->array->len) {
//        g_byte_array_set_size(priv->array,
//                              idx + 1);
//    }
//
//    g_array_index(priv->array, guint8, idx) = v;
//
//    // TODO: possible leak?
//    /* Stop JS from storing a copy of the value */
//    //value_p.set(JSVAL_VOID);
//
//    return TRUE;
//}
//
///* a hook on setting a property; set value_p to override property value to
// * be set. Return value is JS_FALSE on OOM/exception.
// */
//static bool
//byte_array_set_prop(JSContextRef context,
//                    JSObjectRef object,
//                    JSStringRef propertyName,
//                    JSValueRef value,
//                    JSValueRef* exception)
//{
//    ByteArrayInstance *priv;
//    jsval id_value;
//
//    priv = priv_from_js(object);
//
//    if (priv == NULL)
//        return FALSE; /* prototype, not an instance. */
//
//    char *prop = gwkjs_jsstring_to_cstring(propertyName);
//	char *err = NULL;
//	// TODO: might have problems here regarding locale
//	double index = g_ascii_strtod(prop, &err);
//	g_free(prop);
//
//	if (err)
//	    return JSValueMakeNull(context);
//
//    /* First handle array indexing */
//    gsize idx;
//    if (!gwkjs_value_to_gsize(context, value, &idx))
//        return FALSE;
//
//    return byte_array_set_index(context, object, priv, idx, value);
//
//    /* We don't special-case anything else for now */
//
//    return TRUE;
//}
//
//static GByteArray *
//gwkjs_g_byte_array_new(int preallocated_length)
//{
//    GByteArray *array;
//
//    /* can't use g_byte_array_new() because we need to clear to zero.
//     * We nul-terminate too for ease of toString() and for security
//     * paranoia.
//     */
//    array =  (GByteArray*) g_array_sized_new (TRUE, /* nul-terminated */
//                                              TRUE, /* clear to zero */
//                                              1, /* element size */
//                                              preallocated_length);
//   if (preallocated_length > 0) {
//       /* we want to not only allocate the size, but have it
//        * already be the array's length.
//        */
//       g_byte_array_set_size(array, preallocated_length);
//   }
//
//   return array;
//}
//
//GWKJS_NATIVE_CONSTRUCTOR_DECLARE(byte_array)
//{
//    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(byte_array)
//    ByteArrayInstance *priv;
//    gsize preallocated_length;
//
//    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(byte_array);
//
//    preallocated_length = 0;
//    if (argc >= 1) {
//        if (!gwkjs_value_to_gsize(context, arguments[0], &preallocated_length)) {
//            gwkjs_throw(context,
//                      "Argument to ByteArray constructor should be a positive number for array length");
//            return JS_FALSE;
//        }
//    }
//
//    priv = g_slice_new0(ByteArrayInstance);
//    priv->array = gwkjs_g_byte_array_new(preallocated_length);
//    g_assert(priv_from_js(context, object) == NULL);
//    JS_SetPrivate(object, priv);
//
//    GWKJS_NATIVE_CONSTRUCTOR_FINISH(byte_array);
//
//    return JS_TRUE;
//}
//
//static void
//byte_array_finalize(JSFreeOp *fop,
//                    JSObjectRef obj)
//{
//    ByteArrayInstance *priv;
//
//    priv = (ByteArrayInstance*) JS_GetPrivate(obj);
//
//    if (priv == NULL)
//        return; /* prototype, not instance */
//
//    if (priv->array) {
//        g_byte_array_free(priv->array, TRUE);
//        priv->array = NULL;
//    } else if (priv->bytes) {
//        g_clear_pointer(&priv->bytes, g_bytes_unref);
//    }
//
//    g_slice_free(ByteArrayInstance, priv);
//}
//
///* implement toString() with an optional encoding arg */
//static JSBool
//to_string_func(JSContextRef context,
//               unsigned   argc,
//               jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    JSObjectRef object = JSVAL_TO_OBJECT(argv.thisv());
//    ByteArrayInstance *priv;
//    char *encoding;
//    gboolean encoding_is_utf8;
//    gchar *data;
//
//    priv = priv_from_js(context, object);
//
//    if (priv == NULL)
//        return JS_TRUE; /* prototype, not instance */
//
//    byte_array_ensure_array(priv);
//
//    if (argc >= 1 &&
//        JSVAL_IS_STRING(argv[0])) {
//        if (!gwkjs_string_to_utf8(context, argv[0], &encoding))
//            return JS_FALSE;
//
//        /* maybe we should be smarter about utf8 synonyms here.
//         * doesn't matter much though. encoding_is_utf8 is
//         * just an optimization anyway.
//         */
//        if (strcmp(encoding, "UTF-8") == 0) {
//            encoding_is_utf8 = TRUE;
//            g_free(encoding);
//            encoding = NULL;
//        } else {
//            encoding_is_utf8 = FALSE;
//        }
//    } else {
//        encoding_is_utf8 = TRUE;
//    }
//
//    if (priv->array->len == 0)
//        /* the internal data pointer could be NULL in this case */
//        data = (gchar*)"";
//    else
//        data = (gchar*)priv->array->data;
//
//    if (encoding_is_utf8) {
//        /* optimization, avoids iconv overhead and runs
//         * libmozjs hardwired utf8-to-utf16
//         */
//        jsval retval;
//        JSBool ok;
//
//        ok = gwkjs_string_from_utf8(context,
//                                  data,
//                                  priv->array->len,
//                                  &retval);
//        if (ok)
//            argv.rval().set(retval);
//        return ok;
//    } else {
//        JSBool ok = JS_FALSE;
//        gsize bytes_written;
//        GError *error;
//        JSString *s;
//        char *u16_str;
//
//        error = NULL;
//        u16_str = g_convert(data,
//                           priv->array->len,
//                           "UTF-16",
//                           encoding,
//                           NULL, /* bytes read */
//                           &bytes_written,
//                           &error);
//        g_free(encoding);
//        if (u16_str == NULL) {
//            /* frees the GError */
//            gwkjs_throw_g_error(context, error);
//            return JS_FALSE;
//        }
//
//        /* bytes_written should be bytes in a UTF-16 string so
//         * should be a multiple of 2
//         */
//        g_assert((bytes_written % 2) == 0);
//
//        s = JS_NewUCStringCopyN(context,
//                                (jschar*) u16_str,
//                                bytes_written / 2);
//        if (s != NULL) {
//            ok = JS_TRUE;
//            argv.rval().set(STRING_TO_JSVAL(s));
//        }
//
//        g_free(u16_str);
//        return ok;
//    }
//}
//
//static JSBool
//to_gbytes_func(JSContextRef context,
//               unsigned   argc,
//               jsval     *vp)
//{
//    JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
//    JSObjectRef object = JSVAL_TO_OBJECT(rec.thisv());
//    ByteArrayInstance *priv;
//    JSObjectRef ret_bytes_obj;
//    GIBaseInfo *gbytes_info;
//
//    priv = priv_from_js(context, object);
//    if (priv == NULL)
//        return JS_TRUE; /* prototype, not instance */
//    
//    byte_array_ensure_gbytes(priv);
//
//    gbytes_info = g_irepository_find_by_gtype(NULL, G_TYPE_BYTES);
//    ret_bytes_obj = gwkjs_boxed_from_c_struct(context, (GIStructInfo*)gbytes_info,
//                                            priv->bytes, GWKJS_BOXED_CREATION_NONE);
//
//    rec.rval().set(OBJECT_TO_JSVAL(ret_bytes_obj));
//    return JS_TRUE;
//}
//
///* Ensure that the module and class objects exists, and that in turn
// * ensures that JS_InitClass has been called. */
//static JSObjectRef 
//byte_array_get_prototype(JSContextRef context)
//{
//    jsval retval;
//    JSObjectRef prototype;
//
//    retval = gwkjs_get_global_slot (context, GWKJS_GLOBAL_SLOT_BYTE_ARRAY_PROTOTYPE);
//
//    if (!JSVAL_IS_OBJECT (retval)) {
//        if (!gwkjs_eval_with_scope(context, NULL,
//                                 "imports.byteArray.ByteArray.prototype;", -1,
//                                 "<internal>", &retval))
//            g_error ("Could not import byte array prototype\n");
//    }
//
//    return JSVAL_TO_OBJECT(retval);
//}
//
//static JSObject*
//byte_array_new(JSContextRef context)
//{
//    JSObjectRef array;
//    ByteArrayInstance *priv;
//
//    array = JS_NewObject(context, &gwkjs_byte_array_class,
//                         byte_array_get_prototype(context), NULL);
//
//    priv = g_slice_new0(ByteArrayInstance);
//
//    g_assert(priv_from_js(context, array) == NULL);
//    JS_SetPrivate(array, priv);
//
//    return array;
//}
//
///* fromString() function implementation */
//static JSBool
//from_string_func(JSContextRef context,
//                 unsigned   argc,
//                 jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    ByteArrayInstance *priv;
//    char *encoding;
//    gboolean encoding_is_utf8;
//    JSObjectRef obj;
//    JSBool retval = JS_FALSE;
//
//    obj = byte_array_new(context);
//    if (obj == NULL)
//        return JS_FALSE;
//
//    JS_AddObjectRoot(context, &obj);
//
//    priv = priv_from_js(context, obj);
//    g_assert (priv != NULL);
//
//    g_assert(argc > 0); /* because we specified min args 1 */
//
//    priv->array = gwkjs_g_byte_array_new(0);
//
//    if (!JSVAL_IS_STRING(argv[0])) {
//        gwkjs_throw(context,
//                  "byteArray.fromString() called with non-string as first arg");
//        goto out;
//    }
//
//    if (argc > 1 &&
//        JSVAL_IS_STRING(argv[1])) {
//        if (!gwkjs_string_to_utf8(context, argv[1], &encoding))
//            goto out;
//
//        /* maybe we should be smarter about utf8 synonyms here.
//         * doesn't matter much though. encoding_is_utf8 is
//         * just an optimization anyway.
//         */
//        if (strcmp(encoding, "UTF-8") == 0) {
//            encoding_is_utf8 = TRUE;
//            g_free(encoding);
//            encoding = NULL;
//        } else {
//            encoding_is_utf8 = FALSE;
//        }
//    } else {
//        encoding_is_utf8 = TRUE;
//    }
//
//    if (encoding_is_utf8) {
//        /* optimization? avoids iconv overhead and runs
//         * libmozjs hardwired utf16-to-utf8.
//         */
//        char *utf8 = NULL;
//        if (!gwkjs_string_to_utf8(context,
//                                argv[0],
//                                &utf8))
//            goto out;
//
//        g_byte_array_set_size(priv->array, 0);
//        g_byte_array_append(priv->array, (guint8*) utf8, strlen(utf8));
//        g_free(utf8);
//    } else {
//        char *encoded;
//        gsize bytes_written;
//        GError *error;
//        const jschar *u16_chars;
//        gsize u16_len;
//
//        u16_chars = JS_GetStringCharsAndLength(context, JSVAL_TO_STRING(argv[0]), &u16_len);
//        if (u16_chars == NULL)
//            goto out;
//
//        error = NULL;
//        encoded = g_convert((char*) u16_chars,
//                            u16_len * 2,
//                            encoding, /* to_encoding */
//                            "UTF-16", /* from_encoding */
//                            NULL, /* bytes read */
//                            &bytes_written,
//                            &error);
//        g_free(encoding);
//        if (encoded == NULL) {
//            /* frees the GError */
//            gwkjs_throw_g_error(context, error);
//            goto out;
//        }
//
//        g_byte_array_set_size(priv->array, 0);
//        g_byte_array_append(priv->array, (guint8*) encoded, bytes_written);
//
//        g_free(encoded);
//    }
//
//    argv.rval().set(OBJECT_TO_JSVAL(obj));
//
//    retval = JS_TRUE;
// out:
//    JS_RemoveObjectRoot(context, &obj);
//    return retval;
//}
//
///* fromArray() function implementation */
//static JSBool
//from_array_func(JSContextRef context,
//                unsigned   argc,
//                jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    ByteArrayInstance *priv;
//    guint32 len;
//    guint32 i;
//    JSObjectRef obj;
//    JSBool ret = JS_FALSE;
//
//    obj = byte_array_new(context);
//    if (obj == NULL)
//        return JS_FALSE;
//
//    JS_AddObjectRoot(context, &obj);
//
//    priv = priv_from_js(context, obj);
//    g_assert (priv != NULL);
//
//    g_assert(argc > 0); /* because we specified min args 1 */
//
//    priv->array = gwkjs_g_byte_array_new(0);
//
//    if (!JS_IsArrayObject(context, JSVAL_TO_OBJECT(argv[0]))) {
//        gwkjs_throw(context,
//                  "byteArray.fromArray() called with non-array as first arg");
//        goto out;
//    }
//
//    if (!JS_GetArrayLength(context, JSVAL_TO_OBJECT(argv[0]), &len)) {
//        gwkjs_throw(context,
//                  "byteArray.fromArray() can't get length of first array arg");
//        goto out;
//    }
//
//    g_byte_array_set_size(priv->array, len);
//
//    for (i = 0; i < len; ++i) {
//        jsval elem;
//        guint8 b;
//
//        elem = JSVAL_VOID;
//        if (!JS_GetElement(context, JSVAL_TO_OBJECT(argv[0]), i, &elem)) {
//            /* this means there was an exception, while elem == JSVAL_VOID
//             * means no element found
//             */
//            goto out;
//        }
//
//        if (JSVAL_IS_VOID(elem))
//            continue;
//
//        if (!gwkjs_value_to_byte(context, elem, &b))
//            goto out;
//
//        g_array_index(priv->array, guint8, i) = b;
//    }
//
//    ret = JS_TRUE;
//    argv.rval().set(OBJECT_TO_JSVAL(obj));
// out:
//    JS_RemoveObjectRoot(context, &obj);
//    return ret;
//}
//
//static JSBool
//from_gbytes_func(JSContextRef context,
//                 unsigned   argc,
//                 jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    JSObjectRef bytes_obj;
//    GBytes *gbytes;
//    ByteArrayInstance *priv;
//    JSObjectRef obj;
//    JSBool ret = JS_FALSE;
//
//    if (!gwkjs_parse_call_args(context, "overrides_gbytes_to_array", "o", argv,
//                        "bytes", &bytes_obj))
//        return JS_FALSE;
//
//    if (!gwkjs_typecheck_boxed(context, bytes_obj, NULL, G_TYPE_BYTES, TRUE))
//        return JS_FALSE;
//
//    gbytes = (GBytes*) gwkjs_c_struct_from_boxed(context, bytes_obj);
//
//    obj = byte_array_new(context);
//    if (obj == NULL)
//        return JS_FALSE;
//    priv = priv_from_js(context, obj);
//    g_assert (priv != NULL);
//
//    priv->bytes = g_bytes_ref(gbytes);
//
//    ret = JS_TRUE;
//    argv.rval().set(OBJECT_TO_JSVAL(obj));
//    return ret;
//}
//
JSObjectRef
gwkjs_byte_array_from_byte_array (JSContextRef context,
                                GByteArray *array)
{
    gwkjs_throw(context, "gwkjs_byte_array_from_byte_array not implemented");
// TODO: implement
//    JSObjectRef object;
//    ByteArrayInstance *priv;
//
//    g_return_val_if_fail(context != NULL, NULL);
//    g_return_val_if_fail(array != NULL, NULL);
//
//    object = gwkjs_new_object(context, gwkjs_byte_array_class_ref,
//                              byte_array_get_prototype(context), NULL);
//    if (!object) {
//        gwkjs_throw(context, "failed to create byte array");
//        return NULL;
//    }
//
//    priv = g_slice_new0(ByteArrayInstance);
//    g_assert(priv_from_js(object) == NULL);
//    JSObjectSetPrivate(object, priv);
//    priv->array = g_byte_array_new();
//    priv->array->data = (guint8*) g_memdup(array->data, array->len);
//    priv->array->len = array->len;
//
//    return object;
}

//JSObjectRef 
//gwkjs_byte_array_from_bytes (JSContextRef context,
//                           GBytes    *bytes)
//{
//    JSObjectRef object;
//    ByteArrayInstance *priv;
//
//    g_return_val_if_fail(context != NULL, NULL);
//    g_return_val_if_fail(bytes != NULL, NULL);
//
//    object = JS_NewObject(context, &gwkjs_byte_array_class,
//                          byte_array_get_prototype(context), NULL);
//    if (!object) {
//        gwkjs_throw(context, "failed to create byte array");
//        return NULL;
//    }
//
//    priv = g_slice_new0(ByteArrayInstance);
//    g_assert(priv_from_js(context, object) == NULL);
//    JS_SetPrivate(object, priv);
//    priv->bytes = g_bytes_ref (bytes);
//
//    return object;
//}
//
GBytes *
gwkjs_byte_array_get_bytes (JSContextRef  context,
                          JSObjectRef   object)
{
gwkjs_throw(context, " gwkjs_byte_array_get_bytes  not implemented");
return NULL;
//TODO: implement
//    ByteArrayInstance *priv;
//    priv = priv_from_js(context, object);
//    g_assert(priv != NULL);
//
//    byte_array_ensure_gbytes(priv);
//
//    return g_bytes_ref (priv->bytes);
}

GByteArray *
gwkjs_byte_array_get_byte_array (JSContextRef   context,
                               JSObjectRef    obj)
{
gwkjs_throw(context, " gwkjs_byte_array_get_byte_array  not implemented");
return NULL;
//TODO: implement
//    ByteArrayInstance *priv;
//    priv = priv_from_js(context, obj);
//    g_assert(priv != NULL);
//
//    byte_array_ensure_array(priv);
//
//    return g_byte_array_ref (priv->array);
}

//void
//gwkjs_byte_array_peek_data (JSContextRef  context,
//                          JSObjectRef   obj,
//                          guint8    **out_data,
//                          gsize      *out_len)
//{
//    ByteArrayInstance *priv;
//    priv = priv_from_js(context, obj);
//    g_assert(priv != NULL);
//    
//    if (priv->array != NULL) {
//        *out_data = (guint8*)priv->array->data;
//        *out_len = (gsize)priv->array->len;
//    } else if (priv->bytes != NULL) {
//        *out_data = (guint8*)g_bytes_get_data(priv->bytes, out_len);
//    } else {
//        g_assert_not_reached();
//    }
//}
//
///* no idea what this is used for. examples in
// * spidermonkey use -1, -2, -3, etc. for tinyids.
// */
//enum ByteArrayTinyId {
//    BYTE_ARRAY_TINY_ID_LENGTH = -1
//};
//
//JSStaticValue gwkjs_byte_array_proto_props[] = {
//    { "length",
//      byte_array_length_getter,
//      byte_array_length_setter,
//      kJSPropertyAttributeNone
//    },
//    { NULL, NULL, NULL, NULL }
//};
//
//JSFunctionSpec gwkjs_byte_array_proto_funcs[] = {
//    { "toString", to_string_func, 0 },
//    { "toGBytes", to_gbytes_func, 0 },
//    { NULL }
//};
//
//static JSFunctionSpec gwkjs_byte_array_module_funcs[] = {
//    { "fromString", from_string_func, 0 },
//    { "fromArray", from_array_func, 0 },
//    { "fromGBytes", from_gbytes_func, 0 },
//    { NULL, NULL, 0 }
//};
//
//JSBool
//gwkjs_define_byte_array_stuff(JSContextRef  context,
//                            JSObjectRef  *module_out)
//{
//    JSObjectRef module;
//    JSObjectRef prototype;
//
//    module = JS_NewObject (context, NULL, NULL, NULL);
//
//    JSClassRef byte_array_class = JSClassCreate(&gwkjs_byte_array_class);
//    JSObjectMake(ctx, importer_class, NULL);
//
//
//    prototype = JS_InitClass(context, module,
//                             NULL,
//                             &gwkjs_byte_array_class,
//                             gwkjs_byte_array_constructor,
//                             0,
//                             &gwkjs_byte_array_proto_props[0],
//                             &gwkjs_byte_array_proto_funcs[0],
//                             NULL,
//                             NULL);
//
//    if (!JS_DefineFunctions(context, module, &gwkjs_byte_array_module_funcs[0]))
//        return JS_FALSE;
//
//    g_assert(JSVAL_IS_VOID(gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_BYTE_ARRAY_PROTOTYPE)));
//    gwkjs_set_global_slot(context, GWKJS_GLOBAL_SLOT_BYTE_ARRAY_PROTOTYPE,
//                        OBJECT_TO_JSVAL(prototype));
//
//    *module_out = module;
//    return JS_TRUE;
//}
