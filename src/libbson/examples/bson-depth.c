/*
 * Copyright 2018-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* -- sphinx-include-start -- */
/* Reports the maximum nested depth of a BSON document. */
#include <bson/bson.h>
#include <stdio.h>
#include <assert.h>

typedef struct {
   uint32_t depth;
   uint32_t max_depth;
} check_depth_t;

bool
_check_depth_document (const bson_iter_t *iter,
                       const char *key,
                       const bson_t *v_document,
                       void *data);

static const bson_visitor_t check_depth_funcs = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   _check_depth_document,
   _check_depth_document,
   NULL,
};

bool
_check_depth_document (const bson_iter_t *iter,
                       const char *key,
                       const bson_t *v_document,
                       void *data)
{
   check_depth_t *state = (check_depth_t *) data;
   bson_iter_t child;

   if (!bson_iter_init (&child, v_document)) {
      fprintf (stderr, "corrupt\n");
      return true; /* cancel */
   }

   state->depth++;
   if (state->depth > state->max_depth) {
      state->max_depth = state->depth;
   }

   bson_iter_visit_all (&child, &check_depth_funcs, state);
   state->depth--;
   return false; /* continue */
}

void
print_depth (const bson_t *bson)
{
   bson_iter_t iter;
   check_depth_t state = {0};
   char *as_json;

   if (!bson_iter_init (&iter, bson)) {
      fprintf (stderr, "corrupt\n");
   }

   _check_depth_document (&iter, NULL, bson, &state);
   as_json = bson_as_canonical_extended_json (bson, NULL);
   printf ("document  : %s\n", as_json);
   printf ("max depth : %d\n\n", state.max_depth);
   bson_free (as_json);
}

int
main (int argc, char **argv)
{
   bson_reader_t *bson_reader;
   const bson_t *bson;
   bool reached_eof;
   char *filename;
   bson_error_t error;

   if (argc == 1) {
      fprintf (stderr, "usage: %s [--json] FILE\n", argv[0]);
      fprintf (stderr, "Computes the depth of the BSON/JSON contained in FILE.\n");
      fprintf (stderr, "FILE should contain valid BSON. If --json is passed, then "
                       "FILE should contain valid JSON.\n");
   }

   filename = argv[1];
   bson_reader = bson_reader_new_from_file (filename, &error);
   if (!bson_reader) {
      printf ("could not read %s: %s\n", filename, error.message);
      return 1;
   }

   while ((bson = bson_reader_read (bson_reader, &reached_eof))) {
      print_depth (bson);
   }

   if (!reached_eof) {
      printf ("error reading BSON\n");
   }

   bson_reader_destroy (bson_reader);
   return 0;
}