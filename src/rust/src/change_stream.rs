use crate::bson_view::BsonView;
use crate::log;
use std::ffi::c_void;

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "change_stream";

/// A synchronously-iterable change stream over a collection's change events.
pub struct ChangeStream<'r> {
    runtime: &'r tokio::runtime::Runtime,
    inner: mongodb::change_stream::ChangeStream<
        mongodb::change_stream::event::ChangeStreamEvent<mongodb::bson::Document>,
    >,
    /// Serialized BSON bytes of the current event. Refilled on each [`mongoc_rust_change_stream_next`].
    current_bson: Vec<u8>,
}

impl<'r> ChangeStream<'r> {
    pub fn new(
        runtime: &'r tokio::runtime::Runtime,
        inner: mongodb::change_stream::ChangeStream<
            mongodb::change_stream::event::ChangeStreamEvent<mongodb::bson::Document>,
        >,
    ) -> Self {
        Self { runtime, inner, current_bson: Vec::new() }
    }
}

/// Destroy the change stream.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_change_stream_destroy(stream: *mut ChangeStream) {
    if !stream.is_null() {
        unsafe { drop(Box::from_raw(stream)) }
    }
}

/// Retrieve the next available event from the change stream.
///
/// Makes one request to the server.  Returns `true` if an event was received
/// and can be read with [`mongoc_rust_change_stream_current`].
/// Returns `false` when no event is immediately available or on error.
/// On error, `error` (if non-null) is set to an owned error the caller must
/// destroy with [`mongoc_rust_error_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_change_stream_next_await(
    stream: *mut ChangeStream,
    error: *mut *mut crate::error::Error,
) -> bool {
    let ChangeStream { runtime, inner, current_bson } = unsafe { &mut *stream };

    match runtime.block_on(inner.next_if_any()) {
        Ok(Some(event)) => {
            current_bson.clear();
            match mongodb::bson::serialize_to_vec(&event) {
                Ok(bytes) => {
                    *current_bson = bytes;
                    true
                }
                Err(e) => {
                    log::error(MONGOC_LOG_DOMAIN, &format!("failed to serialize event to BSON: {e}"));
                    false
                }
            }
        }
        Ok(None) => false,
        Err(e) => {
            if !error.is_null() {
                unsafe {
                    *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e)));
                }
            }
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            false
        }
    }
}

/// Return a non-owning view of the current event's raw BSON bytes.
///
/// Valid only until the next call to [`mongoc_rust_change_stream_next`].
/// Behavior is undefined if called before the first successful
/// [`mongoc_rust_change_stream_next`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_change_stream_current(stream: *const ChangeStream) -> BsonView {
    let bson = &unsafe { &*stream }.current_bson;
    BsonView {
        data: bson.as_ptr() as *const c_void,
        len: bson.len(),
    }
}
