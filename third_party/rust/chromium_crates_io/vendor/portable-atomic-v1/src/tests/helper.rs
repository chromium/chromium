// SPDX-License-Identifier: Apache-2.0 OR MIT

#![allow(unused_macros, clippy::undocumented_unsafe_blocks)]

use std::{
    eprintln,
    sync::atomic::{AtomicIsize, AtomicUsize, Ordering},
};

use crossbeam_utils::thread;

macro_rules! __test_atomic_common {
    ($atomic_type:ty, $value_type:ty) => {
        use std::mem;
        #[test]
        fn assert_auto_traits() {
            fn _assert<T: Send + Sync + Unpin + std::panic::UnwindSafe>() {}
            _assert::<$atomic_type>();
        }
        #[test]
        fn alignment() {
            // https://github.com/rust-lang/rust/blob/1.84.0/library/core/tests/atomic.rs#L252
            assert_eq!(mem::align_of::<$atomic_type>(), mem::size_of::<$atomic_type>());
            assert_eq!(mem::size_of::<$atomic_type>(), mem::size_of::<$value_type>());
        }
        #[test]
        fn is_lock_free() {
            const IS_ALWAYS_LOCK_FREE: bool = <$atomic_type>::IS_ALWAYS_LOCK_FREE;
            assert_eq!(IS_ALWAYS_LOCK_FREE, <$atomic_type>::IS_ALWAYS_LOCK_FREE);
            let is_lock_free = <$atomic_type>::is_lock_free();
            if IS_ALWAYS_LOCK_FREE {
                // If is_always_lock_free is true, then is_lock_free must always be true.
                assert!(is_lock_free);
            }
        }
    };
}
macro_rules! __test_atomic_pub_common {
    ($atomic_type:ty, $value_type:ty) => {
        #[test]
        fn is_always_lock_free() {
            assert_eq!(<$atomic_type>::IS_ALWAYS_LOCK_FREE, <$atomic_type>::is_always_lock_free());
        }
        #[test]
        fn assert_ref_unwind_safe() {
            #[cfg(not(all(portable_atomic_no_core_unwind_safe, not(feature = "std"))))]
            static_assertions::assert_impl_all!($atomic_type: std::panic::RefUnwindSafe);
            #[cfg(all(portable_atomic_no_core_unwind_safe, not(feature = "std")))]
            static_assertions::assert_not_impl_all!($atomic_type: std::panic::RefUnwindSafe);
        }
    };
}

macro_rules! __test_atomic_int_load_store {
    ($atomic_type:ty, $int_type:ident, single_thread) => {
        __test_atomic_common!($atomic_type, $int_type);
        use crate::tests::helper::{self, *};
        #[test]
        fn accessor() {
            let a = <$atomic_type>::new(10);
            unsafe {
                assert_eq!(*a.as_ptr(), 10);
                *a.as_ptr() = 5;
                assert_eq!(a.as_ptr() as *const (), &a as *const _ as *const ());
                assert_eq!(*a.as_ptr(), 5);
            }
        }
        // https://bugs.llvm.org/show_bug.cgi?id=37061
        #[test]
        fn static_load_only() {
            static VAR: $atomic_type = <$atomic_type>::new(10);
            for &order in &helper::LOAD_ORDERINGS {
                assert_eq!(VAR.load(order), 10);
            }
        }
        #[test]
        fn load_store() {
            static VAR: $atomic_type = <$atomic_type>::new(10);
            test_load_ordering(&|order| {
                VAR.load(order);
            });
            test_store_ordering(&|order| VAR.store(10, order));
            for (&load_order, &store_order) in
                helper::LOAD_ORDERINGS.iter().zip(&helper::STORE_ORDERINGS)
            {
                assert_eq!(VAR.load(load_order), 10);
                VAR.store(5, store_order);
                assert_eq!(VAR.load(load_order), 5);
                VAR.store(10, store_order);
                let a = <$atomic_type>::new(1);
                assert_eq!(a.load(load_order), 1);
                a.store($int_type::MIN, store_order);
                assert_eq!(a.load(load_order), $int_type::MIN);
                a.store($int_type::MAX, store_order);
                assert_eq!(a.load(load_order), $int_type::MAX);
            }
        }
    };
    ($atomic_type:ty, $int_type:ident) => {
        __test_atomic_int_load_store!($atomic_type, $int_type, single_thread);

        use std::{collections::BTreeSet, eprintln, time::Instant, vec, vec::Vec};

        use crossbeam_utils::thread;

        #[test]
        fn stress_load_store() {
            let mut rng = fastrand::Rng::new();
            let (iterations, threads) = stress_test_config(&mut rng);
            let data1 = (0..iterations).map(|_| rng.$int_type(..)).collect::<Vec<_>>();
            let set = data1.iter().copied().collect::<BTreeSet<_>>();
            let a = <$atomic_type>::new(data1[rng.usize(0..iterations)]);
            let now = &Instant::now();
            thread::scope(|s| {
                for _ in 0..threads {
                    s.spawn(|_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        for i in 0..iterations {
                            a.store(data1[i], rand_store_ordering(&mut rng));
                        }
                        eprintln!("store end={:?}", now.elapsed());
                    });
                    s.spawn(|_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        let mut v = vec![0; iterations];
                        for i in 0..iterations {
                            v[i] = a.load(rand_load_ordering(&mut rng));
                        }
                        eprintln!("load end={:?}", now.elapsed());
                        for v in v {
                            assert!(set.contains(&v), "load unexpected={}", v);
                        }
                    });
                }
            })
            .unwrap();
        }
    };
}
macro_rules! __test_atomic_float_load_store {
    ($atomic_type:ty, $float_type:ident, single_thread) => {
        __test_atomic_common!($atomic_type, $float_type);
        use crate::tests::helper::{self, *};
        #[test]
        fn accessor() {
            let a = <$atomic_type>::new(10.);
            unsafe {
                assert_eq!(*a.as_ptr(), 10.);
                *a.as_ptr() = 5.;
                assert_eq!(a.as_ptr() as *const (), &a as *const _ as *const ());
                assert_eq!(*a.as_ptr(), 5.);
            }
        }
        // https://bugs.llvm.org/show_bug.cgi?id=37061
        #[test]
        fn static_load_only() {
            static VAR: $atomic_type = <$atomic_type>::new(10.);
            for &order in &helper::LOAD_ORDERINGS {
                assert_eq!(VAR.load(order), 10.);
            }
        }
        #[test]
        fn load_store() {
            static VAR: $atomic_type = <$atomic_type>::new(10.);
            test_load_ordering(&|order| {
                VAR.load(order);
            });
            test_store_ordering(&|order| VAR.store(10., order));
            for (&load_order, &store_order) in
                helper::LOAD_ORDERINGS.iter().zip(&helper::STORE_ORDERINGS)
            {
                assert_eq!(VAR.load(load_order), 10.);
                VAR.store(5., store_order);
                assert_eq!(VAR.load(load_order), 5.);
                VAR.store(10., store_order);
                let a = <$atomic_type>::new(1.);
                assert_eq!(a.load(load_order), 1.);
                a.store(2., store_order);
                assert_eq!(a.load(load_order), 2.);
            }
        }
    };
    ($atomic_type:ty, $float_type:ident) => {
        __test_atomic_float_load_store!($atomic_type, $float_type, single_thread);
        // TODO: multi thread
    };
}
macro_rules! __test_atomic_bool_load_store {
    ($atomic_type:ty, single_thread) => {
        __test_atomic_common!($atomic_type, bool);
        use crate::tests::helper::{self, *};
        #[test]
        fn accessor() {
            let a = <$atomic_type>::new(false);
            unsafe {
                assert_eq!(*a.as_ptr(), false);
                *a.as_ptr() = true;
                assert_eq!(a.as_ptr() as *const (), &a as *const _ as *const ());
                assert_eq!(*a.as_ptr(), true);
            }
        }
        // https://bugs.llvm.org/show_bug.cgi?id=37061
        #[test]
        fn static_load_only() {
            static VAR: $atomic_type = <$atomic_type>::new(false);
            for &order in &helper::LOAD_ORDERINGS {
                assert_eq!(VAR.load(order), false);
            }
        }
        #[test]
        fn load_store() {
            static VAR: $atomic_type = <$atomic_type>::new(false);
            test_load_ordering(&|order| {
                VAR.load(order);
            });
            test_store_ordering(&|order| VAR.store(false, order));
            for (&load_order, &store_order) in
                helper::LOAD_ORDERINGS.iter().zip(&helper::STORE_ORDERINGS)
            {
                assert_eq!(VAR.load(load_order), false);
                VAR.store(true, store_order);
                assert_eq!(VAR.load(load_order), true);
                VAR.store(false, store_order);
                let a = <$atomic_type>::new(true);
                assert_eq!(a.load(load_order), true);
                a.store(false, store_order);
                assert_eq!(a.load(load_order), false);
            }
        }
    };
    ($atomic_type:ty) => {
        __test_atomic_bool_load_store!($atomic_type, single_thread);
        // TODO: multi thread
    };
}
macro_rules! __test_atomic_ptr_load_store {
    ($atomic_type:ty, single_thread) => {
        __test_atomic_common!($atomic_type, *mut u8);

        use std::ptr;

        use crate::tests::helper::{self, *};

        #[test]
        fn accessor() {
            let mut v = 1;
            let a = <$atomic_type>::new(ptr::null_mut());
            unsafe {
                assert!((*a.as_ptr()).is_null());
                *a.as_ptr() = &mut v;
                assert_eq!(a.as_ptr() as *const (), &a as *const _ as *const ());
                assert!(!(*a.as_ptr()).is_null());
            }
        }
        // https://bugs.llvm.org/show_bug.cgi?id=37061
        #[test]
        fn static_load_only() {
            static VAR: $atomic_type = <$atomic_type>::new(ptr::null_mut());
            for &order in &helper::LOAD_ORDERINGS {
                assert_eq!(VAR.load(order), ptr::null_mut());
            }
        }
        #[test]
        fn load_store() {
            static VAR: $atomic_type = <$atomic_type>::new(ptr::null_mut());
            test_load_ordering(&|order| {
                VAR.load(order);
            });
            test_store_ordering(&|order| VAR.store(ptr::null_mut(), order));
            let mut v = 1_u8;
            let p = &mut v as *mut u8;
            for (&load_order, &store_order) in
                helper::LOAD_ORDERINGS.iter().zip(&helper::STORE_ORDERINGS)
            {
                assert_eq!(VAR.load(load_order), ptr::null_mut());
                VAR.store(p, store_order);
                assert_eq!(VAR.load(load_order), p);
                VAR.store(ptr::null_mut(), store_order);
                let a = <$atomic_type>::new(p);
                assert_eq!(a.load(load_order), p);
                a.store(ptr::null_mut(), store_order);
                assert_eq!(a.load(load_order), ptr::null_mut());
            }
        }
    };
    ($atomic_type:ty) => {
        __test_atomic_ptr_load_store!($atomic_type, single_thread);
        // TODO: multi thread
    };
}

macro_rules! __test_atomic_int {
    ($atomic_type:ty, $int_type:ident, single_thread) => {
        #[test]
        fn swap() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(5);
            test_swap_ordering(&|order| {
                a.swap(5, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                assert_eq!(a.swap(10, order), 5);
                assert_eq!(a.swap($int_type::MIN, order), 10);
                assert_eq!(a.swap($int_type::MAX, order), $int_type::MIN);
                assert_eq!(a.swap(5, order), $int_type::MAX);
            }
        }
        #[test]
        fn compare_exchange() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(5);
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange(5, 5, success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(5);
                assert_eq!(a.compare_exchange(5, 10, success, failure), Ok(5));
                assert_eq!(a.load(Ordering::Relaxed), 10);
                assert_eq!(a.compare_exchange(6, 12, success, failure), Err(10));
                assert_eq!(a.load(Ordering::Relaxed), 10);
            }
        }
        #[test]
        fn compare_exchange_weak() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(4);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange_weak(4, 4, success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(4);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.compare_exchange_weak(6, 8, success, failure), Err(4));
                let mut old = a.load(Ordering::Relaxed);
                loop {
                    let new = old * 2;
                    match a.compare_exchange_weak(old, new, success, failure) {
                        Ok(_) => break,
                        Err(x) => old = x,
                    }
                }
                assert_eq!(a.load(Ordering::Relaxed), 8);
            }
        }
        #[test]
        fn fetch_add() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(0);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| {
                a.fetch_add(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_add(10, order), 0);
                assert_eq!(a.load(Ordering::Relaxed), 10);
                let a = <$atomic_type>::new($int_type::MAX);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_add(1, order), $int_type::MAX);
                assert_eq!(a.load(Ordering::Relaxed), $int_type::MAX.wrapping_add(1));
            }
        }
        #[test]
        fn add() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(0);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| a.add(0, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                a.add(10, order);
                assert_eq!(a.load(Ordering::Relaxed), 10);
                let a = <$atomic_type>::new($int_type::MAX);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                a.add(1, order);
                assert_eq!(a.load(Ordering::Relaxed), $int_type::MAX.wrapping_add(1));
            }
        }
        #[test]
        fn fetch_sub() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(20);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| {
                a.fetch_sub(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(20);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_sub(10, order), 20);
                assert_eq!(a.load(Ordering::Relaxed), 10);
                let a = <$atomic_type>::new($int_type::MIN);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_sub(1, order), $int_type::MIN);
                assert_eq!(a.load(Ordering::Relaxed), $int_type::MIN.wrapping_sub(1));
            }
        }
        #[test]
        fn sub() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(20);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| a.sub(0, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(20);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                a.sub(10, order);
                assert_eq!(a.load(Ordering::Relaxed), 10);
                let a = <$atomic_type>::new($int_type::MIN);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                a.sub(1, order);
                assert_eq!(a.load(Ordering::Relaxed), $int_type::MIN.wrapping_sub(1));
            }
        }
        #[test]
        fn fetch_and() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| {
                a.fetch_and(0b101101, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                assert_eq!(a.fetch_and(0b110011, order), 0b101101);
                assert_eq!(a.load(Ordering::Relaxed), 0b100001);
            }
        }
        #[test]
        fn and() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| a.and(0b101101, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                a.and(0b110011, order);
                assert_eq!(a.load(Ordering::Relaxed), 0b100001);
            }
        }
        #[test]
        fn fetch_nand() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") {
                return;
            }
            let a = <$atomic_type>::new(0x13);
            test_swap_ordering(&|order| {
                a.fetch_nand(0x31, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0x13);
                assert_eq!(a.fetch_nand(0x31, order), 0x13);
                assert_eq!(a.load(Ordering::Relaxed), !(0x13 & 0x31));
            }
        }
        #[test]
        fn fetch_or() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| {
                a.fetch_or(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                assert_eq!(a.fetch_or(0b110011, order), 0b101101);
                assert_eq!(a.load(Ordering::Relaxed), 0b111111);
            }
        }
        #[test]
        fn or() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| a.or(0, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                a.or(0b110011, order);
                assert_eq!(a.load(Ordering::Relaxed), 0b111111);
            }
        }
        #[test]
        fn fetch_xor() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| {
                a.fetch_xor(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                assert_eq!(a.fetch_xor(0b110011, order), 0b101101);
                assert_eq!(a.load(Ordering::Relaxed), 0b011110);
            }
        }
        #[test]
        fn xor() {
            let a = <$atomic_type>::new(0b101101);
            test_swap_ordering(&|order| a.xor(0, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b101101);
                a.xor(0b110011, order);
                assert_eq!(a.load(Ordering::Relaxed), 0b011110);
            }
        }
        #[test]
        fn fetch_max() {
            // TODO(gcc): failure: https://github.com/rust-lang/rustc_codegen_gcc/issues/821#issuecomment-3793567607
            if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                && $int_type::MIN == 0
                && mem::size_of::<$int_type>() <= 8
            {
                return;
            }
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(23);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| {
                a.fetch_max(23, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(23);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_max(22, order), 23);
                assert_eq!(a.load(Ordering::Relaxed), 23);
                assert_eq!(a.fetch_max(24, order), 23);
                assert_eq!(a.load(Ordering::Relaxed), 24);
                let a = <$atomic_type>::new(0);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_max(1, order), 0);
                assert_eq!(a.load(Ordering::Relaxed), 1);
                assert_eq!(a.fetch_max(0, order), 1);
                assert_eq!(a.load(Ordering::Relaxed), 1);
                let a = <$atomic_type>::new(!0);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_max(0, order), !0);
                assert_eq!(a.load(Ordering::Relaxed), core::cmp::max(!0, 0));
            }
        }
        #[test]
        fn fetch_min() {
            // TODO(gcc): failure: https://github.com/rust-lang/rustc_codegen_gcc/issues/821#issuecomment-3793567607
            if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                && $int_type::MIN == 0
                && mem::size_of::<$int_type>() <= 8
            {
                return;
            }
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(23);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_swap_ordering(&|order| {
                a.fetch_min(23, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(23);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_min(24, order), 23);
                assert_eq!(a.load(Ordering::Relaxed), 23);
                assert_eq!(a.fetch_min(22, order), 23);
                assert_eq!(a.load(Ordering::Relaxed), 22);
                let a = <$atomic_type>::new(1);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_min(0, order), 1);
                assert_eq!(a.load(Ordering::Relaxed), 0);
                assert_eq!(a.fetch_min(1, order), 0);
                assert_eq!(a.load(Ordering::Relaxed), 0);
                let a = <$atomic_type>::new(!0);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_min(0, order), !0);
                assert_eq!(a.load(Ordering::Relaxed), core::cmp::min(!0, 0));
            }
        }
        #[test]
        fn fetch_not() {
            let a = <$atomic_type>::new(1);
            test_swap_ordering(&|order| {
                a.fetch_not(order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(1);
                assert_eq!(a.fetch_not(order), 1);
                assert_eq!(a.load(Ordering::Relaxed), !1);
            }
        }
        #[test]
        fn not() {
            let a = <$atomic_type>::new(1);
            test_swap_ordering(&|order| a.not(order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(1);
                a.not(order);
                assert_eq!(a.load(Ordering::Relaxed), !1);
            }
        }
        #[test]
        fn fetch_neg() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(5);
            test_swap_ordering(&|order| {
                a.fetch_neg(order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(5);
                assert_eq!(a.fetch_neg(order), 5);
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::wrapping_neg(5));
                assert_eq!(a.fetch_neg(order), <$int_type>::wrapping_neg(5));
                assert_eq!(a.load(Ordering::Relaxed), 5);
                let a = <$atomic_type>::new(<$int_type>::MIN);
                assert_eq!(a.fetch_neg(order), <$int_type>::MIN);
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::MIN.wrapping_neg());
                assert_eq!(a.fetch_neg(order), <$int_type>::MIN.wrapping_neg());
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::MIN);
            }
        }
        #[test]
        fn neg() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(5);
            test_swap_ordering(&|order| a.neg(order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(5);
                a.neg(order);
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::wrapping_neg(5));
                a.neg(order);
                assert_eq!(a.load(Ordering::Relaxed), 5);
                let a = <$atomic_type>::new(<$int_type>::MIN);
                a.neg(order);
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::MIN.wrapping_neg());
                a.neg(order);
                assert_eq!(a.load(Ordering::Relaxed), <$int_type>::MIN);
            }
        }
        #[test]
        fn bit_set() {
            let a = <$atomic_type>::new(0b0001);
            test_swap_ordering(&|order| assert!(a.bit_set(0, order)));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b0000);
                assert!(!a.bit_set(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0001);
                assert!(a.bit_set(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0001);
            }
        }
        #[test]
        fn bit_clear() {
            let a = <$atomic_type>::new(0b0000);
            test_swap_ordering(&|order| assert!(!a.bit_clear(0, order)));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b0001);
                assert!(a.bit_clear(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0000);
                assert!(!a.bit_clear(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0000);
            }
        }
        #[test]
        fn bit_toggle() {
            let a = <$atomic_type>::new(0b0000);
            test_swap_ordering(&|order| {
                a.bit_toggle(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0b0000);
                assert!(!a.bit_toggle(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0001);
                assert!(a.bit_toggle(0, order));
                assert_eq!(a.load(Ordering::Relaxed), 0b0000);
            }
        }
        ::quickcheck::quickcheck! {
            fn quickcheck_swap(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.swap(y, order), x);
                    assert_eq!(a.swap(x, order), y);
                }
                true
            }
            fn quickcheck_compare_exchange(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                #[cfg(all(
                    target_arch = "arm",
                    not(any(target_feature = "v6", portable_atomic_target_feature = "v6")),
                ))]
                {
                    // TODO: LLVM bug:
                    // https://github.com/llvm/llvm-project/issues/61880
                    // https://github.com/taiki-e/portable-atomic/issues/2
                    if mem::size_of::<$int_type>() <= 2 {
                        return true;
                    }
                }
                let mut rng = fastrand::Rng::new();
                let z = loop {
                    let z = rng.$int_type(..);
                    if z != y {
                        break z;
                    }
                };
                for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.compare_exchange(x, y, success, failure).unwrap(), x);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                    assert_eq!(a.compare_exchange(z, x, success, failure).unwrap_err(), y);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                }
                true
            }
            fn quickcheck_fetch_add(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_add(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_add(y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_add(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), y.wrapping_add(x));
                }
                true
            }
            fn quickcheck_add(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    a.add(y, order);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_add(y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    a.add(x, order);
                    assert_eq!(a.load(Ordering::Relaxed), y.wrapping_add(x));
                }
                true
            }
            fn quickcheck_fetch_sub(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_sub(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_sub(y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_sub(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), y.wrapping_sub(x));
                }
                true
            }
            fn quickcheck_sub(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    a.sub(y, order);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_sub(y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    a.sub(x, order);
                    assert_eq!(a.load(Ordering::Relaxed), y.wrapping_sub(x));
                }
                true
            }
            fn quickcheck_fetch_and(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_and(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x & y);
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_and(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), y & x);
                }
                true
            }
            fn quickcheck_and(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    a.and(y, order);
                    assert_eq!(a.load(Ordering::Relaxed), x & y);
                    let a = <$atomic_type>::new(y);
                    a.and(x, order);
                    assert_eq!(a.load(Ordering::Relaxed), y & x);
                }
                true
            }
            fn quickcheck_fetch_nand(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_nand(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), !(x & y));
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_nand(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), !(y & x));
                }
                true
            }
            fn quickcheck_fetch_or(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_or(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x | y);
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_or(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), y | x);
                }
                true
            }
            fn quickcheck_or(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    a.or(y, order);
                    assert_eq!(a.load(Ordering::Relaxed), x | y);
                    let a = <$atomic_type>::new(y);
                    a.or(x, order);
                    assert_eq!(a.load(Ordering::Relaxed), y | x);
                }
                true
            }
            fn quickcheck_fetch_xor(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_xor(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x ^ y);
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_xor(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), y ^ x);
                }
                true
            }
            fn quickcheck_xor(x: $int_type, y: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    a.xor(y, order);
                    assert_eq!(a.load(Ordering::Relaxed), x ^ y);
                    let a = <$atomic_type>::new(y);
                    a.xor(x, order);
                    assert_eq!(a.load(Ordering::Relaxed), y ^ x);
                }
                true
            }
            fn quickcheck_fetch_max(x: $int_type, y: $int_type) -> bool {
                // TODO(gcc): failure: https://github.com/rust-lang/rustc_codegen_gcc/issues/821#issuecomment-3793567607
                if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                    && $int_type::MIN == 0
                    && mem::size_of::<$int_type>() <= 8
                {
                    return true;
                }
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_max(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), core::cmp::max(x, y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_max(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), core::cmp::max(y, x));
                }
                true
            }
            fn quickcheck_fetch_min(x: $int_type, y: $int_type) -> bool {
                // TODO(gcc): failure: https://github.com/rust-lang/rustc_codegen_gcc/issues/821#issuecomment-3793567607
                if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                    && $int_type::MIN == 0
                    && mem::size_of::<$int_type>() <= 8
                {
                    return true;
                }
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_min(y, order), x);
                    assert_eq!(a.load(Ordering::Relaxed), core::cmp::min(x, y));
                    let a = <$atomic_type>::new(y);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.fetch_min(x, order), y);
                    assert_eq!(a.load(Ordering::Relaxed), core::cmp::min(y, x));
                }
                true
            }
            fn quickcheck_fetch_not(x: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_not(order), x);
                    assert_eq!(a.load(Ordering::Relaxed), !x);
                    assert_eq!(a.fetch_not(order), !x);
                    assert_eq!(a.load(Ordering::Relaxed), x);
                }
                true
            }
            fn quickcheck_not(x: $int_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    a.not(order);
                    assert_eq!(a.load(Ordering::Relaxed), !x);
                    a.not(order);
                    assert_eq!(a.load(Ordering::Relaxed), x);
                }
                true
            }
            fn quickcheck_fetch_neg(x: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                #[cfg(all(
                    target_arch = "arm",
                    not(any(target_feature = "v6", portable_atomic_target_feature = "v6")),
                ))]
                {
                    // TODO: LLVM bug:
                    // https://github.com/llvm/llvm-project/issues/61880
                    // https://github.com/taiki-e/portable-atomic/issues/2
                    if mem::size_of::<$int_type>() <= 2 {
                        return true;
                    }
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_neg(order), x);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_neg());
                    assert_eq!(a.fetch_neg(order), x.wrapping_neg());
                    assert_eq!(a.load(Ordering::Relaxed), x);
                }
                true
            }
            fn quickcheck_neg(x: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    a.neg(order);
                    assert_eq!(a.load(Ordering::Relaxed), x.wrapping_neg());
                    a.neg(order);
                    assert_eq!(a.load(Ordering::Relaxed), x);
                }
                true
            }
            fn quickcheck_bit_set(x: $int_type, bit: u32) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_set(bit, order);
                    let mask = <$int_type>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed), x | mask);
                    assert_eq!(b, x & mask != 0);
                }
                true
            }
            fn quickcheck_bit_clear(x: $int_type, bit: u32) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_clear(bit, order);
                    let mask = <$int_type>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed), x & !mask);
                    assert_eq!(b, x & mask != 0);
                }
                true
            }
            fn quickcheck_bit_toggle(x: $int_type, bit: u32) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_toggle(bit, order);
                    let mask = <$int_type>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed), x ^ mask);
                    assert_eq!(b, x & mask != 0);
                }
                true
            }
        }
    };
    ($atomic_type:ty, $int_type:ident) => {
        __test_atomic_int!($atomic_type, $int_type, single_thread);

        #[test]
        fn stress_swap() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let mut rng = fastrand::Rng::new();
            let (iterations, threads) = stress_test_config(&mut rng);
            let data1 = &(0..threads)
                .map(|_| (0..iterations).map(|_| rng.$int_type(..)).collect::<Vec<_>>())
                .collect::<Vec<_>>();
            let data2 = &(0..threads)
                .map(|_| (0..iterations).map(|_| rng.$int_type(..)).collect::<Vec<_>>())
                .collect::<Vec<_>>();
            let set = &data1
                .iter()
                .flat_map(|v| v.iter().copied())
                .chain(data2.iter().flat_map(|v| v.iter().copied()))
                .collect::<BTreeSet<_>>();
            let a = &<$atomic_type>::new(data2[0][rng.usize(0..iterations)]);
            let now = &Instant::now();
            thread::scope(|s| {
                for thread in 0..threads {
                    if thread % 2 == 0 {
                        s.spawn(move |_| {
                            let mut rng = fastrand::Rng::new();
                            let now = *now;
                            for i in 0..iterations {
                                a.store(data1[thread][i], rand_store_ordering(&mut rng));
                            }
                            eprintln!("store end={:?}", now.elapsed());
                        });
                    } else {
                        s.spawn(|_| {
                            let mut rng = fastrand::Rng::new();
                            let now = *now;
                            let mut v = vec![0; iterations];
                            for i in 0..iterations {
                                v[i] = a.load(rand_load_ordering(&mut rng));
                            }
                            eprintln!("load end={:?}", now.elapsed());
                            for v in v {
                                assert!(set.contains(&v), "load unexpected={}", v);
                            }
                        });
                    }
                    s.spawn(move |_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        let mut v = vec![0; iterations];
                        for i in 0..iterations {
                            v[i] = a.swap(data2[thread][i], rand_swap_ordering(&mut rng));
                        }
                        eprintln!("swap end={:?}", now.elapsed());
                        for v in v {
                            assert!(set.contains(&v), "swap unexpected={}", v);
                        }
                    });
                }
            })
            .unwrap();
        }
        #[test]
        fn stress_compare_exchange() {
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let mut rng = fastrand::Rng::new();
            let (iterations, threads) = stress_test_config(&mut rng);
            let data1 = &(0..threads)
                .map(|_| (0..iterations).map(|_| rng.$int_type(..)).collect::<Vec<_>>())
                .collect::<Vec<_>>();
            let data2 = &(0..threads)
                .map(|_| (0..iterations).map(|_| rng.$int_type(..)).collect::<Vec<_>>())
                .collect::<Vec<_>>();
            let set = &data1
                .iter()
                .flat_map(|v| v.iter().copied())
                .chain(data2.iter().flat_map(|v| v.iter().copied()))
                .collect::<BTreeSet<_>>();
            let a = &<$atomic_type>::new(data2[0][rng.usize(0..iterations)]);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(a);
            }
            let now = &Instant::now();
            thread::scope(|s| {
                for thread in 0..threads {
                    s.spawn(move |_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        for i in 0..iterations {
                            a.store(data1[thread][i], rand_store_ordering(&mut rng));
                        }
                        eprintln!("store end={:?}", now.elapsed());
                    });
                    s.spawn(|_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        let mut v = vec![data2[0][0]; iterations];
                        for i in 0..iterations {
                            v[i] = a.load(rand_load_ordering(&mut rng));
                        }
                        eprintln!("load end={:?}", now.elapsed());
                        for v in v {
                            assert!(set.contains(&v), "load unexpected={}", v);
                        }
                    });
                    s.spawn(move |_| {
                        let mut rng = fastrand::Rng::new();
                        let now = *now;
                        let mut v = vec![data2[0][0]; iterations];
                        for i in 0..iterations {
                            let old = if i % 2 == 0 {
                                rng.$int_type(..)
                            } else {
                                a.load(Ordering::Relaxed)
                            };
                            let new = data2[thread][i];
                            let o = rand_compare_exchange_ordering(&mut rng);
                            match a.compare_exchange(old, new, o.0, o.1) {
                                Ok(r) => assert_eq!(old, r),
                                Err(r) => v[i] = r,
                            }
                        }
                        eprintln!("compare_exchange end={:?}", now.elapsed());
                        for v in v {
                            assert!(set.contains(&v), "cas unexpected={}", v);
                        }
                    });
                }
            })
            .unwrap();
        }
    };
}
macro_rules! __test_atomic_float {
    ($atomic_type:ty, $float_type:ident, single_thread) => {
        #[test]
        fn swap() {
            let a = <$atomic_type>::new(5.);
            test_swap_ordering(&|order| { a.swap(5., order); });
            for &order in &helper::SWAP_ORDERINGS {
                assert_eq!(a.swap(10., order), 5.);
                assert_eq!(a.swap(5., order), 10.);
            }
        }
        #[test]
        fn compare_exchange() {
            let a = <$atomic_type>::new(5.);
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange(5., 5., success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(5.);
                assert_eq!(a.compare_exchange(5., 10., success, failure), Ok(5.));
                assert_eq!(a.load(Ordering::Relaxed), 10.);
                assert_eq!(a.compare_exchange(6., 12., success, failure), Err(10.));
                assert_eq!(a.load(Ordering::Relaxed), 10.);
            }
        }
        #[test]
        fn compare_exchange_weak() {
            let a = <$atomic_type>::new(4.);
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange_weak(4., 4., success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(4.);
                assert_eq!(a.compare_exchange_weak(6., 8., success, failure), Err(4.));
                let mut old = a.load(Ordering::Relaxed);
                loop {
                    let new = old * 2.;
                    match a.compare_exchange_weak(old, new, success, failure) {
                        Ok(_) => break,
                        Err(x) => old = x,
                    }
                }
                assert_eq!(a.load(Ordering::Relaxed), 8.);
            }
        }
        #[test]
        fn fetch_add() {
            let a = <$atomic_type>::new(0.);
            test_swap_ordering(&|order| { a.fetch_add(0., order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(0.);
                assert_eq!(a.fetch_add(10., order), 0.);
                assert_eq!(a.load(Ordering::Relaxed), 10.);
                let a = <$atomic_type>::new($float_type::MAX);
                assert_eq!(a.fetch_add(1., order), $float_type::MAX);
                assert_eq!(a.load(Ordering::Relaxed), $float_type::MAX + 1.);
            }
        }
        #[test]
        fn fetch_sub() {
            let a = <$atomic_type>::new(20.);
            test_swap_ordering(&|order| { a.fetch_sub(0., order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(20.);
                assert_eq!(a.fetch_sub(10., order), 20.);
                assert_eq!(a.load(Ordering::Relaxed), 10.);
                let a = <$atomic_type>::new($float_type::MIN);
                assert_eq!(a.fetch_sub(1., order), $float_type::MIN);
                assert_eq!(a.load(Ordering::Relaxed), $float_type::MIN - 1.);
            }
        }
        #[test]
        fn fetch_max() {
            if mem::size_of::<$float_type>() == 16
                && cfg!(any(
                    target_arch = "arm",
                    target_arch = "mips",
                    target_arch = "mips32r6",
                    target_arch = "mips64",
                    target_arch = "mips64r6",
                    target_vendor = "apple",
                    windows,
                ))
            {
                // TODO(f128):
                return;
            }
            let a = <$atomic_type>::new(23.);
            test_swap_ordering(&|order| { a.fetch_max(23., order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(23.);
                assert_eq!(a.fetch_max(22., order), 23.);
                assert_eq!(a.load(Ordering::Relaxed), 23.);
                assert_eq!(a.fetch_max(24., order), 23.);
                assert_eq!(a.load(Ordering::Relaxed), 24.);
            }
        }
        #[test]
        fn fetch_min() {
            if mem::size_of::<$float_type>() == 16
                && cfg!(any(
                    target_arch = "arm",
                    target_arch = "mips",
                    target_arch = "mips32r6",
                    target_arch = "mips64",
                    target_arch = "mips64r6",
                    target_vendor = "apple",
                    windows,
                ))
            {
                // TODO(f128):
                return;
            }
            let a = <$atomic_type>::new(23.);
            test_swap_ordering(&|order| { a.fetch_min(23., order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(23.);
                assert_eq!(a.fetch_min(24., order), 23.);
                assert_eq!(a.load(Ordering::Relaxed), 23.);
                assert_eq!(a.fetch_min(22., order), 23.);
                assert_eq!(a.load(Ordering::Relaxed), 22.);
            }
        }
        #[test]
        fn fetch_neg() {
            let a = <$atomic_type>::new(5.);
            test_swap_ordering(&|order| { a.fetch_neg(order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(5.);
                assert_eq!(a.fetch_neg(order), 5.);
                assert_eq!(a.load(Ordering::Relaxed), -5.);
                assert_eq!(a.fetch_neg(order), -5.);
                assert_eq!(a.load(Ordering::Relaxed), 5.);
            }
        }
        #[test]
        fn fetch_abs() {
            let a = <$atomic_type>::new(23.);
            test_swap_ordering(&|order| { a.fetch_abs(order); });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(-23.);
                assert_eq!(a.fetch_abs(order), -23.);
                assert_eq!(a.load(Ordering::Relaxed), 23.);
                assert_eq!(a.fetch_abs(order), 23.);
                assert_eq!(a.load(Ordering::Relaxed), 23.);
            }
        }
        ::quickcheck::quickcheck! {
            fn quickcheck_swap(x: $float_type, y: $float_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.swap(y, order), x);
                    assert_float_op_eq!(a.swap(x, order), y);
                }
                true
            }
            fn quickcheck_compare_exchange(x: $float_type, y: $float_type) -> bool {
                #[cfg(all(
                    target_arch = "arm",
                    not(any(target_feature = "v6", portable_atomic_target_feature = "v6")),
                ))]
                {
                    // TODO: LLVM bug:
                    // https://github.com/llvm/llvm-project/issues/61880
                    // https://github.com/taiki-e/portable-atomic/issues/2
                    if mem::size_of::<$float_type>() <= 2 {
                        return true;
                    }
                }
                let mut rng = fastrand::Rng::new();
                let z = loop {
                    let z = float_rand::$float_type(&mut rng);
                    if z != y {
                        break z;
                    }
                };
                for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.compare_exchange(x, y, success, failure).unwrap(), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y);
                    assert_float_op_eq!(
                        a.compare_exchange(z, x, success, failure).unwrap_err(),
                        y,
                    );
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y);
                }
                true
            }
            fn quickcheck_fetch_add(x: $float_type, y: $float_type) -> bool {
                if cfg!(all(not(debug_assertions), target_arch = "x86", not(target_feature = "sse2"))) {
                    // TODO: rustc bug: https://github.com/rust-lang/rust/issues/114479
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_add(y, order), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x + y);
                    let a = <$atomic_type>::new(y);
                    assert_float_op_eq!(a.fetch_add(x, order), y);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y + x);
                }
                true
            }
            fn quickcheck_fetch_sub(x: $float_type, y: $float_type) -> bool {
                if cfg!(all(not(debug_assertions), target_arch = "x86", not(target_feature = "sse2"))) {
                    // TODO: rustc bug: https://github.com/rust-lang/rust/issues/114479
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_sub(y, order), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x - y);
                    let a = <$atomic_type>::new(y);
                    assert_float_op_eq!(a.fetch_sub(x, order), y);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y - x);
                }
                true
            }
            fn quickcheck_fetch_max(x: $float_type, y: $float_type) -> bool {
                if mem::size_of::<$float_type>() == 16
                    && cfg!(any(
                        target_arch = "arm",
                        target_arch = "mips",
                        target_arch = "mips32r6",
                        target_arch = "mips64",
                        target_arch = "mips64r6",
                        target_vendor = "apple",
                        windows,
                    ))
                {
                    // TODO(f128):
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_max(y, order), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x.max(y));
                    let a = <$atomic_type>::new(y);
                    assert_float_op_eq!(a.fetch_max(x, order), y);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y.max(x));
                }
                true
            }
            fn quickcheck_fetch_min(x: $float_type, y: $float_type) -> bool {
                if mem::size_of::<$float_type>() == 16
                    && cfg!(any(
                        target_arch = "arm",
                        target_arch = "mips",
                        target_arch = "mips32r6",
                        target_arch = "mips64",
                        target_arch = "mips64r6",
                        target_vendor = "apple",
                        windows,
                    ))
                {
                    // TODO(f128):
                    return true;
                }
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_min(y, order), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x.min(y));
                    let a = <$atomic_type>::new(y);
                    assert_float_op_eq!(a.fetch_min(x, order), y);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), y.min(x));
                }
                true
            }
            fn quickcheck_fetch_neg(x: $float_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_neg(order), x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), -x);
                    assert_float_op_eq!(a.fetch_neg(order), -x);
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x);
                }
                true
            }
            fn quickcheck_fetch_abs(x: $float_type) -> bool {
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_float_op_eq!(a.fetch_abs(order), x);
                    assert_float_op_eq!(a.fetch_abs(order), x.abs());
                    assert_float_op_eq!(a.load(Ordering::Relaxed), x.abs());
                }
                true
            }
        }
    };
    ($atomic_type:ty, $float_type:ident) => {
        __test_atomic_float!($atomic_type, $float_type, single_thread);
        // TODO: multi thread
    };
}
macro_rules! __test_atomic_bool {
    ($atomic_type:ty, single_thread) => {
        #[test]
        fn swap() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| {
                a.swap(true, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                assert_eq!(a.swap(true, order), true);
                assert_eq!(a.swap(false, order), true);
                assert_eq!(a.swap(false, order), false);
                assert_eq!(a.swap(true, order), false);
            }
        }
        #[test]
        fn compare_exchange() {
            let a = <$atomic_type>::new(true);
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange(true, true, success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.compare_exchange(true, false, success, failure), Ok(true));
                assert_eq!(a.load(Ordering::Relaxed), false);
                assert_eq!(a.compare_exchange(true, true, success, failure), Err(false));
                assert_eq!(a.load(Ordering::Relaxed), false);
            }
        }
        #[test]
        fn compare_exchange_weak() {
            let a = <$atomic_type>::new(false);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS {
                mark_aligned_defined(&a);
            }
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange_weak(false, false, success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(false);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.compare_exchange_weak(true, true, success, failure), Err(false));
                let mut old = a.load(Ordering::Relaxed);
                let new = true;
                loop {
                    match a.compare_exchange_weak(old, new, success, failure) {
                        Ok(_) => break,
                        Err(x) => old = x,
                    }
                }
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn fetch_and() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| assert_eq!(a.fetch_and(true, order), true));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_and(false, order), true);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_and(true, order), true);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_and(false, order), false);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_and(true, order), false);
                assert_eq!(a.load(Ordering::Relaxed), false);
            }
        }
        #[test]
        fn and() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| a.and(true, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                a.and(false, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(true);
                a.and(true, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(false);
                a.and(false, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                a.and(true, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
            }
        }
        #[test]
        fn fetch_or() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| assert_eq!(a.fetch_or(false, order), true));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_or(false, order), true);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_or(true, order), true);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_or(false, order), false);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_or(true, order), false);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn or() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| a.or(false, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                a.or(false, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(true);
                a.or(true, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(false);
                a.or(false, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                a.or(true, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn fetch_xor() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| assert_eq!(a.fetch_xor(false, order), true));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_xor(false, order), true);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_xor(true, order), true);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_xor(false, order), false);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_xor(true, order), false);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn xor() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| a.xor(false, order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                a.xor(false, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(true);
                a.xor(true, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                a.xor(false, order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                a.xor(true, order);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        ::quickcheck::quickcheck! {
            fn quickcheck_compare_exchange(x: bool, y: bool) -> bool {
                let z = !y;
                for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    #[cfg(valgrind)]
                    if IMP_EMU_SUB_WORD_CAS {
                        mark_aligned_defined(&a);
                    }
                    assert_eq!(a.compare_exchange(x, y, success, failure).unwrap(), x);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                    assert_eq!(a.compare_exchange(z, x, success, failure).unwrap_err(), y);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                }
                true
            }
        }
    };
    ($atomic_type:ty) => {
        __test_atomic_bool!($atomic_type, single_thread);
        // TODO: multi thread
    };
}
macro_rules! __test_atomic_ptr {
    ($atomic_type:ty, single_thread) => {
        #[allow(unused_imports)]
        use sptr::Strict as _; // for old rustc
        #[test]
        fn swap() {
            let a = <$atomic_type>::new(ptr::null_mut());
            test_swap_ordering(&|order| {
                a.swap(ptr::null_mut(), order);
            });
            let x = &mut 1;
            for &order in &helper::SWAP_ORDERINGS {
                assert_eq!(a.swap(x, order), ptr::null_mut());
                assert_eq!(a.swap(ptr::null_mut(), order), x as *mut _);
            }
        }
        #[test]
        fn compare_exchange() {
            let a = <$atomic_type>::new(ptr::null_mut());
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange(ptr::null_mut(), ptr::null_mut(), success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(ptr::null_mut());
                let x = &mut 1;
                assert_eq!(
                    a.compare_exchange(ptr::null_mut(), x, success, failure),
                    Ok(ptr::null_mut()),
                );
                assert_eq!(a.load(Ordering::Relaxed), x as *mut _);
                assert_eq!(
                    a.compare_exchange(ptr::null_mut(), ptr::null_mut(), success, failure),
                    Err(x as *mut _),
                );
                assert_eq!(a.load(Ordering::Relaxed), x as *mut _);
            }
        }
        #[test]
        fn compare_exchange_weak() {
            let a = <$atomic_type>::new(ptr::null_mut());
            test_compare_exchange_ordering(&|success, failure| {
                let _ = a.compare_exchange_weak(ptr::null_mut(), ptr::null_mut(), success, failure);
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(ptr::null_mut());
                let x = &mut 1;
                assert_eq!(a.compare_exchange_weak(x, x, success, failure), Err(ptr::null_mut()));
                let mut old = a.load(Ordering::Relaxed);
                loop {
                    match a.compare_exchange_weak(old, x, success, failure) {
                        Ok(_) => break,
                        Err(x) => old = x,
                    }
                }
                assert_eq!(a.load(Ordering::Relaxed), x as *mut _);
            }
        }
        // https://github.com/rust-lang/rust/blob/1.84.0/library/core/tests/atomic.rs#L130-L213
        #[test]
        fn ptr_add_null() {
            let atom = AtomicPtr::<i64>::new(core::ptr::null_mut());
            assert_eq!(atom.fetch_ptr_add(1, Ordering::SeqCst).addr(), 0);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 8);

            assert_eq!(atom.fetch_byte_add(1, Ordering::SeqCst).addr(), 8);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 9);

            assert_eq!(atom.fetch_ptr_sub(1, Ordering::SeqCst).addr(), 9);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 1);

            assert_eq!(atom.fetch_byte_sub(1, Ordering::SeqCst).addr(), 1);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 0);
        }
        #[test]
        fn ptr_add_data() {
            let num = 0i64;
            let n = &num as *const i64 as *mut _;
            let atom = AtomicPtr::<i64>::new(n);
            assert_eq!(atom.fetch_ptr_add(1, Ordering::SeqCst), n);
            assert_eq!(atom.load(Ordering::SeqCst), n.wrapping_add(1));

            assert_eq!(atom.fetch_ptr_sub(1, Ordering::SeqCst), n.wrapping_add(1));
            assert_eq!(atom.load(Ordering::SeqCst), n);
            #[allow(clippy::cast_ptr_alignment)]
            let bytes_from_n = |b| n.cast::<u8>().wrapping_add(b).cast::<i64>();

            assert_eq!(atom.fetch_byte_add(1, Ordering::SeqCst), n);
            assert_eq!(atom.load(Ordering::SeqCst), bytes_from_n(1));

            assert_eq!(atom.fetch_byte_add(5, Ordering::SeqCst), bytes_from_n(1));
            assert_eq!(atom.load(Ordering::SeqCst), bytes_from_n(6));

            assert_eq!(atom.fetch_byte_sub(1, Ordering::SeqCst), bytes_from_n(6));
            assert_eq!(atom.load(Ordering::SeqCst), bytes_from_n(5));

            assert_eq!(atom.fetch_byte_sub(5, Ordering::SeqCst), bytes_from_n(5));
            assert_eq!(atom.load(Ordering::SeqCst), n);
        }
        #[test]
        fn ptr_bitops() {
            let atom = AtomicPtr::<i64>::new(core::ptr::null_mut());
            assert_eq!(atom.fetch_or(0b0111, Ordering::SeqCst).addr(), 0);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 0b0111);

            assert_eq!(atom.fetch_and(0b1101, Ordering::SeqCst).addr(), 0b0111);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 0b0101);

            assert_eq!(atom.fetch_xor(0b1111, Ordering::SeqCst).addr(), 0b0101);
            assert_eq!(atom.load(Ordering::SeqCst).addr(), 0b1010);
        }
        #[test]
        fn ptr_bitops_tagging() {
            const MASK_TAG: usize = 0b1111;
            const MASK_PTR: usize = !MASK_TAG;

            #[repr(align(16))]
            struct Tagme(#[allow(dead_code)] u128);

            let tagme = Tagme(1000);
            let ptr = &tagme as *const Tagme as *mut Tagme;
            let atom: AtomicPtr<Tagme> = AtomicPtr::new(ptr);

            assert_eq!(ptr.addr() & MASK_TAG, 0);

            assert_eq!(atom.fetch_or(0b0111, Ordering::SeqCst), ptr);
            assert_eq!(atom.load(Ordering::SeqCst), ptr.map_addr(|a| a | 0b111));

            assert_eq!(
                atom.fetch_and(MASK_PTR | 0b0010, Ordering::SeqCst),
                ptr.map_addr(|a| a | 0b111)
            );
            assert_eq!(atom.load(Ordering::SeqCst), ptr.map_addr(|a| a | 0b0010));

            assert_eq!(atom.fetch_xor(0b1011, Ordering::SeqCst), ptr.map_addr(|a| a | 0b0010));
            assert_eq!(atom.load(Ordering::SeqCst), ptr.map_addr(|a| a | 0b1001));

            assert_eq!(atom.fetch_and(MASK_PTR, Ordering::SeqCst), ptr.map_addr(|a| a | 0b1001));
            assert_eq!(atom.load(Ordering::SeqCst), ptr);
        }
        #[test]
        fn bit_set() {
            let a = <$atomic_type>::new(ptr::null_mut::<u64>().cast::<u8>().map_addr(|a| a | 1));
            test_swap_ordering(&|order| assert!(a.bit_set(0, order)));
            for &order in &helper::SWAP_ORDERINGS {
                let pointer = &mut 1u64 as *mut u64 as *mut u8;
                let atom = <$atomic_type>::new(pointer);
                // Tag the bottom bit of the pointer.
                assert!(!atom.bit_set(0, order));
                // Extract and untag.
                let tagged = atom.load(Ordering::Relaxed);
                assert_eq!(tagged.addr() & 1, 1);
                assert_eq!(tagged.map_addr(|p| p & !1), pointer);
            }
        }
        #[test]
        fn bit_clear() {
            let a = <$atomic_type>::new(ptr::null_mut::<u64>().cast::<u8>());
            test_swap_ordering(&|order| assert!(!a.bit_clear(0, order)));
            for &order in &helper::SWAP_ORDERINGS {
                let pointer = &mut 1u64 as *mut u64 as *mut u8;
                // A tagged pointer
                let atom = <$atomic_type>::new(pointer.map_addr(|a| a | 1));
                assert!(atom.bit_set(0, order));
                // Untag
                assert!(atom.bit_clear(0, order));
            }
        }
        #[test]
        fn bit_toggle() {
            let a = <$atomic_type>::new(ptr::null_mut::<u64>().cast::<u8>());
            test_swap_ordering(&|order| {
                a.bit_toggle(0, order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let pointer = &mut 1u64 as *mut u64 as *mut u8;
                let atom = <$atomic_type>::new(pointer);
                // Toggle a tag bit on the pointer.
                atom.bit_toggle(0, order);
                assert_eq!(atom.load(Ordering::Relaxed).addr() & 1, 1);
            }
        }
        ::quickcheck::quickcheck! {
            fn quickcheck_swap(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.swap(y, order), x);
                    assert_eq!(a.swap(x, order), y);
                }
                true
            }
            fn quickcheck_compare_exchange(x: usize, y: usize) -> bool {
                let mut rng = fastrand::Rng::new();
                let z = loop {
                    let z = rng.usize(..);
                    if z != y {
                        break z;
                    }
                };
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut(y);
                let z = sptr::invalid_mut(z);
                for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.compare_exchange(x, y, success, failure).unwrap(), x);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                    assert_eq!(a.compare_exchange(z, x, success, failure).unwrap_err(), y);
                    assert_eq!(a.load(Ordering::Relaxed), y);
                }
                true
            }
            fn quickcheck_fetch_byte_add(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut::<u8>(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_byte_add(y.addr(), order), x);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr().wrapping_add(y.addr()));
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_byte_add(x.addr(), order), y);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), y.addr().wrapping_add(x.addr()));
                }
                true
            }
            fn quickcheck_fetch_byte_sub(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut::<u8>(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_byte_sub(y.addr(), order), x);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr().wrapping_sub(y.addr()));
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_byte_sub(x.addr(), order), y);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), y.addr().wrapping_sub(x.addr()));
                }
                true
            }
            fn quickcheck_fetch_and(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut::<u8>(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_and(y.addr(), order), x);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() & y.addr());
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_and(x.addr(), order), y);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), y.addr() & x.addr());
                }
                true
            }
            fn quickcheck_fetch_or(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut::<u8>(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_or(y.addr(), order), x);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() | y.addr());
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_or(x.addr(), order), y);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), y.addr() | x.addr());
                }
                true
            }
            fn quickcheck_fetch_xor(x: usize, y: usize) -> bool {
                let x = sptr::invalid_mut(x);
                let y = sptr::invalid_mut::<u8>(y);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(a.fetch_xor(y.addr(), order), x);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() ^ y.addr());
                    let a = <$atomic_type>::new(y);
                    assert_eq!(a.fetch_xor(x.addr(), order), y);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), y.addr() ^ x.addr());
                }
                true
            }
            fn quickcheck_bit_set(x: usize, bit: u32) -> bool {
                let x = sptr::invalid_mut(x);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_set(bit, order);
                    let mask = <usize>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() | mask);
                    assert_eq!(b, x.addr() & mask != 0);
                }
                true
            }
            fn quickcheck_bit_clear(x: usize, bit: u32) -> bool {
                let x = sptr::invalid_mut(x);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_clear(bit, order);
                    let mask = <usize>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() & !mask);
                    assert_eq!(b, x.addr() & mask != 0);
                }
                true
            }
            fn quickcheck_bit_toggle(x: usize, bit: u32) -> bool {
                let x = sptr::invalid_mut(x);
                for &order in &helper::SWAP_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    let b = a.bit_toggle(bit, order);
                    let mask = <usize>::wrapping_shl(1, bit);
                    assert_eq!(a.load(Ordering::Relaxed).addr(), x.addr() ^ mask);
                    assert_eq!(b, x.addr() & mask != 0);
                }
                true
            }
        }
    };
    ($atomic_type:ty) => {
        __test_atomic_ptr!($atomic_type, single_thread);
        // TODO: multi thread
    };
}

macro_rules! __test_atomic_int_pub {
    ($atomic_type:ty, $int_type:ident) => {
        __test_atomic_pub_common!($atomic_type, $int_type);
        use std::boxed::Box;

        #[allow(unused_imports)]
        use sptr::Strict as _; // for old rustc
        #[test]
        fn fetch_update() {
            // TODO(gcc): failure
            if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                && !cfg!(debug_assertions)
            {
                return;
            }
            // TODO(riscv): wrong result (as of Valgrind 3.26)
            #[cfg(valgrind)]
            if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                return;
            }
            let a = <$atomic_type>::new(7);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                mark_aligned_defined(&a);
            }
            test_compare_exchange_ordering(&|set, fetch| {
                let _ = a.fetch_update(set, fetch, |x| Some(x));
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(7);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS && mem::size_of::<$int_type>() <= 2 {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_update(success, failure, |_| None), Err(7));
                assert_eq!(a.fetch_update(success, failure, |x| Some(x + 1)), Ok(7));
                assert_eq!(a.fetch_update(success, failure, |x| Some(x + 1)), Ok(8));
                assert_eq!(a.load(Ordering::SeqCst), 9);
            }
        }
        #[test]
        fn impls() {
            #[cfg(not(portable_atomic_no_const_transmute))]
            const INTO_INNER: $int_type = {
                let a = <$atomic_type>::new(10);
                a.into_inner()
            };
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            const GET_MUT: $atomic_type = {
                let mut a = <$atomic_type>::new(10);
                let _ = unsafe { <$atomic_type>::from_ptr(a.as_ptr()) };
                *a.get_mut() = 5;
                a
            };
            #[cfg(not(portable_atomic_no_const_transmute))]
            {
                assert_eq!(INTO_INNER, 10);
            }
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            {
                assert_eq!(GET_MUT.into_inner(), 5);
            }
            let a = <$atomic_type>::default();
            let b = <$atomic_type>::from(0);
            assert_eq!(a.load(Ordering::SeqCst), b.load(Ordering::SeqCst));
            assert_eq!(std::format!("{:?}", a), std::format!("{:?}", a.load(Ordering::SeqCst)));
            assert_eq!(a.into_inner(), 0);
            assert_eq!(b.into_inner(), 0);

            unsafe {
                let ptr: *mut Align16<$int_type> = Box::into_raw(Box::new(Align16(0)));
                assert!(ptr.addr() % mem::align_of::<$atomic_type>() == 0);
                {
                    let a = <$atomic_type>::from_ptr(ptr.cast::<$int_type>());
                    *a.as_ptr() = 1;
                }
                assert_eq!((*ptr).0, 1);
                drop(Box::from_raw(ptr));
            }
        }
        ::quickcheck::quickcheck! {
            fn quickcheck_fetch_update(x: $int_type, y: $int_type) -> bool {
                // TODO(riscv): wrong result (as of Valgrind 3.26)
                #[cfg(valgrind)]
                if cfg!(target_arch = "riscv64") && mem::size_of::<$int_type>() <= 2 {
                    return true;
                }
                let mut rng = fastrand::Rng::new();
                let z = loop {
                    let z = rng.$int_type(..);
                    if z != y {
                        break z;
                    }
                };
                for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                    let a = <$atomic_type>::new(x);
                    assert_eq!(
                        a.fetch_update(success, failure, |_| Some(y))
                        .unwrap(),
                        x
                    );
                    assert_eq!(
                        a.fetch_update(success, failure, |_| Some(z))
                        .unwrap(),
                        y
                    );
                    assert_eq!(a.load(Ordering::Relaxed), z);
                    assert_eq!(
                        a.fetch_update(success, failure, |z| if z == y { Some(z) } else { None })
                        .unwrap_err(),
                        z
                    );
                    assert_eq!(a.load(Ordering::Relaxed), z);
                }
                true
            }
        }
    };
}
macro_rules! __test_atomic_float_pub {
    ($atomic_type:ty, $float_type:ident) => {
        __test_atomic_pub_common!($atomic_type, $float_type);
        use std::boxed::Box;

        #[allow(unused_imports)]
        use sptr::Strict as _; // for old rustc
        #[test]
        fn fetch_update() {
            let a = <$atomic_type>::new(7.);
            test_compare_exchange_ordering(&|set, fetch| {
                let _ = a.fetch_update(set, fetch, |x| Some(x));
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(7.);
                assert_eq!(a.fetch_update(success, failure, |_| None), Err(7.));
                assert_eq!(a.fetch_update(success, failure, |x| Some(x + 1.)), Ok(7.));
                assert_eq!(a.fetch_update(success, failure, |x| Some(x + 1.)), Ok(8.));
                assert_eq!(a.load(Ordering::SeqCst), 9.);
            }
        }
        #[test]
        fn impls() {
            #[cfg(not(portable_atomic_no_const_transmute))]
            const INTO_INNER: $float_type = {
                let a = <$atomic_type>::new(10.);
                a.into_inner()
            };
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            const GET_MUT: $atomic_type = {
                let mut a = <$atomic_type>::new(10.);
                let _ = unsafe { <$atomic_type>::from_ptr(a.as_ptr()) };
                *a.get_mut() = 5.;
                a
            };
            #[cfg(not(portable_atomic_no_const_transmute))]
            {
                assert_eq!(INTO_INNER, 10.);
            }
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            {
                assert_eq!(GET_MUT.into_inner(), 5.);
            }
            let a = <$atomic_type>::default();
            let b = <$atomic_type>::from(0.);
            assert_eq!(a.load(Ordering::SeqCst), b.load(Ordering::SeqCst));
            assert_eq!(std::format!("{:?}", a), std::format!("{:?}", a.load(Ordering::SeqCst)));
            assert_eq!(a.into_inner(), 0.);
            assert_eq!(b.into_inner(), 0.);

            unsafe {
                let ptr: *mut Align16<$float_type> = Box::into_raw(Box::new(Align16(0.)));
                assert!(ptr.addr() % mem::align_of::<$atomic_type>() == 0);
                {
                    let a = <$atomic_type>::from_ptr(ptr.cast::<$float_type>());
                    *a.as_ptr() = 1.;
                }
                assert_eq!((*ptr).0, 1.);
                drop(Box::from_raw(ptr));
            }
        }
    };
}
macro_rules! __test_atomic_bool_pub {
    ($atomic_type:ty) => {
        __test_atomic_pub_common!($atomic_type, bool);
        use std::boxed::Box;

        #[allow(unused_imports)]
        use sptr::Strict as _; // for old rustc
        #[test]
        fn fetch_nand() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| assert_eq!(a.fetch_nand(false, order), true));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_nand(false, order), true);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_nand(true, order), true);
                assert_eq!(a.load(Ordering::Relaxed) as usize, 0);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_nand(false, order), false);
                assert_eq!(a.load(Ordering::Relaxed), true);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_nand(true, order), false);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn fetch_not() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| {
                a.fetch_not(order);
            });
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                assert_eq!(a.fetch_not(order), true);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                assert_eq!(a.fetch_not(order), false);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn not() {
            let a = <$atomic_type>::new(true);
            test_swap_ordering(&|order| a.not(order));
            for &order in &helper::SWAP_ORDERINGS {
                let a = <$atomic_type>::new(true);
                a.not(order);
                assert_eq!(a.load(Ordering::Relaxed), false);
                let a = <$atomic_type>::new(false);
                a.not(order);
                assert_eq!(a.load(Ordering::Relaxed), true);
            }
        }
        #[test]
        fn fetch_update() {
            // TODO(gcc): failure
            if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                && !cfg!(debug_assertions)
            {
                return;
            }
            let a = <$atomic_type>::new(false);
            #[cfg(valgrind)]
            if IMP_EMU_SUB_WORD_CAS {
                mark_aligned_defined(&a);
            }
            test_compare_exchange_ordering(&|set, fetch| {
                let _ = a.fetch_update(set, fetch, |x| Some(x));
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(false);
                #[cfg(valgrind)]
                if IMP_EMU_SUB_WORD_CAS {
                    mark_aligned_defined(&a);
                }
                assert_eq!(a.fetch_update(success, failure, |_| None), Err(false));
                assert_eq!(a.fetch_update(success, failure, |x| Some(!x)), Ok(false));
                assert_eq!(a.fetch_update(success, failure, |x| Some(!x)), Ok(true));
                assert_eq!(a.load(Ordering::SeqCst), false);
            }
        }
        #[test]
        fn impls() {
            #[cfg(not(portable_atomic_no_const_transmute))]
            const INTO_INNER: bool = {
                let a = <$atomic_type>::new(true);
                a.into_inner()
            };
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            const GET_MUT: $atomic_type = {
                let mut a = <$atomic_type>::new(true);
                let _ = unsafe { <$atomic_type>::from_ptr(a.as_ptr()) };
                *a.get_mut() = false;
                a
            };
            #[cfg(not(portable_atomic_no_const_transmute))]
            {
                assert_eq!(INTO_INNER, true);
            }
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            {
                assert_eq!(GET_MUT.into_inner(), false);
            }
            let a = <$atomic_type>::default();
            let b = <$atomic_type>::from(false);
            assert_eq!(a.load(Ordering::SeqCst), b.load(Ordering::SeqCst));
            assert_eq!(std::format!("{:?}", a), std::format!("{:?}", a.load(Ordering::SeqCst)));
            assert_eq!(a.into_inner(), false);
            assert_eq!(b.into_inner(), false);

            unsafe {
                let ptr: *mut bool = Box::into_raw(Box::new(false));
                assert!(ptr.addr() % mem::align_of::<$atomic_type>() == 0);
                {
                    let a = <$atomic_type>::from_ptr(ptr);
                    *a.as_ptr() = true;
                }
                assert_eq!((*ptr), true);
                drop(Box::from_raw(ptr));
            }
        }
    };
}
macro_rules! __test_atomic_ptr_pub {
    ($atomic_type:ty) => {
        __test_atomic_pub_common!($atomic_type, *mut u8);
        use std::boxed::Box;
        #[test]
        fn fetch_update() {
            // TODO(gcc): failure
            if option_env!("RUSTC_CODEGEN_GCC").unwrap_or_default() == "1"
                && !cfg!(debug_assertions)
            {
                return;
            }
            let a = <$atomic_type>::new(ptr::null_mut());
            test_compare_exchange_ordering(&|set, fetch| {
                let _ = a.fetch_update(set, fetch, |x| Some(x));
            });
            for &(success, failure) in &helper::COMPARE_EXCHANGE_ORDERINGS {
                let a = <$atomic_type>::new(ptr::null_mut());
                assert_eq!(a.fetch_update(success, failure, |_| None), Err(ptr::null_mut()));
                assert_eq!(
                    a.fetch_update(success, failure, |_| Some(&a as *const _ as *mut _)),
                    Ok(ptr::null_mut())
                );
                assert_eq!(a.load(Ordering::SeqCst), &a as *const _ as *mut _);
            }
        }
        #[test]
        fn impls() {
            #[cfg(not(portable_atomic_no_const_transmute))]
            const INTO_INNER: *mut u8 = {
                let a = <$atomic_type>::new(ptr::null_mut());
                a.into_inner()
            };
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            const GET_MUT: $atomic_type = {
                let mut a = <$atomic_type>::new(ptr::null_mut());
                let _ = unsafe { <$atomic_type>::from_ptr(a.as_ptr()) };
                *a.get_mut() = ptr::null_mut::<u8>().wrapping_add(1);
                a
            };
            #[cfg(not(portable_atomic_no_const_transmute))]
            {
                assert!(INTO_INNER.is_null());
            }
            #[cfg(not(portable_atomic_no_const_mut_refs))]
            {
                assert_eq!(GET_MUT.into_inner(), ptr::null_mut::<u8>().wrapping_add(1));
            }
            let a = <$atomic_type>::default();
            let b = <$atomic_type>::from(ptr::null_mut());
            assert_eq!(a.load(Ordering::SeqCst), b.load(Ordering::SeqCst));
            assert_eq!(std::format!("{:?}", a), std::format!("{:?}", a.load(Ordering::SeqCst)));
            assert_eq!(std::format!("{:p}", a), std::format!("{:p}", a.load(Ordering::SeqCst)));
            assert_eq!(a.into_inner(), ptr::null_mut());
            assert_eq!(b.into_inner(), ptr::null_mut());

            unsafe {
                let ptr: *mut Align16<*mut u8> = Box::into_raw(Box::new(Align16(ptr::null_mut())));
                assert!(ptr.addr() % mem::align_of::<$atomic_type>() == 0);
                {
                    let a = <$atomic_type>::from_ptr(ptr.cast::<*mut u8>());
                    *a.as_ptr() = ptr::null_mut::<u8>().wrapping_add(1);
                }
                assert_eq!((*ptr).0, ptr::null_mut::<u8>().wrapping_add(1));
                drop(Box::from_raw(ptr));
            }
        }
    };
}

macro_rules! test_atomic_int_load_store {
    ($int_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<test_atomic_ $int_type>] {
                use super::*;
                __test_atomic_int_load_store!([<Atomic $int_type:camel>], $int_type);
            }
        }
    };
}
macro_rules! test_atomic_ptr_load_store {
    () => {
        #[allow(
            clippy::alloc_instead_of_core,
            clippy::std_instead_of_alloc,
            clippy::std_instead_of_core,
            clippy::undocumented_unsafe_blocks
        )]
        mod test_atomic_ptr {
            use super::*;
            __test_atomic_ptr_load_store!(AtomicPtr<u8>);
        }
    };
}

macro_rules! test_atomic_int_single_thread {
    ($int_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<test_atomic_ $int_type>] {
                use super::*;
                __test_atomic_int_load_store!([<Atomic $int_type:camel>], $int_type, single_thread);
                __test_atomic_int!([<Atomic $int_type:camel>], $int_type, single_thread);
            }
        }
    };
}
macro_rules! test_atomic_ptr_single_thread {
    () => {
        #[allow(
            clippy::alloc_instead_of_core,
            clippy::std_instead_of_alloc,
            clippy::std_instead_of_core,
            clippy::undocumented_unsafe_blocks
        )]
        mod test_atomic_ptr {
            use super::*;
            __test_atomic_ptr_load_store!(AtomicPtr<u8>, single_thread);
            __test_atomic_ptr!(AtomicPtr<u8>, single_thread);
        }
    };
}

macro_rules! test_atomic_int {
    ($int_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<test_atomic_ $int_type>] {
                use super::*;
                __test_atomic_int_load_store!([<Atomic $int_type:camel>], $int_type);
                __test_atomic_int!([<Atomic $int_type:camel>], $int_type);
            }
        }
    };
}
macro_rules! test_atomic_ptr {
    () => {
        #[allow(
            clippy::alloc_instead_of_core,
            clippy::std_instead_of_alloc,
            clippy::std_instead_of_core,
            clippy::undocumented_unsafe_blocks
        )]
        #[allow(unstable_name_collisions)] // for sptr crate
        mod test_atomic_ptr {
            use super::*;
            __test_atomic_ptr_load_store!(AtomicPtr<u8>);
            __test_atomic_ptr!(AtomicPtr<u8>);
        }
    };
}

macro_rules! test_atomic_int_pub {
    ($int_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<test_atomic_ $int_type>] {
                use super::*;
                __test_atomic_int_load_store!([<Atomic $int_type:camel>], $int_type);
                __test_atomic_int!([<Atomic $int_type:camel>], $int_type);
                __test_atomic_int_pub!([<Atomic $int_type:camel>], $int_type);
            }
        }
    };
}
#[cfg(feature = "float")]
macro_rules! test_atomic_float_pub {
    ($float_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::float_arithmetic,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<test_atomic_ $float_type>] {
                use super::*;
                __test_atomic_float_load_store!([<Atomic $float_type:camel>], $float_type);
                __test_atomic_float!([<Atomic $float_type:camel>], $float_type);
                __test_atomic_float_pub!([<Atomic $float_type:camel>], $float_type);
            }
        }
    };
}
macro_rules! test_atomic_bool_pub {
    () => {
        #[allow(
            clippy::alloc_instead_of_core,
            clippy::std_instead_of_alloc,
            clippy::std_instead_of_core,
            clippy::undocumented_unsafe_blocks
        )]
        mod test_atomic_bool {
            use super::*;
            __test_atomic_bool_load_store!(AtomicBool);
            __test_atomic_bool!(AtomicBool);
            __test_atomic_bool_pub!(AtomicBool);
        }
    };
}
macro_rules! test_atomic_ptr_pub {
    () => {
        #[allow(
            clippy::alloc_instead_of_core,
            clippy::std_instead_of_alloc,
            clippy::std_instead_of_core,
            clippy::undocumented_unsafe_blocks
        )]
        #[allow(unstable_name_collisions)] // for sptr crate
        mod test_atomic_ptr {
            use super::*;
            __test_atomic_ptr_load_store!(AtomicPtr<u8>);
            __test_atomic_ptr!(AtomicPtr<u8>);
            __test_atomic_ptr_pub!(AtomicPtr<u8>);
        }
    };
}

// Asserts that `$a` and `$b` have performed equivalent operations.
#[cfg(feature = "float")]
macro_rules! assert_float_op_eq {
    ($a:expr, $b:expr $(,)?) => {{
        // See also:
        // - https://github.com/rust-lang/unsafe-code-guidelines/issues/237.
        // - https://github.com/rust-lang/portable-simd/issues/39.
        let a = $a;
        let b = $b;
        if a.is_nan() && b.is_nan() // don't check sign of NaN: https://github.com/rust-lang/rust/issues/55131
            || a.is_infinite()
                && b.is_infinite()
                && a.is_sign_positive() == b.is_sign_positive()
                && a.is_sign_negative() == b.is_sign_negative()
        {
            // ok
        } else {
            assert_eq!(a, b);
        }
    }};
}

#[allow(unused_unsafe)] // for old rustc
fn assert_panic(f: &dyn Fn()) -> std::string::String {
    let backtrace = std::env::var_os("RUST_BACKTRACE");
    let hook = std::panic::take_hook();
    // set_var/remove_var is fine as we run tests with RUST_TEST_THREADS=1
    // std::panic::set_backtrace_style is better way here, but is unstable.
    unsafe { std::env::set_var("RUST_BACKTRACE", "0") } // Suppress backtrace
    std::panic::set_hook(std::boxed::Box::new(|_| {})); // Suppress panic msg
    let res = std::panic::catch_unwind(std::panic::AssertUnwindSafe(f));
    std::panic::set_hook(hook);
    match backtrace {
        Some(v) => unsafe { std::env::set_var("RUST_BACKTRACE", v) },
        None => unsafe { std::env::remove_var("RUST_BACKTRACE") },
    }
    let msg = res.unwrap_err();
    msg.downcast_ref::<std::string::String>()
        .cloned()
        .unwrap_or_else(|| msg.downcast_ref::<&'static str>().copied().unwrap().into())
}
pub(crate) const LOAD_ORDERINGS: [Ordering; 3] =
    [Ordering::Relaxed, Ordering::Acquire, Ordering::SeqCst];
pub(crate) fn rand_load_ordering(rng: &mut fastrand::Rng) -> Ordering {
    LOAD_ORDERINGS[rng.usize(0..LOAD_ORDERINGS.len())]
}
pub(crate) fn test_load_ordering(f: &dyn Fn(Ordering)) {
    for &order in &LOAD_ORDERINGS {
        f(order);
    }
    if !skip_should_panic_test() {
        assert_eq!(
            assert_panic(&|| f(Ordering::Release)),
            "there is no such thing as a release load"
        );
        assert_eq!(
            assert_panic(&|| f(Ordering::AcqRel)),
            "there is no such thing as an acquire-release load"
        );
    }
}
pub(crate) const STORE_ORDERINGS: [Ordering; 3] =
    [Ordering::Relaxed, Ordering::Release, Ordering::SeqCst];
pub(crate) fn rand_store_ordering(rng: &mut fastrand::Rng) -> Ordering {
    STORE_ORDERINGS[rng.usize(0..STORE_ORDERINGS.len())]
}
pub(crate) fn test_store_ordering(f: &dyn Fn(Ordering)) {
    for &order in &STORE_ORDERINGS {
        f(order);
    }
    if !skip_should_panic_test() {
        assert_eq!(
            assert_panic(&|| f(Ordering::Acquire)),
            "there is no such thing as an acquire store"
        );
        assert_eq!(
            assert_panic(&|| f(Ordering::AcqRel)),
            "there is no such thing as an acquire-release store"
        );
    }
}
pub(crate) const SWAP_ORDERINGS: [Ordering; 5] =
    [Ordering::Relaxed, Ordering::Release, Ordering::Acquire, Ordering::AcqRel, Ordering::SeqCst];
pub(crate) fn rand_swap_ordering(rng: &mut fastrand::Rng) -> Ordering {
    SWAP_ORDERINGS[rng.usize(0..SWAP_ORDERINGS.len())]
}
pub(crate) fn test_swap_ordering(f: &dyn Fn(Ordering)) {
    for &order in &SWAP_ORDERINGS {
        f(order);
    }
}
pub(crate) const COMPARE_EXCHANGE_ORDERINGS: [(Ordering, Ordering); 15] = [
    (Ordering::Relaxed, Ordering::Relaxed),
    (Ordering::Relaxed, Ordering::Acquire),
    (Ordering::Relaxed, Ordering::SeqCst),
    (Ordering::Acquire, Ordering::Relaxed),
    (Ordering::Acquire, Ordering::Acquire),
    (Ordering::Acquire, Ordering::SeqCst),
    (Ordering::Release, Ordering::Relaxed),
    (Ordering::Release, Ordering::Acquire),
    (Ordering::Release, Ordering::SeqCst),
    (Ordering::AcqRel, Ordering::Relaxed),
    (Ordering::AcqRel, Ordering::Acquire),
    (Ordering::AcqRel, Ordering::SeqCst),
    (Ordering::SeqCst, Ordering::Relaxed),
    (Ordering::SeqCst, Ordering::Acquire),
    (Ordering::SeqCst, Ordering::SeqCst),
];
pub(crate) fn rand_compare_exchange_ordering(rng: &mut fastrand::Rng) -> (Ordering, Ordering) {
    COMPARE_EXCHANGE_ORDERINGS[rng.usize(0..COMPARE_EXCHANGE_ORDERINGS.len())]
}
pub(crate) fn test_compare_exchange_ordering(f: &dyn Fn(Ordering, Ordering)) {
    for &(success, failure) in &COMPARE_EXCHANGE_ORDERINGS {
        f(success, failure);
    }
    if !skip_should_panic_test() {
        for &order in &SWAP_ORDERINGS {
            let msg = assert_panic(&|| f(order, Ordering::AcqRel));
            assert!(
                msg == "there is no such thing as an acquire-release failure ordering"
                    || msg == "there is no such thing as an acquire-release load",
                "{}",
                msg
            );
            let msg = assert_panic(&|| f(order, Ordering::Release));
            assert!(
                msg == "there is no such thing as a release failure ordering"
                    || msg == "there is no such thing as a release load",
                "{}",
                msg
            );
        }
    }
}

// Used in stress tests to find atomicity bug generated by __test_atomic_* macro.
pub(crate) fn stress_test_config(rng: &mut fastrand::Rng) -> (usize, usize) {
    let iterations = if cfg!(miri) {
        50
    } else if cfg!(any(debug_assertions, qemu, valgrind)) {
        5_000
    } else {
        25_000
    };
    let threads = if cfg!(any(debug_assertions, valgrind)) { 2 } else { rng.usize(2..=8) };
    eprintln!("threads={}", threads);
    (iterations, threads)
}

// true if the current implementation implements sub-word atomics using word-size CAS.
#[cfg(valgrind)]
pub(crate) const IMP_EMU_SUB_WORD_CAS: bool = cfg!(target_arch = "s390x");

#[cfg(valgrind)]
#[inline(always)]
pub(crate) fn mark_aligned_defined<T: ?Sized>(a: &T) {
    test_helper::valgrind::make_aligned_mem_defined::<T, 4>(a);
}

fn skip_should_panic_test() -> bool {
    // Miri's panic handling is slow
    // MSAN false positive: https://gist.github.com/taiki-e/dd6269a8ffec46284fdc764a4849f884
    // Valgrind false positive on s390x
    is_panic_abort()
        || cfg!(miri)
        || option_env!("CARGO_PROFILE_RELEASE_LTO").map_or(false, |v| v == "fat")
            && build_context::SANITIZE.contains("memory")
        || cfg!(all(valgrind, target_arch = "s390x"))
}

// For -C panic=abort -Z panic_abort_tests: https://github.com/rust-lang/rust/issues/67650
fn is_panic_abort() -> bool {
    !matches!(build_context::PANIC, "unwind" | "") // cfg(panic) requires Rust 1.60
}

#[repr(C, align(16))]
pub(crate) struct Align16<T>(pub(crate) T);

// Stress tests to find memory ordering bug.
// Note that this is not exhaustive and only tests a few cases.
macro_rules! __stress_acquire_release {
    (should_pass,
        $int_type:ident, $read:ident, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        paste::paste! {
            #[test]
            #[cfg_attr(all(any(debug_assertions, valgrind_cross), not(miri)), ignore = "slow in some environments")] // debug mode is slow.
            #[allow(
                clippy::cast_possible_wrap,
                clippy::cast_sign_loss,
                clippy::cast_possible_truncation,
            )]
            fn [<acquire_release1 _ $read _ $load_order:lower _ $write _ $store_order:lower>]() {
                __call_stress_test_fn!(acquire_release1, [<Atomic $int_type:camel>],
                    $int_type, $read, $write, $load_order, $store_order);
            }
        }
    };
    (can_panic,
        $int_type:ident, $read:ident, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        paste::paste! {
            // Currently, to make this test work well enough outside of Miri, tens of thousands
            // of iterations are needed, but this test is slow in some environments.
            // So, ignore on non-Miri environments by default. See also catch_unwind_on_weak_memory_arch.
            #[test]
            #[cfg_attr(not(miri), ignore = "slow in some environments")]
            #[allow(
                clippy::cast_possible_wrap,
                clippy::cast_sign_loss,
                clippy::cast_possible_truncation,
            )]
            fn [<acquire_release1 _ $read _ $load_order:lower _ $write _ $store_order:lower>]() {
                use crate::tests::helper::catch_unwind_on_weak_memory_arch as can_panic;
                can_panic("a=", test_helper::function_name!(), || {
                    __call_stress_test_fn!(acquire_release1, [<Atomic $int_type:camel>],
                        $int_type, $read, $write, $load_order, $store_order);
                });
            }
        }
    };
}
macro_rules! __stress_seqcst {
    (should_pass,
        $int_type:ident, $read:ident, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        paste::paste! {
            // Currently, to make this test work well enough outside of Miri, tens of thousands
            // of iterations are needed, but this test is slow in some environments.
            // So, ignore on QEMU by default.
            #[test]
            #[cfg_attr(any(all(any(debug_assertions, valgrind_cross), not(miri)), qemu), ignore = "slow in some environments")] // debug mode is slow.
            #[allow(
                clippy::cast_possible_wrap,
                clippy::cast_sign_loss,
                clippy::cast_possible_truncation,
            )]
            fn [<seqcst1 _ $read _ $load_order:lower _ $write _ $store_order:lower>]() {
                __call_stress_test_fn!(seqcst1, [<Atomic $int_type:camel>],
                    $int_type, $read, $write, $load_order, $store_order);
            }
        }
    };
    (can_panic,
        $int_type:ident, $read:ident, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        paste::paste! {
            // Currently, to make this test work well enough outside of Miri, tens of thousands
            // of iterations are needed, but this test is slow in some environments.
            // So, ignore on non-Miri environments by default. See also catch_unwind_on_non_seqcst_arch.
            #[test]
            #[cfg_attr(not(miri), ignore = "slow in some environments")]
            #[allow(
                clippy::cast_possible_wrap,
                clippy::cast_sign_loss,
                clippy::cast_possible_truncation,
            )]
            fn [<seqcst1 _ $read _ $load_order:lower _ $write _ $store_order:lower>]() {
                use crate::tests::helper::catch_unwind_on_non_seqcst_arch as can_panic;
                can_panic("c=2", test_helper::function_name!(), || {
                    __call_stress_test_fn!(seqcst1, [<Atomic $int_type:camel>],
                        $int_type, $read, $write, $load_order, $store_order);
                });
            }
        }
    };
}
// To test this working correctly:
// - Uncomment can_panic case in __stress_seqcst,
//   and run test with `--release -- acquire_release1 --ignored`.
//   At least on AArch64 machine, some "panicked as expected" will be shown.
pub(crate) fn acquire_release1<T: Send + Sync>(
    min: isize,
    max: isize,
    new: impl Fn(isize) -> T,
    load: impl Fn(&T) -> isize + Send + Sync,
    store: impl Fn(&T, isize) + Send + Sync,
) {
    let n: usize = if cfg!(miri) {
        10
    } else if cfg!(any(qemu, valgrind)) {
        5_000
    } else {
        50_000
    };

    let mut start: isize = 0;
    #[allow(clippy::cast_possible_wrap)]
    let mut end: isize = n as isize;
    // This test is relatively fast because it spawns only one thread, but
    // the iterations are limited to a maximum value of integers.
    if max < end {
        start = min;
        end = max;
    }
    assert!((start..end).len() <= n, "{}", (start..end).len());
    let a = &new(start);
    let b = &AtomicIsize::new(start);
    let a_ready = &AtomicUsize::new(0);
    let b_ready = &AtomicUsize::new(0);
    thread::scope(|s| {
        s.spawn(|_| {
            while a_ready.load(Ordering::Relaxed) == 0 {}
            b_ready.store(1, Ordering::Relaxed);
            for i in start..end {
                b.store(i, Ordering::Relaxed);
                store(a, i);
            }
        });
        let end = end - 1;
        a_ready.store(1, Ordering::Relaxed);
        while b_ready.load(Ordering::Relaxed) == 0 {}
        loop {
            let a = load(a);
            let b = b.load(Ordering::Relaxed);
            assert!(a <= b, "a={},b={}", a, b);
            if a == end && b == end {
                break;
            }
        }
    })
    .unwrap();
}
// To test this working correctly:
// - Uncomment can_panic case in __stress_seqcst,
//   and run test with `--release -- seqcst1 --ignored`.
//   At least on AArch64 machine, some "panicked as expected" will be shown.
// - Change aarch64.rs's 128-bit seqcst load from `ldar; ldp; dmb ishld` to `ldp; dmb ish`
//   and run test with `--release -- seqcst1` on AArch64 macOS or other AArch64 which supports FEAT_LSE2
//   with `RUSTFLAGS=-C target-feature=+lse2`.
//   tests::stress_swap_u128::seqcst1_load_seqcst_swap_seqcst should fail within a few repetitions.
pub(crate) fn seqcst1<T: Send + Sync>(
    _min: isize,
    #[allow(unused_variables)] max: isize,
    new: impl Fn(isize) -> T,
    load: impl Fn(&T) -> isize + Send + Sync,
    store: impl Fn(&T, isize) + Send + Sync,
) {
    let n: usize = if cfg!(miri) {
        8
    } else if cfg!(valgrind)
        || build_context::SANITIZE.contains("address")
        || build_context::SANITIZE.contains("memory")
    {
        50
    } else if cfg!(qemu)
        || option_env!("GITHUB_ACTIONS").is_some() && cfg!(not(target_os = "linux"))
    {
        // GitHub Actions' macOS and Windows runners are slow.
        5_000
    } else {
        50_000
    };

    let a = &new(0);
    let b = &new(0);
    let c = &AtomicUsize::new(0);
    let a_done = &AtomicUsize::new(0);
    let a_ready = &AtomicUsize::new(0);
    let b_ready = &AtomicUsize::new(0);
    let panicked = &AtomicUsize::new(0);
    thread::scope(|s| {
        s.spawn(|_| {
            for n in 1..n {
                // Acquire to make initialization of a/b/c visible.
                while a_ready.load(Ordering::Acquire) != n {
                    if panicked.load(Ordering::Relaxed) != 0 {
                        return;
                    }
                }
                b_ready.store(n, Ordering::Relaxed);
                store(a, 1);
                if load(b) == 0 {
                    c.fetch_add(1, Ordering::Relaxed);
                }
                // Release to make update of c visible.
                a_done.store(n, Ordering::Release);
            }
        });
        for n in 1..n {
            store(a, 0);
            store(b, 0);
            c.store(0, Ordering::Relaxed);
            // Release to make initialization of a/b/c visible.
            a_ready.store(n, Ordering::Release);
            while b_ready.load(Ordering::Relaxed) != n {}
            store(b, 1);
            if load(a) == 0 {
                c.fetch_add(1, Ordering::Relaxed);
            }
            // Acquire to make update of c visible.
            while a_done.load(Ordering::Acquire) != n {}
            let c = c.load(Ordering::Relaxed);
            let ok = matches!(c, 0 | 1);
            if !ok {
                panicked.store(1, Ordering::Relaxed);
                panic!("c={},n={}", c, n);
            }
        }
    })
    .unwrap();
}
// Catches unwinding panic on architectures with weak memory models.
#[allow(dead_code)]
pub(crate) fn catch_unwind_on_weak_memory_arch(pat: &str, function_name: &str, f: impl Fn()) {
    // With x86 TSO, RISC-V TSO (optional), SPARC TSO (optional, default?),
    // and IBM-370 memory models should never be a panic here.
    // Miri emulates weak memory models regardless of target architectures.
    if cfg!(all(
        any(
            target_arch = "x86",
            target_arch = "x86_64",
            target_arch = "s390x",
            target_arch = "sparc",
            target_arch = "sparc64",
            all(any(target_arch = "riscv32", target_arch = "riscv64"), target_feature = "ztso"),
        ),
        not(any(miri)),
    )) {
        f();
    } else if !is_panic_abort() {
        // This could be Err on architectures with weak memory models.
        // However, this does not necessarily mean that it will always be panic,
        // and implementing it with stronger orderings is also okay.
        match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
            Ok(()) => {
                // panic!();
            }
            Err(msg) => {
                let msg = msg
                    .downcast_ref::<std::string::String>()
                    .cloned()
                    .unwrap_or_else(|| msg.downcast_ref::<&'static str>().copied().unwrap().into());
                assert!(msg.contains(pat), "{}", msg);
                test_helper::eprintln_nocapture!("  {} panicked as expected", function_name);
            }
        }
    }
}
// Catches unwinding panic on architectures with non-sequentially consistent memory models.
#[allow(dead_code)]
pub(crate) fn catch_unwind_on_non_seqcst_arch(pat: &str, function_name: &str, f: impl Fn()) {
    if !is_panic_abort() {
        // This could be Err on architectures with non-sequentially consistent memory models.
        // However, this does not necessarily mean that it will always be panic,
        // and implementing it with stronger orderings is also okay.
        match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
            Ok(()) => {
                // panic!();
            }
            Err(msg) => {
                let msg = msg
                    .downcast_ref::<std::string::String>()
                    .cloned()
                    .unwrap_or_else(|| msg.downcast_ref::<&'static str>().copied().unwrap().into());
                assert!(msg.contains(pat), "{}", msg);
                test_helper::eprintln_nocapture!("  {} panicked as expected", function_name);
            }
        }
    }
}
macro_rules! __call_stress_test_fn {
    ($name:ident, $atomic_type:ident,
        $int_type:ident, load, cas, $load_order:ident, $store_order:ident
    ) => {
        $name(
            $int_type::MIN.try_into().unwrap_or(isize::MIN),
            $int_type::MAX.try_into().unwrap_or(isize::MAX),
            |v| <$atomic_type>::new(v as $int_type),
            |a| a.load(Ordering::$load_order) as isize,
            |a, v| {
                a.fetch_update(Ordering::$store_order, Ordering::Relaxed, |_| Some(v as $int_type))
                    .unwrap();
            },
        )
    };
    ($name:ident, $atomic_type:ident,
        $int_type:ident, load, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        $name(
            $int_type::MIN.try_into().unwrap_or(isize::MIN),
            $int_type::MAX.try_into().unwrap_or(isize::MAX),
            |v| <$atomic_type>::new(v as $int_type),
            |a| a.load(Ordering::$load_order) as isize,
            |a, v| {
                a.$write(v as $int_type, Ordering::$store_order);
            },
        )
    };
    ($name:ident, $atomic_type:ident,
        $int_type:ident, cas, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        $name(
            $int_type::MIN.try_into().unwrap_or(isize::MIN),
            $int_type::MAX.try_into().unwrap_or(isize::MAX),
            |v| <$atomic_type>::new(v as $int_type),
            |a| {
                a.compare_exchange(0, 0, Ordering::$load_order, match Ordering::$load_order {
                    Ordering::Release => Ordering::Relaxed,
                    Ordering::AcqRel => Ordering::Acquire,
                    _ => Ordering::$load_order,
                })
                .unwrap_or_else(|x| x) as isize
            },
            |a, v| {
                a.$write(v as $int_type, Ordering::$store_order);
            },
        )
    };
    ($name:ident, $atomic_type:ident,
        $int_type:ident, cas_weak, $write:ident, $load_order:ident, $store_order:ident
    ) => {
        $name(
            $int_type::MIN.try_into().unwrap_or(isize::MIN),
            $int_type::MAX.try_into().unwrap_or(isize::MAX),
            |v| <$atomic_type>::new(v as $int_type),
            |a| {
                a.compare_exchange_weak(0, 0, Ordering::$load_order, match Ordering::$load_order {
                    Ordering::Release => Ordering::Relaxed,
                    Ordering::AcqRel => Ordering::Acquire,
                    _ => Ordering::$load_order,
                })
                .unwrap_or_else(|x| x) as isize
            },
            |a, v| {
                a.$write(v as $int_type, Ordering::$store_order);
            },
        )
    };
}
macro_rules! stress_test_load_store {
    ($int_type:ident) => {
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<stress_load_store_ $int_type>] {
                use std::{convert::TryInto as _, sync::atomic::Ordering};

                use crate::tests::helper::*;
                use super::*;

                __stress_acquire_release!(can_panic, $int_type, load, store, Relaxed, Relaxed);
                __stress_acquire_release!(can_panic, $int_type, load, store, Relaxed, Release);
                __stress_acquire_release!(can_panic, $int_type, load, store, Relaxed, SeqCst);
                __stress_acquire_release!(can_panic, $int_type, load, store, Acquire, Relaxed);
                __stress_acquire_release!(should_pass, $int_type, load, store, Acquire, Release);
                __stress_acquire_release!(should_pass, $int_type, load, store, Acquire, SeqCst);
                __stress_acquire_release!(can_panic, $int_type, load, store, SeqCst, Relaxed);
                __stress_acquire_release!(should_pass, $int_type, load, store, SeqCst, Release);
                __stress_acquire_release!(should_pass, $int_type, load, store, SeqCst, SeqCst);

                __stress_seqcst!(can_panic, $int_type, load, store, Relaxed, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, store, Relaxed, Release);
                __stress_seqcst!(can_panic, $int_type, load, store, Relaxed, SeqCst);
                __stress_seqcst!(can_panic, $int_type, load, store, Acquire, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, store, Acquire, Release);
                __stress_seqcst!(can_panic, $int_type, load, store, Acquire, SeqCst);
                __stress_seqcst!(can_panic, $int_type, load, store, SeqCst, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, store, SeqCst, Release);
                __stress_seqcst!(should_pass, $int_type, load, store, SeqCst, SeqCst);
            }
        }
    };
}
macro_rules! stress_test {
    ($int_type:ident) => {
        stress_test_load_store!($int_type);
        paste::paste! {
            #[allow(
                clippy::alloc_instead_of_core,
                clippy::std_instead_of_alloc,
                clippy::std_instead_of_core,
                clippy::undocumented_unsafe_blocks
            )]
            mod [<stress_swap_ $int_type>] {
                use std::{convert::TryInto as _, sync::atomic::Ordering};

                use crate::tests::helper::*;
                use super::*;

                __stress_acquire_release!(can_panic, $int_type, load, swap, Relaxed, Relaxed);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Relaxed, Acquire);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Relaxed, Release);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Relaxed, AcqRel);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Relaxed, SeqCst);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Acquire, Relaxed);
                __stress_acquire_release!(can_panic, $int_type, load, swap, Acquire, Acquire);
                __stress_acquire_release!(should_pass, $int_type, load, swap, Acquire, Release);
                __stress_acquire_release!(should_pass, $int_type, load, swap, Acquire, AcqRel);
                __stress_acquire_release!(should_pass, $int_type, load, swap, Acquire, SeqCst);
                __stress_acquire_release!(can_panic, $int_type, load, swap, SeqCst, Relaxed);
                __stress_acquire_release!(can_panic, $int_type, load, swap, SeqCst, Acquire);
                __stress_acquire_release!(should_pass, $int_type, load, swap, SeqCst, Release);
                __stress_acquire_release!(should_pass, $int_type, load, swap, SeqCst, AcqRel);
                __stress_acquire_release!(should_pass, $int_type, load, swap, SeqCst, SeqCst);

                __stress_seqcst!(can_panic, $int_type, load, swap, Relaxed, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, swap, Relaxed, Acquire);
                __stress_seqcst!(can_panic, $int_type, load, swap, Relaxed, Release);
                __stress_seqcst!(can_panic, $int_type, load, swap, Relaxed, AcqRel);
                __stress_seqcst!(can_panic, $int_type, load, swap, Relaxed, SeqCst);
                __stress_seqcst!(can_panic, $int_type, load, swap, Acquire, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, swap, Acquire, Acquire);
                __stress_seqcst!(can_panic, $int_type, load, swap, Acquire, Release);
                __stress_seqcst!(can_panic, $int_type, load, swap, Acquire, AcqRel);
                __stress_seqcst!(can_panic, $int_type, load, swap, Acquire, SeqCst);
                __stress_seqcst!(can_panic, $int_type, load, swap, SeqCst, Relaxed);
                __stress_seqcst!(can_panic, $int_type, load, swap, SeqCst, Acquire);
                __stress_seqcst!(can_panic, $int_type, load, swap, SeqCst, Release);
                __stress_seqcst!(can_panic, $int_type, load, swap, SeqCst, AcqRel);
                __stress_seqcst!(should_pass, $int_type, load, swap, SeqCst, SeqCst);
            }
        }
        // TODO: cas
    };
}

#[cfg(feature = "float")]
pub(crate) mod float_rand {
    #[cfg(portable_atomic_unstable_f16)]
    pub(crate) fn f16(rng: &mut fastrand::Rng) -> f16 {
        f16::from_bits(rng.u16(..))
    }
    pub(crate) fn f32(rng: &mut fastrand::Rng) -> f32 {
        f32::from_bits(rng.u32(..))
    }
    #[cfg(not(all(not(debug_assertions), target_arch = "x86", not(target_feature = "sse2"))))]
    pub(crate) fn f64(rng: &mut fastrand::Rng) -> f64 {
        f64::from_bits(rng.u64(..))
    }
    #[cfg(portable_atomic_unstable_f128)]
    pub(crate) fn f128(rng: &mut fastrand::Rng) -> f128 {
        f128::from_bits(rng.u128(..))
    }
}
