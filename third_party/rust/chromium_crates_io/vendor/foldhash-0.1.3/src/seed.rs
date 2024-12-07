use core::hash::BuildHasher;

// These constants may end up unused depending on platform support.
#[allow(unused)]
use crate::{ARBITRARY1, ARBITRARY9};

use super::{
    folded_multiply, ARBITRARY2, ARBITRARY3, ARBITRARY4, ARBITRARY5, ARBITRARY6, ARBITRARY7,
    ARBITRARY8,
};

/// Used for FixedState, and RandomState if atomics for dynamic init are unavailable.
const FIXED_GLOBAL_SEED: [u64; 4] = [ARBITRARY4, ARBITRARY5, ARBITRARY6, ARBITRARY7];

pub mod fast {
    use super::*;
    use crate::fast::FoldHasher;

    /// A [`BuildHasher`] for [`fast::FoldHasher`]s that are randomly initialized.
    #[derive(Copy, Clone, Debug)]
    pub struct RandomState {
        per_hasher_seed: u64,
        global_seed: global::GlobalSeed,
    }

    impl Default for RandomState {
        fn default() -> Self {
            let per_hasher_seed;

            // If we have the standard library available we use a thread-local
            // counter for the per-hasher seed.
            #[cfg(feature = "std")]
            {
                use std::cell::Cell;
                thread_local! {
                    static PER_HASHER_NONDETERMINISM: Cell<u64> = const { Cell::new(0) };
                }

                let mut nondeterminism = PER_HASHER_NONDETERMINISM.get();
                nondeterminism = nondeterminism.wrapping_add(ARBITRARY1 | 1); // Ensure number is odd for maximum period.
                PER_HASHER_NONDETERMINISM.set(nondeterminism);
                per_hasher_seed = folded_multiply(nondeterminism, ARBITRARY2);
            };

            // If we don't have the standard library we use our current stack
            // address in combination with a global PER_HASHER_NONDETERMINISM to
            // create a new value that is very likely to have never been used as
            // a random state before.
            //
            // PER_HASHER_NONDETERMINISM is loaded and updated in a racy manner,
            // but this doesn't matter in practice - it is impossible that two
            // different threads have the same stack location, so they'll almost
            // surely generate different seeds, and provide a different possible
            // update for PER_HASHER_NONDETERMINISM. If we would use a proper
            // fetch_add atomic update then there is a larger chance of
            // problematic contention.
            //
            // We use usize instead of 64-bit atomics for best platform support.
            #[cfg(not(feature = "std"))]
            {
                use core::sync::atomic::{AtomicUsize, Ordering};
                static PER_HASHER_NONDETERMINISM: AtomicUsize = AtomicUsize::new(0);

                let nondeterminism = PER_HASHER_NONDETERMINISM.load(Ordering::Relaxed) as u64;
                let stack_ptr = &nondeterminism as *const _ as u64;
                per_hasher_seed = folded_multiply(nondeterminism ^ stack_ptr, ARBITRARY2);
                PER_HASHER_NONDETERMINISM.store(per_hasher_seed as usize, Ordering::Relaxed);
            }

            Self {
                per_hasher_seed,
                global_seed: global::GlobalSeed::new(),
            }
        }
    }

    impl BuildHasher for RandomState {
        type Hasher = FoldHasher;

        fn build_hasher(&self) -> FoldHasher {
            FoldHasher::with_seed(self.per_hasher_seed, self.global_seed.get())
        }
    }

    /// A [`BuildHasher`] for [`fast::FoldHasher`]s that all have the same fixed seed.
    ///
    /// Not recommended unless you absolutely need determinism.
    #[derive(Copy, Clone, Debug)]
    pub struct FixedState {
        per_hasher_seed: u64,
    }

    impl FixedState {
        /// Creates a [`FixedState`] with the given seed.
        pub const fn with_seed(seed: u64) -> Self {
            // XOR with ARBITRARY3 such that with_seed(0) matches default.
            Self {
                per_hasher_seed: seed ^ ARBITRARY3,
            }
        }
    }

    impl Default for FixedState {
        fn default() -> Self {
            Self {
                per_hasher_seed: ARBITRARY3,
            }
        }
    }

    impl BuildHasher for FixedState {
        type Hasher = FoldHasher;

        fn build_hasher(&self) -> FoldHasher {
            FoldHasher::with_seed(self.per_hasher_seed, &FIXED_GLOBAL_SEED)
        }
    }
}

pub mod quality {
    use super::*;
    use crate::quality::FoldHasher;

    /// A [`BuildHasher`] for [`quality::FoldHasher`]s that are randomly initialized.
    #[derive(Copy, Clone, Default, Debug)]
    pub struct RandomState {
        inner: fast::RandomState,
    }

    impl BuildHasher for RandomState {
        type Hasher = FoldHasher;

        fn build_hasher(&self) -> FoldHasher {
            FoldHasher {
                inner: self.inner.build_hasher(),
            }
        }
    }

    /// A [`BuildHasher`] for [`quality::FoldHasher`]s that all have the same fixed seed.
    ///
    /// Not recommended unless you absolutely need determinism.
    #[derive(Copy, Clone, Default, Debug)]
    pub struct FixedState {
        inner: fast::FixedState,
    }

    impl FixedState {
        /// Creates a [`FixedState`] with the given seed.
        pub const fn with_seed(seed: u64) -> Self {
            Self {
                // We do an additional folded multiply with the seed here for
                // the quality hash to ensure better independence between seed
                // and hash. If the seed is zero the folded multiply is zero,
                // preserving with_seed(0) == default().
                inner: fast::FixedState::with_seed(folded_multiply(seed, ARBITRARY8)),
            }
        }
    }

    impl BuildHasher for FixedState {
        type Hasher = FoldHasher;

        fn build_hasher(&self) -> FoldHasher {
            FoldHasher {
                inner: self.inner.build_hasher(),
            }
        }
    }
}

#[cfg(target_has_atomic = "8")]
mod global {
    use super::*;
    use core::cell::UnsafeCell;
    use core::sync::atomic::{AtomicU8, Ordering};

    fn generate_global_seed() -> [u64; 4] {
        let mix = |seed: u64, x: u64| folded_multiply(seed ^ x, ARBITRARY9);

        // Use address space layout randomization as our main randomness source.
        // This isn't great, but we don't advertise HashDoS resistance in the first
        // place. This is a whole lot better than nothing, at near zero cost with
        // no dependencies.
        let mut seed = 0;
        let stack_ptr = &seed as *const _;
        let func_ptr = generate_global_seed;
        let static_ptr = &GLOBAL_SEED_STORAGE as *const _;
        seed = mix(seed, stack_ptr as usize as u64);
        seed = mix(seed, func_ptr as usize as u64);
        seed = mix(seed, static_ptr as usize as u64);

        // If we have the standard library available, augment entropy with the
        // current time and an address from the allocator.
        #[cfg(feature = "std")]
        {
            #[cfg(not(all(target_family = "wasm", target_os = "unknown")))]
            if let Ok(duration) = std::time::UNIX_EPOCH.elapsed() {
                seed = mix(seed, duration.subsec_nanos() as u64);
                seed = mix(seed, duration.as_secs());
            }

            let box_ptr = &*Box::new(0u8) as *const _;
            seed = mix(seed, box_ptr as usize as u64);
        }

        let seed_a = mix(seed, 0);
        let seed_b = mix(mix(mix(seed_a, 0), 0), 0);
        let seed_c = mix(mix(mix(seed_b, 0), 0), 0);
        let seed_d = mix(mix(mix(seed_c, 0), 0), 0);

        // Zeroes form a weak-point for the multiply-mix, and zeroes tend to be
        // a common input. So we want our global seeds that are XOR'ed with the
        // input to always be non-zero. To also ensure there is always a good spread
        // of bits, we give up 3 bits of entropy and simply force some bits on.
        const FORCED_ONES: u64 = (1 << 63) | (1 << 31) | 1;
        [
            seed_a | FORCED_ONES,
            seed_b | FORCED_ONES,
            seed_c | FORCED_ONES,
            seed_d | FORCED_ONES,
        ]
    }

    // Now all the below code purely exists to cache the above seed as
    // efficiently as possible. Even if we weren't a no_std crate and had access to
    // OnceLock, we don't want to check whether the global is set each time we
    // hash an object, so we hand-roll a global storage where type safety allows us
    // to assume the storage is initialized after construction.
    struct GlobalSeedStorage {
        state: AtomicU8,
        seed: UnsafeCell<[u64; 4]>,
    }

    const UNINIT: u8 = 0;
    const LOCKED: u8 = 1;
    const INIT: u8 = 2;

    // SAFETY: we only mutate the UnsafeCells when state is in the thread-exclusive
    // LOCKED state, and only read the UnsafeCells when state is in the
    // once-achieved-eternally-preserved state INIT.
    unsafe impl Sync for GlobalSeedStorage {}

    static GLOBAL_SEED_STORAGE: GlobalSeedStorage = GlobalSeedStorage {
        state: AtomicU8::new(UNINIT),
        seed: UnsafeCell::new([0; 4]),
    };

    /// An object representing an initialized global seed.
    ///
    /// Does not actually store the seed inside itself, it is a zero-sized type.
    /// This prevents inflating the RandomState size and in turn HashMap's size.
    #[derive(Copy, Clone, Debug)]
    pub struct GlobalSeed {
        // So we can't accidentally type GlobalSeed { } within this crate.
        _no_accidental_unsafe_init: (),
    }

    impl GlobalSeed {
        #[inline(always)]
        pub fn new() -> Self {
            if GLOBAL_SEED_STORAGE.state.load(Ordering::Acquire) != INIT {
                Self::init_slow()
            }
            Self {
                _no_accidental_unsafe_init: (),
            }
        }

        #[cold]
        #[inline(never)]
        fn init_slow() {
            // Generate seed outside of critical section.
            let seed = generate_global_seed();

            loop {
                match GLOBAL_SEED_STORAGE.state.compare_exchange_weak(
                    UNINIT,
                    LOCKED,
                    Ordering::Relaxed,
                    Ordering::Acquire,
                ) {
                    Ok(_) => unsafe {
                        // SAFETY: we just acquired an exclusive lock.
                        *GLOBAL_SEED_STORAGE.seed.get() = seed;
                        GLOBAL_SEED_STORAGE.state.store(INIT, Ordering::Release);
                        return;
                    },

                    Err(INIT) => return,

                    // Yes, it's a spin loop. We need to support no_std (so no easy
                    // access to proper locks), this is a one-time-per-program
                    // initialization, and the critical section is only a few
                    // store instructions, so it'll be fine.
                    _ => core::hint::spin_loop(),
                }
            }
        }

        #[inline(always)]
        pub fn get(self) -> &'static [u64; 4] {
            // SAFETY: our constructor ensured we are in the INIT state and thus
            // this raw read does not race with any write.
            unsafe { &*GLOBAL_SEED_STORAGE.seed.get() }
        }
    }
}

#[cfg(not(target_has_atomic = "8"))]
mod global {
    #[derive(Copy, Clone, Debug)]
    pub struct GlobalSeed {}

    impl GlobalSeed {
        #[inline(always)]
        pub fn new() -> Self {
            Self {}
        }

        #[inline(always)]
        pub fn get(self) -> &'static [u64; 4] {
            &super::FIXED_GLOBAL_SEED
        }
    }
}
