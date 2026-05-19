#[cfg(feature = "logging")]
use log::debug;
use parking_lot::{Condvar, Mutex, ReentrantMutex, ReentrantMutexGuard};
use std::{sync::Arc, time::Duration};

struct LockState {
    parallels: u32,
}

struct LockData {
    mutex: Mutex<LockState>,
    serial: ReentrantMutex<()>,
    condvar: Condvar,
}

#[derive(Clone)]
pub(crate) struct Locks {
    arc: Arc<LockData>,
    // Name we're locking for (mostly test usage)
    #[cfg(feature = "logging")]
    pub(crate) name: String,
}

pub(crate) struct MutexGuardWrapper<'a> {
    #[allow(dead_code)] // need it around to get dropped
    mutex_guard: ReentrantMutexGuard<'a, ()>,
    locks: Locks,
}

impl Drop for MutexGuardWrapper<'_> {
    fn drop(&mut self) {
        #[cfg(feature = "logging")]
        debug!("End serial");
        self.locks.arc.condvar.notify_one();
    }
}

impl Locks {
    #[allow(unused_variables)]
    pub fn new(name: &str) -> Locks {
        Locks {
            arc: Arc::new(LockData {
                mutex: Mutex::new(LockState { parallels: 0 }),
                condvar: Condvar::new(),
                serial: Default::default(),
            }),
            #[cfg(feature = "logging")]
            name: name.to_owned(),
        }
    }

    #[cfg(test)]
    pub fn is_locked(&self) -> bool {
        self.arc.serial.is_locked()
    }

    pub fn is_locked_by_current_thread(&self) -> bool {
        self.arc.serial.is_owned_by_current_thread()
    }

    pub fn serial(&self) -> MutexGuardWrapper<'_> {
        #[cfg(feature = "logging")]
        debug!("Get serial lock '{}'", self.name);
        let mut lock_state = self.arc.mutex.lock();
        loop {
            #[cfg(feature = "logging")]
            debug!("Serial acquire {} {}", lock_state.parallels, self.name);
            // If all the things we want are true, try to lock out serial
            if lock_state.parallels == 0 {
                let possible_serial_lock = self.arc.serial.try_lock();
                if let Some(serial_lock) = possible_serial_lock {
                    #[cfg(feature = "logging")]
                    debug!("Got serial '{}'", self.name);
                    return MutexGuardWrapper {
                        mutex_guard: serial_lock,
                        locks: self.clone(),
                    };
                } else {
                    #[cfg(feature = "logging")]
                    debug!("Someone else has serial '{}'", self.name);
                }
            }

            self.arc
                .condvar
                .wait_for(&mut lock_state, Duration::from_secs(1));
        }
    }

    pub fn start_parallel(&self) {
        #[cfg(feature = "logging")]
        debug!("Get parallel lock '{}'", self.name);
        let mut lock_state = self.arc.mutex.lock();
        loop {
            #[cfg(feature = "logging")]
            debug!(
                "Parallel, existing {} '{}'",
                lock_state.parallels, self.name
            );
            if lock_state.parallels > 0 {
                // fast path, as someone else already has it locked
                lock_state.parallels += 1;
                return;
            }

            let possible_serial_lock = self.arc.serial.try_lock();
            if possible_serial_lock.is_some() {
                #[cfg(feature = "logging")]
                debug!("Parallel first '{}'", self.name);
                // We now know no-one else has the serial lock, so we can add to parallel
                lock_state.parallels = 1; // Had to have been 0 before, as otherwise we'd have hit the fast path
                return;
            }

            #[cfg(feature = "logging")]
            debug!("Parallel waiting '{}'", self.name);
            self.arc
                .condvar
                .wait_for(&mut lock_state, Duration::from_secs(1));
        }
    }

    pub fn end_parallel(&self) {
        #[cfg(feature = "logging")]
        debug!("End parallel '{}", self.name);
        let mut lock_state = self.arc.mutex.lock();
        assert!(lock_state.parallels > 0);
        lock_state.parallels -= 1;
        drop(lock_state);
        self.arc.condvar.notify_one();
    }

    #[cfg(test)]
    pub fn parallel_count(&self) -> u32 {
        let lock_state = self.arc.mutex.lock();
        lock_state.parallels
    }
}
