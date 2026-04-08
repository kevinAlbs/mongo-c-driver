pub mod apm;
pub mod bson_owned;
pub mod change_stream;
pub mod client;
pub mod collection;
pub mod cursor;
pub mod database;
pub mod error;
pub mod future;
pub mod log;
pub mod options;
pub mod runtime_handle;
pub mod session;
pub mod bson_view;

/// cbindgen:ignore
pub mod bson;

/// cbindgen:ignore
pub mod mongoc;

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_sanity_check(n: i32) -> i32 {
    n
}
