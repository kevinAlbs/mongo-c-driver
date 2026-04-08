use crate::log;

/// cbindgen:ignore
const MONGOC_LOG_DOMAIN: &str = "session";

/// An active client session, optionally within a multi-document transaction.
///
/// Opaque to permit future extension without ABI breaks.
/// The caller must eventually call [`mongoc_rust_session_destroy`].
pub struct Session {
    /// Raw pointer to the owning Client's Tokio runtime.
    /// Valid for the lifetime of the Client; the C caller is responsible for
    /// not outliving the client.
    pub(crate) runtime: *const tokio::runtime::Runtime,
    pub(crate) inner: mongodb::ClientSession,
}

// Safety: Session is only ever used from the C caller's single thread.
// The raw runtime pointer is stable (heap-allocated in Client).
unsafe impl Send for Session {}

/// Destroy the session, rolling back any active transaction.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_session_destroy(session: *mut Session) {
    if !session.is_null() {
        unsafe { drop(Box::from_raw(session)) }
    }
}

/// Begin a new transaction on this session.
///
/// Returns true on success.  On failure `error` (if non-null) is set to an
/// owned error the caller must destroy.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_session_start_transaction_await(
    session: *mut Session,
    error: *mut *mut crate::error::Error,
) -> bool {
    let s = unsafe { &mut *session };
    let rt = unsafe { &*s.runtime };
    match rt.block_on(async { s.inner.start_transaction().await }) {
        Ok(_) => true,
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

/// Commit the active transaction.
///
/// Returns true on success.  On failure `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_session_commit_transaction_await(
    session: *mut Session,
    error: *mut *mut crate::error::Error,
) -> bool {
    let s = unsafe { &mut *session };
    let rt = unsafe { &*s.runtime };
    match rt.block_on(async { s.inner.commit_transaction().await }) {
        Ok(_) => true,
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

/// Abort (roll back) the active transaction.
///
/// Returns true on success.  On failure `error` (if non-null) is set.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_session_abort_transaction_await(
    session: *mut Session,
    error: *mut *mut crate::error::Error,
) -> bool {
    let s = unsafe { &mut *session };
    let rt = unsafe { &*s.runtime };
    match rt.block_on(async { s.inner.abort_transaction().await }) {
        Ok(_) => true,
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
