use std::sync::Arc;

/// A thread-safe handle to the Tokio runtime owned by a [`Client`].
///
/// Obtained via [`mongoc_async_client_get_runtime_handle`].  Multiple handles
/// to the same runtime may exist simultaneously, each owned by a different
/// thread.  The runtime is kept alive until all handles (and the originating
/// [`Client`]) are destroyed.
///
/// Typical usage — a dedicated progress thread while another thread polls:
///
/// ```c
/// mongoc_async_runtime_handle_t *h =
///     mongoc_async_client_get_runtime_handle(client);
///
/// // Background thread:
/// while (!done) {
///     mongoc_async_runtime_handle_make_progress(h, 5 /* ms */);
/// }
/// mongoc_async_runtime_handle_destroy(h);
/// ```
pub struct RuntimeHandle {
    pub(crate) runtime: Arc<tokio::runtime::Runtime>,
}

/// Destroy the runtime handle.
///
/// Decrements the shared reference count.  The runtime itself is destroyed
/// only after the originating [`Client`] and all handles are destroyed.
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_runtime_handle_destroy(handle: *mut RuntimeHandle) {
    if !handle.is_null() {
        unsafe { drop(Box::from_raw(handle)) }
    }
}

/// Drive IO and timer tasks on the calling thread, blocking until `timeout_ms`
/// milliseconds elapse or until IO events wake tasks — whichever comes first.
///
/// Pass `timeout_ms = 0` to process any currently-ready tasks and return
/// immediately without parking the thread.  This is useful for callers that
/// run their own event loop and want to flush pending Tokio work without
/// blocking when there is nothing ready.
///
/// ## How it works (waker-based, no spinning)
///
/// Under the hood this calls `block_on` with a future that is always
/// `Poll::Pending` (`std::future::pending()`), combined with a
/// `tokio::time::timeout`.  The Tokio scheduler responds to a stalled task by
/// calling `epoll_wait` (Linux) / `kevent` (macOS) on the IO driver fd,
/// parking the thread in the kernel.  When real IO arrives — a MongoDB reply
/// comes off the socket — the IO driver fires the appropriate wakers, queues
/// those tasks as ready, processes them, and then re-parks.  When the timer
/// deadline expires, Tokio fires the timer waker, the `timeout` wrapper
/// returns `Err(Elapsed)`, and `block_on` returns to the caller.
///
/// With `timeout_ms = 0`, the deadline has already elapsed before the first
/// poll, so Tokio flushes any ready tasks and returns without parking.
///
/// The timer is created *inside* the `async {}` block so that it is
/// instantiated lazily on the first poll, after `block_on` has entered the
/// Tokio runtime context.  Creating it outside `block_on` would panic
/// ("no reactor running").
///
/// ## Threading
///
/// The `current_thread` Tokio scheduler serialises `block_on` calls: only one
/// thread at a time may drive the reactor.  This function is intended for a
/// single dedicated progress thread; concurrent callers will queue behind the
/// first one.
///
/// ```c
/// mongoc_async_runtime_handle_t *h =
///     mongoc_async_client_get_runtime_handle(client);
///
/// // Dedicated progress thread — call from one thread at a time:
/// while (!done) {
///     mongoc_async_runtime_handle_make_progress(h, 5 /* ms */);
/// }
/// mongoc_async_runtime_handle_destroy(h);
/// ```
#[unsafe(no_mangle)]
pub extern "C" fn mongoc_async_runtime_handle_make_progress(
    handle: *mut RuntimeHandle,
    timeout_ms: u64,
) {
    let h = unsafe { &*handle };
    // The async block is lazy: tokio::time::timeout (and the Sleep it creates
    // internally) is only instantiated during the first poll, which happens
    // inside block_on after the Tokio context is active.
    h.runtime.block_on(async move {
        // pending() never completes on its own; timeout() wraps it with a
        // deadline.  The scheduler parks in epoll/kqueue until IO arrives or
        // the deadline fires.
        let _ = tokio::time::timeout(
            std::time::Duration::from_millis(timeout_ms),
            std::future::pending::<()>(),
        )
        .await;
    });
}
