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

#include <gwkjs/gi.h>
#include "object.h"
#include "gtype.h"
#include "interface.h"
#include "arg.h"
#include "repo.h"
#include "gtype.h"
#include "function.h"
#include "proxyutils.h"
#include "param.h"
#include "value.h"
#include "keep-alive.h"
#include "closure.h"
#include "gwkjs_gi_trace.h"

#include <gwkjs/gwkjs-module.h>
#include <gwkjs/compat.h>
#include <gwkjs/type-module.h>
#include <gwkjs/context-private.h>

#include <util/log.h>
#include <util/hash-x32.h>
#include <girepository.h>

typedef struct {
    guint64 signature;
    GIObjectInfo *info;
    GObject *gobj; /* NULL if we are the prototype and not an instance */
    JSObjectRef keep_alive; /* NULL if we are not added to it */
    GType gtype;

    /* a list of all signal connections, used when tracing */
    GList *signals;

    /* the GObjectClass wrapped by this JS Object (only used for
       prototypes) */
    GTypeClass *klass;

    GHashTable *modules;
} ObjectInstance;
// TODO: Remove this later
static guint64 ObjectInstanceSignature = 123456789;

typedef struct {
    ObjectInstance *obj;
    GList *link;
    GClosure *closure;
} ConnectData;

typedef enum
{
  TOGGLE_DOWN,
  TOGGLE_UP,
} ToggleDirection;

typedef struct
{
    GObject         *gobj;
    ToggleDirection  direction;
    guint            needs_unref : 1;
} ToggleRefNotifyOperation;

enum {
    PROP_0,
    PROP_JS_HANDLED,
};

static GSList *object_init_list;
static GHashTable *class_init_properties;

extern JSClassDefinition gwkjs_object_instance_class;
static JSClassRef gwkjs_object_instance_class_ref = NULL;
static GThread *gwkjs_eval_thread;
static volatile gint pending_idle_toggles;

GWKJS_DEFINE_PRIV_FROM_JS(ObjectInstance, gwkjs_object_instance_class)

static JSObjectRef       peek_js_obj  (GObject   *gobj);
static void              set_js_obj   (GObject   *gobj,
                                       JSObjectRef  obj);

static void            disassociate_js_gobject (GObject *gobj);
static void            invalidate_all_signals (ObjectInstance *priv);
typedef enum {
    SOME_ERROR_OCCURRED = JS_FALSE,
    NO_SUCH_G_PROPERTY,
    VALUE_WAS_SET
} ValueFromPropertyResult;

static JSBool
object_instance_new_resolve(JSContextRef context,
                            JSObjectRef obj,
                            const gchar propertyName[],
                            JSValueRef *objp);

static GQuark
gwkjs_is_custom_type_quark (void)
{
    static GQuark val = 0;
    if (!val)
        val = g_quark_from_static_string ("gwkjs::custom-type");

    return val;
}

static GQuark
gwkjs_is_custom_property_quark (void)
{
    static GQuark val = 0;
    if (!val)
        val = g_quark_from_static_string ("gwkjs::custom-property");

    return val;
}

static GQuark
gwkjs_object_priv_quark (void)
{
    static GQuark val = 0;
    if (G_UNLIKELY (!val))
        val = g_quark_from_static_string ("gwkjs::private");

    return val;
}

static GQuark
gwkjs_toggle_down_quark (void)
{
    static GQuark val = 0;
    if (G_UNLIKELY (!val))
        val = g_quark_from_static_string ("gwkjs::toggle-down-quark");

    return val;
}

static GQuark
gwkjs_toggle_up_quark (void)
{
    static GQuark val = 0;
    if (G_UNLIKELY (!val))
        val = g_quark_from_static_string ("gwkjs::toggle-up-quark");

    return val;
}

/* Plain g_type_query fails and leaves @query uninitialized for
   dynamic types.
   See https://bugzilla.gnome.org/show_bug.cgi?id=687184 and
   https://bugzilla.gnome.org/show_bug.cgi?id=687211
*/
static void
g_type_query_dynamic_safe (GType       type,
                           GTypeQuery *query)
{
    while (g_type_get_qdata(type, gwkjs_is_custom_type_quark()))
        type = g_type_parent(type);

    g_type_query(type, query);
}

static void
throw_priv_is_null_error(JSContextRef context)
{
    gwkjs_throw(context,
              "This JS object wrapper isn't wrapping a GObject."
              " If this is a custom subclass, are you sure you chained"
              " up to the parent _init properly?");
}

static ValueFromPropertyResult
init_g_param_from_property(JSContextRef  context,
                           const char *js_prop_name,
                           jsval       js_value,
                           GType       gtype,
                           GParameter *parameter,
                           gboolean    constructing)
{
    char *gname;
    GParamSpec *param_spec;
    void *klass;

    gname = gwkjs_hyphen_from_camel(js_prop_name);
    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Hyphen name %s on %s", gname, g_type_name(gtype));

    klass = g_type_class_ref(gtype);
    param_spec = g_object_class_find_property(G_OBJECT_CLASS(klass),
                                              gname);
    g_type_class_unref(klass);
    g_free(gname);

    if (param_spec == NULL) {
        /* not a GObject prop, so nothing else to do */
        return NO_SUCH_G_PROPERTY;
    }

    /* Do not set JS overridden properties through GObject, to avoid
     * infinite recursion (but set them when constructing) */
    if (!constructing &&
        g_param_spec_get_qdata(param_spec, gwkjs_is_custom_property_quark()))
        return NO_SUCH_G_PROPERTY;


    if ((param_spec->flags & G_PARAM_WRITABLE) == 0) {
        /* prevent setting the prop even in JS */
        gwkjs_throw(context, "Property %s (GObject %s) is not writable",
                     js_prop_name, param_spec->name);
        return SOME_ERROR_OCCURRED;
    }

    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Syncing %s to GObject prop %s",
                     js_prop_name, param_spec->name);

    g_value_init(&parameter->value, G_PARAM_SPEC_VALUE_TYPE(param_spec));
    if (!gwkjs_value_to_g_value(context, js_value, &parameter->value)) {
        g_value_unset(&parameter->value);
        return SOME_ERROR_OCCURRED;
    }

    parameter->name = param_spec->name;

    return VALUE_WAS_SET;
}

static inline ObjectInstance *
proto_priv_from_js(JSContextRef context,
                   JSObjectRef  obj)
{
    JSValueRef proto_val = JSObjectGetPrototype(context, obj);
    JSObjectRef proto = NULL;

    if (JSValueIsObject(context, proto_val))
        proto = JSValueToObject(context, proto_val, NULL);
    else {
        gwkjs_throw(context, "Proto_priv_from_js shouldn't return nothing");
        return NULL;
    }

    return priv_from_js(proto);
}

static JSValueRef
object_instance_get_prop(JSContextRef context,
                      JSObjectRef obj,
                      JSStringRef property_name,
                      JSValueRef* exception)
{
    ObjectInstance *priv;
    char *name;
    char *gname;
    GParamSpec *param;
    GValue gvalue = { 0, };
    JSValueRef ret = NULL;
    JSValueRef tryOut = NULL;

    name = gwkjs_jsstring_to_cstring(property_name);

    priv = priv_from_js(obj);
    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Get prop '%s' hook obj %p priv %p",
                     name, (void *)obj, priv);

    if (priv == NULL) {
        /* If we reach this point, either object_instance_new_resolve
         * did not throw (so name == "_init"), or the property actually
         * exists and it's not something we should be concerned with */
        goto out;
    }
    g_assert(priv->signature == ObjectInstanceSignature);

    if (g_hash_table_contains(priv->modules, name)) {
        ret = (JSValueRef) g_hash_table_lookup(priv->modules, name);
        goto out;
    }

    g_hash_table_replace(priv->modules, g_strdup(name), NULL);
    object_instance_new_resolve(context, obj, name, &tryOut);
    if (tryOut) {
        g_hash_table_replace(priv->modules, g_strdup(name), (void*)tryOut);
        ret = tryOut;
        goto out;
    }


    if (priv->gobj == NULL) /* prototype, not an instance. */
        goto out;

    gname = gwkjs_hyphen_from_camel(name);
    param = g_object_class_find_property(G_OBJECT_GET_CLASS(priv->gobj),
                                         gname);
    g_free(gname);

    if (param == NULL) {
        /* leave value_p as it was */
        goto out;
    }

    /* Do not fetch JS overridden properties from GObject, to avoid
     * infinite recursion. */
    if (g_param_spec_get_qdata(param, gwkjs_is_custom_property_quark()))
        goto out;

    if ((param->flags & G_PARAM_READABLE) == 0)
        goto out;

    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Overriding %s with GObject prop %s",
                     name, param->name);

    g_value_init(&gvalue, G_PARAM_SPEC_VALUE_TYPE(param));
    g_object_get_property(priv->gobj, param->name,
                          &gvalue);
    if (!gwkjs_value_from_g_value(context, &ret, &gvalue)) {
        g_value_unset(&gvalue);
        goto out;
    }
    g_value_unset(&gvalue);

 out:
//    if (!ret)
//        g_warning("object_instance_get_prop is NULL for      %p: %s", obj, name);
//    else
//        g_warning("object_instance_get_prop is NOT! NULL for %p %s", obj, name);

    g_free(name);
    return ret;
}

///* a hook on setting a property; set value_p to override property value to
// * be set. Return value is JS_FALSE on OOM/exception.
// */
static bool 
object_instance_set_prop(JSContextRef ctx,
                         JSObjectRef obj,
                         JSStringRef propertyName,
                         JSValueRef value,
                         JSValueRef* exception)
{
    ObjectInstance *priv = NULL;
    char *name = NULL;
    GParameter param = { NULL, { 0, }};
    JSBool ret = FALSE;

    name = gwkjs_jsstring_to_cstring(propertyName);

    priv = priv_from_js(obj);
    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Set prop '%s' hook obj %p priv %p",
                     name, (void *)obj, priv);

    if (priv == NULL) {
        /* see the comment in object_instance_get_prop() on this */
        goto out;
    }
    if (priv->gobj == NULL) /* prototype, not an instance. */
        goto out;

    switch (init_g_param_from_property(ctx, name,
                                       value,
                                       G_TYPE_FROM_INSTANCE(priv->gobj),
                                       &param,
                                       FALSE /* constructing */)) {
    case SOME_ERROR_OCCURRED:
        ret = JS_FALSE;
    case NO_SUCH_G_PROPERTY:
        goto out;
    case VALUE_WAS_SET:
        break;
    }

    g_object_set_property(priv->gobj, param.name,
                          &param.value);

    g_value_unset(&param.value);

    /* note that the prop will also have been set in JS, which I think
     * is OK, since we hook get and set so will always override that
     * value. We could also use JS_DefineProperty though and specify a
     * getter/setter maybe, don't know if that is better.
     */

 out:
//    g_warning("object_instance_SET_prop for %s %p == %p   |  %d", name, obj, value, ret);
    g_free(name);
    return ret;
}

static gboolean
is_vfunc_unchanged(GIVFuncInfo *info,
                   GType        gtype)
{
    GType ptype = g_type_parent(gtype);
    GError *error = NULL;
    gpointer addr1, addr2;

    addr1 = g_vfunc_info_get_address(info, gtype, &error);
    if (error) {
        g_clear_error(&error);
        return FALSE;
    }

    addr2 = g_vfunc_info_get_address(info, ptype, &error);
    if (error) {
        g_clear_error(&error);
        return FALSE;
    }

    return addr1 == addr2;
}

static GIVFuncInfo *
find_vfunc_on_parents(GIObjectInfo *info,
                      gchar        *name,
                      gboolean     *out_defined_by_parent)
{
    GIVFuncInfo *vfunc = NULL;
    GIObjectInfo *parent;
    gboolean defined_by_parent = FALSE;

    /* ref the first info so that we don't destroy
     * it when unrefing parents later */
    g_base_info_ref(info);
    parent = info;

    /* Since it isn't possible to override a vfunc on
     * an interface without reimplementing it, we don't need
     * to search the parent types when looking for a vfunc. */
    vfunc = g_object_info_find_vfunc_using_interfaces(parent, name, NULL);
    while (!vfunc && parent) {
        GIObjectInfo *tmp = parent;
        parent = g_object_info_get_parent(tmp);
        g_base_info_unref(tmp);
        if (parent)
            vfunc = g_object_info_find_vfunc(parent, name);

        defined_by_parent = TRUE;
    }

    if (parent)
        g_base_info_unref(parent);

    if (out_defined_by_parent)
        *out_defined_by_parent = defined_by_parent;

    return vfunc;
}

static JSBool
object_instance_new_resolve_no_info(JSContextRef       context,
                                     JSObjectRef       obj,
                                     JSValueRef       *objp,
                                    ObjectInstance  *priv,
                                    char            *name)
{
    GIFunctionInfo *method_info;
    JSBool ret;
    GType *interfaces;
    guint n_interfaces;
    guint i;

    ret = JS_TRUE;
    interfaces = g_type_interfaces(priv->gtype, &n_interfaces);
    for (i = 0; i < n_interfaces; i++) {
        GIBaseInfo *base_info;
        GIInterfaceInfo *iface_info;

        base_info = g_irepository_find_by_gtype(g_irepository_get_default(),
                                                interfaces[i]);

        if (base_info == NULL)
            continue;

        /* An interface GType ought to have interface introspection info */
        g_assert (g_base_info_get_type(base_info) == GI_INFO_TYPE_INTERFACE);

        iface_info = (GIInterfaceInfo*) base_info;

        method_info = g_interface_info_find_method(iface_info, name);

        g_base_info_unref(base_info);


        if (method_info != NULL) {
            if (g_function_info_get_flags (method_info) & GI_FUNCTION_IS_METHOD) {
                if ((*objp = gwkjs_define_function(context, obj, priv->gtype,
                                        (GICallableInfo *)method_info))) {
                } else {
                    ret = JS_FALSE;
                }
            }

            g_base_info_unref( (GIBaseInfo*) method_info);
        }
    }

    g_free(interfaces);
    return ret;
}

/*
 * Like JSResolveOp, but flags provide contextual information as follows:
 *
 *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
 *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
 *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
 *  JSRESOLVE_DECLARING   var, const, or object prolog declaration opcode
 *  JSRESOLVE_CLASSNAME   class name used when constructing
 *
 * The *objp out parameter, on success, should be null to indicate that id
 * was not resolved; and non-null, referring to obj or one of its prototypes,
 * if id was resolved.
 */
static JSBool
object_instance_new_resolve(JSContextRef context,
                            JSObjectRef obj,
                            const gchar propertyName[],
                            JSValueRef *objp)
{
    GIFunctionInfo *method_info;
    ObjectInstance *priv;
    gchar *name;
    JSBool ret = JS_FALSE;

    name = g_strdup(propertyName);

    priv = priv_from_js(obj);

    gwkjs_debug_jsprop(GWKJS_DEBUG_GOBJECT,
                     "Resolve prop '%s' hook obj %p priv %p (%s.%s) gobj %p %s",
                     name,
                     (void *)obj,
                     priv,
                     priv && priv->info ? g_base_info_get_namespace (priv->info) : "",
                     priv && priv->info ? g_base_info_get_name (priv->info) : "",
                     priv ? priv->gobj : NULL,
                     (priv && priv->gobj) ? g_type_name_from_instance((GTypeInstance*) priv->gobj) : "(type unknown)");

    if (priv == NULL) {
        /* We won't have a private until the initializer is called, so
         * just defer to prototype chains in this case.
         *
         * This isn't too bad: either you get undefined if the field
         * doesn't exist on any of the prototype chains, or whatever code
         * will run afterwards will fail because of the "priv == NULL"
         * check there.
         */
        ret = JS_TRUE;
        goto out;
    }

    if (priv->gobj != NULL) {
        ret = JS_TRUE;
        goto out;
    }

    /* If we have no GIRepository information (we're a JS GObject subclass),
     * we need to look at exposing interfaces. Look up our interfaces through
     * GType data, and then hope that *those* are introspectable. */
    if (priv->info == NULL) {
        ret = object_instance_new_resolve_no_info(context, obj, objp, priv, name);
        goto out;
    }

    if (g_str_has_prefix (name, "vfunc_")) {
        /* The only time we find a vfunc info is when we're the base
         * class that defined the vfunc. If we let regular prototype
         * chaining resolve this, we'd have the implementation for the base's
         * vfunc on the base class, without any other "real" implementations
         * in the way. If we want to expose a "real" vfunc implementation,
         * we need to go down to the parent infos and look at their VFuncInfos.
         *
         * This is good, but it's memory-hungry -- we would define every
         * possible vfunc on every possible object, even if it's the same
         * "real" vfunc underneath. Instead, only expose vfuncs that are
         * different from their parent, and let prototype chaining do the
         * rest.
         */

        gchar *name_without_vfunc_ = &name[6];
        GIVFuncInfo *vfunc;
        gboolean defined_by_parent;

        vfunc = find_vfunc_on_parents(priv->info, name_without_vfunc_, &defined_by_parent);
        if (vfunc != NULL) {
            /* In the event that the vfunc is unchanged, let regular
             * prototypal inheritance take over. */
            if (defined_by_parent && is_vfunc_unchanged(vfunc, priv->gtype)) {
                g_base_info_unref((GIBaseInfo *)vfunc);
                ret = JS_TRUE;
                goto out;
            }

            *objp = gwkjs_define_function(context, obj, priv->gtype, vfunc);
            g_base_info_unref((GIBaseInfo *)vfunc);
            ret = JS_TRUE;
            goto out;
        }

        /* If the vfunc wasn't found, fall through, back to normal
         * method resolution. */
    }

    /* find_method does not look at methods on parent classes,
     * we rely on javascript to walk up the __proto__ chain
     * and find those and define them in the right prototype.
     *
     * Note that if it isn't a method on the object, since JS
     * lacks multiple inheritance, we're sticking the iface
     * methods in the object prototype, which means there are many
     * copies of the iface methods (one per object class node that
     * introduces the iface)
     */

    method_info = g_object_info_find_method_using_interfaces(priv->info,
                                                             name,
                                                             NULL);

    /**
     * Search through any interfaces implemented by the GType;
     * this could be done better.  See
     * https://bugzilla.gnome.org/show_bug.cgi?id=632922
     */
    if (method_info == NULL) {
        ret = object_instance_new_resolve_no_info(context, obj, objp,
                                                  priv, name);
        goto out;
    } else {
#if GWKJS_VERBOSE_ENABLE_GI_USAGE
        _gwkjs_log_info_usage((GIBaseInfo*) method_info);
#endif

        if (g_function_info_get_flags (method_info) & GI_FUNCTION_IS_METHOD) {
            gwkjs_debug(GWKJS_DEBUG_GOBJECT,
                      "Defining method %s in prototype for %s (%s.%s)",
                      g_base_info_get_name( (GIBaseInfo*) method_info),
                      g_type_name(priv->gtype),
                      g_base_info_get_namespace( (GIBaseInfo*) priv->info),
                      g_base_info_get_name( (GIBaseInfo*) priv->info));

            *objp = gwkjs_define_function(context, obj, priv->gtype, method_info);
            if (*objp == NULL) {
                g_base_info_unref( (GIBaseInfo*) method_info);
                goto out;
            }
        }

        g_base_info_unref( (GIBaseInfo*) method_info);
    }

    ret = JS_TRUE;
 out:
    g_free(name);
    return ret;
}

static void
free_g_params(GParameter *params,
              int         n_params)
{
    int i;

    for (i = 0; i < n_params; ++i) {
        g_value_unset(&params[i].value);
    }
    g_free(params);
}

/* Set properties from args to constructor (argv[0] is supposed to be
 * a hash)
 */
static JSBool
object_instance_props_to_g_parameters(JSContextRef   context,
                                      JSObjectRef    obj,
                                      unsigned     argc,
                                      const JSValueRef argv[],
                                      GType        gtype,
                                      GParameter **gparams_p,
                                      int         *n_gparams_p)
{
    JSPropertyNameArrayRef property_names = NULL;
    JSObjectRef prop_obj = NULL;
    GArray *gparams;
    int i = 0;

    if (gparams_p)
        *gparams_p = NULL;
    if (n_gparams_p)
        *n_gparams_p = 0;

    gparams = g_array_new(/* nul term */ FALSE, /* clear */ TRUE,
                          sizeof(GParameter));

    if (argc == 0 || JSVAL_IS_VOID(context, argv[0]))
        goto out;

    if (!JSValueIsObject(context, argv[0])) {
        gwkjs_throw(context, "argument should be a hash with props to set");
        goto free_array_and_fail;
    }
    prop_obj = JSValueToObject(context, argv[0], NULL);

    property_names = JSObjectCopyPropertyNames(context, prop_obj);
    for (i = 0; i < JSPropertyNameArrayGetCount(property_names); i++) {
        gchar *name;
        jsval value = NULL;
        GParameter gparam = { NULL, { 0, }};
        JSStringRef prop_name_js = JSPropertyNameArrayGetNameAtIndex(property_names, i);
        name = gwkjs_jsstring_to_cstring(prop_name_js);

        if (!gwkjs_object_require_property(context, prop_obj, "property list", name, &value)) {
            g_free(name);
            goto free_array_and_fail;
        }

        switch (init_g_param_from_property(context, name,
                                           value,
                                           gtype,
                                           &gparam,
                                           TRUE /* constructing */)) {
        case NO_SUCH_G_PROPERTY:
            gwkjs_throw(context, "No property %s on this GObject %s",
                         name, g_type_name(gtype));
        case SOME_ERROR_OCCURRED:
            g_free(name);
            goto free_array_and_fail;
        case VALUE_WAS_SET:
            break;
        }
        g_free(name);

        g_array_append_val(gparams, gparam);
    }

 out:
    if (n_gparams_p)
        *n_gparams_p = gparams->len;
    if (gparams_p)
        *gparams_p = (GParameter*) g_array_free(gparams, FALSE);

    if (property_names) JSPropertyNameArrayRelease(property_names);

    return JS_TRUE;

 free_array_and_fail:
    {
        if (property_names) JSPropertyNameArrayRelease(property_names);

        GParameter *to_free;
        int count;
        count = gparams->len;
        to_free = (GParameter*) g_array_free(gparams, FALSE);
        free_g_params(to_free, count);
    }
    return JS_FALSE;
}

#define DEBUG_DISPOSE 0
#if DEBUG_DISPOSE
static void
wrapped_gobj_dispose_notify(gpointer      data,
                            GObject      *where_the_object_was)
{
    gwkjs_debug(GWKJS_DEBUG_GOBJECT, "JSObject %p GObject %p disposed", data, where_the_object_was);
}
#endif

static void
gobj_no_longer_kept_alive_func(JSObjectRef obj,
                               void     *data)
{
    ObjectInstance *priv;

    priv = (ObjectInstance *) data;

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
                        "GObject wrapper %p will no longer be kept alive, eligible for collection",
                        obj);

    priv->keep_alive = NULL;
}

static GQuark
get_qdata_key_for_toggle_direction(ToggleDirection direction)
{
    GQuark quark;

    switch (direction) {
        case TOGGLE_UP:
            quark = gwkjs_toggle_up_quark();
            break;
        case TOGGLE_DOWN:
            quark = gwkjs_toggle_down_quark();
            break;
        default:
            g_assert_not_reached();
    }

    return quark;
}

static gboolean
clear_toggle_idle_source(GObject          *gobj,
                         ToggleDirection   direction)
{
    GQuark qdata_key;

    qdata_key = get_qdata_key_for_toggle_direction(direction);

    return g_object_steal_qdata(gobj, qdata_key) != NULL;
}

static gboolean
toggle_idle_source_is_queued(GObject          *gobj,
                             ToggleDirection   direction)
{
    GQuark qdata_key;

    qdata_key = get_qdata_key_for_toggle_direction(direction);

    return g_object_get_qdata(gobj, qdata_key) != NULL;
}

static gboolean
cancel_toggle_idle(GObject         *gobj,
                   ToggleDirection  direction)
{
    GQuark qdata_key;
    GSource *source;

    qdata_key = get_qdata_key_for_toggle_direction(direction);

    source = (GSource*) g_object_steal_qdata(gobj, qdata_key);

    if (source)
        g_source_destroy(source);

    return source != 0;
}

static void
handle_toggle_down(GObject *gobj)
{
    ObjectInstance *priv;
    JSObjectRef obj;

    obj = peek_js_obj(gobj);

    priv = (ObjectInstance *) JSObjectGetPrivate(obj);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
                        "Toggle notify gobj %p obj %p is_last_ref TRUE keep-alive %p",
                        gobj, obj, priv->keep_alive);

    /* Change to weak ref so the wrapper-wrappee pair can be
     * collected by the GC
     */
    if (priv->keep_alive != NULL) {
        gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT, "Removing object from keep alive");
        gwkjs_keep_alive_remove_child(priv->keep_alive,
                                    gobj_no_longer_kept_alive_func,
                                    obj,
                                    priv);
        priv->keep_alive = NULL;
    }
}

static void
handle_toggle_up(GObject   *gobj)
{
    ObjectInstance *priv;
    JSObjectRef obj;

    /* We need to root the JSObject associated with the passed in GObject so it
     * doesn't get garbage collected (and lose any associated javascript state
     * such as custom properties).
     */
    obj = peek_js_obj(gobj);

    if (!obj) /* Object already GC'd */
        return;

    priv = (ObjectInstance *) JSObjectGetPrivate(obj);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
                        "Toggle notify gobj %p obj %p is_last_ref FALSEd keep-alive %p",
                        gobj, obj, priv->keep_alive);

    g_assert(priv != NULL);

    /* Change to strong ref so the wrappee keeps the wrapper alive
     * in case the wrapper has data in it that the app cares about
     */
    if (priv->keep_alive == NULL) {
        /* FIXME: thread the context through somehow. Maybe by looking up
         * the compartment that obj belongs to. */
        GwkjsContext *context = gwkjs_context_get_current();
        gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT, "Adding object to keep alive");
        priv->keep_alive = gwkjs_keep_alive_get_global((JSContextRef) gwkjs_context_get_native_context(context));
        gwkjs_keep_alive_add_child(priv->keep_alive,
                                 gobj_no_longer_kept_alive_func,
                                 obj,
                                 priv);
    }
}

static gboolean
idle_handle_toggle(gpointer data)
{
    ToggleRefNotifyOperation *operation = (ToggleRefNotifyOperation *) data;

    if (!clear_toggle_idle_source(operation->gobj, operation->direction)) {
        /* Already cleared, the JSObject is going away, abort mission */
        goto out;
    }

    switch (operation->direction) {
        case TOGGLE_UP:
            handle_toggle_up(operation->gobj);
            break;
        case TOGGLE_DOWN:
            handle_toggle_down(operation->gobj);
            break;
        default:
            g_assert_not_reached();
    }

out:
    return FALSE;
}

static void
toggle_ref_notify_operation_free(ToggleRefNotifyOperation *operation)
{
    if (operation->needs_unref)
        g_object_unref (operation->gobj);
    g_slice_free(ToggleRefNotifyOperation, operation);
    g_atomic_int_add(&pending_idle_toggles, -1);
}

static void
queue_toggle_idle(GObject         *gobj,
                  ToggleDirection  direction)
{
    ToggleRefNotifyOperation *operation;
    GQuark qdata_key;
    GSource *source;

    operation = g_slice_new0(ToggleRefNotifyOperation);
    operation->direction = direction;

    switch (direction) {
        case TOGGLE_UP:
            /* If we're toggling up we take a reference to the object now,
             * so it won't toggle down before we process it. This ensures we
             * only ever have at most two toggle notifications queued.
             * (either only up, or down-up)
             */
            operation->gobj = (GObject*) g_object_ref(gobj);
            operation->needs_unref = TRUE;
            break;
        case TOGGLE_DOWN:
            /* If we're toggling down, we don't need to take a reference since
             * the associated JSObject already has one, and that JSObject won't
             * get finalized until we've completed toggling (since it's rooted,
             * until we unroot it when we dispatch the toggle down idle).
             *
             * Taking a reference now would be bad anyway, since it would force
             * the object to toggle back up again.
             */
            operation->gobj = gobj;
            break;
        default:
            g_assert_not_reached();
    }

    qdata_key = get_qdata_key_for_toggle_direction(direction);

    source = g_idle_source_new();
    g_source_set_priority(source, G_PRIORITY_HIGH);
    g_source_set_callback(source,
                          idle_handle_toggle,
                          operation,
                          (GDestroyNotify) toggle_ref_notify_operation_free);

    g_atomic_int_inc(&pending_idle_toggles);
    g_object_set_qdata (gobj, qdata_key, source);
    g_source_attach (source, NULL);

    /* object qdata is piggy-backing off the main loop's ref of the source */
    g_source_unref (source);
}

static void
wrapped_gobj_toggle_notify(gpointer      data,
                           GObject      *gobj,
                           gboolean      is_last_ref)
{
    gboolean is_main_thread, is_sweeping;
    gboolean toggle_up_queued, toggle_down_queued;
    GwkjsContext *context;
    JSContextRef js_context;

    context = gwkjs_context_get_current();
    if (_gwkjs_context_destroying(context)) {
        /* Do nothing here - we're in the process of disassociating
         * the objects.
         */
        return;
    }

    /* We only want to touch javascript from one thread.
     * If we're not in that thread, then we need to defer processing
     * to it.
     * In case we're toggling up (and thus rooting the JS object) we
     * also need to take care if GC is running. The marking side
     * of it is taken care by JS::Heap, which we use in KeepAlive,
     * so we're safe. As for sweeping, it is too late: the JS object
     * is dead, and attempting to keep it alive would soon crash
     * the process. Plus, if we touch the JSAPI, libmozjs aborts in
     * the first BeginRequest.
     * Thus, in the toggleup+sweeping case we deassociate the object
     * and the wrapper and let the wrapper die. Then, if the object
     * appears again, we log a critical.
     * In practice, a toggle up during JS finalize can only happen
     * for temporary refs/unrefs of objects that are garbage anyway,
     * because JS code is never invoked while the finalizers run
     * and C code needs to clean after itself before it returns
     * from dispose()/finalize().
     * On the other hand, toggling down is a lot simpler, because
     * we're creating more garbage. So we just add the object to
     * the keep alive and wait for the next GC cycle.
     *
     * Note that one would think that toggling up only happens
     * in the main thread (because toggling up is the result of
     * the JS object, previously visible only to JS code, becoming
     * visible to the refcounted C world), but because of weird
     * weak singletons like g_bus_get_sync() objects can see toggle-ups
     * from different threads too.
     * We could lock the keep alive structure and avoid the idle maybe,
     * but there aren't many peculiar objects like that and it's
     * not a big deal.
     */
    is_main_thread = (gwkjs_eval_thread == g_thread_self());
    if (is_main_thread) {
        js_context = (JSContextRef) gwkjs_context_get_native_context(context);
        is_sweeping = JS_FALSE;
    } else {
        is_sweeping = FALSE;
    }

    toggle_up_queued = toggle_idle_source_is_queued(gobj, TOGGLE_UP);
    toggle_down_queued = toggle_idle_source_is_queued(gobj, TOGGLE_DOWN);

    if (is_last_ref) {
        /* We've transitions from 2 -> 1 references,
         * The JSObject is rooted and we need to unroot it so it
         * can be garbage collected
         */
        if (is_main_thread) {
            if (G_UNLIKELY (toggle_up_queued || toggle_down_queued)) {
                g_error("toggling down object %s that's already queued to toggle %s\n",
                        G_OBJECT_TYPE_NAME(gobj),
                        toggle_up_queued && toggle_down_queued? "up and down" :
                        toggle_up_queued? "up" : "down");
            }

            handle_toggle_down(gobj);
        } else {
            queue_toggle_idle(gobj, TOGGLE_DOWN);
        }
    } else {
        /* We've transitioned from 1 -> 2 references.
         *
         * The JSObject associated with the gobject is not rooted,
         * but it needs to be. We'll root it.
         */
        if (is_main_thread && !toggle_down_queued) {
            if (G_UNLIKELY (toggle_up_queued)) {
                g_error("toggling up object %s that's already queued to toggle up\n",
                        G_OBJECT_TYPE_NAME(gobj));
            }
            if (is_sweeping) {
                JSObjectRef object;

                object = peek_js_obj(gobj);
// TODO: no equivalence in JSC
//                if (JS_IsAboutToBeFinalized(&object)) {
//                    /* Ouch, the JS object is dead already. Disassociate the GObject
//                     * and hope the GObject dies too.
//                     */
//                    disassociate_js_gobject(gobj);
//                }
            } else {
                handle_toggle_up(gobj);
            }
        } else {
            queue_toggle_idle(gobj, TOGGLE_UP);
        }
    }
}

static void
release_native_object (ObjectInstance *priv)
{
    set_js_obj(priv->gobj, NULL);
    g_object_remove_toggle_ref(priv->gobj, wrapped_gobj_toggle_notify, NULL);
    priv->gobj = NULL;
}

/* At shutdown, we need to ensure we've cleared the context of any
 * pending toggle references.
 */
void
gwkjs_object_prepare_shutdown (JSContextRef context)
{
    JSObjectRef keep_alive = gwkjs_keep_alive_get_global_if_exists (context);
    GwkjsKeepAliveIter kiter;
    JSObjectRef child;
    void *data;

    if (!keep_alive)
        return;

    /* First, get rid of anything left over on the main context */
    while (g_main_context_pending(NULL) &&
           g_atomic_int_get(&pending_idle_toggles) > 0) {
        g_main_context_iteration(NULL, FALSE);
    }

    /* Now, we iterate over all of the objects, breaking the JS <-> C
     * associaton.  We avoid the potential recursion implied in:
     *   toggle ref removal -> gobj dispose -> toggle ref notify
     * by simply ignoring toggle ref notifications during this process.
     */
    gwkjs_keep_alive_iterator_init(&kiter, keep_alive);
    while (gwkjs_keep_alive_iterator_next(&kiter,
                                        gobj_no_longer_kept_alive_func,
                                        &child, &data)) {
        ObjectInstance *priv = (ObjectInstance*)data;

        release_native_object(priv);
    }
}

static ObjectInstance *
init_object_private (JSContextRef context,
                     JSObjectRef  object)
{
    ObjectInstance *proto_priv;
    ObjectInstance *priv;

    priv = g_slice_new0(ObjectInstance);
    priv->signature = ObjectInstanceSignature;
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    GWKJS_INC_COUNTER(object);

    g_assert(priv_from_js(object) == NULL);
    JSObjectSetPrivate(object, priv);
    g_assert(priv_from_js(object) == priv);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
                        "obj instance constructor, obj %p priv %p", object, priv);

    proto_priv = proto_priv_from_js(context, object);
    g_assert(proto_priv != NULL);
    g_assert(proto_priv->signature == ObjectInstanceSignature);

    priv->gtype = proto_priv->gtype;
    priv->info = proto_priv->info;
    if (priv->info)
        g_base_info_ref( (GIBaseInfo*) priv->info);

    return priv;
}

static void
associate_js_gobject (JSContextRef      context,
                      JSObjectRef       object,
                      GObject        *gobj)
{
    ObjectInstance *priv;

    priv = priv_from_js(object);
    priv->gobj = gobj;

    g_assert(peek_js_obj(gobj) == NULL);
    set_js_obj(gobj, object);

#if DEBUG_DISPOSE
    g_object_weak_ref(gobj, wrapped_gobj_dispose_notify, object);
#endif

    /* OK, here is where things get complicated. We want the
     * wrapped gobj to keep the JSObject* wrapper alive, because
     * people might set properties on the JSObject* that they care
     * about. Therefore, whenever the refcount on the wrapped gobj
     * is >1, i.e. whenever something other than the wrapper is
     * referencing the wrapped gobj, the wrapped gobj has a strong
     * ref (gc-roots the wrapper). When the refcount on the
     * wrapped gobj is 1, then we change to a weak ref to allow
     * the wrapper to be garbage collected (and thus unref the
     * wrappee).
     */
    priv->keep_alive = gwkjs_keep_alive_get_global(context);
    gwkjs_keep_alive_add_child(priv->keep_alive,
                             gobj_no_longer_kept_alive_func,
                             object,
                             priv);

    g_object_add_toggle_ref(gobj, wrapped_gobj_toggle_notify, NULL);

}

//static void
//disassociate_js_gobject (GObject   *gobj)
//{
//    JSObjectRef object;
//    ObjectInstance *priv;
//
//    object = peek_js_obj(gobj);
//    priv = (ObjectInstance*) JS_GetPrivate(object);
//    /* Idles are already checked in the only caller of this
//       function, the toggle ref notify, but let's check again...
//    */
//    g_assert(!cancel_toggle_idle(gobj, TOGGLE_UP));
//    g_assert(!cancel_toggle_idle(gobj, TOGGLE_DOWN));
//
//    invalidate_all_signals(priv);
//    release_native_object(priv);
//
//    /* Use -1 to mark that a JS object once existed, but it doesn't any more */
//    set_js_obj(gobj, (JSObject*)(-1));
//
//#if DEBUG_DISPOSE
//    g_object_weak_unref(gobj, wrapped_gobj_dispose_notify, object);
//#endif
//}
//
static JSBool
object_instance_init (JSContextRef context,
                      JSObjectRef *object,
                      unsigned   argc,
                      const JSValueRef argv[])
{
    ObjectInstance *priv = NULL;
    GType gtype;
    GParameter *params;
    int n_params;
    GTypeQuery query;
    JSObjectRef old_jsobj;
    GObject *gobj;

    priv = (ObjectInstance *) JSObjectGetPrivate(*object);

    gtype = priv->gtype;
    g_assert(gtype != G_TYPE_NONE);

    if (!object_instance_props_to_g_parameters(context, *object, argc, argv,
                                               gtype,
                                               &params, &n_params)) {
        return JS_FALSE;
    }

    /* Mark this object in the construction stack, it
       will be popped in gwkjs_object_custom_init() later
       down.
    */
    if (g_type_get_qdata(gtype, gwkjs_is_custom_type_quark()))
        object_init_list = g_slist_prepend(object_init_list, *object);

    gobj = (GObject*) g_object_newv(gtype, n_params, params);

    free_g_params(params, n_params);

    old_jsobj = peek_js_obj(gobj);
    if (old_jsobj != NULL && old_jsobj != *object) {
        /* g_object_newv returned an object that's already tracked by a JS
         * object. Let's assume this is a singleton like IBus.IBus and return
         * the existing JS wrapper object.
         *
         * 'object' has a value that was originally created by
         * JS_NewObjectForConstructor in GWKJS_NATIVE_CONSTRUCTOR_PRELUDE, but
         * we're not actually using it, so just let it get collected. Avoiding
         * this would require a non-trivial amount of work.
         * */
        *object = old_jsobj;
        g_object_unref(gobj); /* We already own a reference */
        gobj = NULL;
        goto out;
    }

    g_type_query_dynamic_safe(gtype, &query);
//TODO: There's no equivalent in JSC
//    if (G_LIKELY (query.type))
//        JS_updateMallocCounter(context, query.instance_size);

    if (G_IS_INITIALLY_UNOWNED(gobj) &&
        !g_object_is_floating(gobj)) {
        /* GtkWindow does not return a ref to caller of g_object_new.
         * Need a flag in gobject-introspection to tell us this.
         */
        gwkjs_debug(GWKJS_DEBUG_GOBJECT,
                  "Newly-created object is initially unowned but we did not get the "
                  "floating ref, probably GtkWindow, using hacky workaround");
        g_object_ref(gobj);
    } else if (g_object_is_floating(gobj)) {
        g_object_ref_sink(gobj);
    } else {
        /* we should already have a ref */
    }

    if (priv->gobj == NULL)
        associate_js_gobject(context, *object, gobj);
    /* We now have both a ref and a toggle ref, we only want the
     * toggle ref. This may immediately remove the GC root
     * we just added, since refcount may drop to 1.
     */
    g_object_unref(gobj);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
                        "JSObject created with GObject %p %s",
                        priv->gobj, g_type_name_from_instance((GTypeInstance*) priv->gobj));

    TRACE(GWKJS_OBJECT_PROXY_NEW(priv, priv->gobj,
                               priv->info ? g_base_info_get_namespace((GIBaseInfo*) priv->info) : "_gwkjs_private",
                               priv->info ? g_base_info_get_name((GIBaseInfo*) priv->info) : g_type_name(gtype)));

 out:
    return JS_TRUE;
}

GWKJS_NATIVE_CONSTRUCTOR_DECLARE(object_instance)
{
    GWKJS_NATIVE_CONSTRUCTOR_VARIABLES(object_instance)
    JSBool ret;
    jsval initer;
    jsval rval;
    const gchar* object_init_name;

    GWKJS_NATIVE_CONSTRUCTOR_PRELUDE(object_instance);

    /* Init the private variable before we do anything else. If a garbage
     * collection happens when calling the init function then this object
     * might be traced and we will end up dereferencing a null pointer */
    init_object_private(context, object);

    object_init_name = gwkjs_context_get_const_string(context, GWKJS_STRING_GOBJECT_INIT);
    if (!gwkjs_object_require_property(context, object,
                                       "GObject instance",
                                       object_init_name,
                                       &initer))
        return NULL;

    rval = NULL;
    ret = gwkjs_call_function_value(context, object, initer, argc, arguments, &rval);

    if (JSVAL_IS_VOID(context, rval))
        rval = object;

// TODO: implement
// Native code used to return rval. We're returning the object itself.
// So, we're keeping a g_warning to check this later
    if (!JSValueIsObject(context, rval))
        g_warning("Return value probably incorrect");

    return JSValueToObject(context, rval, NULL);
}

static void
invalidate_all_signals(ObjectInstance *priv)
{
    GList *iter, *next;

    for (iter = priv->signals; iter; ) {
        ConnectData *cd = (ConnectData*) iter->data;
        next = iter->next;

        /* This will also free cd and iter, through
           the closure invalidation mechanism */
        g_closure_invalidate(cd->closure);

        iter = next;
    }
}

//static void
//object_instance_trace(JSTracer *tracer,
//                      JSObjectRef obj)
//{
//    ObjectInstance *priv;
//    GList *iter;
//
//    priv = (ObjectInstance *) JS_GetPrivate(obj);
//
//    for (iter = priv->signals; iter; iter = iter->next) {
//        ConnectData *cd = (ConnectData *) iter->data;
//
//        gwkjs_closure_trace(cd->closure, tracer);
//    }
//}
//
static void
object_instance_finalize(JSObjectRef  obj)
{
    gwkjs_throw(NULL, " object_instance_finalize  not implemented");
//TODO: implement
//    ObjectInstance *priv;
//
//    priv = (ObjectInstance *) JS_GetPrivate(obj);
//    gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
//                        "finalize obj %p priv %p gtype %s gobj %p", obj, priv,
//                        (priv && priv->gobj) ?
//                        g_type_name_from_instance( (GTypeInstance*) priv->gobj) :
//                        "<no gobject>",
//                        priv ? priv->gobj : NULL);
//    g_assert (priv != NULL);
//
//    TRACE(GWKJS_OBJECT_PROXY_FINALIZE(priv, priv->gobj,
//                                    priv->info ? g_base_info_get_namespace((GIBaseInfo*) priv->info) : "_gwkjs_private",
//                                    priv->info ? g_base_info_get_name((GIBaseInfo*) priv->info) : g_type_name(priv->gtype)));
//
//    if (priv->gobj) {
//        gboolean had_toggle_up;
//        gboolean had_toggle_down;
//
//        invalidate_all_signals (priv);
//
//        if (G_UNLIKELY (priv->gobj->ref_count <= 0)) {
//            g_error("Finalizing proxy for an already freed object of type: %s.%s\n",
//                    priv->info ? g_base_info_get_namespace((GIBaseInfo*) priv->info) : "",
//                    priv->info ? g_base_info_get_name((GIBaseInfo*) priv->info) : g_type_name(priv->gtype));
//        }
//
//        had_toggle_up = cancel_toggle_idle(priv->gobj, TOGGLE_UP);
//        had_toggle_down = cancel_toggle_idle(priv->gobj, TOGGLE_DOWN);
//
//        if (!had_toggle_up && had_toggle_down) {
//            g_error("Finalizing proxy for an object that's scheduled to be unrooted: %s.%s\n",
//                    priv->info ? g_base_info_get_namespace((GIBaseInfo*) priv->info) : "",
//                    priv->info ? g_base_info_get_name((GIBaseInfo*) priv->info) : g_type_name(priv->gtype));
//        }
//        
//        release_native_object(priv);
//    }
//
//    if (priv->keep_alive != NULL) {
//        /* This happens when the refcount on the object is still >1,
//         * for example with global objects GDK never frees like GdkDisplay,
//         * when we close down the JS runtime.
//         */
//        gwkjs_debug(GWKJS_DEBUG_GOBJECT,
//                  "Wrapper was finalized despite being kept alive, has refcount >1");
//
//        gwkjs_debug_lifecycle(GWKJS_DEBUG_GOBJECT,
//                            "Removing from keep alive");
//
//        gwkjs_keep_alive_remove_child(priv->keep_alive,
//                                    gobj_no_longer_kept_alive_func,
//                                    obj,
//                                    priv);
//    }
//
//    if (priv->info) {
//        g_base_info_unref( (GIBaseInfo*) priv->info);
//        priv->info = NULL;
//    }
//
//    if (priv->klass) {
//        g_type_class_unref (priv->klass);
//        priv->klass = NULL;
//    }
//
//    GWKJS_DEC_COUNTER(object);
//    g_slice_free(ObjectInstance, priv);
}

static JSObjectRef
gwkjs_lookup_object_constructor_from_info(JSContextRef    context,
                                        GIObjectInfo *info,
                                        GType         gtype)
{
    JSObjectRef in_object = NULL;
    JSObjectRef constructor = NULL;
    const char *constructor_name = NULL;
    JSValueRef value = NULL;
    JSValueRef exception = NULL;

    if (info) {
        in_object = gwkjs_lookup_namespace_object(context, (GIBaseInfo*) info);
        constructor_name = g_base_info_get_name((GIBaseInfo*) info);
    } else {
        in_object = gwkjs_lookup_private_namespace(context);
        constructor_name = g_type_name(gtype);
    }

    if (G_UNLIKELY (!in_object))
        return NULL;

    value = gwkjs_object_get_property(context, in_object, constructor_name, &exception);
    if (exception)
        return NULL;

    if (JSVAL_IS_VOID(context, value)) {
        /* In case we're looking for a private type, and we don't find it,
           we need to define it first.
        */
        gwkjs_define_object_class(context, in_object, NULL, gtype, &constructor);
    } else {
        if (G_UNLIKELY (!JSValueIsObject(context, value) || JSVAL_IS_NULL(context, value)))
            return NULL;

        constructor = JSValueToObject(context, value, NULL);
    }

    g_assert(constructor != NULL);

    return constructor;
}

static JSObjectRef
gwkjs_lookup_object_prototype_from_info(JSContextRef    context,
                                      GIObjectInfo *info,
                                      GType         gtype)
{
    JSObjectRef constructor = NULL;
    jsval value = NULL;

    constructor = gwkjs_lookup_object_constructor_from_info(context, info, gtype);

    if (G_UNLIKELY (constructor == NULL))
        return NULL;

    if (!gwkjs_object_get_property_const(context, constructor,
                                               GWKJS_STRING_PROTOTYPE, &value))
        return NULL;

    if (G_UNLIKELY (!JSValueIsObject(context, value)))
        return NULL;

    return JSValueToObject(context, value, NULL);
}

static JSObjectRef
gwkjs_lookup_object_prototype(JSContextRef context,
                            GType      gtype)
{
    GIObjectInfo *info;
    JSObjectRef proto;

    info = (GIObjectInfo*)g_irepository_find_by_gtype(g_irepository_get_default(), gtype);
    proto = gwkjs_lookup_object_prototype_from_info(context, info, gtype);
    if (info)
        g_base_info_unref((GIBaseInfo*)info);

    return proto;
}

static void
signal_connection_invalidated (gpointer  user_data,
                               GClosure *closure)
{
    ConnectData *connect_data = (ConnectData *) user_data;

    connect_data->obj->signals = g_list_delete_link(connect_data->obj->signals,
                                                    connect_data->link);
    g_slice_free(ConnectData, connect_data);
}

static JSValueRef
real_connect_func(JSContextRef context,
                  JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, gboolean after)
{
    g_warning("real_connect_func NOT IMPLEMENTED");
    return JSValueMakeUndefined(context);

//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    JSObjectRef obj = JSVAL_TO_OBJECT(argv.thisv());
//
//    ObjectInstance *priv;
//    GClosure *closure;
//    gulong id;
//    guint signal_id;
//    char *signal_name;
//    GQuark signal_detail;
//    jsval retval;
//    ConnectData *connect_data;
//    JSBool ret = JS_FALSE;
//
//    if (!do_base_typecheck(context, obj, JS_TRUE))
//        return JS_FALSE;
//
//    priv = priv_from_js(context, obj);
//    gwkjs_debug_gsignal("connect obj %p priv %p argc %d", obj, priv, argc);
//    if (priv == NULL) {
//        throw_priv_is_null_error(context);
//        return JS_FALSE; /* wrong class passed in */
//    }
//    if (priv->gobj == NULL) {
//        /* prototype, not an instance. */
//        gwkjs_throw(context, "Can't connect to signals on %s.%s.prototype; only on instances",
//                  priv->info ? g_base_info_get_namespace( (GIBaseInfo*) priv->info) : "",
//                  priv->info ? g_base_info_get_name( (GIBaseInfo*) priv->info) : g_type_name(priv->gtype));
//        return JS_FALSE;
//    }
//
//    /* Best I can tell, there is no way to know if argv[1] is really
//     * callable other than to just try it. Checking whether it's a
//     * function will not detect native objects that provide
//     * JSClass::call, for example.
//     */
//
//    if (argc != 2 ||
//        !JSVAL_IS_STRING(argv[0]) ||
//        !JSVAL_IS_OBJECT(argv[1])) {
//        gwkjs_throw(context, "connect() takes two args, the signal name and the callback");
//        return JS_FALSE;
//    }
//
//    if (!gwkjs_string_to_utf8(context, argv[0], &signal_name)) {
//        return JS_FALSE;
//    }
//
//    if (!g_signal_parse_name(signal_name,
//                             G_OBJECT_TYPE(priv->gobj),
//                             &signal_id,
//                             &signal_detail,
//                             TRUE)) {
//        gwkjs_throw(context, "No signal '%s' on object '%s'",
//                     signal_name,
//                     g_type_name(G_OBJECT_TYPE(priv->gobj)));
//        goto out;
//    }
//
//    closure = gwkjs_closure_new_for_signal(context, JSVAL_TO_OBJECT(argv[1]), "signal callback", signal_id);
//    if (closure == NULL)
//        goto out;
//
//    connect_data = g_slice_new(ConnectData);
//    priv->signals = g_list_prepend(priv->signals, connect_data);
//    connect_data->obj = priv;
//    connect_data->link = priv->signals;
//    /* This is a weak reference, and will be cleared when the closure is invalidated */
//    connect_data->closure = closure;
//    g_closure_add_invalidate_notifier(closure, connect_data, signal_connection_invalidated);
//
//    id = g_signal_connect_closure_by_id(priv->gobj,
//                                        signal_id,
//                                        signal_detail,
//                                        closure,
//                                        after);
//
//    if (!JS_NewNumberValue(context, id, &retval)) {
//        g_signal_handler_disconnect(priv->gobj, id);
//        goto out;
//    }
//    
//    argv.rval().set(retval);
//
//    ret = JS_TRUE;
// out:
//    g_free(signal_name);
//    return ret;
}

static JSValueRef
connect_after_func(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return real_connect_func(ctx, function, thisObject, argumentCount, arguments, exception, TRUE);
}

static JSValueRef
connect_func(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return real_connect_func(ctx, function, thisObject, argumentCount, arguments, exception, false);
}

static JSValueRef
emit_func(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    g_warning("emit_func NOT IMPLEMENTED");
    return NULL;
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    JSObjectRef obj = JSVAL_TO_OBJECT(argv.thisv());
//
//    ObjectInstance *priv;
//    guint signal_id;
//    GQuark signal_detail;
//    GSignalQuery signal_query;
//    char *signal_name;
//    GValue *instance_and_args;
//    GValue rvalue = G_VALUE_INIT;
//    unsigned int i;
//    gboolean failed;
//    jsval retval;
//    JSBool ret = JS_FALSE;
//
//    if (!do_base_typecheck(context, obj, JS_TRUE))
//        return JS_FALSE;
//
//    priv = priv_from_js(context, obj);
//    gwkjs_debug_gsignal("emit obj %p priv %p argc %d", obj, priv, argc);
//
//    if (priv == NULL) {
//        throw_priv_is_null_error(context);
//        return JS_FALSE; /* wrong class passed in */
//    }
//
//    if (priv->gobj == NULL) {
//        /* prototype, not an instance. */
//        gwkjs_throw(context, "Can't emit signal on %s.%s.prototype; only on instances",
//                  priv->info ? g_base_info_get_namespace( (GIBaseInfo*) priv->info) : "",
//                  priv->info ? g_base_info_get_name( (GIBaseInfo*) priv->info) : g_type_name(priv->gtype));
//        return JS_FALSE;
//    }
//
//    if (argc < 1 ||
//        !JSVAL_IS_STRING(argv[0])) {
//        gwkjs_throw(context, "emit() first arg is the signal name");
//        return JS_FALSE;
//    }
//
//    if (!gwkjs_string_to_utf8(context, argv[0], &signal_name))
//        return JS_FALSE;
//
//    if (!g_signal_parse_name(signal_name,
//                             G_OBJECT_TYPE(priv->gobj),
//                             &signal_id,
//                             &signal_detail,
//                             FALSE)) {
//        gwkjs_throw(context, "No signal '%s' on object '%s'",
//                     signal_name,
//                     g_type_name(G_OBJECT_TYPE(priv->gobj)));
//        goto out;
//    }
//
//    g_signal_query(signal_id, &signal_query);
//
//    if ((argc - 1) != signal_query.n_params) {
//        gwkjs_throw(context, "Signal '%s' on %s requires %d args got %d",
//                     signal_name,
//                     g_type_name(G_OBJECT_TYPE(priv->gobj)),
//                     signal_query.n_params,
//                     argc - 1);
//        goto out;
//    }
//
//    if (signal_query.return_type != G_TYPE_NONE) {
//        g_value_init(&rvalue, signal_query.return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
//    }
//
//    instance_and_args = g_newa(GValue, signal_query.n_params + 1);
//    memset(instance_and_args, 0, sizeof(GValue) * (signal_query.n_params + 1));
//
//    g_value_init(&instance_and_args[0], G_TYPE_FROM_INSTANCE(priv->gobj));
//    g_value_set_instance(&instance_and_args[0], priv->gobj);
//
//    failed = FALSE;
//    for (i = 0; i < signal_query.n_params; ++i) {
//        GValue *value;
//        value = &instance_and_args[i + 1];
//
//        g_value_init(value, signal_query.param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE);
//        if ((signal_query.param_types[i] & G_SIGNAL_TYPE_STATIC_SCOPE) != 0)
//            failed = !gwkjs_value_to_g_value_no_copy(context, argv[i+1], value);
//        else
//            failed = !gwkjs_value_to_g_value(context, argv[i+1], value);
//
//        if (failed)
//            break;
//    }
//
//    if (!failed) {
//        g_signal_emitv(instance_and_args, signal_id, signal_detail,
//                       &rvalue);
//    }
//
//    if (signal_query.return_type != G_TYPE_NONE) {
//        if (!gwkjs_value_from_g_value(context,
//                                    &retval,
//                                    &rvalue))
//            failed = TRUE;
//
//        g_value_unset(&rvalue);
//    } else {
//        retval = JSVAL_VOID;
//    }
//
//    for (i = 0; i < (signal_query.n_params + 1); ++i) {
//        g_value_unset(&instance_and_args[i]);
//    }
//
//    if (!failed)
//        argv.rval().set(retval);
//
//    ret = !failed;
// out:
//    g_free(signal_name);
//    return ret;
}

static JSValueRef
to_string_func(JSContextRef context, JSObjectRef function, JSObjectRef obj, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{

    ObjectInstance *priv = NULL;
    jsval retval = NULL;

    if (!do_base_typecheck(context, obj, JS_TRUE))
        goto out;

    priv = priv_from_js(obj);

    if (priv == NULL) {
        throw_priv_is_null_error(context);
        goto out;  /* wrong class passed in */
    }

    if (!_gwkjs_proxy_to_string_func(context, obj, "object", (GIBaseInfo*)priv->info,
                                   priv->gtype, priv->gobj, &retval))
        goto out;

    return retval;
 out:
    return NULL;
}

static JSValueRef
init_func (JSContextRef context,
           JSObjectRef function,
           JSObjectRef obj,
           size_t argumentCount,
           const JSValueRef arguments[],
           JSValueRef* exception)
{
    JSBool ret;

    if (!do_base_typecheck(context, obj, TRUE))
        return FALSE;

    ret = object_instance_init(context, &obj, argumentCount, arguments);

    if (ret)
        return obj;

    return NULL;
}


JSBool
gwkjs_object_define_static_methods(JSContextRef    context,
                                 JSObjectRef     constructor,
                                 GType         gtype,
                                 GIObjectInfo *object_info)
{
    GIStructInfo *gtype_struct;
    int i;
    int n_methods;

    n_methods = g_object_info_get_n_methods(object_info);

    for (i = 0; i < n_methods; i++) {
        GIFunctionInfo *meth_info;
        GIFunctionInfoFlags flags;

        meth_info = g_object_info_get_method(object_info, i);
        flags = g_function_info_get_flags (meth_info);

        /* Anything that isn't a method we put on the prototype of the
         * constructor.  This includes <constructor> introspection
         * methods, as well as the forthcoming "static methods"
         * support.  We may want to change this to use
         * GI_FUNCTION_IS_CONSTRUCTOR and GI_FUNCTION_IS_STATIC or the
         * like in the near future.
         */
        if (!(flags & GI_FUNCTION_IS_METHOD)) {
            gwkjs_define_function(context, constructor, gtype,
                                (GICallableInfo *)meth_info);
        }

        g_base_info_unref((GIBaseInfo*) meth_info);
    }

    gtype_struct = g_object_info_get_class_struct(object_info);

    if (gtype_struct == NULL)
        return JS_TRUE;

    n_methods = g_struct_info_get_n_methods(gtype_struct);

    for (i = 0; i < n_methods; i++) {
        GIFunctionInfo *meth_info;

        meth_info = g_struct_info_get_method(gtype_struct, i);

        gwkjs_define_function(context, constructor, gtype,
                            (GICallableInfo*)meth_info);

        g_base_info_unref((GIBaseInfo*) meth_info);
    }

    g_base_info_unref((GIBaseInfo*) gtype_struct);
    return JS_TRUE;
}

JSStaticValue gwkjs_object_instance_proto_props[] = {
    { NULL, NULL, NULL, 0 }
};

JSStaticFunction gwkjs_object_instance_proto_funcs[] = {
    { "_init", init_func, 0 },
    { "connect", connect_func, 0 },
    { "connect_after", connect_after_func, 0 },
    { "emit", emit_func, 0 },
    { "toString", to_string_func, 0 },
    { NULL, NULL, 0 }
};


JSClassDefinition gwkjs_object_instance_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GObject_Object",          //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    NULL,                      //     const JSStaticValue*                staticValues;
    NULL,                      //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    object_instance_finalize,  //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;

    object_instance_get_prop,  //TODO: is this really resolve?
                               //     JSObjectGetPropertyCallback         getProperty;

    object_instance_set_prop,  //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    NULL,                      //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_object_instance_constructor, //     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};

JSObjectRef
gwkjs_define_object_class(JSContextRef      context,
                        JSObjectRef       in_object,
                        GIObjectInfo   *info,
                        GType           gtype,
                        JSObjectRef      *constructor_p)
{
    const char *constructor_name = NULL;
    JSObjectRef prototype = NULL;
    JSObjectRef constructor = NULL;
    JSObjectRef parent_proto = NULL;
    JSObjectRef global = NULL;

    JSObjectRef value = NULL;
    ObjectInstance *priv = NULL;
    const char *ns = NULL;
    GType parent_type;

    g_assert(in_object != NULL);
    g_assert(gtype != G_TYPE_INVALID);

    /*   http://egachine.berlios.de/embedding-sm-best-practice/apa.html
     *   http://www.sitepoint.com/blogs/2006/01/17/javascript-inheritance/
     *   http://www.cs.rit.edu/~atk/JavaScript/manuals/jsobj/
     *
     * What we want is:
     *
     * repoobj.Gtk.Window is constructor for a GtkWindow wrapper JSObject
     *   (gwkjs_define_object_constructor() is supposed to define Window in Gtk)
     *
     * Window.prototype contains the methods on Window, e.g. set_default_size()
     * mywindow.__proto__ is Window.prototype
     * mywindow.__proto__.__proto__ is Bin.prototype
     * mywindow.__proto__.__proto__.__proto__ is Container.prototype
     *
     * Because Window.prototype is an instance of Window in a sense,
     * Window.prototype.__proto__ is Window.prototype, just as
     * mywindow.__proto__ is Window.prototype
     *
     * If we do "mywindow = new Window()" then we should get:
     *     mywindow.__proto__ == Window.prototype
     * which means "mywindow instanceof Window" is true.
     *
     * Remember "Window.prototype" is "the __proto__ of stuff
     * constructed with new Window()"
     *
     * __proto__ is used to search for properties if you do "this.foo"
     * while __parent__ defines the scope to search if you just have
     * "foo".
     *
     * __proto__ is used to look up properties, while .prototype is only
     * relevant for constructors and is used to set __proto__ on new'd
     * objects. So .prototype only makes sense on constructors.
     *
     * JS_SetPrototype() and JS_GetPrototype() are for __proto__.
     * To set/get .prototype, just use the normal property accessors,
     * or JS_InitClass() sets it up automatically.
     *
     * JavaScript is SO AWESOME
     */

    parent_proto = NULL;
    parent_type = g_type_parent(gtype);
    if (parent_type != G_TYPE_INVALID)
       parent_proto = gwkjs_lookup_object_prototype(context, parent_type);

    ns = gwkjs_get_names_from_gtype_and_gi_info(gtype, (GIBaseInfo *) info,
                                              &constructor_name);

    if (!gwkjs_object_instance_class_ref) {
        gwkjs_object_instance_class_ref = JSClassCreate(&gwkjs_object_instance_class);
    }

// TODO: check the conversion bellow
//    if (!gwkjs_init_class_dynamic(context, in_object,
//                                parent_proto,
//                                ns, constructor_name,
//                                &gwkjs_object_instance_class,
//                                &gwkjs_object_instance_class_ref,
//                                gwkjs_object_instance_constructor, 0,
//                                /* props of prototype */
//                                parent_proto ? NULL : &gwkjs_object_instance_proto_props[0],
//                                /* funcs of prototype */
//                                parent_proto ? NULL : &gwkjs_object_instance_proto_funcs[0],
//                                /* props of constructor, MyConstructor.myprop */
//                                NULL,
//                                /* funcs of constructor, MyConstructor.myfunc() */
//                                NULL,
//                                &prototype,
//                                &constructor)) {
//        g_error("Can't init class %s", constructor_name);
//    }
//
    if (!gwkjs_init_class_dynamic(context, in_object,
                                  parent_proto, ns, constructor_name,
                                  &gwkjs_object_instance_class,
                                  gwkjs_object_instance_class_ref,
                                  gwkjs_object_instance_constructor, 0,
                                  /* props of prototype */
                                  parent_proto ? NULL : &gwkjs_object_instance_proto_props[0],
                                  /* funcs of prototype */
                                  parent_proto ? NULL : &gwkjs_object_instance_proto_funcs[0],
                                  /* props of constructor, MyConstructor.myprop */
                                  NULL,
                                  /* funcs of constructor, MyConstructor.myfunc() */
                                  NULL,
                                  &prototype,
                                  &constructor)) {
        g_error("Can't init class %s", constructor_name);
    }

    GWKJS_INC_COUNTER(object);
    priv = g_slice_new0(ObjectInstance);
    priv->signature = ObjectInstanceSignature;
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    priv->info = info;
    if (info)
        g_base_info_ref((GIBaseInfo*) info);
    priv->gtype = gtype;
    priv->klass = (GTypeClass*) g_type_class_ref (gtype);
    JSObjectSetPrivate(prototype, priv);

    gwkjs_debug(GWKJS_DEBUG_GOBJECT, "Defined class %s prototype %p class %p in object %p",
              constructor_name, prototype, prototype, in_object);

    if (info)
        gwkjs_object_define_static_methods(context, constructor, gtype, info);

    value = gwkjs_gtype_create_gtype_wrapper(context, gtype);
    gwkjs_object_set_property(context, constructor, "$gtype", value, kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete, NULL);

    if (constructor_p) {
        *constructor_p = constructor;
    }

    return constructor;
}

static JSObjectRef
peek_js_obj(GObject *gobj)
{
    JSObjectRef object = (JSObjectRef) g_object_get_qdata(gobj, gwkjs_object_priv_quark());

    if (G_UNLIKELY (object == (JSObjectRef)(-1))) {
        g_critical ("Object %p (a %s) resurfaced after the JS wrapper was finalized. "
                    "This is some library doing dubious memory management inside dispose()",
                    gobj, g_type_name(G_TYPE_FROM_INSTANCE(object)));
        return NULL; /* return null to associate again with a new wrapper */
    }

    return object;
}

static void
set_js_obj(GObject  *gobj,
           JSObjectRef obj)
{
    g_object_set_qdata(gobj, gwkjs_object_priv_quark(), obj);
}

JSObjectRef
gwkjs_object_from_g_object(JSContextRef    context,
                         GObject      *gobj)
{
    JSObjectRef obj;
    JSObjectRef global;

    if (gobj == NULL)
        return NULL;

    obj = peek_js_obj(gobj);

    if (obj == NULL) {
        /* We have to create a wrapper */
        JSObjectRef proto;
        GType gtype;

        gwkjs_debug_marshal(GWKJS_DEBUG_GOBJECT,
                          "Wrapping %s with JSObject",
                          g_type_name_from_instance((GTypeInstance*) gobj));

        gtype = G_TYPE_FROM_INSTANCE(gobj);
        proto = gwkjs_lookup_object_prototype(context, gtype);

        obj = gwkjs_new_object(context, NULL, proto, gwkjs_get_import_global (context));

        if (obj == NULL)
            goto out;

        init_object_private(context, obj);

        g_object_ref_sink(gobj);
        associate_js_gobject(context, obj, gobj);

        /* see the comment in init_object_instance() for this */
        g_object_unref(gobj);

        g_assert(peek_js_obj(gobj) == obj);
    }

 out:
    return obj;
}

GObject*
gwkjs_g_object_from_object(JSContextRef    context,
                           JSObjectRef     obj)
{
    ObjectInstance *priv;

    if (obj == NULL)
        return NULL;

    priv = priv_from_js(obj);
    return priv->gobj;
}

JSBool
gwkjs_typecheck_is_object(JSContextRef     context,
                        JSObjectRef      object,
                        JSBool         throw_error)
{
    return do_base_typecheck(context, object, throw_error);
}

JSBool
gwkjs_typecheck_object(JSContextRef context,
                     JSObjectRef    object,
                     GType          expected_type,
                     JSBool         throw_error)
{
    ObjectInstance *priv;
    JSBool result;

    if (!do_base_typecheck(context, object, throw_error))
        return JS_FALSE;

    priv = priv_from_js(object);

    if (priv == NULL) {
        if (throw_error) {
            gwkjs_throw(context,
                      "Object instance or prototype has not been properly initialized yet. "
                      "Did you forget to chain-up from _init()?");
        }

        return JS_FALSE;
    }

    if (priv->gobj == NULL) {
        if (throw_error) {
            gwkjs_throw(context,
                      "Object is %s.%s.prototype, not an object instance - cannot convert to GObject*",
                      priv->info ? g_base_info_get_namespace( (GIBaseInfo*) priv->info) : "",
                      priv->info ? g_base_info_get_name( (GIBaseInfo*) priv->info) : g_type_name(priv->gtype));
        }

        return JS_FALSE;
    }

    g_assert(priv->gtype == G_OBJECT_TYPE(priv->gobj));

    if (expected_type != G_TYPE_NONE)
        result = g_type_is_a (priv->gtype, expected_type);
    else
        result = JS_TRUE;

    if (!result && throw_error) {
        if (priv->info) {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is of type %s.%s - cannot convert to %s",
                             g_base_info_get_namespace((GIBaseInfo*) priv->info),
                             g_base_info_get_name((GIBaseInfo*) priv->info),
                             g_type_name(expected_type));
        } else {
            gwkjs_throw_custom(context, "TypeError",
                             "Object is of type %s - cannot convert to %s",
                             g_type_name(priv->gtype),
                             g_type_name(expected_type));
        }
    }

    return result;
}


//static void
//find_vfunc_info (JSContextRef context,
//                 GType implementor_gtype,
//                 GIBaseInfo *vfunc_info,
//                 gchar *vfunc_name,
//                 gpointer *implementor_vtable_ret,
//                 GIFieldInfo **field_info_ret)
//{
//    GType ancestor_gtype;
//    int length, i;
//    GIBaseInfo *ancestor_info;
//    GIStructInfo *struct_info;
//    gpointer implementor_class;
//    gboolean is_interface;
//
//    *field_info_ret = NULL;
//    *implementor_vtable_ret = NULL;
//
//    ancestor_info = g_base_info_get_container(vfunc_info);
//    ancestor_gtype = g_registered_type_info_get_g_type((GIRegisteredTypeInfo*)ancestor_info);
//
//    is_interface = g_base_info_get_type(ancestor_info) == GI_INFO_TYPE_INTERFACE;
//
//    implementor_class = g_type_class_ref(implementor_gtype);
//    if (is_interface) {
//        GTypeInstance *implementor_iface_class;
//        implementor_iface_class = (GTypeInstance*) g_type_interface_peek(implementor_class,
//                                                        ancestor_gtype);
//        if (implementor_iface_class == NULL) {
//            g_type_class_unref(implementor_class);
//            gwkjs_throw (context, "Couldn't find GType of implementor of interface %s.",
//                       g_type_name(ancestor_gtype));
//            return;
//        }
//
//        *implementor_vtable_ret = implementor_iface_class;
//
//        struct_info = g_interface_info_get_iface_struct((GIInterfaceInfo*)ancestor_info);
//    } else {
//        struct_info = g_object_info_get_class_struct((GIObjectInfo*)ancestor_info);
//        *implementor_vtable_ret = implementor_class;
//    }
//
//    g_type_class_unref(implementor_class);
//
//    length = g_struct_info_get_n_fields(struct_info);
//    for (i = 0; i < length; i++) {
//        GIFieldInfo *field_info;
//        GITypeInfo *type_info;
//
//        field_info = g_struct_info_get_field(struct_info, i);
//
//        if (strcmp(g_base_info_get_name((GIBaseInfo*)field_info), vfunc_name) != 0) {
//            g_base_info_unref(field_info);
//            continue;
//        }
//
//        type_info = g_field_info_get_type(field_info);
//        if (g_type_info_get_tag(type_info) != GI_TYPE_TAG_INTERFACE) {
//            /* We have a field with the same name, but it's not a callback.
//             * There's no hope of being another field with a correct name,
//             * so just abort early. */
//            g_base_info_unref(type_info);
//            g_base_info_unref(field_info);
//            break;
//        } else {
//            g_base_info_unref(type_info);
//            *field_info_ret = field_info;
//            break;
//        }
//    }
//
//    g_base_info_unref(struct_info);
//}
//
//static JSBool
//gwkjs_hook_up_vfunc(JSContextRef cx,
//                  unsigned   argc,
//                  jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    gchar *name;
//    JSObjectRef object;
//    JSObjectRef function;
//    ObjectInstance *priv;
//    GType gtype, info_gtype;
//    GIObjectInfo *info;
//    GIVFuncInfo *vfunc;
//    gpointer implementor_vtable;
//    GIFieldInfo *field_info;
//
//    if (!gwkjs_parse_call_args(cx, "hook_up_vfunc",
//                        "oso", argv,
//                        "object", &object,
//                        "name", &name,
//                        "function", &function))
//        return JS_FALSE;
//
//    if (!do_base_typecheck(cx, object, JS_TRUE))
//        return JS_FALSE;
//
//    priv = priv_from_js(cx, object);
//    gtype = priv->gtype;
//    info = priv->info;
//
//    /* find the first class that actually has repository information */
//    info_gtype = gtype;
//    while (!info && info_gtype != G_TYPE_OBJECT) {
//        info_gtype = g_type_parent(info_gtype);
//
//        info = g_irepository_find_by_gtype(g_irepository_get_default(), info_gtype);
//    }
//
//    /* If we don't have 'info', we don't have the base class (GObject).
//     * This is awful, so abort now. */
//    g_assert(info != NULL);
//
//    argv.rval().set(JSVAL_VOID);
//
//    vfunc = find_vfunc_on_parents(info, name, NULL);
//
//    if (!vfunc) {
//        guint i, n_interfaces;
//        GType *interface_list;
//        GIInterfaceInfo *interface;
//
//        interface_list = g_type_interfaces(gtype, &n_interfaces);
//
//        for (i = 0; i < n_interfaces; i++) {
//            interface = (GIInterfaceInfo*)g_irepository_find_by_gtype(g_irepository_get_default(),
//                                                                      interface_list[i]);
//
//            /* The interface doesn't have to exist -- it could be private
//             * or dynamic. */
//            if (interface) {
//                vfunc = g_interface_info_find_vfunc(interface, name);
//
//                g_base_info_unref((GIBaseInfo*)interface);
//
//                if (vfunc)
//                    break;
//            }
//        }
//
//        g_free(interface_list);
//    }
//
//    if (!vfunc) {
//        gwkjs_throw(cx, "Could not find definition of virtual function %s", name);
//
//        g_free(name);
//        return JS_FALSE;
//    }
//
//    find_vfunc_info(cx, gtype, vfunc, name, &implementor_vtable, &field_info);
//    if (field_info != NULL) {
//        GITypeInfo *type_info;
//        GIBaseInfo *interface_info;
//        GICallbackInfo *callback_info;
//        gint offset;
//        gpointer method_ptr;
//        GwkjsCallbackTrampoline *trampoline;
//
//        type_info = g_field_info_get_type(field_info);
//
//        interface_info = g_type_info_get_interface(type_info);
//
//        callback_info = (GICallbackInfo*)interface_info;
//        offset = g_field_info_get_offset(field_info);
//        method_ptr = G_STRUCT_MEMBER_P(implementor_vtable, offset);
//
//        trampoline = gwkjs_callback_trampoline_new(cx, OBJECT_TO_JSVAL(function), callback_info,
//                                                 GI_SCOPE_TYPE_NOTIFIED, TRUE);
//
//        *((ffi_closure **)method_ptr) = trampoline->closure;
//
//        g_base_info_unref(interface_info);
//        g_base_info_unref(type_info);
//        g_base_info_unref(field_info);
//    }
//
//    g_base_info_unref(vfunc);
//    g_free(name);
//    return JS_TRUE;
//}
//
//static gchar *
//hyphen_to_underscore (gchar *string)
//{
//    gchar *str, *s;
//    str = s = g_strdup(string);
//    while (*(str++) != '\0') {
//        if (*str == '-')
//            *str = '_';
//    }
//    return s;
//}
//
//static void
//gwkjs_object_get_gproperty (GObject    *object,
//                          guint       property_id,
//                          GValue     *value,
//                          GParamSpec *pspec)
//{
//    GwkjsContext *gwkjs_context;
//    JSContextRef context;
//    JSObjectRef js_obj;
//    jsval jsvalue;
//    gchar *underscore_name;
//
//    gwkjs_context = gwkjs_context_get_current();
//    context = (JSContext*) gwkjs_context_get_native_context(gwkjs_context);
//
//    js_obj = peek_js_obj(object);
//
//    underscore_name = hyphen_to_underscore((gchar *)pspec->name);
//    if (!JS_GetProperty(context, js_obj, underscore_name, &jsvalue))
//        gwkjs_log_exception(context);
//    else
//        gwkjs_value_to_g_value(context, jsvalue, value);
//    g_free (underscore_name);
//}
//
//static void
//jsobj_set_gproperty (JSContextRef    context,
//                     JSObjectRef     object,
//                     const GValue *value,
//                     GParamSpec   *pspec)
//{
//    jsval jsvalue;
//    gchar *underscore_name;
//
//    if (!gwkjs_value_from_g_value(context, &jsvalue, value))
//        return;
//
//    underscore_name = hyphen_to_underscore((gchar *)pspec->name);
//    if (!JS_SetProperty(context, object, underscore_name, &jsvalue))
//        gwkjs_log_exception(context);
//    g_free (underscore_name);
//}
//
//static GObject *
//gwkjs_object_constructor (GType                  type,
//                        guint                  n_construct_properties,
//                        GObjectConstructParam *construct_properties)
//{
//    GObject *gobj = NULL;
//
//    if (object_init_list) {
//        GType parent_type = g_type_parent(type);
//
//        /* The object is being constructed from JS:
//         * Simply chain up to the first non-gwkjs constructor
//         */
//        while (G_OBJECT_CLASS(g_type_class_peek(parent_type))->constructor == gwkjs_object_constructor)
//            parent_type = g_type_parent(parent_type);
//
//        gobj = G_OBJECT_CLASS(g_type_class_peek(parent_type))->constructor(type, n_construct_properties, construct_properties);
//    } else {
//        GwkjsContext *gwkjs_context;
//        JSContextRef context;
//        JSObjectRef object, *constructor;
//        ObjectInstance *priv;
//
//        /* The object is being constructed from native code (e.g. GtkBuilder):
//         * Construct the JS object from the constructor, then use the GObject
//         * that was associated in gwkjs_object_custom_init()
//         */
//        gwkjs_context = gwkjs_context_get_current();
//        context = (JSContext*) gwkjs_context_get_native_context(gwkjs_context);
//
//        JS_BeginRequest(context);
//
//        constructor = gwkjs_lookup_object_constructor_from_info(context, NULL, type);
//        if (!constructor)
//          goto out;
//
//        if (n_construct_properties) {
//            JSObjectRef args;
//            jsval argv;
//            guint i;
//
//            args = JS_NewObject(context, NULL, NULL, NULL);
//
//            for (i = 0; i < n_construct_properties; i++)
//                jsobj_set_gproperty(context, args,
//                                    construct_properties[i].value,
//                                    construct_properties[i].pspec);
//
//            argv = OBJECT_TO_JSVAL(args);
//            object = JS_New(context, constructor, 1, &argv);
//        } else {
//            object = JS_New(context, constructor, 0, NULL);
//        }
//
//        if (!object)
//          goto out;
//
//        priv = (ObjectInstance*) JS_GetPrivate(object);
//        /* We only hold a toggle ref at this point, add back a ref that the
//         * native code can own.
//         */
//        gobj = G_OBJECT(g_object_ref(priv->gobj));
//
//out:
//        JS_EndRequest(context);
//    }
//
//    return gobj;
//}
//
//static void
//gwkjs_object_set_gproperty (GObject      *object,
//                          guint         property_id,
//                          const GValue *value,
//                          GParamSpec   *pspec)
//{
//    GwkjsContext *gwkjs_context;
//    JSContextRef context;
//    JSObjectRef js_obj;
//
//    gwkjs_context = gwkjs_context_get_current();
//    context = (JSContext*) gwkjs_context_get_native_context(gwkjs_context);
//
//    js_obj = peek_js_obj(object);
//    jsobj_set_gproperty(context, js_obj, value, pspec);
//}
//
//static JSBool
//gwkjs_override_property(JSContextRef cx,
//                      unsigned   argc,
//                      jsval     *vp)
//{
//    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
//    gchar *name = NULL;
//    JSObjectRef type;
//    GParamSpec *pspec;
//    GParamSpec *new_pspec;
//    GType gtype;
//    GTypeInterface *interface_type;
//
//    if (!gwkjs_parse_call_args(cx, "override_property", "so", args,
//                             "name", &name,
//                             "type", &type))
//        return JS_FALSE;
//
//    if ((gtype = gwkjs_gtype_get_actual_gtype(cx, type)) == G_TYPE_INVALID) {
//        gwkjs_throw(cx, "Invalid parameter type was not a GType");
//        g_clear_pointer(&name, g_free);
//        return JS_FALSE;
//    }
//
//    if (g_type_is_a(gtype, G_TYPE_INTERFACE)) {
//        GTypeInterface *interface_type =
//            (GTypeInterface *) g_type_default_interface_ref(gtype);
//        pspec = g_object_interface_find_property(interface_type, name);
//        g_type_default_interface_unref(interface_type);
//    } else {
//        GTypeClass *class_type = (GTypeClass *) g_type_class_ref(gtype);
//        pspec = g_object_class_find_property(G_OBJECT_CLASS(class_type), name);
//        g_type_class_unref(class_type);
//    }
//
//    if (pspec == NULL) {
//        gwkjs_throw(cx, "No such property '%s' to override on type '%s'", name,
//                  g_type_name(gtype));
//        g_clear_pointer(&name, g_free);
//        return JS_FALSE;
//    }
//
//    new_pspec = g_param_spec_override(name, pspec);
//    g_clear_pointer(&name, g_free);
//
//    g_param_spec_set_qdata(new_pspec, gwkjs_is_custom_property_quark(), GINT_TO_POINTER(1));
//
//    args.rval().setObject(*gwkjs_param_from_g_param(cx, new_pspec));
//    g_param_spec_unref(new_pspec);
//
//    return JS_TRUE;
//}
//
//static void
//gwkjs_interface_init(GTypeInterface *g_iface,
//                   gpointer        iface_data)
//{
//    GPtrArray *properties;
//    GType gtype;
//    guint i;
//
//    gtype = G_TYPE_FROM_INTERFACE (g_iface);
//
//    properties = (GPtrArray *) gwkjs_hash_table_for_gsize_lookup(class_init_properties, gtype);
//    if (properties == NULL)
//        return;
//
//    for (i = 0; i < properties->len; i++) {
//        GParamSpec *pspec = (GParamSpec *) properties->pdata[i];
//        g_param_spec_set_qdata(pspec, gwkjs_is_custom_property_quark(), GINT_TO_POINTER(1));
//        g_object_interface_install_property(g_iface, pspec);
//    }
//
//    gwkjs_hash_table_for_gsize_remove(class_init_properties, gtype);
//}
//
//static void
//gwkjs_object_class_init(GObjectClass *klass,
//                      gpointer      user_data)
//{
//    GPtrArray *properties;
//    GType gtype;
//    guint i;
//
//    gtype = G_OBJECT_CLASS_TYPE (klass);
//
//    klass->constructor = gwkjs_object_constructor;
//    klass->set_property = gwkjs_object_set_gproperty;
//    klass->get_property = gwkjs_object_get_gproperty;
//
//    gwkjs_eval_thread = g_thread_self();
//
//    properties = (GPtrArray*) gwkjs_hash_table_for_gsize_lookup (class_init_properties, gtype);
//    if (properties != NULL) {
//        for (i = 0; i < properties->len; i++) {
//            GParamSpec *pspec = (GParamSpec*) properties->pdata[i];
//            g_param_spec_set_qdata(pspec, gwkjs_is_custom_property_quark(), GINT_TO_POINTER(1));
//            g_object_class_install_property (klass, i+1, pspec);
//        }
//        
//        gwkjs_hash_table_for_gsize_remove (class_init_properties, gtype);
//    }
//}
//
//static void
//gwkjs_object_custom_init(GTypeInstance *instance,
//                       gpointer       klass)
//{
//    GwkjsContext *gwkjs_context;
//    JSContextRef context;
//    JSObjectRef object;
//    ObjectInstance *priv;
//    jsval v, r;
//
//    if (!object_init_list)
//      return;
//
//    object = (JSObject*) object_init_list->data;
//    priv = (ObjectInstance*) JS_GetPrivate(object);
//
//    if (priv->gtype != G_TYPE_FROM_INSTANCE (instance)) {
//        /* This is not the most derived instance_init function,
//           do nothing.
//         */
//        return;
//    }
//
//    object_init_list = g_slist_delete_link(object_init_list,
//                                           object_init_list);
//
//    gwkjs_context = gwkjs_context_get_current();
//    context = (JSContext*) gwkjs_context_get_native_context(gwkjs_context);
//
//    associate_js_gobject(context, object, G_OBJECT (instance));
//
//    if (!gwkjs_object_get_property_const(context, object,
//                                       GWKJS_STRING_INSTANCE_INIT, &v)) {
//        gwkjs_log_exception(context);
//        return;
//    }
//
//    if (!JSVAL_IS_OBJECT(v) || JSVAL_IS_NULL(v))
//        return;
//
//    if (!JS_CallFunctionValue(context, object, v,
//                              0, NULL, &r))
//        gwkjs_log_exception(context);
//}
//
//static inline void
//gwkjs_add_interface(GType instance_type,
//                  GType interface_type)
//{
//    static GInterfaceInfo interface_vtable = { NULL, NULL, NULL };
//
//    g_type_add_interface_static(instance_type,
//                                interface_type,
//                                &interface_vtable);
//}
//
//static gboolean
//validate_interfaces_and_properties_args(JSContextRef cx,
//                                        JSObjectRef  interfaces,
//                                        JSObjectRef  properties,
//                                        guint32   *n_interfaces,
//                                        guint32   *n_properties)
//{
//    guint32 n_int, n_prop;
//
//    if (!JS_IsArrayObject(cx, interfaces)) {
//        gwkjs_throw(cx, "Invalid parameter interfaces (expected Array)");
//        return FALSE;
//    }
//
//    if (!JS_GetArrayLength(cx, interfaces, &n_int))
//        return FALSE;
//
//    if (!JS_IsArrayObject(cx, properties)) {
//        gwkjs_throw(cx, "Invalid parameter properties (expected Array)");
//        return FALSE;
//    }
//
//    if (!JS_GetArrayLength(cx, properties, &n_prop))
//        return FALSE;
//
//    if (n_interfaces != NULL)
//        *n_interfaces = n_int;
//    if (n_properties != NULL)
//        *n_properties = n_prop;
//    return TRUE;
//}
//
//static gboolean
//get_interface_gtypes(JSContextRef cx,
//                     JSObjectRef  interfaces,
//                     guint32   n_interfaces,
//                     GType    *iface_types)
//{
//    guint32 i;
//
//    for (i = 0; i < n_interfaces; i++) {
//        JS::RootedValue iface_val(cx);
//        GType iface_type;
//
//        if (!JS_GetElement(cx, interfaces, i, &iface_val.get()))
//            return FALSE;
//
//        if (!iface_val.isObject()) {
//            gwkjs_throw (cx, "Invalid parameter interfaces (element %d was not a GType)", i);
//            return FALSE;
//        }
//
//        iface_type = gwkjs_gtype_get_actual_gtype(cx, &iface_val.toObject());
//        if (iface_type == G_TYPE_INVALID) {
//            gwkjs_throw (cx, "Invalid parameter interfaces (element %d was not a GType)", i);
//            return FALSE;
//        }
//
//        iface_types[i] = iface_type;
//    }
//    return TRUE;
//}
//
//static gboolean
//save_properties_for_class_init(JSContextRef cx,
//                               JSObjectRef  properties,
//                               guint32    n_properties,
//                               GType      gtype)
//{
//    GPtrArray *properties_native = NULL;
//    guint32 i;
//
//    if (!class_init_properties)
//        class_init_properties = gwkjs_hash_table_new_for_gsize((GDestroyNotify) g_ptr_array_unref);
//    properties_native = g_ptr_array_new_with_free_func((GDestroyNotify) g_param_spec_unref);
//    for (i = 0; i < n_properties; i++) {
//        JS::RootedValue prop_val(cx);
//
//        if (!JS_GetElement(cx, properties, i, &prop_val.get())) {
//            g_clear_pointer(&properties_native, g_ptr_array_unref);
//            return FALSE;
//        }
//        if (!prop_val.isObject()) {
//            g_clear_pointer(&properties_native, g_ptr_array_unref);
//            gwkjs_throw(cx, "Invalid parameter, expected object");
//            return FALSE;
//        }
//
//        JS::RootedObject prop_obj(cx, &prop_val.toObject());
//        if (!gwkjs_typecheck_param(cx, prop_obj, G_TYPE_NONE, JS_TRUE)) {
//            g_clear_pointer(&properties_native, g_ptr_array_unref);
//            return FALSE;
//        }
//        g_ptr_array_add(properties_native, g_param_spec_ref(gwkjs_g_param_from_param(cx, prop_obj)));
//    }
//    gwkjs_hash_table_for_gsize_insert(class_init_properties, (gsize) gtype,
//                                    g_ptr_array_ref(properties_native));
//
//    g_clear_pointer(&properties_native, g_ptr_array_unref);
//    return TRUE;
//}
//
//static JSBool
//gwkjs_register_interface(JSContextRef cx,
//                       unsigned   argc,
//                       jsval     *vp)
//{
//    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
//    char *name = NULL;
//    JSObjectRef constructor, *interfaces, *properties, *module;
//    guint32 i, n_interfaces, n_properties;
//    GType *iface_types;
//    GType interface_type;
//    GTypeModule *type_module;
//    GTypeInfo type_info = {
//        sizeof (GTypeInterface), /* class_size */
//
//        (GBaseInitFunc) NULL,
//        (GBaseFinalizeFunc) NULL,
//
//        (GClassInitFunc) gwkjs_interface_init,
//        (GClassFinalizeFunc) NULL,
//        NULL, /* class_data */
//
//        0,    /* instance_size */
//        0,    /* n_preallocs */
//        NULL, /* instance_init */
//    };
//
//    if (!gwkjs_parse_call_args(cx, "register_interface", "soo", args,
//                             "name", &name,
//                             "interfaces", &interfaces,
//                             "properties", &properties))
//        return JS_FALSE;
//
//    if (!validate_interfaces_and_properties_args(cx, interfaces, properties,
//                                                 &n_interfaces, &n_properties)) {
//        g_clear_pointer(&name, g_free);
//        return JS_FALSE;
//    }
//
//    iface_types = (GType *) g_alloca(sizeof(GType) * n_interfaces);
//
//    /* We do interface addition in two passes so that any failure
//       is caught early, before registering the GType (which we can't undo) */
//    if (!get_interface_gtypes(cx, interfaces, n_interfaces, iface_types)) {
//        g_clear_pointer(&name, g_free);
//        return JS_FALSE;
//    }
//
//    if (g_type_from_name(name) != G_TYPE_INVALID) {
//        gwkjs_throw(cx, "Type name %s is already registered", name);
//        g_clear_pointer(&name, g_free);
//        return JS_FALSE;
//    }
//
//    type_module = G_TYPE_MODULE(gwkjs_type_module_get());
//    interface_type = g_type_module_register_type(type_module,
//                                                 G_TYPE_INTERFACE,
//                                                 name,
//                                                 &type_info,
//                                                 (GTypeFlags) 0);
//    g_clear_pointer(&name, g_free);
//
//    g_type_set_qdata(interface_type, gwkjs_is_custom_type_quark(), GINT_TO_POINTER(1));
//
//    if (!save_properties_for_class_init(cx, properties, n_properties, interface_type))
//        return JS_FALSE;
//
//    for (i = 0; i < n_interfaces; i++)
//        g_type_interface_add_prerequisite(interface_type, iface_types[i]);
//
//    /* create a custom JSClass */
//    if ((module = gwkjs_lookup_private_namespace(cx)) == NULL)
//        return JS_FALSE;  /* error will have been thrown already */
//    gwkjs_define_interface_class(cx, module, NULL, interface_type, &constructor);
//
//    args.rval().setObject(*constructor);
//    return JS_TRUE;
//}
//
//static JSBool
//gwkjs_register_type(JSContextRef cx,
//                  unsigned   argc,
//                  jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    gchar *name;
//    JSObjectRef parent, *constructor, *interfaces, *properties, *module;
//    GType instance_type, parent_type;
//    GTypeQuery query;
//    GTypeModule *type_module;
//    ObjectInstance *parent_priv;
//    GTypeInfo type_info = {
//        0, /* class_size */
//
//	(GBaseInitFunc) NULL,
//	(GBaseFinalizeFunc) NULL,
//
//	(GClassInitFunc) gwkjs_object_class_init,
//	(GClassFinalizeFunc) NULL,
//	NULL, /* class_data */
//
//	0,    /* instance_size */
//	0,    /* n_preallocs */
//	gwkjs_object_custom_init,
//    };
//    guint32 i, n_interfaces, n_properties;
//    GType *iface_types;
//    JSBool retval = JS_FALSE;
//
//    JS_BeginRequest(cx);
//
//    if (!gwkjs_parse_call_args(cx, "register_type",
//                        "osoo", argv,
//                        "parent", &parent,
//                        "name", &name,
//                        "interfaces", &interfaces,
//                        "properties", &properties))
//        goto out;
//
//    if (!parent)
//        goto out;
//
//    if (!do_base_typecheck(cx, parent, JS_TRUE))
//        goto out;
//
//    if (!validate_interfaces_and_properties_args(cx, interfaces, properties,
//                                                 &n_interfaces, &n_properties))
//        goto out;
//
//    iface_types = (GType*) g_alloca(sizeof(GType) * n_interfaces);
//
//    /* We do interface addition in two passes so that any failure
//       is caught early, before registering the GType (which we can't undo) */
//    if (!get_interface_gtypes(cx, interfaces, n_interfaces, iface_types))
//        goto out;
//
//    if (g_type_from_name(name) != G_TYPE_INVALID) {
//        gwkjs_throw (cx, "Type name %s is already registered", name);
//        goto out;
//    }
//
//    parent_priv = priv_from_js(cx, parent);
//
//    /* We checked parent above, in do_base_typecheck() */
//    g_assert(parent_priv != NULL);
//
//    parent_type = parent_priv->gtype;
//
//    g_type_query_dynamic_safe(parent_type, &query);
//    if (G_UNLIKELY (query.type == 0)) {
//        gwkjs_throw (cx, "Cannot inherit from a non-gwkjs dynamic type [bug 687184]");
//        goto out;
//    }
//
//    type_info.class_size = query.class_size;
//    type_info.instance_size = query.instance_size;
//
//    type_module = G_TYPE_MODULE (gwkjs_type_module_get());
//    instance_type = g_type_module_register_type(type_module,
//                                                parent_type,
//                                                name,
//                                                &type_info,
//                                                (GTypeFlags) 0);
//
//    g_free(name);
//
//    g_type_set_qdata (instance_type, gwkjs_is_custom_type_quark(), GINT_TO_POINTER (1));
//
//    if (!save_properties_for_class_init(cx, properties, n_properties, instance_type))
//        goto out;
//
//    for (i = 0; i < n_interfaces; i++)
//        gwkjs_add_interface(instance_type, iface_types[i]);
//
//    /* create a custom JSClass */
//    module = gwkjs_lookup_private_namespace(cx);
//    gwkjs_define_object_class(cx, module, NULL, instance_type, &constructor);
//
//    argv.rval().set(OBJECT_TO_JSVAL(constructor));
//
//    retval = JS_TRUE;
//
//out:
//    JS_EndRequest(cx);
//
//    return retval;
//}
//
//static JSBool
//gwkjs_signal_new(JSContextRef cx,
//               unsigned   argc,
//               jsval     *vp)
//{
//    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);
//    JSObjectRef obj;
//    GType gtype;
//    gchar *signal_name = NULL;
//    GSignalAccumulator accumulator;
//    gint signal_id;
//    guint i, n_parameters;
//    GType *params, return_type;
//    JSBool ret;
//
//    if (argc != 6)
//        return JS_FALSE;
//
//    JS_BeginRequest(cx);
//
//    if (!gwkjs_string_to_utf8(cx, argv[1], &signal_name)) {
//        ret = JS_FALSE;
//        goto out;
//    }
//
//    obj = JSVAL_TO_OBJECT(argv[0]);
//    if (!gwkjs_typecheck_gtype(cx, obj, JS_TRUE))
//        return JS_FALSE;
//
//    /* we only support standard accumulators for now */
//    switch (JSVAL_TO_INT(argv[3])) {
//    case 1:
//        accumulator = g_signal_accumulator_first_wins;
//        break;
//    case 2:
//        accumulator = g_signal_accumulator_true_handled;
//        break;
//    case 0:
//    default:
//        accumulator = NULL;
//    }
//
//    return_type = gwkjs_gtype_get_actual_gtype(cx, JSVAL_TO_OBJECT(argv[4]));
//
//    if (accumulator == g_signal_accumulator_true_handled && return_type != G_TYPE_BOOLEAN) {
//        gwkjs_throw (cx, "GObject.SignalAccumulator.TRUE_HANDLED can only be used with boolean signals");
//        ret = JS_FALSE;
//        goto out;
//    }
//
//    if (!JS_GetArrayLength(cx, JSVAL_TO_OBJECT(argv[5]), &n_parameters)) {
//        ret = JS_FALSE;
//        goto out;
//    }
//    params = g_newa(GType, n_parameters);
//    for (i = 0; i < n_parameters; i++) {
//        jsval gtype_val;
//        if (!JS_GetElement(cx, JSVAL_TO_OBJECT(argv[5]), i, &gtype_val) ||
//            !JSVAL_IS_OBJECT(gtype_val)) {
//            gwkjs_throw(cx, "Invalid signal parameter number %d", i);
//            ret = JS_FALSE;
//            goto out;
//        }
//
//        params[i] = gwkjs_gtype_get_actual_gtype(cx, JSVAL_TO_OBJECT(gtype_val));
//    }
//
//    gtype = gwkjs_gtype_get_actual_gtype(cx, obj);
//
//    signal_id = g_signal_newv(signal_name,
//                              gtype,
//                              (GSignalFlags) JSVAL_TO_INT(argv[2]), /* signal_flags */
//                              NULL, /* class closure */
//                              accumulator,
//                              NULL, /* accu_data */
//                              g_cclosure_marshal_generic,
//                              return_type, /* return type */
//                              n_parameters,
//                              params);
//
//    argv.rval().set(INT_TO_JSVAL(signal_id));
//    ret = JS_TRUE;
//
// out:
//    JS_EndRequest(cx);
//
//    free (signal_name);
//    return ret;
//}
//
//static JSFunctionSpec module_funcs[] = {
//    { "override_property", JSOP_WRAPPER((JSNative) gwkjs_override_property), 2, GWKJS_MODULE_PROP_FLAGS },
//    { "register_interface", JSOP_WRAPPER((JSNative) gwkjs_register_interface), 3, GWKJS_MODULE_PROP_FLAGS },
//    { "register_type", JSOP_WRAPPER ((JSNative) gwkjs_register_type), 4, GWKJS_MODULE_PROP_FLAGS },
//    { "add_interface", JSOP_WRAPPER ((JSNative) gwkjs_add_interface), 2, GWKJS_MODULE_PROP_FLAGS },
//    { "hook_up_vfunc", JSOP_WRAPPER ((JSNative) gwkjs_hook_up_vfunc), 3, GWKJS_MODULE_PROP_FLAGS },
//    { "signal_new", JSOP_WRAPPER ((JSNative) gwkjs_signal_new), 6, GWKJS_MODULE_PROP_FLAGS },
//    { NULL },
//};
//
//JSBool
//gwkjs_define_private_gi_stuff(JSContextRef context,
//                            JSObjectRef *module_out)
//{
//    JSObjectRef module;
//
//    module = JS_NewObject (context, NULL, NULL, NULL);
//
//    if (!JS_DefineFunctions(context, module, &module_funcs[0]))
//        return JS_FALSE;
//
//    *module_out = module;
//
//    return JS_TRUE;
//}
//
JSBool
gwkjs_lookup_object_constructor(JSContextRef context,
                              GType      gtype,
                              jsval     *value_p)
{
    gwkjs_throw(context, "gwkjs_lookup_object_constructor  not implemented");
    return FALSE;
//TODO: implement
//    JSObjectRef constructor;
//    GIObjectInfo *object_info;
//
//    object_info = (GIObjectInfo*)g_irepository_find_by_gtype(NULL, gtype);
//
//    g_assert(object_info == NULL ||
//             g_base_info_get_type((GIBaseInfo*)object_info) ==
//             GI_INFO_TYPE_OBJECT);
//
//    constructor = gwkjs_lookup_object_constructor_from_info(context, object_info, gtype);
//
//    if (G_UNLIKELY (constructor == NULL))
//        return JS_FALSE;
//
//    if (object_info)
//        g_base_info_unref((GIBaseInfo*)object_info);
//
//    *value_p = OBJECT_TO_JSVAL(constructor);
//    return JS_TRUE;
}
