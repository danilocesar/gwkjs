/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008-2010  litl, LLC
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
#include <glib.h>

#include <gwkjs/gwkjs-module.h>
#include <gwkjs/importer.h>
#include <gwkjs/compat.h>

#include <gio/gio.h>

#include <string.h>
#include "exceptions.h"

#define MODULE_INIT_FILENAME "__init__.js"

static char **gwkjs_search_path = NULL;

typedef struct {
    gboolean is_root;
    GHashTable *modules;
} Importer;

//typedef struct {
//    GPtrArray *elements;
//    unsigned int index;
//} ImporterIterator;

extern JSClassDefinition gwkjs_importer_class;
static JSClassRef gwkjs_importer_class_ref = NULL;

GWKJS_DEFINE_PRIV_FROM_JS(Importer, gwkjs_importer_class)

static JSBool
define_meta_properties(JSContextRef context,
                       JSObjectRef  module_obj,
                       const char  *full_path,
                       const char  *module_name,
                       JSObjectRef parent)
{
    gboolean parent_is_module = FALSE;

    /* We define both __moduleName__ and __parentModule__ to null
     * on the root importer
     */
    // XXX: review this later if it's the same functionality
    parent_is_module = parent && JSValueIsObjectOfClass(context, parent, gwkjs_importer_class_ref);

    gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Defining parent %p of %p '%s' is mod %d",
              parent, module_obj, module_name ? module_name : "<root>", parent_is_module);

    JSValueRef exception = NULL;
    if (full_path != NULL) {
        gwkjs_object_set_property(context, module_obj,
                               "__file__",
                               JSValueMakeString(context, gwkjs_cstring_to_jsstring(full_path)),
                               /* don't set ENUMERATE since we wouldn't want to copy
                                * this symbol to any other object for example.
                                */
                               kJSPropertyAttributeReadOnly |
                               kJSPropertyAttributeDontEnum |
                               kJSPropertyAttributeDontDelete,
                               &exception);
        if (exception)
            return FALSE;
    }

    gwkjs_object_set_property(context, module_obj,
                               "__moduleName__",
                               JSValueMakeString(context, gwkjs_cstring_to_jsstring(module_name)),
                               /* don't set ENUMERATE since we wouldn't want to copy
                                * this symbol to any other object for example.
                                */
                               kJSPropertyAttributeReadOnly |
                               kJSPropertyAttributeDontEnum |
                               kJSPropertyAttributeDontDelete,
                               &exception);
    if (exception)
        return FALSE;


    gwkjs_object_set_property(context, module_obj,
                           "__parentModule__",
                           parent_is_module ? parent :  JSValueMakeUndefined(context),
                           /* don't set ENUMERATE since we wouldn't want to copy
                            * this symbol to any other object for example.
                            */
                           kJSPropertyAttributeReadOnly |
                           kJSPropertyAttributeDontEnum |
                           kJSPropertyAttributeDontDelete,
                           &exception);
    if (exception)
        return FALSE;

    return TRUE;
}

static JSValueRef
import_directory(JSContextRef context,
                 JSObjectRef  obj,
                 const char   *name,
                 const char   **full_paths)
{
    JSObjectRef importer = NULL;
    gwkjs_debug(GWKJS_DEBUG_IMPORTER,
              "Importing directory '%s'",
              name);

    /* We define a sub-importer that has only the given directories on
     * its search path. gwkjs_define_importer() exits if it fails, so
     * this always succeeds.
     */
    importer = gwkjs_define_importer(context, obj, name, full_paths, FALSE);

    return importer;
}

static JSBool
define_import(JSContextRef  context,
              JSObjectRef   obj,
              JSObjectRef   module_obj,
              const char *name)
{
    JSValueRef exception = NULL;
    gwkjs_object_set_property(context, obj, name, module_obj, kJSPropertyAttributeNone, &exception);
    if (exception) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "Failed to define '%s' in importer",
                  name);
        return JS_FALSE;
    }

    return JS_TRUE;
}

/* Make the property we set in define_import permament;
 * we do this after the import succesfully completes.
 */
static JSBool
seal_import(JSContextRef  context,
            JSObjectRef   obj,
            const char *name)
{
    JSValueRef exception = NULL ;

    if (!gwkjs_object_has_property(context, obj, name)) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "Failed to get attributes to seal '%s' in importer", name);
        return FALSE;
    }

    JSValueRef prop = gwkjs_object_get_property(context, obj, name, NULL);
    gwkjs_object_set_property(context, obj, name, prop, kJSPropertyAttributeDontDelete,
                              &exception);

    if (exception) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "Failed to set attributes to seal '%s' in importer",
                  name);
        return JS_FALSE;
    }

    return JS_TRUE;
}

/* An import failed. Delete the property pointing to the import
 * from the parent namespace. In complicated situations this might
 * not be sufficient to get us fully back to a sane state. If:
 *
 *  - We import module A
 *  - module A imports module B
 *  - module B imports module A, storing a reference to the current
 *    module A module object
 *  - module A subsequently throws an exception
 *
 * Then module B is left imported, but the imported module B has
 * a reference to the failed module A module object. To handle this
 * we could could try to track the entire "import operation" and
 * roll back *all* modifications made to the namespace objects.
 * It's not clear that the complexity would be worth the small gain
 * in robustness. (You can still come up with ways of defeating
 * the attempt to clean up.)
 */
static void
cancel_import(JSContextRef  context,
              JSObjectRef   obj,
              const char *name)
{
    gwkjs_debug(GWKJS_DEBUG_IMPORTER,
              "Cleaning up from failed import of '%s'",
              name);
    JSStringRef jsname = gwkjs_cstring_to_jsstring(name);
    if (!JSObjectDeleteProperty(context, obj, jsname, NULL)) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "Failed to delete '%s' in importer",
                  name);
    }
    JSStringRelease(jsname);
}

static JSValueRef
import_native_file(JSContextRef  context,
                   JSObjectRef   obj,
                   const char *name)
{
    JSObjectRef module_obj = NULL;
    JSValueRef exception = NULL;
    JSValueRef retval = NULL;

    gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Importing '%s'", name);

    if (!gwkjs_import_native_module(context, name, &module_obj))
        goto out;

    if (!define_meta_properties(context, module_obj, NULL, name, obj))
        goto out;

    // XXX: So, we can't set a property to retrieve it later. Creates an infinite recursion
    gwkjs_object_set_property(context, obj, name, module_obj, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exception);
    if (exception)
        goto out;

    retval = module_obj;

 out:
    return retval;
}

static JSObjectRef
create_module_object(JSContextRef context)
{
    return gwkjs_new_object(context, NULL, NULL, NULL);
}

static JSBool
import_file(JSContextRef  context,
            const char *name,
            GFile      *file,
            JSObjectRef   module_obj)
{
    JSBool ret = JS_FALSE;
    char *script = NULL;
    char *full_path = NULL;
    gsize script_len = 0;
    jsval script_retval;
    GError *error = NULL;

    if (!(g_file_load_contents(file, NULL, &script, &script_len, NULL, &error))) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY) &&
            !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY) &&
            !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            gwkjs_throw_g_error(context, error);
        else
            g_error_free(error);

        goto out;
    }

    g_assert(script != NULL);

    full_path = g_file_get_parse_name (file);

    if (!gwkjs_eval_with_scope(context, module_obj, script, script_len,
                             full_path, NULL, NULL))
        goto out;

    ret = JS_TRUE;

 out:
    g_free(script);
    g_free(full_path);
    return ret;
}

static JSObjectRef
load_module_init(JSContextRef  context,
                 JSObjectRef   in_object,
                 const char *full_path)
{
    JSObjectRef module_obj;
    JSValueRef exception = NULL;
    JSBool found;
    const gchar* module_init_name;
    GFile *file;

    /* First we check if js module has already been loaded  */
    module_init_name = gwkjs_context_get_const_string(context, GWKJS_STRING_MODULE_INIT);
    found = gwkjs_object_has_property(context, in_object, module_init_name);
    if (found) {
        jsval module_obj_val;

        module_obj_val = gwkjs_object_get_property(context, in_object, module_init_name, &exception);
        if (module_obj_val && !exception) {
            return JSValueToObject(context, module_obj_val, NULL);
        }
    }

    module_obj = create_module_object (context);
    file = g_file_new_for_commandline_arg(full_path);
    if (!import_file (context, "__init__", file, module_obj))
        goto out;

    gwkjs_object_set_property(context, in_object, module_init_name, module_obj,
                              kJSPropertyAttributeNone, &exception);
    if (exception) {
        gwkjs_throw(context, "Could't define __init__ for %s", full_path);
        goto out;
    }

 out:
    g_object_unref (file);
    return module_obj;
}

static void
load_module_elements(JSContextRef context,
                     JSObjectRef in_object,
                     JSPropertyNameAccumulatorRef accumulator,
                     const char *init_path) {
    JSObjectRef module_obj;
    JSObjectRef jsiter;

    module_obj = load_module_init(context, in_object, init_path);

    if (module_obj != NULL) {
        jsid idp;
        int i = 0;

        JSPropertyNameArrayRef props = JSObjectCopyPropertyNames(context, module_obj);
        if (props == NULL)
            return;

        while (i < JSPropertyNameArrayGetCount(props)) {
            JSStringRef name = JSPropertyNameArrayGetNameAtIndex(props, i);
            JSPropertyNameAccumulatorAddName(accumulator,  name);
            i++;
        }
    }
}

static JSValueRef
import_file_on_module(JSContextRef context,
                      JSObjectRef  obj,
                      const char *name,
                      GFile      *file)
{
    JSObjectRef module_obj;
    JSBool retval = JS_FALSE;
    char *full_path = NULL;

    module_obj = create_module_object (context);

    if (!define_import(context, obj, module_obj, name))
        goto out;

    if (!import_file(context, name, file, module_obj))
        goto out;

    full_path = g_file_get_parse_name (file);
    if (!define_meta_properties(context, module_obj, full_path, name, obj))
        goto out;

    if (!seal_import(context, obj, name))
        goto out;

    retval = JS_TRUE;

 out:
    if (!retval)
        cancel_import(context, obj, name);

    g_free (full_path);
    return module_obj;
}

static JSValueRef
do_import(JSContextRef context,
          JSObjectRef  obj,
          Importer   *priv,
          const char *name)
{
    char *filename;
    char *full_path;
    char *dirname = NULL;
    JSValueRef search_path_val = NULL;
    JSObjectRef search_path = NULL;
    JSObjectRef module_obj = NULL;
    guint32 search_path_len;
    guint32 i;
    JSValueRef result = NULL;
    GPtrArray *directories;
    const gchar * search_path_name;
    GFile *gfile;
    gboolean exists;

    search_path_name = gwkjs_context_get_const_string(context, GWKJS_STRING_SEARCH_PATH);
    if (!gwkjs_object_require_property(context, obj, "importer", search_path_name, &search_path_val)) {
        return result;
    }

    if (!JSValueIsObject(context, search_path_val)) {
        gwkjs_throw(context, "searchPath property on importer is not an object");
        return result;
    }

    if (!JSValueIsArray(context, search_path_val)) {
        gwkjs_throw(context, "searchPath property on importer is not an array");
        return result;
    }

    search_path = JSValueToObject(context, search_path_val, NULL);
    g_assert(search_path != NULL);

    if (!gwkjs_array_get_length(context, search_path, &search_path_len)) {
        gwkjs_throw(context, "searchPath array has no length");
        return result;
    }

    filename = g_strdup_printf("%s.js", name);
    full_path = NULL;
    directories = NULL;

    /* First try importing an internal module like byteArray */
    if (priv->is_root &&
        gwkjs_is_registered_native_module(context, obj, name) &&
        (result = import_native_file(context, obj, name))) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "successfully imported module '%s'", name);
        goto out;
    }

    for (i = 0; i < search_path_len; ++i) {
        JSValueRef elem = NULL;

        if (!gwkjs_array_get_element(context, search_path, i, &elem)) {
            /* this means there was an exception, while elem == JSVAL_VOID
             * means no element found
             */
            goto out;
        }

        if (JSVAL_IS_VOID(context, elem))
            continue;

        if (!JSVAL_IS_STRING(context, elem)) {
            gwkjs_throw(context, "importer searchPath contains non-string");
            goto out;
        }

        g_free(dirname);
        dirname = NULL;

        if (!(dirname == gwkjs_jsvalue_to_cstring(context, elem, NULL)))
            goto out; /* Error message already set */

        /* Ignore empty path elements */
        if (dirname[0] == '\0')
            continue;

        /* Try importing __init__.js and loading the symbol from it */
        if (full_path)
            g_free(full_path);
        full_path = g_build_filename(dirname, MODULE_INIT_FILENAME,
                                     NULL);

        module_obj = load_module_init(context, obj, full_path);
        if (module_obj != NULL) {
            JSValueRef obj_val = NULL;

            if ((obj_val = gwkjs_object_get_property(context,
                                                     module_obj,
                                                     name,
                                                     NULL))) {
                if (!JSVAL_IS_VOID(context, obj_val)) {
                    gwkjs_object_set_property(context, obj, name, obj_val, kJSPropertyAttributeNone, NULL);
                    result = obj_val;
                    goto out;
                }
            }
        }

        /* Second try importing a directory (a sub-importer) */
        if (full_path)
            g_free(full_path);
        full_path = g_build_filename(dirname, name,
                                     NULL);
        gfile = g_file_new_for_commandline_arg(full_path);

        if (g_file_query_file_type(gfile, (GFileQueryInfoFlags) 0, NULL) == G_FILE_TYPE_DIRECTORY) {
            gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                      "Adding directory '%s' to child importer '%s'",
                      full_path, name);
            if (directories == NULL) {
                directories = g_ptr_array_new();
            }
            g_ptr_array_add(directories, full_path);
            /* don't free it twice - pass ownership to ptr array */
            full_path = NULL;
        }

        g_object_unref(gfile);

        /* If we just added to directories, we know we don't need to
         * check for a file.  If we added to directories on an earlier
         * iteration, we want to ignore any files later in the
         * path. So, always skip the rest of the loop block if we have
         * directories.
         */
        if (directories != NULL) {
            continue;
        }

        /* Third, if it's not a directory, try importing a file */
        g_free(full_path);
        full_path = g_build_filename(dirname, filename,
                                     NULL);
        gfile = g_file_new_for_commandline_arg(full_path);
        exists = g_file_query_exists(gfile, NULL);

        if (!exists) {
            gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                      "JS import '%s' not found in %s",
                      name, dirname);

            g_object_unref(gfile);
            continue;
        }

        if ((result = import_file_on_module (context, obj, name, gfile))) {
            gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                      "successfully imported module '%s'", name);
        }

        g_object_unref(gfile);

        /* Don't keep searching path if we fail to load the file for
         * reasons other than it doesn't exist... i.e. broken files
         * block searching for nonbroken ones
         */
        goto out;
    }

    if (directories != NULL) {
        /* NULL-terminate the char** */
        g_ptr_array_add(directories, NULL);

        if ((result = import_directory(context, obj, name,
                             (const char**) directories->pdata))) {
            gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                      "successfully imported directory '%s'", name);
        }
    }

 out:
    if (directories != NULL) {
        char **str_array;

        /* NULL-terminate the char**
         * (maybe for a second time, but doesn't matter)
         */
        g_ptr_array_add(directories, NULL);

        str_array = (char**) directories->pdata;
        g_ptr_array_free(directories, FALSE);
        g_strfreev(str_array);
    }

    g_free(full_path);
    g_free(filename);
    g_free(dirname);

    if (!result) {
        /* If no exception occurred, the problem is just that we got to the
         * end of the path. Be sure an exception is set.
         */
        gwkjs_throw(context, "No JS module '%s' found in search path", name);
    }

    return result;
}
//
//static ImporterIterator *
//importer_iterator_new(void)
//{
//    ImporterIterator *iter;
//
//    iter = g_slice_new0(ImporterIterator);
//
//    iter->elements = g_ptr_array_new();
//    iter->index = 0;
//
//    return iter;
//}
//
//static void
//importer_iterator_free(ImporterIterator *iter)
//{
//    g_ptr_array_foreach(iter->elements, (GFunc)g_free, NULL);
//    g_ptr_array_free(iter->elements, TRUE);
//    g_slice_free(ImporterIterator, iter);
//}
//
///*
// * Like JSEnumerateOp, but enum provides contextual information as follows:
// *
// * JSENUMERATE_INIT: allocate private enum struct in state_p, return number
// * of elements in *id_p
// * JSENUMERATE_NEXT: return next property id in *id_p, and if no new property
// * free state_p and set to JSVAL_NULL
// * JSENUMERATE_DESTROY : destroy state_p
// *
// * Note that in a for ... in loop, this will be called first on the object,
// * then on its prototype.
// *
// */

static void
importer_new_enumerate(JSContextRef context,
                       JSObjectRef object,
                       JSPropertyNameAccumulatorRef propertyNames)
{
    Importer *priv = NULL;
    JSValueRef search_path_val = NULL;
    JSObjectRef search_path = NULL;
    guint32 search_path_len;
    guint32 i;

    priv = priv_from_js(object);
    if (!priv)
        return;

    const gchar*  search_path_name = gwkjs_context_get_const_string(context, GWKJS_STRING_SEARCH_PATH);
    if (!gwkjs_object_require_property(context, object, "importer", search_path_name, &search_path_val))
       return;

   if (!JSValueIsObject(context, search_path_val)) {
       gwkjs_throw(context, "searchPath property on importer is not an object");
       return;
   }

   if (!JSValueIsArray(context, search_path_val)) {
       gwkjs_throw(context, "searchPath property on importer is not an array");
       return;
   }
   search_path = JSValueToObject(context, search_path_val, NULL);

   if (!gwkjs_array_get_length(context, search_path, &search_path_len)) {
       gwkjs_throw(context, "searchPath array has no length");
       return;
   }

    for (i = 0; i < search_path_len; ++i) {
        char *dirname = NULL;
        char *init_path = NULL;
        const char *filename;
        JSValueRef elem;
        GDir *dir = NULL;

        if (!gwkjs_array_get_element(context, search_path, i, &elem)) {
            /* this means there was an exception, while elem == JSVAL_VOID
             * means no element found
             */
            return;
        }
        if (JSVAL_IS_VOID(context, elem))
            continue;

        if (!JSVAL_IS_STRING(context, elem)) {
            gwkjs_throw(context, "importer searchPath contains non-string");
            return;
        }

        if(!gwkjs_string_to_utf8(context, elem, &dirname)) {
            return;
        }

        init_path = g_build_filename(dirname, MODULE_INIT_FILENAME, NULL);

        load_module_elements(context, object, propertyNames, init_path);
        g_free(init_path);

        dir = g_dir_open(dirname, 0, NULL);
        if (!dir) {
            g_free(dirname);
            continue;
        }

        while ((filename = g_dir_read_name(dir))) {
            gchar *full_path;
            /* skip hidden files and directories (.svn, .git, ...) */
            if (filename[0] == '.')
                continue;

            /* skip module init file */
            if (strcmp(filename, MODULE_INIT_FILENAME) == 0)
                continue;

            full_path = g_build_filename(dirname, filename, NULL);

            if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                JSStringRef name = gwkjs_cstring_to_jsstring(filename);
                JSPropertyNameAccumulatorAddName(propertyNames, name);
            } else {
                if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX) ||
                    g_str_has_suffix(filename, ".js")) {
                    gchar *cname = g_strndup(filename, strlen(filename) - 3);
                    JSStringRef name = gwkjs_cstring_to_jsstring(cname);
                    JSPropertyNameAccumulatorAddName(propertyNames, name);
                    g_free(cname);
                }
            }
            g_free(full_path);
        }
        g_dir_close(dir);
        g_free(dirname);

    }
}

//static JSBool
//importer_new_enumerate(JSContextRef  context,
//                       JS::HandleObject object,
//                       JSIterateOp enum_op,
//                       JS::MutableHandleValue statep,
//                       JS::MutableHandleId idp)
//{
//    ImporterIterator *iter;
//
//    switch (enum_op) {
//    case JSENUMERATE_INIT_ALL:
//    case JSENUMERATE_INIT: {
//        Importer *priv;
//        JSObjectRef search_path;
//        jsval search_path_val;
//        guint32 search_path_len;
//        guint32 i;
//        jsid search_path_name;
//
//        statep.set(JSVAL_NULL);
//
//        idp.set(INT_TO_JSID(0));
//
//        priv = priv_from_js(context, object);
//
//        if (!priv)
//            /* we are enumerating the prototype properties */
//            return JS_TRUE;
//
//        search_path_name = gwkjs_context_get_const_string(context, GWKJS_STRING_SEARCH_PATH);
//        if (!gwkjs_object_require_property(context, object, "importer", search_path_name, &search_path_val))
//            return JS_FALSE;
//
//        if (!JSVAL_IS_OBJECT(search_path_val)) {
//            gwkjs_throw(context, "searchPath property on importer is not an object");
//            return JS_FALSE;
//        }
//
//        search_path = JSVAL_TO_OBJECT(search_path_val);
//
//        if (!JS_IsArrayObject(context, search_path)) {
//            gwkjs_throw(context, "searchPath property on importer is not an array");
//            return JS_FALSE;
//        }
//
//        if (!JS_GetArrayLength(context, search_path, &search_path_len)) {
//            gwkjs_throw(context, "searchPath array has no length");
//            return JS_FALSE;
//        }
//
//        iter = importer_iterator_new();
//
//        for (i = 0; i < search_path_len; ++i) {
//            char *dirname = NULL;
//            char *init_path;
//            const char *filename;
//            jsval elem;
//            GDir *dir = NULL;
//
//            elem = JSVAL_VOID;
//            if (!JS_GetElement(context, search_path, i, &elem)) {
//                /* this means there was an exception, while elem == JSVAL_VOID
//                 * means no element found
//                 */
//                importer_iterator_free(iter);
//                return JS_FALSE;
//            }
//
//            if (JSVAL_IS_VOID(elem))
//                continue;
//
//            if (!JSVAL_IS_STRING(elem)) {
//                gwkjs_throw(context, "importer searchPath contains non-string");
//                importer_iterator_free(iter);
//                return JS_FALSE;
//            }
//
//            if (!gwkjs_string_to_utf8(context, elem, &dirname)) {
//                importer_iterator_free(iter);
//                return JS_FALSE; /* Error message already set */
//            }
//
//            init_path = g_build_filename(dirname, MODULE_INIT_FILENAME,
//                                         NULL);
//
//            load_module_elements(context, object, iter, init_path);
//
//            g_free(init_path);
//
//            dir = g_dir_open(dirname, 0, NULL);
//
//            if (!dir) {
//                g_free(dirname);
//                continue;
//            }
//
//            while ((filename = g_dir_read_name(dir))) {
//                char *full_path;
//
//                /* skip hidden files and directories (.svn, .git, ...) */
//                if (filename[0] == '.')
//                    continue;
//
//                /* skip module init file */
//                if (strcmp(filename, MODULE_INIT_FILENAME) == 0)
//                    continue;
//
//                full_path = g_build_filename(dirname, filename, NULL);
//
//                if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
//                    g_ptr_array_add(iter->elements, g_strdup(filename));
//                } else {
//                    if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX) ||
//                        g_str_has_suffix(filename, ".js")) {
//                        g_ptr_array_add(iter->elements,
//                                        g_strndup(filename, strlen(filename) - 3));
//                    }
//                }
//
//                g_free(full_path);
//            }
//            g_dir_close(dir);
//
//            g_free(dirname);
//        }
//
//        statep.set(PRIVATE_TO_JSVAL(iter));
//
//        idp.set(INT_TO_JSID(iter->elements->len));
//
//        break;
//    }
//
//    case JSENUMERATE_NEXT: {
//        jsval element_val;
//
//        if (JSVAL_IS_NULL(statep)) /* Iterating prototype */
//            return JS_TRUE;
//
//        iter = (ImporterIterator*) JSVAL_TO_PRIVATE(statep);
//
//        if (iter->index < iter->elements->len) {
//            if (!gwkjs_string_from_utf8(context,
//                                         (const char*) g_ptr_array_index(iter->elements,
//                                                           iter->index++),
//                                         -1,
//                                         &element_val))
//                return JS_FALSE;
//
//            jsid id;
//            if (!JS_ValueToId(context, element_val, &id))
//                return JS_FALSE;
//            idp.set(id);
//
//            break;
//        }
//        /* else fall through to destroying the iterator */
//    }
//
//    case JSENUMERATE_DESTROY: {
//        if (!JSVAL_IS_NULL(statep)) {
//            iter = (ImporterIterator*) JSVAL_TO_PRIVATE(statep);
//
//            importer_iterator_free(iter);
//
//            statep.set(JSVAL_NULL);
//        }
//    }
//    }
//
//    return JS_TRUE;
//}
//
///*
// * Like JSResolveOp, but flags provide contextual information as follows:
// *
// *  JSRESOLVE_QUALIFIED   a qualified property id: obj.id or obj[id], not id
// *  JSRESOLVE_ASSIGNING   obj[id] is on the left-hand side of an assignment
// *  JSRESOLVE_DETECTING   'if (o.p)...' or similar detection opcode sequence
// *  JSRESOLVE_DECLARING   var, const, or function prolog declaration opcode
// *  JSRESOLVE_CLASSNAME   class name used when constructing
// *
// * The *objp out parameter, on success, should be null to indicate that id
// * was not resolved; and non-null, referring to obj or one of its prototypes,
// * if id was resolved.
// */

static JSValueRef
importer_get_property(JSContextRef ctx,
                      JSObjectRef object,
                      JSStringRef property_name,
                      JSValueRef* exception)
{
    JSValueRef ret = NULL;
    gchar *name = NULL;
    Importer *priv;

    const gchar *module_init_name = gwkjs_context_get_const_string(ctx, GWKJS_STRING_MODULE_INIT);
    name = gwkjs_jsstring_to_cstring(property_name);

    if (g_strcmp0(name, module_init_name) == 0) {
        ret = JSValueMakeUndefined(ctx);
        goto out;
    }

    /* let Object.prototype resolve these */
    if (strcmp(name, "valueOf") == 0 ||
        strcmp(name, "toString") == 0 ||
        strcmp(name, "toStringTag") == 0 ||
        strcmp(name, "searchPath") == 0 ||
        strcmp(name, "__filename__") == 0 ||
        strcmp(name, "__moduleName__") == 0 ||
        strcmp(name, "__parentModule__") == 0 ||
        strcmp(name, "__iterator__") == 0)
        goto out;

    priv = priv_from_js(object);
    if (priv == NULL) /* we are the prototype, or have the wrong class */
        goto out;

    // If the module was already included, the default proto system
    // should return the property, not this method.
    if (g_hash_table_contains(priv->modules, name))
        goto out;


    gwkjs_debug_jsprop(GWKJS_DEBUG_IMPORTER,
                     "Resolve prop '%s' hook obj %p priv %p",
                     name, (void *)object, priv);

    // We need to add the module BEFORE trying to import it.
    // Otherwise, there will be an infinite recursing in SET/GET methods.
    g_hash_table_replace(priv->modules, g_strdup(name), NULL);
    if ((ret = do_import(ctx, object, priv, name))) {
        g_warning("Module imported");
    } else {
        g_hash_table_remove(priv->modules, name);
    }

out:
    g_free(name);
    return ret;
}

GWKJS_NATIVE_CONSTRUCTOR_DEFINE_ABSTRACT(importer)


static void
importer_finalize(JSObjectRef obj)
{
    Importer *priv;

    priv = (Importer*) JSObjectGetPrivate(obj);
    gwkjs_debug_lifecycle(GWKJS_DEBUG_IMPORTER,
                        "finalize, obj %p priv %p", obj, priv);
    if (priv == NULL)
        return; /* we are the prototype, not a real instance */

    //TODO: should finalize modules?

    GWKJS_DEC_COUNTER(importer);
    g_slice_free(Importer, priv);
}

///* The bizarre thing about this vtable is that it applies to both
// * instances of the object, and to the prototype that instances of the
// * class have.
// */
//struct JSClass gwkjs_importer_class = {
//    "GwkjsFileImporter",
//    JSCLASS_HAS_PRIVATE |
//    JSCLASS_NEW_RESOLVE |
//    JSCLASS_NEW_ENUMERATE,
//    JS_PropertyStub,
//    JS_DeletePropertyStub,
//    JS_PropertyStub,
//    JS_StrictPropertyStub,
//    (JSEnumerateOp) importer_new_enumerate, /* needs cast since it's the new enumerate signature */
//    (JSResolveOp) importer_new_resolve, /* needs cast since it's the new resolve signature */
//    JS_ConvertStub,
//    importer_finalize,
//    JSCLASS_NO_OPTIONAL_MEMBERS
//};

JSClassDefinition gwkjs_importer_class = {
    0,                         //     Version
    kJSPropertyAttributeNone,  //     JSClassAttributes
    "GwkjsFileImporter",       //     const char* className;
    NULL,                      //     JSClassRef parentClass;
    NULL,                      //     const JSStaticValue*                staticValues;
    NULL,                      //     const JSStaticFunction*             staticFunctions;
    NULL,                      //     JSObjectInitializeCallback          initialize;
    importer_finalize,         //     JSObjectFinalizeCallback            finalize;
    NULL,                      //     JSObjectHasPropertyCallback         hasProperty;

    importer_get_property,     //TODO: is this really resolve?
                               //     JSObjectGetPropertyCallback         getProperty;

    NULL,                      //     JSObjectSetPropertyCallback         setProperty;
    NULL,                      //     JSObjectDeletePropertyCallback      deleteProperty;
    importer_new_enumerate,    //     JSObjectGetPropertyNamesCallback    getPropertyNames;
    NULL,                      //     JSObjectCallAsFunctionCallback      callAsFunction;
    gwkjs_importer_constructor,//     JSObjectCallAsConstructorCallback   callAsConstructor;
    NULL,                      //     JSObjectHasInstanceCallback         hasInstance;
    NULL,                      //     JSObjectConvertToTypeCallback       convertToType;
};

//
//JSPropertySpec gwkjs_importer_proto_props[] = {
//    { NULL }
//};
//
//JSFunctionSpec gwkjs_importer_proto_funcs[] = {
//    { NULL }
//};

static JSObjectRef
importer_new(JSContextRef context,
             gboolean   is_root)
{
    Importer *priv = NULL;
    JSObjectRef ret = NULL;
    JSObjectRef global = NULL;
    gboolean found = FALSE;
    JSObjectRef proto = NULL;
    JSValueRef exception = NULL;

    // XXX: NOT THREAD SAFE?
    if (!gwkjs_importer_class_ref) {
        gwkjs_importer_class_ref = JSClassCreate(&gwkjs_importer_class);
    }

    global = gwkjs_get_import_global(context);
    if (!(found = gwkjs_object_has_property(context,
                                            global,
                                            gwkjs_importer_class.className))) {
        proto = JSObjectMake(context,
                             gwkjs_importer_class_ref,
                             NULL);

        gwkjs_object_set_property(context, global,
                                  gwkjs_importer_class.className,
                                  proto,
                                  GWKJS_PROTO_PROP_FLAGS,
                                  &exception);

        if (exception)
           g_error("Can't init class %s", gwkjs_importer_class.className);

        gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Initialized class %s prototype %p",
                    gwkjs_importer_class.className, proto);
    } else {
        JSValueRef proto_val = gwkjs_object_get_property(context,
                                                         global,
                                                         gwkjs_importer_class.className,
                                                         &exception);

        if (exception || proto_val == NULL || !JSValueIsObject(context, proto_val))
            g_error("Can't get protoType for class %s", gwkjs_importer_class.className);

        proto = JSValueToObject(context, proto_val, NULL);
    }
    g_assert(proto != NULL);

    ret = JSObjectMake(context, gwkjs_importer_class_ref, NULL);

    if (ret == NULL)
        return ret;

    JSObjectSetPrototype(context, ret, proto);

    priv = g_slice_new0(Importer);
    priv->is_root = is_root;
    priv->modules = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    g_assert(priv_from_js(ret) == NULL);
    GWKJS_INC_COUNTER(importer);

    gwkjs_debug_lifecycle(GWKJS_DEBUG_IMPORTER,
                        "importer constructor, obj %p priv %p", ret, priv);
    JSObjectSetPrivate(ret, priv);

    return ret;


// TODO:  we might want to check the PROTOTYPE stuff later
//    JSObjectRef importer;
//    Importer *priv;
//    JSObjectRef global;
//    JSBool found;
//
//    global = gwkjs_get_import_global(context);
//
//    if (!JS_HasProperty(context, global, gwkjs_importer_class.name, &found))
//        g_error("HasProperty call failed creating importer class");
//
//    if (!found) {
//        JSObjectRef prototype;
//        prototype = JS_InitClass(context, global,
//                                 /* parent prototype JSObject* for
//                                  * prototype; NULL for
//                                  * Object.prototype
//                                  */
//                                 NULL,
//                                 &gwkjs_importer_class,
//                                 /* constructor or instances (NULL for
//                                  * none - just name the prototype like
//                                  * Math - rarely correct)
//                                  */
//                                 gwkjs_importer_constructor,
//                                 /* number of constructor args */
//                                 0,
//                                 /* props of prototype */
//                                 &gwkjs_importer_proto_props[0],
//                                 /* funcs of prototype */
//                                 &gwkjs_importer_proto_funcs[0],
//                                 /* props of constructor, MyConstructor.myprop */
//                                 NULL,
//                                 /* funcs of constructor, MyConstructor.myfunc() */
//                                 NULL);
//        if (prototype == NULL)
//            g_error("Can't init class %s", gwkjs_importer_class.name);
//
//        gwkjs_debug(GWKJS_DEBUG_IMPORTER, "Initialized class %s prototype %p",
//                  gwkjs_importer_class.name, prototype);
//    }
//
//    importer = JS_NewObject(context, &gwkjs_importer_class, NULL, global);
//    if (importer == NULL)
//        g_error("No memory to create importer importer");
//
//    priv = g_slice_new0(Importer);
//    priv->is_root = is_root;
//
//    GWKJS_INC_COUNTER(importer);
//
//    g_assert(priv_from_js(context, importer) == NULL);
//    JS_SetPrivate(importer, priv);
//
//    gwkjs_debug_lifecycle(GWKJS_DEBUG_IMPORTER,
//                        "importer constructor, obj %p priv %p", importer, priv);
//
//    return importer;
}

static G_CONST_RETURN char * G_CONST_RETURN *
gwkjs_get_search_path(void)
{
    char **search_path;

    /* not thread safe */

    if (!gwkjs_search_path) {
        G_CONST_RETURN gchar* G_CONST_RETURN * system_data_dirs;
        const char *envstr;
        GPtrArray *path;
        gsize i;

        path = g_ptr_array_new();

        /* in order of priority */

        /* $GWKJS_PATH */
        envstr = g_getenv("GWKJS_PATH");
        if (envstr) {
            char **dirs, **d;
            dirs = g_strsplit(envstr, G_SEARCHPATH_SEPARATOR_S, 0);
            for (d = dirs; *d != NULL; d++)
                g_ptr_array_add(path, *d);
            /* we assume the array and strings are allocated separately */
            g_free(dirs);
        }

        g_ptr_array_add(path, g_strdup("resource:///org/gnome/gwkjs/modules/"));

        /* $XDG_DATA_DIRS /gwkjs-1.0 */
        system_data_dirs = g_get_system_data_dirs();
        for (i = 0; system_data_dirs[i] != NULL; ++i) {
            char *s;
            s = g_build_filename(system_data_dirs[i], "gwkjs-1.0", NULL);
            g_ptr_array_add(path, s);
        }

        /* ${datadir}/share/gwkjs-1.0 */
        g_ptr_array_add(path, g_strdup(GWKJS_JS_DIR));

        g_ptr_array_add(path, NULL);

        search_path = (char**)g_ptr_array_free(path, FALSE);

        gwkjs_search_path = search_path;
    } else {
        search_path = gwkjs_search_path;
    }

    return (G_CONST_RETURN char * G_CONST_RETURN *)search_path;
}

static JSObjectRef
gwkjs_create_importer(JSContextRef context,
                    const char   *importer_name,
                    const char  **initial_search_path,
                    gboolean      add_standard_search_path,
                    gboolean      is_root,
                    JSObjectRef   in_object)
{
    JSObjectRef importer;
    char **paths[2] = {0};
    char **search_path;

    paths[0] = (char**)initial_search_path;
    if (add_standard_search_path) {
        /* Stick the "standard" shared search path after the provided one. */
        paths[1] = (char**)gwkjs_get_search_path();
    }

    search_path = gwkjs_g_strv_concat(paths, 2);

    importer = importer_new(context, is_root);

    /* API users can replace this property from JS, is the idea */
    if (!gwkjs_define_string_array(context, importer,
                                 "searchPath", -1, (const char **)search_path,
                                 /* settable (no READONLY) but not deleteable (PERMANENT) */
                                 kJSPropertyAttributeDontDelete, NULL))
        g_error("no memory to define importer search path prop");

    g_strfreev(search_path);

    if (!define_meta_properties(context, importer, NULL, importer_name, in_object))
        g_error("failed to define meta properties on importer");

    return importer;
}

JSObjectRef
gwkjs_define_importer(JSContextRef  context,
                    JSObjectRef     in_object,
                    const char      *importer_name,
                    const char      **initial_search_path,
                    gboolean        add_standard_search_path)

{
    JSObjectRef importer;
    JSValueRef exception = NULL;

    importer = gwkjs_create_importer(context, importer_name, initial_search_path, add_standard_search_path, FALSE, in_object);

    if(importer == NULL) {
        g_warning("No importer");
        return NULL;
    }

    gwkjs_object_set_property(context, in_object, importer_name, importer, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum, &exception);

    if (exception == NULL) {
        g_error("Problems to define importer property");
    }

    gwkjs_debug(GWKJS_DEBUG_IMPORTER,
              "Defined importer '%s' %p in %p", importer_name, importer, in_object);

    return importer;
}

/* If this were called twice for the same runtime with different args it
 * would basically be a bug, but checking for that is a lot of code so
 * we just ignore all calls after the first and hope the args are the same.
 */
JSObjectRef
gwkjs_create_root_importer(GwkjsContext   *c,
                           const char **initial_search_path,
                           gboolean     add_standard_search_path)
{
    JSContextRef context = (JSContextRef) gwkjs_context_get_native_context(c);
    JSObjectRef global = JSContextGetGlobalObject(context);
    JSValueRef importer_v  = gwkjs_object_get_property(context, global,
                                                     "imports", NULL);

    if (G_UNLIKELY (!JSValueIsUndefined(context, importer_v) &&
                    !JSValueIsNull(context, importer_v))) {
        gwkjs_debug(GWKJS_DEBUG_IMPORTER,
                  "Someone else already created root importer, ignoring second request");

        // TODO: we might want to return here. As double imports
        // wouldn'y fail, would only be ignored.
        return NULL;
    }

    JSObjectRef importer = gwkjs_create_importer(context, "imports",
                                     initial_search_path,
                                     add_standard_search_path,
                                     TRUE, NULL);

    gwkjs_set_global_slot(context, GWKJS_GLOBAL_SLOT_IMPORTS, importer);

    return importer;
}

JSBool
gwkjs_define_root_importer_object(JSContextRef     context,
                                  JSObjectRef  in_object,
                                  JSValueRef  root_importer)
{
    JSBool success = FALSE;
    JSValueRef exception = NULL;

    // Do we really want a read only attr?
    gwkjs_object_set_property(context, in_object, "imports",
                              root_importer,
                              kJSPropertyAttributeDontDelete |
                              kJSPropertyAttributeReadOnly, NULL);

    if (exception) {
        gchar *msg = gwkjs_exception_to_string(context, exception);
        gwkjs_debug(GWKJS_DEBUG_IMPORTER, "DefineProperty imports on %p failed with: %s",
                    in_object, msg);
        g_free(msg);
        goto fail;
    }

    success = JS_TRUE;
 fail:
    return success;
}

JSBool
gwkjs_define_root_importer(JSContextRef   context,
                           JSObjectRef in_object,
                           JSValueRef importer)
{
    return gwkjs_define_root_importer_object(context,
                                             in_object,
                                             importer);
}
