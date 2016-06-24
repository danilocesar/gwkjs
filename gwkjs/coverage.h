/*
 * Copyright © 2014 Endless Mobile, Inc.
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
#ifndef _GWKJS_COVERAGE_H
#define _GWKJS_COVERAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GWKJS_TYPE_COVERAGE gwkjs_coverage_get_type()

#define GWKJS_COVERAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
     GWKJS_TYPE_COVERAGE, GwkjsCoverage))

#define GWKJS_COVERAGE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
     GWKJS_TYPE_COVERAGE, GwkjsCoverageClass))

#define GWKJS_IS_COVERAGE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
     GWKJS_TYPE_COVERAGE))

#define GWKJS_IS_COVERAGE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
     GWKJS_TYPE_COVERAGE))

#define GWKJS_COVERAGE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
     GWKJS_TYPE_COVERAGE, GwkjsCoverageClass))

typedef struct _GFile GFile;
typedef struct _GwkjsContext GwkjsContext;

typedef struct _GwkjsCoverage GwkjsCoverage;
typedef struct _GwkjsCoverageClass GwkjsCoverageClass;
typedef struct _GwkjsCoveragePrivate GwkjsCoveragePrivate;

struct _GwkjsCoverage {
    GObject parent;
};

struct _GwkjsCoverageClass {
    GObjectClass parent_class;
};

GType gwkjs_coverage_get_type(void);

/**
 * gwkjs_coverage_write_statistics:
 * @coverage: A #GwkjsCoveerage
 * @output_directory: A directory to write coverage information to. Scripts
 * which were provided as part of the coverage-paths construction property will be written
 * out to output_directory, in the same directory structure relative to the source dir where
 * the tests were run.
 *
 * This function takes all available statistics and writes them out to either the file provided
 * or to files of the pattern (filename).info in the same directory as the scanned files. It will
 * provide coverage data for all files ending with ".js" in the coverage directories, even if they
 * were never actually executed.
 */
void gwkjs_coverage_write_statistics(GwkjsCoverage *coverage,
                                   const char  *output_directory);

GwkjsCoverage * gwkjs_coverage_new(const char   **coverage_prefixes,
                               GwkjsContext    *coverage_context);

GwkjsCoverage * gwkjs_coverage_new_from_cache(const char **coverage_prefixes,
                                          GwkjsContext *context,
                                          const char *cache_path);

G_END_DECLS

#endif
