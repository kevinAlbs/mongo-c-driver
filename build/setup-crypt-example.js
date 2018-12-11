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

print("Dropping admin.datakeys collection");
db = db.getSiblingDB("admin")
db.datakeys.drop();
db.datakeys.insert([
    {
        _id: uuids[0],
        keyMaterial: new BinData(0, keys[0]),
        creationDate: Date(),
        updatedDate: Date(),
        status: 1,
        masterKey: null /* not necessary */
    },
    {
        _id: uuids[1],
        keyAltName: ["Todd Davis"],
        keyMaterial: new BinData(0, keys[1]),
        creationDate: Date(),
        updatedDate: Date(),
        status: 1,
        masterKey: null /* not necessary */
    },
    {
        _id: uuids[2],
        keyMaterial: new BinData(0, keys[2]),
        creationDate: Date(),
        updatedDate: Date(),
        status: 0, /* disabled key */
        masterKey: null /* not necessary */
    }
]);