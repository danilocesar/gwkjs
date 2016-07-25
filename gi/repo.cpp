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
    JSValueProtect(context, gi_namespace);

    /* Define the property early, to avoid reentrancy issues if
       the override module looks for namespaces that import this */
    gwkjs_object_set_property(context, repo_obj,
                              ns_name, gi_namespace,
                              kJSPropertyAttributeDontDelete, &exception);
    if (exception)
        g_error("no memory to define ns property %s", ns_name);

    override = lookup_override_function(context, ns_name);
    if (override && JSObjectIsFunction(context, override))
        result = JSObjectCallAsFunction(context, override, gi_namespace, 0, NULL, &exception);

    if (exception)
        goto out;

  gwkjs_debug(GWKJS_DEBUG_GNAMESPACE,
                "Defined namespace '%s' %p in GIRepository %p",
                ns_name, gi_namespace, repo_obj);

    gwkjs_schedule_gc_if_needed(context);

 out:
    if (gi_namespace)
        JSValueUnprotect(context, gi_namespace);
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

    gwkjs_debug_jsprop(GWKJS_DEBUG_GREPO, "Resolve prop '%s' hook obj %p priv %p",
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
        JSValueRef val = gwkjs_object_get_property(context, repo, "GLib", NULL);
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
static JSObjectRef
gwkjs_define_constant(JSContextRef   context,
                     JSObjectRef       in_object,
                     GIConstantInfo *info)
{
    JSValueRef value = NULL;
    JSValueRef exception = NULL;
    GArgument garg = { 0, };
    GITypeInfo *type_info;
    const char *name;

    type_info = g_constant_info_get_type(info);
    g_constant_info_get_value(info, &garg);

    if (!gwkjs_value_from_g_argument(context, &value, type_info, &garg, TRUE))
        goto out;

    if (!value)
        goto out;

    name = g_base_info_get_name((GIBaseInfo*) info);

    gwkjs_object_set_property(context, in_object, name, value, kJSPropertyAttributeDontDelete,
                              &exception);

    // FIXME: propert exception handling
    if (exception)
        gwkjs_throw(context, "gwkjs_define_constant FAILED for %s", name);


out:
    g_constant_info_free_value (info, &garg);
    g_base_info_unref((GIBaseInfo*) type_info);

    if (!JSValueIsObject(context, value)) {
        gwkjs_throw(context, "gwkjs_define_constant VALUE is NOT Object in %p:%s", value, name);
        return NULL;
    }
    return JSValueToObject(context, value, NULL);
}

#if GWKJS_VERBOSE_ENABLE_GI_USAGE
void
_gwkjs_log_info_usage(GIBaseInfo *info)
{
#define DIRECTION_STRING(d) ( ((d) == GI_DIRECTION_IN) ? "IN" : ((d) == GI_DIRECTION_OUT) ? "OUT" : "INOUT" )
#define TRANSFER_STRING(t) ( ((t) == GI_TRANSFER_NOTHING) ? "NOTHING" : ((t) == GI_TRANSFER_CONTAINER) ? "CONTAINER" : "EVERYTHING" )

    {
        char *details;
        GIInfoType info_type;
        GIBaseInfo *container;

        info_type = g_base_info_get_type(info);

        if (info_type == GI_INFO_TYPE_FUNCTION) {
            GString *args;
            int n_args;
            int i;
            GITransfer retval_transfer;

            args = g_string_new("{ ");

            n_args = g_callable_info_get_n_args((GICallableInfo*) info);
            for (i = 0; i < n_args; ++i) {
                GIArgInfo *arg;
                GIDirection direction;
                GITransfer transfer;

                arg = g_callable_info_get_arg((GICallableInfo*)info, i);
                direction = g_arg_info_get_direction(arg);
                transfer = g_arg_info_get_ownership_transfer(arg);

                g_string_append_printf(args,
                                       "{ GI_DIRECTION_%s, GI_TRANSFER_%s }, ",
                                       DIRECTION_STRING(direction),
                                       TRANSFER_STRING(transfer));

                g_base_info_unref((GIBaseInfo*) arg);
            }
            if (args->len > 2)
                g_string_truncate(args, args->len - 2); /* chop comma */

            g_string_append(args, " }");

            retval_transfer = g_callable_info_get_caller_owns((GICallableInfo*) info);

            details = g_strdup_printf(".details = { .func = { .retval_transfer = GI_TRANSFER_%s, .n_args = %d, .args = %s } }",
                                      TRANSFER_STRING(retval_transfer), n_args, args->str);
            g_string_free(args, TRUE);
        } else {
            details = g_strdup_printf(".details = { .nothing = {} }");
        }

        container = g_base_info_get_container(info);

        gwkjs_debug_gi_usage("{ GI_INFO_TYPE_%s, \"%s\", \"%s\", \"%s\", %s },",
                           gwkjs_info_type_name(info_type),
                           g_base_info_get_namespace(info),
                           container ? g_base_info_get_name(container) : "",
                           g_base_info_get_name(info),
                           details);
        g_free(details);
    }
}
#endif /* GWKJS_VERBOSE_ENABLE_GI_USAGE */


//TODO:  RENAME the following methods to DEFINE_AND_RETURN
JSObjectRef
gwkjs_define_info(JSContextRef context,
                  JSObjectRef  in_object,
                  GIBaseInfo   *info,
                  gboolean     *defined)
{
    JSObjectRef ret = NULL;
#if GWKJS_VERBOSE_ENABLE_GI_USAGE
    _gwkjs_log_info_usage(info);
#endif

    *defined = TRUE;

    switch (g_base_info_get_type(info)) {
    case GI_INFO_TYPE_FUNCTION:
        {
            ret = gwkjs_define_function(context, in_object, 0, (GICallableInfo*) info);
            if (ret == NULL)
                return NULL;
        }
        break;
    case GI_INFO_TYPE_OBJECT:
        {
            GType gtype;
            gtype = g_registered_type_info_get_g_type((GIRegisteredTypeInfo*)info);

            if (g_type_is_a (gtype, G_TYPE_PARAM)) {
                ret = gwkjs_define_param_class(context, in_object);
            } else if (g_type_is_a (gtype, G_TYPE_OBJECT)) {
                ret = gwkjs_define_object_class(context, in_object, (GIObjectInfo*) info, gtype, NULL);
            } else if (G_TYPE_IS_INSTANTIATABLE(gtype)) {
                gwkjs_throw(context, "Thing was a INSTANTIABLE!");
//TODO: IMPLEMENT
//                ret = gwkjs_define_fundamental_class(context, in_object, (GIObjectInfo*)info, NULL, NULL);
                if (!ret) {
                    gwkjs_throw (context,
                               "Unsupported fundamental class creation for type %s",
                               g_type_name(gtype));
                    return JS_FALSE;
                }
            } else {
                gwkjs_throw (context,
                           "Unsupported type %s, deriving from fundamental %s",
                           g_type_name(gtype), g_type_name(g_type_fundamental(gtype)));
                return JS_FALSE;
            }
        }
        break;
    case GI_INFO_TYPE_STRUCT:
        /* We don't want GType structures in the namespace,
           we expose their fields as vfuncs and their methods
           as static methods
        */
        if (g_struct_info_is_gtype_struct((GIStructInfo*) info)) {
            *defined = FALSE;
            break;
        }
        /* Fall through */

    case GI_INFO_TYPE_BOXED:
         gwkjs_throw(context, "Thing was a Boxed!");
// TODO: IMPLEMENT
//        ret = gwkjs_define_boxed_class(context, in_object, (GIBoxedInfo*) info);
        break;
    case GI_INFO_TYPE_UNION:
         gwkjs_throw(context, "Thing was a Union!");
// TODO: IMPLEMENT
//        ret = gwkjs_define_union_class(context, in_object, (GIUnionInfo*) info);
        if (ret == NULL)
            return ret;
        break;
    case GI_INFO_TYPE_ENUM:
// TODO: IMPLEMENT
        if (g_enum_info_get_error_domain((GIEnumInfo*) info)) {
            /* define as GError subclass */
//            ret = gwkjs_define_error_class(context, in_object, (GIEnumInfo*) info);
            break;
        }
        /* fall through */

    case GI_INFO_TYPE_FLAGS:
         gwkjs_throw(context, "Thing was a TYPE FLAGS!");
        ret = gwkjs_define_enumeration(context, in_object, (GIEnumInfo*) info);
        if (!ret)
            return ret;
        break;
    case GI_INFO_TYPE_CONSTANT:
         gwkjs_throw(context, "Thing was a TYPE constant!");
// TODO: IMPLEMENT
//        ret = gwkjs_define_constant(context, in_object, (GIConstantInfo*) info);
        if (ret)
            return ret;
        break;
    case GI_INFO_TYPE_INTERFACE:
         gwkjs_throw(context, "Thing was a interface!");
// TODO: IMPLEMENT
//        ret = gwkjs_define_interface_class(context, in_object, (GIInterfaceInfo *) info,
//                                   g_registered_type_info_get_g_type((GIRegisteredTypeInfo *) info),
//                                   NULL);
        break;
    default:
        gwkjs_throw(context, "API of type %s not implemented, cannot define %s.%s",
                  gwkjs_info_type_name(g_base_info_get_type(info)),
                  g_base_info_get_namespace(info),
                  g_base_info_get_name(info));
    }

    return ret;
}

///* Get the "unknown namespace", which should be used for unnamespaced types */
JSObjectRef
gwkjs_lookup_private_namespace(JSContextRef context)
{
    const gchar *ns_name = gwkjs_context_get_const_string(context, GWKJS_STRING_PRIVATE_NS_MARKER);
    return gwkjs_lookup_namespace_object_by_name(context, ns_name);
}

/* Get the namespace object that the GIBaseInfo should be inside */
JSObjectRef
gwkjs_lookup_namespace_object(JSContextRef  context,
                            GIBaseInfo *info)
{
    const char *ns;

    ns = g_base_info_get_namespace(info);
    if (ns == NULL) {
        gwkjs_throw(context, "%s '%s' does not have a namespace",
                     gwkjs_info_type_name(g_base_info_get_type(info)),
                     g_base_info_get_name(info));

        return NULL;
    }

    return gwkjs_lookup_namespace_object_by_name(context, ns);
}

static JSObjectRef
lookup_override_function(JSContextRef  context,
                         const gchar   *ns_name)
{
    jsval overridespkg = NULL;
    jsval module = NULL;
    jsval function = NULL;
    JSObjectRef importer = NULL;
    jsval importer_val = NULL;
    const gchar * overrides_name = NULL;
    const gchar * object_init_name = NULL;


    importer_val = gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_IMPORTS);
    g_assert(JSValueIsObject(context, importer_val));
    importer = JSValueToObject(context, importer_val, NULL);

    overrides_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_OVERRIDES);
    if (!gwkjs_object_require_property(context, importer, "importer",
                                       overrides_name, &overridespkg) ||
        !JSValueIsObject(context, overridespkg))
        goto fail;

    if (!gwkjs_object_require_property(context, JSValueToObject(context, overridespkg, NULL), "GI repository object", ns_name, &module)
        || !JSValueIsObject(context, module))
        goto fail;

    object_init_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GOBJECT_INIT);
    if (!gwkjs_object_require_property(context,
                                       JSValueToObject(context, module, NULL),
                                       "override module",
                                       object_init_name, &function) ||
        !JSValueIsObject(context, function))
        goto fail;

    return JSValueToObject(context, function, NULL);

 fail:
    g_warning("lookup_override_function for %s returned nothing", ns_name);
    return NULL;
}

JSObjectRef
gwkjs_lookup_namespace_object_by_name(JSContextRef      context,
                                      const gchar*      ns_name)
{
    JSObjectRef repo_obj;
    jsval importer = NULL;
    jsval girepository = NULL;
    jsval ns_obj = NULL;
    const gchar *gi_name;


    importer = gwkjs_get_global_slot(context, GWKJS_GLOBAL_SLOT_IMPORTS);
    g_assert(JSValueIsObject(context, importer));

    girepository = NULL;
    gi_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GI_MODULE);
    if (!gwkjs_object_require_property(context, JSValueToObject(context, importer, NULL),
                                       "importer", gi_name, &girepository) ||
        !JSValueIsObject(context, girepository)) {
        gwkjs_throw(context, "No gi property in importer");
        goto fail;
    }

    repo_obj = JSValueToObject(context, girepository, NULL);

    if (!gwkjs_object_require_property(context, repo_obj, "GI repository object", ns_name, &ns_obj)) {
        goto fail;
    }

    if (!JSValueIsObject(context, ns_obj)) {
        gwkjs_throw(context, "Namespace '%s' is not an object?", ns_name);
        goto fail;
    }

    return JSValueToObject(context, ns_obj, NULL);

 fail:
    return NULL;
}

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

char*
gwkjs_camel_from_hyphen(const char *hyphen_name)
{
    GString *s;
    const char *p;
    gboolean next_upper;

    s = g_string_sized_new(strlen(hyphen_name) + 1);

    next_upper = FALSE;
    for (p = hyphen_name; *p; ++p) {
        if (*p == '-' || *p == '_') {
            next_upper = TRUE;
        } else {
            if (next_upper) {
                g_string_append_c(s, g_ascii_toupper(*p));
                next_upper = FALSE;
            } else {
                g_string_append_c(s, *p);
            }
        }
    }

    return g_string_free(s, FALSE);
}

char*
gwkjs_hyphen_from_camel(const char *camel_name)
{
    GString *s;
    const char *p;

    /* four hyphens should be reasonable guess */
    s = g_string_sized_new(strlen(camel_name) + 4 + 1);

    for (p = camel_name; *p; ++p) {
        if (g_ascii_isupper(*p)) {
            g_string_append_c(s, '-');
            g_string_append_c(s, g_ascii_tolower(*p));
        } else {
            g_string_append_c(s, *p);
        }
    }

    return g_string_free(s, FALSE);
}

JSObjectRef
gwkjs_lookup_generic_constructor(JSContextRef  context,
                               GIBaseInfo *info)
{
    JSObjectRef in_object;
    JSObjectRef constructor;
    const char *constructor_name;
    jsval value;

    in_object = gwkjs_lookup_namespace_object(context, (GIBaseInfo*) info);
    constructor_name = g_base_info_get_name((GIBaseInfo*) info);

    if (G_UNLIKELY (!in_object))
        return NULL;

    if (!(value = gwkjs_object_get_property(context, in_object, constructor_name, NULL)))
        return NULL;

    if (G_UNLIKELY (!JSValueIsObject(context, value) || JSVAL_IS_NULL(context, value)))
        return NULL;

    constructor = JSValueToObject(context, value, NULL);
    g_assert(constructor != NULL);

    return constructor;
}

JSObjectRef
gwkjs_lookup_generic_prototype(JSContextRef  context,
                             GIBaseInfo *info)
{
    JSObjectRef constructor;
    jsval value;

    constructor = gwkjs_lookup_generic_constructor(context, info);
    if (G_UNLIKELY (constructor == NULL))
        return NULL;

    if (!gwkjs_object_get_property_const(context, constructor,
                                       GWKJS_STRING_PROTOTYPE, &value))
        return NULL;

    if (G_UNLIKELY (!JSValueIsObject(context, value)))
        return NULL;

    return JSValueToObject(context, value, NULL);
}
