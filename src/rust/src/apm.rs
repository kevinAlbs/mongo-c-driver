//! Command monitoring (APM) callbacks for C callers.
//!
//! C callers register optional callbacks for command-started, command-succeeded,
//! and command-failed events.  The callbacks receive a pointer to a
//! short-lived event struct that is **only valid for the duration of the
//! callback**.  Copying any data you need before returning.
//!
//! # Usage pattern
//! ```c
//! mongoc_rust_apm_callbacks_t *apm = mongoc_rust_apm_callbacks_new();
//! mongoc_rust_apm_callbacks_set_started(apm, my_started_cb, ctx);
//! mongoc_rust_apm_callbacks_set_succeeded(apm, my_succeeded_cb, ctx);
//! mongoc_rust_apm_callbacks_set_failed(apm, my_failed_cb, ctx);
//!
//! mongoc_rust_client_t *client =
//!     mongoc_rust_client_new_with_apm("mongodb://localhost:27017", apm);
//! mongoc_rust_apm_callbacks_destroy(apm);
//! ```

use crate::bson_view::BsonView;
use mongodb::event::command::CommandEvent;
use mongodb::event::EventHandler;

use std::ffi::CString;
use std::os::raw::{c_char, c_void};

// ---------------------------------------------------------------------------
// C-visible event structs — valid only for the duration of the callback.
// ---------------------------------------------------------------------------

/// Command-started event passed to the started callback.
#[repr(C)]
pub struct CCommandStartedEvent {
    /// Null-terminated command name (e.g. "insert", "find").
    pub command_name: *const c_char,
    /// Null-terminated database name.
    pub database_name: *const c_char,
    /// Raw BSON bytes of the full command document.
    pub command: BsonView,
    /// Server-assigned request id.
    pub request_id: i32,
}

/// Command-succeeded event passed to the succeeded callback.
#[repr(C)]
pub struct CCommandSucceededEvent {
    /// Null-terminated command name.
    pub command_name: *const c_char,
    /// Raw BSON bytes of the server reply.
    pub reply: BsonView,
    /// Round-trip duration in microseconds.
    pub duration_micros: i64,
    /// Server-assigned request id.
    pub request_id: i32,
}

/// Command-failed event passed to the failed callback.
#[repr(C)]
pub struct CCommandFailedEvent {
    /// Null-terminated command name.
    pub command_name: *const c_char,
    /// Null-terminated human-readable failure message.
    pub failure_message: *const c_char,
    /// Round-trip duration in microseconds.
    pub duration_micros: i64,
    /// Server-assigned request id.
    pub request_id: i32,
}

// ---------------------------------------------------------------------------
// ApmCallbacks — the opaque struct exposed to C callers.
// ---------------------------------------------------------------------------

/// Opaque container for command-monitoring callbacks.
///
/// All fields default to null (no-op).  Pass to
/// [`mongoc_rust_client_new_with_apm`] to enable monitoring.
pub struct ApmCallbacks {
    pub(crate) started_cb:
        Option<unsafe extern "C" fn(*const CCommandStartedEvent, *mut c_void)>,
    pub(crate) succeeded_cb:
        Option<unsafe extern "C" fn(*const CCommandSucceededEvent, *mut c_void)>,
    pub(crate) failed_cb:
        Option<unsafe extern "C" fn(*const CCommandFailedEvent, *mut c_void)>,
    pub(crate) user_data: *mut c_void,
}

// Safety: the C caller is responsible for ensuring `user_data` and the
// function pointers are safe to use from any thread.
unsafe impl Send for ApmCallbacks {}
unsafe impl Sync for ApmCallbacks {}

/// Allocate a new [`ApmCallbacks`] with all callbacks unset.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_apm_callbacks_new() -> *mut ApmCallbacks {
    Box::into_raw(Box::new(ApmCallbacks {
        started_cb: None,
        succeeded_cb: None,
        failed_cb: None,
        user_data: std::ptr::null_mut(),
    }))
}

/// Free an [`ApmCallbacks`] allocated by [`mongoc_rust_apm_callbacks_new`].
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_apm_callbacks_destroy(apm: *mut ApmCallbacks) {
    if !apm.is_null() {
        unsafe { drop(Box::from_raw(apm)) }
    }
}

/// Set the command-started callback and `user_data` pointer.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_apm_callbacks_set_started(
    apm: *mut ApmCallbacks,
    cb: Option<unsafe extern "C" fn(*const CCommandStartedEvent, *mut c_void)>,
    user_data: *mut c_void,
) {
    let a = unsafe { &mut *apm };
    a.started_cb = cb;
    a.user_data = user_data;
}

/// Set the command-succeeded callback (reuses the `user_data` already set).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_apm_callbacks_set_succeeded(
    apm: *mut ApmCallbacks,
    cb: Option<unsafe extern "C" fn(*const CCommandSucceededEvent, *mut c_void)>,
) {
    unsafe { &mut *apm }.succeeded_cb = cb;
}

/// Set the command-failed callback (reuses the `user_data` already set).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_apm_callbacks_set_failed(
    apm: *mut ApmCallbacks,
    cb: Option<unsafe extern "C" fn(*const CCommandFailedEvent, *mut c_void)>,
) {
    unsafe { &mut *apm }.failed_cb = cb;
}

// ---------------------------------------------------------------------------
// Internal: build a Rust EventHandler from an ApmCallbacks.
// ---------------------------------------------------------------------------

/// Consume an [`ApmCallbacks`] and return a Rust [`EventHandler`] that
/// invokes the C callbacks for each command event.
pub(crate) fn make_event_handler(apm: ApmCallbacks) -> EventHandler<CommandEvent> {
    // Function pointer types (Option<extern "C" fn(...)>) are Send+Sync.
    // user_data (*mut c_void) is !Send; store as usize so the closure is
    // Send+Sync.  The C caller guarantees thread safety.
    let started_cb = apm.started_cb;
    let succeeded_cb = apm.succeeded_cb;
    let failed_cb = apm.failed_cb;
    let ud_usize: usize = apm.user_data as usize;

    EventHandler::callback(move |event| {
        // Restore *mut c_void from the stored usize inside the closure.
        let ud: *mut c_void = ud_usize as *mut c_void;
        match event {
            CommandEvent::Started(e) => {
                let Some(cb) = started_cb else { return };
                // Build null-terminated strings for C.
                let name_cs = CString::new(e.command_name.as_str()).unwrap_or_default();
                let db_cs = CString::new(e.db.as_str()).unwrap_or_default();
                // Serialize command document to BSON bytes.
                let mut cmd_bytes: Vec<u8> = Vec::new();
                if e.command.to_writer(&mut cmd_bytes).is_err() {
                    cmd_bytes.clear();
                }
                let c_event = CCommandStartedEvent {
                    command_name: name_cs.as_ptr(),
                    database_name: db_cs.as_ptr(),
                    command: BsonView {
                        data: cmd_bytes.as_ptr() as *const c_void,
                        len: cmd_bytes.len(),
                    },
                    request_id: e.request_id,
                };
                unsafe { cb(&c_event, ud) };
                // name_cs, db_cs, cmd_bytes are dropped here.
            }
            CommandEvent::Succeeded(e) => {
                let Some(cb) = succeeded_cb else { return };
                let name_cs = CString::new(e.command_name.as_str()).unwrap_or_default();
                let mut reply_bytes: Vec<u8> = Vec::new();
                if e.reply.to_writer(&mut reply_bytes).is_err() {
                    reply_bytes.clear();
                }
                let micros = e.duration.as_micros() as i64;
                let c_event = CCommandSucceededEvent {
                    command_name: name_cs.as_ptr(),
                    reply: BsonView {
                        data: reply_bytes.as_ptr() as *const c_void,
                        len: reply_bytes.len(),
                    },
                    duration_micros: micros,
                    request_id: e.request_id,
                };
                unsafe { cb(&c_event, ud) };
            }
            CommandEvent::Failed(e) => {
                let Some(cb) = failed_cb else { return };
                let name_cs = CString::new(e.command_name.as_str()).unwrap_or_default();
                let msg_cs =
                    CString::new(e.failure.to_string().as_str()).unwrap_or_default();
                let micros = e.duration.as_micros() as i64;
                let c_event = CCommandFailedEvent {
                    command_name: name_cs.as_ptr(),
                    failure_message: msg_cs.as_ptr(),
                    duration_micros: micros,
                    request_id: e.request_id,
                };
                unsafe { cb(&c_event, ud) };
            }
            // CommandEvent is non_exhaustive; ignore any future variants.
            _ => {}
        }
    })
}
