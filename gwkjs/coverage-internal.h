/*
 * Copyright © 2015 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authored By: Sam Spilsbury <sam@endlessm.com>
 */

#ifndef _GWKJS_COVERAGE_INTERNAL_H
#define _GWKJS_COVERAGE_INTERNAL_H

#ifndef GWKJS_COMPILATION
#error This file is for internal use and use in the tests only
#endif

#include "jsapi-util.h"
#include "coverage.h"

G_BEGIN_DECLS

GArray * gwkjs_fetch_statistics_from_js(GwkjsCoverage *coverage,
                                      char        **covered_paths);
GBytes * gwkjs_serialize_statistics(GwkjsCoverage *coverage);

JSString * gwkjs_deserialize_cache_to_object(GwkjsCoverage *coverage,
                                           GBytes      *cache_bytes);

gboolean gwkjs_run_script_in_coverage_compartment(GwkjsCoverage *coverage,
                                                const char  *script);
gboolean gwkjs_inject_value_into_coverage_compartment(GwkjsCoverage     *coverage,
                                                    JS::HandleValue value,
                                                    const char      *property);

gboolean gwkjs_get_path_mtime(const char *path,
                            GTimeVal   *mtime);
gchar * gwkjs_get_path_checksum(const char *path);

gboolean gwkjs_write_cache_to_path(const char *path,
                                 GBytes     *cache_bytes);

extern const char *GWKJS_COVERAGE_CACHE_FILE_NAME;

G_END_DECLS

#endif
