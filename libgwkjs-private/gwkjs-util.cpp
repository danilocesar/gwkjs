///* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
///* Copyright 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
// *
// * Permission is hereby granted, free of charge, to any person obtaining a copy
// * of this software and associated documentation files (the "Software"), to
// * deal in the Software without restriction, including without limitation the
// * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// * sell copies of the Software, and to permit persons to whom the Software is
// * furnished to do so, subject to the following conditions:
// *
// * The above copyright notice and this permission notice shall be included in
// * all copies or substantial portions of the Software.
// *
// * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// * IN THE SOFTWARE.
// */
//
//#include <config.h>
//#include <string.h>
//
//#include <glib.h>
//#include <glib/gi18n.h>
//
//#include "gwkjs-util.h"
//
//char *
//gwkjs_format_int_alternative_output(int n)
//{
//    return g_strdup_printf("%Id", n);
//}
//
//GType
//gwkjs_locale_category_get_type(void)
//{
//  static volatile size_t g_define_type_id__volatile = 0;
//  if (g_once_init_enter(&g_define_type_id__volatile)) {
//      static const GEnumValue v[] = {
//          { GWKJS_LOCALE_CATEGORY_ALL, "GWKJS_LOCALE_CATEGORY_ALL", "all" },
//          { GWKJS_LOCALE_CATEGORY_COLLATE, "GWKJS_LOCALE_CATEGORY_COLLATE", "collate" },
//          { GWKJS_LOCALE_CATEGORY_CTYPE, "GWKJS_LOCALE_CATEGORY_CTYPE", "ctype" },
//          { GWKJS_LOCALE_CATEGORY_MESSAGES, "GWKJS_LOCALE_CATEGORY_MESSAGES", "messages" },
//          { GWKJS_LOCALE_CATEGORY_MONETARY, "GWKJS_LOCALE_CATEGORY_MONETARY", "monetary" },
//          { GWKJS_LOCALE_CATEGORY_NUMERIC, "GWKJS_LOCALE_CATEGORY_NUMERIC", "numeric" },
//          { GWKJS_LOCALE_CATEGORY_TIME, "GWKJS_LOCALE_CATEGORY_TIME", "time" },
//          { 0, NULL, NULL }
//      };
//      GType g_define_type_id =
//        g_enum_register_static(g_intern_static_string("GwkjsLocaleCategory"), v);
//
//      g_once_init_leave(&g_define_type_id__volatile, g_define_type_id);
//  }
//  return g_define_type_id__volatile;
//}
//
///**
// * gwkjs_setlocale:
// * @category:
// * @locale: (allow-none):
// *
// * Returns:
// */
//const char *
//gwkjs_setlocale(GwkjsLocaleCategory category, const char *locale)
//{
//    /* According to man setlocale(3), the return value may be allocated in
//     * static storage. */
//    return (const char *) setlocale(category, locale);
//}
//
//void
//gwkjs_textdomain(const char *domain)
//{
//    textdomain(domain);
//}
//
//void
//gwkjs_bindtextdomain(const char *domain,
//                   const char *location)
//{
//    bindtextdomain(domain, location);
//    /* Always use UTF-8; we assume it internally here */
//    bind_textdomain_codeset(domain, "UTF-8");
//}
//
//GParamFlags
//gwkjs_param_spec_get_flags(GParamSpec *pspec)
//{
//    return pspec->flags;
//}
//
//GType
//gwkjs_param_spec_get_value_type(GParamSpec *pspec)
//{
//    return pspec->value_type;
//}
//
//GType
//gwkjs_param_spec_get_owner_type(GParamSpec *pspec)
//{
//    return pspec->owner_type;
//}
