#include <mongoc/mongoc.h>

#include "util.h"

#include <unistd.h>

static void
command_started (const mongoc_apm_command_started_t *event)
{
   char *s;

   s = bson_as_canonical_extended_json (
      mongoc_apm_command_started_get_command (event), NULL);
   MONGOC_INFO ("Command %s started on %s:\n%s\n\n",
           mongoc_apm_command_started_get_command_name (event),
           mongoc_apm_command_started_get_host (event)->host,
           s);

   bson_free (s);
}

static void
command_succeeded (const mongoc_apm_command_succeeded_t *event)
{
   char *s;

   s = bson_as_canonical_extended_json (
      mongoc_apm_command_succeeded_get_reply (event), NULL);
   MONGOC_INFO ("Command %s succeeded:\n%s\n\n",
           mongoc_apm_command_succeeded_get_command_name (event),
           s);

   bson_free (s);
}

static void
command_failed (const mongoc_apm_command_failed_t *event)
{
   bson_error_t error;

   mongoc_apm_command_failed_get_error (event, &error);
   MONGOC_INFO ("Command %s failed:\n\"%s\"\n\n",
           mongoc_apm_command_failed_get_command_name (event),
           error.message);
}

mongoc_apm_callbacks_t * util_log_callbacks_new (void) {
   mongoc_apm_callbacks_t *callbacks = mongoc_apm_callbacks_new ();
   mongoc_apm_set_command_started_cb (callbacks, command_started);
   mongoc_apm_set_command_succeeded_cb (callbacks, command_succeeded);
   mongoc_apm_set_command_failed_cb (callbacks, command_failed);
   return callbacks;
}

bson_t *util_args_parse (int argc, char** argv) {
   bson_json_reader_t *reader = NULL;
   bson_t *out = NULL;
   bson_t file_config = BSON_INITIALIZER;
   bson_iter_t iter;
   char *config_path = "config.json";
   bson_error_t error;
   
   if (access(config_path, F_OK) == 0) {
      reader = bson_json_reader_new_from_file (config_path, &error);
      if (!reader) {
         DIE ("reader error: %s", error.message);
      }
      if (-1 == bson_json_reader_read (reader, &file_config, &error)) {
         DIE ("read error: %s", error.message);
      }
   }

   if (argc % 2 != 1) {
      DIE ("CLI args should have this form: ./program key1 value1 key2 value2");
   }

   out = bson_new ();
   for (int i = 1; i < argc; i+=2) {
      BSON_APPEND_UTF8 (out, argv[i], argv[i+1]);
   }

   /* Use config.json values as defaults. */
   bson_iter_init (&iter, &file_config);
   while (bson_iter_next (&iter)) {
      if (!bson_has_field (out, bson_iter_key (&iter))) {
         BSON_APPEND_UTF8 (out, bson_iter_key (&iter), bson_iter_utf8 (&iter, NULL));
      }
   }

   bson_destroy (&file_config);
   bson_json_reader_destroy (reader);
   return out;
}

bool util_args_eq (bson_t* args, const char* key, const char* val) {
   bson_iter_t iter;

   if (!bson_iter_init_find (&iter, args, key)) {
      return false;
   }
   if (0 == strcmp (bson_iter_utf8 (&iter, NULL), val)) {
      return true;
   }
   return false;
}

const char* util_args_get (bson_t* args, const char* key) {
   bson_iter_t iter;

   if (!bson_iter_init_find (&iter, args, key)) {
      DIE ("arg not found: %s", key);
   }
   
   return bson_iter_utf8 (&iter, NULL);
}
