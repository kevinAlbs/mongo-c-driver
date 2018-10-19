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


#include "mongoc/mongoc-config.h"
#include "mongoc/mongoc-collection-private.h"
#include "mongoc/mongoc-host-list-private.h"
#include "mongoc/mongoc-server-description-private.h"
#include "mongoc/mongoc-topology-description-private.h"
#include "mongoc/mongoc-topology-private.h"
#include "mongoc/mongoc-util-private.h"
#include "mongoc/mongoc-util-private.h"

#include "TestSuite.h"
#include "test-conveniences.h"

#include "json-test.h"
#include "json-test-operations.h"
#include "test-libmongoc.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <dirent.h>
#endif

#ifdef BSON_HAVE_STRINGS_H
#include <strings.h>
#endif

static bool
ends_with (const char *s, const char *suffix)
{
   size_t s_len;
   size_t suffix_len;

   if (!s) {
      return false;
   }

   s_len = strlen (s);
   suffix_len = strlen (suffix);
   return s_len >= suffix_len && !strcmp (s + s_len - suffix_len, suffix);
}

/* test that an event's "host" field is set to a reasonable value */
static void
assert_host_in_uri (const mongoc_host_list_t *host, const mongoc_uri_t *uri)
{
   const mongoc_host_list_t *hosts;

   hosts = mongoc_uri_get_hosts (uri);
   while (hosts) {
      if (_mongoc_host_list_equal (hosts, host)) {
         return;
      }

      hosts = hosts->next;
   }

   fprintf (stderr,
            "Host \"%s\" not in \"%s\"",
            host->host_and_port,
            mongoc_uri_get_string (uri));
   fflush (stderr);
   abort ();
}


static void
started_cb (const mongoc_apm_command_started_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_started_get_context (event);
   char *cmd_json;
   bson_t *events = &ctx->events;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      cmd_json = bson_as_canonical_extended_json (event->command, NULL);
      printf ("%s\n", cmd_json);
      fflush (stdout);
      bson_free (cmd_json);
   }

   BSON_ASSERT (mongoc_apm_command_started_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_started_get_server_id (event) > 0);
   /* check that event->host is sane */
   assert_host_in_uri (event->host, ctx->test_framework_uri);
   new_event = BCON_NEW ("command_started_event",
                         "{",
                         "command",
                         BCON_DOCUMENT (event->command),
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "database_name",
                         BCON_UTF8 (event->database_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
}


static void
succeeded_cb (const mongoc_apm_command_succeeded_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_succeeded_get_context (event);
   char *reply_json;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      reply_json = bson_as_canonical_extended_json (event->reply, NULL);
      printf ("\t\t<-- %s\n", reply_json);
      fflush (stdout);
      bson_free (reply_json);
   }

   BSON_ASSERT (mongoc_apm_command_succeeded_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_succeeded_get_server_id (event) > 0);
   assert_host_in_uri (event->host, ctx->test_framework_uri);
   new_event = BCON_NEW ("command_succeeded_event",
                         "{",
                         "reply",
                         BCON_DOCUMENT (event->reply),
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (&ctx->events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
}


static void
failed_cb (const mongoc_apm_command_failed_t *event)
{
   json_test_ctx_t *ctx =
      (json_test_ctx_t *) mongoc_apm_command_failed_get_context (event);
   bson_t reply = BSON_INITIALIZER;
   char str[16];
   const char *key;
   bson_t *new_event;

   if (ctx->verbose) {
      printf (
         "\t\t<-- %s FAILED: %s\n", event->command_name, event->error->message);
      fflush (stdout);
   }

   BSON_ASSERT (mongoc_apm_command_failed_get_request_id (event) > 0);
   BSON_ASSERT (mongoc_apm_command_failed_get_server_id (event) > 0);
   assert_host_in_uri (event->host, ctx->test_framework_uri);

   new_event = BCON_NEW ("command_failed_event",
                         "{",
                         "command_name",
                         BCON_UTF8 (event->command_name),
                         "operation_id",
                         BCON_INT64 (event->operation_id),
                         "}");

   bson_uint32_to_string (ctx->n_events, &key, str, sizeof str);
   BSON_APPEND_DOCUMENT (&ctx->events, key, new_event);

   ctx->n_events++;

   bson_destroy (new_event);
   bson_destroy (&reply);
}


void
set_apm_callbacks (json_test_ctx_t *ctx, mongoc_client_t *client)
{
   mongoc_apm_callbacks_t *callbacks;

   callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, started_cb);

   if (!ctx->config->command_started_events_only) {
      mongoc_apm_set_command_succeeded_cb (callbacks, succeeded_cb);
      mongoc_apm_set_command_failed_cb (callbacks, failed_cb);
   }

   mongoc_client_set_apm_callbacks (client, callbacks, ctx);
   mongoc_apm_callbacks_destroy (callbacks);
}


static bool
lsids_match (const bson_t *a, const bson_t *b)
{
   /* need a match context in case lsids DON'T match, since match_bson() without
    * context aborts on mismatch */
   char errmsg[1000];
   match_ctx_t ctx = {0};
   ctx.errmsg = errmsg;
   ctx.errmsg_len = sizeof (errmsg);

   return match_bson_with_ctx (a, b, &ctx);
}


static match_action_t
apm_match_visitor (match_ctx_t *ctx,
                   bson_iter_t *pattern_iter,
                   bson_iter_t *doc_iter)
{
   const char *key = bson_iter_key (pattern_iter);
   json_test_ctx_t *test_ctx = (json_test_ctx_t *) ctx->visitor_ctx;

#define SHOULD_EXIST                          \
   do {                                       \
      if (!doc_iter) {                        \
         match_err (ctx, "expected %s", key); \
         return MATCH_ACTION_ABORT;           \
      }                                       \
   } while (0)
#define IS_COMMAND(cmd) \
   (!strcmp (ctx->path, "") && ctx->is_command && !strcmp (key, cmd))

   if (IS_COMMAND ("find") || IS_COMMAND ("aggregate")) {
      /* New query. Next server reply or getMore will set cursor_id. */
      test_ctx->cursor_id = 0;
   } else if (!strcmp (key, "id") && ends_with (ctx->path, "cursor")) {
      test_ctx->cursor_id = bson_iter_as_int64 (doc_iter);
   } else if (!strcmp (key, "errmsg")) {
      /* "errmsg values of "" MUST assert that the value is not empty" */
      const char *errmsg = bson_iter_utf8 (pattern_iter, NULL);

      if (strcmp (errmsg, "") == 0) {
         if (!doc_iter || bson_iter_type (doc_iter) != BSON_TYPE_UTF8 ||
             strlen (bson_iter_utf8 (doc_iter, NULL)) == 0) {
            match_err (ctx, "expected non-empty 'errmsg'");
            return MATCH_ACTION_ABORT;
         }
         return MATCH_ACTION_SKIP;
      }
   } else if (IS_COMMAND ("getMore")) {
      /* "When encountering a cursor or getMore value of "42" in a test, the
       * driver MUST assert that the values are equal to each other and
       * greater than zero."
       */
      SHOULD_EXIST;
      if (test_ctx->cursor_id == 0) {
         test_ctx->cursor_id = bson_iter_as_int64 (doc_iter);
      } else {
         if (test_ctx->cursor_id != bson_iter_as_int64 (doc_iter)) {
            match_err (ctx,
                       "cursor returned in getMore (%" PRId64
                       ") does not match previously seen (%" PRId64 ")",
                       bson_iter_as_int64 (doc_iter),
                       test_ctx->cursor_id);
            return MATCH_ACTION_ABORT;
         }
      }
   } else if (!strcmp (key, "lsid")) {
      const char *session_name = bson_iter_utf8 (pattern_iter, NULL);
      bson_t lsid;
      bool fail = false;

      SHOULD_EXIST;
      bson_iter_bson (doc_iter, &lsid);

      /* Transactions tests: "Each command-started event in "expectations"
       * includes an lsid with the value "session0" or "session1". Tests MUST
       * assert that the command's actual lsid matches the id of the correct
       * ClientSession named session0 or session1." */
      if (!strcmp (session_name, "session0") &&
          !lsids_match (&test_ctx->lsids[0], &lsid)) {
         fail = true;
      }

      if (!strcmp (session_name, "session1") &&
          !lsids_match (&test_ctx->lsids[1], &lsid)) {
         fail = true;
      }

      if (fail) {
         char *str = bson_as_json (&lsid, NULL);
         match_err (
            ctx, "expected %s, but used session: %s", session_name, str);
         bson_free (str);
         return MATCH_ACTION_ABORT;
      } else {
         return MATCH_ACTION_SKIP;
      }
   } else if (strstr (ctx->path, "updates.")) {
      /* tests expect "multi: false" and "upsert: false" explicitly;
      * we don't send them. fix when path is like "updates.0", "updates.1", ...
      */

      if (!strcmp (key, "multi") && !bson_iter_bool (pattern_iter)) {
         return MATCH_ACTION_SKIP;
      }
      if (!strcmp (key, "upsert") && !bson_iter_bool (pattern_iter)) {
         return MATCH_ACTION_SKIP;
      }
   } else if (!strcmp (ctx->command, "findAndModify") && !strcmp (key, "new")) {
      /* transaction tests expect "new: false" explicitly; we don't send it */
      return MATCH_ACTION_SKIP;
   }

   return MATCH_ACTION_CONTINUE;
}


typedef struct {
   int64_t operation_id;
   bson_t *command;
   bson_t *reply;
   char *command_name;
   char *database_name;
   char *type;
} apm_event_t;


static apm_event_t
apm_event_from_bson (const bson_t *bson)
{
   apm_event_t event = {0};
   bson_iter_t iter;

   bson_iter_init (&iter, bson);
   bson_iter_next (&iter);
   event.type = bson_strdup (bson_iter_key (&iter));
   bson_iter_recurse (&iter, &iter);
   while (bson_iter_next (&iter)) {
      if (!strcmp (bson_iter_key (&iter), "operation_id")) {
         event.operation_id = bson_iter_as_int64 (&iter);
      } else if (!strcmp (bson_iter_key (&iter), "database_name")) {
         event.database_name = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (!strcmp (bson_iter_key (&iter), "command_name")) {
         event.command_name = bson_strdup (bson_iter_utf8 (&iter, NULL));
      } else if (!strcmp (bson_iter_key (&iter), "reply")) {
         bson_t subdoc;
         bson_iter_bson (&iter, &subdoc);
         event.reply = bson_copy (&subdoc);
      } else if (!strcmp (bson_iter_key (&iter), "command")) {
         bson_t subdoc;
         bson_iter_bson (&iter, &subdoc);
         event.command = bson_copy (&subdoc);
      }
   }

   return event;
}


static bool
match_apm_event (const bson_t *actual_bson,
                 const bson_t *expected_bson,
                 match_ctx_t *match_ctx)
{
   apm_event_t actual = apm_event_from_bson (actual_bson);
   apm_event_t expected = apm_event_from_bson (expected_bson);

#define CHECK_STRING(field)                                               \
   do {                                                                   \
      if (expected.field && strcmp (actual.field, expected.field) != 0) { \
         match_err (match_ctx,                                            \
                    "got " #field " %s, expected %s\n",                   \
                    actual.field,                                         \
                    expected.field);                                      \
         return false;                                                    \
      }                                                                   \
   } while (0)

   CHECK_STRING (type);
   CHECK_STRING (command_name);
   CHECK_STRING (database_name);

   if (expected.operation_id && actual.operation_id != expected.operation_id) {
      match_err (match_ctx,
                 "got operation_id %" PRId64 ", expected %" PRId64 "\n",
                 actual.operation_id,
                 expected.operation_id);
      return false;
   }

   match_ctx->is_command = true;
   if (!match_bson_with_ctx (actual.command, expected.command, match_ctx)) {
      return false;
   }
   match_ctx->is_command = false;

   return match_bson_with_ctx (actual.reply, expected.reply, match_ctx);
}


/*
 *-----------------------------------------------------------------------
 *
 * check_json_apm_events --
 *
 *      Compare actual APM events with expected sequence. The two docs
 *      are each like:
 *
 * [
 *   {
 *     "command_started_event": {
 *       "command": { ... },
 *       "command_name": "count",
 *       "database_name": "command-monitoring-tests",
 *       "operation_id": 123
 *     }
 *   },
 *   {
 *     "command_failed_event": {
 *       "command_name": "count",
 *       "operation_id": 123
 *     }
 *   }
 * ]
 *
 *      If @allow_subset is true, then expectations is allowed to be
 *      a subset of events.
 *
 *-----------------------------------------------------------------------
 */
void
check_json_apm_events (json_test_ctx_t *ctx, const bson_t *expectations)
{
   bson_iter_t expectations_iter;
   bson_iter_t events_iter;
   bool allow_subset;
   match_ctx_t match_ctx;
   int i;
   char errmsg[1000] = {0};

   /* Old mongod returns a double for "count", newer returns int32.
    * Ignore this and other insignificant type differences. */
   match_ctx.strict_numeric_types = false;
   match_ctx.retain_dots_in_keys = true;
   match_ctx.errmsg = errmsg;
   match_ctx.errmsg_len = sizeof errmsg;
   match_ctx.allow_placeholders = true;
   match_ctx.visitor_fn = apm_match_visitor;
   match_ctx.visitor_ctx = (void *) ctx;

   allow_subset = ctx->config->command_monitoring_allow_subset;

   BSON_ASSERT (bson_iter_init (&expectations_iter, expectations));
   BSON_ASSERT (bson_iter_init (&events_iter, &ctx->events));
   i = 0;

   while (bson_iter_next (&expectations_iter)) {
      bson_t expectation;
      bson_iter_bson (&expectations_iter, &expectation);

      for (; i < ctx->n_events; i++) {
         bson_t event;

         bson_iter_next (&events_iter);
         bson_iter_bson (&events_iter, &event);

         if (match_apm_event (&event, &expectation, &match_ctx)) {
            bson_destroy (&event);
            break;
         }

         if (!allow_subset || i == ctx->n_events - 1) {
            test_error ("could not match APM event\n"
                        "\texpected: %s\n\n"
                        "\tactual  : %s\n\n"
                        "\terror   : %s\n\n",
                        bson_as_canonical_extended_json (&event, NULL),
                        bson_as_canonical_extended_json (&expectation, NULL),
                        match_ctx.errmsg);
         }
         bson_destroy (&event);
      }
      bson_destroy (&expectation);
   }
}

void
test_apm_matching (void)
{
   json_test_ctx_t test_ctx = {0};
   char errmsg[1000] = {0};
   match_ctx_t match_ctx = {0};
   const char *e1 = "{"
                    "  'command_succeeded_event': {"
                    "    'command_name': 'find',"
                    "    'reply': {'cursor': { 'id': 123 }}"
                    "  }"
                    "}";

   const char *e2 = "{"
                    "  'command_started_event': {"
                    "    'command_name': 'getMore',"
                    "    'command': {'getMore': 124}"
                    "  }"
                    "}";

   match_ctx.errmsg = errmsg;
   match_ctx.errmsg_len = sizeof errmsg;
   match_ctx.visitor_fn = apm_match_visitor;
   match_ctx.visitor_ctx = (void *) &test_ctx;

   /* match_apm_event must verify the cursor id returned in a getMore is the
    * same cursor id returned in a find reply. */
   BSON_ASSERT (match_apm_event (tmp_bson (e1), tmp_bson (e1), &match_ctx));
   BSON_ASSERT (!match_apm_event (tmp_bson (e2), tmp_bson (e2), &match_ctx));
   ASSERT_CONTAINS (match_ctx.errmsg, "cursor returned in getMore");
}

void
test_apm_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/apm_test_matching", test_apm_matching);
}
