#[cfg(feature = "loom")]
#[cfg(test)]
mod test_model {
    use crate::{suspend, AtomicOwned, AtomicShared, Guard};
    use loom::sync::atomic::AtomicUsize;
    use loom::thread::{spawn, yield_now};
    use std::sync::atomic::Ordering::Relaxed;
    use std::sync::{Arc, Mutex};

    struct A(String, Arc<AtomicUsize>);
    impl Drop for A {
        fn drop(&mut self) {
            self.1.fetch_add(1, Relaxed);
        }
    }

    static SERIALIZER: Mutex<()> = Mutex::new(());

    #[test]
    fn ebr_owned() {
        let _guard = SERIALIZER.lock().unwrap();
        loom::model(|| {
            let str = "HOW ARE YOU HOW ARE YOU";
            let drop_count = Arc::new(AtomicUsize::new(0));
            let data_owned = AtomicOwned::new(A(str.to_string(), drop_count.clone()));

            let guard = Guard::new();
            let ptr = data_owned.load(Relaxed, &guard);

            let thread = spawn(move || {
                let guard = Guard::new();
                let ptr = data_owned.load(Relaxed, &guard);
                drop(data_owned);

                assert_eq!(ptr.as_ref().unwrap().0, str);
                guard.accelerate();
                drop(guard);

                assert!(suspend());
            });

            assert_eq!(ptr.as_ref().unwrap().0, str);
            guard.accelerate();
            drop(guard);

            while drop_count.load(Relaxed) != 1 {
                Guard::new().accelerate();
                yield_now();
            }

            assert!(thread.join().is_ok());
            assert_eq!(drop_count.load(Relaxed), 1);
        });
    }

    #[test]
    fn ebr_shared() {
        let _guard = SERIALIZER.lock().unwrap();
        loom::model(|| {
            let str = "HOW ARE YOU HOW ARE YOU";
            let drop_count = Arc::new(AtomicUsize::new(0));
            let data_shared = AtomicShared::new(A(str.to_string(), drop_count.clone()));

            let guard = Guard::new();
            let ptr = data_shared.load(Relaxed, &guard);

            let thread = spawn(move || {
                let data_shared_clone = data_shared.get_shared(Relaxed, &Guard::new()).unwrap();
                drop(data_shared);

                assert_eq!(data_shared_clone.0, str);

                let guard = Guard::new();
                let ptr = data_shared_clone.get_guarded_ptr(&guard);
                drop(data_shared_clone);
                guard.accelerate();

                assert_eq!(ptr.as_ref().unwrap().0, str);
                drop(guard);

                assert!(suspend());
            });

            assert_eq!(ptr.as_ref().unwrap().0, str);
            guard.accelerate();
            drop(guard);

            while drop_count.load(Relaxed) != 1 {
                Guard::new().accelerate();
                yield_now();
            }

            assert!(thread.join().is_ok());
            assert_eq!(drop_count.load(Relaxed), 1);
        });
    }
}
