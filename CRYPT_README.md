Welcome to the experimental field-level encryption branch. Good luck.

## Setting up
Run:
```
$ mongo ./build/setup-crypt-example.js
```
This creates a simple key vault with some example keys and a collection named test.crypt, that has a JSONSchema with an encrypted 'ssn' field.

Start the mockupcryptd process. See [installation instructions here](https://github.com/mongodb-labs/mockupcryptd).
```
$ mockupcryptd
Listening with domain socket /tmp/mongocryptd.sock
URI is mongodb://%2Ftmp%2Fmongocryptd.sock
```

(TODO) Test out encryption!
```
$ mkdir cmake-build && cd cmake-build
$ cmake -DENABLE_SSL=OPENSSL -DENABLE_CRYPT_TRACING=ON ../
$ make -j8 example-crypt
$ ./src/libmongoc/example-crypt
Inserting { "name": "Todd Davis", "ssn": "457-55-5642" }
CRYPT_TRACE: encrypted collection detected
```
## Field-Level Encryption Support

Currently, encryption is enabled with a URI parameter:

```
mongoc_client_t *client = mongoc_client_new(
   "mongodb://localhost/db?encryption=true");
```

The current encryption is simplified to take one document at a time.

The "marking" is a BSON encoding of the document
```
{ 
  "v": <BSON value>,
  "k": <UUID|utf8>, /* key id */
  "iv": <binary>,
  "a": <utf8>,
  "u": <utf8>
}
```

Documents are encrypted with unencrypted metadata and an encrypted value e
```
{
  "e": <binary subtype 0, representing encryption of { "v": <value> }>,
  "k": <UUID>, /* key id */
  "iv": <binary>,
  "a": <utf8>,
  "u": <utf8>
}
```

The bytes representing this BSON are stored as a binary subtype 6.