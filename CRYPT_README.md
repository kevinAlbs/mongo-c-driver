This branch of mongo-c-driver includes experimental support for client side field-level encryption.

Currently, encryption is enabled with a URI parameter:

```
mongoc_client_t *client = mongoc_client_new(
   "mongodb://localhost/db?encryption=true");
```

The current encryption is simplified to take one document at a time.

The "marking" is a BSON encoding of the document
```
{ "k": <int64>, "v": <bson value> }
```