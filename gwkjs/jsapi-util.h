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

#ifndef __GWKJS_JSAPI_UTIL_H__
#define __GWKJS_JSAPI_UTIL_H__

#if !defined (__GWKJS_GWKJS_MODULE_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs-module.h> can be included directly."
#endif

#include <gwkjs/compat.h>
#include <gwkjs/runtime.h>
#include <glib-object.h>
#include <gi/gtype.h>

G_BEGIN_DECLS
//
//#define GWKJS_UTIL_ERROR gwkjs_util_error_quark ()
//GQuark gwkjs_util_error_quark (void);
//enum {
//  GWKJS_UTIL_ERROR_NONE,
//  GWKJS_UTIL_ERROR_ARGUMENT_INVALID,
//  GWKJS_UTIL_ERROR_ARGUMENT_UNDERFLOW,
//  GWKJS_UTIL_ERROR_ARGUMENT_OVERFLOW,
//  GWKJS_UTIL_ERROR_ARGUMENT_TYPE_MISMATCH
//};
//
//typedef enum {
//    GWKJS_GLOBAL_SLOT_IMPORTS,
//    GWKJS_GLOBAL_SLOT_KEEP_ALIVE,
//    GWKJS_GLOBAL_SLOT_BYTE_ARRAY_PROTOTYPE,
//    GWKJS_GLOBAL_SLOT_LAST,
//} GwkjsGlobalSlot;
//
//typedef struct GwkjsRootedArray GwkjsRootedArray;
//
///* Flags that should be set on properties exported from native code modules.
// * Basically set these on API, but do NOT set them on data.
// *
// * READONLY:  forbid setting prop to another value
// * PERMANENT: forbid deleting the prop
// * ENUMERATE: allows copyProperties to work among other reasons to have it
// */
//#define GWKJS_MODULE_PROP_FLAGS (JSPROP_PERMANENT | JSPROP_ENUMERATE)
//

#define GWKJS_PROTO_PROP_FLAGS (kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete)

/*
 * Helper methods to access private data:
 *
 * do_base_typecheck: checks that object has the right JSClass, and possibly
 *                    throw a TypeError exception if the check fails
 * priv_from_js: accesses the object private field; as a debug measure,
 *               it also checks that the object is of a compatible
 *               JSClass, but it doesn't raise an exception (it
 *               wouldn't be of much use, if subsequent code crashes on
 *               NULL)
 * priv_from_js_with_typecheck: a convenience function to call
 *                              do_base_typecheck and priv_from_js
 */
#define GWKJS_DEFINE_PRIV_FROM_JS(type, klass)                          \
    static inline type *                                                \
    priv_from_js(JSObjectRef object)                                    \
    {                                                                   \
        return (type *) JSObjectGetPrivate(object);                     \
    }                                                                   \
// TODO: IMPLEMENT                                                      \
//    __attribute__((unused)) static inline JSBool                        \
//    do_base_typecheck(JSContextRef context,                             \
//                      JSObjectRef  object,                              \
//                      JSBool       throw_error)                         \
//    {                                                                   \
//        return gwkjs_typecheck_instance(context, object, &klass, throw_error);  \
//    }                                                                   \
//    __attribute__((unused)) static JSBool                               \
//    priv_from_js_with_typecheck(JSContextRef context,                   \
//                                JSObjectRef  object,                    \
//                                type         **out)                     \
//    {                                                                   \
//        if (!do_base_typecheck(context, object, JS_FALSE))              \
//            return JS_FALSE;                                            \
//        *out = priv_from_js(object);                                    \
//        return JS_TRUE;                                                 \
//    }

/**
 * GWKJS_DEFINE_PROTO:
 * @tn: The name of the prototype, as a string
 * @cn: The name of the prototype, separated by _
 * @flags: additional JSClass flags, such as JSCLASS_BACKGROUND_FINALIZE
 *
 * A convenience macro for prototype implementations.
 */
#define GWKJS_DEFINE_PROTO(tn, cn, flags) \
GWKJS_NATIVE_CONSTRUCTOR_DECLARE(cn); \
_GWKJS_DEFINE_PROTO_FULL(tn, cn, gwkjs_##cn##_constructor, G_TYPE_NONE, flags)

/**
 * GWKJS_DEFINE_PROTO_ABSTRACT:
 * @tn: The name of the prototype, as a string
 * @cn: The name of the prototype, separated by _
 *
 * A convenience macro for prototype implementations.
 * Similar to GWKJS_DEFINE_PROTO but marks the prototype as abstract,
 * you won't be able to instantiate it using the new keyword
 */
#define GWKJS_DEFINE_PROTO_ABSTRACT(tn, cn, flags) \
_GWKJS_DEFINE_PROTO_FULL(tn, cn, NULL, G_TYPE_NONE, flags)

#define GWKJS_DEFINE_PROTO_WITH_GTYPE(tn, cn, gtype, flags)   \
GWKJS_NATIVE_CONSTRUCTOR_DECLARE(cn); \
_GWKJS_DEFINE_PROTO_FULL(tn, cn, gwkjs_##cn##_constructor, gtype, flags)

#define GWKJS_DEFINE_PROTO_ABSTRACT_WITH_GTYPE(tn, cn, gtype, flags)   \
_GWKJS_DEFINE_PROTO_FULL(tn, cn, NULL, gtype, flags)

#define _GWKJS_DEFINE_PROTO_FULL(type_name, cname, ctor, gtype, jsclass_flags)     \
extern JSStaticValue gjs_##cname##_proto_props[]; \
extern JSStaticFunction gjs_##cname##_proto_funcs[]; \
static void gwkjs_##cname##_finalize(JSObjectRef obj); \
JSClassDefinition gwkjs_##cname##_class = { \
    0,                         \
    kJSPropertyAttributeNone | jsclass_flags, \
    type_name,                  \
    NULL,                      \
    NULL,                      \
    NULL,                      \
    NULL,                      \
    gwkjs_##cname##_finalize,  \
    NULL,                      /* hasProperty */\
    NULL,                      /* TODO: getProperty is NULL \
                                but it used to have a  new_resolve in gjs; */ \
    NULL,                      \
    NULL,                      \
    NULL,                      \
    NULL,                      \
    ctor,                      \
    NULL,                      \
    NULL,                      \
}; \
JSClassRef gwkjs_##cname##_class_ref = NULL; \
JSObjectRef gwkjs_##cname##_create_proto(JSContextRef context, JSObjectRef module, const char *proto_name, JSObjectRef parent) \
{ \
    JSValueRef exception = NULL; \
    JSValueRef rval = NULL; \
    if (!gwkjs_##cname##_class_ref) {  \
        gwkjs_##cname##_class_ref = JSClassCreate(&gwkjs_##cname##_class); \
    } \
    JSObjectRef global = gwkjs_get_import_global(context); \
    rval = gwkjs_object_get_property(context, global, gwkjs_##cname##_class.className, &exception); \
    if (JSVAL_IS_VOID(context, rval)) { \
        JSValueRef value; \
        JSObjectRef prototype = JSObjectMake (context, gwkjs_##cname##_class_ref, \
                                              NULL); \
        if (prototype == NULL) { \
            gwkjs_throw(context, "Couldn't create object for %s", proto_name); \
            return NULL; \
        } \
        if (!gwkjs_object_require_property( \
                context, global, NULL, \
                gwkjs_##cname##_class.className, &rval)) { \
            return NULL; \
        } \
        if (!gwkjs_object_set_property(context, module, proto_name, \
                               rval, GWKJS_PROTO_PROP_FLAGS, NULL)) \
            return NULL; \
        if (!JSValueIsObject(context, rval)) \
            return NULL;  \
        if (gtype != G_TYPE_NONE) { \
            value = gwkjs_gtype_create_gtype_wrapper(context, gtype); \
            gwkjs_object_set_property(context, JSValueToObject(context, rval, NULL), \
                                      "$gtype", value,                               \
                                      kJSPropertyAttributeDontDelete, NULL);         \
        } \
    } \
    if (!JSValueIsObject(context, rval)) {\
        gwkjs_throw(context, "gwkjs_##cname##_create_proto returning NULL for %s", type_name); \
        return NULL;  \
    } \
    return JSValueToObject(context, rval, NULL); \
}

JSObjectRef gwkjs_new_object(JSContextRef context,
                             JSClassRef clas, JSObjectRef proto,
                             JSObjectRef parent);

JSValueRef
gwkjs_cstring_to_jsvalue(JSContextRef context, const gchar *str);

guint
gwkjs_jsvalue_to_uint(JSContextRef ctx, JSValueRef val, JSValueRef* exception);

gint
gwkjs_jsvalue_to_int(JSContextRef ctx, JSValueRef val, JSValueRef* exception);

gchar*
gwkjs_jsvalue_to_cstring(JSContextRef ctx, JSValueRef val, JSValueRef* exception);

JSStringRef
gwkjs_cstring_to_jsstring(const gchar* str);

JSObjectRef
gwkjs_make_function(JSContextRef ctx, const gchar *name, JSObjectCallAsFunctionCallback cb);

JSValueRef
gwkjs_object_get_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName,
                          JSValueRef* exception);
const gboolean
gwkjs_object_has_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName);


gboolean
gwkjs_object_set_property(JSContextRef ctx,
                          JSObjectRef object,
                          const gchar* propertyName,
                          JSValueRef value,
                          JSPropertyAttributes attributes,
                          JSValueRef* exception);

gchar *     gwkjs_jsstring_to_cstring          (JSStringRef     property_name);

gboolean
gwkjs_array_get_length(JSContextRef context,
                       JSObjectRef array,
                       guint32 *result_out);

gboolean
gwkjs_array_get_element(JSContextRef context,
                        JSObjectRef obj,
                        guint pos,
                        JSValueRef *out);

//gboolean    gwkjs_init_context_standard        (JSContextRef    context,
//                                                JSObjectRef     *global_out);

JSObjectRef gwkjs_get_import_global              (JSContextRef    context);

//jsval       gwkjs_get_global_slot              (JSContextRef       context,
//                                                GwkjsGlobalSlot    slot);
//void        gwkjs_set_global_slot              (JSContextRef       context,
//                                                GwkjsGlobalSlot    slot,
//                                                jsval            value);

gboolean    gwkjs_object_require_property      (JSContextRef     context,
                                                JSObjectRef      obj,
                                                const char       *obj_description,
                                                const char       *property_name,
                                                JSValueRef       *value_p);

//JSObjectRef gwkjs_new_object_for_constructor   (JSContextRef    context,
//                                                JSClassRef      clasp);
//
///*
// * XXX: FIX
//JSBool      gwkjs_init_class_dynamic           (JSContextRef  context,
//                                              JSObjectRef        *in_object,
//                                              JSObjectRef        parent_proto,
//                                              const char      *ns_name,
//                                              const char      *class_name,
//                                              JSClassRef         clasp,
//                                              JSNative         constructor,
//                                              unsigned         nargs,
//                                              JSPropertySpec  *ps,
//                                              JSFunctionSpec  *fs,
//                                              JSPropertySpec  *static_ps,
//                                              JSFunctionSpec  *static_fs,
//                                              JSObjectRef       *constructor_p,
//                                              JSObjectRef       *prototype_p);
//*/
//void gwkjs_throw_constructor_error             (JSContextRef       context);

void gwkjs_throw_abstract_constructor_error    (JSContextRef       context,
                                                JSObjectRef     obj,
                                                JSValueRef      *exception);

//JSBool gwkjs_typecheck_instance                 (JSContextRef  context,
//                                               JSObjectRef   obj,
//                                               JSClassDefinition    *static_clasp,
//                                               JSBool      _throw);
//
//JSObjectRef   gwkjs_construct_object_dynamic     (JSContextRef       context,
//                                                  JSObjectRef        proto,
//                                                  unsigned         argc,
//                                                  jsval           *argv);

JSObjectRef   gwkjs_build_string_array           (JSContextRef     context,
                                                  gssize           array_length,
                                                  char           **array_values);

JSObjectRef   gwkjs_define_string_array          (JSContextRef       context,
                                                  JSObjectRef        obj,
                                                  const char      *array_name,
                                                  gssize           array_length,
                                                  const char     **array_values,
                                                  unsigned       attrs,
                                                  JSValueRef     *exception);

void        gwkjs_throw                        (JSContextRef       context,
                                                const char      *format,
                                                ...)  G_GNUC_PRINTF (2, 3);
//void        gwkjs_throw_custom                 (JSContextRef       context,
//                                              const char      *error_class,
//                                              const char      *format,
//                                              ...)  G_GNUC_PRINTF (3, 4);
//void        gwkjs_throw_literal                (JSContextRef       context,
//                                              const char      *string);
//void        gwkjs_throw_g_error                (JSContextRef       context,
//                                              GError          *error);
//
JSBool      gwkjs_log_exception                (JSContextRef       context,
                                                JSValueRef         exception);

//JSBool      gwkjs_log_and_keep_exception       (JSContextRef       context);
//JSBool      gwkjs_move_exception               (JSContextRef       src_context,
//                                                JSContextRef       dest_context);
//
JSBool      gwkjs_log_exception_full           (JSContextRef       context,
                                                JSValueRef         exc,
                                                const gchar        *message);
//
//#ifdef __GWKJS_UTIL_LOG_H__
//void        gwkjs_log_object_props             (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              GwkjsDebugTopic    topic,
//                                              const char      *prefix);
//#endif
//char*       gwkjs_value_debug_string           (JSContextRef       context,
//                                              jsval            value);
//void        gwkjs_explain_scope                (JSContextRef       context,
//                                              const char      *title);
//JSBool      gwkjs_call_function_value          (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              jsval            fval,
//                                              unsigned         argc,
//                                              jsval           *argv,
//                                              jsval           *rval);
///* TODO: removed error reporter
//void        gwkjs_error_reporter               (JSContextRef       context,
//                                              const char      *message,
//                                              JSErrorReport   *report);
//*/
//JSObjectRef*gwkjs_get_global_object            (JSContextRef cx);
//JSBool      gwkjs_get_prop_verbose_stub        (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              jsval            id,
//                                              jsval           *value_p);
//JSBool      gwkjs_set_prop_verbose_stub        (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              jsval            id,
//                                              jsval           *value_p);
//JSBool      gwkjs_add_prop_verbose_stub        (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              jsval            id,
//                                              jsval           *value_p);
//JSBool      gwkjs_delete_prop_verbose_stub     (JSContextRef       context,
//                                              JSObjectRef        obj,
//                                              jsval            id,
//                                              jsval           *value_p);
//
//JSBool      gwkjs_string_to_utf8               (JSContextRef       context,
//                                              const            jsval string_val,
//                                              char           **utf8_string_p);
//JSBool      gwkjs_string_from_utf8             (JSContextRef       context,
//                                              const char      *utf8_string,
//                                              gssize           n_bytes,
//                                              jsval           *value_p);
//JSBool      gwkjs_string_to_filename           (JSContextRef       context,
//                                              const jsval      string_val,
//                                              char           **filename_string_p);
//JSBool      gwkjs_string_from_filename         (JSContextRef       context,
//                                              const char      *filename_string,
//                                              gssize           n_bytes,
//                                              jsval           *value_p);
//JSBool      gwkjs_string_get_uint16_data       (JSContextRef       context,
//                                              jsval            value,
//                                              guint16        **data_p,
//                                              gsize           *len_p);
//
//JSBool      gwkjs_get_string_id                (JSContextRef       context,
//                                              jsid             id,
//                                              char           **name_p);
//jsid        gwkjs_intern_string_to_id          (JSContextRef       context,
//                                              const char      *string);
//
//gboolean    gwkjs_unichar_from_string          (JSContextRef       context,
//                                              jsval            string,
//                                              gunichar        *result);
//
//const char* gwkjs_get_type_name                (jsval            value);
//
//JSBool      gwkjs_value_to_int64               (JSContextRef       context,
//                                              const jsval      val,
//                                              gint64          *result);
//
//JSBool      gwkjs_parse_args                   (JSContextRef  context,
//                                              const char *function_name,
//                                              const char *format,
//                                              unsigned   argc,
//                                              jsval     *argv,
//                                              ...);
//
//JSBool      gwkjs_parse_call_args              (JSContextRef    context,
//                                              const char   *function_name,
//                                              const char   *format,
//                                              // TODO: fix
//                                              // JS::CallArgs &args,
//                                              ...);
//
//GwkjsRootedArray*   gwkjs_rooted_array_new        (void);
//
//void              gwkjs_rooted_array_append     (JSContextRef        context,
//                                               GwkjsRootedArray *array,
//                                               jsval             value);
//jsval             gwkjs_rooted_array_get        (JSContextRef        context,
//                                               GwkjsRootedArray *array,
//                                               int               i);
//jsval*            gwkjs_rooted_array_get_data   (JSContextRef        context,
//                                               GwkjsRootedArray *array);
//int               gwkjs_rooted_array_get_length (JSContextRef        context,
//                                               GwkjsRootedArray *array);
//jsval*            gwkjs_rooted_array_free       (JSContextRef        context,
//                                               GwkjsRootedArray *array,
//                                               gboolean          free_segment);
//void              gwkjs_set_values              (JSContextRef        context,
//                                               jsval            *locations,
//                                               int               n_locations,
//                                               jsval             initializer);
//void              gwkjs_root_value_locations    (JSContextRef        context,
//                                               jsval            *locations,
//                                               int               n_locations);
//void              gwkjs_unroot_value_locations  (JSContextRef        context,
//                                               jsval            *locations,
//                                               int               n_locations);
//
///* Functions intended for more "internal" use */
//
//void gwkjs_maybe_gc (JSContextRef context);
//
//JSBool            gwkjs_context_get_frame_info (JSContextRef  context,
//                                              jsval      *stack,
//                                              jsval      *fileName,
//                                              jsval      *lineNumber);
//
JSBool            gwkjs_eval_with_scope        (JSContextRef    context,
                                                JSObjectRef     object,
                                                const char   *script,
                                                gssize        script_len,
                                                const char   *filename,
                                                JSValueRef   *retval_p,
                                                JSValueRef   *exception);

typedef enum {
  GWKJS_STRING_CONSTRUCTOR,
  GWKJS_STRING_PROTOTYPE,
  GWKJS_STRING_LENGTH,
  GWKJS_STRING_IMPORTS,
  GWKJS_STRING_PARENT_MODULE,
  GWKJS_STRING_MODULE_INIT,
  GWKJS_STRING_SEARCH_PATH,
  GWKJS_STRING_KEEP_ALIVE_MARKER,
  GWKJS_STRING_PRIVATE_NS_MARKER,
  GWKJS_STRING_GI_MODULE,
  GWKJS_STRING_GI_VERSIONS,
  GWKJS_STRING_GI_OVERRIDES,
  GWKJS_STRING_GOBJECT_INIT,
  GWKJS_STRING_INSTANCE_INIT,
  GWKJS_STRING_NEW_INTERNAL,
  GWKJS_STRING_NEW,
  GWKJS_STRING_MESSAGE,
  GWKJS_STRING_CODE,
  GWKJS_STRING_STACK,
  GWKJS_STRING_FILENAME,
  GWKJS_STRING_LINE_NUMBER,
  GWKJS_STRING_NAME,
  GWKJS_STRING_X,
  GWKJS_STRING_Y,
  GWKJS_STRING_WIDTH,
  GWKJS_STRING_HEIGHT,
  GWKJS_STRING_LAST
} GwkjsConstString;

const gchar*              gwkjs_context_get_const_string  (JSContextRef       context,
                                                           GwkjsConstString   string);
//gboolean          gwkjs_object_get_property_const (JSContextRef       context,
//                                                 JSObjectRef        obj,
//                                                 GwkjsConstString   property_name,
//                                                 jsval           *value_p);
//
//const char * gwkjs_strip_unix_shebang(const char *script,
//                                    gssize     *script_len,
//                                    int        *new_start_line_number);
//
G_END_DECLS

#endif  /* __GWKJS_JSAPI_UTIL_H__ */
