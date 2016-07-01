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

#include "repo.h"
#include "ns.h"
#include "function.h"
#include "object.h"
#include "param.h"
#include "boxed.h"
#include "union.h"
#include "enumeration.h"
#include "arg.h"
#include "foreign.h"
#include "fundamental.h"
#include "interface.h"
#include "gerror.h"

#include <gwkjs/compat.h>
#include <gwkjs/jsapi-private.h>

#include <util/misc.h>

#include <girepository.h>
#include <string.h>

typedef struct {
    void *dummy;
    GHashTable *modules;
} Repo;

extern JSClassDefinition gwkjs_repo_class;
JSClassRef gwkjs_repo_class_ref = NULL;

GWKJS_DEFINE_PRIV_FROM_JS(Repo, gwkjs_repo_class)

static JSObjectRef lookup_override_function(JSContextRef, const gchar*);

static JSBool
get_version_for_ns (JSContextRef context,
                    JSObjectRef  repo_obj,
                    const gchar* nameid,
                    char     **version)
{
    const gchar* versions_name;
    JSValueRef versions_val = NULL;
    JSObjectRef versions;
    JSValueRef version_val = NULL;
    JSValueRef exception = NULL;

    versions_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_VERSIONS);
    if (!gwkjs_object_require_property(context, repo_obj, "GI repository object",
                                       versions_name, &versions_val)
        || !JSValueIsObject(context, versions_val)) {
        gwkjs_throw(context, "No 'versions' property in GI repository object");
        return FALSE;
    }

    versions = JSValueToObject(context, versions_val, NULL);

    *version = NULL;
    version_val = gwkjs_object_get_property(context, versions, nameid, &exception);

    if (exception == NULL &&
        JSVAL_IS_STRING(context, version_val)) {
        *version = gwkjs_jsvalue_to_cstring(context, version_val, NULL);
    }

    return JS_TRUE;
}

static JSValueRef
resolve_namespace_object(JSContextRef context,
                         JSObjectRef  repo_obj,
                         const char  *ns_name)
{
    GIRepository *repo;
    GError *error;
    char *version;
    JSObjectRef override = NULL;
    JSValueRef result = NULL;
    JSValueRef exception = NULL;
    JSObjectRef gi_namespace = NULL;
    JSValueRef ret = NULL;

    if (!get_version_for_ns(context, repo_obj, ns_name, &version))
        goto out;

    repo = g_irepository_get_default();

    error = NULL;
    g_irepository_require(repo, ns_name, version, (GIRepositoryLoadFlags) 0, &error);
    if (error != NULL) {
        gwkjs_throw(context,
                  "Requiring %s, version %s: %s",
                  ns_name, version?version:"none", error->message);

        g_error_free(error);
        g_free(version);
        goto out;
    }

    g_free(version);

    /* Defines a property on "obj" (the javascript repo object)
     * with the given namespace name, pointing to that namespace
     * in the repo.
     */
    gi_namespace = gwkjs_create_ns(context, ns_name);

    /* Define the property early, to avoid reentrancy issues if
       the override module looks for namespaces that import this */
    gwkjs_object_set_property(context, repo_obj,
                              ns_name, gi_namespace,
                              kJSPropertyAttributeDontDelete, &exception);
    if (exception)
        g_error("no memory to define ns property %s", ns_name);

    override = lookup_override_function(context, ns_name);
// TODO: IMPLEMENT
//    if (override && !JS_CallFunctionValue (context,
//                                           gi_namespace, // thisp
//                                           OBJECT_TO_JSVAL(override), // callee
//                                           0, // argc
//                                           NULL, // argv
//                                           &result))
//        goto out;
//
    gwkjs_debug(GWKJS_DEBUG_GNAMESPACE,
                "Defined namespace '%s' %p in GIRepository %p",
                ns_name, gi_namespace, repo_obj);

    gwkjs_schedule_gc_if_needed(context);

 out:
    return ret;
}


static JSValueRef
repo_get_property(JSContextRef ctx,
                  JSObjectRef obj,
                  JSStringRef property_name,
                  JSValueRef* exception)
{
    JSValueRef ret = NULL;
    gchar *name = NULL;
    Repo *priv = NULL;

    name = gwkjs_jsstring_to_cstring(property_name);

    if (strcmp(name, "valueOf") == 0 ||
        strcmp(name, "__filename__") == 0 ||
        strcmp(name, "__moduleName__") == 0 ||
        strcmp(name, "__parentModule__") == 0 ||
        strcmp(name, "toString") == 0)
        goto out;

    priv = priv_from_js(obj);
    if (priv == NULL) // We're the prototype
        goto out;

    // Let default handler deal with already loaded modules
    if (g_hash_table_contains(priv->modules, name))
        goto out;

    gwkjs_debug_jsprop(GWKJS_DEBUG_GREPO, "Resolve prop '%s' hook obj %p priv %p"
                       name, (void *)obj, priv);

    if (priv == NULL) /* we are the prototype, or have the wrong class */
        goto out;

    // We need to add the module to the list of having modules before trying
    // to import it.
    g_hash_table_replace(priv->modules, g_strdup(name), NULL);
    if ((ret = resolve_namespace_object(ctx, obj, name))) {
        g_warning("Module GI:%s imported", name);
    } else {
        g_hash_table_remove(priv->modules, name);
    }

out:
    g_free(name);
    return ret;
}

GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(repo)

static void
repo_finalize(JSObjectRef obj)
{
    Repo *priv;

    priv = (Repo*) JSObjectGetPrivate(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_GREPO,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* we are the prototype, not a real instance */

    GWKJS_DEC_COUNTER(repo);
    g_slice_free(Repo, priv);
}
//
///* The bizarre thing about this vtable is that it applies to both
// * instances of the object, and to the prototype that instances of the
// * class have.
// */
//struct JSClass gwkjs_repo_class = {
//    "GIRepository", /* means "new GIRepository()" works */
//    JSCLASS_HAS_PRIVATE |
//    JSCLASS_NEW_RESOLVE,
//    JS_PropertyStub,
//    JS_DeletePropertyStub,
//    JS_PropertyStub,
//    JS_StrictPropertyStub,
//    JS_EnumerateStub,
//    (JSResolveOp) repo_new_resolve, /* needs cast since it's the new resolve signature */
//    JS_ConvertStub,
//    repo_finalize,
//    JSCLASS_NO_OPTIONAL_MEMBERS
//};
//
//JSPropertySpec gwkjs_repo_proto_props[] = {
//    { NULL }
//};
//
//JSFunctionSpec gwkjs_repo_proto_funcs[] = {
//    { NULL }
//};
//
JSClassDefinition gwkjs_repo_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GIRepository",            //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    NULL,                      //     const JSStaticValue*                staticValues;
    NULL,                      //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    repo_finalize,             //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;
    repo_get_property,         //     JSObjectGetPropertyCallback         getProperty;
    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_repo_constructor,    //     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};



static JSObjectRef
repo_new(JSContextRef context)
{
    Repo *priv = NULL;
    JSObjectRef repo = NULL;
    JSObjectRef global = NULL;
    JSObjectRef versions = NULL;
    JSObjectRef private_ns = NULL;
    JSObjectRef proto = NULL;
    JSValueRef exception = NULL;
    gboolean found;

    const gchar* versions_name;
    const gchar* private_ns_name;

    global = gwkjs_get_import_global(context);
    if (!gwkjs_repo_class_ref) {
         gwkjs_repo_class_ref = JSClassCreate(&gwkjs_repo_class);
    }
    if (!(found = gwkjs_object_has_property(context,
                                            global, gwkjs_repo_class.className))) {
        proto = JSObjectMake(context,
                             gwkjs_repo_class_ref,
                             NULL);

        gwkjs_object_set_property(context, global,
                                  gwkjs_repo_class.className,
                                  proto,
                                  GWKJS_PROTO_PROP_FLAGS,
                                  &exception);

        if (exception)
           g_error("Can't init class %s", gwkjs_repo_class.className);

        gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Initialized class %s prototype %p",
            gwkjs_repo_class.className, proto);
    } else {
        JSValueRef proto_val = gwkjs_object_get_property(context,
                                                         global,
                                                         gwkjs_repo_class.className,
                                                         &exception);

        if (exception || proto_val == NULL || !JSValueIsObject(context, proto_val))
            g_error("Can't get protoType for class %s", gwkjs_repo_class.className);

        proto = JSValueToObject(context, proto_val, NULL);
    }
    g_assert(proto != NULL);

    repo = JSObjectMake(context, gwkjs_repo_class_ref, NULL);
    if (repo == NULL)
        return repo;
    JSObjectSetPrototype(context, repo, proto);


    priv = g_slice_new0(Repo);
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    GWKJS_INC_COUNTER(repo);

    g_assert(priv_from_js(repo) == NULL);
    JSObjectSetPrivate(repo, priv);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GREPO,
                         "repo constructor, obj %p priv %p", repo, priv);

    versions = JSObjectMake(context, NULL, NULL);
    versions_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_VERSIONS);

    g_hash_table_replace(priv->modules, g_strdup(versions_name), NULL);
    gwkjs_object_set_property(context, repo,
                              versions_name,
                              versions,
                              kJSPropertyAttributeDontDelete,
                              NULL);

    private_ns = JSObjectMake(context, NULL, NULL);
    private_ns_name = gwkjs_context_get_const_string(context, GWKJS_STRING_PRIVATE_NS_MARKER);
    g_hash_table_replace(priv->modules, g_strdup(private_ns_name), NULL);
    gwkjs_object_set_property(context, repo,
                              private_ns_name,
                              private_ns,
                              kJSPropertyAttributeDontDelete, NULL);

    /* FIXME - hack to make namespaces load, since
     * gobject-introspection does not yet search a path properly.
     */
    {
        //JSValueRef val = gwkjs_object_get_property(context, repo, "GLib", NULL);
    }

    return repo;
}


JSBool
gwkjs_define_repo(JSContextRef context,
                  JSObjectRef  *module_out,
                  const char *name)
{
    JSObjectRef repo;
    repo = repo_new(context);
    *module_out = repo;

    return JS_TRUE;
}
// TODO: IMPLEMENT
//static JSBool
//gwkjs_define_constant(JSContext      *context,
//                    JSObject       *in_object,
//                    GIConstantInfo *info)
//{
//    jsval value;
//    GArgument garg = { 0, };
//    GITypeInfo *type_info;
//    const char *name;
//    JSBool ret = JS_FALSE;
//
//    type_info = g_constant_info_get_type(info);
//    g_constant_info_get_value(info, &garg);
//
//    if (!gwkjs_value_from_g_argument(context, &value, type_info, &garg, TRUE))
//        goto out;
//
//    name = g_base_info_get_name((GIBaseInfo*) info);
//
//    if (JS_DefineProperty(context, in_object,
//                          name, value,
//                          NULL, NULL,
//                          GWKJS_MODULE_PROP_FLAGS))
//        ret = JS_TRUE;
//
// out:
//    g_constant_info_free_value (info, &garg);
//    g_base_info_unref((GIBaseInfo*) type_info);
//    return ret;
//}
//
//#if GWKJS_VERBOSE_ENABLE_GI_USAGE
//void
//_gwkjs_log_info_usage(GIBaseInfo *info)
//{
//#define DIRECTION_STRING(d) ( ((d) == GI_DIRECTION_IN) ? "IN" : ((d) == GI_DIRECTION_OUT) ? "OUT" : "INOUT" )
//#define TRANSFER_STRING(t) ( ((t) == GI_TRANSFER_NOTHING) ? "NOTHING" : ((t) == GI_TRANSFER_CONTAINER) ? "CONTAINER" : "EVERYTHING" )
//
//    {
//        char *details;
//        GIInfoType info_type;
//        GIBaseInfo *container;
//
//        info_type = g_base_info_get_type(info);
//
//        if (info_type == GI_INFO_TYPE_FUNCTION) {
//            GString *args;
//            int n_args;
//            int i;
//            GITransfer retval_transfer;
//
//            args = g_string_new("{ ");
//
//            n_args = g_callable_info_get_n_args((GICallableInfo*) info);
//            for (i = 0; i < n_args; ++i) {
//                GIArgInfo *arg;
//                GIDirection direction;
//                GITransfer transfer;
//
//                arg = g_callable_info_get_arg((GICallableInfo*)info, i);
//                direction = g_arg_info_get_direction(arg);
//                transfer = g_arg_info_get_ownership_transfer(arg);
//
//                g_string_append_printf(args,
//                                       "{ GI_DIRECTION_%s, GI_TRANSFER_%s }, ",
//                                       DIRECTION_STRING(direction),
//                                       TRANSFER_STRING(transfer));
//
//                g_base_info_unref((GIBaseInfo*) arg);
//            }
//            if (args->len > 2)
//                g_string_truncate(args, args->len - 2); /* chop comma */
//
//            g_string_append(args, " }");
//
//            retval_transfer = g_callable_info_get_caller_owns((GICallableInfo*) info);
//
//            details = g_strdup_printf(".details = { .func = { .retval_transfer = GI_TRANSFER_%s, .n_args = %d, .args = %s } }",
//                                      TRANSFER_STRING(retval_transfer), n_args, args->str);
//            g_string_free(args, TRUE);
//        } else {
//            details = g_strdup_printf(".details = { .nothing = {} }");
//        }
//
//        container = g_base_info_get_container(info);
//
//        gwkjs_debug_gi_usage("{ GI_INFO_TYPE_%s, \"%s\", \"%s\", \"%s\", %s },",
//                           gwkjs_info_type_name(info_type),
//                           g_base_info_get_namespace(info),
//                           container ? g_base_info_get_name(container) : "",
//                           g_base_info_get_name(info),
//                           details);
//        g_free(details);
//    }
//}
//#endif /* GWKJS_VERBOSE_ENABLE_GI_USAGE */
//

//TODO:  RENAME the following methods to DEFINE_AND_RETURN
JSObjectRef
gwkjs_define_info(JSContextRef context,
                  JSObjectRef  in_object,
                  GIBaseInfo   *info,
                  gboolean     *defined)
{
// TODO: IMPLEMENT
    g_warning("DEFINE INFO: %s, %u", g_base_info_get_name(info), g_base_info_get_type(info),
              g_base_info_get_type(info));

    return JSObjectMake(context, NULL, NULL);

//    JSObjectRef ret;
//#if GWKJS_VERBOSE_ENABLE_GI_USAGE
//    _gwkjs_log_info_usage(info);
//#endif
//
//    *defined = TRUE;
//
//    switch (g_base_info_get_type(info)) {
//    case GI_INFO_TYPE_FUNCTION:
//        {
//            ret = gwkjs_define_function(context, in_object, 0, (GICallableInfo*) info);
//            if (ret == NULL)
//                return NULL;
//        }
//        break;
//    case GI_INFO_TYPE_OBJECT:
//        {
//            GType gtype;
//            gtype = g_registered_type_info_get_g_type((GIRegisteredTypeInfo*)info);
//
//            if (g_type_is_a (gtype, G_TYPE_PARAM)) {
//                gwkjs_define_param_class(context, in_object);
//            } else if (g_type_is_a (gtype, G_TYPE_OBJECT)) {
//                gwkjs_define_object_class(context, in_object, (GIObjectInfo*) info, gtype, NULL);
//            } else if (G_TYPE_IS_INSTANTIATABLE(gtype)) {
//                if (!gwkjs_define_fundamental_class(context, in_object, (GIObjectInfo*)info, NULL, NULL)) {
//                    gwkjs_throw (context,
//                               "Unsupported fundamental class creation for type %s",
//                               g_type_name(gtype));
//                    return JS_FALSE;
//                }
//            } else {
//                gwkjs_throw (context,
//                           "Unsupported type %s, deriving from fundamental %s",
//                           g_type_name(gtype), g_type_name(g_type_fundamental(gtype)));
//                return JS_FALSE;
//            }
//        }
//        break;
//    case GI_INFO_TYPE_STRUCT:
//        /* We don't want GType structures in the namespace,
//           we expose their fields as vfuncs and their methods
//           as static methods
//        */
//        if (g_struct_info_is_gtype_struct((GIStructInfo*) info)) {
//            *defined = FALSE;
//            break;
//        }
//        /* Fall through */
//
//    case GI_INFO_TYPE_BOXED:
//        gwkjs_define_boxed_class(context, in_object, (GIBoxedInfo*) info);
//        break;
//    case GI_INFO_TYPE_UNION:
//        if (!gwkjs_define_union_class(context, in_object, (GIUnionInfo*) info))
//            return JS_FALSE;
//        break;
//    case GI_INFO_TYPE_ENUM:
//        if (g_enum_info_get_error_domain((GIEnumInfo*) info)) {
//            /* define as GError subclass */
//            gwkjs_define_error_class(context, in_object, (GIEnumInfo*) info);
//            break;
//        }
//        /* fall through */
//
//    case GI_INFO_TYPE_FLAGS:
//        if (!gwkjs_define_enumeration(context, in_object, (GIEnumInfo*) info))
//            return JS_FALSE;
//        break;
//    case GI_INFO_TYPE_CONSTANT:
//        if (!gwkjs_define_constant(context, in_object, (GIConstantInfo*) info))
//            return JS_FALSE;
//        break;
//    case GI_INFO_TYPE_INTERFACE:
//        gwkjs_define_interface_class(context, in_object, (GIInterfaceInfo *) info,
//                                   g_registered_type_info_get_g_type((GIRegisteredTypeInfo *) info),
//                                   NULL);
//        break;
//    default:
//        gwkjs_throw(context, "API of type %s not implemented, cannot define %s.%s",
//                  gwkjs_info_type_name(g_base_info_get_type(info)),
//                  g_base_info_get_namespace(info),
//                  g_base_info_get_name(info));
//        return JS_FALSE;
//    }
//
//    return JS_TRUE;
}
//
///* Get the "unknown namespace", which should be used for unnamespaced types */
//JSObject*
//gwkjs_lookup_private_namespace(JSContext *context)
//{
//    jsid ns_name;
//
//    ns_name = gwkjs_context_get_const_string(context, GWKJS_STRING_PRIVATE_NS_MARKER);
//    return gwkjs_lookup_namespace_object_by_name(context, ns_name);
//}
//
///* Get the namespace object that the GIBaseInfo should be inside */
//JSObject*
//gwkjs_lookup_namespace_object(JSContext  *context,
//                            GIBaseInfo *info)
//{
//    const char *ns;
//    jsid ns_name;
//
//    ns = g_base_info_get_namespace(info);
//    if (ns == NULL) {
//        gwkjs_throw(context, "%s '%s' does not have a namespace",
//                     gwkjs_info_type_name(g_base_info_get_type(info)),
//                     g_base_info_get_name(info));
//
//        return NULL;
//    }
//
//    ns_name = gwkjs_intern_string_to_id(context, ns);
//    return gwkjs_lookup_namespace_object_by_name(context, ns_name);
//}
//
static JSObjectRef
lookup_override_function(JSContextRef  context,
                         const gchar   *ns_name)
{
    return NULL;
//TODO: IMPLEMENT
//    jsval importer;
//    jsval overridespkg;
//    jsval module;
//    jsval function;
//    jsid overrides_name, object_init_name;
//
//    JS_BeginRequest(context);
//
//    importer = gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_IMPORTS);
//    g_assert(JSVAL_IS_OBJECT(importer));
//
//    overridespkg = JSVAL_VOID;
//    overrides_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_OVERRIDES);
//    if (!gwkjs_object_require_property(context, JSVAL_TO_OBJECT(importer), "importer",
//                                     overrides_name, &overridespkg) ||
//        !JSVAL_IS_OBJECT(overridespkg))
//        goto fail;
//
//    module = JSVAL_VOID;
//    if (!gwkjs_object_require_property(context, JSVAL_TO_OBJECT(overridespkg), "GI repository object", ns_name, &module)
//        || !JSVAL_IS_OBJECT(module))
//        goto fail;
//
//    object_init_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GOBJECT_INIT);
//    if (!gwkjs_object_require_property(context, JSVAL_TO_OBJECT(module), "override module",
//                                     object_init_name, &function) ||
//        !JSVAL_IS_OBJECT(function))
//        goto fail;
//
//    JS_EndRequest(context);
//    return JSVAL_TO_OBJECT(function);
//
// fail:
//    JS_ClearPendingException(context);
//    JS_EndRequest(context);
//    return NULL;
}
//
//JSObject*
//gwkjs_lookup_namespace_object_by_name(JSContext      *context,
//                                    jsid            ns_name)
//{
//    JSObject *repo_obj;
//    jsval importer;
//    jsval girepository;
//    jsval ns_obj;
//    jsid gi_name;
//
//    JS_BeginRequest(context);
//
//    importer = gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_IMPORTS);
//    g_assert(JSVAL_IS_OBJECT(importer));
//
//    girepository = JSVAL_VOID;
//    gi_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_MODULE);
//    if (!gwkjs_object_require_property(context, JSVAL_TO_OBJECT(importer), "importer",
//                                     gi_name, &girepository) ||
//        !JSVAL_IS_OBJECT(girepository)) {
//        gwkjs_log_exception(context);
//        gwkjs_throw(context, "No gi property in importer");
//        goto fail;
//    }
//
//    repo_obj = JSVAL_TO_OBJECT(girepository);
//
//    if (!gwkjs_object_require_property(context, repo_obj, "GI repository object", ns_name, &ns_obj)) {
//        goto fail;
//    }
//
//    if (!JSVAL_IS_OBJECT(ns_obj)) {
//        char *name;
//
//        gwkjs_get_string_id(context, ns_name, &name);
//        gwkjs_throw(context, "Namespace '%s' is not an object?", name);
//
//        g_free(name);
//        goto fail;
//    }
//
//    JS_EndRequest(context);
//    return JSVAL_TO_OBJECT(ns_obj);
//
// fail:
//    JS_EndRequest(context);
//    return NULL;
//}

const char*
gwkjs_info_type_name(GIInfoType type)
{
    switch (type) {
    case GI_INFO_TYPE_INVALID:
        return "INVALID";
    case GI_INFO_TYPE_FUNCTION:
        return "FUNCTION";
    case GI_INFO_TYPE_CALLBACK:
        return "CALLBACK";
    case GI_INFO_TYPE_STRUCT:
        return "STRUCT";
    case GI_INFO_TYPE_BOXED:
        return "BOXED";
    case GI_INFO_TYPE_ENUM:
        return "ENUM";
    case GI_INFO_TYPE_FLAGS:
        return "FLAGS";
    case GI_INFO_TYPE_OBJECT:
        return "OBJECT";
    case GI_INFO_TYPE_INTERFACE:
        return "INTERFACE";
    case GI_INFO_TYPE_CONSTANT:
        return "CONSTANT";
    case GI_INFO_TYPE_UNION:
        return "UNION";
    case GI_INFO_TYPE_VALUE:
        return "VALUE";
    case GI_INFO_TYPE_SIGNAL:
        return "SIGNAL";
    case GI_INFO_TYPE_VFUNC:
        return "VFUNC";
    case GI_INFO_TYPE_PROPERTY:
        return "PROPERTY";
    case GI_INFO_TYPE_FIELD:
        return "FIELD";
    case GI_INFO_TYPE_ARG:
        return "ARG";
    case GI_INFO_TYPE_TYPE:
        return "TYPE";
    case GI_INFO_TYPE_UNRESOLVED:
        return "UNRESOLVED";
    case GI_INFO_TYPE_INVALID_0:
        g_assert_not_reached();
        break;
    }

    return "???";
}

// TODO: implement
//char*
//gwkjs_camel_from_hyphen(const char *hyphen_name)
//{
//    GString *s;
//    const char *p;
//    gboolean next_upper;
//
//    s = g_string_sized_new(strlen(hyphen_name) + 1);
//
//    next_upper = FALSE;
//    for (p = hyphen_name; *p; ++p) {
//        if (*p == '-' || *p == '_') {
//            next_upper = TRUE;
//        } else {
//            if (next_upper) {
//                g_string_append_c(s, g_ascii_toupper(*p));
//                next_upper = FALSE;
//            } else {
//                g_string_append_c(s, *p);
//            }
//        }
//    }
//
//    return g_string_free(s, FALSE);
//}
//
//char*
//gwkjs_hyphen_from_camel(const char *camel_name)
//{
//    GString *s;
//    const char *p;
//
//    /* four hyphens should be reasonable guess */
//    s = g_string_sized_new(strlen(camel_name) + 4 + 1);
//
//    for (p = camel_name; *p; ++p) {
//        if (g_ascii_isupper(*p)) {
//            g_string_append_c(s, '-');
//            g_string_append_c(s, g_ascii_tolower(*p));
//        } else {
//            g_string_append_c(s, *p);
//        }
//    }
//
//    return g_string_free(s, FALSE);
//}
//
//JSObject *
//gwkjs_lookup_generic_constructor(JSContext  *context,
//                               GIBaseInfo *info)
//{
//    JSObject *in_object;
//    JSObject *constructor;
//    const char *constructor_name;
//    jsval value;
//
//    in_object = gwkjs_lookup_namespace_object(context, (GIBaseInfo*) info);
//    constructor_name = g_base_info_get_name((GIBaseInfo*) info);
//
//    if (G_UNLIKELY (!in_object))
//        return NULL;
//
//    if (!JS_GetProperty(context, in_object, constructor_name, &value))
//        return NULL;
//
//    if (G_UNLIKELY (!JSVAL_IS_OBJECT(value) || JSVAL_IS_NULL(value)))
//        return NULL;
//
//    constructor = JSVAL_TO_OBJECT(value);
//    g_assert(constructor != NULL);
//
//    return constructor;
//}
//
//JSObject *
//gwkjs_lookup_generic_prototype(JSContext  *context,
//                             GIBaseInfo *info)
//{
//    JSObject *constructor;
//    jsval value;
//
//    constructor = gwkjs_lookup_generic_constructor(context, info);
//    if (G_UNLIKELY (constructor == NULL))
//        return NULL;
//
//    if (!gwkjs_object_get_property_const(context, constructor,
//                                       GWKJS_STRING_PROTOTYPE, &value))
//        return NULL;
//
//    if (G_UNLIKELY (!JSVAL_IS_OBJECT(value)))
//        return NULL;
//
//    return JSVAL_TO_OBJECT(value);
//}
