/*
 * Copyright 2019-present MongoDB, Inc.
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

#define MONGOC_LOG_DOMAIN "client-side-encryption"

#include "mongoc/mongoc-client-side-encryption-private.h"
#include "mongoc/mongoc-topology-private.h"

#ifndef _WIN32
#include <sys/wait.h>
#include <signal.h>
#endif

#include "mongoc/mongoc.h"

/* Auto encryption opts. */
struct _mongoc_auto_encryption_opts_t {
   /* key_vault_client and key_vault_client_pool is not owned and must outlive
    * auto encrypted client/pool. */
   mongoc_client_t *key_vault_client;
   mongoc_client_pool_t *key_vault_client_pool;
   char *db;
   char *coll;
   bson_t *kms_providers;
   bson_t *schema_map;
   bool bypass_auto_encryption;
   bson_t *extra;
};

mongoc_auto_encryption_opts_t *
mongoc_auto_encryption_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_auto_encryption_opts_t));
}

void
mongoc_auto_encryption_opts_destroy (mongoc_auto_encryption_opts_t *opts)
{
   if (!opts) {
      return;
   }
   bson_destroy (opts->extra);
   bson_destroy (opts->kms_providers);
   bson_destroy (opts->schema_map);
   bson_free (opts->db);
   bson_free (opts->coll);
   bson_free (opts);
}

void
mongoc_auto_encryption_opts_set_key_vault_client (
   mongoc_auto_encryption_opts_t *opts, mongoc_client_t *client)
{
   /* Does not own. */
   opts->key_vault_client = client;
}

void
mongoc_auto_encryption_opts_set_key_vault_client_pool (
   mongoc_auto_encryption_opts_t *opts, mongoc_client_pool_t *pool)
{
   /* Does not own. */
   opts->key_vault_client_pool = pool;
}

void
mongoc_auto_encryption_opts_set_key_vault_namespace (
   mongoc_auto_encryption_opts_t *opts, const char *db, const char *coll)
{
   bson_free (opts->db);
   opts->db = bson_strdup (db);
   bson_free (opts->coll);
   opts->coll = bson_strdup (coll);
}

void
mongoc_auto_encryption_opts_set_kms_providers (
   mongoc_auto_encryption_opts_t *opts, const bson_t *providers)
{
   bson_destroy (opts->kms_providers);
   opts->kms_providers = NULL;
   if (providers) {
      opts->kms_providers = bson_copy (providers);
   }
}

void
mongoc_auto_encryption_opts_set_schema_map (mongoc_auto_encryption_opts_t *opts,
                                            const bson_t *schema_map)
{
   bson_destroy (opts->schema_map);
   opts->schema_map = NULL;
   if (schema_map) {
      opts->schema_map = bson_copy (schema_map);
   }
}

void
mongoc_auto_encryption_opts_set_bypass_auto_encryption (
   mongoc_auto_encryption_opts_t *opts, bool bypass_auto_encryption)
{
   opts->bypass_auto_encryption = bypass_auto_encryption;
}

void
mongoc_auto_encryption_opts_set_extra (mongoc_auto_encryption_opts_t *opts,
                                       const bson_t *extra)
{
   bson_destroy (opts->extra);
   opts->extra = NULL;
   if (extra) {
      opts->extra = bson_copy (extra);
   }
}

struct _mongoc_client_encryption_opts_t {
   mongoc_client_t *key_vault_client;
   char *key_vault_db;
   char *key_vault_coll;
   bson_t *kms_providers;
};

mongoc_client_encryption_opts_t *
mongoc_client_encryption_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_client_encryption_opts_t));
}

void
mongoc_client_encryption_opts_destroy (mongoc_client_encryption_opts_t *opts)
{
   bson_free (opts->key_vault_db);
   bson_free (opts->key_vault_coll);
   bson_destroy (opts->kms_providers);
   bson_free (opts);
}

void
mongoc_client_encryption_opts_set_key_vault_client (
   mongoc_client_encryption_opts_t *opts, mongoc_client_t *key_vault_client)
{
   opts->key_vault_client = key_vault_client;
}

void
mongoc_client_encryption_opts_set_key_vault_namespace (
   mongoc_client_encryption_opts_t *opts,
   const char *key_vault_db,
   const char *key_vault_coll)
{
   bson_free (opts->key_vault_db);
   opts->key_vault_db = bson_strdup (key_vault_db);
   bson_free (opts->key_vault_coll);
   opts->key_vault_coll = bson_strdup (key_vault_coll);
}

void
mongoc_client_encryption_opts_set_kms_providers (
   mongoc_client_encryption_opts_t *opts, const bson_t *kms_providers)
{
   bson_destroy (opts->kms_providers);
   opts->kms_providers = NULL;
   if (kms_providers) {
      opts->kms_providers = bson_copy (kms_providers);
   }
}

struct _mongoc_client_encryption_datakey_opts_t {
   bson_t *masterkey;
   char **keyaltnames;
   uint32_t keyaltnames_count;
};

mongoc_client_encryption_datakey_opts_t *
mongoc_client_encryption_datakey_opts_new ()
{
   return bson_malloc0 (sizeof (mongoc_client_encryption_datakey_opts_t));
}

void
mongoc_client_encryption_datakey_opts_destroy (
   mongoc_client_encryption_datakey_opts_t *opts)
{
   if (!opts) {
      return;
   }

   bson_destroy (opts->masterkey);
   if (opts->keyaltnames) {
      int i;

      for (i = 0; i < opts->keyaltnames_count; i++) {
         bson_free (opts->keyaltnames[i]);
      }
      bson_free (opts->keyaltnames);
      opts->keyaltnames_count = 0;
   }

   bson_free (opts);
}

void
mongoc_client_encryption_datakey_opts_set_masterkey (
   mongoc_client_encryption_datakey_opts_t *opts, const bson_t *masterkey)
{
   bson_destroy (opts->masterkey);
   opts->masterkey = NULL;
   if (masterkey) {
      opts->masterkey = bson_copy (masterkey);
   }
}

void
mongoc_client_encryption_datakey_opts_set_keyaltnames (
   mongoc_client_encryption_datakey_opts_t *opts,
   char **keyaltnames,
   uint32_t keyaltnames_count)
{
   int i;

   if (opts->keyaltnames_count) {
      for (i = 0; i < opts->keyaltnames_count; i++) {
         bson_free (opts->keyaltnames[i]);
      }
      bson_free (opts->keyaltnames);
      opts->keyaltnames = NULL;
      opts->keyaltnames_count = 0;
   }

   if (keyaltnames_count) {
      opts->keyaltnames = bson_malloc (sizeof (char *) * keyaltnames_count);
      for (i = 0; i < keyaltnames_count; i++) {
         opts->keyaltnames[i] = bson_strdup (keyaltnames[i]);
      }
      opts->keyaltnames_count = keyaltnames_count;
   }
}

struct _mongoc_client_encryption_encrypt_opts_t {
   bson_value_t keyid;
   char *algorithm;
   char *keyaltname;
};

mongoc_client_encryption_encrypt_opts_t *
mongoc_client_encryption_encrypt_opts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_client_encryption_encrypt_opts_t));
}

void
mongoc_client_encryption_encrypt_opts_destroy (
   mongoc_client_encryption_encrypt_opts_t *opts)
{
   if (!opts) {
      return;
   }
   bson_value_destroy (&opts->keyid);
   bson_free (opts->algorithm);
   bson_free (opts->keyaltname);
   bson_free (opts);
}

void
mongoc_client_encryption_encrypt_opts_set_keyid (
   mongoc_client_encryption_encrypt_opts_t *opts, const bson_value_t *keyid)
{
   bson_value_destroy (&opts->keyid);
   memset (&opts->keyid, 0, sizeof (opts->keyid));
   if (keyid) {
      bson_value_copy (keyid, &opts->keyid);
   }
}

void
mongoc_client_encryption_encrypt_opts_set_keyaltname (
   mongoc_client_encryption_encrypt_opts_t *opts, const char *keyaltname)
{
   bson_free (opts->keyaltname);
   opts->keyaltname = NULL;
   if (keyaltname) {
      opts->keyaltname = bson_strdup (keyaltname);
   }
}

void
mongoc_client_encryption_encrypt_opts_set_algorithm (
   mongoc_client_encryption_encrypt_opts_t *opts, const char *algorithm)
{
   bson_free (opts->algorithm);
   opts->algorithm = NULL;
   if (algorithm) {
      opts->algorithm = bson_strdup (algorithm);
   }
}

#ifndef MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION

bool
_mongoc_cse_auto_encrypt (mongoc_client_t *client,
                          const mongoc_cmd_t *cmd,
                          mongoc_cmd_t *encrypted_cmd,
                          bson_t *encrypted,
                          bson_error_t *error)
{
   bson_init (encrypted);
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

bool
_mongoc_cse_auto_decrypt (mongoc_client_t *client,
                          const char *db_name,
                          const bson_t *reply,
                          bson_t *decrypted,
                          bson_error_t *error)
{
   bson_init (decrypted);
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

bool
_mongoc_cse_enable_auto_encryption (
   mongoc_client_t *client,
   mongoc_auto_encryption_opts_t *opts /* may be NULL */,
   bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

bool
_mongoc_topology_cse_enable_auto_encryption (
   mongoc_topology_t *topology,
   mongoc_auto_encryption_opts_t *opts /* may be NULL */,
   bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "libmongoc is not built with support for Client-Side Field "
                   "Level Encryption. Configure with "
                   "ENABLE_CLIENT_SIDE_ENCRYPTION=ON.");
   return false;
}

#else

#include <mongocrypt/mongocrypt.h>

#include "mongoc/mongoc-client-private.h"
#include "mongoc/mongoc-stream-private.h"
#include "mongoc/mongoc-host-list-private.h"
#include "mongoc/mongoc-trace-private.h"
#include "mongoc/mongoc-util-private.h"

/*--------------------------------------------------------------------------
 *
 * _prep_for_auto_encryption --
 *    If @cmd contains a type=1 payload (document sequence), convert it into
 *    a type=0 payload (array payload). See OP_MSG spec for details.
 *    Place the command BSON that should be encrypted into @out.
 *
 * Post-conditions:
 *    @out is initialized and set to the full payload. If @cmd did not include
 *    a type=1 payload, @out is statically initialized. Caller must not modify
 *    @out after, but must call bson_destroy.
 *
 * --------------------------------------------------------------------------
 */
static void
_prep_for_auto_encryption (const mongoc_cmd_t *cmd, bson_t *out)
{
   /* If there is no type=1 payload, return the command unchanged. */
   if (!cmd->payload || !cmd->payload_size) {
      bson_init_static (out, bson_get_data (cmd->command), cmd->command->len);
      return;
   }

   /* Otherwise, append the type=1 payload as an array. */
   bson_copy_to (cmd->command, out);
   _mongoc_cmd_append_payload_as_array (cmd, out);
}

mongoc_client_t *_get_mongocryptd_client (mongoc_client_t* client_encrypted) {
   if (client_encrypted->topology->single_threaded) {
      return  client_encrypted->mongocryptd_client;
   }
   return mongoc_client_pool_pop (client_encrypted->topology->mongocryptd_client_pool);
}

void _release_mongocryptd_client (mongoc_client_t* client_encrypted, mongoc_client_t* mongocryptd_client) {
   if (!mongocryptd_client) {
      return;
   }
   if (!client_encrypted->topology->single_threaded) {
      mongoc_client_pool_push (client_encrypted->topology->mongocryptd_client_pool, mongocryptd_client)
   }
}

mongoc_collection_t *_get_keyvault_coll (mongoc_client_t* client_encrypted) {
   mongoc_client_t *key_vault_client;
   const char* db;
   const char* coll;

   if (client_encrypted->topology->single_threaded) {
      key_vault_client = client_encrypted->keyvault_client;
      db = client_encrypted->key_vault_db;
      coll = client_encrypted->key_vault_coll;
   } else {
      key_vault_client = mongoc_client_pool_pop  (client_encrypted->topology->keyvault_client_pool);
      db = client_encrypted->topology->key_vault_db;
      coll = client_encrypted->topology->key_vault_coll;
   }
   return mongoc_client_get_collection (key_vault_client, db, coll);
}

void _release_keyvault_coll (mongoc_client_t* client_encrypted, mongoc_collection_t* keyvault_coll) {
   mongoc_client_t *keyvault_client;

   if (!keyvault_coll) {
      return;
   }

   keyvault_client =  keyvault_coll->client;
   mongoc_collection_destroy (keyvault_coll);
   if (!client_encrypted->topology->single_threaded) {
      mongoc_client_pool_push  (client_encrypted->topology->keyvault_client_pool, keyvault_client);
   }
}

/*--------------------------------------------------------------------------
 *
 * _mongoc_cse_auto_encrypt --
 *
 *       Perform automatic encryption if enabled.
 *
 * Return:
 *       True on success, false on error.
 *
 * Pre-conditions:
 *       CSE is enabled on client or its associated client pool.
 *
 * Post-conditions:
 *       If return false, @error is set. @encrypted is always initialized.
 *       @encrypted_cmd is set to the mongoc_cmd_t to send, which may refer
 *       to @encrypted.
 *       If automatic encryption was bypassed, @encrypted is set to an empty
 *       document, but @encrypted_cmd is a copy of @cmd. Caller must always
 *       bson_destroy @encrypted.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cse_auto_encrypt (mongoc_client_t *client,
                          const mongoc_cmd_t *cmd,
                          mongoc_cmd_t *encrypted_cmd,
                          bson_t *encrypted,
                          bson_error_t *error)
{
   bool ret = false;
   bson_t cmd_bson = BSON_INITIALIZER;
   bson_t *result = NULL;
   bson_iter_t iter;
   mongoc_client_t *mongocryptd_client = NULL;
   mongoc_collection_t *keyvault_coll = NULL;

   ENTRY;

   bson_init (encrypted);

   if (auto_encrypt->bypass_auto_encryption) {
      memcpy (encrypted_cmd, cmd, sizeof (mongoc_cmd_t));
      bson_destroy (&cmd_bson);
      return true;
   }

   if (!auto_encrypt->bypass_auto_encryption &&
       cmd->server_stream->sd->max_wire_version < WIRE_VERSION_CSE) {
      bson_set_error (
         error,
         MONGOC_ERROR_PROTOCOL,
         MONGOC_ERROR_PROTOCOL_BAD_WIRE_VERSION,
         "%s",
         "Auto-encryption requires a minimum MongoDB version of 4.2");
      goto fail;
   }

   /* Construct the command we're sending to libmongocrypt. If cmd includes a
    * type 1 payload, convert it to a type 0 payload. */
   bson_destroy (&cmd_bson);
   _prep_for_auto_encryption (cmd, &cmd_bson);
   bson_destroy (&encrypted);
   keyvault_coll =  _get_keyvault_coll (client);
   mongocryptd_client =  _get_mongocryptd_client (client);
   if (!_mongoc_crypt_auto_encrypt (client->crypt, keyvault_coll, mongocryptd_client, client, cmd->db_name, &cmd_bson, encrypted, error)) {
      goto fail;
   }


   /* Re-append $db if encryption stripped it. */
   if (!bson_iter_init_find (&iter, encrypted, "$db")) {
      BSON_APPEND_UTF8 (encrypted, "$db", cmd->db_name);
   }

   /* Create the modified cmd_t. */
   memcpy (encrypted_cmd, cmd, sizeof (mongoc_cmd_t));
   /* Modify the mongoc_cmd_t and clear the payload, since
    * _mongoc_cse_auto_encrypt converted the payload into an embedded array. */
   encrypted_cmd->payload = NULL;
   encrypted_cmd->payload_size = 0;
   encrypted_cmd->command = encrypted;

   ret = true;

fail:
   bson_destroy (result);
   bson_destroy (&cmd_bson);
   _release_mongocryptd_client (client, mongocryptd_client);
   _release_keyvault_coll (client, keyvault_coll);
   RETURN (ret);
}

/*--------------------------------------------------------------------------
 *
 * _mongoc_cse_auto_decrypt --
 *
 *       Perform automatic decryption.
 *
 * Return:
 *       True on success, false on error.
 *
 * Pre-conditions:
 *       FLE is enabled on client.
 *
 * Post-conditions:
 *       If return false, @error is set. @decrypted is always initialized.
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_cse_auto_decrypt (mongoc_client_t *client,
                          const char *db_name,
                          const bson_t *reply,
                          bson_t *decrypted,
                          bson_error_t *error)
{
   bool ret = false;
   bson_t *result = NULL;
   mongoc_collection_t* keyvault_coll =  NULL;

   ENTRY;

   keyvault_coll = _get_keyvault_coll (client);
   if (!_mongoc_crypt_auto_decrypt (client->crypt,
                            key_vault_coll,
                            reply,
                            decrypted,
                            error)) {
                               goto fail;
                            }

   ret = true;

fail:
   _release_keyvault_coll (client, keyvault_coll);
   RETURN (ret);
}

static void
_uri_construction_error (bson_error_t *error)
{
   bson_set_error (error,
                   MONGOC_ERROR_CLIENT,
                   MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                   "Error constructing URI to mongocryptd");
}


#ifdef _WIN32
static bool
_do_spawn (const char *path, char **args, bson_error_t *error)
{
   bson_string_t *command;
   char **arg;
   PROCESS_INFORMATION process_information;
   STARTUPINFO startup_info;

   /* Construct the full command, quote path and arguments. */
   command = bson_string_new ("");
   bson_string_append (command, "\"");
   if (path) {
      bson_string_append (command, path);
   }
   bson_string_append (command, "mongocryptd.exe");
   bson_string_append (command, "\"");
   /* skip the "mongocryptd" first arg. */
   arg = args + 1;
   while (*arg) {
      bson_string_append (command, " \"");
      bson_string_append (command, *arg);
      bson_string_append (command, "\"");
      arg++;
   }

   ZeroMemory (&process_information, sizeof (process_information));
   ZeroMemory (&startup_info, sizeof (startup_info));

   startup_info.cb = sizeof (startup_info);

   if (!CreateProcessA (NULL,
                        command->str,
                        NULL,
                        NULL,
                        false /* inherit descriptors */,
                        DETACHED_PROCESS /* FLAGS */,
                        NULL /* environment */,
                        NULL /* current directory */,
                        &startup_info,
                        &process_information)) {
      long lastError = GetLastError ();
      LPSTR message = NULL;

      FormatMessageA (
         FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL,
         lastError,
         0,
         (LPSTR) &message,
         0,
         NULL);

      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "failed to spawn mongocryptd: %s",
                      message);
      LocalFree (message);
      bson_string_free (command, true);
      return false;
   }

   bson_string_free (command, true);
   return true;
}
#else

/*--------------------------------------------------------------------------
 *
 * _do_spawn --
 *
 *   Spawn process defined by arg[0] on POSIX systems.
 *
 *   Note, if mongocryptd fails to spawn (due to not being found on the path),
 *   an error is not reported and true is returned. Users will get an error
 *   later, upon first attempt to use mongocryptd.
 *
 *   These comments refer to three distinct processes: parent, child, and
 *   mongocryptd.
 *   - parent is initial calling process
 *   - child is the first forked child. It fork-execs mongocryptd then
 *     terminates. This makes mongocryptd an orphan, making it immediately
 *     adopted by the init process.
 *   - mongocryptd is the final background daemon (grandchild process).
 *
 * Return:
 *   False if an error definitely occurred. Returns true if no reportable
 *   error occurred (though an error may have occurred in starting
 *   mongocryptd, resulting in the process not running).
 *
 * Arguments:
 *    args - A NULL terminated list of arguments. The first argument MUST
 *    be the name of the process to execute, and the last argument MUST be
 *    NULL.
 *
 * Post-conditions:
 *    If return false, @error is set.
 *
 *--------------------------------------------------------------------------
 */
static bool
_do_spawn (const char *path, char **args, bson_error_t *error)
{
   pid_t pid;
   int fd;
   char *to_exec;

   /* Fork. The child will terminate immediately (after fork-exec'ing
    * mongocryptd). This orphans mongocryptd, and allows parent to wait on
    * child. */
   pid = fork ();
   if (pid < 0) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "failed to fork (errno=%d) '%s'",
                      errno,
                      strerror (errno));
      return false;
   } else if (pid > 0) {
      int child_status;

      /* Child will spawn mongocryptd and immediately terminate to turn
       * mongocryptd into an orphan. */
      if (waitpid (pid, &child_status, 0 /* options */) < 0) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "failed to wait for child (errno=%d) '%s'",
                         errno,
                         strerror (errno));
         return false;
      }
      /* parent is done at this point, return. */
      return true;
   }

   /* We're no longer in the parent process. Errors encountered result in an
    * exit.
    * Note, we're not logging here, because that would require the user's log
    * callback to be fork-safe.
    */

   /* Start a new session for the child, so it is not bound to the current
    * session (e.g. terminal session). */
   if (setsid () < 0) {
      exit (EXIT_FAILURE);
   }

   /* Fork again. Child terminates so mongocryptd gets orphaned and immedately
    * adopted by init. */
   signal (SIGHUP, SIG_IGN);
   pid = fork ();
   if (pid < 0) {
      exit (EXIT_FAILURE);
   } else if (pid > 0) {
      /* Child terminates immediately. */
      exit (EXIT_SUCCESS);
   }

   /* TODO: Depending on the outcome of MONGOCRYPT-115, possibly change the
    * process's working directory with chdir like: `chdir (default_pid_path)`.
    * Currently pid file ends up in application's working directory. */

   /* Set the user file creation mask to zero. */
   umask (0);

   /* Close and reopen stdin. */
   fd = open ("/dev/null", O_RDONLY);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   dup2 (fd, STDIN_FILENO);
   close (fd);

   /* Close and reopen stdout. */
   fd = open ("/dev/null", O_WRONLY);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   if (dup2 (fd, STDOUT_FILENO) < 0 || close (fd) < 0) {
      exit (EXIT_FAILURE);
   }

   /* Close and reopen stderr. */
   fd = open ("/dev/null", O_RDWR);
   if (fd < 0) {
      exit (EXIT_FAILURE);
   }
   if (dup2 (fd, STDERR_FILENO) < 0 || close (fd) < 0) {
      exit (EXIT_FAILURE);
   }
   fd = 0;

   if (path) {
      to_exec = bson_strdup_printf ("%s%s", path, args[0]);
   } else {
      to_exec = bson_strdup (args[0]);
   }
   if (execvp (to_exec, args) < 0) {
      /* Need to exit. */
      exit (EXIT_FAILURE);
   }

   /* Will never execute. */
   return false;
}
#endif

/* Initial state shared when enabling automatic encryption on pooled and single
 * threaded clients
 */
typedef struct {
   mongoc_uri_t *mongocryptd_uri;
   bool mongocryptd_bypass_spawn;
   const char *mongocryptd_spawn_path;
   bson_iter_t mongocryptd_spawn_args;
   bool has_spawn_args;
} _extra_parsed_t;

static _extra_parsed_t *
_extra_parsed_new (void)
{
   return bson_malloc0 (sizeof (_extra_parsed_t));
}

static void
_extra_parsed_destroy (_extra_parsed_t *extra_parsed)
{
   if (!extra_parsed) {
      return;
   }
   mongoc_uri_destroy (extra_parsed->mongocryptd_uri);
   bson_free (extra_parsed);
}

static bool
_extra_parsed_init (const bson_t* extra,
                    _extra_parsed_t *extra_parsed,
                    bson_error_t *error)
{
   bson_iter_t iter;
   bool ret = false;
   mongoc_uri_t *mongocryptd_uri = NULL;

   ENTRY;

   if (!extra) {
      return true;
   }

   extra_parsed->bypass_auto_encryption = opts->bypass_auto_encryption;

   if (!extra_parsed->bypass_auto_encryption) {
      /* Spawn mongocryptd if needed, and create a client to it. */

      if (opts->extra) {
         if (bson_iter_init_find (
                &iter, opts->extra, "mongocryptdBypassSpawn") &&
             bson_iter_as_bool (&iter)) {
            extra_parsed->mongocryptd_bypass_spawn = true;
         }
         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdSpawnPath") &&
             BSON_ITER_HOLDS_UTF8 (&iter)) {
            extra_parsed->mongocryptd_spawn_path = bson_iter_utf8 (&iter, NULL);
         }
         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdSpawnArgs") &&
             BSON_ITER_HOLDS_ARRAY (&iter)) {
            memcpy (&extra_parsed->mongocryptd_spawn_args,
                    &iter,
                    sizeof (bson_iter_t));
            extra_parsed->has_spawn_args = true;
         }

         if (bson_iter_init_find (&iter, opts->extra, "mongocryptdURI")) {
            if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
               bson_set_error (error,
                               MONGOC_ERROR_CLIENT,
                               MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                               "Expected string for option 'mongocryptdURI'");
               goto fail;
            }
            extra_parsed->mongocryptd_uri =
               mongoc_uri_new_with_error (bson_iter_utf8 (&iter, NULL), error);
            if (!mongocryptd_uri) {
               goto fail;
            }
         }
      }

      if (!extra_parsed->mongocryptd_uri) {
         /* Always default to connecting to TCP, despite spec v1.0.0. Because
          * starting mongocryptd when one is running removes the domain socket
          * file per SERVER-41029. Connecting over TCP is more reliable.
          */
         extra_parsed->mongocryptd_uri =
            mongoc_uri_new_with_error ("mongodb://localhost:27020", error);

         if (!extra_parsed->mongocryptd_uri) {
            goto fail;
         }

         if (!mongoc_uri_set_option_as_int32 (
                extra_parsed->mongocryptd_uri,
                MONGOC_URI_SERVERSELECTIONTIMEOUTMS,
                5000)) {
            _uri_construction_error (error);
            goto fail;
         }
      }
   }

   ret = true;
fail:
   RETURN (ret);
}


bool
_mongoc_cse_client_enable_auto_encryption (mongoc_client_t *client,
                                    mongoc_auto_encryption_opts_t *opts,
                                    bson_error_t *error)
{
   bool ret = false;
   _extra_parsed_t *extra_parsed = NULL;

   ENTRY;

   if (!client->topology->single_threaded) {
      bson_set_error (
         error,
         MONGOC_ERROR_CLIENT,
         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
         "Automatic encryption on pooled clients must be set on the pool");
      GOTO (fail);
   }

   if (client->cse_enabled) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "Automatic encryption already set");
GOTO (fail);
   }

   if (!opts) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Auto encryption options required");
GOTO (fail);
   }

   if (opts->key_vault_client_pool) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "The key vault client pool only applies to a client "
                      "pool, not a single threaded client");
GOTO (fail);
   }

   /* Check for required options */
   if (!opts->db || !opts->coll) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Key vault namespace option required");
GOTO (fail);
   }

   if (!opts->kms_providers) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "KMS providers option required");
GOTO (fail);
   }

   extra_parsed = _extra_parsed_new ();
   if (!_extra_parsed_init (opts->extra, extra_parsed, error)) {
      GOTO (fail);
   }

   client->crypt = _mongoc_crypt_new (opts->kms_providers, opts->schema_map, error);
   if (!client->crypt) {
      goto fail;
   }
   client->cse_enabled = true;
   client->bypass_auto_encryption = opts->bypass_auto_encryption;

   if (!client->bypass_auto_encryption) {
      if (!extra_parsed->mongocryptd_bypass_spawn) {
         if (!_spawn_mongocryptd (extra_parsed->mongocryptd_spawn_path,
                                  extra_parsed->has_spawn_args
                                     ? &extra_parsed->mongocryptd_spawn_args
                                     : NULL,
                                  error)) {
            goto fail;
         }
      }

      /* By default, single threaded clients set serverSelectionTryOnce to
       * true, which means server selection fails if a topology scan fails
       * the first time (i.e. it will not make repeat attempts until
       * serverSelectionTimeoutMS expires). Override this, since the first
       * attempt to connect to mongocryptd may fail when spawning, as it
       * takes some time for mongocryptd to listen on sockets. */
      if (!mongoc_uri_set_option_as_bool (extra_parsed->mongocryptd_uri,
                                          MONGOC_URI_SERVERSELECTIONTRYONCE,
                                          false)) {
         _uri_construction_error (error);
         goto fail;
      }

      client->mongocryptd_client =
         mongoc_client_new_from_uri (extra_parsed->mongocryptd_uri);

      if (!client->mongocryptd_client) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "Unable to create client to mongocryptd");
         goto fail;
      }
      /* Similarly, single threaded clients will by default wait for 5 second
       * cooldown period after failing to connect to a server before making
       * another attempt. Meaning if the first attempt to mongocryptd fails
       * to connect, then the user observes a 5 second delay. This is not
       * configurable in the URI, so override. */
      _mongoc_topology_bypass_cooldown (client->mongocryptd_client->topology);
   }

   client->key_vault_db = bson_strdup (opts->db);
   client->key_vault_coll = bson_strdup (opts->coll);
   if (opts->key_vault_client) {
      client->key_vault_client = opts->key_vault_client;
   }

   ret = true;
fail:
   _extra_parsed_destroy (extra_parsed);
   RETURN (ret);
}

bool
_mongoc_topology_cse_enable_auto_encryption (
   mongoc_topology_t *topology,
   mongoc_auto_encryption_opts_t *opts,
   bson_error_t *error)
{
   bool ret = false;
   _extra_parsed_t *extra_parsed = NULL;

   if (topology->cse_enabled) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "Automatic encryption already set");
      goto fail;
   }

   if (!opts) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Auto encryption options required");
      goto fail;
   }

   if (opts->key_vault_client) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "The key vault client only applies to a single threaded "
                      "client not a single threaded client. Set a key vault "
                      "client pool");
      goto fail;
   }

   /* Check for required options */
   if (!opts->db || !opts->coll) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Key vault namespace option required");
      goto fail;
   }

   if (!opts->kms_providers) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "KMS providers option required");
      goto fail;
   }

   /* TODO: lock the mutex? */
   extra_parsed = _extra_parsed_init_new ();
   if (!_extra_parsed_init (opts, extra_parsed, error)) {
      goto fail;
   }

   topology->crypt = _mongoc_crypt_new (opts->kms_providers, opts->schema_map, error);
   if (!topology->crypt) {
      goto fail;
   }
   topology->cse_enabled = true;
   topology->bypass_auto_encryption = opts->bypass_auto_encryption;

   if (!topology->bypass_auto_encryption) {
      if (!extra_parsed->mongocryptd_bypass_spawn) {
         if (!_spawn_mongocryptd (extra_parsed->mongocryptd_spawn_path,
                                  extra_parsed->has_spawn_args
                                     ? &extra_parsed->mongocryptd_spawn_args
                                     : NULL,
                                  error)) {
            goto fail;
         }
      }

      topology->mongocryptd_client_pool =
         mongoc_client_pool_new (extra_parsed->mongocryptd_uri);

      if (!topology->mongocryptd_client_pool) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "Unable to create client pool to mongocryptd");
         goto fail;
      }
   }

   topology->key_vault_db = bson_strdup (opts->db);
   topology->key_vault_coll = bson_strdup (opts->coll);
   if (opts->key_vault_client_pool) {
      topology->key_vault_client_pool = opts->key_vault_client_pool;
   }

   ret = true;
fail:
   _extra_parsed_init_destroy (extra_parsed);
   return ret;
}



struct _mongoc_client_encryption_t {
   mongocrypt_t *crypt;
   mongoc_client_t *key_vault_coll;
   bson_t *kms_providers;
};

mongoc_client_encryption_t *
mongoc_client_encryption_new (mongoc_client_encryption_opts_t *opts,
                              bson_error_t *error)
{
   mongoc_client_encryption_t *client_encryption = NULL;
   bool success = false;

   /* Check for required options */
   if (!opts->key_vault_client || !opts->key_vault_db || !opts->key_vault_coll) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "Key vault client and namespace option required");
      goto fail;
   }

   if (!opts->kms_providers) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_ARG,
                      "KMS providers option required");
      goto fail;
   }


   client_encryption = bson_malloc0 (sizeof (*client_encryption));
   client_encryption->key_vault_coll = mongoc_client_get_collection (opts->key_vault_client, opts->key_vault_db, opts->key_vault_coll);
   client_encryption->kms_providers = bson_copy (opts->kms_providers);
   client_encryption->crypt = _mongoc_crypt_new (opts->kms_providers, NULL /* schema_map */, error);
   if (!client_encryption->crypt) {
      goto fail;
   }
   success = true;

fail:
   if (!success) {
      mongoc_client_encryption_destroy (client_encryption);
      return NULL;
   }
   return client_encryption;
}

void
mongoc_client_encryption_destroy (mongoc_client_encryption_t *client_encryption)
{
   _mongoc_crypt_destroy (client_encryption->crypt);
   mongoc_collection_destroy (client_encryption->key_vault_coll);
   bson_destroy (client_encryption->kms_providers);
   bson_free (client_encryption);
}

bool
mongoc_client_encryption_create_datakey (
   mongoc_client_encryption_t *client_encryption,
   const char *kms_provider,
   mongoc_client_encryption_datakey_opts_t *opts,
   bson_value_t *keyid,
   bson_error_t *error)
{
   bool ret = false;
   bson_t datakey = BSON_INITIALIZER;
   mongoc_write_concern_t *wc = NULL;
   bson_t insert_opts = BSON_INITIALIZER;
   mongoc_collection_t *coll = NULL;

   ENTRY;

   /* reset, so it is safe for caller to call bson_value_destroy on error or
    * success. */
   if (keyid) {
      keyid->value_type = BSON_TYPE_EOD;
   }

   bson_destroy (datakey);
   if (!_mongoc_crypt_create_datakey (auto_encrypt->crypt, crypt,
                               opts->masterkey,
                               opts->keyaltnames,
                               opts->keyaltnames_count,
                               datakey,
                               error) {
                                  goto fail;
                               }

   /* Insert the data key with write concern majority */
   wc = mongoc_write_concern_new ();
   mongoc_write_concern_set_wmajority (wc, 1000);
   coll = mongoc_client_get_collection (client_encryption->key_vault_client,
                                        client_encryption->key_vault_db,
                                        client_encryption->key_vault_coll);
   mongoc_collection_set_write_concern (coll, wc);
   ret = mongoc_collection_insert_one (
      coll, datakey, NULL /* opts */, NULL /* reply */, error);
   if (!ret) {
      goto fail;
   }

   if (keyid) {
      bson_iter_t iter;
      const bson_value_t *id_value;

      if (!bson_iter_init_find (&iter, datakey, "_id")) {
         bson_set_error (error,
                         MONGOC_ERROR_CLIENT,
                         MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                         "data key not did not contain _id");
         goto fail;
      } else {
         id_value = bson_iter_value (&iter);
         bson_value_copy (id_value, keyid);
      }
   }

   ret = true;

fail:
   bson_destroy (&insert_opts);
   bson_destroy (datakey);
   mongoc_write_concern_destroy (wc);
   mongoc_collection_destroy (coll);
   RETURN (ret);
}

bool
mongoc_client_encryption_encrypt (mongoc_client_encryption_t *client_encryption,
                                  bson_value_t *value,
                                  mongoc_client_encryption_encrypt_opts_t *opts,
                                  bson_value_t *ciphertext,
                                  bson_error_t *error)
{
   bool ret = false;

   ENTRY;

   /* reset, so it is safe for caller to call bson_value_destroy on error or
    * success. */
   ciphertext->value_type = BSON_TYPE_EOD;

   bson_destroy (&result);
   if (!_mongoc_crypt_explicit_encrypt (client_encryption->crypt,
                                client_encryption->key_vault_coll,
                                value_in,
                                ciphertext,
                                error)) {
      goto fail;
   }

   ret = true;
fail:
   RETURN (ret);
}

bool
mongoc_client_encryption_decrypt (mongoc_client_encryption_t *client_encryption,
                                  bson_value_t *ciphertext,
                                  bson_value_t *value,
                                  bson_error_t *error)
{
   bool ret = false;
   bson_t *to_decrypt_doc = NULL;
   mongocrypt_binary_t *to_decrypt_bin = NULL;
   bson_t result = BSON_INITIALIZER;
   bson_iter_t iter;

   ENTRY;

   /* reset, so it is safe for caller to call bson_value_destroy on error or
    * success. */
   if (value) {
      value->value_type = BSON_TYPE_EOD;
   }

   bson_destroy (result);
   if (!_mongoc_crypt_auto_decrypt (client_encryption->crypt,
                            client_encryption->key_vault_coll,
                            to_decrypt_doc,
                            &result,
                            error)) {
                               goto fail;
                            }
   if (!result) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "could not decrypt due to unknown error");
      goto fail;
   }

   /* extract value */
   if (!bson_iter_init_find (&iter, result, "v")) {
      bson_set_error (error,
                      MONGOC_ERROR_CLIENT,
                      MONGOC_ERROR_CLIENT_INVALID_ENCRYPTION_STATE,
                      "decrypted result unexpected");
      goto fail;
   } else {
      const bson_value_t *tmp;

      tmp = bson_iter_value (&iter);
      bson_value_copy (tmp, value);
   }

   ret = true;
fail:
   bson_destroy (to_decrypt_doc);
   mongocrypt_binary_destroy (to_decrypt_bin);
   bson_destroy (result);
   RETURN (ret);
}

#endif /* MONGOC_ENABLE_CLIENT_SIDE_ENCRYPTION */