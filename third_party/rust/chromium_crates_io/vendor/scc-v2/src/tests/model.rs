#[cfg(feature = "loom")]
#[cfg(test)]
mod test_model {
    use crate::TreeIndex;
    use loom::model::Builder;
    use loom::thread::{spawn, yield_now};
    use sdd::Guard;
    use std::borrow::Borrow;
    use std::sync::atomic::AtomicUsize;
    use std::sync::atomic::Ordering::Relaxed;
    use std::sync::{Arc, Mutex};

    #[derive(Debug)]
    struct A(usize, Arc<AtomicUsize>);
    impl A {
        fn new(d: usize, c: Arc<AtomicUsize>) -> Self {
            c.fetch_add(1, Relaxed);
            Self(d, c)
        }
    }
    impl Clone for A {
        fn clone(&self) -> Self {
            self.1.fetch_add(1, Relaxed);
            Self(self.0, self.1.clone())
        }
    }
    impl Drop for A {
        fn drop(&mut self) {
            self.1.fetch_sub(1, Relaxed);
        }
    }

    static SERIALIZER: Mutex<()> = Mutex::new(());

    // Checks if keys are visible while the leaf node is being split.
    #[test]
    fn tree_index_split_leaf_node() {
        let _guard = SERIALIZER.lock().unwrap();

        let keys = 14;
        let key_to_remove = 0;
        let mut model_builder_leaf_node = Builder::new();
        model_builder_leaf_node.max_branches = 1_048_576;
        model_builder_leaf_node.check(move || {
            let cnt = Arc::new(AtomicUsize::new(0));
            let tree_index = Arc::new(TreeIndex::<usize, A>::default());

            for k in 0..keys {
                assert!(tree_index.insert(k, A::new(k, cnt.clone())).is_ok());
            }

            let cnt_clone = cnt.clone();
            let tree_index_clone = tree_index.clone();
            let thread_insert = spawn(move || {
                assert!(tree_index_clone
                    .insert(keys, A::new(keys, cnt_clone))
                    .is_ok());
            });

            let thread_remove = spawn(move || {
                let key: usize = key_to_remove;
                assert_eq!(
                    tree_index
                        .peek_with(key.borrow(), |_key, value| value.0)
                        .unwrap(),
                    key
                );
                assert!(tree_index.remove(key.borrow()));
                assert!(tree_index
                    .peek_with(key.borrow(), |_key, value| value.0)
                    .is_none());
            });

            assert!(thread_insert.join().is_ok());
            assert!(thread_remove.join().is_ok());

            while cnt.load(Relaxed) != 0 {
                Guard::new().accelerate();
                yield_now();
            }
        });
    }

    // Checks if keys are visible while the internal node is being split.
    #[test]
    fn tree_index_split_internal_node() {
        let _guard = SERIALIZER.lock().unwrap();

        let keys = 365;
        let key_to_remove = 0;
        let mut model_builder_new_internal_node = Builder::new();
        model_builder_new_internal_node.max_branches = 1_048_576 * 16;
        model_builder_new_internal_node.check(move || {
            let cnt = Arc::new(AtomicUsize::new(0));
            let tree_index = Arc::new(TreeIndex::<usize, A>::default());

            for k in 0..keys {
                assert!(tree_index.insert(k, A::new(k, cnt.clone())).is_ok());
            }

            let cnt_clone = cnt.clone();
            let tree_index_clone = tree_index.clone();
            let thread_insert = spawn(move || {
                assert!(tree_index_clone
                    .insert(keys, A::new(keys, cnt_clone))
                    .is_ok());
            });

            let thread_remove = spawn(move || {
                let key: usize = key_to_remove;
                assert_eq!(
                    tree_index
                        .peek_with(key.borrow(), |_key, value| value.0)
                        .unwrap(),
                    key
                );
                assert!(tree_index.remove(key.borrow()));
            });

            assert!(thread_insert.join().is_ok());
            assert!(thread_remove.join().is_ok());

            while cnt.load(Relaxed) != 0 {
                Guard::new().accelerate();
                yield_now();
            }
        });
    }

    // Checks if keys are visible while a leaf node is being removed.
    #[test]
    fn tree_index_remove_leaf_node() {
        let _guard = SERIALIZER.lock().unwrap();

        let keys = 15;
        let key_to_remove = 14;
        let mut model_builder_remove_leaf = Builder::new();
        model_builder_remove_leaf.max_branches = 1_048_576 * 16;
        model_builder_remove_leaf.check(move || {
            let cnt = Arc::new(AtomicUsize::new(0));
            let tree_index = Arc::new(TreeIndex::<usize, A>::default());

            for k in 0..keys {
                assert!(tree_index.insert(k, A::new(k, cnt.clone())).is_ok());
            }

            for k in 0..keys - 3 {
                assert!(tree_index.remove(k.borrow()));
            }

            let tree_index_clone = tree_index.clone();
            let thread_remove = spawn(move || {
                let key_to_remove = keys - 2;
                assert!(tree_index_clone.remove(key_to_remove.borrow()));
            });

            let thread_read = spawn(move || {
                let key = key_to_remove;
                assert_eq!(
                    tree_index.peek_with(&key, |_key, value| value.0).unwrap(),
                    key
                );
            });

            assert!(thread_remove.join().is_ok());
            assert!(thread_read.join().is_ok());

            while cnt.load(Relaxed) != 0 {
                Guard::new().accelerate();
                yield_now();
            }
        });
    }

    // Check if keys are visible while a node is being deallocated.
    #[test]
    fn tree_index_remove_internal_node() {
        let _guard = SERIALIZER.lock().unwrap();

        let keys = 366;
        let key_to_remove = 338;
        let mut model_builder_remove_node = Builder::new();
        model_builder_remove_node.max_branches = 1_048_576 * 16;
        model_builder_remove_node.check(move || {
            let cnt = Arc::new(AtomicUsize::new(0));
            let tree_index = Arc::new(TreeIndex::<usize, A>::default());

            for k in 0..keys {
                assert!(tree_index.insert(k, A::new(k, cnt.clone())).is_ok());
            }

            for k in key_to_remove + 1..keys {
                assert!(tree_index.remove(&k));
            }

            let tree_index_clone = tree_index.clone();
            let thread_read = spawn(move || {
                assert_eq!(
                    tree_index_clone
                        .peek_with(&0, |_key, value| value.0)
                        .unwrap(),
                    0
                );
            });

            let thread_remove = spawn(move || {
                assert!(tree_index.remove(key_to_remove.borrow()));
            });

            assert!(thread_read.join().is_ok());
            assert!(thread_remove.join().is_ok());

            while cnt.load(Relaxed) != 0 {
                Guard::new().accelerate();
                yield_now();
            }
        });
    }
}
