This branch of mongo-c-driver includes experimental support for client side field-level encryption.

Currently, encryption is enabled with a URI parameter:

```
mongoc_client_t *client = mongoc_client_new(
   "mongodb://localhost/db?encryption=true");
```

The current encryption is simplified to take on document at a time.