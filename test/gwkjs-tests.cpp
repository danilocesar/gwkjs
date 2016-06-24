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
#include <glib.h>
#include <glib-object.h>
#include <gwkjs/gwkjs-module.h>
#include <util/glib.h>
#include <util/crash.h>

#include "gwkjs-tests-add-funcs.h"

typedef struct _GwkjsUnitTestFixture GwkjsUnitTestFixture;

struct _GwkjsUnitTestFixture {
    JSContext *context;
    GwkjsContext *gwkjs_context;
};

static void
test_error_reporter(JSContext     *context,
                    const char    *message,
                    JSErrorReport *report)
{
    g_printerr("error reported by test: %s\n", message);
}

static void
_gwkjs_unit_test_fixture_begin (GwkjsUnitTestFixture *fixture)
{
    fixture->gwkjs_context = gwkjs_context_new ();
    fixture->context = (JSContext *) gwkjs_context_get_native_context (fixture->gwkjs_context);
    JS_BeginRequest(fixture->context);
    JS_SetErrorReporter(fixture->context, test_error_reporter);
}

static void
_gwkjs_unit_test_fixture_finish (GwkjsUnitTestFixture *fixture)
{
    JS_EndRequest(fixture->context);
    g_object_unref(fixture->gwkjs_context);
}

static void
gwkjstest_test_func_gwkjs_context_construct_destroy(void)
{
    GwkjsContext *context;

    /* Construct twice just to possibly a case where global state from
     * the first leaks.
     */
    context = gwkjs_context_new ();
    g_object_unref (context);

    context = gwkjs_context_new ();
    g_object_unref (context);
}

static void
gwkjstest_test_func_gwkjs_context_construct_eval(void)
{
    GwkjsContext *context;
    int estatus;
    GError *error = NULL;

    context = gwkjs_context_new ();
    if (!gwkjs_context_eval (context, "1+1", -1, "<input>", &estatus, &error))
        g_error ("%s", error->message);
    g_object_unref (context);
}

#define N_ELEMS 15

static void
gwkjstest_test_func_gwkjs_jsapi_util_array(void)
{
    GwkjsUnitTestFixture fixture;
    JSContext *context;
    JSObject *global;
    GwkjsRootedArray *array;
    int i;
    jsval value;

    _gwkjs_unit_test_fixture_begin(&fixture);
    context = fixture.context;

    array = gwkjs_rooted_array_new();

    global = JS_GetGlobalObject(context);
    JSCompartment *oldCompartment = JS_EnterCompartment(context, global);

    for (i = 0; i < N_ELEMS; i++) {
        value = STRING_TO_JSVAL(JS_NewStringCopyZ(context, "abcdefghijk"));
        gwkjs_rooted_array_append(context, array, value);
    }

    JS_GC(JS_GetRuntime(context));

    for (i = 0; i < N_ELEMS; i++) {
        char *ascii;

        value = gwkjs_rooted_array_get(context, array, i);
        g_assert(JSVAL_IS_STRING(value));
        gwkjs_string_to_utf8(context, value, &ascii);
        g_assert(strcmp(ascii, "abcdefghijk") == 0);
        g_free(ascii);
    }

    gwkjs_rooted_array_free(context, array, TRUE);

    JS_LeaveCompartment(context, oldCompartment);
    _gwkjs_unit_test_fixture_finish(&fixture);
}

#undef N_ELEMS

static void
gwkjstest_test_func_gwkjs_jsapi_util_string_js_string_utf8(void)
{
    GwkjsUnitTestFixture fixture;
    JSContext *context;
    JSObject *global;

    const char *utf8_string = "\303\211\303\226 foobar \343\203\237";
    char *utf8_result;
    jsval js_string;

    _gwkjs_unit_test_fixture_begin(&fixture);
    context = fixture.context;
    global = JS_GetGlobalObject(context);
    JSCompartment *oldCompartment = JS_EnterCompartment(context, global);

    g_assert(gwkjs_string_from_utf8(context, utf8_string, -1, &js_string) == JS_TRUE);
    g_assert(JSVAL_IS_STRING(js_string));
    g_assert(gwkjs_string_to_utf8(context, js_string, &utf8_result) == JS_TRUE);

    JS_LeaveCompartment(context, oldCompartment);
    _gwkjs_unit_test_fixture_finish(&fixture);

    g_assert(g_str_equal(utf8_string, utf8_result));

    g_free(utf8_result);
}

static void
gwkjstest_test_func_gwkjs_jsapi_util_error_throw(void)
{
    GwkjsUnitTestFixture fixture;
    JSContext *context;
    JSObject *global;
    jsval exc, value, previous;
    char *s = NULL;
    int strcmp_result;

    _gwkjs_unit_test_fixture_begin(&fixture);
    context = fixture.context;
    global = JS_GetGlobalObject(context);
    JSCompartment *oldCompartment = JS_EnterCompartment(context, global);
    /* Test that we can throw */

    gwkjs_throw(context, "This is an exception %d", 42);

    g_assert(JS_IsExceptionPending(context));

    exc = JSVAL_VOID;
    JS_GetPendingException(context, &exc);
    g_assert(!JSVAL_IS_VOID(exc));

    value = JSVAL_VOID;
    JS_GetProperty(context, JSVAL_TO_OBJECT(exc), "message",
                   &value);

    g_assert(JSVAL_IS_STRING(value));

    gwkjs_string_to_utf8(context, value, &s);
    g_assert(s != NULL);
    strcmp_result = strcmp(s, "This is an exception 42");
    free(s);
    if (strcmp_result != 0)
        g_error("Exception has wrong message '%s'", s);

    /* keep this around before we clear it */
    previous = exc;
    JS_AddValueRoot(context, &previous);

    JS_ClearPendingException(context);

    g_assert(!JS_IsExceptionPending(context));

    /* Check that we don't overwrite a pending exception */
    JS_SetPendingException(context, previous);

    g_assert(JS_IsExceptionPending(context));

    gwkjs_throw(context, "Second different exception %s", "foo");

    g_assert(JS_IsExceptionPending(context));

    exc = JSVAL_VOID;
    JS_GetPendingException(context, &exc);
    g_assert(!JSVAL_IS_VOID(exc));
    g_assert(JSVAL_TO_OBJECT(exc) == JSVAL_TO_OBJECT(previous));

    JS_RemoveValueRoot(context, &previous);

    JS_LeaveCompartment(context, oldCompartment);
    _gwkjs_unit_test_fixture_finish(&fixture);
}

static void
gwkjstest_test_func_util_glib_strv_concat_null(void)
{
    char **ret;

    ret = gwkjs_g_strv_concat(NULL, 0);
    g_assert(ret != NULL);
    g_assert(ret[0] == NULL);

    g_strfreev(ret);
}

static void
gwkjstest_test_func_util_glib_strv_concat_pointers(void)
{
    char  *strv0[2] = {(char*)"foo", NULL};
    char  *strv1[1] = {NULL};
    char **strv2    = NULL;
    char  *strv3[2] = {(char*)"bar", NULL};
    char **stuff[4];
    char **ret;

    stuff[0] = strv0;
    stuff[1] = strv1;
    stuff[2] = strv2;
    stuff[3] = strv3;

    ret = gwkjs_g_strv_concat(stuff, 4);
    g_assert(ret != NULL);
    g_assert_cmpstr(ret[0], ==, strv0[0]);  /* same string */
    g_assert(ret[0] != strv0[0]);           /* different pointer */
    g_assert_cmpstr(ret[1], ==, strv3[0]);
    g_assert(ret[1] != strv3[0]);
    g_assert(ret[2] == NULL);

    g_strfreev(ret);
}

static void
gwkjstest_test_strip_shebang_no_advance_for_no_shebang(void)
{
    const char *script = "foo\nbar";
    gssize     script_len_original = strlen(script);
    gssize     script_len = script_len_original;
    int        line_number = 1;

    const char *stripped = gwkjs_strip_unix_shebang(script,
                                                  &script_len,
                                                  &line_number);

    g_assert_cmpstr(script, ==, stripped);
    g_assert(script_len == script_len_original);
    g_assert(line_number == 1);
}

static void
gwkjstest_test_strip_shebang_advance_for_shebang(void)
{
    const char *script = "#!foo\nbar";
    gssize     script_len_original = strlen(script);
    gssize     script_len = script_len_original;
    int        line_number = 1;

    const char *stripped = gwkjs_strip_unix_shebang(script,
                                                  &script_len,
                                                  &line_number);

    g_assert_cmpstr(stripped, ==, "bar");
    g_assert(script_len == 3);
    g_assert(line_number == 2);
}

static void
gwkjstest_test_strip_shebang_return_null_for_just_shebang(void)
{
    const char *script = "#!foo";
    gssize     script_len_original = strlen(script);
    gssize     script_len = script_len_original;
    int        line_number = 1;

    const char *stripped = gwkjs_strip_unix_shebang(script,
                                                  &script_len,
                                                  &line_number);

    g_assert(stripped == NULL);
    g_assert(script_len == 0);
    g_assert(line_number == -1);
}

int
main(int    argc,
     char **argv)
{
    gwkjs_crash_after_timeout(60*7); /* give the unit tests 7 minutes to complete */

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/gwkjs/context/construct/destroy", gwkjstest_test_func_gwkjs_context_construct_destroy);
    g_test_add_func("/gwkjs/context/construct/eval", gwkjstest_test_func_gwkjs_context_construct_eval);
    g_test_add_func("/gwkjs/jsapi/util/array", gwkjstest_test_func_gwkjs_jsapi_util_array);
    g_test_add_func("/gwkjs/jsapi/util/error/throw", gwkjstest_test_func_gwkjs_jsapi_util_error_throw);
    g_test_add_func("/gwkjs/jsapi/util/string/js/string/utf8", gwkjstest_test_func_gwkjs_jsapi_util_string_js_string_utf8);
    g_test_add_func("/gwkjs/jsutil/strip_shebang/no_shebang", gwkjstest_test_strip_shebang_no_advance_for_no_shebang);
    g_test_add_func("/gwkjs/jsutil/strip_shebang/have_shebang", gwkjstest_test_strip_shebang_advance_for_shebang);
    g_test_add_func("/gwkjs/jsutil/strip_shebang/only_shebang", gwkjstest_test_strip_shebang_return_null_for_just_shebang);
    g_test_add_func("/util/glib/strv/concat/null", gwkjstest_test_func_util_glib_strv_concat_null);
    g_test_add_func("/util/glib/strv/concat/pointers", gwkjstest_test_func_util_glib_strv_concat_pointers);

    gwkjs_test_add_tests_for_coverage ();

    g_test_run();

    return 0;
}
