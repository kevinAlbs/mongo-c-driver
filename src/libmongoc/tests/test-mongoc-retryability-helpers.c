#include "test-mongoc-retryability-helpers.h"

#include <bson/bson-error.h>

#include "test-conveniences.h"
#include "test-libmongoc.h"
#include "TestSuite.h"

#include <stddef.h>

test_get_two_mongos_clients_result_t
_test_get_two_mongos_clients (void)
{
   bson_error_t error = {0};
   mongoc_client_t *clients[2] = {0};

   // Sharded topology for tests should have mongos on port 27017 and 27018.
   const char *const ports[] = {"27017", "27018"};
   const size_t num_ports = sizeof (ports) / sizeof (*ports);

   for (size_t i = 0u; i < num_ports; ++i) {
      const char *const port = ports[i];

      char *const host_and_port =
         bson_strdup_printf ("mongodb://localhost:%s", port);
      char *const uri_str =
         test_framework_add_user_password_from_env (host_and_port);

      mongoc_uri_t *const uri = mongoc_uri_new_with_error (uri_str, &error);
      ASSERT_OR_PRINT (uri, error);

      mongoc_client_t *const client =
         mongoc_client_new_from_uri_with_error (uri, &error);
      ASSERT_OR_PRINT (client, error);
      test_framework_set_ssl_opts (client);

      {
         bson_t reply = BSON_INITIALIZER;

         ASSERT_OR_PRINT (
            mongoc_client_command_simple (client,
                                          "admin",
                                          tmp_bson ("{'hello': 1}"),
                                          NULL,
                                          &reply,
                                          &error),
            error);

         ASSERT_WITH_MSG (
            bson_has_field (&reply, "msg") &&
               strcmp (bson_lookup_utf8 (&reply, "msg"), "isdbgrid") == 0,
            "expected a mongos on port %s",
            port);

         bson_destroy (&reply);
      }

      clients[i] = client; // Ownership transfer.

      mongoc_uri_destroy (uri);

      bson_free (host_and_port);
      bson_free (uri_str);
   }

   return (test_get_two_mongos_clients_result_t){
      .s0 = clients[0],
      .s1 = clients[1],
   };
}