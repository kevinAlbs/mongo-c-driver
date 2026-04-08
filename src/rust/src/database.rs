use crate::bson::{bson_malloc, bson_strndup};
use crate::bson_owned::BsonOwned;
use crate::bson_view::BsonView;
use crate::collection::Collection;
use crate::cursor::Cursor;
use crate::future::{Future, FutureValue, FutureValueType};
use crate::log;
use crate::options::AggregateOpts;

use std::ffi::{CStr, CString, c_char};

use async_ffi::FutureExt;

use mongodb;

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "database";

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_destroy(database: *mut Database) {
    if !database.is_null() {
        unsafe { drop(Box::from_raw(database)) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_get_name(database: *mut Database) -> *const c_char {
    let name = &unsafe { &*database }.name;

    name.as_ptr() as *const c_char
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_drop_await(database: *mut Database) -> bool {
    let database = &unsafe { &*database };

    match database
        .runtime
        .block_on(async move { database.db.drop().await })
    {
        Ok(_) => true,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            false
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_drop<'r>(
    database: *mut Database<'r>,
) -> *mut Future<'r>
{
    let Database { runtime, db, .. } = &unsafe { &*database };

    let future = Future {
        runtime: &runtime,
        value: FutureValue::Void(FutureValueType {
            future: async move { db.drop().await }.into_ffi(),
            result: None,
        }),
    };

    Box::into_raw(Box::new(future))
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_create_collection_await(
    database: *mut Database,
    name: *const c_char,
) -> *mut Collection {
    let database = &unsafe { &*database };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    match database
        .runtime
        .block_on(async move { database.db.create_collection(name).await })
    {
        Ok(_) => Box::into_raw(Box::new(database.collection(name))),
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_get_collection_names_with_opts_await(
    database: *mut Database,
) -> *mut *mut c_char {
    let database = &unsafe { &*database };

    let mut names = match database
        .runtime
        .block_on(async move { database.db.list_collection_names().await })
    {
        Ok(names) => names,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };
    names.sort(); // For `test_database_create_collection()`.
    let names = names;

    unsafe {
        let ret =
            bson_malloc(std::mem::size_of::<*mut c_char>() * (names.len() + 1)) as *mut *mut c_char;

        for (i, name) in (&names).into_iter().enumerate() {
            let bytes = name.as_bytes();
            *ret.add(i) = bson_strndup(bytes.as_ptr() as *const c_char, bytes.len());
        }

        *ret.add(names.len()) = std::ptr::null_mut();

        ret
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_get_collection<'r>(
    database: *mut Database<'r>,
    name: *const c_char,
) -> *mut Collection<'r> {
    let database = &unsafe { &*database };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    Box::into_raw(Box::new(database.collection(name)))
}

/// Create a Collection with custom write concern and/or read concern.
///
/// `write_concern_w`: -1 = no write concern, 0 = unacknowledged, 1+ = node count.
/// `read_concern_level`: null = no read concern; otherwise e.g. "majority", "local".
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_get_collection_with_opts<'r>(
    database: *mut Database<'r>,
    name: *const c_char,
    write_concern_w: i32,
    read_concern_level: *const c_char,
) -> *mut Collection<'r> {
    let database = &unsafe { &*database };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    let wc: Option<mongodb::options::WriteConcern> = if write_concern_w >= 0 {
        Some(
            mongodb::options::WriteConcern::builder()
                .w(mongodb::options::Acknowledgment::Nodes(write_concern_w as u32))
                .build(),
        )
    } else {
        None
    };
    let rc: Option<mongodb::options::ReadConcern> = if !read_concern_level.is_null() {
        let level_str = unsafe { CStr::from_ptr(read_concern_level) }.to_str().unwrap();
        Some(mongodb::options::ReadConcern::custom(level_str))
    } else {
        None
    };
    let options = mongodb::options::CollectionOptions::builder()
        .write_concern(wc)
        .read_concern(rc)
        .build();

    let coll = database
        .db
        .collection_with_options::<mongodb::bson::Document>(name, options);
    Box::into_raw(Box::new(Collection::new(&database.runtime, coll)))
}

/// Create a Collection with a specific read preference (and optional write/read concern).
///
/// `read_preference_mode`: "primary", "primaryPreferred", "secondary",
/// "secondaryPreferred", "nearest".  Null = no read preference override.
/// `max_staleness_seconds`: -1 = no limit; >= 0 = max staleness in seconds.
/// `write_concern_w`: -1 = no write concern.
/// `read_concern_level`: null = no read concern.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_get_collection_with_read_preference<'r>(
    database: *mut Database<'r>,
    name: *const c_char,
    read_preference_mode: *const c_char,
    max_staleness_seconds: i64,
    write_concern_w: i32,
    read_concern_level: *const c_char,
) -> *mut Collection<'r> {
    use mongodb::options::{ReadPreference, ReadPreferenceOptions, SelectionCriteria};

    let database = &unsafe { &*database };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    let rp_opts = if max_staleness_seconds >= 0 {
        Some(
            ReadPreferenceOptions::builder()
                .max_staleness(std::time::Duration::from_secs(
                    max_staleness_seconds as u64,
                ))
                .build(),
        )
    } else {
        None
    };

    let selection_criteria: Option<SelectionCriteria> = if !read_preference_mode.is_null() {
        let mode = unsafe { CStr::from_ptr(read_preference_mode) }
            .to_str()
            .unwrap_or("primary");
        let rp = match mode.to_lowercase().as_str() {
            "secondary" => ReadPreference::Secondary { options: rp_opts },
            "secondarypreferred" => ReadPreference::SecondaryPreferred { options: rp_opts },
            "primarypreferred" => ReadPreference::PrimaryPreferred { options: rp_opts },
            "nearest" => ReadPreference::Nearest { options: rp_opts },
            _ => ReadPreference::Primary,
        };
        Some(SelectionCriteria::ReadPreference(rp))
    } else {
        None
    };

    let wc: Option<mongodb::options::WriteConcern> = if write_concern_w >= 0 {
        Some(
            mongodb::options::WriteConcern::builder()
                .w(mongodb::options::Acknowledgment::Nodes(
                    write_concern_w as u32,
                ))
                .build(),
        )
    } else {
        None
    };
    let rc: Option<mongodb::options::ReadConcern> = if !read_concern_level.is_null() {
        let level_str =
            unsafe { CStr::from_ptr(read_concern_level) }.to_str().unwrap();
        Some(mongodb::options::ReadConcern::custom(level_str))
    } else {
        None
    };

    let options = mongodb::options::CollectionOptions::builder()
        .selection_criteria(selection_criteria)
        .write_concern(wc)
        .read_concern(rc)
        .build();

    let coll = database
        .db
        .collection_with_options::<mongodb::bson::Document>(name, options);
    Box::into_raw(Box::new(Collection::new(&database.runtime, coll)))
}

/// Drop a collection by name.
///
/// Returns true on success, false on error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_drop_collection_await(
    database: *mut Database,
    name: *const c_char,
) -> bool {
    let database = &unsafe { &*database };
    let name = unsafe {
        assert!(!name.is_null());
        CStr::from_ptr(name)
    }
    .to_str()
    .unwrap();

    let coll = database.db.collection::<mongodb::bson::Document>(name);
    match database.runtime.block_on(async move { coll.drop().await }) {
        Ok(_) => true,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            false
        }
    }
}

/// Execute a command on this database and return the reply document.
///
/// Returns null on error; `error` (if non-null) is set.
/// The caller must destroy a non-null return value with [`mongoc_rust_bson_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_run_command_await(
    database: *mut Database,
    command: BsonView,
    error: *mut *mut crate::error::Error,
) -> *mut BsonOwned {
    let database = &unsafe { &*database };
    let bytes = unsafe { std::slice::from_raw_parts(command.data as *const u8, command.len) };
    let command_doc = match mongodb::bson::Document::from_reader(std::io::Cursor::new(bytes)) {
        Ok(d) => d,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };

    match database.runtime.block_on(async move { database.db.run_command(command_doc).await }) {
        Ok(reply) => match BsonOwned::from_doc(&reply) {
            Some(b) => Box::into_raw(Box::new(b)),
            None => {
                log::error(MONGOC_LOG_DOMAIN, "failed to serialize run_command reply");
                std::ptr::null_mut()
            }
        },
        Err(e) => {
            if !error.is_null() {
                unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e))) };
            }
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

/// Execute a command asynchronously and return a future that resolves to the reply.
///
/// Returns an `OptionalDocument` future; use [`mongoc_rust_future_get_bson`]
/// to retrieve the reply document (null on error).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_run_command<'r>(
    database: *mut Database<'r>,
    command: BsonView,
) -> *mut Future<'r> {
    let Database { runtime, db, .. } = &unsafe { &*database };
    let bytes =
        unsafe { std::slice::from_raw_parts(command.data as *const u8, command.len) }.to_owned();
    let command_doc = match mongodb::bson::Document::from_reader(std::io::Cursor::new(&bytes)) {
        Ok(d) => d,
        Err(e) => {
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            return std::ptr::null_mut();
        }
    };

    let future = Future {
        runtime,
        value: FutureValue::OptionalDocument(FutureValueType {
            future: async move { db.run_command(command_doc).await.map(Some) }.into_ffi(),
            result: None,
        }),
    };
    Box::into_raw(Box::new(future))
}

/// Run an aggregation pipeline on the database and return a cursor.
///
/// `pipeline` is a C array of `n_stages` [`bson_rust_view_t`] values.
/// `opts` may be null (default options).
/// Returns null on error; `error` (if non-null) is set.
/// The caller must destroy the cursor with [`mongoc_rust_cursor_destroy`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_database_aggregate_await<'r>(
    database: *mut Database<'r>,
    pipeline: *const BsonView,
    n_stages: usize,
    opts: *const AggregateOpts,
    error: *mut *mut crate::error::Error,
) -> *mut Cursor<'r> {
    let Database { runtime, db, .. } = &unsafe { &*database };
    let views = unsafe { std::slice::from_raw_parts(pipeline, n_stages) };
    let opts = unsafe { opts.as_ref() };

    let mut stages = Vec::with_capacity(n_stages);
    for view in views {
        let bytes = unsafe { std::slice::from_raw_parts(view.data as *const u8, view.len) };
        match mongodb::bson::Document::from_reader(std::io::Cursor::new(bytes)) {
            Ok(d) => stages.push(d),
            Err(e) => {
                log::error(MONGOC_LOG_DOMAIN, &e.to_string());
                return std::ptr::null_mut();
            }
        }
    }

    match runtime.block_on(async move {
        let mut op = db.aggregate(stages);
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
            if !error.is_null() {
                unsafe { *error = Box::into_raw(Box::new(crate::error::Error::from_mongodb(&e))) };
            }
            log::error(MONGOC_LOG_DOMAIN, &e.to_string());
            std::ptr::null_mut()
        }
    }
}

pub struct Database<'r> {
    runtime: &'r tokio::runtime::Runtime,
    db: mongodb::Database,
    name: CString, // Allow `mongoc_rust_database_get_name()` to return `*const c_char`.
}

impl<'r> Database<'r> {
    pub fn new(runtime: &'r tokio::runtime::Runtime, db: mongodb::Database) -> Self {
        let name = CString::new(db.name()).unwrap();

        Self { runtime, db, name }
    }

    fn collection(&'r self, name: &str) -> Collection<'r> {
        Collection::new(
            &self.runtime,
            self.db.collection::<mongodb::bson::Document>(name),
        )
    }
}
