# Fix: Async Loader Deadlock

**Date:** 2026-02-17
**Files:** `src/async_loader.c`
**Symptom:** Application freezes during HDR environment map loading

## Root Cause

A **mutex double-lock deadlock** in `async_worker_func()`.

The worker thread flow was:

```c
pthread_mutex_unlock(&request_mutex);   // (1) Unlock for heavy work

if (has_work) {
    async_handle_io(path_to_load);      // (2) Returns with mutex HELD
}

pthread_mutex_lock(&request_mutex);     // (3) DEADLOCK — already held!
has_pending_work = false;

```

`async_handle_io()` internally re-acquires the mutex before returning (in all
code paths: success, failure, cancel). Step (3) then tried to lock an already-held
mutex — **undefined behavior** on POSIX default (non-recursive) mutexes, which on

Linux manifests as a permanent deadlock.

Once the worker thread deadlocked on itself, the main thread's `async_loader_poll()`
also tried to acquire the same mutex and blocked forever, freezing the entire
application.

## Fix

Made the re-lock conditional on whether `async_handle_io()` was called:

```c
if (has_work) {
    async_handle_io(path_to_load);
    /* Returns with mutex HELD — no re-lock needed */

} else {
    /* No work dispatched, re-acquire for next iteration */

    pthread_mutex_lock(&request_mutex);
}
has_pending_work = false;

```

## Why It Appeared Intermittently

The deadlock only triggers when:

1. The worker thread finishes all internal work (IO + conversion)

2. The main thread happens to call `async_loader_poll()` after the worker

   re-acquires the mutex at step (3)

On faster GPUs/CPUs, timing differences could mask the bug. On Intel HD 4600
(Haswell) with a 4K HDR (64 MB PBO), the IO + SIMD conversion took ~55 ms,
making the race window consistently hit.

## Backward Compatibility

This fix is unconditionally correct on all platforms. The previous code was
always undefined behavior — it just happened to not deadlock on some systems
due to timing.
