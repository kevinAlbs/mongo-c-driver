use crate::bson_view::BsonView;
use std::ffi::c_void;

/// An owned, heap-allocated BSON document returned from an operation.
///
/// Opaque to permit future extension without ABI breaks.
/// The caller is responsible for freeing it with [`mongoc_rust_bson_destroy`].
pub struct BsonOwned {
    data: Vec<u8>,
}

impl BsonOwned {
    /// Serialize a [`mongodb::bson::Document`] into an owned BSON buffer.
    pub fn from_doc(doc: &mongodb::bson::Document) -> Option<Self> {
        let mut data = Vec::new();
        doc.to_writer(&mut data).ok().map(|_| Self { data })
    }
}

/// Destroy the BSON object.
#[unsafe(no_mangle)]
pub extern "C" fn bson_owned_destroy(bson: *mut BsonOwned) {
    if !bson.is_null() {
        unsafe { drop(Box::from_raw(bson)) }
    }
}

/// Return a non-owning view of the raw BSON bytes.
///
/// The view is valid only for the lifetime of the `mongoc_rust_bson_t`.
/// The bytes may be used with `bson_init_static` to read fields.
#[unsafe(no_mangle)]
pub extern "C" fn bson_owned_as_view(bson: *const BsonOwned) -> BsonView {
    let b = unsafe { &*bson };
    BsonView {
        data: b.data.as_ptr() as *const c_void,
        len: b.data.len(),
    }
}
