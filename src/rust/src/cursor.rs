use crate::bson_view::BsonView;
use crate::log;
use std::ffi::c_void;

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "cursor";

/// A synchronously-iterable cursor over a sequence of BSON documents.
pub struct Cursor<'r> {
    runtime: &'r tokio::runtime::Runtime,
    inner: mongodb::Cursor<mongodb::bson::Document>,
    /// Serialized BSON bytes of the current document. Refilled on each [`mongoc_rust_cursor_next`].
    current_bson: Vec<u8>,
}

impl<'r> Cursor<'r> {
    pub fn new(
        runtime: &'r tokio::runtime::Runtime,
        inner: mongodb::Cursor<mongodb::bson::Document>,
    ) -> Self {
        Self {
            runtime,
            inner,
            current_bson: Vec::new(),
        }
    }
}

/// Destroy the cursor.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_cursor_destroy(cursor: *mut Cursor) {
    if !cursor.is_null() {
        unsafe { drop(Box::from_raw(cursor)) }
    }
}

/// Advance the cursor to the next document.
///
/// Returns `true` if a document is available and may be read with
/// [`mongoc_rust_cursor_current`].
/// Returns `false` when exhausted or on error; if `error` is non-null it is
/// set to an owned error that the caller must destroy with
/// [`mongoc_rust_error_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_cursor_next_await(
    cursor: *mut Cursor,
    error: *mut *mut crate::error::Error,
) -> bool {
    let Cursor { runtime, inner, current_bson } = unsafe { &mut *cursor };

    match runtime.block_on(inner.advance()) {
        Ok(true) => {
            current_bson.clear();
            current_bson.extend_from_slice(inner.current().as_bytes());
            true
        }
        Ok(false) => false,
        Err(e) => {
            if !error.is_null() {
                unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e))) };
            }
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            false
        }
    }
}

/// Return a non-owning view of the current document's raw BSON bytes.
///
/// Valid only until the next call to [`mongoc_rust_cursor_next`].
/// Behavior is undefined if called before the first successful `mongoc_rust_cursor_next`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_cursor_current(cursor: *const Cursor) -> BsonView {
    let bson = &unsafe { &*cursor }.current_bson;
    BsonView {
        data: bson.as_ptr() as *const c_void,
        len: bson.len(),
    }
}
