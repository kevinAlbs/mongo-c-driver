use crate::apm::{ApmCallbacks, make_event_handler};
use crate::bson::{bson_malloc, bson_strndup};
use crate::bson_owned::BsonOwned;
use crate::bson_view::BsonView;
use crate::database::Database;
use crate::log;
use crate::runtime_handle::RuntimeHandle;

use std::ffi::{CStr, c_char};
use std::sync::Arc;

use tokio;

use mongodb;
use mongodb::bson::{Array, Bson, Document};
use mongodb::options::{
    ClientOptions, DeleteManyModel, DeleteOneModel, InsertOneModel, ReplaceOneModel,
    UpdateManyModel, UpdateModifications, UpdateOneModel, WriteModel,
};
use mongodb::Namespace;

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "client";

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_new(uri_string: *const c_char) -> *mut Client {
    let uri_string = unsafe { CStr::from_ptr(uri_string) }.to_str().unwrap();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all() // Enable I/O drivers and timers.
        .build()
        .unwrap();
    let _guard = runtime.enter();

    match runtime.block_on(mongodb::Client::with_uri_str(uri_string)) {
        Ok(client) => Box::into_raw(Box::new(Client::new(runtime, client))),
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

/// Create a client with command-monitoring callbacks.
///
/// `apm` may be null (no monitoring).  The callbacks struct is consumed
/// (copied) and need not outlive this call.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_new_with_apm(
    uri_string: *const c_char,
    apm: *const ApmCallbacks,
) -> *mut Client {
    let uri_string = unsafe { CStr::from_ptr(uri_string) }.to_str().unwrap();

    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap();

    // Parse the URI string into ClientOptions (async because SRV may do DNS).
    // ClientOptions::parse returns ParseConnectionString (an Action type),
    // which implements IntoFuture; wrap in an async block for block_on.
    let _guard = runtime.enter();
    let mut options = match runtime.block_on(async { ClientOptions::parse(uri_string).await }) {
        Ok(o) => o,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };

    if !apm.is_null() {
        // Safety: caller guarantees the pointer is valid; we copy it.
        let apm_copy = unsafe {
            let a = &*apm;
            ApmCallbacks {
                started_cb: a.started_cb,
                succeeded_cb: a.succeeded_cb,
                failed_cb: a.failed_cb,
                user_data: a.user_data,
            }
        };
        options.command_event_handler = Some(make_event_handler(apm_copy));
    }

    // Client::with_options is synchronous.
    match mongodb::Client::with_options(options) {
        Ok(client) => Box::into_raw(Box::new(Client::new(runtime, client))),
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_destroy(client: *mut Client) {
    if !client.is_null() {
        unsafe { drop(Box::from_raw(client)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_get_database<'r>(
    client: *mut Client,
    name: *const c_char,
) -> *mut Database<'r> {
    let client = unsafe { &*client };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    Box::into_raw(Box::new(client.database(name)))
}

/// Create a Database with a specific read preference.
///
/// `read_preference_mode`: one of "primary", "primaryPreferred", "secondary",
/// "secondaryPreferred", "nearest".  Case-insensitive.
/// `max_staleness_seconds`: -1 = no limit; >= 0 = max staleness in seconds.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_get_database_with_opts<'r>(
    client: *mut Client,
    name: *const c_char,
    read_preference_mode: *const c_char,
    max_staleness_seconds: i64,
) -> *mut Database<'r> {
    use mongodb::options::{DatabaseOptions, ReadPreference, ReadPreferenceOptions, SelectionCriteria};

    let c = unsafe { &*client };
    let name_str = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    let rp_opts = if max_staleness_seconds >= 0 {
        Some(
            ReadPreferenceOptions::builder()
                .max_staleness(std::time::Duration::from_secs(max_staleness_seconds as u64))
                .build(),
        )
    } else {
        None
    };

    let rp: ReadPreference = if !read_preference_mode.is_null() {
        let mode = unsafe { CStr::from_ptr(read_preference_mode) }
            .to_str()
            .unwrap_or("primary");
        match mode.to_lowercase().as_str() {
            "secondary" => ReadPreference::Secondary { options: rp_opts },
            "secondarypreferred" => ReadPreference::SecondaryPreferred { options: rp_opts },
            "primarypreferred" => ReadPreference::PrimaryPreferred { options: rp_opts },
            "nearest" => ReadPreference::Nearest { options: rp_opts },
            _ => ReadPreference::Primary,
        }
    } else {
        ReadPreference::Primary
    };

    let options = DatabaseOptions::builder()
        .selection_criteria(SelectionCriteria::ReadPreference(rp))
        .build();
    let db = c.client.database_with_options(name_str, options);
    Box::into_raw(Box::new(crate::database::Database::new(&*c.runtime, db)))
}

/// Start a new client session.
///
/// Returns an owned session that the caller must destroy with
/// [`mongoc_rust_session_destroy`].  On failure `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_start_session_await(
    client: *mut Client,
    error: *mut *mut crate::error::Error,
) -> *mut crate::session::Session {
    let c = unsafe { &*client };
    match c.runtime.block_on(async { c.client.start_session().await }) {
        Ok(inner) => Box::into_raw(Box::new(crate::session::Session {
            runtime: &*c.runtime as *const _,
            inner,
        })),
        Err(e) => {
            if !error.is_null() {
                unsafe {
                    *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e)));
                }
            }
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

/// Return a null-terminated array of database name strings.
///
/// Returns a `char **` that the caller must free: each element with `bson_free`,
/// then the array itself with `bson_free`.  Returns null on error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_get_database_names_await(
    client: *mut Client,
) -> *mut *mut c_char {
    let c = unsafe { &*client };
    let mut names = match c.runtime.block_on(async move {
        c.client.list_database_names().await
    }) {
        Ok(n) => n,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };
    names.sort();
    unsafe {
        let ret =
            bson_malloc(std::mem::size_of::<*mut c_char>() * (names.len() + 1)) as *mut *mut c_char;
        for (i, name) in names.iter().enumerate() {
            let bytes = name.as_bytes();
            *ret.add(i) = bson_strndup(bytes.as_ptr() as *const c_char, bytes.len());
        }
        *ret.add(names.len()) = std::ptr::null_mut();
        ret
    }
}

/// Parse a namespace string "db.coll" into a `Namespace`.
fn parse_namespace(s: &str) -> Option<Namespace> {
    let dot = s.find('.')?;
    Some(Namespace {
        db: s[..dot].to_owned(),
        coll: s[dot + 1..].to_owned(),
    })
}

/// Parse a BSON value (document or array) as `UpdateModifications`.
fn parse_update_mods(bson: &Bson) -> Option<UpdateModifications> {
    match bson {
        Bson::Document(d) => Some(UpdateModifications::Document(d.clone())),
        Bson::Array(arr) => {
            let mut stages: Vec<Document> = Vec::new();
            for item in arr {
                if let Bson::Document(d) = item {
                    stages.push(d.clone());
                } else {
                    return None;
                }
            }
            Some(UpdateModifications::Pipeline(stages))
        }
        _ => None,
    }
}

/// Parse a BSON value as an index hint: either a string name or a document key pattern.
fn parse_hint(bson: &Bson) -> Option<Bson> {
    match bson {
        Bson::String(_) | Bson::Document(_) => Some(bson.clone()),
        _ => None,
    }
}

/// Parse a BSON document `{ "w": ... }` into a `WriteConcern`.
fn parse_write_concern(doc: &Document) -> Option<mongodb::options::WriteConcern> {
    use mongodb::options::{Acknowledgment, WriteConcern};
    let w = match doc.get("w") {
        Some(Bson::String(s)) if s == "majority" => Acknowledgment::Majority,
        Some(Bson::Int32(n)) => Acknowledgment::Nodes(*n as u32),
        Some(Bson::Int64(n)) => Acknowledgment::Nodes(*n as u32),
        _ => return None,
    };
    Some(WriteConcern::builder().w(w).build())
}

/// Parse `arrayFilters` from a BSON array.
fn parse_array_filters(arr: &Array) -> Option<Array> {
    let mut out: Array = Vec::new();
    for item in arr {
        if let Bson::Document(_) = item {
            out.push(item.clone());
        } else {
            return None;
        }
    }
    Some(out)
}

/// Serialize a `VerboseBulkWriteResult` to a BSON document.
fn doc_from_verbose(res: &mongodb::results::VerboseBulkWriteResult) -> Document {
    let mut doc = Document::new();
    doc.insert("insertedCount", res.summary.inserted_count);
    doc.insert("upsertedCount", res.summary.upserted_count);
    doc.insert("matchedCount", res.summary.matched_count);
    doc.insert("modifiedCount", res.summary.modified_count);
    doc.insert("deletedCount", res.summary.deleted_count);

    let mut insert_results = Document::new();
    for (idx, ir) in &res.insert_results {
        let mut entry = Document::new();
        entry.insert("insertedId", ir.inserted_id.clone());
        insert_results.insert(idx.to_string(), entry);
    }
    doc.insert("insertResults", insert_results);

    let mut update_results = Document::new();
    for (idx, ur) in &res.update_results {
        let mut entry = Document::new();
        entry.insert("matchedCount", ur.matched_count as i64);
        entry.insert("modifiedCount", ur.modified_count as i64);
        if let Some(ref uid) = ur.upserted_id {
            entry.insert("upsertedId", uid.clone());
        }
        update_results.insert(idx.to_string(), entry);
    }
    doc.insert("updateResults", update_results);

    let mut delete_results = Document::new();
    for (idx, dr) in &res.delete_results {
        let mut entry = Document::new();
        entry.insert("deletedCount", dr.deleted_count as i64);
        delete_results.insert(idx.to_string(), entry);
    }
    doc.insert("deleteResults", delete_results);

    doc
}

/// Append `BulkWriteError.write_errors` into an existing Document as `writeErrors`.
fn append_write_errors(doc: &mut Document, bwe: &mongodb::error::BulkWriteError) {
    if !bwe.write_errors.is_empty() {
        let mut we = Document::new();
        for (idx, err) in &bwe.write_errors {
            let mut entry = Document::new();
            entry.insert("code", err.code);
            we.insert(idx.to_string(), entry);
        }
        doc.insert("writeErrors", we);
    }
}

/// Serialize a `SummaryBulkWriteResult` to a BSON document with empty per-op maps.
fn doc_from_summary(res: &mongodb::results::SummaryBulkWriteResult) -> Document {
    let mut doc = Document::new();
    doc.insert("insertedCount", res.inserted_count);
    doc.insert("upsertedCount", res.upserted_count);
    doc.insert("matchedCount", res.matched_count);
    doc.insert("modifiedCount", res.modified_count);
    doc.insert("deletedCount", res.deleted_count);
    doc.insert("insertResults", Document::new());
    doc.insert("updateResults", Document::new());
    doc.insert("deleteResults", Document::new());
    doc
}

/// Execute a client-level bulk write operation.
///
/// `args` is a BSON document with:
/// - `models`: array of write models (each is a doc with one key = model type)
/// - `verboseResults`: bool (optional, default false)
/// - `ordered`: bool (optional, default true)
/// - `comment`: Bson (optional)
/// - `let`: Document (optional)
///
/// On partial failure (some ops succeed, some fail), the partial result is written
/// to `*partial_result_out` (if non-null). The caller must destroy it.
///
/// Returns a `*mut BsonOwned` containing the serialized result, or null on error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_bulk_write_await(
    client: *mut Client,
    args: BsonView,
    error: *mut *mut crate::error::Error,
    partial_result_out: *mut *mut BsonOwned,
) -> *mut BsonOwned {
    // Initialize out-params.
    if !partial_result_out.is_null() {
        unsafe { *partial_result_out = std::ptr::null_mut(); }
    }
    let c = unsafe { &*client };
    let bytes =
        unsafe { std::slice::from_raw_parts(args.data as *const u8, args.len) };
    let args_doc = match Document::from_reader(std::io::Cursor::new(bytes)) {
        Ok(d) => d,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };

    // Parse models array.
    let models_bson = match args_doc.get_array("models") {
        Ok(arr) => arr.clone(),
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: {}", e));
            return std::ptr::null_mut();
        }
    };

    let mut models: Vec<WriteModel> = Vec::with_capacity(models_bson.len());
    for (i, item) in models_bson.iter().enumerate() {
        let model_doc = match item {
            Bson::Document(d) => d,
            _ => {
                log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: model[{}] not a doc", i));
                return std::ptr::null_mut();
            }
        };

        // Each model doc has exactly one key: the model type name.
        let (model_type, model_args) = match model_doc.iter().next() {
            Some((k, Bson::Document(v))) => (k.as_str(), v),
            _ => {
                log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: model[{}] invalid", i));
                return std::ptr::null_mut();
            }
        };

        let ns_str = match model_args.get_str("namespace") {
            Ok(s) => s,
            Err(e) => {
                log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: model[{}] namespace: {}", i, e));
                return std::ptr::null_mut();
            }
        };
        let ns = match parse_namespace(ns_str) {
            Some(n) => n,
            None => {
                log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: invalid namespace '{}'", ns_str));
                return std::ptr::null_mut();
            }
        };

        let model: WriteModel = match model_type {
            "insertOne" => {
                let document = match model_args.get_document("document").cloned() {
                    Ok(d) => d,
                    Err(e) => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: insertOne document: {}", e));
                        return std::ptr::null_mut();
                    }
                };
                WriteModel::InsertOne(
                    InsertOneModel::builder()
                        .namespace(ns)
                        .document(document)
                        .build(),
                )
            }
            "updateOne" | "updateMany" => {
                let filter = match model_args.get_document("filter").cloned() {
                    Ok(d) => d,
                    Err(e) => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: {}.filter: {}", model_type, e));
                        return std::ptr::null_mut();
                    }
                };
                let update_bson = match model_args.get("update") {
                    Some(b) => b,
                    None => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: {} missing update", model_type));
                        return std::ptr::null_mut();
                    }
                };
                let update = match parse_update_mods(update_bson) {
                    Some(u) => u,
                    None => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: {} invalid update", model_type));
                        return std::ptr::null_mut();
                    }
                };
                let hint = model_args.get("hint").and_then(|b| parse_hint(b));
                let upsert = model_args.get_bool("upsert").ok();
                let sort = model_args.get_document("sort").cloned().ok();
                let array_filters: Option<Array> = model_args
                    .get_array("arrayFilters")
                    .ok()
                    .and_then(|arr| parse_array_filters(arr));
                let collation = model_args.get_document("collation").cloned().ok();

                if model_type == "updateOne" {
                    WriteModel::UpdateOne(
                        UpdateOneModel::builder()
                            .namespace(ns)
                            .filter(filter)
                            .update(update)
                            .array_filters(array_filters)
                            .collation(collation)
                            .hint(hint)
                            .upsert(upsert)
                            .sort(sort)
                            .build(),
                    )
                } else {
                    WriteModel::UpdateMany(
                        UpdateManyModel::builder()
                            .namespace(ns)
                            .filter(filter)
                            .update(update)
                            .array_filters(array_filters)
                            .collation(collation)
                            .hint(hint)
                            .upsert(upsert)
                            .build(),
                    )
                }
            }
            "replaceOne" => {
                let filter = match model_args.get_document("filter").cloned() {
                    Ok(d) => d,
                    Err(e) => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: replaceOne filter: {}", e));
                        return std::ptr::null_mut();
                    }
                };
                let replacement = match model_args.get_document("replacement").cloned() {
                    Ok(d) => d,
                    Err(e) => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: replaceOne replacement: {}", e));
                        return std::ptr::null_mut();
                    }
                };
                let hint = model_args.get("hint").and_then(|b| parse_hint(b));
                let upsert = model_args.get_bool("upsert").ok();
                let sort = model_args.get_document("sort").cloned().ok();
                let collation = model_args.get_document("collation").cloned().ok();
                WriteModel::ReplaceOne(
                    ReplaceOneModel::builder()
                        .namespace(ns)
                        .filter(filter)
                        .replacement(replacement)
                        .collation(collation)
                        .hint(hint)
                        .upsert(upsert)
                        .sort(sort)
                        .build(),
                )
            }
            "deleteOne" | "deleteMany" => {
                let filter = match model_args.get_document("filter").cloned() {
                    Ok(d) => d,
                    Err(e) => {
                        log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: {}.filter: {}", model_type, e));
                        return std::ptr::null_mut();
                    }
                };
                let hint = model_args.get("hint").and_then(|b| parse_hint(b));
                let collation = model_args.get_document("collation").cloned().ok();
                if model_type == "deleteOne" {
                    WriteModel::DeleteOne(
                        DeleteOneModel::builder()
                            .namespace(ns)
                            .filter(filter)
                            .collation(collation)
                            .hint(hint)
                            .build(),
                    )
                } else {
                    WriteModel::DeleteMany(
                        DeleteManyModel::builder()
                            .namespace(ns)
                            .filter(filter)
                            .collation(collation)
                            .hint(hint)
                            .build(),
                    )
                }
            }
            other => {
                log::error(MONGOC_LOG_DOMAIN, &format!("clientBulkWrite: unknown model '{}'", other));
                return std::ptr::null_mut();
            }
        };
        models.push(model);
    }

    // Parse operation-level options.
    let ordered = args_doc.get_bool("ordered").ok();
    let let_vars = args_doc.get_document("let").cloned().ok();
    let comment = args_doc.get("comment").cloned();
    let bypass_doc_validation = args_doc.get_bool("bypassDocumentValidation").ok();
    let write_concern: Option<mongodb::options::WriteConcern> = args_doc
        .get_document("writeConcern")
        .ok()
        .and_then(|wc_doc| parse_write_concern(wc_doc));
    let verbose = args_doc.get_bool("verboseResults").unwrap_or(false);

    // Client-side validation for unacknowledged write concern (w=0).
    let is_unacknowledged = write_concern.as_ref()
        .and_then(|wc| wc.w.as_ref())
        .map(|w| matches!(w, mongodb::options::Acknowledgment::Nodes(0)))
        .unwrap_or(false);
    if is_unacknowledged {
        let msg = if verbose {
            "Cannot request unacknowledged write concern and verbose results"
        } else if ordered.unwrap_or(true) {
            // ordered defaults to true per spec; w=0 + ordered=true is invalid.
            "Cannot request unacknowledged write concern and ordered writes"
        } else {
            ""
        };
        if !msg.is_empty() {
            if !error.is_null() {
                unsafe { *error = Box::into_raw(Box::new(crate::error::Error::client_error(msg))); }
            }
            log::error(MONGOC_LOG_DOMAIN, msg);
            return std::ptr::null_mut();
        }
    }

    if verbose {
        // Verbose results: sends errorsOnly: false in the wire command.
        // Returns per-operation result maps.
        let result = c.runtime.block_on(async move {
            let mut op = c.client.bulk_write(models).verbose_results();
            if let Some(v) = ordered { op = op.ordered(v); }
            if let Some(d) = let_vars { op = op.let_vars(d); }
            if let Some(bval) = comment { op = op.comment(bval); }
            if let Some(v) = bypass_doc_validation { op = op.bypass_document_validation(v); }
            if let Some(wc) = write_concern { op = op.write_concern(wc); }
            op.await
        });

        match result {
            Ok(res) => {
                let doc = doc_from_verbose(&res);
                match BsonOwned::from_doc(&doc) {
                    Some(b) => Box::into_raw(Box::new(b)),
                    None => {
                        log::error(MONGOC_LOG_DOMAIN, "clientBulkWrite: failed to serialize result");
                        std::ptr::null_mut()
                    }
                }
            }
            Err(e) => {
                // Build partial result from BulkWriteError (partial results + write errors).
                if !partial_result_out.is_null() {
                    if let mongodb::error::ErrorKind::BulkWrite(bwe) = e.kind.as_ref() {
                        let mut doc = match &bwe.partial_result {
                            Some(mongodb::error::PartialBulkWriteResult::Verbose(vr)) => doc_from_verbose(vr),
                            _ => Document::new(),
                        };
                        append_write_errors(&mut doc, bwe);
                        if !doc.is_empty() {
                            if let Some(b) = BsonOwned::from_doc(&doc) {
                                unsafe { *partial_result_out = Box::into_raw(Box::new(b)); }
                            }
                        }
                    }
                }
                if !error.is_null() {
                    unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e))); }
                }
                log::error(MONGOC_LOG_DOMAIN, &e.to_string());
                std::ptr::null_mut()
            }
        }
    } else {
        // Non-verbose (summary) results: sends errorsOnly: true in the wire command.
        // Returns aggregate counts only; per-op result maps are empty.
        let result = c.runtime.block_on(async move {
            let mut op = c.client.bulk_write(models);
            if let Some(v) = ordered { op = op.ordered(v); }
            if let Some(d) = let_vars { op = op.let_vars(d); }
            if let Some(bval) = comment { op = op.comment(bval); }
            if let Some(v) = bypass_doc_validation { op = op.bypass_document_validation(v); }
            if let Some(wc) = write_concern { op = op.write_concern(wc); }
            op.await
        });

        match result {
            Ok(res) => {
                let doc = doc_from_summary(&res);
                match BsonOwned::from_doc(&doc) {
                    Some(b) => Box::into_raw(Box::new(b)),
                    None => {
                        log::error(MONGOC_LOG_DOMAIN, "clientBulkWrite: failed to serialize result");
                        std::ptr::null_mut()
                    }
                }
            }
            Err(e) => {
                // Build partial result from BulkWriteError (partial results + write errors).
                if !partial_result_out.is_null() {
                    if let mongodb::error::ErrorKind::BulkWrite(bwe) = e.kind.as_ref() {
                        let mut doc = match &bwe.partial_result {
                            Some(mongodb::error::PartialBulkWriteResult::Summary(sr)) => doc_from_summary(sr),
                            _ => Document::new(),
                        };
                        append_write_errors(&mut doc, bwe);
                        if !doc.is_empty() {
                            if let Some(b) = BsonOwned::from_doc(&doc) {
                                unsafe { *partial_result_out = Box::into_raw(Box::new(b)); }
                            }
                        }
                    }
                }
                if !error.is_null() {
                    unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e))); }
                }
                log::error(MONGOC_LOG_DOMAIN, &e.to_string());
                std::ptr::null_mut()
            }
        }
    }
}

pub struct Client {
    pub(crate) runtime: Arc<tokio::runtime::Runtime>,
    client: mongodb::Client,
}

impl Client {
    fn new(runtime: tokio::runtime::Runtime, client: mongodb::Client) -> Self {
        Self { runtime: Arc::new(runtime), client }
    }

    fn database<'r>(&'r self, name: &str) -> Database<'r> {
        Database::new(&*self.runtime, self.client.database(name))
    }
}

/// Obtain a thread-safe progress handle for this client's runtime.
///
/// The returned handle shares ownership of the runtime (via [`Arc`]) and may
/// be passed to any thread.  Destroy it with
/// [`mongoc_rust_runtime_handle_destroy`] when done.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_client_get_runtime_handle(client: *mut Client) -> *mut RuntimeHandle {
    let c = unsafe { &*client };
    Box::into_raw(Box::new(RuntimeHandle { runtime: Arc::clone(&c.runtime) }))
}
