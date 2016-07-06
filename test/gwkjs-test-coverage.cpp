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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <fcntl.h>
#include <ftw.h>

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixoutputstream.h>
#include <gwkjs/gwkjs.h>
#include <gwkjs/coverage.h>
#include <gwkjs/coverage-internal.h>
#include <gwkjs/gwkjs-module.h>

typedef struct _GwkjsCoverageFixture {
    GwkjsContext    *context;
    GwkjsCoverage   *coverage;
    char          *temporary_js_script_directory_name;
    char          *temporary_js_script_filename;
    int           temporary_js_script_open_handle;
} GwkjsCoverageFixture;

static void
write_to_file(int        handle,
              const char *contents)
{
    if (write(handle,
              (gconstpointer) contents,
              sizeof(char) * strlen(contents)) == -1)
        g_error("Failed to write %s to file", contents);
}

static void
write_to_file_at_beginning(int        handle,
                           const char *content)
{
    if (ftruncate(handle, 0) == -1)
        g_print("Error deleting contents of test temporary file: %s\n", strerror(errno));
    lseek(handle, 0, SEEK_SET);
    write_to_file(handle, content);
}

static int
unlink_if_node_is_a_file(const char *path, const struct stat *sb, int typeflag)
{
    if (typeflag == FTW_F)
        unlink(path);
    return 0;
}

static int
rmdir_if_node_is_a_dir(const char *path, const struct stat *sb, int typeflag)
{
    if (typeflag == FTW_D)
        rmdir(path);
    return 0;
}

static void
recursive_delete_dir_at_path(const char *path)
{
    /* We have to recurse twice - once to delete files, and once
     * to delete directories (because ftw uses preorder traversal) */
    ftw(path, unlink_if_node_is_a_file, 100);
    ftw(path, rmdir_if_node_is_a_dir, 100);
}

static void
gwkjs_coverage_fixture_set_up(gpointer      fixture_data,
                            gconstpointer user_data)
{
    GwkjsCoverageFixture *fixture = (GwkjsCoverageFixture *) fixture_data;
    const char         *js_script = "function f() { return 1; }\n";

    fixture->temporary_js_script_directory_name = g_strdup("/tmp/gwkjs_coverage_tmp.XXXXXX");
    fixture->temporary_js_script_directory_name =
        mkdtemp(fixture->temporary_js_script_directory_name);

    if (!fixture->temporary_js_script_directory_name)
        g_error("Failed to create temporary directory for test files: %s\n", strerror(errno));

    fixture->temporary_js_script_filename = g_strconcat(fixture->temporary_js_script_directory_name,
                                                        "/",
                                                        "gwkjs_coverage_script_XXXXXX.js",
                                                        NULL);
    fixture->temporary_js_script_open_handle =
        mkstemps(fixture->temporary_js_script_filename, 3);

    /* Allocate a strv that we can pass over to gwkjs_coverage_new */
    const char *coverage_paths[] = {
        fixture->temporary_js_script_filename,
        NULL
    };

    const char *search_paths[] = {
        fixture->temporary_js_script_directory_name,
        NULL
    };

    fixture->context = gwkjs_context_new_with_search_path((char **) search_paths);
    fixture->coverage = gwkjs_coverage_new(coverage_paths,
                                         fixture->context);

    write_to_file(fixture->temporary_js_script_open_handle, js_script);
}

static void
gwkjs_coverage_fixture_tear_down(gpointer      fixture_data,
                               gconstpointer user_data)
{
    GwkjsCoverageFixture *fixture = (GwkjsCoverageFixture *) fixture_data;
    unlink(fixture->temporary_js_script_filename);
    g_free(fixture->temporary_js_script_filename);
    close(fixture->temporary_js_script_open_handle);
    recursive_delete_dir_at_path(fixture->temporary_js_script_directory_name);
    g_free(fixture->temporary_js_script_directory_name);

    g_object_unref(fixture->coverage);
    g_object_unref(fixture->context);
}

typedef struct _GwkjsCoverageToSingleOutputFileFixture {
    GwkjsCoverageFixture base_fixture;
    char         *output_file_directory;
    char         *output_file_name;
    unsigned int output_file_handle;
} GwkjsCoverageToSingleOutputFileFixture;

static void
gwkjs_coverage_to_single_output_file_fixture_set_up(gpointer      fixture_data,
                                                  gconstpointer user_data)
{
    gwkjs_coverage_fixture_set_up(fixture_data, user_data);

    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;
    fixture->output_file_directory = g_build_filename(fixture->base_fixture.temporary_js_script_directory_name,
                                                      "gwkjs_coverage_test_coverage.XXXXXX",
                                                      NULL);
    fixture->output_file_directory = mkdtemp(fixture->output_file_directory);
    fixture->output_file_name = g_build_filename(fixture->output_file_directory,
                                                 "coverage.lcov",
                                                 NULL);
    fixture->output_file_handle = open(fixture->output_file_name,
                                       O_CREAT | O_CLOEXEC | O_RDWR,
                                       S_IRWXU);
}

static void
gwkjs_coverage_to_single_output_file_fixture_tear_down(gpointer      fixture_data,
                                                     gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;
    unlink(fixture->output_file_name);
    close(fixture->output_file_handle);
    g_free(fixture->output_file_name);
    recursive_delete_dir_at_path(fixture->output_file_directory);
    g_free(fixture->output_file_directory);

    gwkjs_coverage_fixture_tear_down(fixture_data, user_data);
}

static const char *
line_starting_with(const char *data,
                   const char *needle)
{
    const gsize needle_length = strlen(needle);
    const char  *iter = data;

    while (iter) {
        if (strncmp(iter, needle, needle_length) == 0)
          return iter;

        iter = strstr(iter, "\n");

        if (iter)
          iter += 1;
    }

    return NULL;
}

static char *
write_statistics_and_get_coverage_data(GwkjsCoverage *coverage,
                                       const char  *filename,
                                       const char  *output_directory,
                                       gsize       *coverage_data_length_return)
{
    gwkjs_coverage_write_statistics(coverage, output_directory);

    gsize coverage_data_length;
    char  *coverage_data_contents;

    char  *output_filename = g_build_filename(output_directory,
                                              "coverage.lcov",
                                              NULL);

    g_file_get_contents(output_filename,
                        &coverage_data_contents,
                        &coverage_data_length,
                        NULL);

    g_free(output_filename);

    if (coverage_data_length_return)
      *coverage_data_length_return = coverage_data_length;

    return coverage_data_contents;
}

static char *
eval_script_and_get_coverage_data(GwkjsContext  *context,
                                  GwkjsCoverage *coverage,
                                  const char  *filename,
                                  const char  *output_directory,
                                  gsize       *coverage_data_length_return)
{
    gwkjs_context_eval_file(context,
                          filename,
                          NULL,
                          NULL);

    return write_statistics_and_get_coverage_data(coverage,
                                                  filename,
                                                  output_directory,
                                                  coverage_data_length_return);
}

static gboolean
coverage_data_contains_value_for_key(const char *data,
                                     const char *key,
                                     const char *value)
{
    const char *sf_line = line_starting_with(data, key);

    if (!sf_line)
        return FALSE;

    return strncmp(&sf_line[strlen(key)],
                   value,
                   strlen(value)) == 0;
}

typedef gboolean (*CoverageDataMatchFunc) (const char *value,
                                           gpointer    user_data);

static gboolean
coverage_data_matches_value_for_key_internal(const char            *line,
                                             const char            *key,
                                             CoverageDataMatchFunc  match,
                                             gpointer               user_data)
{
    return (*match)(line, user_data);
}

static gboolean
coverage_data_matches_value_for_key(const char            *data,
                                    const char            *key,
                                    CoverageDataMatchFunc  match,
                                    gpointer               user_data)
{
    const char *line = line_starting_with(data, key);

    if (!line)
        return FALSE;

    return coverage_data_matches_value_for_key_internal(line, key, match, user_data);
}

static gboolean
coverage_data_matches_any_value_for_key(const char            *data,
                                        const char            *key,
                                        CoverageDataMatchFunc  match,
                                        gpointer               user_data)
{
    data = line_starting_with(data, key);

    while (data) {
        if (coverage_data_matches_value_for_key_internal(data, key, match, user_data))
            return TRUE;

        data = line_starting_with(data + 1, key);
    }

    return FALSE;
}

static gboolean
coverage_data_matches_values_for_key(const char            *data,
                                     const char            *key,
                                     gsize                  n,
                                     CoverageDataMatchFunc  match,
                                     gpointer               user_data,
                                     gsize                  data_size)
{
    const char *line = line_starting_with (data, key);
    /* Keep matching. If we fail to match one of them then
     * bail out */
    char *data_iterator = (char *) user_data;

    while (line && n > 0) {
        if (!coverage_data_matches_value_for_key_internal(line, key, match, (gpointer) data_iterator))
            return FALSE;

        line = line_starting_with(line + 1, key);
        --n;
        data_iterator += data_size;
    }

    /* If n is zero then we've found all available matches */
    if (n == 0)
        return TRUE;

    return FALSE;
}

/* A simple wrapper around gwkjs_coverage_new */
GwkjsCoverage *
create_coverage_for_script(GwkjsContext *context,
                           const char *script)
{
    const char *coverage_scripts[] = {
        script,
        NULL
    };

    return gwkjs_coverage_new(coverage_scripts,
                            context);
}

GwkjsCoverage *
create_coverage_for_script_and_cache(GwkjsContext *context,
                                     const char *cache,
                                     const char *script)
{
    const char *coverage_scripts[] = {
        script,
        NULL
    };


    return gwkjs_coverage_new_from_cache(coverage_scripts,
                                       context,
                                       cache);
}

static void
test_covered_file_is_duplicated_into_output_if_resource(gpointer      fixture_data,
                                                        gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *mock_resource_filename = "resource:///org/gnome/gwkjs/mock/test/gwkjs-test-coverage/loadedJSFromResource.js";
    const char *coverage_scripts[] = {
        mock_resource_filename,
        NULL
    };

    g_object_unref(fixture->base_fixture.context);
    g_object_unref(fixture->base_fixture.coverage);
    const char *search_paths[] = {
        fixture->base_fixture.temporary_js_script_directory_name,
        NULL
    };

    fixture->base_fixture.context = gwkjs_context_new_with_search_path((char **) search_paths);
    fixture->base_fixture.coverage =
        gwkjs_coverage_new(coverage_scripts,
                         fixture->base_fixture.context);

    gwkjs_context_eval_file(fixture->base_fixture.context,
                          mock_resource_filename,
                          NULL,
                          NULL);

    gwkjs_coverage_write_statistics(fixture->base_fixture.coverage,
                                  fixture->output_file_directory);

    char *expected_temporary_js_script_file_path =
        g_build_filename(fixture->output_file_directory,
                         "org/gnome/gwkjs/mock/test/gwkjs-test-coverage/loadedJSFromResource.js",
                         NULL);

    g_assert_true(g_file_test(expected_temporary_js_script_file_path, G_FILE_TEST_EXISTS));
    g_free(expected_temporary_js_script_file_path);
}


static void
test_covered_file_is_duplicated_into_output_if_path(gpointer      fixture_data,
                                                    gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    gwkjs_context_eval_file(fixture->base_fixture.context,
                          fixture->base_fixture.temporary_js_script_filename,
                          NULL,
                          NULL);

    gwkjs_coverage_write_statistics(fixture->base_fixture.coverage,
                                  fixture->output_file_directory);

    char *temporary_js_script_basename =
        g_filename_display_basename(fixture->base_fixture.temporary_js_script_filename);
    char *expected_temporary_js_script_file_path =
        g_build_filename(fixture->output_file_directory,
                         temporary_js_script_basename,
                         NULL);

    g_assert_true(g_file_test(expected_temporary_js_script_file_path, G_FILE_TEST_EXISTS));

    g_free(expected_temporary_js_script_file_path);
    g_free(temporary_js_script_basename);
}

static void
test_previous_contents_preserved(gpointer      fixture_data,
                                 gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;
    const char *existing_contents = "existing_contents\n";
    write_to_file(fixture->output_file_handle, existing_contents);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    g_assert(strstr(coverage_data_contents, existing_contents) != NULL);
    g_free(coverage_data_contents);
}


static void
test_new_contents_written(gpointer      fixture_data,
                          gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;
    const char *existing_contents = "existing_contents\n";
    write_to_file(fixture->output_file_handle, existing_contents);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* We have new content in the coverage data */
    g_assert(strlen(existing_contents) != strlen(coverage_data_contents));
    g_free(coverage_data_contents);
}

static void
test_expected_source_file_name_written_to_coverage_data(gpointer      fixture_data,
                                                        gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    char *temporary_js_script_basename =
        g_filename_display_basename(fixture->base_fixture.temporary_js_script_filename);
    char *expected_source_filename =
        g_build_filename(fixture->output_file_directory,
                         temporary_js_script_basename,
                         NULL);

    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "SF:",
                                                  expected_source_filename));

    g_free(expected_source_filename);
    g_free(temporary_js_script_basename);
    g_free(coverage_data_contents);
}

static void
silence_log_func(const gchar    *domain,
                 GLogLevelFlags  log_level,
                 const gchar    *message,
                 gpointer        user_data)
{
}

static void
test_expected_entry_not_written_for_nonexistent_file(gpointer      fixture_data,
                                                        gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *coverage_paths[] = {
        "doesnotexist",
        NULL
    };

    g_object_unref(fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = gwkjs_coverage_new(coverage_paths,
                                                      fixture->base_fixture.context);

    /* Temporarily disable fatal mask and silence warnings */
    GLogLevelFlags old_flags = g_log_set_always_fatal((GLogLevelFlags) G_LOG_LEVEL_ERROR);
    GLogFunc old_log_func = g_log_set_default_handler(silence_log_func, NULL);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          "doesnotexist",
                                          fixture->output_file_directory,
                                          NULL);

    g_log_set_always_fatal(old_flags);
    g_log_set_default_handler(old_log_func, NULL);

    char *temporary_js_script_basename =
        g_filename_display_basename("doesnotexist");

    g_assert(!(coverage_data_contains_value_for_key(coverage_data_contents,
                                                    "SF:",
                                                    temporary_js_script_basename)));

    g_free(coverage_data_contents);
    g_free(temporary_js_script_basename);
}

typedef enum _BranchTaken {
    NOT_EXECUTED,
    NOT_TAKEN,
    TAKEN
} BranchTaken;

typedef struct _BranchLineData {
    int         expected_branch_line;
    int         expected_id;
    BranchTaken taken;
} BranchLineData;

static gboolean
branch_at_line_should_be_taken(const char *line,
                               gpointer user_data)
{
    BranchLineData *branch_data = (BranchLineData *) user_data;
    int line_no, branch_id, block_no, hit_count_num, nmatches;
    char hit_count[20];  /* can hold maxint64 (19 digits) + nul terminator */

    /* Advance past "BRDA:" */
    line += 5;

    nmatches = sscanf(line, "%i,%i,%i,%19s", &line_no, &block_no, &branch_id, &hit_count);
    if (nmatches != 4) {
        if (errno != 0)
            g_error("sscanf: %s", strerror(errno));
        else
            g_error("sscanf: only matched %i", nmatches);
    }

    /* Determine the branch hit count. It will be either:
     * > -1 if the line containing the branch was never executed, or
     * > N times the branch was taken.
     *
     * The value of -1 is represented by a single "-" character, so
     * we should detect this case and set the value based on that */
    if (strlen(hit_count) == 1 && *hit_count == '-')
        hit_count_num = -1;
    else
        hit_count_num = atoi(hit_count);

    const gboolean hit_correct_branch_line =
        branch_data->expected_branch_line == line_no;
    const gboolean hit_correct_branch_id =
        branch_data->expected_id == branch_id;
    gboolean branch_correctly_taken_or_not_taken;

    switch (branch_data->taken) {
    case NOT_EXECUTED:
        branch_correctly_taken_or_not_taken = hit_count_num == -1;
        break;
    case NOT_TAKEN:
        branch_correctly_taken_or_not_taken = hit_count_num == 0;
        break;
    case TAKEN:
        branch_correctly_taken_or_not_taken = hit_count_num > 0;
        break;
    default:
        g_assert_not_reached();
    };

    return hit_correct_branch_line &&
           hit_correct_branch_id &&
           branch_correctly_taken_or_not_taken;

}

static void
test_single_branch_coverage_written_to_coverage_data(gpointer      fixture_data,
                                                     gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_basic_branch =
            "let x = 0;\n"
            "if (x > 0)\n"
            "    x++;\n"
            "else\n"
            "    x++;\n";

    /* We have to seek backwards and overwrite */
    lseek(fixture->base_fixture.temporary_js_script_open_handle, 0, SEEK_SET);

    if (write(fixture->base_fixture.temporary_js_script_open_handle,
              (const char *) script_with_basic_branch,
              sizeof(char) * strlen(script_with_basic_branch)) == 0)
        g_error("Failed to basic branch script");

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    const BranchLineData expected_branches[] = {
        { 2, 0, NOT_TAKEN },
        { 2, 1, TAKEN }
    };
    const gsize expected_branches_len = G_N_ELEMENTS(expected_branches);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "BRDA:",
                                                  expected_branches_len,
                                                  branch_at_line_should_be_taken,
                                                  (gpointer) expected_branches,
                                                  sizeof(BranchLineData)));

    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "BRF:",
                                                  "2"));
    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "BRH:",
                                                  "1"));
    g_free(coverage_data_contents);
}

static void
test_multiple_branch_coverage_written_to_coverage_data(gpointer      fixture_data,
                                                       gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_case_statements_branch =
            "let y;\n"
            "for (let x = 0; x < 3; x++) {\n"
            "    switch (x) {\n"
            "    case 0:\n"
            "        y = x + 1;\n"
            "        break;\n"
            "    case 1:\n"
            "        y = x + 1;\n"
            "        break;\n"
            "    case 2:\n"
            "        y = x + 1;\n"
            "        break;\n"
            "    }\n"
            "}\n";

    /* We have to seek backwards and overwrite */
    lseek(fixture->base_fixture.temporary_js_script_open_handle, 0, SEEK_SET);

    if (write(fixture->base_fixture.temporary_js_script_open_handle,
              (const char *) script_with_case_statements_branch,
              sizeof(char) * strlen(script_with_case_statements_branch)) == 0)
        g_error("Failed to write script");

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    const BranchLineData expected_branches[] = {
        { 3, 0, TAKEN },
        { 3, 1, TAKEN },
        { 3, 2, TAKEN }
    };
    const gsize expected_branches_len = G_N_ELEMENTS(expected_branches);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "BRDA:",
                                                  expected_branches_len,
                                                  branch_at_line_should_be_taken,
                                                  (gpointer) expected_branches,
                                                  sizeof(BranchLineData)));
    g_free(coverage_data_contents);
}

static void
test_branches_for_multiple_case_statements_fallthrough(gpointer      fixture_data,
                                                       gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_case_statements_branch =
            "let y;\n"
            "for (let x = 0; x < 3; x++) {\n"
            "    switch (x) {\n"
            "    case 0:\n"
            "    case 1:\n"
            "        y = x + 1;\n"
            "        break;\n"
            "    case 2:\n"
            "        y = x + 1;\n"
            "        break;\n"
            "    case 3:\n"
            "        y = x +1;\n"
            "        break;\n"
            "    }\n"
            "}\n";

    /* We have to seek backwards and overwrite */
    lseek(fixture->base_fixture.temporary_js_script_open_handle, 0, SEEK_SET);

    if (write(fixture->base_fixture.temporary_js_script_open_handle,
              (const char *) script_with_case_statements_branch,
              sizeof(char) * strlen(script_with_case_statements_branch)) == 0)
        g_error("Failed to write script");

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    const BranchLineData expected_branches[] = {
        { 3, 0, TAKEN },
        { 3, 1, TAKEN },
        { 3, 2, NOT_TAKEN }
    };
    const gsize expected_branches_len = G_N_ELEMENTS(expected_branches);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "BRDA:",
                                                  expected_branches_len,
                                                  branch_at_line_should_be_taken,
                                                  (gpointer) expected_branches,
                                                  sizeof(BranchLineData)));
    g_free(coverage_data_contents);
}

static void
test_branch_not_hit_written_to_coverage_data(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_never_executed_branch =
            "let x = 0;\n"
            "if (x > 0) {\n"
            "    if (x > 0)\n"
            "        x++;\n"
            "} else {\n"
            "    x++;\n"
            "}\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_never_executed_branch);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    const BranchLineData expected_branch = {
        3, 0, NOT_EXECUTED
    };

    g_assert(coverage_data_matches_any_value_for_key(coverage_data_contents,
                                                     "BRDA:",
                                                     branch_at_line_should_be_taken,
                                                     (gpointer) &expected_branch));
    g_free(coverage_data_contents);
}

static gboolean
has_function_name(const char *line,
                  gpointer    user_data)
{
    /* User data is const char ** */
    const char *expected_function_name = *((const char **) user_data);

    /* Advance past "FN:" */
    line += 3;

    /* Advance past the first comma */
    while (*(line - 1) != ',')
        ++line;

    return strncmp(line,
                   expected_function_name,
                   strlen(expected_function_name)) == 0;
}

static void
test_function_names_written_to_coverage_data(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_named_and_unnamed_functions =
            "function f(){}\n"
            "let b = function(){}\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_named_and_unnamed_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* The internal hash table is sorted in alphabetical order
     * so the function names need to be in this order too */
    const char * expected_function_names[] = {
        "(anonymous):2:0",
        "f:1:0"
    };
    const gsize expected_function_names_len = G_N_ELEMENTS(expected_function_names);

    /* Just expect that we've got an FN matching out expected function names */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "FN:",
                                                  expected_function_names_len,
                                                  has_function_name,
                                                  (gpointer) expected_function_names,
                                                  sizeof(const char *)));
    g_free(coverage_data_contents);
}

static gboolean
has_function_line(const char *line,
                  gpointer    user_data)
{
    /* User data is const char ** */
    const char *expected_function_line = *((const char **) user_data);

    /* Advance past "FN:" */
    line += 3;

    return strncmp(line,
                   expected_function_line,
                   strlen(expected_function_line)) == 0;
}

static void
test_function_lines_written_to_coverage_data(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_functions =
        "function f(){}\n"
        "\n"
        "function g(){}\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);
    const char * expected_function_lines[] = {
        "1",
        "3"
    };
    const gsize expected_function_lines_len = G_N_ELEMENTS(expected_function_lines);

    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "FN:",
                                                  expected_function_lines_len,
                                                  has_function_line,
                                                  (gpointer) expected_function_lines,
                                                  sizeof(const char *)));
    g_free(coverage_data_contents);
}

typedef struct _FunctionHitCountData {
    const char   *function;
    unsigned int hit_count_minimum;
} FunctionHitCountData;

static gboolean
hit_count_is_more_than_for_function(const char *line,
                                    gpointer   user_data)
{
    FunctionHitCountData *data = (FunctionHitCountData *) user_data;
    char                 *detected_function = NULL;
    unsigned int         hit_count;
    size_t max_buf_size;
    int nmatches;

    /* Advance past "FNDA:" */
    line += 5;

    max_buf_size = strcspn(line, "\n");
    detected_function = g_new(char, max_buf_size + 1);
    nmatches = sscanf(line, "%i,%s", &hit_count, detected_function);
    if (nmatches != 2) {
        if (errno != 0)
            g_error("sscanf: %s", strerror(errno));
        else
            g_error("sscanf: only matched %d", nmatches);
    }

    const gboolean function_name_match = g_strcmp0(data->function, detected_function) == 0;
    const gboolean hit_count_more_than = hit_count >= data->hit_count_minimum;

    g_free(detected_function);

    return function_name_match &&
           hit_count_more_than;
}

/* For functions with whitespace between their definition and
 * first executable line, its possible that the JS engine might
 * enter their frame a little later in the script than where their
 * definition starts. We need to handle that case */
static void
test_function_hit_counts_for_big_functions_written_to_coverage_data(gpointer      fixture_data,
                                                                    gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_executed_functions =
            "function f(){\n"
            "\n"
            "\n"
            "var x = 1;\n"
            "}\n"
            "let b = function(){}\n"
            "f();\n"
            "b();\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_executed_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* The internal hash table is sorted in alphabetical order
     * so the function names need to be in this order too */
    FunctionHitCountData expected_hit_counts[] = {
        { "(anonymous):6:0", 1 },
        { "f:1:0", 1 }
    };

    const gsize expected_hit_count_len = G_N_ELEMENTS(expected_hit_counts);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "FNDA:",
                                                  expected_hit_count_len,
                                                  hit_count_is_more_than_for_function,
                                                  (gpointer) expected_hit_counts,
                                                  sizeof(FunctionHitCountData)));

    g_free(coverage_data_contents);
}

/* For functions which start executing at a function declaration
 * we also need to make sure that we roll back to the real function, */
static void
test_function_hit_counts_for_little_functions_written_to_coverage_data(gpointer      fixture_data,
                                                                       gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_executed_functions =
            "function f(){\n"
            "var x = function(){};\n"
            "}\n"
            "let b = function(){}\n"
            "f();\n"
            "b();\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_executed_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* The internal hash table is sorted in alphabetical order
     * so the function names need to be in this order too */
    FunctionHitCountData expected_hit_counts[] = {
        { "(anonymous):2:0", 0 },
        { "(anonymous):4:0", 1 },
        { "f:1:0", 1 }
    };

    const gsize expected_hit_count_len = G_N_ELEMENTS(expected_hit_counts);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "FNDA:",
                                                  expected_hit_count_len,
                                                  hit_count_is_more_than_for_function,
                                                  (gpointer) expected_hit_counts,
                                                  sizeof(FunctionHitCountData)));

    g_free(coverage_data_contents);
}

static void
test_function_hit_counts_written_to_coverage_data(gpointer      fixture_data,
                                                  gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_executed_functions =
            "function f(){}\n"
            "let b = function(){}\n"
            "f();\n"
            "b();\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_executed_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* The internal hash table is sorted in alphabetical order
     * so the function names need to be in this order too */
    FunctionHitCountData expected_hit_counts[] = {
        { "(anonymous):2:0", 1 },
        { "f:1:0", 1 }
    };

    const gsize expected_hit_count_len = G_N_ELEMENTS(expected_hit_counts);

    /* There are two possible branches here, the second should be taken
     * and the first should not have been */
    g_assert(coverage_data_matches_values_for_key(coverage_data_contents,
                                                  "FNDA:",
                                                  expected_hit_count_len,
                                                  hit_count_is_more_than_for_function,
                                                  (gpointer) expected_hit_counts,
                                                  sizeof(FunctionHitCountData)));

    g_free(coverage_data_contents);
}

static void
test_total_function_coverage_written_to_coverage_data(gpointer      fixture_data,
                                                      gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_some_executed_functions =
            "function f(){}\n"
            "let b = function(){}\n"
            "f();\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_some_executed_functions);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* More than one assert per test is bad, but we are testing interlinked concepts */
    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "FNF:",
                                                  "2"));
    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "FNH:",
                                                  "1"));
    g_free(coverage_data_contents);
}

typedef struct _LineCountIsMoreThanData {
    unsigned int expected_lineno;
    unsigned int expected_to_be_more_than;
} LineCountIsMoreThanData;

static gboolean
line_hit_count_is_more_than(const char *line,
                            gpointer    user_data)
{
    LineCountIsMoreThanData *data = (LineCountIsMoreThanData *) user_data;

    const char *coverage_line = &line[3];
    char *comma_ptr = NULL;

    unsigned int lineno = strtol(coverage_line, &comma_ptr, 10);

    g_assert(comma_ptr[0] == ',');

    char *end_ptr = NULL;

    unsigned int value = strtol(&comma_ptr[1], &end_ptr, 10);

    g_assert(end_ptr[0] == '\0' ||
             end_ptr[0] == '\n');

    return data->expected_lineno == lineno &&
           value > data->expected_to_be_more_than;
}

static void
test_single_line_hit_written_to_coverage_data(gpointer      fixture_data,
                                              gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    LineCountIsMoreThanData data = {
        1,
        0
    };

    g_assert(coverage_data_matches_value_for_key(coverage_data_contents,
                                                 "DA:",
                                                 line_hit_count_is_more_than,
                                                 &data));
    g_free(coverage_data_contents);
}

static void
test_hits_on_multiline_if_cond(gpointer      fixture_data,
                                gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    const char *script_with_multine_if_cond =
            "let a = 1;\n"
            "let b = 1;\n"
            "if (a &&\n"
            "    b) {\n"
            "}\n";

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               script_with_multine_if_cond);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* Hits on all lines, including both lines with a condition (3 and 4) */
    LineCountIsMoreThanData data[] = {
        { 1, 0 },
        { 2, 0 },
        { 3, 0 },
        { 4, 0 }
    };

    g_assert(coverage_data_matches_value_for_key(coverage_data_contents,
                                                 "DA:",
                                                 line_hit_count_is_more_than,
                                                 data));
    g_free(coverage_data_contents);
}

static void
test_full_line_tally_written_to_coverage_data(gpointer      fixture_data,
                                              gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* More than one assert per test is bad, but we are testing interlinked concepts */
    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "LF:",
                                                  "1"));
    g_assert(coverage_data_contains_value_for_key(coverage_data_contents,
                                                  "LH:",
                                                  "1"));
    g_free(coverage_data_contents);
}

static void
test_no_hits_to_coverage_data_for_unexecuted(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    char *coverage_data_contents =
        write_statistics_and_get_coverage_data(fixture->base_fixture.coverage,
                                               fixture->base_fixture.temporary_js_script_filename,
                                               fixture->output_file_directory,
                                               NULL);

    /* No files were executed, so the coverage data is empty. */
    g_assert_cmpstr(coverage_data_contents, ==, "");
    g_free(coverage_data_contents);
}

static void
test_end_of_record_section_written_to_coverage_data(gpointer      fixture_data,
                                                    gconstpointer user_data)
{
    GwkjsCoverageToSingleOutputFileFixture *fixture = (GwkjsCoverageToSingleOutputFileFixture *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    g_assert(strstr(coverage_data_contents, "end_of_record") != NULL);
    g_free(coverage_data_contents);
}

typedef struct _GwkjsCoverageMultipleSourcesFixture {
    GwkjsCoverageToSingleOutputFileFixture base_fixture;
    char         *second_js_source_file_name;
    unsigned int second_gwkjs_source_file_handle;
} GwkjsCoverageMultpleSourcesFixutre;

static void
gwkjs_coverage_multiple_source_files_to_single_output_fixture_set_up(gpointer fixture_data,
                                                                         gconstpointer user_data)
{
    gwkjs_coverage_to_single_output_file_fixture_set_up(fixture_data, user_data);

    GwkjsCoverageMultpleSourcesFixutre *fixture = (GwkjsCoverageMultpleSourcesFixutre *) fixture_data;
    fixture->second_js_source_file_name = g_strconcat(fixture->base_fixture.base_fixture.temporary_js_script_directory_name,
                                                      "/",
                                                      "gwkjs_coverage_second_source_file_XXXXXX.js",
                                                      NULL);
    fixture->second_gwkjs_source_file_handle = mkstemps(fixture->second_js_source_file_name, 3);

    /* Because GwkjsCoverage searches the coverage paths at object-creation time,
     * we need to destroy the previously constructed one and construct it again */
    const char *coverage_paths[] = {
        fixture->base_fixture.base_fixture.temporary_js_script_filename,
        fixture->second_js_source_file_name,
        NULL
    };

    g_object_unref(fixture->base_fixture.base_fixture.context);
    g_object_unref(fixture->base_fixture.base_fixture.coverage);
    const char *search_paths[] = {
        fixture->base_fixture.base_fixture.temporary_js_script_directory_name,
        NULL
    };

    fixture->base_fixture.base_fixture.context = gwkjs_context_new_with_search_path((char **) search_paths);
    fixture->base_fixture.base_fixture.coverage = gwkjs_coverage_new(coverage_paths,
                                                                   fixture->base_fixture.base_fixture.context);

    char *base_name = g_path_get_basename(fixture->base_fixture.base_fixture.temporary_js_script_filename);
    char *base_name_without_extension = g_strndup(base_name,
                                                  strlen(base_name) - 3);
    char *mock_script = g_strconcat("const FirstScript = imports.",
                                    base_name_without_extension,
                                    ";\n",
                                    "let a = FirstScript.f;\n"
                                    "\n",
                                    NULL);

    write_to_file_at_beginning(fixture->second_gwkjs_source_file_handle, mock_script);

    g_free(mock_script);
    g_free(base_name_without_extension);
    g_free(base_name);
}

static void
gwkjs_coverage_multiple_source_files_to_single_output_fixture_tear_down(gpointer      fixture_data,
                                                                      gconstpointer user_data)
{
    GwkjsCoverageMultpleSourcesFixutre *fixture = (GwkjsCoverageMultpleSourcesFixutre *) fixture_data;
    unlink(fixture->second_js_source_file_name);
    g_free(fixture->second_js_source_file_name);
    close(fixture->second_gwkjs_source_file_handle);

    gwkjs_coverage_to_single_output_file_fixture_tear_down(fixture_data, user_data);
}

static void
test_multiple_source_file_records_written_to_coverage_data(gpointer      fixture_data,
                                                           gconstpointer user_data)
{
    GwkjsCoverageMultpleSourcesFixutre *fixture = (GwkjsCoverageMultpleSourcesFixutre *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.base_fixture.context,
                                          fixture->base_fixture.base_fixture.coverage,
                                          fixture->second_js_source_file_name,
                                          fixture->base_fixture.output_file_directory,
                                          NULL);

    const char *first_sf_record = line_starting_with(coverage_data_contents, "SF:");
    const char *second_sf_record = line_starting_with(first_sf_record + 1, "SF:");

    g_assert(first_sf_record != NULL);
    g_assert(second_sf_record != NULL);

    g_free(coverage_data_contents);
}

typedef struct _ExpectedSourceFileCoverageData {
    const char              *source_file_path;
    LineCountIsMoreThanData *more_than;
    unsigned int            n_more_than_matchers;
    const char              expected_lines_hit_character;
    const char              expected_lines_found_character;
} ExpectedSourceFileCoverageData;

static gboolean
check_coverage_data_for_source_file(ExpectedSourceFileCoverageData *expected,
                                    const gsize                     expected_size,
                                    const char                     *section_start)
{
    gsize i;
    for (i = 0; i < expected_size; ++i) {
        if (strncmp(&section_start[3],
                    expected[i].source_file_path,
                    strlen (expected[i].source_file_path)) == 0) {
            const gboolean line_hits_match = coverage_data_matches_values_for_key(section_start,
                                                                                  "DA:",
                                                                                  expected[i].n_more_than_matchers,
                                                                                  line_hit_count_is_more_than,
                                                                                  expected[i].more_than,
                                                                                  sizeof (LineCountIsMoreThanData));
            const char *total_hits_record = line_starting_with(section_start, "LH:");
            const gboolean total_hits_match = total_hits_record[3] == expected[i].expected_lines_hit_character;
            const char *total_found_record = line_starting_with(section_start, "LF:");
            const gboolean total_found_match = total_found_record[3] == expected[i].expected_lines_found_character;

            return line_hits_match &&
                   total_hits_match &&
                   total_found_match;
        }
    }

    return FALSE;
}

static char *
get_output_path_for_script_on_disk(const char *path_to_script,
                                   const char *path_to_output_dir)

{
    char *base = g_filename_display_basename(path_to_script);
    char *output_path = g_build_filename(path_to_output_dir, base, NULL);

    g_free(base);
    return output_path;
}

static void
test_correct_line_coverage_data_written_for_both_source_file_sectons(gpointer      fixture_data,
                                                                     gconstpointer user_data)
{
    GwkjsCoverageMultpleSourcesFixutre *fixture = (GwkjsCoverageMultpleSourcesFixutre *) fixture_data;

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.base_fixture.context,
                                          fixture->base_fixture.base_fixture.coverage,
                                          fixture->second_js_source_file_name,
                                          fixture->base_fixture.output_file_directory,
                                          NULL);

    LineCountIsMoreThanData first_script_matcher = {
        1,
        0
    };

    LineCountIsMoreThanData second_script_matchers[] = {
        {
            1,
            0
        },
        {
            2,
            0
        }
    };

    char *first_script_basename =
        g_filename_display_basename(fixture->base_fixture.base_fixture.temporary_js_script_filename);
    char *second_script_basename =
        g_filename_display_basename(fixture->second_js_source_file_name);

    char *first_script_output_path =
        g_build_filename(fixture->base_fixture.output_file_directory,
                         first_script_basename,
                         NULL);
    char *second_script_output_path =
        g_build_filename(fixture->base_fixture.output_file_directory,
                         second_script_basename,
                         NULL);

    ExpectedSourceFileCoverageData expected[] = {
        {
            first_script_output_path,
            &first_script_matcher,
            1,
            '1',
            '1'
        },
        {
            second_script_output_path,
            second_script_matchers,
            2,
            '2',
            '2'
        }
    };

    const gsize expected_len = G_N_ELEMENTS(expected);

    const char *first_sf_record = line_starting_with(coverage_data_contents, "SF:");
    g_assert(check_coverage_data_for_source_file(expected, expected_len, first_sf_record));

    const char *second_sf_record = line_starting_with(first_sf_record + 3, "SF:");
    g_assert(check_coverage_data_for_source_file(expected, expected_len, second_sf_record));

    g_free(first_script_basename);
    g_free(first_script_output_path);
    g_free(second_script_basename);
    g_free(second_script_output_path);
    g_free(coverage_data_contents);
}

typedef GwkjsCoverageToSingleOutputFileFixture GwkjsCoverageCacheFixture;

static void
gwkjs_coverage_cache_fixture_set_up(gpointer      fixture_data,
                                  gconstpointer user_data)
{
    gwkjs_coverage_to_single_output_file_fixture_set_up(fixture_data, user_data);
}

static void
gwkjs_coverage_cache_fixture_tear_down(gpointer      fixture_data,
                                     gconstpointer user_data)
{
    gwkjs_coverage_to_single_output_file_fixture_tear_down(fixture_data, user_data);
}

static GString *
append_tuples_to_array_in_object_notation(GString    *string,
                                          const char  *tuple_contents_strv)
{
    char *original_ptr = (char *) tuple_contents_strv;
    char *expected_tuple_contents = NULL;
    while ((expected_tuple_contents = strsep((char **) &tuple_contents_strv, ";")) != NULL) {
       if (!strlen(expected_tuple_contents))
           continue;

       if (expected_tuple_contents != original_ptr)
           g_string_append_printf(string, ",");
        g_string_append_printf(string, "{%s}", expected_tuple_contents);
    }

    return string;
}

static GString *
format_expected_cache_object_notation(const char *mtimes,
                                      const char *hash,
                                      const char *script_name,
                                      const char *expected_executable_lines_array,
                                      const char *expected_branches,
                                      const char *expected_functions)
{
    GString *string = g_string_new("");
    g_string_append_printf(string,
                           "{\"%s\":{\"mtime\":%s,\"checksum\":%s,\"lines\":[%s],\"branches\":[",
                           script_name,
                           mtimes,
                           hash,
                           expected_executable_lines_array);
    append_tuples_to_array_in_object_notation(string, expected_branches);
    g_string_append_printf(string, "],\"functions\":[");
    append_tuples_to_array_in_object_notation(string, expected_functions);
    g_string_append_printf(string, "]}}");
    return string;
}

typedef struct _GwkjsCoverageCacheObjectNotationTestTableData {
    const char *test_name;
    const char *script;
    const char *resource_path;
    const char *expected_executable_lines;
    const char *expected_branches;
    const char *expected_functions;
} GwkjsCoverageCacheObjectNotationTableTestData;

static GBytes *
serialize_ast_to_bytes(GwkjsCoverage *coverage,
                       const char **coverage_paths)
{
    return gwkjs_serialize_statistics(coverage);
}

static char *
serialize_ast_to_object_notation(GwkjsCoverage *coverage,
                                 const char **coverage_paths)
{
    /* Unfortunately, we need to pass in this paramater here since
     * the len parameter is not allow-none.
     *
     * The caller doesn't need to know about the length of the
     * data since it is only used for strcmp and the data is
     * NUL-terminated anyway. */
    gsize len = 0;
    return (char *)g_bytes_unref_to_data(serialize_ast_to_bytes(coverage, coverage_paths),
                                         &len);
}

static char *
eval_file_for_ast_in_object_notation(GwkjsContext  *context,
                                     GwkjsCoverage *coverage,
                                     const char  *filename)
{
    gboolean success = gwkjs_context_eval_file(context,
                                             filename,
                                             NULL,
                                             NULL);
    g_assert_true(success);
    
    const gchar *coverage_paths[] = {
        filename,
        NULL
    };

    return serialize_ast_to_object_notation(coverage, coverage_paths);
}

static void
test_coverage_cache_data_in_expected_format(gpointer      fixture_data,
                                            gconstpointer user_data)
{
    GwkjsCoverageCacheFixture                     *fixture = (GwkjsCoverageCacheFixture *) fixture_data;
    GwkjsCoverageCacheObjectNotationTableTestData *table_data = (GwkjsCoverageCacheObjectNotationTableTestData *) user_data;

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle, table_data->script);
    char *cache_in_object_notation = eval_file_for_ast_in_object_notation(fixture->base_fixture.context,
                                                                          fixture->base_fixture.coverage,
                                                                          fixture->base_fixture.temporary_js_script_filename);
    g_assert(cache_in_object_notation != NULL);

    /* Sleep for a little while to make sure that the new file has a
     * different mtime */
    sleep(1);

    GTimeVal mtime;
    gboolean successfully_got_mtime = gwkjs_get_path_mtime(fixture->base_fixture.temporary_js_script_filename,
                                                         &mtime);
    g_assert_true(successfully_got_mtime);

    char    *mtime_string = g_strdup_printf("[%lli,%lli]", (gint64) mtime.tv_sec, (gint64) mtime.tv_usec);
    GString *expected_cache_object_notation = format_expected_cache_object_notation(mtime_string,
                                                                                    "null",
                                                                                    fixture->base_fixture.temporary_js_script_filename,
                                                                                    table_data->expected_executable_lines,
                                                                                    table_data->expected_branches,
                                                                                    table_data->expected_functions);

    g_assert_cmpstr(cache_in_object_notation, ==, expected_cache_object_notation->str);

    g_string_free(expected_cache_object_notation, TRUE);
    g_free(cache_in_object_notation);
    g_free(mtime_string);
}

static void
test_coverage_cache_data_in_expected_format_resource(gpointer      fixture_data,
                                                     gconstpointer user_data)
{
    GwkjsCoverageCacheFixture                     *fixture = (GwkjsCoverageCacheFixture *) fixture_data;
    GwkjsCoverageCacheObjectNotationTableTestData *table_data = (GwkjsCoverageCacheObjectNotationTableTestData *) user_data;

    char *hash_string_no_quotes = gwkjs_get_path_checksum(table_data->resource_path);
    char *hash_string = g_strdup_printf("\"%s\"", hash_string_no_quotes);
    g_free(hash_string_no_quotes);

    GString *expected_cache_object_notation = format_expected_cache_object_notation("null",
                                                                                    hash_string,
                                                                                    table_data->resource_path,
                                                                                    table_data->expected_executable_lines,
                                                                                    table_data->expected_branches,
                                                                                    table_data->expected_functions);

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script(fixture->base_fixture.context,
                                                                             table_data->resource_path);
    char *cache_in_object_notation = eval_file_for_ast_in_object_notation(fixture->base_fixture.context,
                                                                          fixture->base_fixture.coverage,
                                                                          table_data->resource_path);

    g_assert_cmpstr(cache_in_object_notation, ==, expected_cache_object_notation->str);

    g_string_free(expected_cache_object_notation, TRUE);
    g_free(cache_in_object_notation);
    g_free(hash_string);
}

static char *
generate_coverage_compartment_verify_script(const char *coverage_script_filename,
                                            const char *user_script)
{
    return g_strdup_printf("const JSUnit = imports.jsUnit;\n"
                           "const covered_script_filename = '%s';\n"
                           "function assertArrayEquals(lhs, rhs) {\n"
                           "    JSUnit.assertEquals(lhs.length, rhs.length);\n"
                           "    for (let i = 0; i < lhs.length; i++)\n"
                           "        JSUnit.assertEquals(lhs[i], rhs[i]);\n"
                           "}\n"
                           "\n"
                           "%s", coverage_script_filename, user_script);
}

typedef struct _GwkjsCoverageCacheJSObjectTableTestData {
    const char *test_name;
    const char *script;
    const char *verify_js_script;
} GwkjsCoverageCacheJSObjectTableTestData;

static void
test_coverage_cache_as_js_object_has_expected_properties(gpointer      fixture_data,
                                                         gconstpointer user_data)
{
    GwkjsCoverageCacheFixture               *fixture = (GwkjsCoverageCacheFixture *) fixture_data;
    GwkjsCoverageCacheJSObjectTableTestData *table_data = (GwkjsCoverageCacheJSObjectTableTestData *) user_data;

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle, table_data->script);
    gwkjs_context_eval_file(fixture->base_fixture.context,
                          fixture->base_fixture.temporary_js_script_filename,
                          NULL,
                          NULL);

    const gchar *coverage_paths[] = {
        fixture->base_fixture.temporary_js_script_filename,
        NULL
    };

    GBytes *cache = serialize_ast_to_bytes(fixture->base_fixture.coverage,
                                           coverage_paths);
    JS::RootedString cache_results(JS_GetRuntime((JSContextRef ) gwkjs_context_get_native_context(fixture->base_fixture.context)),
                                   gwkjs_deserialize_cache_to_object(fixture->base_fixture.coverage, cache));
    JS::RootedValue cache_result_value(JS_GetRuntime((JSContextRef ) gwkjs_context_get_native_context(fixture->base_fixture.context)),
                                       STRING_TO_JSVAL(cache_results));
    gwkjs_inject_value_into_coverage_compartment(fixture->base_fixture.coverage,
                                               cache_result_value,
                                               "coverage_cache");

    gchar *verify_script_complete = generate_coverage_compartment_verify_script(fixture->base_fixture.temporary_js_script_filename,
                                                                                table_data->verify_js_script);
    gwkjs_run_script_in_coverage_compartment(fixture->base_fixture.coverage,
                                           verify_script_complete);
    g_free(verify_script_complete);

    g_bytes_unref(cache);
}

typedef struct _GwkjsCoverageCacheEqualResultsTableTestData {
    const char *test_name;
    const char *script;
} GwkjsCoverageCacheEqualResultsTableTestData;

static char *
write_cache_to_temporary_file(const char *temp_dir,
                              GBytes     *cache)
{
    /* Just need a temporary file, don't care about its fd */
    char *temporary_file = g_build_filename(temp_dir, "gwkjs_coverage_cache_XXXXXX", NULL);
    close(mkstemps(temporary_file, 0));

    if (!gwkjs_write_cache_to_path(temporary_file, cache)) {
        g_free(temporary_file);
        return NULL;
    }

    return temporary_file;
}

static char *
serialize_ast_to_cache_in_temporary_file(GwkjsCoverage *coverage,
                                         const char  *output_directory,
                                         const char  **coverage_paths)
{
    GBytes   *cache = serialize_ast_to_bytes(coverage, coverage_paths);
    char   *cache_path = write_cache_to_temporary_file(output_directory, cache);

    g_bytes_unref(cache);

    return cache_path;
}

static void
test_coverage_cache_equal_results_to_reflect_parse(gpointer      fixture_data,
                                                   gconstpointer user_data)
{
    GwkjsCoverageCacheFixture                   *fixture = (GwkjsCoverageCacheFixture *) fixture_data;
    GwkjsCoverageCacheEqualResultsTableTestData *equal_results_data = (GwkjsCoverageCacheEqualResultsTableTestData *) user_data;

    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               equal_results_data->script);

    const gchar *coverage_paths[] = {
        fixture->base_fixture.temporary_js_script_filename,
        NULL
    };

    char *coverage_data_contents_no_cache =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);
    char *cache_path = serialize_ast_to_cache_in_temporary_file(fixture->base_fixture.coverage,
                                                                fixture->output_file_directory,
                                                                coverage_paths);
    g_assert(cache_path != NULL);

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);
    g_free(cache_path);

    /* Overwrite tracefile with nothing and start over */
    write_to_file_at_beginning(fixture->output_file_handle, "");

    char *coverage_data_contents_cached =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          NULL);

    g_assert_cmpstr(coverage_data_contents_cached, ==, coverage_data_contents_no_cache);

    g_free(coverage_data_contents_cached);
    g_free(coverage_data_contents_no_cache);
}

static char *
eval_file_for_ast_cache_path(GwkjsContext  *context,
                             GwkjsCoverage *coverage,
                             const char  *filename,
                             const char  *output_directory)
{
    gboolean success = gwkjs_context_eval_file(context,
                                             filename,
                                             NULL,
                                             NULL);
    g_assert_true(success);

    const gchar *coverage_paths[] = {
        filename,
        NULL
    };

    return serialize_ast_to_cache_in_temporary_file(coverage,
                                                    output_directory,
                                                    coverage_paths);
}

/* Effectively, the results should be what we expect even though
 * we overwrote the original script after getting coverage and
 * fetching the cache */
static void
test_coverage_cache_invalidation(gpointer      fixture_data,
                                 gconstpointer user_data)
{
    GwkjsCoverageCacheFixture *fixture = (GwkjsCoverageCacheFixture *) fixture_data;

    char *cache_path = eval_file_for_ast_cache_path(fixture->base_fixture.context,
                                                    fixture->base_fixture.coverage,
                                                    fixture->base_fixture.temporary_js_script_filename,
                                                    fixture->output_file_directory);

    /* Sleep for a little while to make sure that the new file has a
     * different mtime */
    sleep(1);

    /* Overwrite tracefile with nothing */
    write_to_file_at_beginning(fixture->output_file_handle, "");

    /* Write a new script into the temporary js file, which will be
     * completely different to the original script that was there */
    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               "let i = 0;\n"
                               "let j = 0;\n");

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);
    g_free(cache_path);

    gsize coverage_data_len = 0;
    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          fixture->base_fixture.temporary_js_script_filename,
                                          fixture->output_file_directory,
                                          &coverage_data_len);

    LineCountIsMoreThanData matchers[] =
    {
        {
            1,
            0
        },
        {
            2,
            0
        }
    };

    char *script_output_path = get_output_path_for_script_on_disk(fixture->base_fixture.temporary_js_script_filename,
                                                                  fixture->output_file_directory);

    ExpectedSourceFileCoverageData expected[] = {
        {
            script_output_path,
            matchers,
            2,
            '2',
            '2'
        }
    };

    const gsize expected_len = G_N_ELEMENTS(expected);
    const char *record = line_starting_with(coverage_data_contents, "SF:");
    g_assert(check_coverage_data_for_source_file(expected, expected_len, record));

    g_free(script_output_path);
    g_free(coverage_data_contents);
}

static void
unload_resource(GResource *resource)
{
    g_resources_unregister(resource);
    g_resource_unref(resource);
}

static GResource *
load_resource_from_builddir(const char *name)
{
    char *resource_path = g_build_filename(GWKJS_TOP_BUILDDIR,
                                           name,
                                           NULL);

    GError    *error = NULL;
    GResource *resource = g_resource_load(resource_path,
                                          &error);

    g_assert_no_error(error);
    g_resources_register(resource);

    g_free(resource_path);

    return resource;
}

/* Load first resource, then unload and load second resource. Both have
 * the same path, but different contents */
static void
test_coverage_cache_invalidation_resource(gpointer      fixture_data,
                                          gconstpointer user_data)
{
    GwkjsCoverageCacheFixture *fixture = (GwkjsCoverageCacheFixture *) fixture_data;

    const char *mock_resource_filename = "resource:///org/gnome/gwkjs/mock/cache/resource.js";

    /* Load the resource archive and register it */
    GResource *first_resource = load_resource_from_builddir("mock-cache-invalidation-before.gresource");

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script(fixture->base_fixture.context,
                                                                mock_resource_filename);

    char *cache_path = eval_file_for_ast_cache_path(fixture->base_fixture.context,
                                                    fixture->base_fixture.coverage,
                                                    mock_resource_filename,
                                                    fixture->output_file_directory);

    /* Load the "after" resource, but have the exact same coverage paths */
    unload_resource(first_resource);
    GResource *second_resource = load_resource_from_builddir("mock-cache-invalidation-after.gresource");

    /* Overwrite tracefile with nothing */
    write_to_file_at_beginning(fixture->output_file_handle, "");

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          mock_resource_filename);
    g_free(cache_path);

    char *coverage_data_contents =
        eval_script_and_get_coverage_data(fixture->base_fixture.context,
                                          fixture->base_fixture.coverage,
                                          mock_resource_filename,
                                          fixture->output_file_directory,
                                          NULL);

    /* Don't need this anymore */
    unload_resource(second_resource);

    /* Now assert that the coverage file has executable lines in
     * the places that we expect them to be */
    LineCountIsMoreThanData matchers[] = {
        {
            1,
            0
        },
        {
            2,
            0
        }
    };

    char *script_output_path =
        g_build_filename(fixture->output_file_directory,
                         "org/gnome/gwkjs/mock/cache/resource.js",
                         NULL);

    ExpectedSourceFileCoverageData expected[] = {
        {
            script_output_path,
            matchers,
            2,
            '2',
            '2'
        }
    };

    const gsize expected_len = G_N_ELEMENTS(expected);
    const char *record = line_starting_with(coverage_data_contents, "SF:");
    g_assert(check_coverage_data_for_source_file(expected, expected_len, record));

    g_free(script_output_path);
    g_free(coverage_data_contents);
}

static char *
get_coverage_cache_path(const char *output_directory)
{
    char *cache_path = g_build_filename(output_directory,
                                        "coverage-cache-XXXXXX",
                                        NULL);
    close(mkstemp(cache_path));
    unlink(cache_path);

    return cache_path;
}

static void
test_coverage_cache_file_written_when_no_cache_exists(gpointer      fixture_data,
                                                      gconstpointer user_data)
{
    GwkjsCoverageCacheFixture *fixture = (GwkjsCoverageCacheFixture *) fixture_data;
    char *cache_path = get_coverage_cache_path(fixture->output_file_directory);

    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);

    /* We need to execute the script now in order for a cache entry
     * to be created, since unexecuted scripts are not counted as
     * part of the coverage report. */
    gboolean success = gwkjs_context_eval_file(fixture->base_fixture.context,
                                             fixture->base_fixture.temporary_js_script_filename,
                                             NULL,
                                             NULL);
    g_assert_true(success);

    gwkjs_coverage_write_statistics(fixture->base_fixture.coverage,
                                  fixture->output_file_directory);

    g_assert_true(g_file_test(cache_path, G_FILE_TEST_EXISTS));
    g_free(cache_path);
}

static GTimeVal
eval_script_for_cache_mtime(GwkjsContext  *context,
                            GwkjsCoverage *coverage,
                            const char  *cache_path,
                            const char  *script,
                            const char  *output_directory)
{
    gboolean success = gwkjs_context_eval_file(context,
                                             script,
                                             NULL,
                                             NULL);
    g_assert_true(success);

    gwkjs_coverage_write_statistics(coverage,
                                  output_directory);

    GTimeVal mtime;
    gboolean successfully_got_mtime = gwkjs_get_path_mtime(cache_path, &mtime);
    g_assert_true(successfully_got_mtime);

    return mtime;
}

static void
test_coverage_cache_updated_when_cache_stale(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageCacheFixture *fixture = (GwkjsCoverageCacheFixture *) fixture_data;

    char *cache_path = get_coverage_cache_path(fixture->output_file_directory);
    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);

    GTimeVal first_cache_mtime = eval_script_for_cache_mtime(fixture->base_fixture.context,
                                                             fixture->base_fixture.coverage,
                                                             cache_path,
                                                             fixture->base_fixture.temporary_js_script_filename,
                                                             fixture->output_file_directory);

    /* Sleep for a little while to make sure that the new file has a
     * different mtime */
    sleep(1);

    /* Write a new script into the temporary js file, which will be
     * completely different to the original script that was there */
    write_to_file_at_beginning(fixture->base_fixture.temporary_js_script_open_handle,
                               "let i = 0;\n"
                               "let j = 0;\n");

    /* Re-create coverage object, covering new script */
    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);


    /* Run the script again, which will cause an attempt
     * to look up the AST data. Upon writing the statistics
     * again, the cache should have been missed some of the time
     * so the second mtime will be greater than the first */
    GTimeVal second_cache_mtime = eval_script_for_cache_mtime(fixture->base_fixture.context,
                                                              fixture->base_fixture.coverage,
                                                              cache_path,
                                                              fixture->base_fixture.temporary_js_script_filename,
                                                              fixture->output_file_directory);


    const gboolean seconds_different = (first_cache_mtime.tv_sec != second_cache_mtime.tv_sec);
    const gboolean microseconds_different (first_cache_mtime.tv_usec != second_cache_mtime.tv_usec);

    g_assert_true(seconds_different || microseconds_different);

    g_free(cache_path);
}

static void
test_coverage_cache_not_updated_on_full_hits(gpointer      fixture_data,
                                             gconstpointer user_data)
{
    GwkjsCoverageCacheFixture *fixture = (GwkjsCoverageCacheFixture *) fixture_data;

    char *cache_path = get_coverage_cache_path(fixture->output_file_directory);
    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);

    GTimeVal first_cache_mtime = eval_script_for_cache_mtime(fixture->base_fixture.context,
                                                             fixture->base_fixture.coverage,
                                                             cache_path,
                                                             fixture->base_fixture.temporary_js_script_filename,
                                                             fixture->output_file_directory);

    /* Re-create coverage object, covering same script */
    g_clear_object(&fixture->base_fixture.coverage);
    fixture->base_fixture.coverage = create_coverage_for_script_and_cache(fixture->base_fixture.context,
                                                                          cache_path,
                                                                          fixture->base_fixture.temporary_js_script_filename);


    /* Run the script again, which will cause an attempt
     * to look up the AST data. Upon writing the statistics
     * again, the cache should have been hit of the time
     * so the second mtime will be the same as the first */
    GTimeVal second_cache_mtime = eval_script_for_cache_mtime(fixture->base_fixture.context,
                                                              fixture->base_fixture.coverage,
                                                              cache_path,
                                                              fixture->base_fixture.temporary_js_script_filename,
                                                              fixture->output_file_directory);

    g_assert_cmpint(first_cache_mtime.tv_sec, ==, second_cache_mtime.tv_sec);
    g_assert_cmpint(first_cache_mtime.tv_usec, ==, second_cache_mtime.tv_usec);

    g_free(cache_path);
}

typedef struct _FixturedTest {
    gsize            fixture_size;
    GTestFixtureFunc set_up;
    GTestFixtureFunc tear_down;
} FixturedTest;

static void
add_test_for_fixture(const char      *name,
                     FixturedTest    *fixture,
                     GTestFixtureFunc test_func,
                     gconstpointer    user_data)
{
    g_test_add_vtable(name,
                      fixture->fixture_size,
                      user_data,
                      fixture->set_up,
                      test_func,
                      fixture->tear_down);
}

/* All table driven tests must be binary compatible with at
 * least this header */
typedef struct _TestTableDataHeader {
    const char *test_name;
} TestTableDataHeader;

static void
add_table_driven_test_for_fixture(const char                *name,
                                  FixturedTest              *fixture,
                                  GTestFixtureFunc          test_func,
                                  gsize                     table_entry_size,
                                  gsize                     n_table_entries,
                                  const TestTableDataHeader *test_table)
{
    const char  *test_table_ptr = (const char *)test_table;
    gsize test_table_index;

    for (test_table_index = 0;
         test_table_index < n_table_entries;
         ++test_table_index, test_table_ptr += table_entry_size) {
        TestTableDataHeader *header = (TestTableDataHeader *) test_table_ptr;
        gchar *test_name_for_table_index = g_strdup_printf("%s/%s",
                                                           name,
                                                           header->test_name);
        g_test_add_vtable(test_name_for_table_index,
                          fixture->fixture_size,
                          test_table_ptr,
                          fixture->set_up,
                          test_func,
                          fixture->tear_down);
        g_free(test_name_for_table_index);
    }
}

void gwkjs_test_add_tests_for_coverage()
{
    FixturedTest coverage_to_single_output_fixture = {
        sizeof(GwkjsCoverageToSingleOutputFileFixture),
        gwkjs_coverage_to_single_output_file_fixture_set_up,
        gwkjs_coverage_to_single_output_file_fixture_tear_down
    };

    add_test_for_fixture("/gwkjs/coverage/file_duplicated_into_output_path",
                         &coverage_to_single_output_fixture,
                         test_covered_file_is_duplicated_into_output_if_path,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/file_duplicated_full_resource_path",
                         &coverage_to_single_output_fixture,
                         test_covered_file_is_duplicated_into_output_if_resource,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/contents_preserved_accumulate_mode",
                         &coverage_to_single_output_fixture,
                         test_previous_contents_preserved,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/new_contents_appended_accumulate_mode",
                         &coverage_to_single_output_fixture,
                         test_new_contents_written,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/expected_source_file_name_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_expected_source_file_name_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/entry_not_written_for_nonexistent_file",
                         &coverage_to_single_output_fixture,
                         test_expected_entry_not_written_for_nonexistent_file,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/single_branch_coverage_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_single_branch_coverage_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/multiple_branch_coverage_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_multiple_branch_coverage_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/branches_for_multiple_case_statements_fallthrough",
                         &coverage_to_single_output_fixture,
                         test_branches_for_multiple_case_statements_fallthrough,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/not_hit_branch_point_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_branch_not_hit_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/function_names_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_function_names_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/function_lines_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_function_lines_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/function_hit_counts_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_function_hit_counts_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/big_function_hit_counts_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_function_hit_counts_for_big_functions_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/little_function_hit_counts_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_function_hit_counts_for_little_functions_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/total_function_coverage_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_total_function_coverage_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/single_line_hit_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_single_line_hit_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/hits_on_multiline_if_cond",
                         &coverage_to_single_output_fixture,
                         test_hits_on_multiline_if_cond,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/full_line_tally_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_full_line_tally_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/no_hits_for_unexecuted_file",
                         &coverage_to_single_output_fixture,
                         test_no_hits_to_coverage_data_for_unexecuted,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/end_of_record_section_written_to_coverage_data",
                         &coverage_to_single_output_fixture,
                         test_end_of_record_section_written_to_coverage_data,
                         NULL);

    FixturedTest coverage_for_multiple_files_to_single_output_fixture = {
        sizeof(GwkjsCoverageMultpleSourcesFixutre),
        gwkjs_coverage_multiple_source_files_to_single_output_fixture_set_up,
        gwkjs_coverage_multiple_source_files_to_single_output_fixture_tear_down
    };

    add_test_for_fixture("/gwkjs/coverage/multiple_source_file_records_written_to_coverage_data",
                         &coverage_for_multiple_files_to_single_output_fixture,
                         test_multiple_source_file_records_written_to_coverage_data,
                         NULL);
    add_test_for_fixture("/gwkjs/coverage/correct_line_coverage_data_written_for_both_sections",
                         &coverage_for_multiple_files_to_single_output_fixture,
                         test_correct_line_coverage_data_written_for_both_source_file_sectons,
                         NULL);

    FixturedTest coverage_cache_fixture = {
        sizeof(GwkjsCoverageCacheFixture),
        gwkjs_coverage_cache_fixture_set_up,
        gwkjs_coverage_cache_fixture_tear_down
    };

    /* This must be static, because g_test_add_vtable does not copy it */
    static GwkjsCoverageCacheObjectNotationTableTestData data_in_expected_format_table[] = {
        {
            "simple_executable_lines",
            "let i = 0;\n",
            "resource://org/gnome/gwkjs/mock/test/gwkjs-test-coverage/cache_notation/simple_executable_lines.js",
            "1",
            "",
            ""
        },
        {
            "simple_branch",
            "let i = 0;\n"
            "if (i) {\n"
            "    i = 1;\n"
            "} else {\n"
            "    i = 2;\n"
            "}\n",
            "resource://org/gnome/gwkjs/mock/test/gwkjs-test-coverage/cache_notation/simple_branch.js",
            "1,2,3,5",
            "\"point\":2,\"exits\":[3,5]",
            ""
        },
        {
            "simple_function",
            "function f() {\n"
            "}\n",
            "resource://org/gnome/gwkjs/mock/test/gwkjs-test-coverage/cache_notation/simple_function.js",
            "1,2",
            "",
            "\"key\":\"f:1:0\",\"line\":1"
        }
    };

    add_table_driven_test_for_fixture("/gwkjs/coverage/cache/data_format",
                                      &coverage_cache_fixture,
                                      test_coverage_cache_data_in_expected_format,
                                      sizeof(GwkjsCoverageCacheObjectNotationTableTestData),
                                      G_N_ELEMENTS(data_in_expected_format_table),
                                      (const TestTableDataHeader *) data_in_expected_format_table);

    add_table_driven_test_for_fixture("/gwkjs/coverage/cache/data_format_resource",
                                      &coverage_cache_fixture,
                                      test_coverage_cache_data_in_expected_format_resource,
                                      sizeof(GwkjsCoverageCacheObjectNotationTableTestData),
                                      G_N_ELEMENTS(data_in_expected_format_table),
                                      (const TestTableDataHeader *) data_in_expected_format_table);

    static GwkjsCoverageCacheJSObjectTableTestData object_has_expected_properties_table[] = {
        {
            "simple_executable_lines",
            "let i = 0;\n",
            "assertArrayEquals(JSON.parse(coverage_cache)[covered_script_filename].lines, [1]);\n"
        },
        {
            "simple_branch",
            "let i = 0;\n"
            "if (i) {\n"
            "    i = 1;\n"
            "} else {\n"
            "    i = 2;\n"
            "}\n",
            "JSUnit.assertEquals(2, JSON.parse(coverage_cache)[covered_script_filename].branches[0].point);\n"
            "assertArrayEquals([3, 5], JSON.parse(coverage_cache)[covered_script_filename].branches[0].exits);\n"
        },
        {
            "simple_function",
            "function f() {\n"
            "}\n",
            "JSUnit.assertEquals('f:1:0', JSON.parse(coverage_cache)[covered_script_filename].functions[0].key);\n"
        }
    };

    add_table_driven_test_for_fixture("/gwkjs/coverage/cache/object_props",
                                      &coverage_cache_fixture,
                                      test_coverage_cache_as_js_object_has_expected_properties,
                                      sizeof(GwkjsCoverageCacheJSObjectTableTestData),
                                      G_N_ELEMENTS(object_has_expected_properties_table),
                                      (const TestTableDataHeader *) object_has_expected_properties_table);

    static GwkjsCoverageCacheEqualResultsTableTestData equal_results_table[] = {
        {
            "simple_executable_lines",
            "let i = 0;\n"
            "let j = 1;\n"
        },
        {
            "simple_branch",
            "let i = 0;\n"
            "if (i) {\n"
            "    i = 1;\n"
            "} else {\n"
            "    i = 2;\n"
            "}\n"
        },
        {
            "simple_function",
            "function f() {\n"
            "}\n"
        }
    };

    add_table_driven_test_for_fixture("/gwkjs/coverage/cache/equal/executable_lines",
                                      &coverage_cache_fixture,
                                      test_coverage_cache_equal_results_to_reflect_parse,
                                      sizeof(GwkjsCoverageCacheEqualResultsTableTestData),
                                      G_N_ELEMENTS(equal_results_table),
                                      (const TestTableDataHeader *) equal_results_table);

    add_test_for_fixture("/gwkjs/coverage/cache/invalidation",
                         &coverage_cache_fixture,
                         test_coverage_cache_invalidation,
                         NULL);

    add_test_for_fixture("/gwkjs/coverage/cache/invalidation_resource",
                         &coverage_cache_fixture,
                         test_coverage_cache_invalidation_resource,
                         NULL);

    add_test_for_fixture("/gwkjs/coverage/cache/file_written",
                         &coverage_cache_fixture,
                         test_coverage_cache_file_written_when_no_cache_exists,
                         NULL);

    add_test_for_fixture("/gwkjs/coverage/cache/no_update_on_full_hits",
                         &coverage_cache_fixture,
                         test_coverage_cache_not_updated_on_full_hits,
                         NULL);

    add_test_for_fixture("/gwkjs/coverage/cache/update_on_misses",
                         &coverage_cache_fixture,
                         test_coverage_cache_updated_when_cache_stale,
                         NULL);
}
