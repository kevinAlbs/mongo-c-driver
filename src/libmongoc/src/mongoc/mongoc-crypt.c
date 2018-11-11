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

#include "mongoc/mongoc.h"
#include "mongoc/mongoc-crypt-private.h"
#include "mongoc/mongoc-error.h"
#include "mongoc/mongoc-client-private.h"

typedef struct _mongoc_crypt_t {
   mongoc_client_t *mongocrypt_client;
} mongoc_crypt_t;

/* TODO: use a new error code. */
#define SET_CRYPT_ERR(...) bson_set_error (error, MONGOC_ERROR_CLIENT, MONGOC_ERROR_CLIENT_NOT_READY, __VA_ARGS__)

static void
_spawn_mongocryptd (void)
{
/* oddly, starting mongocryptd --daemonize in context of libmongoc will start
 * multiple instances. */
#ifdef SPAWN_BUG_FIXED
   pid_t pid = fork ();
   printf ("initializing mongocryptd\n");
   if (pid == 0) {
      int ret;
      /* child */
      printf ("child starting mongocryptd\n");
      ret = execlp ("mongocryptd", "mongocryptd", (char *) NULL);
      if (ret == -1) {
         MONGOC_ERROR ("child process unable to exec mongocryptd");
         abort ();
      }
   }
#endif
}


bool
mongoc_client_crypt_init (mongoc_client_t *client, bson_error_t *error)
{
   /* store AWS credentials, init structures in client, store schema
    * somewhere. */
   mongoc_client_t *mongocrypt_client;
   _spawn_mongocryptd ();
   client->encryption = bson_malloc0 (sizeof (mongoc_crypt_t));
   mongocrypt_client = mongoc_client_new ("mongodb://%2Ftmp%2Fmongocryptd.sock");
   if (!mongocrypt_client) {
      SET_CRYPT_ERR("Unable to create client to mongocryptd");
      return false;
   }
   client->encryption->mongocrypt_client = mongocrypt_client;
   return true;
}


static void _make_marking_cmd (const bson_t* data, bson_t* cmd) {
   bson_t child;

   bson_init (cmd);
   BSON_APPEND_INT64 (cmd, "markFields", 1);
   BSON_APPEND_ARRAY_BEGIN (cmd, "data", &child);
   BSON_APPEND_DOCUMENT (&child, "0", data);
   bson_append_array_end (cmd, &child);
   BSON_APPEND_DOCUMENT_BEGIN (cmd, "schema", &child);
   bson_append_document_end (cmd, &child);
}


static bool _replace_markings_recurse (bson_iter_t iter, bson_t* out) {
   while (bson_iter_next (&iter)) {
      printf("key=%s\n", bson_iter_key(&iter));
      if (BSON_ITER_HOLDS_BINARY(&iter)) {
         bson_subtype_t subtype;
         uint32_t binary_len;
         const uint8_t* data;
         bson_iter_binary (&iter, &subtype, &binary_len, &data);
         if (subtype == BSON_SUBTYPE_ENCRYPTED) {
            /* */
            bson_append_utf8(out, bson_iter_key(&iter), bson_iter_key_len(&iter), "ENCRYPTED", -1);
            continue;
         }
      }

      if (BSON_ITER_HOLDS_ARRAY(&iter)) {
         bson_iter_t child_iter;
         bson_t child_out;
         bool ret;

         bson_iter_recurse (&iter, &child_iter);
         bson_append_array_begin (out, bson_iter_key(&iter), bson_iter_key_len(&iter), &child_out);
         ret = _replace_markings_recurse (child_iter, &child_out);
         bson_append_array_end (out, &child_out);
         if (!ret) {
            return false;
         }
      } else if (BSON_ITER_HOLDS_DOCUMENT(&iter)) {
         bson_iter_t child_iter;
         bson_t child_out;
         bool ret;

         bson_iter_recurse (&iter, &child_iter);
         bson_append_document_begin (out, bson_iter_key(&iter), bson_iter_key_len(&iter), &child_out);
         ret = _replace_markings_recurse (child_iter, &child_out);
         bson_append_document_end (out, &child_out);
         if (!ret) {
            return false;
         }
      } else {
         bson_append_value (out, bson_iter_key(&iter), bson_iter_key_len (&iter), bson_iter_value (&iter));
      }
   }
   return true;
}


static bool _replace_markings (const bson_t* reply, bson_t* out, bson_error_t* error) {
   bson_iter_t iter;

   BSON_ASSERT (bson_iter_init_find (&iter, reply, "ok"));
   if (!bson_iter_as_bool (&iter)) {
      SET_CRYPT_ERR("markFields returned ok:0");
      return false;
   }

   if (!bson_iter_init_find (&iter, reply, "data")) {
      SET_CRYPT_ERR("markFields returned ok:0");
      return false;
   }
   /* recurse into array. */
   bson_iter_recurse (&iter, &iter);
   bson_iter_next (&iter);
   /* recurse into first document. */
   bson_iter_recurse (&iter, &iter);
   _replace_markings_recurse (iter, out);
   return true;
}


bool
mongoc_crypt_encrypt (mongoc_client_t *client,
                      const bson_t *data,
                      bson_t *out,
                      bson_error_t *error)
{
   bson_t cmd, reply;

   bool ret;
   ret = false;

   bson_init (out);

   if (!client->encryption) {
      return true;
   }

   _make_marking_cmd (data, &cmd);
   if (!mongoc_client_command_simple (client->encryption->mongocrypt_client,
                                 "admin",
                                 &cmd,
                                 NULL /* read prefs */,
                                 &reply,
                                 error)) {
      goto cleanup;
   }

   printf ("sent %s, got %s\n",
           bson_as_json (&cmd, NULL),
           bson_as_json (&reply, NULL));
   /* TODO: walk through BSON, and encrypt anything that was marked. */

   if (!_replace_markings (&reply, out, error)) {
      goto cleanup;
   }

   ret = true;
cleanup:
   bson_destroy (&cmd);
   bson_destroy (&reply);
   return ret;
}
