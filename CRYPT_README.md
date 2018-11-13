This branch of mongo-c-driver includes experimental support for client side field-level encryption.

Currently, encryption is enabled with a URI parameter:

```
mongoc_client_t *client = mongoc_client_new(
   "mongodb://localhost/db?encryption=true");
```

The current encryption is simplified to take one document at a time.

The "marking" is a BSON encoding of the document
```
{ 
  "k": <utf8>, /* key id */
  "iv": <binary>,
  "v": <bson value>
}
```

Documents are encrypted with unencrypted metadata and an encrypted value e
```
{
  "k": <utf8>, /* key id */
  "iv": <binary>,
  "e": <binary subtype 0, representing encryption of { "v": <value> }>
}
```

The bytes representing this BSON are stored as a binary subtype 6.