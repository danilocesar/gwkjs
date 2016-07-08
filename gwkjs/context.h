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

#ifndef __GWKJS_CONTEXT_H__
#define __GWKJS_CONTEXT_H__

#if !defined (__GWKJS_GWKJS_H__) && !defined (GWKJS_COMPILATION)
#error "Only <gwkjs/gwkjs.h> can be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GwkjsContext      GwkjsContext;
typedef struct _GwkjsContextClass GwkjsContextClass;

#define GWKJS_TYPE_CONTEXT              (gwkjs_context_get_type ())
#define GWKJS_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GWKJS_TYPE_CONTEXT, GwkjsContext))
#define GWKJS_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GWKJS_TYPE_CONTEXT, GwkjsContextClass))
#define GWKJS_IS_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GWKJS_TYPE_CONTEXT))
#define GWKJS_IS_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GWKJS_TYPE_CONTEXT))
#define GWKJS_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GWKJS_TYPE_CONTEXT, GwkjsContextClass))

GType           gwkjs_context_get_type             (void) G_GNUC_CONST;

GwkjsContext*     gwkjs_context_new                  (void);
GwkjsContext*     gwkjs_context_new_with_search_path (char         **search_path);

gboolean        gwkjs_context_eval_file          (GwkjsContext  *js_context,
                                                  const char    *filename,
                                                  int           *exit_status_p,
                                                  GError       **error);
gboolean        gwkjs_context_eval                 (GwkjsContext  *js_context,
                                                  const char    *script,
                                                  gssize         script_len,
                                                  const char    *filename,
                                                  int           *exit_status_p,
                                                  GError       **error);
gboolean        gwkjs_context_define_string_array  (GwkjsContext  *js_context,
                                                  const char    *array_name,
                                                  gssize         array_length,
                                                  const char   **array_values,
                                                  GError       **error);

GList*          gwkjs_context_get_all              (void);

GwkjsContext     *gwkjs_context_get_current          (void);

void            gwkjs_context_make_current          (GwkjsContext *js_context);

gpointer        gwkjs_context_get_native_context    (GwkjsContext *js_context);

void            gwkjs_context_print_stack_stderr    (GwkjsContext *js_context);

void            gwkjs_context_maybe_gc              (GwkjsContext  *context);

void            gwkjs_context_gc                    (GwkjsContext  *context);

void            gwkjs_dumpstack                     (void);

G_END_DECLS

#endif  /* __GWKJS_CONTEXT_H__ */
