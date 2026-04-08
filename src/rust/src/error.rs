use mongodb::error::{ErrorKind, WriteFailure};
use std::ffi::CString;
use std::os::raw::c_char;

/// Error code indicating the category of a [`mongoc_rust_error_t`].
#[repr(C)]
#[derive(Copy, Clone)]
pub enum ErrorCode {
    Unknown = 0,
    InvalidArgument = 1,
    Authentication = 2,
    BsonDeserialization = 3,
    BsonSerialization = 4,
    Bson = 5,
    InsertMany = 6,
    BulkWrite = 7,
    Command = 8,
    DnsResolve = 9,
    GridFs = 10,
    Internal = 11,
    Io = 12,
    ConnectionPoolCleared = 13,
    InvalidResponse = 14,
    ServerSelection = 15,
    SessionsNotSupported = 16,
    InvalidTlsConfig = 17,
    Write = 18,
    Transaction = 19,
    IncompatibleServer = 20,
    MissingResumeToken = 21,
    Encryption = 22,
    Custom = 23,
    Shutdown = 24,
    ProxyConnect = 25,
}

impl ErrorCode {
    fn from_kind(kind: &ErrorKind) -> Self {
        match kind {
            ErrorKind::InvalidArgument { .. } => Self::InvalidArgument,
            ErrorKind::Authentication { .. } => Self::Authentication,
            ErrorKind::BsonDeserialization(_) => Self::BsonDeserialization,
            ErrorKind::BsonSerialization(_) => Self::BsonSerialization,
            ErrorKind::Bson(_) => Self::Bson,
            ErrorKind::InsertMany(_) => Self::InsertMany,
            ErrorKind::BulkWrite(_) => Self::BulkWrite,
            ErrorKind::Command(_) => Self::Command,
            ErrorKind::DnsResolve { .. } => Self::DnsResolve,
            ErrorKind::GridFs(_) => Self::GridFs,
            ErrorKind::Internal { .. } => Self::Internal,
            ErrorKind::Io(_) => Self::Io,
            ErrorKind::ConnectionPoolCleared { .. } => Self::ConnectionPoolCleared,
            ErrorKind::InvalidResponse { .. } => Self::InvalidResponse,
            ErrorKind::ServerSelection { .. } => Self::ServerSelection,
            ErrorKind::SessionsNotSupported => Self::SessionsNotSupported,
            ErrorKind::InvalidTlsConfig { .. } => Self::InvalidTlsConfig,
            ErrorKind::Write(_) => Self::Write,
            ErrorKind::Transaction { .. } => Self::Transaction,
            ErrorKind::IncompatibleServer { .. } => Self::IncompatibleServer,
            ErrorKind::MissingResumeToken => Self::MissingResumeToken,
            ErrorKind::Custom(_) => Self::Custom,
            ErrorKind::Shutdown => Self::Shutdown,
            // Covers feature-gated variants (Encryption, ProxyConnect) and any
            // future additions to the #[non_exhaustive] ErrorKind enum.
            _ => Self::Unknown,
        }
    }
}

/// An error returned by an async operation.
///
/// Opaque to permit future extension without ABI breaks.
pub struct Error {
    code: ErrorCode,
    /// True if the error originates from a server response (not client-side).
    is_server_error: bool,
    /// MongoDB server error code (from CommandError / WriteError).  0 if none.
    server_code: i32,
    /// Human-readable error message as a null-terminated C string.
    message: CString,
}

impl Error {
    pub fn client_error(message: &str) -> Self {
        Self {
            code: ErrorCode::InvalidArgument,
            is_server_error: false,
            server_code: 0,
            message: CString::new(message).unwrap_or_default(),
        }
    }

    pub fn from_mongodb(e: &mongodb::error::Error) -> Self {
        let is_server = matches!(
            e.kind.as_ref(),
            ErrorKind::Authentication { .. }
                | ErrorKind::InsertMany(_)
                | ErrorKind::Command(_)
                | ErrorKind::Write(_)
        );

        let server_code = match e.kind.as_ref() {
            ErrorKind::Command(ce) => ce.code,
            ErrorKind::Write(WriteFailure::WriteError(we)) => we.code,
            _ => 0,
        };

        let message = CString::new(e.to_string()).unwrap_or_default();

        Self {
            code: ErrorCode::from_kind(e.kind.as_ref()),
            is_server_error: is_server,
            server_code,
            message,
        }
    }
}

/// Destroy the error.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_error_destroy(error: *mut Error) {
    if !error.is_null() {
        unsafe { drop(Box::from_raw(error)) }
    }
}

/// Return the error code.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_error_get_code(error: *const Error) -> ErrorCode {
    unsafe { &*error }.code
}

/// Return true if the error originates from a server response.
/// False means it is a client-side error (parameter validation, network, etc.).
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_error_is_server_error(error: *const Error) -> bool {
    unsafe { &*error }.is_server_error
}

/// Return the MongoDB server error code, or 0 if there is no server code.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_error_get_server_code(error: *const Error) -> i32 {
    unsafe { &*error }.server_code
}

/// Return the error message as a null-terminated C string.
/// The pointer is valid for the lifetime of the Error object.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_error_get_message(error: *const Error) -> *const c_char {
    unsafe { &*error }.message.as_ptr()
}
