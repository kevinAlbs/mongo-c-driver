//
// Created by Kevin Albertson on 9/8/17.
//

#include <mongoc.h>

int main() {
   mongoc_init();
   mongoc_client_t* client = mongoc_client_new ("mongodb://localhost:27017");
   bson_error_t err;
   char** names = mongoc_client_get_database_names(client, &err);
   if (err.domain) printf("Err: %s\n", err.message);
   for (int i = 0; names[i]; i++) printf("DB: \"%s\"\n", names[i]);
   bson_free(names);
   mongoc_client_destroy(client);
   mongoc_cleanup();
}

