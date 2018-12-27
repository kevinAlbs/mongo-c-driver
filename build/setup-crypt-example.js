// Sets up an example key vault in mongod on the admin.datakeys collection.
// Some pregenerated UUIDs.
const uuids = [
    UUID("d7e9e25d-ac72-44be-8007-ac51cd4a7f13"),
    UUID("0e239681-7aa6-47dc-a473-8b98964bacf7"),
    UUID("6290d4d1-fd87-4dc5-aa03-8c7bee221a45")
];

// Keys are 32 bytes (currently) for AES_256_CBC_HMAC_SHA256.
// Currently, they are assumed UNENCRYPTED. TODO: change this after KMS integration.
const keys = [
    "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWE=", /* 'a' repeated 32 times */
    "YmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=", /* 'b' repeated 32 times */
    "Y2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2M=", /* 'c' repeated 32 times */
];

// TEMP: These are the keys above (in base64), after they have been encrypted with my CMK.
const wrapped_keys = [
    "AQICAHgg8xic3qACagcogG7tCsrU/az1q4j3Nt2hQcUyQRVMtQHVMeCvT16tqR4Lrx+YZZPNAAAAizCBiAYJKoZIhvcNAQcGoHsweQIBADB0BgkqhkiG9w0BBwEwHgYJYIZIAWUDBAEuMBEEDBgZBeKnJt8ciGCUvAIBEIBH8R8fYshXY1q/VrPGkiQs/+cv6gBCRR1tam+rIEGa2w2xO+Z24f/DcHfCkeVWuMSpGoyEov781YJo0iOE6Ptg0VynQNutuCw=",
    "AQICAHgg8xic3qACagcogG7tCsrU/az1q4j3Nt2hQcUyQRVMtQEEb7Fjw345bF3S/Mtl0KdVAAAAizCBiAYJKoZIhvcNAQcGoHsweQIBADB0BgkqhkiG9w0BBwEwHgYJYIZIAWUDBAEuMBEEDAm3EAPwy8J4dRanzQIBEIBHfe2CnDtIMMTy4EJ2onQ5yYxeKP2dPtZASeKxm2aQWYaWKxNgV0mXoxUXqQ5JDMTEZHAKPxouOaVR5FUVCl4jjas8+1zNFYM=",
    "AQICAHgg8xic3qACagcogG7tCsrU/az1q4j3Nt2hQcUyQRVMtQHaVfzryC4Lnu3rkL8c9gWSAAAAizCBiAYJKoZIhvcNAQcGoHsweQIBADB0BgkqhkiG9w0BBwEwHgYJYIZIAWUDBAEuMBEEDBfPk4bi+iEJ80fcWwIBEIBH5WxHC8QDjihAT3Tq242KRv9woyC2aTR/fA6BFTP8/KZOH36DEPG8v2oBUEYhgAgmRbVyDvj0cbZnis0dKIB5AGmlHdcdO1Q="
];

print("Dropping admin.datakeys collection");
db = db.getSiblingDB("admin")
db.datakeys.drop();
db.datakeys.insert([
    {
        _id: uuids[0],
        keyMaterial: new BinData(0, wrapped_keys[0]), // Temp: using wrapped keys.
        creationDate: Date(),
        updatedDate: Date(),
        status: 1,
        masterKey: null /* not necessary */
    },
    {
        _id: uuids[1],
        keyAltName: ["Todd Davis"],
        keyMaterial: new BinData(0, wrapped_keys[1]),
        creationDate: Date(),
        updatedDate: Date(),
        status: 1,
        masterKey: null /* not necessary */
    },
    {
        _id: uuids[2],
        keyMaterial: new BinData(0, wrapped_keys[2]),
        creationDate: Date(),
        updatedDate: Date(),
        status: 0, /* disabled key */
        masterKey: null /* not necessary */
    }
]);