/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBREADLINE
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <gwkjs/gwkjs-module.h>
#include <gwkjs/exceptions.h>
#include <gwkjs/compat.h>
#include <gwkjs/jsapi-private.h>

#include "console.h"

// TODO: implement
//static void
//gwkjs_console_error_reporter(JSContextRef cx, const char *message, JSErrorReport *report)
//{
//    int i, j, k, n;
//    char *prefix, *tmp;
//    const char *ctmp;
//
//    if (!report) {
//        fprintf(stderr, "%s\n", message);
//        return;
//    }
//
//    prefix = NULL;
//    if (report->filename)
//        prefix = g_strdup_printf("%s:", report->filename);
//    if (report->lineno) {
//        tmp = prefix;
//        prefix = g_strdup_printf("%s%u: ", tmp ? tmp : "", report->lineno);
//        g_free(tmp);
//    }
//    if (JSREPORT_IS_WARNING(report->flags)) {
//        tmp = prefix;
//        prefix = g_strdup_printf("%s%swarning: ",
//                                 tmp ? tmp : "",
//                                 JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
//        g_free(tmp);
//    }
//
//    /* embedded newlines -- argh! */
//    while ((ctmp = strchr(message, '\n')) != NULL) {
//        ctmp++;
//        if (prefix)
//            fputs(prefix, stderr);
//        fwrite(message, 1, ctmp - message, stderr);
//        message = ctmp;
//    }
//
//    /* If there were no filename or lineno, the prefix might be empty */
//    if (prefix)
//        fputs(prefix, stderr);
//    fputs(message, stderr);
//
//    if (!report->linebuf) {
//        fputc('\n', stderr);
//        goto out;
//    }
//
//    /* report->linebuf usually ends with a newline. */
//    n = strlen(report->linebuf);
//    fprintf(stderr, ":\n%s%s%s%s",
//            prefix,
//            report->linebuf,
//            (n > 0 && report->linebuf[n-1] == '\n') ? "" : "\n",
//            prefix);
//    n = ((char*)report->tokenptr) - ((char*) report->linebuf);
//    for (i = j = 0; i < n; i++) {
//        if (report->linebuf[i] == '\t') {
//            for (k = (j + 8) & ~7; j < k; j++) {
//                fputc('.', stderr);
//            }
//            continue;
//        }
//        fputc('.', stderr);
//        j++;
//    }
//    fputs("^\n", stderr);
// out:
//    g_free(prefix);
//}

#ifdef HAVE_LIBREADLINE
static JSBool
gwkjs_console_readline(JSContextRef cx, char **bufp, FILE *file, const char *prompt)
{
    char *line;
    line = readline(prompt);
    if (!line)
        return JS_FALSE;

    if (line[0] != '\0')
        add_history(line);
    *bufp = line;
    return JS_TRUE;
}
#else
static JSBool
gwkjs_console_readline(JSContextRef *cx, char **bufp, FILE *file, const char *prompt)
{
    char line[256];
    fprintf(stdout, "%s", prompt);
    fflush(stdout);
    if (!fgets(line, sizeof line, file))
        return JS_FALSE;
    *bufp = g_strdup(line);
    return JS_TRUE;
}
#endif

static gboolean
_is_compilant(JSContextRef context, const gchar *buffer, gsize len)
{
    JSStringRef str = gwkjs_cstring_to_jsstring(buffer);
    gboolean ret = JSCheckScriptSyntax(context, str, NULL, 1, NULL);

    JSStringRelease(str);
    return ret;
}

static JSValueRef
gwkjs_console_interact(JSContextRef context,
                       JSObjectRef function,
                       JSObjectRef this_object,
                       size_t argumentCount,
                       const JSValueRef arguments[],
                       JSValueRef *exception)
{
    JSObjectRef object = this_object;
    gboolean eof = FALSE;
    JSStringRef str;
    JSValueRef result = NULL;

    GString *buffer = NULL;
    char *temp_buf = NULL;
    char *err_buf = NULL;
    int lineno;
    int startline;
    FILE *file = stdin;

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    lineno = 1;
    do {
        // Cleaning exception
        *exception = NULL;

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineno;
        buffer = g_string_new("");
        guint changedCounter = 0;
        do {
            if (!gwkjs_console_readline(context, &temp_buf, file,
                                        startline == lineno ? "gwkjs> " : ".... ")) {
                eof = JS_TRUE;
                break;
            }
            if (temp_buf[0] == '\0')
                changedCounter++;
            else
                changedCounter = 0;

            g_string_append(buffer, temp_buf);
            g_free(temp_buf);
            lineno++;

            if (changedCounter == 2)
                break;

        } while (!_is_compilant(context, buffer->str, buffer->len));


    	str = gwkjs_cstring_to_jsstring(buffer->str);
        result = JSEvaluateScript(context, str, this_object, NULL, lineno, exception);
        JSStringRelease(str);
        str = NULL;

        gwkjs_schedule_gc_if_needed(context);

        if (*exception) {
             err_buf = gwkjs_exception_to_string(context, *exception);
        } else if (JSVAL_IS_VOID(context, result)) {
            err_buf = NULL;
        } else {
            err_buf = gwkjs_jsvalue_to_cstring(context, result, NULL);
        }

        if (err_buf) {
            g_fprintf(stdout, "%s\n", err_buf);
            g_free(err_buf);
            err_buf = NULL;
        }

        g_string_free(buffer, TRUE);
        buffer = NULL;
        temp_buf = NULL;
    } while (!eof);

    g_fprintf(stdout, "\n");

    if (file != stdin)
        fclose(file);

    return JSValueMakeUndefined(context);
}

JSBool
gwkjs_define_console_stuff(JSContextRef  context,
                           JSObjectRef   *module_out)
{
    JSObjectRef module = NULL;
    JSValueRef exception = NULL;

    module = JSObjectMake(context, NULL, NULL);

    JSObjectRef func = gwkjs_make_function(context, "interact", gwkjs_console_interact);

    gwkjs_object_set_property(context, module, "interact", func,
                              kJSPropertyAttributeReadOnly |
                              kJSPropertyAttributeDontDelete,
                              &exception);

    if (exception)
        return FALSE;

    *module_out = module;
    return JS_TRUE;
}
