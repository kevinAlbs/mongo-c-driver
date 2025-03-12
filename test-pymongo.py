from bson.binary import Binary, BinaryVectorDtype, VECTOR_SUBTYPE

# Pack one 1 bit with 7 bits of padding. The input data has 7 unused 1 bits:
vec = Binary.from_vector([0b11111111], BinaryVectorDtype.PACKED_BIT, padding=7)
assert vec == Binary(
    b"\x10"  # Header: PACKED_BIT 
    b"\x07"  # Padding: 7 bits
    b"\xff", # Data: 11111111 (last 7 unused)
    subtype=VECTOR_SUBTYPE,
)
