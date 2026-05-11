use crate::ebr::Guard;
use crate::maybe_std::yield_now;
use std::future::Future;
use std::pin::Pin;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering::{AcqRel, Relaxed};
use std::sync::{Condvar, Mutex};
use std::task::{Context, Poll, Waker};
use std::thread;

/// `ASYNC` is a flag indicating that the referenced instance corresponds to an asynchronous
/// operation.
const ASYNC: usize = 1_usize;

/// [`WaitQueue`] implements an unfair wait queue.
///
/// The sole purpose of the data structure is to avoid busy-waiting. [`WaitQueue`] should always
/// protected by [`sdd`].
#[derive(Debug, Default)]
pub(crate) struct WaitQueue {
    /// Stores the pointer value of the actual wait queue entry and a flag indicating that the
    /// entry is asynchronous.
    wait_queue: AtomicUsize,
}

impl WaitQueue {
    /// Waits for the condition to be met or signaled.
    #[inline]
    pub(crate) fn wait_sync<T, F: FnOnce() -> Result<T, ()>>(&self, f: F) -> Result<T, ()> {
        if cfg!(miri) || cfg!(feature = "loom") {
            yield_now();
            return f();
        }

        let mut current = self.wait_queue.load(Relaxed);
        let mut entry = SyncWait::new(current);
        let mut entry_mut = Pin::new(&mut entry);

        while let Err(actual) = self.wait_queue.compare_exchange_weak(
            current,
            entry_mut.as_mut().get_mut() as *mut SyncWait as usize,
            AcqRel,
            Relaxed,
        ) {
            current = actual;
            entry_mut.next.store(current, Relaxed);
        }

        // Execute the closure.
        let result = f();
        if result.is_ok() {
            self.signal();
        }

        entry_mut.wait();
        result
    }

    /// Pushes an [`AsyncWait`] into the [`WaitQueue`].
    ///
    /// If it happens to acquire the desired resource, it returns an `Ok(T)` after waking up all
    /// the entries in the [`WaitQueue`].
    #[inline]
    pub(crate) fn push_async_entry<T, F: FnOnce() -> Result<T, ()>>(
        &self,
        async_wait: &mut AsyncWait,
        f: F,
    ) -> Result<T, ()> {
        debug_assert!(async_wait.mutex.is_none());

        let mut current = self.wait_queue.load(Relaxed);
        let wait_queue_ref: &WaitQueue = self;
        async_wait.next.store(current, Relaxed);
        async_wait.mutex.replace(Mutex::new((
            Some(unsafe { std::mem::transmute::<&WaitQueue, &WaitQueue>(wait_queue_ref) }),
            None,
        )));

        while let Err(actual) = self.wait_queue.compare_exchange_weak(
            current,
            (async_wait as *mut AsyncWait as usize) | ASYNC,
            AcqRel,
            Relaxed,
        ) {
            current = actual;
            async_wait.next.store(current, Relaxed);
        }

        // Execute the closure.
        if let Ok(result) = f() {
            self.signal();
            if async_wait.try_wait() {
                async_wait.mutex.take();
                return Ok(result);
            }
            // Another task is waking up `async_wait`: dispose of `result` which is holding the
            // desired resource.
        }

        // The caller has to await.
        Err(())
    }

    /// Signals the threads in the wait queue.
    #[inline]
    pub(crate) fn signal(&self) {
        if cfg!(miri) || cfg!(feature = "loom") {
            return;
        }

        let mut current = self.wait_queue.swap(0, AcqRel);

        // Flip the queue to prioritize oldest entries.
        let mut prev = 0;
        while (current & (!ASYNC)) != 0 {
            current = if (current & ASYNC) == 0 {
                // Synchronous.
                let entry_ptr = current as *const SyncWait;
                let next = unsafe {
                    let next = (*entry_ptr).next.load(Relaxed);
                    (*entry_ptr).next.store(prev, Relaxed);
                    next
                };
                prev = current;
                next
            } else {
                // Asynchronous.
                let entry_ptr = (current & (!ASYNC)) as *const AsyncWait;
                let next = unsafe {
                    let next = (*entry_ptr).next.load(Relaxed);
                    (*entry_ptr).next.store(prev, Relaxed);
                    next
                };
                prev = current;
                next
            };
        }

        // Wake up all the tasks.
        current = prev;
        while (current & (!ASYNC)) != 0 {
            current = if (current & ASYNC) == 0 {
                // Synchronous.
                let entry_ptr = current as *const SyncWait;
                unsafe {
                    let next = (*entry_ptr).next.load(Relaxed);
                    (*entry_ptr).signal();
                    next
                }
            } else {
                // Asynchronous.
                let entry_ptr = (current & (!ASYNC)) as *const AsyncWait;
                unsafe {
                    let next = (*entry_ptr).next.load(Relaxed);
                    (*entry_ptr).signal();
                    next
                }
            };
        }
    }
}

/// [`DeriveAsyncWait`] derives a mutable reference to [`AsyncWait`].
pub(crate) trait DeriveAsyncWait {
    /// Returns a mutable reference to [`AsyncWait`] if available.
    fn derive(&mut self) -> Option<&mut AsyncWait>;
}

impl DeriveAsyncWait for Pin<&mut AsyncWait> {
    #[inline]
    fn derive(&mut self) -> Option<&mut AsyncWait> {
        unsafe { Some(self.as_mut().get_unchecked_mut()) }
    }
}

impl DeriveAsyncWait for () {
    #[inline]
    fn derive(&mut self) -> Option<&mut AsyncWait> {
        None
    }
}

/// [`AsyncWait`] is inserted into [`WaitQueue`] for the caller to await until woken up.
///
/// [`AsyncWait`] has to be pinned outside in order to use it correctly. The type is `Unpin`,
/// therefore it can be moved, however the [`DeriveAsyncWait`] trait forces [`AsyncWait`] to be
/// pinned.
#[derive(Debug, Default)]
pub(crate) struct AsyncWait {
    next: AtomicUsize,
    mutex: Option<Mutex<(Option<&'static WaitQueue>, Option<Waker>)>>,
}

impl AsyncWait {
    /// Sends a signal.
    fn signal(&self) {
        if let Some(mutex) = self.mutex.as_ref() {
            if let Ok(mut locked) = mutex.lock() {
                // Disassociate itself from the `WaitQueue`.
                locked.0.take();
                if let Some(waker) = locked.1.take() {
                    waker.wake();
                }
            }
        } else {
            unreachable!();
        }
    }

    /// Tries to receive a signal.
    fn try_wait(&self) -> bool {
        if let Some(mutex) = self.mutex.as_ref() {
            if let Ok(locked) = mutex.lock() {
                if locked.0.is_none() {
                    // The wait queue entry is not associated with any `WaitQueue`.
                    return true;
                }
            }
        }
        false
    }

    /// Pulls `self` out of the [`WaitQueue`].
    ///
    /// This method is only invoked when `self` is being dropped.
    fn pull(&self) {
        // The `WaitQueue` instance must be pinned in memory.
        let _guard = Guard::new();
        let wait_queue = if let Some(mutex) = self.mutex.as_ref() {
            if let Ok(locked) = mutex.lock() {
                locked.0
            } else {
                None
            }
        } else {
            None
        };

        if let Some(wait_queue) = wait_queue {
            wait_queue.signal();

            // Data race with another thread.
            //  - Another thread pulls `self` from the `WaitQueue` to send a signal.
            //  - This thread completes `wait_queue.signal()` which does not contain `self`
            //  - This thread drops `self`.
            //  - The other thread reads `self`.
            while !self.try_wait() {
                thread::yield_now();
            }
        }
    }
}

impl Drop for AsyncWait {
    #[inline]
    fn drop(&mut self) {
        if self.mutex.is_some() {
            self.pull();
        }
    }
}

impl Future for AsyncWait {
    type Output = ();

    #[inline]
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if let Some(mutex) = self.mutex.as_ref() {
            if let Ok(mut locked) = mutex.lock() {
                if locked.0.is_none() {
                    // The wait queue entry is not associated with any `WaitQueue`.
                    return Poll::Ready(());
                }
                locked.1.replace(cx.waker().clone());
            }
            Poll::Pending
        } else {
            Poll::Ready(())
        }
    }
}

/// [`SyncWait`] is inserted into [`WaitQueue`] for the caller to synchronously wait until
/// signaled.
#[derive(Debug)]
struct SyncWait {
    next: AtomicUsize,
    condvar: Condvar,
    mutex: Mutex<bool>,
}

impl SyncWait {
    /// Creates a new [`SyncWait`].
    const fn new(next: usize) -> Self {
        #[allow(clippy::mutex_atomic)]
        Self {
            next: AtomicUsize::new(next),
            condvar: Condvar::new(),
            mutex: Mutex::new(false),
        }
    }

    /// Waits for a signal.
    fn wait(&self) {
        #[allow(clippy::mutex_atomic)]
        let mut completed = unsafe { self.mutex.lock().unwrap_unchecked() };
        while !*completed {
            completed = unsafe { self.condvar.wait(completed).unwrap_unchecked() };
        }
    }

    /// Sends a signal.
    fn signal(&self) {
        #[allow(clippy::mutex_atomic)]
        let mut completed = unsafe { self.mutex.lock().unwrap_unchecked() };
        *completed = true;
        self.condvar.notify_one();
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod test {
    use super::*;
    use std::sync::atomic::Ordering::Release;
    use std::sync::Arc;
    use std::sync::Barrier;
    use std::thread::yield_now;

    #[cfg_attr(miri, ignore)]
    #[test]
    fn wait_queue_sync() {
        let num_tasks = 8;
        let barrier = Arc::new(Barrier::new(num_tasks + 1));
        let wait_queue = Arc::new(WaitQueue::default());
        let data = Arc::new(AtomicUsize::new(0));
        let mut task_handles = Vec::with_capacity(num_tasks);
        for task_id in 1..=num_tasks {
            let barrier_clone = barrier.clone();
            let wait_queue_clone = wait_queue.clone();
            let data_clone = data.clone();
            task_handles.push(std::thread::spawn(move || {
                barrier_clone.wait();
                while wait_queue_clone
                    .wait_sync(|| {
                        if data_clone
                            .compare_exchange(task_id, task_id + 1, Relaxed, Relaxed)
                            .is_ok()
                        {
                            Ok(())
                        } else {
                            Err(())
                        }
                    })
                    .is_err()
                {
                    yield_now();
                }
                wait_queue_clone.signal();
            }));
        }

        barrier.wait();
        data.fetch_add(1, Release);
        wait_queue.signal();

        task_handles
            .into_iter()
            .for_each(|t| assert!(t.join().is_ok()));
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn wait_queue_async() {
        let num_tasks = 8;
        let barrier = Arc::new(tokio::sync::Barrier::new(num_tasks + 1));
        let wait_queue = Arc::new(WaitQueue::default());
        let data = Arc::new(AtomicUsize::new(0));
        let mut task_handles = Vec::with_capacity(num_tasks);
        for task_id in 1..=num_tasks {
            let barrier_clone = barrier.clone();
            let wait_queue_clone = wait_queue.clone();
            let data_clone = data.clone();
            task_handles.push(tokio::spawn(async move {
                barrier_clone.wait().await;
                let mut async_wait = AsyncWait::default();
                let mut async_wait_pinned = Pin::new(&mut async_wait);
                while wait_queue_clone
                    .push_async_entry(&mut async_wait_pinned, || {
                        if data_clone
                            .compare_exchange(task_id, task_id + 1, Relaxed, Relaxed)
                            .is_ok()
                        {
                            Ok(())
                        } else {
                            Err(())
                        }
                    })
                    .is_err()
                {
                    async_wait_pinned.as_mut().await;
                    if data_clone.load(Relaxed) > task_id {
                        // The operation was successful, but was signaled by another thread.
                        break;
                    }
                    async_wait_pinned.mutex.take();
                }
                wait_queue_clone.signal();
            }));
        }

        barrier.wait().await;
        data.fetch_add(1, Release);
        wait_queue.signal();

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn wait_queue_async_drop() {
        let num_tasks = 8;
        let barrier = Arc::new(tokio::sync::Barrier::new(num_tasks));
        let wait_queue = Arc::new(WaitQueue::default());
        let mut task_handles = Vec::with_capacity(num_tasks);
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let wait_queue_clone = wait_queue.clone();
            task_handles.push(tokio::spawn(async move {
                barrier_clone.wait().await;
                for _ in 0..num_tasks {
                    let mut async_wait = AsyncWait::default();
                    let mut async_wait_pinned = Pin::new(&mut async_wait);
                    if wait_queue_clone
                        .push_async_entry(&mut async_wait_pinned, || {
                            if task_id % 2 == 0 {
                                Ok(())
                            } else {
                                Err(())
                            }
                        })
                        .is_ok()
                    {
                        assert_eq!(task_id % 2, 0);
                    }
                }
                wait_queue_clone.signal();
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }
        drop(wait_queue);
    }
}
