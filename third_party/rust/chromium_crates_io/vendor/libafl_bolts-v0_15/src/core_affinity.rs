//! This crate manages CPU affinities.
//!
//! ## Example
//!
//! This example shows how create a thread for each available processor and pin each thread to its corresponding processor.
//!
//! ```rust
//! # use std::thread;
//! use libafl_bolts::core_affinity;
//!
//! // Retrieve the IDs of all active CPU cores.
//! # #[cfg(not(miri))]
//! let core_ids = core_affinity::get_core_ids().unwrap();
//!
//! // Create a thread for each active CPU core.
//! # #[cfg(not(miri))]
//! let handles = core_ids
//!     .into_iter()
//!     .map(|id| {
//!         thread::spawn(move || {
//!             // Pin this thread to a single CPU core.
//!             id.set_affinity();
//!             // Do more work after this.
//!         })
//!     })
//!     .collect::<Vec<_>>();
//!
//! # #[cfg(not(miri))]
//! for handle in handles.into_iter() {
//!     handle.join().unwrap();
//! }
//! ```
//!
//! *This file is a fork of <https://github.com/Elzair/core_affinity_rs>*

use alloc::{
    string::{String, ToString},
    vec::Vec,
};

use serde::{Deserialize, Serialize};

use crate::Error;

/// This function tries to retrieve information
/// on all the "cores" active on this system.
pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
    get_core_ids_helper()
}

/// This represents a CPU core.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Serialize, Deserialize, Hash)]
#[repr(transparent)]
pub struct CoreId(
    /// The numerical `id` of a core
    pub usize,
);

impl CoreId {
    /// Set the affinity of the current process to this [`CoreId`]
    ///
    /// Note: This will *_not_* fail if the target platform does not support core affinity.
    /// (only on error cases for supported platforms)
    /// If you really need to fail for unsupported platforms (like `aarch64` on `macOS`), use [`CoreId::set_affinity_forced`] instead.
    pub fn set_affinity(&self) -> Result<(), Error> {
        match set_for_current_helper(*self) {
            Ok(()) | Err(Error::Unsupported(_, _)) => Ok(()),
            Err(e) => Err(e),
        }
    }

    /// Set the affinity of the current process to this [`CoreId`]
    pub fn set_affinity_forced(&self) -> Result<(), Error> {
        set_for_current_helper(*self)
    }
}

impl From<usize> for CoreId {
    fn from(id: usize) -> Self {
        CoreId(id)
    }
}

impl From<CoreId> for usize {
    fn from(core_id: CoreId) -> usize {
        core_id.0
    }
}

/// A list of [`CoreId`] to use for fuzzing
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize, Hash)]
pub struct Cores {
    /// The original commandline used during parsing
    pub cmdline: String,

    /// Vec of core ids
    pub ids: Vec<CoreId>,
}

#[cfg(feature = "std")]
impl Cores {
    /// Pick all cores
    pub fn all() -> Result<Self, Error> {
        Self::from_cmdline("all")
    }

    /// Trims the number of cores to the given value, dropping additional cores
    pub fn trim(&mut self, count: usize) -> Result<(), Error> {
        if count > self.ids.len() {
            return Err(Error::illegal_argument(format!(
                "Core trim value {count} is larger than number of chosen cores of {}",
                self.ids.len()
            )));
        }

        self.ids.resize(count, CoreId(0));
        Ok(())
    }

    /// Parses core binding args from user input.
    /// Returns a Vec of CPU IDs.
    /// * `./fuzzer --cores 1,2-4,6`: clients run in cores `1,2,3,4,6`
    /// * `./fuzzer --cores all`: one client runs on each available core
    pub fn from_cmdline(args: &str) -> Result<Self, Error> {
        let mut cores: Vec<CoreId> = vec![];

        // ./fuzzer --cores all -> one client runs in each available core
        if args == "all" {
            // TODO: is this really correct? Core ID != core number?
            let num_cores = get_core_ids()?.len();
            for x in 0..num_cores {
                cores.push(x.into());
            }
        } else {
            let core_args: Vec<&str> = args.split(',').collect();

            // ./fuzzer --cores 1,2-4,6 -> clients run in cores 1,2,3,4,6
            for csv in core_args {
                let core_range: Vec<&str> = csv.split('-').collect();
                if core_range.len() == 1 {
                    cores.push(core_range[0].parse::<usize>()?.into());
                } else if core_range.len() == 2 {
                    for x in core_range[0].parse::<usize>()?..=(core_range[1].parse::<usize>()?) {
                        cores.push(x.into());
                    }
                }
            }
        }

        if cores.is_empty() {
            return Err(Error::illegal_argument(format!(
                "No cores specified! parsed: {args}"
            )));
        }

        Ok(Self {
            cmdline: args.to_string(),
            ids: cores,
        })
    }

    /// Checks if this [`Cores`] instance contains a given ``core_id``
    #[must_use]
    pub fn contains(&self, core_id: CoreId) -> bool {
        self.ids.contains(&core_id)
    }

    /// Returns the index/position of the given [`CoreId`] in this cores.ids list.
    /// Will return `None`, if [`CoreId`] wasn't found.
    #[must_use]
    pub fn position(&self, core_id: CoreId) -> Option<usize> {
        // Since cores a low number, iterating is const-size,
        // and should be faster than hashmap lookups.
        // Prove me wrong.
        self.ids
            .iter()
            .position(|&cur_core_id| cur_core_id == core_id)
    }
}

impl From<&[usize]> for Cores {
    fn from(cores: &[usize]) -> Self {
        let cmdline = cores
            .iter()
            .map(ToString::to_string)
            .collect::<Vec<String>>()
            .join(",");
        let ids = cores.iter().map(|x| (*x).into()).collect();
        Self { cmdline, ids }
    }
}

impl From<Vec<usize>> for Cores {
    fn from(cores: Vec<usize>) -> Self {
        Self::from(cores.as_slice())
    }
}

#[cfg(feature = "std")]
impl TryFrom<&str> for Cores {
    type Error = Error;
    fn try_from(cores: &str) -> Result<Self, Self::Error> {
        Self::from_cmdline(cores)
    }
}

// Linux Section

#[cfg(any(
    target_os = "android",
    target_os = "linux",
    target_os = "dragonfly",
    target_os = "freebsd"
))]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    linux::get_core_ids()
}

#[cfg(any(
    target_os = "android",
    target_os = "linux",
    target_os = "dragonfly",
    target_os = "freebsd"
))]
#[inline]
fn set_for_current_helper(core_id: CoreId) -> Result<(), Error> {
    linux::set_for_current(core_id)
}

#[cfg(any(
    target_os = "android",
    target_os = "linux",
    target_os = "dragonfly",
    target_os = "freebsd"
))]
mod linux {
    use alloc::{string::ToString, vec::Vec};
    use core::mem::{size_of, zeroed};

    #[cfg(not(target_os = "freebsd"))]
    use libc::cpu_set_t;
    #[cfg(target_os = "freebsd")]
    use libc::cpuset_t as cpu_set_t;
    #[cfg(not(target_os = "dragonfly"))]
    use libc::{CPU_ISSET, CPU_SET, CPU_SETSIZE, sched_getaffinity, sched_setaffinity};
    #[cfg(target_os = "dragonfly")]
    use libc::{CPU_ISSET, CPU_SET, sched_getaffinity, sched_setaffinity};
    #[cfg(target_os = "dragonfly")]
    const CPU_SETSIZE: libc::c_int = 256;

    use super::CoreId;
    use crate::Error;

    #[allow(trivial_numeric_casts)]
    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        let full_set = get_affinity_mask()?;
        let mut core_ids: Vec<CoreId> = Vec::new();

        for i in 0..CPU_SETSIZE as usize {
            if unsafe { CPU_ISSET(i, &full_set) } {
                core_ids.push(CoreId(i));
            }
        }

        Ok(core_ids)
    }

    pub fn set_for_current(core_id: CoreId) -> Result<(), Error> {
        // Turn `core_id` into a `libc::cpu_set_t` with only
        // one core active.
        let mut set = new_cpu_set();

        unsafe { CPU_SET(core_id.0, &mut set) };

        // Set the current thread's core affinity.
        let result = unsafe {
            sched_setaffinity(
                0, // Defaults to current thread
                size_of::<cpu_set_t>(),
                &raw const set,
            )
        };

        if result < 0 {
            Err(Error::unknown("Failed to set_for_current"))
        } else {
            Ok(())
        }
    }

    fn get_affinity_mask() -> Result<cpu_set_t, Error> {
        let mut set = new_cpu_set();

        // Try to get current core affinity mask.
        let result = unsafe {
            sched_getaffinity(
                0, // Defaults to current thread
                size_of::<cpu_set_t>(),
                &raw mut set,
            )
        };

        if result == 0 {
            Ok(set)
        } else {
            Err(Error::unknown(
                "Failed to retrieve affinity using sched_getaffinity".to_string(),
            ))
        }
    }

    fn new_cpu_set() -> cpu_set_t {
        // # Safety
        // Returning a new zeroed value that is allowed to be 0.
        unsafe { zeroed::<cpu_set_t>() }
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        #[test]
        #[cfg_attr(miri, ignore)]
        fn test_linux_get_affinity_mask() {
            get_affinity_mask().unwrap();
        }

        #[test]
        #[cfg_attr(miri, ignore)]
        fn test_linux_set_for_current() {
            let ids = get_core_ids().unwrap();

            assert!(!ids.is_empty());

            ids[0].set_affinity().unwrap();

            // Ensure that the system pinned the current thread
            // to the specified core.
            let mut core_mask = new_cpu_set();
            unsafe { CPU_SET(ids[0].0, &mut core_mask) };

            let new_mask = get_affinity_mask().unwrap();

            let mut is_equal = true;

            for i in 0..CPU_SETSIZE as usize {
                let is_set1 = unsafe { CPU_ISSET(i, &core_mask) };
                let is_set2 = unsafe { CPU_ISSET(i, &new_mask) };

                if is_set1 != is_set2 {
                    is_equal = false;
                }
            }

            assert!(is_equal);
        }
    }
}

// Haiku
// FIXME: no sense of cpu granularity (yet ?)

#[cfg(target_os = "haiku")]
#[expect(clippy::unnecessary_wraps)]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    haiku::get_core_ids()
}

#[cfg(target_os = "haiku")]
#[expect(clippy::unnecessary_wraps)]
#[inline]
fn set_for_current_helper(_core_id: CoreId) -> Result<(), Error> {
    Ok(())
}

#[cfg(target_os = "haiku")]
mod haiku {
    use alloc::vec::Vec;
    use std::thread::available_parallelism;

    use crate::core_affinity::{CoreId, Error};

    #[expect(clippy::unnecessary_wraps)]
    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        Ok((0..(usize::from(available_parallelism()?)))
            .map(CoreId)
            .collect::<Vec<_>>())
    }
}

// Windows Section

#[cfg(target_os = "windows")]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    windows::get_core_ids()
}

#[cfg(target_os = "windows")]
#[inline]
fn set_for_current_helper(core_id: CoreId) -> Result<(), Error> {
    windows::set_for_current(core_id)
}

#[cfg(target_os = "windows")]
mod windows {
    use alloc::vec::Vec;

    use windows::Win32::System::{
        SystemInformation::GROUP_AFFINITY,
        Threading::{GetCurrentThread, SetThreadGroupAffinity},
    };

    use crate::core_affinity::{CoreId, Error};

    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        let mut core_ids: Vec<CoreId> = Vec::new();
        match get_num_logical_cpus_ex_windows() {
            Some(total_cores) => {
                for i in 0..total_cores {
                    core_ids.push(CoreId(i));
                }
                Ok(core_ids)
            }
            None => Err(Error::unknown("Unable to get logical CPUs count!")),
        }
    }

    pub fn set_for_current(id: CoreId) -> Result<(), Error> {
        let id: usize = id.into();
        let mut cpu_group = 0;
        let mut cpu_id = id;
        let total_cores = get_num_logical_cpus_ex_windows().unwrap();
        if id >= 64 {
            cpu_group = total_cores / 64;
            cpu_id = id - (cpu_group * 64);
        }
        // log::info!("Setting affinity to group {} and id {}", cpu_group, cpu_id);
        // Convert id to mask
        let mask: usize = 1 << cpu_id;

        // Set core affinity for current thread
        // We need to use this new api when we have > 64 cores
        unsafe {
            let ga = GROUP_AFFINITY {
                Mask: mask,
                Group: cpu_group as u16,
                Reserved: [0; 3],
            };

            let mut outga = GROUP_AFFINITY::default();

            let result =
                SetThreadGroupAffinity(GetCurrentThread(), &raw const ga, Some(&raw mut outga));
            if result.0 == 0 {
                Err(Error::unknown("Failed to set_for_current"))
            } else {
                Ok(())
            }
        }
    }

    #[expect(clippy::cast_ptr_alignment)]
    pub fn get_num_logical_cpus_ex_windows() -> Option<usize> {
        use core::{ptr, slice};

        #[expect(non_upper_case_globals)]
        const RelationProcessorCore: u32 = 0;

        #[repr(C)]
        #[allow(non_camel_case_types)] // expect breaks for some reason
        struct GROUP_AFFINITY {
            mask: usize,
            group: u16,
            reserved: [u16; 3],
        }

        #[repr(C)]
        #[allow(non_camel_case_types)] // expect breaks for some reason
        struct PROCESSOR_RELATIONSHIP {
            flags: u8,
            efficiency_class: u8,
            reserved: [u8; 20],
            group_count: u16,
            group_mask_tenative: [GROUP_AFFINITY; 1],
        }

        #[repr(C)]
        #[allow(non_camel_case_types)] // expect breaks for some reason
        struct SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX {
            relationship: u32,
            size: u32,
            processor: PROCESSOR_RELATIONSHIP,
        }

        unsafe extern "system" {
            fn GetLogicalProcessorInformationEx(
                relationship: u32,
                data: *mut u8,
                length: &mut u32,
            ) -> bool;
        }

        // First we need to determine how much space to reserve.

        // The required size of the buffer, in bytes.
        let mut needed_size = 0;

        unsafe {
            GetLogicalProcessorInformationEx(
                RelationProcessorCore,
                ptr::null_mut(),
                &mut needed_size,
            );
        }

        // Could be 0, or some other bogus size.
        if needed_size == 0 {
            return None;
        }

        // Allocate memory where we will store the processor info.
        let mut buffer: Vec<u8> = vec![0_u8; needed_size as usize];

        unsafe {
            let result: bool = GetLogicalProcessorInformationEx(
                RelationProcessorCore,
                buffer.as_mut_ptr(),
                &mut needed_size,
            );

            if !result {
                return None;
            }
        }

        let mut n_logical_procs: usize = 0;

        let mut byte_offset: usize = 0;
        while byte_offset < needed_size as usize {
            unsafe {
                // interpret this byte-array as SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX struct
                let part_ptr_raw: *const u8 = buffer.as_ptr().add(byte_offset);
                let part_ptr: *const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX =
                    part_ptr_raw as *const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
                let part: &SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX = &*part_ptr;

                // we are only interested in RelationProcessorCore information and hence
                // we have requested only for this kind of data (so we should not see other types of data)
                if part.relationship == RelationProcessorCore {
                    // the number of GROUP_AFFINITY structs in the array will be specified in the 'groupCount'
                    // we tenatively use the first element to get the pointer to it and reinterpret the
                    // entire slice with the groupCount
                    let groupmasks_slice: &[GROUP_AFFINITY] = slice::from_raw_parts(
                        part.processor.group_mask_tenative.as_ptr(),
                        part.processor.group_count as usize,
                    );

                    // count the local logical processors of the group and accumulate
                    let n_local_procs: usize = groupmasks_slice
                        .iter()
                        .map(|g| g.mask.count_ones() as usize)
                        .sum::<usize>();
                    n_logical_procs += n_local_procs;
                }

                // set the pointer to the next part as indicated by the size of this part
                byte_offset += part.size as usize;
            }
        }

        Some(n_logical_procs)
    }
}

// Apple Section

#[cfg(target_vendor = "apple")]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    apple::get_core_ids()
}

#[cfg(target_vendor = "apple")]
#[inline]
fn set_for_current_helper(core_id: CoreId) -> Result<(), Error> {
    apple::set_for_current(core_id)
}

#[cfg(target_vendor = "apple")]
mod apple {
    use alloc::vec::Vec;
    use std::thread::available_parallelism;

    #[cfg(target_arch = "x86_64")]
    use libc::{
        KERN_SUCCESS, THREAD_AFFINITY_POLICY, THREAD_AFFINITY_POLICY_COUNT, integer_t,
        kern_return_t, mach_msg_type_number_t, pthread_mach_thread_np, pthread_self,
        thread_policy_flavor_t, thread_policy_t, thread_t,
    };
    #[cfg(all(target_arch = "aarch64", not(miri)))]
    use libc::{pthread_set_qos_class_self_np, qos_class_t::QOS_CLASS_USER_INITIATED};

    use super::CoreId;
    use crate::Error;

    #[cfg(target_arch = "x86_64")]
    #[repr(C)]
    struct thread_affinity_policy_data_t {
        affinity_tag: integer_t,
    }

    #[cfg(target_arch = "x86_64")]
    #[link(name = "System", kind = "framework")]
    unsafe extern "C" {
        fn thread_policy_set(
            thread: thread_t,
            flavor: thread_policy_flavor_t,
            policy_info: thread_policy_t,
            count: mach_msg_type_number_t,
        ) -> kern_return_t;
    }

    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        Ok((0..(usize::from(available_parallelism()?)))
            .map(CoreId)
            .collect::<Vec<_>>())
    }

    #[cfg(target_arch = "x86_64")]
    pub fn set_for_current(core_id: CoreId) -> Result<(), Error> {
        let mut info = thread_affinity_policy_data_t {
            affinity_tag: core_id.0.try_into().unwrap(),
        };

        unsafe {
            let result = thread_policy_set(
                pthread_mach_thread_np(pthread_self()),
                THREAD_AFFINITY_POLICY as _,
                &raw mut info as thread_policy_t,
                THREAD_AFFINITY_POLICY_COUNT,
            );

            // 0 == KERN_SUCCESS
            if result == KERN_SUCCESS {
                Ok(())
            } else {
                Err(Error::unknown(format!(
                    "Failed to set_for_current {result:?}"
                )))
            }
        }
    }

    #[cfg(target_arch = "aarch64")]
    #[expect(clippy::unnecessary_wraps)]
    pub fn set_for_current(_core_id: CoreId) -> Result<(), Error> {
        // This is the best we can do, unlike on intel architecture
        // the system does not allow to pin a process/thread to specific cpu.
        // We just tell the system that we want performance.
        //
        // Furthermore, this seems to fail on background threads, so we ignore errors (result != 0).

        #[cfg(not(miri))]
        unsafe {
            let _result = pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
        }

        Ok(())
    }
}

// NetBSD Section

#[cfg(target_os = "netbsd")]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    netbsd::get_core_ids()
}

#[cfg(target_os = "netbsd")]
#[inline]
fn set_for_current_helper(core_id: CoreId) -> Result<(), Error> {
    netbsd::set_for_current(core_id)
}

#[cfg(target_os = "netbsd")]
mod netbsd {
    use alloc::vec::Vec;
    use std::thread::available_parallelism;

    use libc::{
        _cpuset, _cpuset_create, _cpuset_destroy, _cpuset_set, _cpuset_size, pthread_self,
        pthread_setaffinity_np,
    };

    use super::CoreId;
    use crate::Error;

    #[expect(trivial_numeric_casts)]
    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        Ok((0..(usize::from(available_parallelism()?)))
            .map(CoreId)
            .collect::<Vec<_>>())
    }

    pub fn set_for_current(core_id: CoreId) -> Result<(), Error> {
        let set = new_cpuset();

        unsafe { _cpuset_set(core_id.0 as u64, set) };
        // Set the current thread's core affinity.
        let result = unsafe {
            pthread_setaffinity_np(
                pthread_self(), // Defaults to current thread
                _cpuset_size(set),
                set,
            )
        };

        unsafe { _cpuset_destroy(set) };

        if result < 0 {
            Err(Error::unknown("Failed to set_for_current"))
        } else {
            Ok(())
        }
    }

    fn new_cpuset() -> *mut _cpuset {
        // # Safety
        // Simply creating new empty cpuset. No user-provided params.
        unsafe { _cpuset_create() }
    }
}

#[cfg(target_os = "openbsd")]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    openbsd::get_core_ids()
}

#[cfg(target_os = "openbsd")]
#[expect(clippy::unnecessary_wraps)]
#[inline]
fn set_for_current_helper(_: CoreId) -> Result<(), Error> {
    Ok(()) // There is no notion of cpu affinity on this platform
}

#[cfg(target_os = "openbsd")]
mod openbsd {
    use alloc::vec::Vec;
    use std::thread::available_parallelism;

    use super::CoreId;
    use crate::Error;

    #[expect(trivial_numeric_casts)]
    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        Ok((0..(usize::from(available_parallelism()?)))
            .map(CoreId)
            .collect::<Vec<_>>())
    }
}

#[cfg(any(target_os = "solaris", target_os = "illumos"))]
#[inline]
fn get_core_ids_helper() -> Result<Vec<CoreId>, Error> {
    solaris::get_core_ids()
}

#[cfg(any(target_os = "solaris", target_os = "illumos"))]
#[inline]
fn set_for_current_helper(core_id: CoreId) -> Result<(), Error> {
    solaris::set_for_current(core_id)
}

#[cfg(any(target_os = "solaris", target_os = "illumos"))]
mod solaris {
    use alloc::vec::Vec;
    use std::thread::available_parallelism;

    use super::CoreId;
    use crate::Error;

    #[expect(clippy::unnecessary_wraps)]
    pub fn get_core_ids() -> Result<Vec<CoreId>, Error> {
        Ok((0..(usize::from(available_parallelism()?)))
            .map(CoreId)
            .collect::<Vec<_>>())
    }

    pub fn set_for_current(core_id: CoreId) -> Result<(), Error> {
        let result = unsafe {
            libc::processor_bind(
                libc::P_PID,
                libc::getpid(),
                core_id.0.try_into().unwrap(),
                std::ptr::null_mut(),
            )
        };
        if result < 0 {
            Err(Error::unknown("Failed to processor_bind"))
        } else {
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use std::thread::available_parallelism;

    use super::*;

    #[test]
    #[cfg_attr(any(miri, target_os = "freebsd"), ignore)]
    fn test_get_core_ids() {
        let set = get_core_ids().unwrap();
        assert_eq!(set.len(), usize::from(available_parallelism().unwrap()));
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_set_affinity() {
        let ids = get_core_ids().unwrap();

        assert!(!ids.is_empty());

        ids[0].set_affinity().unwrap();
    }
}
