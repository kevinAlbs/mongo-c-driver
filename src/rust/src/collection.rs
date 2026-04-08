use crate::bson_owned::BsonOwned;
use crate::bson_view::BsonView;
use crate::cursor::Cursor;
use crate::future::{Future, FutureValue, FutureValueType};
use crate::log;
use crate::options::{AggregateOpts, CountOpts, DeleteOpts, FindOneAndOpts, FindOpts, InsertManyOpts, InsertOneOpts, UpdateOpts};

use std::ffi::{CStr, CString, c_char};

use async_ffi::FutureExt;

use mongodb::bson::{Bson, Document, doc};

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "collection";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Deserialize a [`BsonView`] into a [`Document`], logging on failure.
fn parse_bson_view(view: BsonView) -> Option<Document> {
    let bytes = unsafe { std::slice::from_raw_parts(view.data as *const u8, view.len) };
    match Document::from_reader(std::io::Cursor::new(bytes)) {
        Ok(doc) => Some(doc),
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            None
        }
    }
}

/// Write `e` into the optional error out-parameter and log it.
fn set_error(error: *mut *mut crate::error::Error, e: &mongodb::error::Error) {
    if !error.is_null() {
        unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(e))) };
    }
    log::error(MONGOC_LOG_DOMAIN, &e.to_string());
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_destroy(collection: *mut Collection) {
    if !collection.is_null() {
        unsafe { drop(Box::from_raw(collection)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_get_name(collection: *mut Collection) -> *const c_char {
    unsafe { &*collection }.name.as_ptr() as *const c_char
}

// ---------------------------------------------------------------------------
// Drop
// ---------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_drop_await(collection: *mut Collection) -> bool {
    let c = unsafe { &*collection };
    match c.runtime.block_on(async move { c.coll.drop().await }) {
        Ok(_) => true,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            false
        }
    }
}

// ---------------------------------------------------------------------------
// Insert
// ---------------------------------------------------------------------------

/// Insert a single document.
///
/// `opts` may be null (default options).
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_insert_one_await(
    collection: *mut Collection,
    doc: BsonView,
    opts: *const InsertOneOpts,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(document) = parse_bson_view(doc) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.insert_one(document);
        if let Some(o) = opts {
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.await
    }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Insert a single document within an existing session (and any active transaction).
///
/// `opts` may be null (default options).
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_insert_one_with_session_await(
    collection: *mut Collection,
    doc: BsonView,
    opts: *const InsertOneOpts,
    session: *mut crate::session::Session,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let s = unsafe { &mut *session };
    let Some(document) = parse_bson_view(doc) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async {
        let mut op = c.coll.insert_one(document);
        if let Some(o) = opts {
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.session(&mut s.inner).await
    }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Insert a single document asynchronously; returns a `Void` future.
///
/// `opts` may be null (default options).
/// Use [`mongoc_rust_future_get_void`] to extract the result.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_insert_one<'r>(
    collection: *mut Collection<'r>,
    doc: BsonView,
    opts: *const InsertOneOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(document) = parse_bson_view(doc) else { return std::ptr::null_mut(); };
    let bypass = unsafe { opts.as_ref() }.and_then(|o| o.bypass_document_validation);
    let comment = unsafe { opts.as_ref() }.and_then(|o| o.comment.clone());

    let future = Future {
        runtime,
        value: FutureValue::Void(FutureValueType {
            future: async move {
                let mut op = coll.insert_one(document);
                if let Some(v) = bypass { op = op.bypass_document_validation(v); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await.map(|_| ())
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Insert multiple documents.
///
/// `docs` is a C array of `n_docs` [`bson_rust_view_t`] values.
/// `opts` may be null (default options).
/// On success, if `inserted_count` is non-null it is set to the number of
/// documents inserted.
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_insert_many_await(
    collection: *mut Collection,
    docs: *const BsonView,
    n_docs: usize,
    opts: *const InsertManyOpts,
    inserted_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let views = unsafe { std::slice::from_raw_parts(docs, n_docs) };
    let opts = unsafe { opts.as_ref() };

    let mut documents = Vec::with_capacity(n_docs);
    for view in views {
        match parse_bson_view(*view) {
            Some(d) => documents.push(d),
            None => return false,
        }
    }

    match c.runtime.block_on(async move {
        let mut op = c.coll.insert_many(documents);
        if let Some(o) = opts {
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(v) = o.ordered { op = op.ordered(v); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !inserted_count.is_null() {
                unsafe { *inserted_count = result.inserted_ids.len() as u64 };
            }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Insert multiple documents asynchronously.
///
/// `docs` is a C array of `n_docs` [`bson_rust_view_t`] values.
/// `opts` may be null (default options).
/// Returns a `UInt64` future; use [`mongoc_rust_future_get_uint64`] to retrieve
/// the inserted count after the future resolves.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_insert_many<'r>(
    collection: *mut Collection<'r>,
    docs: *const BsonView,
    n_docs: usize,
    opts: *const InsertManyOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let views = unsafe { std::slice::from_raw_parts(docs, n_docs) };
    let bypass = unsafe { opts.as_ref() }.and_then(|o| o.bypass_document_validation);
    let ordered = unsafe { opts.as_ref() }.and_then(|o| o.ordered);
    let comment = unsafe { opts.as_ref() }.and_then(|o| o.comment.clone());

    let mut documents = Vec::with_capacity(n_docs);
    for view in views {
        match parse_bson_view(*view) {
            Some(d) => documents.push(d),
            None => return std::ptr::null_mut(),
        }
    }

    let future = Future {
        runtime,
        value: FutureValue::UInt64(FutureValueType {
            future: async move {
                let mut op = coll.insert_many(documents);
                if let Some(v) = bypass { op = op.bypass_document_validation(v); }
                if let Some(v) = ordered { op = op.ordered(v); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await.map(|r| r.inserted_ids.len() as u64)
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

// ---------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------

/// Return a cursor over all documents matching `filter`.
///
/// `opts` may be null (default options).
/// On failure `error` (if non-null) is set and null is returned; the caller
/// must destroy a non-null return value with [`mongoc_rust_cursor_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_await<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    opts: *const FindOpts,
    error: *mut *mut crate::error::Error,
) -> *mut Cursor<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let opts = unsafe { opts.as_ref() };

    match runtime.block_on(async move {
        let mut op = coll.find(filter_doc);
        if let Some(o) = opts {
            if let Some(ref d) = o.projection { op = op.projection(d.clone()); }
            if let Some(ref d) = o.sort { op = op.sort(d.clone()); }
            if let Some(v) = o.skip { op = op.skip(v); }
            if let Some(v) = o.limit { op = op.limit(v); }
            if let Some(v) = o.batch_size { op = op.batch_size(v); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(v) = o.allow_disk_use { op = op.allow_disk_use(v); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
        }
        op.await
    }) {
        Ok(inner) => Box::into_raw(Box::new(Cursor::new(runtime, inner))),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Return the first document matching `filter`, or null if none matches.
///
/// `opts` may be null (default options).
/// On error `error` (if non-null) is set; a null return with no error means
/// no document was found. The caller must destroy a non-null return value
/// with [`mongoc_rust_bson_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one_await(
    collection: *mut Collection,
    filter: BsonView,
    opts: *const FindOpts,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let opts_ref = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.find_one(filter_doc);
        if let Some(o) = opts_ref {
            if let Some(ref d) = o.projection { op = op.projection(d.clone()); }
            if let Some(ref d) = o.sort { op = op.sort(d.clone()); }
            if let Some(v) = o.skip { op = op.skip(v); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.await
    }) {
        Ok(Some(doc)) => match BsonOwned::from_doc(&doc) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize find_one result");
                std::ptr::null_mut()
            }
        },
        Ok(None) => std::ptr::null_mut(),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Find one document asynchronously.
///
/// `opts` may be null (default options).
/// Returns an `OptionalDocument` future; use [`mongoc_rust_future_get_bson`] to
/// retrieve the document (null = not found or error).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    opts: *const FindOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let projection = unsafe { opts.as_ref() }.and_then(|o| o.projection.clone());
    let sort = unsafe { opts.as_ref() }.and_then(|o| o.sort.clone());
    let skip = unsafe { opts.as_ref() }.and_then(|o| o.skip);
    let comment = unsafe { opts.as_ref() }.and_then(|o| o.comment.clone());

    let future = Future {
        runtime,
        value: FutureValue::OptionalDocument(FutureValueType {
            future: async move {
                let mut op = coll.find_one(filter_doc);
                if let Some(d) = projection { op = op.projection(d); }
                if let Some(d) = sort { op = op.sort(d); }
                if let Some(v) = skip { op = op.skip(v); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

// ---------------------------------------------------------------------------
// Update / Replace
// ---------------------------------------------------------------------------

/// Update the first document matching `filter`.
///
/// `update` must contain update operators (e.g. `{ "$set": { ... } }`).
/// `opts` may be null (default options).
/// On success, if `matched_count` / `modified_count` are non-null they are set.
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_one_await(
    collection: *mut Collection,
    filter: BsonView,
    update: BsonView,
    opts: *const UpdateOpts,
    matched_count: *mut u64,
    modified_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let Some(update_doc) = parse_bson_view(update) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.update_one(filter_doc, update_doc);
        if let Some(o) = opts {
            if let Some(v) = o.upsert { op = op.upsert(v); }
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref h) = o.hint {
                op = op.hint(h.as_driver_hint());
            }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref s) = o.sort { op = op.sort(s.clone()); }
            if let Some(ref af) = o.array_filters { op = op.array_filters(af.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !matched_count.is_null() { unsafe { *matched_count = result.matched_count }; }
            if !modified_count.is_null() { unsafe { *modified_count = result.modified_count }; }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Update the first document matching `filter` asynchronously.
///
/// `opts` may be null (default options).
/// Returns an `UpdateResult` future; use [`mongoc_rust_future_get_update_result`]
/// to retrieve `matched_count` and `modified_count`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_one<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    update: BsonView,
    opts: *const UpdateOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let Some(update_doc) = parse_bson_view(update) else { return std::ptr::null_mut(); };
    let (upsert, bypass, hint, let_vars, comment, sort) = unsafe { opts.as_ref() }
        .map(|o| (
            o.upsert,
            o.bypass_document_validation,
            o.hint.as_ref().map(|h| h.as_driver_hint()),
            o.let_vars.clone(),
            o.comment.clone(),
            o.sort.clone(),
        ))
        .unwrap_or((None, None, None, None, None, None));

    let future = Future {
        runtime,
        value: FutureValue::UpdateResult(FutureValueType {
            future: async move {
                let mut op = coll.update_one(filter_doc, update_doc);
                if let Some(v) = upsert { op = op.upsert(v); }
                if let Some(v) = bypass { op = op.bypass_document_validation(v); }
                if let Some(h) = hint { op = op.hint(h); }
                if let Some(d) = let_vars { op = op.let_vars(d); }
                if let Some(c) = comment { op = op.comment(c); }
                if let Some(s) = sort { op = op.sort(s); }
                op.await
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Update all documents matching `filter`.
///
/// `opts` may be null (default options). Same signature as
/// [`mongoc_rust_collection_update_one`] with the addition of `opts`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_many_await(
    collection: *mut Collection,
    filter: BsonView,
    update: BsonView,
    opts: *const UpdateOpts,
    matched_count: *mut u64,
    modified_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let Some(update_doc) = parse_bson_view(update) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.update_many(filter_doc, update_doc);
        if let Some(o) = opts {
            if let Some(v) = o.upsert { op = op.upsert(v); }
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref af) = o.array_filters { op = op.array_filters(af.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !matched_count.is_null() { unsafe { *matched_count = result.matched_count }; }
            if !modified_count.is_null() { unsafe { *modified_count = result.modified_count }; }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Update all documents matching `filter` asynchronously.
///
/// `opts` may be null (default options).
/// Returns an `UpdateResult` future; use [`mongoc_rust_future_get_update_result`]
/// to retrieve `matched_count` and `modified_count`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_many<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    update: BsonView,
    opts: *const UpdateOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let Some(update_doc) = parse_bson_view(update) else { return std::ptr::null_mut(); };
    let (upsert, bypass, hint, let_vars, comment) = unsafe { opts.as_ref() }
        .map(|o| (
            o.upsert,
            o.bypass_document_validation,
            o.hint.as_ref().map(|h| h.as_driver_hint()),
            o.let_vars.clone(),
            o.comment.clone(),
        ))
        .unwrap_or((None, None, None, None, None));

    let future = Future {
        runtime,
        value: FutureValue::UpdateResult(FutureValueType {
            future: async move {
                let mut op = coll.update_many(filter_doc, update_doc);
                if let Some(v) = upsert { op = op.upsert(v); }
                if let Some(v) = bypass { op = op.bypass_document_validation(v); }
                if let Some(h) = hint { op = op.hint(h); }
                if let Some(d) = let_vars { op = op.let_vars(d); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Replace the first document matching `filter` with `replacement`.
///
/// `opts` may be null (default options).
/// On success, if `matched_count` / `modified_count` are non-null they are set.
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_replace_one_await(
    collection: *mut Collection,
    filter: BsonView,
    replacement: BsonView,
    opts: *const UpdateOpts,
    matched_count: *mut u64,
    modified_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let Some(replacement_doc) = parse_bson_view(replacement) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.replace_one(filter_doc, replacement_doc);
        if let Some(o) = opts {
            if let Some(v) = o.upsert { op = op.upsert(v); }
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref s) = o.sort { op = op.sort(s.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !matched_count.is_null() { unsafe { *matched_count = result.matched_count }; }
            if !modified_count.is_null() { unsafe { *modified_count = result.modified_count }; }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Replace the first document asynchronously.
///
/// `opts` may be null (default options).
/// Returns an `UpdateResult` future; use [`mongoc_rust_future_get_update_result`]
/// to retrieve `matched_count` and `modified_count`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_replace_one<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    replacement: BsonView,
    opts: *const UpdateOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let Some(replacement_doc) = parse_bson_view(replacement) else { return std::ptr::null_mut(); };
    let (upsert, bypass, hint, let_vars, comment, sort) = unsafe { opts.as_ref() }
        .map(|o| (
            o.upsert,
            o.bypass_document_validation,
            o.hint.as_ref().map(|h| h.as_driver_hint()),
            o.let_vars.clone(),
            o.comment.clone(),
            o.sort.clone(),
        ))
        .unwrap_or((None, None, None, None, None, None));

    let future = Future {
        runtime,
        value: FutureValue::UpdateResult(FutureValueType {
            future: async move {
                let mut op = coll.replace_one(filter_doc, replacement_doc);
                if let Some(v) = upsert { op = op.upsert(v); }
                if let Some(v) = bypass { op = op.bypass_document_validation(v); }
                if let Some(h) = hint { op = op.hint(h); }
                if let Some(d) = let_vars { op = op.let_vars(d); }
                if let Some(c) = comment { op = op.comment(c); }
                if let Some(s) = sort { op = op.sort(s); }
                op.await
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------

/// Delete the first document matching `filter`.
///
/// `opts` may be null (default options).
/// On success, if `deleted_count` is non-null it is set.
/// On failure `error` (if non-null) is set to an owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_delete_one_await(
    collection: *mut Collection,
    filter: BsonView,
    opts: *const DeleteOpts,
    deleted_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.delete_one(filter_doc);
        if let Some(o) = opts {
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !deleted_count.is_null() { unsafe { *deleted_count = result.deleted_count }; }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Delete the first document matching `filter` asynchronously.
///
/// `opts` may be null (default options).
/// Returns a `UInt64` future; use [`mongoc_rust_future_get_uint64`] to retrieve
/// `deleted_count`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_delete_one<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    opts: *const DeleteOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let (hint, let_vars, comment) = unsafe { opts.as_ref() }
        .map(|o| (
            o.hint.as_ref().map(|h| h.as_driver_hint()),
            o.let_vars.clone(),
            o.comment.clone(),
        ))
        .unwrap_or((None, None, None));

    let future = Future {
        runtime,
        value: FutureValue::UInt64(FutureValueType {
            future: async move {
                let mut op = coll.delete_one(filter_doc);
                if let Some(h) = hint { op = op.hint(h); }
                if let Some(d) = let_vars { op = op.let_vars(d); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await.map(|r| r.deleted_count)
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Delete all documents matching `filter`.
///
/// `opts` may be null (default options). Same signature as
/// [`mongoc_rust_collection_delete_one`] with the addition of `opts`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_delete_many_await(
    collection: *mut Collection,
    filter: BsonView,
    opts: *const DeleteOpts,
    deleted_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.delete_many(filter_doc);
        if let Some(o) = opts {
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !deleted_count.is_null() { unsafe { *deleted_count = result.deleted_count }; }
            true
        }
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Delete all documents matching `filter` asynchronously.
///
/// `opts` may be null (default options).
/// Returns a `UInt64` future; use [`mongoc_rust_future_get_uint64`] to retrieve
/// `deleted_count`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_delete_many<'r>(
    collection: *mut Collection<'r>,
    filter: BsonView,
    opts: *const DeleteOpts,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let (hint, let_vars, comment) = unsafe { opts.as_ref() }
        .map(|o| (
            o.hint.as_ref().map(|h| h.as_driver_hint()),
            o.let_vars.clone(),
            o.comment.clone(),
        ))
        .unwrap_or((None, None, None));

    let future = Future {
        runtime,
        value: FutureValue::UInt64(FutureValueType {
            future: async move {
                let mut op = coll.delete_many(filter_doc);
                if let Some(h) = hint { op = op.hint(h); }
                if let Some(d) = let_vars { op = op.let_vars(d); }
                if let Some(c) = comment { op = op.comment(c); }
                op.await.map(|r| r.deleted_count)
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

// ---------------------------------------------------------------------------
// Count / Distinct
// ---------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_count_documents_await(
    collection: *mut Collection,
    opts: *const CountOpts,
) -> i64 {
    let c = unsafe { &*collection };
    let comment = unsafe { opts.as_ref() }.and_then(|o| o.comment.clone());
    match c.runtime.block_on(async move {
        let mut op = c.coll.count_documents(doc! {});
        if let Some(c) = comment { op = op.comment(c); }
        op.await
    }) {
        Ok(count) => count as i64,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            -1
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_count_documents<'r>(
    collection: *mut Collection<'r>,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let future = Future {
        runtime,
        value: FutureValue::UInt64(FutureValueType {
            future: async move { coll.count_documents(doc! {}).await }.into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Return the distinct values of `field_name` across documents matching `filter`.
///
/// The result is a `mongoc_rust_bson_t` whose bytes form a BSON document
/// `{ "values": [ ... ] }`. The caller must destroy it with
/// [`mongoc_rust_bson_destroy`].
/// Returns null on error; `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_distinct_await(
    collection: *mut Collection,
    field_name: *const c_char,
    filter: BsonView,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let field = unsafe { CStr::from_ptr(field_name) }.to_str().unwrap_or("");
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };

    match c.runtime.block_on(async move { c.coll.distinct(field, filter_doc).await }) {
        Ok(values) => {
            let mut result_doc = Document::new();
            result_doc.insert("values", Bson::Array(values));
            match BsonOwned::from_doc(&result_doc) {
                Some(b) => Box::into_raw(Box::new(b)),
                None => {
                    log::error(MONGOC_LOG_DOMAIN, "failed to serialize distinct result");
                    std::ptr::null_mut()
                }
            }
        }
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Return distinct values asynchronously.
///
/// Returns an `OptionalDocument` future; use [`mongoc_rust_future_get_bson`] to
/// retrieve the `{ "values": [...] }` document.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_distinct<'r>(
    collection: *mut Collection<'r>,
    field_name: *const c_char,
    filter: BsonView,
) -> *mut Future<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let field = unsafe { CStr::from_ptr(field_name) }.to_str().unwrap_or("").to_owned();
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };

    let future = Future {
        runtime,
        value: FutureValue::OptionalDocument(FutureValueType {
            future: async move {
                coll.distinct(&field, filter_doc).await.map(|values| {
                    let mut result_doc = Document::new();
                    result_doc.insert("values", Bson::Array(values));
                    Some(result_doc)
                })
            }
            .into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

// ---------------------------------------------------------------------------
// Pipeline update helpers
// ---------------------------------------------------------------------------

/// Update the first document matching `filter` using an aggregation pipeline.
///
/// `pipeline` is a C array of `n_stages` stage documents.
/// `opts` may be null (default options).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_one_pipeline_await(
    collection: *mut Collection,
    filter: BsonView,
    pipeline: *const BsonView,
    n_stages: usize,
    opts: *const UpdateOpts,
    matched_count: *mut u64,
    modified_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let views = unsafe { std::slice::from_raw_parts(pipeline, n_stages) };
    let mut stages: Vec<Document> = Vec::with_capacity(n_stages);
    for view in views {
        match parse_bson_view(*view) { Some(d) => stages.push(d), None => return false }
    }
    let opts = unsafe { opts.as_ref() };
    match c.runtime.block_on(async move {
        let mut op = c.coll.update_one(filter_doc, stages);
        if let Some(o) = opts {
            if let Some(v) = o.upsert { op = op.upsert(v); }
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref s) = o.sort { op = op.sort(s.clone()); }
            if let Some(ref af) = o.array_filters { op = op.array_filters(af.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !matched_count.is_null() { unsafe { *matched_count = result.matched_count }; }
            if !modified_count.is_null() { unsafe { *modified_count = result.modified_count }; }
            true
        }
        Err(e) => { set_error(error, &e); false }
    }
}

/// Update all documents matching `filter` using an aggregation pipeline.
///
/// `pipeline` is a C array of `n_stages` stage documents.
/// `opts` may be null (default options).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_update_many_pipeline_await(
    collection: *mut Collection,
    filter: BsonView,
    pipeline: *const BsonView,
    n_stages: usize,
    opts: *const UpdateOpts,
    matched_count: *mut u64,
    modified_count: *mut u64,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return false; };
    let views = unsafe { std::slice::from_raw_parts(pipeline, n_stages) };
    let mut stages: Vec<Document> = Vec::with_capacity(n_stages);
    for view in views {
        match parse_bson_view(*view) { Some(d) => stages.push(d), None => return false }
    }
    let opts = unsafe { opts.as_ref() };
    match c.runtime.block_on(async move {
        let mut op = c.coll.update_many(filter_doc, stages);
        if let Some(o) = opts {
            if let Some(v) = o.upsert { op = op.upsert(v); }
            if let Some(v) = o.bypass_document_validation { op = op.bypass_document_validation(v); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref d) = o.let_vars { op = op.let_vars(d.clone()); }
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref af) = o.array_filters { op = op.array_filters(af.clone()); }
        }
        op.await
    }) {
        Ok(result) => {
            if !matched_count.is_null() { unsafe { *matched_count = result.matched_count }; }
            if !modified_count.is_null() { unsafe { *modified_count = result.modified_count }; }
            true
        }
        Err(e) => { set_error(error, &e); false }
    }
}

/// Find a document matching `filter`, apply a pipeline update, and return the
/// pre-update document.
///
/// `pipeline` is a C array of `n_stages` stage documents.
/// `opts` may be null (default options).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one_and_update_pipeline_await(
    collection: *mut Collection,
    filter: BsonView,
    pipeline: *const BsonView,
    n_stages: usize,
    opts: *const FindOneAndOpts,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let views = unsafe { std::slice::from_raw_parts(pipeline, n_stages) };
    let mut stages: Vec<Document> = Vec::with_capacity(n_stages);
    for view in views {
        match parse_bson_view(*view) {
            Some(d) => stages.push(d),
            None => return std::ptr::null_mut(),
        }
    }
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.find_one_and_update(filter_doc, stages);
        if let Some(o) = opts {
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
        }
        op.await
    }) {
        Ok(Some(doc)) => match BsonOwned::from_doc(&doc) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize find_one_and_update_pipeline result");
                std::ptr::null_mut()
            }
        },
        Ok(None) => std::ptr::null_mut(),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

// ---------------------------------------------------------------------------
// Estimated count / Aggregate / FindOneAnd*
// ---------------------------------------------------------------------------

/// Return an estimate of the number of documents in the collection.
///
/// Uses collection metadata rather than a full scan.
/// `opts` may be null (default options).
/// Returns -1 on error; `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_estimated_document_count_await(
    collection: *mut Collection,
    opts: *const CountOpts,
    error: *mut *mut crate::error::Error,
) -> i64 {
    let c = unsafe { &*collection };
    let opts_ref = unsafe { opts.as_ref() };
    let comment = opts_ref.and_then(|o| o.comment.clone());
    let max_time_ms = opts_ref.and_then(|o| o.max_time_ms);
    match c.runtime.block_on(async move {
        let mut op = c.coll.estimated_document_count();
        if let Some(c) = comment { op = op.comment(c); }
        if let Some(ms) = max_time_ms {
            op = op.max_time(std::time::Duration::from_millis(ms));
        }
        op.await
    }) {
        Ok(count) => count as i64,
        Err(e) => {
            set_error(error, &e);
            -1
        }
    }
}

/// Create an index on the collection.
///
/// `keys` is a BSON document specifying the index key pattern.
/// `unique` specifies whether the index should enforce uniqueness.
/// Returns true on success, false on error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_create_index_await(
    collection: *mut Collection,
    keys: BsonView,
    unique: bool,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let Some(keys_doc) = parse_bson_view(keys) else { return false; };

    match c.runtime.block_on(async move {
        let mut index_opts = mongodb::IndexModel::builder()
            .keys(keys_doc)
            .build();
        if unique {
            index_opts.options = Some(mongodb::options::IndexOptions::builder()
                .unique(true)
                .build());
        }
        c.coll.create_index(index_opts).await
    }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Drop a single index by name.
///
/// Returns true on success, false on error; `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_drop_index_await(
    collection: *mut Collection,
    name: *const c_char,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let name_str = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    match c.runtime.block_on(async move { c.coll.drop_index(name_str).await }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Drop all non-`_id` indexes on the collection.
///
/// Returns true on success, false on error; `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_drop_indexes_await(
    collection: *mut Collection,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    match c.runtime.block_on(async move { c.coll.drop_indexes().await }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Return a sorted, null-terminated array of index name strings.
///
/// The caller must free each element with `bson_free`, then the array itself
/// with `bson_free`.  Returns null on error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_list_index_names_await(
    collection: *mut Collection,
) -> *mut *mut c_char {
    let c = unsafe { &*collection };
    let mut names = match c.runtime.block_on(async move { c.coll.list_index_names().await }) {
        Ok(n) => n,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };
    names.sort();

    unsafe {
        let ret = crate::bson::bson_malloc(
            std::mem::size_of::<*mut c_char>() * (names.len() + 1),
        ) as *mut *mut c_char;
        for (i, name) in names.iter().enumerate() {
            let bytes = name.as_bytes();
            *ret.add(i) =
                crate::bson::bson_strndup(bytes.as_ptr() as *const c_char, bytes.len());
        }
        *ret.add(names.len()) = std::ptr::null_mut();
        ret
    }
}

/// Rename a collection.
///
/// `new_db_name` specifies the target database (may equal the current database).
/// `new_coll_name` specifies the new collection name.
/// If `drop_target` is true and a collection named `new_coll_name` already
/// exists in `new_db_name`, it will be dropped before the rename.
/// Returns true on success, false on error; `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_rename_await(
    collection: *mut Collection,
    new_db_name: *const c_char,
    new_coll_name: *const c_char,
    drop_target: bool,
    error: *mut *mut crate::error::Error,
) -> bool {
    let c = unsafe { &*collection };
    let new_db = unsafe {
        assert!(!new_db_name.is_null());
        CStr::from_ptr(new_db_name)
    }
    .to_str()
    .unwrap();
    let new_name = unsafe {
        assert!(!new_coll_name.is_null());
        CStr::from_ptr(new_coll_name)
    }
    .to_str()
    .unwrap();

    // The Rust driver has no native rename; use the renameCollection admin command.
    let src_ns = format!(
        "{}.{}",
        c.coll.namespace().db,
        c.coll.namespace().coll
    );
    let dst_ns = format!("{}.{}", new_db, new_name);
    let cmd = mongodb::bson::doc! {
        "renameCollection": src_ns,
        "to": dst_ns,
        "dropTarget": drop_target,
    };
    let admin = c.coll.client().database("admin");
    match c.runtime.block_on(async move { admin.run_command(cmd).await }) {
        Ok(_) => true,
        Err(e) => {
            set_error(error, &e);
            false
        }
    }
}

/// Run an aggregation pipeline and return a cursor over the result documents.
///
/// `pipeline` is a C array of `n_stages` [`bson_rust_view_t`] values, each
/// representing one pipeline stage document (e.g. `{ "$match": { ... } }`).
/// `opts` may be null (default options).
/// Returns null on error; `error` (if non-null) is set.
/// The caller must destroy the cursor with [`mongoc_rust_cursor_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_aggregate_await<'r>(
    collection: *mut Collection<'r>,
    pipeline: *const BsonView,
    n_stages: usize,
    opts: *const AggregateOpts,
    error: *mut *mut crate::error::Error,
) -> *mut Cursor<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let views = unsafe { std::slice::from_raw_parts(pipeline, n_stages) };
    let opts = unsafe { opts.as_ref() };

    let mut stages = Vec::with_capacity(n_stages);
    for view in views {
        match parse_bson_view(*view) {
            Some(d) => stages.push(d),
            None => return std::ptr::null_mut(),
        }
    }

    match runtime.block_on(async move {
        let mut op = coll.aggregate(stages);
        if let Some(o) = opts {
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(v) = o.batch_size { op = op.batch_size(v); }
            if let Some(v) = o.allow_disk_use { op = op.allow_disk_use(v); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
        }
        op.await
    }) {
        Ok(inner) => Box::into_raw(Box::new(Cursor::new(runtime, inner))),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Find a document matching `filter`, atomically delete it, and return it.
///
/// `opts` may be null (default options).
/// Returns null if no document matches (not an error) or on error; `error`
/// distinguishes the two cases. The caller must destroy a non-null return
/// value with [`mongoc_rust_bson_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one_and_delete_await(
    collection: *mut Collection,
    filter: BsonView,
    opts: *const FindOneAndOpts,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.find_one_and_delete(filter_doc);
        if let Some(o) = opts {
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
        }
        op.await
    }) {
        Ok(Some(doc)) => match BsonOwned::from_doc(&doc) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize find_one_and_delete result");
                std::ptr::null_mut()
            }
        },
        Ok(None) => std::ptr::null_mut(),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Find a document matching `filter`, apply `update`, and return the
/// pre-update document (before modification).
///
/// `update` must contain update operators (e.g. `{ "$set": { ... } }`).
/// `opts` may be null (default options).
/// Returns null if no document matches or on error. The caller must destroy
/// a non-null return value with [`mongoc_rust_bson_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one_and_update_await(
    collection: *mut Collection,
    filter: BsonView,
    update: BsonView,
    opts: *const FindOneAndOpts,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let Some(update_doc) = parse_bson_view(update) else { return std::ptr::null_mut(); };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.find_one_and_update(filter_doc, update_doc);
        if let Some(o) = opts {
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
            if let Some(v) = o.upsert { op = op.upsert(v); }
        }
        op.await
    }) {
        Ok(Some(doc)) => match BsonOwned::from_doc(&doc) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize find_one_and_update result");
                std::ptr::null_mut()
            }
        },
        Ok(None) => std::ptr::null_mut(),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

/// Find a document matching `filter`, replace it with `replacement`, and
/// return the pre-replacement document.
///
/// `opts` may be null (default options).
/// Returns null if no document matches or on error. The caller must destroy
/// a non-null return value with [`mongoc_rust_bson_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_find_one_and_replace_await(
    collection: *mut Collection,
    filter: BsonView,
    replacement: BsonView,
    opts: *const FindOneAndOpts,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let c = unsafe { &*collection };
    let Some(filter_doc) = parse_bson_view(filter) else { return std::ptr::null_mut(); };
    let Some(replacement_doc) = parse_bson_view(replacement) else { return std::ptr::null_mut(); };
    let opts = unsafe { opts.as_ref() };

    match c.runtime.block_on(async move {
        let mut op = c.coll.find_one_and_replace(filter_doc, replacement_doc);
        if let Some(o) = opts {
            if let Some(ref c) = o.comment { op = op.comment(c.clone()); }
            if let Some(ref h) = o.hint { op = op.hint(h.as_driver_hint()); }
            if let Some(ref lv) = o.let_vars { op = op.let_vars(lv.clone()); }
            if let Some(v) = o.upsert { op = op.upsert(v); }
        }
        op.await
    }) {
        Ok(Some(doc)) => match BsonOwned::from_doc(&doc) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize find_one_and_replace result");
                std::ptr::null_mut()
            }
        },
        Ok(None) => std::ptr::null_mut(),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

// ---------------------------------------------------------------------------
// Change stream
// ---------------------------------------------------------------------------

/// Open a change stream on the collection.
///
/// `pipeline` is a C array of `n_stages` pipeline stage documents.
/// Returns null on error; `error` (if non-null) is set.
/// The caller must destroy the stream with [`mongoc_rust_change_stream_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_collection_watch_await<'r>(
    collection: *mut Collection<'r>,
    pipeline: *const BsonView,
    n_stages: usize,
    error: *mut *mut crate::error::Error,
) -> *mut crate::change_stream::ChangeStream<'r> {
    let Collection { runtime, coll, .. } = unsafe { &*collection };
    let views = if n_stages == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(pipeline, n_stages) }
    };

    let mut stages = Vec::with_capacity(n_stages);
    for view in views {
        match parse_bson_view(*view) {
            Some(d) => stages.push(d),
            None => return std::ptr::null_mut(),
        }
    }

    match runtime.block_on(async move { coll.watch().pipeline(stages).await }) {
        Ok(inner) => Box::into_raw(Box::new(crate::change_stream::ChangeStream::new(runtime, inner))),
        Err(e) => {
            set_error(error, &e);
            std::ptr::null_mut()
        }
    }
}

// ---------------------------------------------------------------------------
// Collection struct
// ---------------------------------------------------------------------------

pub struct Collection<'r> {
    runtime: &'r tokio::runtime::Runtime,
    coll: mongodb::Collection<mongodb::bson::Document>,
    name: CString,
}

impl<'r> Collection<'r> {
    pub fn new(
        runtime: &'r tokio::runtime::Runtime,
        coll: mongodb::Collection<mongodb::bson::Document>,
    ) -> Self {
        let name = CString::new(coll.name()).unwrap();
        Self { runtime, coll, name }
    }
}
