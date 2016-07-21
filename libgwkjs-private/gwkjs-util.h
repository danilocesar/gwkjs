/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* Copyright 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef __GWKJS_PRIVATE_UTIL_H__
#define __GWKJS_PRIVATE_UTIL_H__

#include <locale.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* For imports.format */
char * gwkjs_format_int_alternative_output (int n);

/* For imports.gettext */
typedef enum
{
  GWKJS_LOCALE_CATEGORY_ALL = LC_ALL,
  GWKJS_LOCALE_CATEGORY_COLLATE = LC_COLLATE,
  GWKJS_LOCALE_CATEGORY_CTYPE = LC_CTYPE,
  GWKJS_LOCALE_CATEGORY_MESSAGES = LC_MESSAGES,
  GWKJS_LOCALE_CATEGORY_MONETARY = LC_MONETARY,
  GWKJS_LOCALE_CATEGORY_NUMERIC = LC_NUMERIC,
  GWKJS_LOCALE_CATEGORY_TIME = LC_TIME
} GwkjsLocaleCategory;

const char *gwkjs_setlocale                (GwkjsLocaleCategory category,
                                          const char       *locale);
void        gwkjs_textdomain               (const char *domain);
void        gwkjs_bindtextdomain           (const char *domain,
                                          const char *location);
GType       gwkjs_locale_category_get_type (void) G_GNUC_CONST;

/* For imports.overrides.GObject */
GParamFlags gwkjs_param_spec_get_flags (GParamSpec *pspec);
GType       gwkjs_param_spec_get_value_type (GParamSpec *pspec);
GType       gwkjs_param_spec_get_owner_type (GParamSpec *pspec);

G_END_DECLS

#endif
