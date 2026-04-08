//! Opaque options structs for collection/database operations.
//!
//! Each struct is heap-allocated by a `_new()` function and freed by
//! `_destroy()`.  All setters are optional — unset fields use the Rust
//! driver's defaults.  Pass a null pointer where an options argument is
//! accepted to use all defaults.

use crate::bson_view::{BsonValueC, BsonView};
use mongodb::bson::{Bson, Document};
use std::ffi::CStr;
use std::os::raw::c_char;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

fn bson_view_to_document(view: BsonView) -> Option<Document> {
    let bytes = unsafe { std::slice::from_raw_parts(view.data as *const u8, view.len) };
    Document::from_reader(std::io::Cursor::new(bytes)).ok()
}

fn c_str_to_bson_string(s: *const c_char) -> Option<Bson> {
    if s.is_null() {
        return None;
    }
    let rust_str = unsafe { CStr::from_ptr(s) }.to_str().ok()?.to_owned();
    Some(Bson::String(rust_str))
}

// ---------------------------------------------------------------------------
// Hint — shared by update, delete, distinct
// ---------------------------------------------------------------------------

pub enum Hint {
    Name(String),
    Keys(Document),
}

impl Hint {
    pub fn as_driver_hint(&self) -> mongodb::options::Hint {
        match self {
            Hint::Name(s) => mongodb::options::Hint::Name(s.clone()),
            Hint::Keys(d) => mongodb::options::Hint::Keys(d.clone()),
        }
    }
}

// ---------------------------------------------------------------------------
// InsertOneOpts
// ---------------------------------------------------------------------------

pub struct InsertOneOpts {
    pub bypass_document_validation: Option<bool>,
    pub comment: Option<Bson>,
}

/// Create a new [`InsertOneOpts`] with all fields unset (defaults).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_one_opts_new() -> *mut InsertOneOpts {
    Box::into_raw(Box::new(InsertOneOpts {
        bypass_document_validation: None,
        comment: None,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_one_opts_destroy(opts: *mut InsertOneOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_one_opts_set_bypass_document_validation(
    opts: *mut InsertOneOpts,
    val: bool,
) {
    unsafe { &mut *opts }.bypass_document_validation = Some(val);
}

/// Set `comment` from a UTF-8 C string (the most common form).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_one_opts_set_comment(
    opts: *mut InsertOneOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

/// Set `comment` from any BSON type via a `bson_value_t *`.
/// Pass the result of `bson_iter_value()` or a pointer to a `bson_value_t`.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_one_opts_set_comment_value(
    opts: *mut InsertOneOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// InsertManyOpts
// ---------------------------------------------------------------------------

pub struct InsertManyOpts {
    pub bypass_document_validation: Option<bool>,
    pub ordered: Option<bool>,
    pub comment: Option<Bson>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_new() -> *mut InsertManyOpts {
    Box::into_raw(Box::new(InsertManyOpts {
        bypass_document_validation: None,
        ordered: None,
        comment: None,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_destroy(opts: *mut InsertManyOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_set_bypass_document_validation(
    opts: *mut InsertManyOpts,
    val: bool,
) {
    unsafe { &mut *opts }.bypass_document_validation = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_set_ordered(
    opts: *mut InsertManyOpts,
    val: bool,
) {
    unsafe { &mut *opts }.ordered = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_set_comment(
    opts: *mut InsertManyOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_insert_many_opts_set_comment_value(
    opts: *mut InsertManyOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// FindOpts  (used by both find and find_one)
// ---------------------------------------------------------------------------

pub struct FindOpts {
    pub projection: Option<Document>,
    pub sort: Option<Document>,
    pub skip: Option<u64>,
    pub limit: Option<i64>,
    pub batch_size: Option<u32>,
    pub comment: Option<Bson>,
    pub allow_disk_use: Option<bool>,
    pub let_vars: Option<Document>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_new() -> *mut FindOpts {
    Box::into_raw(Box::new(FindOpts {
        projection: None,
        sort: None,
        skip: None,
        limit: None,
        batch_size: None,
        comment: None,
        allow_disk_use: None,
        let_vars: None,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_allow_disk_use(opts: *mut FindOpts, val: bool) {
    unsafe { &mut *opts }.allow_disk_use = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_let(opts: *mut FindOpts, let_vars: BsonView) {
    unsafe { &mut *opts }.let_vars = bson_view_to_document(let_vars);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_destroy(opts: *mut FindOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

/// Set the projection document (which fields to include/exclude).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_projection(
    opts: *mut FindOpts,
    projection: BsonView,
) {
    unsafe { &mut *opts }.projection = bson_view_to_document(projection);
}

/// Set the sort document.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_sort(opts: *mut FindOpts, sort: BsonView) {
    unsafe { &mut *opts }.sort = bson_view_to_document(sort);
}

/// Set the number of documents to skip.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_skip(opts: *mut FindOpts, skip: u64) {
    unsafe { &mut *opts }.skip = Some(skip);
}

/// Set the maximum number of documents to return (find only; ignored by find_one).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_limit(opts: *mut FindOpts, limit: i64) {
    unsafe { &mut *opts }.limit = Some(limit);
}

/// Set the number of documents to return per batch.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_batch_size(opts: *mut FindOpts, batch_size: u32) {
    unsafe { &mut *opts }.batch_size = Some(batch_size);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_comment(
    opts: *mut FindOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_opts_set_comment_value(
    opts: *mut FindOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// UpdateOpts  (update_one, update_many, replace_one)
// ---------------------------------------------------------------------------

pub struct UpdateOpts {
    pub upsert: Option<bool>,
    pub bypass_document_validation: Option<bool>,
    pub hint: Option<Hint>,
    pub let_vars: Option<Document>,
    pub comment: Option<Bson>,
    pub sort: Option<Document>,
    pub array_filters: Option<Vec<Document>>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_new() -> *mut UpdateOpts {
    Box::into_raw(Box::new(UpdateOpts {
        upsert: None,
        bypass_document_validation: None,
        hint: None,
        let_vars: None,
        comment: None,
        sort: None,
        array_filters: None,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_destroy(opts: *mut UpdateOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_upsert(opts: *mut UpdateOpts, val: bool) {
    unsafe { &mut *opts }.upsert = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_bypass_document_validation(
    opts: *mut UpdateOpts,
    val: bool,
) {
    unsafe { &mut *opts }.bypass_document_validation = Some(val);
}

/// Set the index hint by name.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_hint_name(
    opts: *mut UpdateOpts,
    name: *const c_char,
) {
    if name.is_null() {
        return;
    }
    let s = unsafe { CStr::from_ptr(name) }
        .to_str()
        .unwrap_or("")
        .to_owned();
    unsafe { &mut *opts }.hint = Some(Hint::Name(s));
}

/// Set the index hint by key pattern document.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_hint_keys(
    opts: *mut UpdateOpts,
    keys: BsonView,
) {
    if let Some(doc) = bson_view_to_document(keys) {
        unsafe { &mut *opts }.hint = Some(Hint::Keys(doc));
    }
}

/// Set `let` variables (BSON document).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_let(opts: *mut UpdateOpts, let_vars: BsonView) {
    unsafe { &mut *opts }.let_vars = bson_view_to_document(let_vars);
}

/// Set the sort document (MongoDB 8.0+; only for updateOne/replaceOne).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_sort(opts: *mut UpdateOpts, sort: BsonView) {
    unsafe { &mut *opts }.sort = bson_view_to_document(sort);
}

/// Set arrayFilters: a C array of `n` BsonView stages.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_array_filters(
    opts: *mut UpdateOpts,
    filters: *const BsonView,
    n: usize,
) {
    let views = unsafe { std::slice::from_raw_parts(filters, n) };
    let mut docs = Vec::with_capacity(n);
    for view in views {
        if let Some(d) = bson_view_to_document(*view) {
            docs.push(d);
        }
    }
    unsafe { &mut *opts }.array_filters = Some(docs);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_comment(
    opts: *mut UpdateOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_update_opts_set_comment_value(
    opts: *mut UpdateOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// DeleteOpts  (delete_one, delete_many)
// ---------------------------------------------------------------------------

pub struct DeleteOpts {
    pub hint: Option<Hint>,
    pub let_vars: Option<Document>,
    pub comment: Option<Bson>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_new() -> *mut DeleteOpts {
    Box::into_raw(Box::new(DeleteOpts {
        hint: None,
        let_vars: None,
        comment: None,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_destroy(opts: *mut DeleteOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_set_hint_name(
    opts: *mut DeleteOpts,
    name: *const c_char,
) {
    if name.is_null() {
        return;
    }
    let s = unsafe { CStr::from_ptr(name) }
        .to_str()
        .unwrap_or("")
        .to_owned();
    unsafe { &mut *opts }.hint = Some(Hint::Name(s));
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_set_hint_keys(
    opts: *mut DeleteOpts,
    keys: BsonView,
) {
    if let Some(doc) = bson_view_to_document(keys) {
        unsafe { &mut *opts }.hint = Some(Hint::Keys(doc));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_set_let(opts: *mut DeleteOpts, let_vars: BsonView) {
    unsafe { &mut *opts }.let_vars = bson_view_to_document(let_vars);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_set_comment(
    opts: *mut DeleteOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_delete_opts_set_comment_value(
    opts: *mut DeleteOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// FindOneAndOpts  (find_one_and_delete, find_one_and_update, find_one_and_replace)
// ---------------------------------------------------------------------------

pub struct FindOneAndOpts {
    pub comment: Option<Bson>,
    pub hint: Option<Hint>,
    pub let_vars: Option<Document>,
    pub upsert: Option<bool>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_new() -> *mut FindOneAndOpts {
    Box::into_raw(Box::new(FindOneAndOpts { comment: None, hint: None, let_vars: None, upsert: None }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_upsert(opts: *mut FindOneAndOpts, val: bool) {
    unsafe { &mut *opts }.upsert = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_let(opts: *mut FindOneAndOpts, let_vars: BsonView) {
    unsafe { &mut *opts }.let_vars = bson_view_to_document(let_vars);
}

/// Set the index hint by name.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_hint_name(
    opts: *mut FindOneAndOpts,
    name: *const c_char,
) {
    if name.is_null() { return; }
    let s = unsafe { CStr::from_ptr(name) }.to_str().unwrap_or("").to_owned();
    unsafe { &mut *opts }.hint = Some(Hint::Name(s));
}

/// Set the index hint by key pattern document.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_hint_keys(
    opts: *mut FindOneAndOpts,
    keys: BsonView,
) {
    unsafe { &mut *opts }.hint = bson_view_to_document(keys).map(Hint::Keys);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_destroy(opts: *mut FindOneAndOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_comment(
    opts: *mut FindOneAndOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_find_one_and_opts_set_comment_value(
    opts: *mut FindOneAndOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

// ---------------------------------------------------------------------------
// CountOpts  (count_documents, estimated_document_count)
// ---------------------------------------------------------------------------

pub struct CountOpts {
    pub comment: Option<Bson>,
    pub max_time_ms: Option<u64>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_count_opts_new() -> *mut CountOpts {
    Box::into_raw(Box::new(CountOpts { comment: None, max_time_ms: None }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_count_opts_destroy(opts: *mut CountOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_count_opts_set_comment(
    opts: *mut CountOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_count_opts_set_comment_value(
    opts: *mut CountOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_count_opts_set_max_time_ms(opts: *mut CountOpts, ms: u64) {
    unsafe { &mut *opts }.max_time_ms = Some(ms);
}

// ---------------------------------------------------------------------------
// AggregateOpts  (collection aggregate, database aggregate)
// ---------------------------------------------------------------------------

pub struct AggregateOpts {
    pub comment: Option<Bson>,
    pub batch_size: Option<u32>,
    pub allow_disk_use: Option<bool>,
    pub let_vars: Option<Document>,
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_new() -> *mut AggregateOpts {
    Box::into_raw(Box::new(AggregateOpts { comment: None, batch_size: None, allow_disk_use: None, let_vars: None }))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_set_let(opts: *mut AggregateOpts, let_vars: BsonView) {
    unsafe { &mut *opts }.let_vars = bson_view_to_document(let_vars);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_set_batch_size(opts: *mut AggregateOpts, batch_size: u32) {
    unsafe { &mut *opts }.batch_size = Some(batch_size);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_set_allow_disk_use(opts: *mut AggregateOpts, val: bool) {
    unsafe { &mut *opts }.allow_disk_use = Some(val);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_destroy(opts: *mut AggregateOpts) {
    if !opts.is_null() {
        unsafe { drop(Box::from_raw(opts)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_set_comment(
    opts: *mut AggregateOpts,
    comment: *const c_char,
) {
    unsafe { &mut *opts }.comment = c_str_to_bson_string(comment);
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_aggregate_opts_set_comment_value(
    opts: *mut AggregateOpts,
    comment: *const BsonValueC,
) {
    unsafe { &mut *opts }.comment = unsafe { BsonValueC::ptr_to_bson(comment) };
}
