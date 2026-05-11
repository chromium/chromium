#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod hashmap_test {
    use crate::hash_map::{self, Entry, Reserve};
    use crate::{Equivalent, HashMap};
    use proptest::prelude::*;
    use proptest::strategy::ValueTree;
    use proptest::test_runner::TestRunner;
    use std::collections::BTreeSet;
    use std::hash::{Hash, Hasher};
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::Ordering::{Acquire, Relaxed, Release};
    use std::sync::atomic::{AtomicU64, AtomicUsize};
    use std::sync::{Arc, Barrier};
    use std::thread;
    use std::time::Duration;
    use tokio::sync::Barrier as AsyncBarrier;

    static_assertions::assert_not_impl_all!(HashMap<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_map::Entry<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(HashMap<String, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(Reserve<String, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(HashMap<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Reserve<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(hash_map::OccupiedEntry<String, String>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_map::OccupiedEntry<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(hash_map::VacantEntry<String, String>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_map::VacantEntry<String, *const String>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize);
    impl R {
        fn new(cnt: &'static AtomicUsize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            self.0.fetch_add(1, Relaxed);
            R(self.0)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    struct Data {
        data: usize,
        checker: Arc<AtomicUsize>,
    }

    impl Data {
        fn new(data: usize, checker: Arc<AtomicUsize>) -> Data {
            checker.fetch_add(1, Relaxed);
            Data { data, checker }
        }
    }

    impl Clone for Data {
        fn clone(&self) -> Self {
            Data::new(self.data, self.checker.clone())
        }
    }

    impl Drop for Data {
        fn drop(&mut self) {
            self.checker.fetch_sub(1, Relaxed);
        }
    }

    impl Eq for Data {}

    impl Hash for Data {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.data.hash(state);
        }
    }

    impl PartialEq for Data {
        fn eq(&self, other: &Self) -> bool {
            self.data == other.data
        }
    }

    #[derive(Debug, Eq, PartialEq)]
    struct EqTest(String, usize);

    impl Equivalent<EqTest> for str {
        fn equivalent(&self, key: &EqTest) -> bool {
            key.0.eq(self)
        }
    }

    impl Hash for EqTest {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.0.hash(state);
        }
    }

    #[test]
    fn equivalent() {
        let hashmap: HashMap<EqTest, usize> = HashMap::default();
        assert!(hashmap.insert(EqTest("HELLO".to_owned(), 1), 1).is_ok());
        assert!(!hashmap.contains("NO"));
        assert!(hashmap.contains("HELLO"));
    }

    #[test]
    fn insert_drop() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let hashmap: HashMap<usize, R> = HashMap::default();
        let workload_size = 256;
        for k in 0..workload_size {
            assert!(hashmap.insert(k, R::new(&INST_CNT)).is_ok());
        }
        assert_eq!(INST_CNT.load(Relaxed), workload_size);
        assert_eq!(hashmap.len(), workload_size);
        drop(hashmap);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn insert_drop_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let hashmap: HashMap<usize, R> = HashMap::default();
        let workload_size = 1024;
        for k in 0..workload_size {
            assert!(hashmap.insert_async(k, R::new(&INST_CNT)).await.is_ok());
        }
        assert_eq!(INST_CNT.load(Relaxed), workload_size);
        assert_eq!(hashmap.len(), workload_size);
        drop(hashmap);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn clear_sync() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let hashmap: HashMap<usize, R> = HashMap::default();
        let workload_size = 1_usize << 8;
        for _ in 0..2 {
            for k in 0..workload_size {
                assert!(hashmap.insert(k, R::new(&INST_CNT)).is_ok());
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size);
            assert_eq!(hashmap.len(), workload_size);
            hashmap.clear();
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[test]
    fn read_remove() {
        let hashmap = Arc::new(HashMap::<String, Vec<u8>>::new());
        let barrier = Arc::new(Barrier::new(2));

        hashmap.insert("first".into(), vec![123]).unwrap();

        let hashmap_clone = hashmap.clone();
        let barrier_clone = barrier.clone();
        let task = thread::spawn(move || {
            hashmap_clone.read("first", |_key, value| {
                {
                    let first_item = value.first();
                    assert_eq!(first_item.unwrap(), &123_u8);
                }
                barrier_clone.wait();
                thread::sleep(Duration::from_millis(16));
                {
                    let first_item = value.first();
                    assert_eq!(first_item.unwrap(), &123_u8);
                }
            });
        });

        barrier.wait();
        assert!(hashmap.remove("first").is_some());
        assert!(task.join().is_ok());
    }

    #[test]
    fn from_iter() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let workload_size = 256;
        let hashmap = (0..workload_size)
            .map(|k| (k / 2, R::new(&INST_CNT)))
            .collect::<HashMap<usize, R>>();
        assert_eq!(hashmap.len(), workload_size / 2);
        hashmap.clear();
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn clear_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let hashmap: HashMap<usize, R> = HashMap::default();
        let workload_size = 1_usize << 18;
        for _ in 0..2 {
            for k in 0..workload_size {
                assert!(hashmap.insert_async(k, R::new(&INST_CNT)).await.is_ok());
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size);
            assert_eq!(hashmap.len(), workload_size);
            hashmap.clear_async().await;
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    #[test]
    fn clone() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let hashmap: HashMap<usize, R> = HashMap::default();
        let workload_size = 256;
        for k in 0..workload_size {
            assert!(hashmap.insert(k, R::new(&INST_CNT)).is_ok());
        }
        let hashmap_clone = hashmap.clone();
        hashmap.clear();
        for k in 0..workload_size {
            assert!(hashmap_clone.read(&k, |_, _| ()).is_some());
        }
        hashmap_clone.clear();
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn compare() {
        let hashmap1: HashMap<String, usize> = HashMap::new();
        let hashmap2: HashMap<String, usize> = HashMap::new();
        assert_eq!(hashmap1, hashmap2);

        assert!(hashmap1.insert("Hi".to_string(), 1).is_ok());
        assert_ne!(hashmap1, hashmap2);

        assert!(hashmap2.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(hashmap1, hashmap2);

        assert!(hashmap1.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(hashmap1, hashmap2);

        assert!(hashmap2.insert("Hi".to_string(), 1).is_ok());
        assert_eq!(hashmap1, hashmap2);

        assert!(hashmap1.remove("Hi").is_some());
        assert_ne!(hashmap1, hashmap2);
    }

    #[test]
    fn local_ref() {
        struct L<'a>(&'a AtomicUsize);
        impl<'a> L<'a> {
            fn new(cnt: &'a AtomicUsize) -> Self {
                cnt.fetch_add(1, Relaxed);
                L(cnt)
            }
        }
        impl Drop for L<'_> {
            fn drop(&mut self) {
                self.0.fetch_sub(1, Relaxed);
            }
        }

        let workload_size = 256;
        let cnt = AtomicUsize::new(0);
        let hashmap: HashMap<usize, L> = HashMap::default();

        for k in 0..workload_size {
            assert!(hashmap.insert(k, L::new(&cnt)).is_ok());
        }
        hashmap.retain(|k, _| {
            assert!(*k < workload_size);
            true
        });
        assert_eq!(cnt.load(Relaxed), workload_size);

        for k in 0..workload_size / 2 {
            assert!(hashmap.remove(&k).is_some());
        }
        hashmap.retain(|k, _| {
            assert!(*k >= workload_size / 2);
            true
        });
        assert_eq!(cnt.load(Relaxed), workload_size / 2);

        drop(hashmap);
        assert_eq!(cnt.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn integer_key() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());

        let num_tasks = 8;
        let workload_size = 256;
        let mut task_handles = Vec::with_capacity(num_tasks);
        let barrier = Arc::new(AsyncBarrier::new(num_tasks));
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let hashmap_clone = hashmap.clone();
            task_handles.push(tokio::task::spawn(async move {
                barrier_clone.wait().await;
                let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                for id in range.clone() {
                    let result = hashmap_clone.update_async(&id, |_, _| 1).await;
                    assert!(result.is_none());
                }
                for id in range.clone() {
                    if id % 10 == 0 {
                        hashmap_clone.entry_async(id).await.or_insert(id);
                    } else if id % 5 == 0 {
                        hashmap_clone.entry(id).or_insert(id);
                    } else if id % 2 == 0 {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    } else if id % 3 == 0 {
                        let entry = if id % 6 == 0 {
                            hashmap_clone.entry(id)
                        } else {
                            hashmap_clone.entry_async(id).await
                        };
                        let o = match entry {
                            Entry::Occupied(mut o) => {
                                *o.get_mut() = id;
                                o
                            }
                            Entry::Vacant(v) => v.insert_entry(id),
                        };
                        assert_eq!(*o.get(), id);
                    } else {
                        let result = hashmap_clone.insert(id, id);
                        assert!(result.is_ok());
                    }
                }
                for id in range.clone() {
                    if id % 7 == 4 {
                        let entry = hashmap_clone.entry(id);
                        match entry {
                            Entry::Occupied(mut o) => {
                                *o.get_mut() += 1;
                            }
                            Entry::Vacant(v) => {
                                v.insert_entry(id);
                            }
                        }
                    } else {
                        let result = hashmap_clone
                            .update_async(&id, |_, v| {
                                *v += 1;
                                *v
                            })
                            .await;
                        assert_eq!(result, Some(id + 1));
                    }
                }
                for id in range.clone() {
                    let result = hashmap_clone.read_async(&id, |_, v| *v).await;
                    assert_eq!(result, Some(id + 1));
                    let result = hashmap_clone.read(&id, |_, v| *v);
                    assert_eq!(result, Some(id + 1));
                    assert_eq!(*hashmap_clone.get(&id).unwrap().get(), id + 1);
                    assert_eq!(*hashmap_clone.get_async(&id).await.unwrap().get(), id + 1);
                }
                for id in range.clone() {
                    if id % 2 == 0 {
                        let result = hashmap_clone.remove_if_async(&id, |v| *v == id + 1).await;
                        assert_eq!(result, Some((id, id + 1)));
                    } else {
                        let result = hashmap_clone.remove_if(&id, |v| *v == id + 1);
                        assert_eq!(result, Some((id, id + 1)));
                    }
                    assert!(hashmap_clone.read_async(&id, |_, v| *v).await.is_none());
                    assert!(hashmap_clone.read(&id, |_, v| *v).is_none());
                    assert!(hashmap_clone.get(&id).is_none());
                    assert!(hashmap_clone.get_async(&id).await.is_none());
                }
                for id in range {
                    let result = hashmap_clone.remove_if_async(&id, |v| *v == id + 1).await;
                    assert_eq!(result, None);
                }
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }

        assert_eq!(hashmap.len(), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn insert_read_remove() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashmap_clone = hashmap.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    for id in range.clone() {
                        assert!(hashmap_clone.read_async(&id, |_, _| ()).await.is_some());
                        assert_eq!(*hashmap_clone.get_async(&id).await.unwrap().get(), id);
                    }
                    for id in range.clone() {
                        assert!(hashmap_clone.remove_async(&id).await.is_some());
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashmap.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn entry_next_retain() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashmap_clone = hashmap.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    for id in range.clone() {
                        assert!(hashmap_clone.read_async(&id, |_, _| ()).await.is_some());
                    }

                    let mut call_async = false;
                    let mut in_range = 0;
                    let mut entry = if task_id % 2 == 0 {
                        hashmap_clone.first_entry()
                    } else {
                        hashmap_clone.first_entry_async().await
                    };
                    while let Some(current_entry) = entry.take() {
                        if range.contains(current_entry.key()) {
                            in_range += 1;
                        }
                        if call_async {
                            entry = current_entry.next_async().await;
                        } else {
                            entry = current_entry.next();
                        }
                        call_async = !call_async;
                    }
                    assert!(in_range >= workload_size, "{in_range} {workload_size}");

                    let mut removed = 0;
                    hashmap_clone
                        .retain_async(|k, _| {
                            if range.contains(k) {
                                removed += 1;
                                false
                            } else {
                                true
                            }
                        })
                        .await;
                    assert_eq!(removed, workload_size);

                    let mut entry = if task_id % 2 == 0 {
                        hashmap_clone.first_entry()
                    } else {
                        hashmap_clone.first_entry_async().await
                    };
                    while let Some(current_entry) = entry.take() {
                        assert!(!range.contains(current_entry.key()));
                        if call_async {
                            entry = current_entry.next_async().await;
                        } else {
                            entry = current_entry.next();
                        }
                        call_async = !call_async;
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashmap.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn prune() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashmap_clone = hashmap.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    assert!(hashmap_clone.any(|k, _| range.contains(k)));
                    let mut removed = 0;
                    if task_id % 3 == 0 {
                        hashmap_clone.prune(|k, v| {
                            if range.contains(k) {
                                assert_eq!(*k, v);
                                removed += 1;
                                None
                            } else {
                                Some(v)
                            }
                        });
                    } else {
                        hashmap_clone
                            .prune_async(|k, v| {
                                if range.contains(k) {
                                    assert_eq!(*k, v);
                                    removed += 1;
                                    None
                                } else {
                                    Some(v)
                                }
                            })
                            .await;
                    }
                    assert_eq!(removed, workload_size);
                    assert!(!hashmap_clone.any_async(|k, _| range.contains(k)).await);
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashmap.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn retain_any() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashmap_clone = hashmap.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    for id in range.clone() {
                        let result = hashmap_clone.insert_async(id, id).await;
                        assert_eq!(result, Err((id, id)));
                    }
                    let mut iterated = 0;
                    hashmap_clone
                        .retain_async(|k, _| {
                            if range.contains(k) {
                                iterated += 1;
                            }
                            true
                        })
                        .await;
                    assert!(iterated >= workload_size);
                    assert!(hashmap_clone.any(|k, _| range.contains(k)));
                    assert!(hashmap_clone.any_async(|k, _| range.contains(k)).await);

                    let mut removed = 0;
                    if task_id % 3 == 0 {
                        hashmap_clone.retain(|k, _| {
                            if range.contains(k) {
                                removed += 1;
                                false
                            } else {
                                true
                            }
                        });
                    } else {
                        hashmap_clone
                            .retain_async(|k, _| {
                                if range.contains(k) {
                                    removed += 1;
                                    false
                                } else {
                                    true
                                }
                            })
                            .await;
                    }
                    assert_eq!(removed, workload_size);

                    assert!(!hashmap_clone.any(|k, _| range.contains(k)));
                    assert!(!hashmap_clone.any_async(|k, _| range.contains(k)).await);
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashmap.len(), 0);
        }
    }

    #[test]
    fn string_key() {
        let hashmap1: HashMap<String, u32> = HashMap::default();
        let hashmap2: HashMap<u32, String> = HashMap::default();
        let mut checker1 = BTreeSet::new();
        let mut checker2 = BTreeSet::new();
        let mut runner = TestRunner::default();
        let test_size = if cfg!(miri) { 16 } else { 4096 };
        for i in 0..test_size {
            let prop_str = "[a-z]{1,16}".new_tree(&mut runner).unwrap();
            let str_val = prop_str.current();
            if hashmap1.insert(str_val.clone(), i).is_ok() {
                checker1.insert((str_val.clone(), i));
            }
            let str_borrowed = str_val.as_str();
            assert!(hashmap1.contains(str_borrowed));
            assert!(hashmap1.read(str_borrowed, |_, _| ()).is_some());

            if hashmap2.insert(i, str_val.clone()).is_ok() {
                checker2.insert((i, str_val.clone()));
            }
        }
        assert_eq!(hashmap1.len(), checker1.len());
        assert_eq!(hashmap2.len(), checker2.len());
        for iter in checker1 {
            let v = hashmap1.remove(iter.0.as_str());
            assert_eq!(v.unwrap().1, iter.1);
        }
        for iter in checker2 {
            let e = hashmap2.entry(iter.0);
            match e {
                Entry::Occupied(o) => assert_eq!(o.remove(), iter.1),
                Entry::Vacant(_) => unreachable!(),
            }
        }
        assert_eq!(hashmap1.len(), 0);
        assert_eq!(hashmap2.len(), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 2)]
    async fn iterator() {
        let data_size = 4096;
        for _ in 0..16 {
            let hashmap: Arc<HashMap<u64, u64>> = Arc::new(HashMap::default());
            let hashmap_clone = hashmap.clone();
            let barrier = Arc::new(AsyncBarrier::new(2));
            let barrier_clone = barrier.clone();
            let inserted = Arc::new(AtomicU64::new(0));
            let inserted_clone = inserted.clone();
            let removed = Arc::new(AtomicU64::new(data_size));
            let removed_clone = removed.clone();
            let task_handle = tokio::task::spawn(async move {
                // test insert
                barrier_clone.wait().await;
                let mut scanned = 0;
                let mut checker = BTreeSet::new();
                let mut max = inserted_clone.load(Acquire);
                hashmap_clone.retain(|k, _| {
                    scanned += 1;
                    checker.insert(*k);
                    true
                });
                for key in 0..max {
                    assert!(checker.contains(&key));
                }

                barrier_clone.wait().await;
                scanned = 0;
                checker = BTreeSet::new();
                max = inserted_clone.load(Acquire);
                hashmap_clone
                    .retain_async(|k, _| {
                        scanned += 1;
                        checker.insert(*k);
                        true
                    })
                    .await;
                for key in 0..max {
                    assert!(checker.contains(&key));
                }

                // test remove
                barrier_clone.wait().await;
                scanned = 0;
                max = removed_clone.load(Acquire);
                hashmap_clone.retain(|k, _| {
                    scanned += 1;
                    assert!(*k < max);
                    true
                });

                barrier_clone.wait().await;
                scanned = 0;
                max = removed_clone.load(Acquire);
                hashmap_clone
                    .retain_async(|k, _| {
                        scanned += 1;
                        assert!(*k < max);
                        true
                    })
                    .await;
            });

            // insert
            barrier.wait().await;
            for i in 0..data_size {
                if i == data_size / 2 {
                    barrier.wait().await;
                }
                assert!(hashmap.insert(i, i).is_ok());
                inserted.store(i, Release);
            }

            // remove
            barrier.wait().await;
            for i in (0..data_size).rev() {
                if i == data_size / 2 {
                    barrier.wait().await;
                }
                assert!(hashmap.remove(&i).is_some());
                removed.store(i, Release);
            }

            assert!(task_handle.await.is_ok());
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 4)]
    async fn read() {
        let hashmap: Arc<HashMap<usize, usize>> = Arc::new(HashMap::default());
        let num_tasks = 4;
        let workload_size = 1024 * 1024;

        for k in 0..num_tasks {
            assert!(hashmap.insert(k, k).is_ok());
        }

        let mut task_handles = Vec::with_capacity(num_tasks);
        let barrier = Arc::new(AsyncBarrier::new(num_tasks));
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let hashmap_clone = hashmap.clone();
            task_handles.push(tokio::task::spawn(async move {
                barrier_clone.wait().await;
                if task_id == 0 {
                    for k in num_tasks..workload_size {
                        assert!(hashmap_clone.insert(k, k).is_ok());
                    }
                    for k in num_tasks..workload_size {
                        assert!(hashmap_clone.remove(&k).is_some());
                    }
                } else {
                    for k in 0..num_tasks {
                        assert!(hashmap_clone.read(&k, |_, _| ()).is_some());
                    }
                }
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }
    }

    proptest! {
        #[cfg_attr(miri, ignore)]
        #[test]
        fn insert(key in 0_usize..16) {
            let range = 4096;
            let checker = Arc::new(AtomicUsize::new(0));
            let hashmap: HashMap<Data, Data> = HashMap::default();
            for d in key..(key + range) {
                assert!(hashmap.insert(Data::new(d, checker.clone()), Data::new(d, checker.clone())).is_ok());
                *hashmap.entry(Data::new(d, checker.clone())).or_insert(Data::new(d + 1, checker.clone())).get_mut() = Data::new(d + 2, checker.clone());
            }

            for d in (key + range)..(key + range + range) {
                assert!(hashmap.insert(Data::new(d, checker.clone()), Data::new(d, checker.clone())).is_ok());
                *hashmap.entry(Data::new(d, checker.clone())).or_insert(Data::new(d + 1, checker.clone())).get_mut() = Data::new(d + 2, checker.clone());
            }

            let mut removed = 0;
            hashmap.retain(|k, _| if k.data  >= key + range { removed += 1; false } else { true });
            assert_eq!(removed, range);

            assert_eq!(hashmap.len(), range);
            let mut found_keys = 0;
            hashmap.retain(|k, v| {
                assert!(k.data < key + range);
                assert!(v.data >= key);
                found_keys += 1;
                true
            });
            assert_eq!(found_keys, range);
            assert_eq!(checker.load(Relaxed), range * 2);
            for d in key..(key + range) {
                assert!(hashmap.contains(&Data::new(d, checker.clone())));
            }
            for d in key..(key + range) {
                assert!(hashmap.remove(&Data::new(d, checker.clone())).is_some());
            }
            assert_eq!(checker.load(Relaxed), 0);

            for d in key..(key + range) {
                assert!(hashmap.insert(Data::new(d, checker.clone()), Data::new(d, checker.clone())).is_ok());
                *hashmap.entry(Data::new(d, checker.clone())).or_insert(Data::new(d + 1, checker.clone())).get_mut() = Data::new(d + 2, checker.clone());
            }
            hashmap.clear();
            assert_eq!(checker.load(Relaxed), 0);

            for d in key..(key + range) {
                assert!(hashmap.insert(Data::new(d, checker.clone()), Data::new(d, checker.clone())).is_ok());
                *hashmap.entry(Data::new(d, checker.clone())).or_insert(Data::new(d + 1, checker.clone())).get_mut() = Data::new(d + 2, checker.clone());
            }
            assert_eq!(checker.load(Relaxed), range * 2);
            drop(hashmap);
            assert_eq!(checker.load(Relaxed), 0);
        }
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod hashindex_test {
    use crate::ebr::Guard;
    use crate::hash_index::{self, Iter};
    use crate::{Equivalent, HashIndex};
    use proptest::strategy::{Strategy, ValueTree};
    use proptest::test_runner::TestRunner;
    use std::collections::BTreeSet;
    use std::hash::{Hash, Hasher};
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::Ordering::{Acquire, Relaxed, Release};
    use std::sync::atomic::{fence, AtomicU64, AtomicUsize};
    use std::sync::Arc;
    use std::thread;
    use tokio::sync::Barrier as AsyncBarrier;

    static_assertions::assert_not_impl_all!(HashIndex<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_index::Entry<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(HashIndex<String, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(Iter<'static, 'static, String, String>: UnwindSafe);
    static_assertions::assert_not_impl_all!(HashIndex<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Iter<'static, 'static, String, *const String>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize);
    impl R {
        fn new(cnt: &'static AtomicUsize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            self.0.fetch_add(1, Relaxed);
            R(self.0)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    #[derive(Clone, Debug, Eq, PartialEq)]
    struct EqTest(String, usize);

    impl Equivalent<EqTest> for str {
        fn equivalent(&self, key: &EqTest) -> bool {
            key.0.eq(self)
        }
    }

    impl Hash for EqTest {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.0.hash(state);
        }
    }

    #[test]
    fn equivalent() {
        let hashindex: HashIndex<EqTest, usize> = HashIndex::default();
        assert!(hashindex.insert(EqTest("HELLO".to_owned(), 1), 1).is_ok());
        assert!(!hashindex.contains("NO"));
        assert!(hashindex.contains("HELLO"));
    }

    #[test]
    fn compare() {
        let hashindex1: HashIndex<String, usize> = HashIndex::new();
        let hashindex2: HashIndex<String, usize> = HashIndex::new();
        assert_eq!(hashindex1, hashindex2);

        assert!(hashindex1.insert("Hi".to_string(), 1).is_ok());
        assert_ne!(hashindex1, hashindex2);

        assert!(hashindex2.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(hashindex1, hashindex2);

        assert!(hashindex1.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(hashindex1, hashindex2);

        assert!(hashindex2.insert("Hi".to_string(), 1).is_ok());
        assert_eq!(hashindex1, hashindex2);

        assert!(hashindex1.remove("Hi"));
        assert_ne!(hashindex1, hashindex2);
    }

    #[test]
    fn clear_sync() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashindex: HashIndex<usize, R> = HashIndex::default();

        let workload_size = 1_usize << 8;

        for _ in 0..2 {
            for k in 0..workload_size {
                assert!(hashindex.insert(k, R::new(&INST_CNT)).is_ok());
            }
            assert!(INST_CNT.load(Relaxed) >= workload_size);
            assert_eq!(hashindex.len(), workload_size);
            hashindex.clear();
        }
        drop(hashindex);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn clear_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashindex: HashIndex<usize, R> = HashIndex::default();

        let workload_size = 1_usize << 18;

        for _ in 0..2 {
            for k in 0..workload_size {
                assert!(hashindex.insert_async(k, R::new(&INST_CNT)).await.is_ok());
            }
            assert!(INST_CNT.load(Relaxed) >= workload_size);
            assert_eq!(hashindex.len(), workload_size);
            hashindex.clear_async().await;
        }
        drop(hashindex);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            tokio::task::yield_now().await;
        }
    }

    #[test]
    fn from_iter() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let workload_size = 256;
        let hashindex = (0..workload_size)
            .map(|k| (k / 2, R::new(&INST_CNT)))
            .collect::<HashIndex<usize, R>>();
        assert_eq!(hashindex.len(), workload_size / 2);
        drop(hashindex);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[test]
    fn clone() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashindex: HashIndex<usize, R> = HashIndex::default();

        let workload_size = 256;

        for k in 0..workload_size {
            assert!(hashindex.insert(k, R::new(&INST_CNT)).is_ok());
        }
        let hashindex_clone = hashindex.clone();
        drop(hashindex);
        for k in 0..workload_size {
            assert!(hashindex_clone.peek_with(&k, |_, _| ()).is_some());
        }
        drop(hashindex_clone);

        while INST_CNT.load(Relaxed) != 0 {
            drop(Guard::new());
            thread::yield_now();
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn clone_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashindex: HashIndex<usize, R> = HashIndex::default();

        let workload_size = 1024;

        for k in 0..workload_size {
            assert!(hashindex.insert_async(k, R::new(&INST_CNT)).await.is_ok());
        }
        let hashindex_clone = hashindex.clone();
        drop(hashindex);
        for k in 0..workload_size {
            assert!(hashindex_clone.peek_with(&k, |_, _| ()).is_some());
        }
        drop(hashindex_clone);

        while INST_CNT.load(Relaxed) != 0 {
            drop(Guard::new());
            tokio::task::yield_now().await;
        }
    }

    #[test]
    fn string_key() {
        let hashindex1: HashIndex<String, u32> = HashIndex::default();
        let hashindex2: HashIndex<u32, String> = HashIndex::default();
        let mut checker1 = BTreeSet::new();
        let mut checker2 = BTreeSet::new();
        let mut runner = TestRunner::default();
        let test_size = if cfg!(miri) { 16 } else { 4096 };
        for i in 0..test_size {
            let prop_str = "[a-z]{1,16}".new_tree(&mut runner).unwrap();
            let str_val = prop_str.current();
            if hashindex1.insert(str_val.clone(), i).is_ok() {
                checker1.insert((str_val.clone(), i));
            }
            let str_borrowed = str_val.as_str();
            assert!(hashindex1.peek_with(str_borrowed, |_, _| ()).is_some());

            if hashindex2.insert(i, str_val.clone()).is_ok() {
                checker2.insert((i, str_val.clone()));
            }
        }
        assert_eq!(hashindex1.len(), checker1.len());
        assert_eq!(hashindex2.len(), checker2.len());
        for iter in checker1 {
            assert!(hashindex1.remove(iter.0.as_str()));
        }
        for iter in checker2 {
            assert!(hashindex2.remove(&iter.0));
        }
        assert_eq!(hashindex1.len(), 0);
        assert_eq!(hashindex2.len(), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 4)]
    async fn read() {
        let hashindex: Arc<HashIndex<usize, usize>> = Arc::new(HashIndex::default());
        let num_tasks = 4;
        let workload_size = 1024 * 1024;

        for k in 0..num_tasks {
            assert!(hashindex.insert(k, k).is_ok());
        }

        let mut task_handles = Vec::with_capacity(num_tasks);
        let barrier = Arc::new(AsyncBarrier::new(num_tasks));
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let hashindex_clone = hashindex.clone();
            task_handles.push(tokio::task::spawn(async move {
                barrier_clone.wait().await;
                if task_id == 0 {
                    for k in num_tasks..workload_size {
                        assert!(hashindex_clone.insert(k, k).is_ok());
                    }
                    for k in num_tasks..workload_size {
                        assert!(hashindex_clone.remove(&k));
                    }
                } else {
                    for k in 0..num_tasks {
                        assert!(hashindex_clone.peek_with(&k, |_, _| ()).is_some());
                    }
                }
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 4)]
    async fn drop_entries() {
        let hashindex: Arc<HashIndex<usize, String>> = Arc::new(HashIndex::default());
        let num_tasks = 4;
        let num_iter = 64;
        let workload_size = 256;

        let str = "HOW ARE YOU HOW ARE YOU";
        for k in 0..num_tasks * workload_size {
            assert!(hashindex.insert(k, str.to_string()).is_ok());
        }

        let mut task_handles = Vec::with_capacity(num_tasks);
        let barrier = Arc::new(AsyncBarrier::new(num_tasks));
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let hashindex_clone = hashindex.clone();
            task_handles.push(tokio::task::spawn(async move {
                barrier_clone.wait().await;
                let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                if task_id == 0 {
                    for _ in 0..num_iter {
                        let guard = Guard::new();
                        let v = hashindex_clone
                            .peek(&(task_id * workload_size), &guard)
                            .unwrap();
                        assert_eq!(str, v);
                        fence(Acquire);
                        for id in range.clone() {
                            assert!(hashindex_clone.remove(&id));
                            assert!(hashindex_clone.insert(id, str.to_string()).is_ok());
                        }
                        fence(Acquire);
                        assert_eq!(str, v);
                    }
                } else {
                    for _ in 0..num_iter {
                        for id in range.clone() {
                            assert!(hashindex_clone.remove_async(&id).await);
                            assert!(hashindex_clone
                                .insert_async(id, str.to_string())
                                .await
                                .is_ok());
                        }
                    }
                }
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }

        assert_eq!(hashindex.len(), num_tasks * workload_size);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 4)]
    async fn rebuild() {
        let hashindex: Arc<HashIndex<usize, usize>> = Arc::new(HashIndex::default());
        let num_tasks = 4;
        let num_iter = 64;
        let workload_size = 256;

        for k in 0..num_tasks * workload_size {
            assert!(hashindex.insert(k, k).is_ok());
        }

        let mut task_handles = Vec::with_capacity(num_tasks);
        let barrier = Arc::new(AsyncBarrier::new(num_tasks));
        for task_id in 0..num_tasks {
            let barrier_clone = barrier.clone();
            let hashindex_clone = hashindex.clone();
            task_handles.push(tokio::task::spawn(async move {
                barrier_clone.wait().await;
                let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                for _ in 0..num_iter {
                    for id in range.clone() {
                        assert!(hashindex_clone.remove_async(&id).await);
                        assert!(hashindex_clone.insert_async(id, id).await.is_ok());
                    }
                }
            }));
        }

        for r in futures::future::join_all(task_handles).await {
            assert!(r.is_ok());
        }

        assert_eq!(hashindex.len(), num_tasks * workload_size);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn entry_next_retain() {
        let hashindex: Arc<HashIndex<usize, usize>> = Arc::new(HashIndex::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashindex_clone = hashindex.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashindex_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    for id in range.clone() {
                        assert!(hashindex_clone.peek_with(&id, |_, _| ()).is_some());
                    }

                    let mut call_async = false;
                    let mut in_range = 0;
                    let mut entry = if task_id % 2 == 0 {
                        hashindex_clone.first_entry()
                    } else {
                        hashindex_clone.first_entry_async().await
                    };
                    while let Some(current_entry) = entry.take() {
                        if range.contains(current_entry.key()) {
                            in_range += 1;
                        }
                        if call_async {
                            entry = current_entry.next_async().await;
                        } else {
                            entry = current_entry.next();
                        }
                        call_async = !call_async;
                    }
                    assert!(in_range >= workload_size, "{in_range} {workload_size}");

                    let mut removed = 0;
                    hashindex_clone
                        .retain_async(|k, _| {
                            if range.contains(k) {
                                removed += 1;
                                false
                            } else {
                                true
                            }
                        })
                        .await;
                    assert_eq!(removed, workload_size);

                    let mut entry = if task_id % 2 == 0 {
                        hashindex_clone.first_entry()
                    } else {
                        hashindex_clone.first_entry_async().await
                    };
                    while let Some(current_entry) = entry.take() {
                        assert!(!range.contains(current_entry.key()));
                        if call_async {
                            entry = current_entry.next_async().await;
                        } else {
                            entry = current_entry.next();
                        }
                        call_async = !call_async;
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashindex.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 2)]
    async fn iterator() {
        let data_size = 4096;
        for _ in 0..16 {
            let hashindex: Arc<HashIndex<u64, u64>> = Arc::new(HashIndex::default());
            let hashindex_clone = hashindex.clone();
            let barrier = Arc::new(AsyncBarrier::new(2));
            let barrier_clone = barrier.clone();
            let inserted = Arc::new(AtomicU64::new(0));
            let inserted_clone = inserted.clone();
            let removed = Arc::new(AtomicU64::new(data_size));
            let removed_clone = removed.clone();
            let task_handle = tokio::task::spawn(async move {
                // test insert
                barrier_clone.wait().await;
                let mut scanned = 0;
                let mut checker = BTreeSet::new();
                let mut max = inserted_clone.load(Acquire);
                hashindex_clone.retain(|k, _| {
                    scanned += 1;
                    checker.insert(*k);
                    true
                });
                for key in 0..max {
                    assert!(checker.contains(&key));
                }

                barrier_clone.wait().await;
                scanned = 0;
                checker = BTreeSet::new();
                max = inserted_clone.load(Acquire);
                hashindex_clone
                    .retain_async(|k, _| {
                        scanned += 1;
                        checker.insert(*k);
                        true
                    })
                    .await;
                for key in 0..max {
                    assert!(checker.contains(&key));
                }

                // test remove
                barrier_clone.wait().await;
                scanned = 0;
                max = removed_clone.load(Acquire);
                hashindex_clone.retain(|k, _| {
                    scanned += 1;
                    assert!(*k < max);
                    true
                });

                barrier_clone.wait().await;
                scanned = 0;
                max = removed_clone.load(Acquire);
                hashindex_clone
                    .retain_async(|k, _| {
                        scanned += 1;
                        assert!(*k < max);
                        true
                    })
                    .await;
            });

            // insert
            barrier.wait().await;
            for i in 0..data_size {
                if i == data_size / 2 {
                    barrier.wait().await;
                }
                assert!(hashindex.insert(i, i).is_ok());
                inserted.store(i, Release);
            }

            // remove
            barrier.wait().await;
            for i in (0..data_size).rev() {
                if i == data_size / 2 {
                    barrier.wait().await;
                }
                assert!(hashindex.remove(&i));
                removed.store(i, Release);
            }

            assert!(task_handle.await.is_ok());
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn update() {
        let hashindex: Arc<HashIndex<usize, usize>> = Arc::new(HashIndex::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashindex_clone = hashindex.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashindex_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                        let entry = if id % 4 == 0 {
                            hashindex_clone.get(&id).unwrap()
                        } else {
                            hashindex_clone.get_async(&id).await.unwrap()
                        };
                        if *entry.get() != id {
                            entry.update(0);
                        }
                    }
                    for id in range.clone() {
                        hashindex_clone.peek_with(&id, |k, v| assert_eq!(k, v));
                        let entry = if id % 4 == 0 {
                            hashindex_clone.get(&id).unwrap()
                        } else {
                            hashindex_clone.get_async(&id).await.unwrap()
                        };
                        if *entry.get() == id {
                            entry.update(usize::MAX);
                        }
                    }
                    for id in range.clone() {
                        hashindex_clone.peek_with(&id, |_, v| assert_eq!(*v, usize::MAX));
                        let entry = if id % 4 == 0 {
                            hashindex_clone.get(&id).unwrap()
                        } else {
                            hashindex_clone.get_async(&id).await.unwrap()
                        };
                        entry.remove_entry();
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashindex.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn retain() {
        let hashindex: Arc<HashIndex<usize, usize>> = Arc::new(HashIndex::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashindex_clone = hashindex.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        let result = hashindex_clone.insert_async(id, id).await;
                        assert!(result.is_ok());
                    }
                    for id in range.clone() {
                        let result = hashindex_clone.insert_async(id, id).await;
                        assert_eq!(result, Err((id, id)));
                    }
                    let mut iterated = 0;
                    hashindex_clone.iter(&Guard::new()).for_each(|(k, _)| {
                        if range.contains(k) {
                            iterated += 1;
                        }
                    });
                    assert!(iterated >= workload_size);
                    assert!(hashindex_clone
                        .iter(&Guard::new())
                        .any(|(k, _)| range.contains(k)));

                    let mut removed = 0;
                    if task_id % 4 == 0 {
                        hashindex_clone.retain(|k, _| {
                            if range.contains(k) {
                                removed += 1;
                                false
                            } else {
                                true
                            }
                        });
                    } else {
                        hashindex_clone
                            .retain_async(|k, _| {
                                if range.contains(k) {
                                    removed += 1;
                                    false
                                } else {
                                    true
                                }
                            })
                            .await;
                    }
                    assert_eq!(removed, workload_size);
                    assert!(!hashindex_clone
                        .iter(&Guard::new())
                        .any(|(k, _)| range.contains(k)));
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashindex.len(), 0);
        }
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod hashset_test {
    use crate::{Equivalent, HashSet};
    use std::hash::{Hash, Hasher};
    use std::panic::UnwindSafe;
    use std::rc::Rc;

    static_assertions::assert_not_impl_all!(HashSet<Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(HashSet<String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(HashSet<*const String>: Send, Sync, UnwindSafe);

    #[derive(Debug, Eq, PartialEq)]
    struct EqTest(String, usize);

    impl Equivalent<EqTest> for str {
        fn equivalent(&self, key: &EqTest) -> bool {
            key.0.eq(self)
        }
    }

    impl Hash for EqTest {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.0.hash(state);
        }
    }

    #[test]
    fn equivalent() {
        let hashset: HashSet<EqTest> = HashSet::default();
        assert!(hashset.insert(EqTest("HELLO".to_owned(), 1)).is_ok());
        assert!(!hashset.contains("NO"));
        assert!(hashset.contains("HELLO"));
    }

    #[test]
    fn from_iter() {
        let workload_size = 256;
        let hashset = (0..workload_size)
            .map(|k| k / 2)
            .collect::<HashSet<usize>>();
        assert_eq!(hashset.len(), workload_size / 2);
    }

    #[test]
    fn compare() {
        let hashset1: HashSet<String> = HashSet::new();
        let hashset2: HashSet<String> = HashSet::new();
        assert_eq!(hashset1, hashset2);

        assert!(hashset1.insert("Hi".to_string()).is_ok());
        assert_ne!(hashset1, hashset2);

        assert!(hashset2.insert("Hello".to_string()).is_ok());
        assert_ne!(hashset1, hashset2);

        assert!(hashset1.insert("Hello".to_string()).is_ok());
        assert_ne!(hashset1, hashset2);

        assert!(hashset2.insert("Hi".to_string()).is_ok());
        assert_eq!(hashset1, hashset2);

        assert!(hashset1.remove("Hi").is_some());
        assert_ne!(hashset1, hashset2);
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod hashcache_test {
    use crate::hash_cache;
    use crate::{Equivalent, HashCache};
    use proptest::prelude::*;
    use std::hash::{Hash, Hasher};
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::AtomicUsize;
    use std::sync::atomic::Ordering::Relaxed;
    use std::sync::Arc;
    use tokio::sync::Barrier as AsyncBarrier;

    static_assertions::assert_not_impl_all!(HashCache<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_cache::Entry<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(HashCache<String, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(HashCache<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(hash_cache::OccupiedEntry<String, String>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_cache::OccupiedEntry<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(hash_cache::VacantEntry<String, String>: Send, Sync);
    static_assertions::assert_not_impl_all!(hash_cache::VacantEntry<String, *const String>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize);
    impl R {
        fn new(cnt: &'static AtomicUsize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            self.0.fetch_add(1, Relaxed);
            R(self.0)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    #[derive(Debug, Eq, PartialEq)]
    struct EqTest(String, usize);

    impl Equivalent<EqTest> for str {
        fn equivalent(&self, key: &EqTest) -> bool {
            key.0.eq(self)
        }
    }

    impl Hash for EqTest {
        fn hash<H: Hasher>(&self, state: &mut H) {
            self.0.hash(state);
        }
    }

    #[test]
    fn equivalent() {
        let hashcache: HashCache<EqTest, usize> = HashCache::default();
        assert!(hashcache.put(EqTest("HELLO".to_owned(), 1), 1).is_ok());
        assert!(!hashcache.contains("NO"));
        assert!(hashcache.contains("HELLO"));
    }

    #[test]
    fn put_drop() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashcache: HashCache<usize, R> = HashCache::default();

        let workload_size = 256;
        for k in 0..workload_size {
            assert!(hashcache.put(k, R::new(&INST_CNT)).is_ok());
        }
        assert!(INST_CNT.load(Relaxed) <= hashcache.capacity());
        drop(hashcache);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn put_drop_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashcache: HashCache<usize, R> = HashCache::default();

        let workload_size = 1024;
        for k in 0..workload_size {
            assert!(hashcache.put_async(k, R::new(&INST_CNT)).await.is_ok());
        }
        assert!(INST_CNT.load(Relaxed) <= hashcache.capacity());
        drop(hashcache);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn put_full_clear_put() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let capacity = 256;
        let hashcache: HashCache<usize, R> = HashCache::with_capacity(capacity, capacity);

        let mut max_key = 0;
        for k in 0..=capacity {
            if let Ok(Some(_)) = hashcache.put(k, R::new(&INST_CNT)) {
                max_key = k;
                break;
            }
        }

        hashcache.clear();
        for k in 0..=capacity {
            if let Ok(Some(_)) = hashcache.put(k, R::new(&INST_CNT)) {
                assert_eq!(max_key, k);
                break;
            }
        }

        assert!(INST_CNT.load(Relaxed) <= hashcache.capacity());
        drop(hashcache);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn put_full_clear_put_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let capacity = 256;
        let hashcache: HashCache<usize, R> = HashCache::with_capacity(capacity, capacity);

        let mut max_key = 0;
        for k in 0..=capacity {
            if let Ok(Some(_)) = hashcache.put_async(k, R::new(&INST_CNT)).await {
                max_key = k;
                break;
            }
        }

        for i in 0..4 {
            if i % 2 == 0 {
                hashcache.clear_async().await;
            } else {
                hashcache.clear();
            }
            for k in 0..=capacity {
                if let Ok(Some(_)) = hashcache.put_async(k, R::new(&INST_CNT)).await {
                    assert_eq!(max_key, k);
                    break;
                }
            }
        }

        assert!(INST_CNT.load(Relaxed) <= hashcache.capacity());
        drop(hashcache);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn put_retain_get() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let workload_size = 256;
        let retain_limit = 64;
        let hashcache: HashCache<usize, R> = HashCache::with_capacity(0, 256);

        for k in 0..workload_size {
            assert!(hashcache.put(k, R::new(&INST_CNT)).is_ok());
        }

        hashcache.retain(|k, _| *k <= retain_limit);
        for k in 0..workload_size {
            if hashcache.get(&k).is_some() {
                assert!(k <= retain_limit);
            }
        }
        for k in 0..workload_size {
            if hashcache.put(k, R::new(&INST_CNT)).is_err() {
                assert!(k <= retain_limit);
            }
        }

        hashcache.retain(|k, _| *k > retain_limit);
        for k in 0..workload_size {
            if hashcache.get(&k).is_some() {
                assert!(k > retain_limit);
            }
        }
        for k in 0..workload_size {
            if hashcache.put(k, R::new(&INST_CNT)).is_err() {
                assert!(k > retain_limit);
            }
        }

        assert!(INST_CNT.load(Relaxed) <= hashcache.capacity());
        drop(hashcache);

        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn sparse_cache() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashcache = HashCache::<usize, R>::with_capacity(64, 64);
        for s in 0..16 {
            for k in s * 4..s * 4 + 4 {
                assert!(hashcache.put(k, R::new(&INST_CNT)).is_ok());
            }
            hashcache.retain(|k, _| *k % 2 == 0);
            for k in s * 4..s * 4 + 4 {
                if hashcache.put(k, R::new(&INST_CNT)).is_err() {
                    assert!(k % 2 == 0);
                }
            }
            hashcache.clear();
        }

        drop(hashcache);

        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn put_get_remove() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let hashcache: Arc<HashCache<usize, R>> = Arc::new(HashCache::default());
        for _ in 0..256 {
            let num_tasks = 8;
            let workload_size = 256;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let hashcache_clone = hashcache.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        if id % 8 == 0 {
                            assert!(hashcache_clone.put(id, R::new(&INST_CNT)).is_ok());
                        } else if id % 4 == 0 {
                            hashcache_clone
                                .entry_async(id)
                                .await
                                .or_put(R::new(&INST_CNT));
                        } else {
                            assert!(hashcache_clone
                                .put_async(id, R::new(&INST_CNT))
                                .await
                                .is_ok());
                        }
                    }
                    let mut hit_count = 0;
                    for id in range.clone() {
                        let hit = if id % 8 == 0 {
                            hashcache_clone.get(&id).is_some()
                        } else {
                            hashcache_clone.get_async(&id).await.is_some()
                        };
                        if hit {
                            hit_count += 1;
                        }
                    }
                    assert!(hit_count <= *hashcache_clone.capacity_range().end());
                    let mut remove_count = 0;
                    for id in range.clone() {
                        let removed = if id % 8 == 0 {
                            hashcache_clone.remove(&id).is_some()
                        } else {
                            hashcache_clone.remove_async(&id).await.is_some()
                        };
                        if removed {
                            remove_count += 1;
                        }
                    }
                    assert!(remove_count <= hit_count);
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            assert_eq!(hashcache.len(), 0);
        }
        drop(hashcache);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 8)]
    async fn put_remove_maintain() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        for _ in 0..64 {
            let hashcache: Arc<HashCache<usize, R>> = Arc::new(HashCache::with_capacity(256, 1024));
            let num_tasks = 8;
            let workload_size = 2048;
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            let evicted = Arc::new(AtomicUsize::new(0));
            let removed = Arc::new(AtomicUsize::new(0));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let evicted_clone = evicted.clone();
                let removed_clone = removed.clone();
                let hashcache_clone = hashcache.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    let mut evicted = 0;
                    for key in range.clone() {
                        let result = if key % 2 == 0 {
                            hashcache_clone.put(key, R::new(&INST_CNT))
                        } else {
                            hashcache_clone.put_async(key, R::new(&INST_CNT)).await
                        };
                        match result {
                            Ok(Some(_)) => evicted += 1,
                            Ok(_) => (),
                            Err(_) => unreachable!(),
                        }
                    }
                    evicted_clone.fetch_add(evicted, Relaxed);
                    let mut removed = 0;
                    for key in range.clone() {
                        let result = if key % 2 == 0 {
                            hashcache_clone.remove(&key)
                        } else {
                            hashcache_clone.remove_async(&key).await
                        };
                        if result.is_some() {
                            removed += 1;
                        }
                    }
                    removed_clone.fetch_add(removed, Relaxed);
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
            assert_eq!(
                evicted.load(Relaxed) + removed.load(Relaxed),
                workload_size * num_tasks
            );
            assert_eq!(hashcache.len(), 0);
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    proptest! {
        #[cfg_attr(miri, ignore)]
        #[test]
        fn capacity(xs in 0_usize..256) {
            let hashcache: HashCache<usize, usize> = HashCache::with_capacity(0, 64);
            for k in 0..xs {
                assert!(hashcache.put(k, k).is_ok());
            }
            assert!(hashcache.capacity() <= 64);

            let hashcache: HashCache<usize, usize> = HashCache::with_capacity(xs, xs * 2);
            for k in 0..xs {
                assert!(hashcache.put(k, k).is_ok());
            }
            if xs == 0 {
                assert_eq!(hashcache.capacity_range(), 0..=64);
            } else {
                assert_eq!(hashcache.capacity_range(), xs.next_power_of_two().max(64)..=(xs * 2).next_power_of_two().max(64));
            }
         }
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod treeindex_test {
    use crate::ebr::Guard;
    use crate::tree_index::{Iter, Range};
    use crate::{Comparable, Equivalent, TreeIndex};
    use proptest::prelude::*;
    use proptest::strategy::ValueTree;
    use proptest::test_runner::TestRunner;
    use sdd::suspend;
    use std::borrow::Borrow;
    use std::cmp::Ordering;
    use std::collections::BTreeSet;
    use std::ops::RangeInclusive;
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::Ordering::{Acquire, Relaxed, Release};
    use std::sync::atomic::{AtomicBool, AtomicUsize};
    use std::sync::{Arc, Barrier};
    use std::thread;
    use tokio::sync::Barrier as AsyncBarrier;
    use tokio::task;

    static_assertions::assert_not_impl_all!(TreeIndex<Rc<String>, Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(TreeIndex<String, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(Iter<'static, 'static, String, String>: UnwindSafe);
    static_assertions::assert_impl_all!(Range<'static, 'static, String, String, String, RangeInclusive<String>>: UnwindSafe);
    static_assertions::assert_not_impl_all!(TreeIndex<String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Iter<'static, 'static, String, *const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Range<'static, 'static, String, *const String, String, RangeInclusive<String>>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize);
    impl R {
        fn new(cnt: &'static AtomicUsize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            self.0.fetch_add(1, Relaxed);
            R(self.0)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    #[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
    struct CmpTest(String, usize);

    impl Comparable<CmpTest> for &str {
        fn compare(&self, key: &CmpTest) -> Ordering {
            self.cmp(&key.0.borrow())
        }
    }

    impl Equivalent<CmpTest> for &str {
        fn equivalent(&self, key: &CmpTest) -> bool {
            key.0.eq(self)
        }
    }

    #[test]
    fn comparable() {
        let tree: TreeIndex<CmpTest, usize> = TreeIndex::default();

        assert!(tree.insert(CmpTest("A".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"A", |_, _| true).unwrap());
        assert!(tree.insert(CmpTest("B".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"B", |_, _| true).unwrap());
        assert!(tree.insert(CmpTest("C".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"C", |_, _| true).unwrap());

        assert!(tree.insert(CmpTest("Z".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"Z", |_, _| true).unwrap());
        assert!(tree.insert(CmpTest("Y".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"Y", |_, _| true).unwrap());
        assert!(tree.insert(CmpTest("X".to_owned(), 0), 0).is_ok());
        assert!(tree.peek_with(&"X", |_, _| true).unwrap());

        let guard = Guard::new();
        let mut range = tree.range("C".."Y", &guard);
        assert_eq!(range.next().unwrap().0 .0, "C");
        assert_eq!(range.next().unwrap().0 .0, "X");
        assert!(range.next().is_none());

        tree.remove_range("C".."Y");

        assert!(tree.peek_with(&"A", |_, _| true).unwrap());
        assert!(tree.peek_with(&"B", |_, _| true).unwrap());
        assert!(tree.peek_with(&"C", |_, _| true).is_none());
        assert!(tree.peek_with(&"X", |_, _| true).is_none());
        assert!(tree.peek_with(&"Y", |_, _| true).unwrap());
        assert!(tree.peek_with(&"Z", |_, _| true).unwrap());
    }

    #[test]
    fn insert_drop() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let tree: TreeIndex<usize, R> = TreeIndex::default();

        let workload_size = 256;
        for k in 0..workload_size {
            assert!(tree.insert(k, R::new(&INST_CNT)).is_ok());
        }
        assert!(INST_CNT.load(Relaxed) >= workload_size);
        assert_eq!(tree.len(), workload_size);
        drop(tree);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test]
    async fn insert_drop_async() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let tree: TreeIndex<usize, R> = TreeIndex::default();

        let workload_size = 1024;
        for k in 0..workload_size {
            assert!(tree.insert_async(k, R::new(&INST_CNT)).await.is_ok());
        }
        assert!(INST_CNT.load(Relaxed) >= workload_size);
        assert_eq!(tree.len(), workload_size);
        drop(tree);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            tokio::task::yield_now().await;
        }
    }

    #[test]
    fn insert_remove() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let tree: TreeIndex<usize, R> = TreeIndex::default();

        let workload_size = 256;
        for k in 0..workload_size {
            assert!(tree.insert(k, R::new(&INST_CNT)).is_ok());
        }
        assert!(INST_CNT.load(Relaxed) >= workload_size);
        assert_eq!(tree.len(), workload_size);
        for k in 0..workload_size {
            assert!(tree.remove(&k));
        }
        assert_eq!(tree.len(), 0);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[test]
    fn insert_remove_clear() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let num_threads = 3;
        let num_iter = if cfg!(miri) { 1 } else { 8 };
        let workload_size = if cfg!(miri) { 32 } else { 1024 };
        let tree: Arc<TreeIndex<usize, R>> = Arc::new(TreeIndex::default());
        let mut thread_handles = Vec::with_capacity(num_threads);
        let barrier = Arc::new(Barrier::new(num_threads));
        for task_id in 0..num_threads {
            let barrier_clone = barrier.clone();
            let tree_clone = tree.clone();
            thread_handles.push(thread::spawn(move || {
                for _ in 0..num_iter {
                    barrier_clone.wait();
                    match task_id {
                        0 => {
                            for k in 0..workload_size {
                                assert!(tree_clone.insert(k, R::new(&INST_CNT)).is_ok());
                            }
                        }
                        1 => {
                            for k in 0..workload_size / 8 {
                                tree_clone.remove(&(k * 4));
                            }
                        }
                        _ => {
                            for _ in 0..workload_size / 64 {
                                if tree_clone.len() >= workload_size / 4 {
                                    tree_clone.clear();
                                }
                            }
                        }
                    }
                    tree_clone.clear();
                    assert!(suspend());
                }
                drop(tree_clone);
            }));
        }

        for handle in thread_handles {
            handle.join().unwrap();
        }

        drop(tree);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[test]
    fn clear() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let tree: TreeIndex<usize, R> = TreeIndex::default();

        let workload_size = if cfg!(miri) { 256 } else { 1024 * 1024 };
        for k in 0..workload_size {
            assert!(tree.insert(k, R::new(&INST_CNT)).is_ok());
        }
        assert!(INST_CNT.load(Relaxed) >= workload_size);
        assert_eq!(tree.len(), workload_size);
        tree.clear();

        let mut cnt: usize = 0;
        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
            cnt += 1;
        }
        println!("{cnt}");
    }

    #[test]
    fn clone() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        let tree: TreeIndex<usize, R> = TreeIndex::default();

        let workload_size = 256;
        for k in 0..workload_size {
            assert!(tree.insert(k, R::new(&INST_CNT)).is_ok());
        }
        let tree_clone = tree.clone();
        tree.clear();
        for k in 0..workload_size {
            assert!(tree_clone.peek_with(&k, |_, _| ()).is_some());
        }
        tree_clone.clear();

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn integer_key() {
        let num_tasks = 8;
        let workload_size = 256;
        for _ in 0..256 {
            let tree: Arc<TreeIndex<usize, usize>> = Arc::new(TreeIndex::default());
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let tree_clone = tree.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let range = (task_id * workload_size)..((task_id + 1) * workload_size);
                    for id in range.clone() {
                        assert!(tree_clone.insert_async(id, id).await.is_ok());
                        assert!(tree_clone.insert_async(id, id).await.is_err());
                    }
                    for id in range.clone() {
                        let result = tree_clone.peek_with(&id, |_, v| *v);
                        assert_eq!(result, Some(id));
                    }
                    for id in range.clone() {
                        assert!(tree_clone.remove_if_async(&id, |v| *v == id).await);
                    }
                    for id in range {
                        assert!(!tree_clone.remove_if_async(&id, |v| *v == id).await);
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
            assert_eq!(tree.len(), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 2)]
    async fn remove_range() {
        let num_tasks = 2;
        let workload_size = 4096;
        for _ in 0..16 {
            let tree: Arc<TreeIndex<usize, usize>> = Arc::new(TreeIndex::default());
            let mut task_handles = Vec::with_capacity(num_tasks);
            let barrier = Arc::new(AsyncBarrier::new(num_tasks));
            let data = Arc::new(AtomicUsize::default());
            for task_id in 0..num_tasks {
                let barrier_clone = barrier.clone();
                let data_clone = data.clone();
                let tree_clone = tree.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    if task_id == 0 {
                        for k in 1..=workload_size {
                            assert!(tree_clone.insert(k, k).is_ok());
                            assert!(tree_clone.peek(&k, &Guard::new()).is_some());
                            data_clone.store(k, Release);
                        }
                    } else {
                        loop {
                            let end_bound = data_clone.load(Acquire);
                            if end_bound == workload_size {
                                break;
                            } else if end_bound <= 1 {
                                task::yield_now().await;
                                continue;
                            }
                            if end_bound % 2 == 0 {
                                for (k, _) in tree_clone.range(..end_bound, &Guard::new()) {
                                    tree_clone.remove(k);
                                }
                            } else if end_bound % 3 == 0 {
                                tree_clone.remove_range(..end_bound);
                            } else {
                                tree_clone.remove_range_async(..end_bound).await;
                            }
                            if end_bound % 5 == 0 {
                                for (k, v) in tree_clone.iter(&Guard::new()) {
                                    assert_eq!(k, v);
                                    assert!(!(..end_bound).contains(k), "{k}");
                                }
                            } else {
                                assert!(
                                    tree_clone.peek(&(end_bound - 1), &Guard::new()).is_none(),
                                    "{end_bound} {}",
                                    data_clone.load(Relaxed)
                                );
                                assert!(tree_clone.peek(&end_bound, &Guard::new()).is_some());
                            }
                        }
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }

            tree.remove_range(..workload_size);
            assert!(tree.peek(&(workload_size - 1), &Guard::new()).is_none());
            assert!(tree.peek(&workload_size, &Guard::new()).is_some());
            assert_eq!(tree.len(), 1);
            assert_eq!(tree.depth(), 1);
        }
    }

    #[test]
    fn reclaim() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        struct R(usize);
        impl R {
            fn new() -> R {
                R(INST_CNT.fetch_add(1, Relaxed))
            }
        }
        impl Clone for R {
            fn clone(&self) -> Self {
                INST_CNT.fetch_add(1, Relaxed);
                R(self.0)
            }
        }
        impl Drop for R {
            fn drop(&mut self) {
                INST_CNT.fetch_sub(1, Relaxed);
            }
        }

        let data_size = 256;
        let tree: TreeIndex<usize, R> = TreeIndex::new();
        for k in 0..data_size {
            assert!(tree.insert(k, R::new()).is_ok());
        }
        for k in (0..data_size).rev() {
            assert!(tree.remove(&k));
        }

        let mut cnt = 0;
        while INST_CNT.load(Relaxed) > 0 {
            Guard::new().accelerate();
            cnt += 1;
        }
        println!("{cnt}");
        assert!(cnt >= INST_CNT.load(Relaxed));

        let tree: TreeIndex<usize, R> = TreeIndex::new();
        for k in 0..(data_size / 16) {
            assert!(tree.insert(k, R::new()).is_ok());
        }
        tree.clear();

        while INST_CNT.load(Relaxed) > 0 {
            Guard::new().accelerate();
        }
    }

    #[test]
    fn mixed() {
        let range = if cfg!(miri) { 64 } else { 4096 };
        let num_threads = if cfg!(miri) { 2 } else { 16 };
        let tree: Arc<TreeIndex<usize, usize>> = Arc::new(TreeIndex::new());
        let barrier = Arc::new(Barrier::new(num_threads));
        let mut thread_handles = Vec::with_capacity(num_threads);
        for thread_id in 0..num_threads {
            let tree_clone = tree.clone();
            let barrier_clone = barrier.clone();
            thread_handles.push(thread::spawn(move || {
                let first_key = thread_id * range;
                barrier_clone.wait();
                for key in first_key..(first_key + range / 2) {
                    assert!(tree_clone.insert(key, key).is_ok());
                }
                for key in first_key..(first_key + range / 2) {
                    assert!(tree_clone
                        .peek_with(&key, |key, val| assert_eq!(key, val))
                        .is_some());
                }
                for key in (first_key + range / 2)..(first_key + range) {
                    assert!(tree_clone.insert(key, key).is_ok());
                }
                for key in (first_key + range / 2)..(first_key + range) {
                    assert!(tree_clone
                        .peek_with(&key, |key, val| assert_eq!(key, val))
                        .is_some());
                }
            }));
        }
        for handle in thread_handles {
            handle.join().unwrap();
        }
        let mut found = 0;
        for key in 0..num_threads * range {
            if tree
                .peek_with(&key, |key, val| assert_eq!(key, val))
                .is_some()
            {
                found += 1;
            }
        }
        assert_eq!(found, num_threads * range);
        for key in 0..num_threads * range {
            assert!(tree
                .peek_with(&key, |key, val| assert_eq!(key, val))
                .is_some());
        }

        let guard = Guard::new();
        let scanner = tree.iter(&guard);
        let mut prev = 0;
        for entry in scanner {
            assert!(prev == 0 || prev < *entry.0);
            assert_eq!(*entry.0, *entry.1);
            prev = *entry.0;
        }
    }

    #[test]
    fn compare() {
        let tree1: TreeIndex<String, usize> = TreeIndex::new();
        let tree2: TreeIndex<String, usize> = TreeIndex::new();
        assert_eq!(tree1, tree2);

        assert!(tree1.insert("Hi".to_string(), 1).is_ok());
        assert_ne!(tree1, tree2);

        assert!(tree2.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(tree1, tree2);

        assert!(tree1.insert("Hello".to_string(), 2).is_ok());
        assert_ne!(tree1, tree2);

        assert!(tree2.insert("Hi".to_string(), 1).is_ok());
        assert_eq!(tree1, tree2);

        assert!(tree1.remove("Hi"));
        assert_ne!(tree1, tree2);
    }

    #[test]
    fn complex() {
        let range = if cfg!(miri) { 4 } else { 4096 };
        let num_threads = if cfg!(miri) { 4 } else { 16 };
        let tree: Arc<TreeIndex<usize, usize>> = Arc::new(TreeIndex::new());
        for t in 0..num_threads {
            // insert markers
            assert!(tree.insert(t * range, t * range).is_ok());
        }
        let stopped: Arc<AtomicBool> = Arc::new(AtomicBool::new(false));
        let barrier = Arc::new(Barrier::new(num_threads + 1));
        let mut thread_handles = Vec::with_capacity(num_threads);
        for thread_id in 0..num_threads {
            let tree_clone = tree.clone();
            let stopped_clone = stopped.clone();
            let barrier_clone = barrier.clone();
            thread_handles.push(thread::spawn(move || {
                let first_key = thread_id * range;
                barrier_clone.wait();
                while !stopped_clone.load(Relaxed) {
                    for key in (first_key + 1)..(first_key + range) {
                        assert!(tree_clone.insert(key, key).is_ok());
                    }
                    for key in (first_key + 1)..(first_key + range) {
                        assert!(tree_clone
                            .peek_with(&key, |key, val| assert_eq!(key, val))
                            .is_some());
                    }
                    {
                        let guard = Guard::new();
                        let mut range_scanner = tree_clone.range(first_key.., &guard);
                        let mut entry = range_scanner.next().unwrap();
                        assert_eq!(entry, (&first_key, &first_key));
                        entry = range_scanner.next().unwrap();
                        assert_eq!(entry, (&(first_key + 1), &(first_key + 1)));
                        entry = range_scanner.next().unwrap();
                        assert_eq!(entry, (&(first_key + 2), &(first_key + 2)));
                        entry = range_scanner.next().unwrap();
                        assert_eq!(entry, (&(first_key + 3), &(first_key + 3)));
                    }

                    let key_at_halfway = first_key + range / 2;
                    for key in (first_key + 1)..(first_key + range) {
                        if key == key_at_halfway {
                            let guard = Guard::new();
                            let mut range_scanner = tree_clone.range((first_key + 1).., &guard);
                            let entry = range_scanner.next().unwrap();
                            assert_eq!(entry, (&key_at_halfway, &key_at_halfway));
                            let entry = range_scanner.next().unwrap();
                            assert_eq!(entry, (&(key_at_halfway + 1), &(key_at_halfway + 1)));
                        }
                        assert!(tree_clone.remove(&key));
                        assert!(!tree_clone.remove(&key));
                        assert!(tree_clone.peek_with(&(first_key + 1), |_, _| ()).is_none());
                        assert!(tree_clone.peek_with(&key, |_, _| ()).is_none());
                    }
                    for key in (first_key + 1)..(first_key + range) {
                        assert!(tree_clone
                            .peek_with(&key, |key, val| assert_eq!(key, val))
                            .is_none());
                    }
                }
            }));
        }
        barrier.wait();

        let iteration = if cfg!(miri) { 16 } else { 512 };
        for _ in 0..iteration {
            let mut found_0 = false;
            let mut found_markers = 0;
            let mut prev_marker = 0;
            let mut prev = 0;
            let guard = Guard::new();
            for iter in tree.iter(&guard) {
                let current = *iter.0;
                if current % range == 0 {
                    found_markers += 1;
                    if current == 0 {
                        found_0 = true;
                    }
                    if current > 0 {
                        assert_eq!(prev_marker + range, current);
                    }
                    prev_marker = current;
                }
                assert!(prev == 0 || prev < current);
                prev = current;
            }
            assert!(found_0);
            assert_eq!(found_markers, num_threads);
        }

        stopped.store(true, Release);
        for handle in thread_handles {
            handle.join().unwrap();
        }
    }

    #[test]
    fn remove() {
        let num_threads = if cfg!(miri) { 2 } else { 16 };
        let tree: Arc<TreeIndex<usize, usize>> = Arc::new(TreeIndex::new());
        let barrier = Arc::new(Barrier::new(num_threads));
        let mut thread_handles = Vec::with_capacity(num_threads);
        for thread_id in 0..num_threads {
            let tree_clone = tree.clone();
            let barrier_clone = barrier.clone();
            thread_handles.push(thread::spawn(move || {
                barrier_clone.wait();
                let data_size = if cfg!(miri) { 16 } else { 4096 };
                for _ in 0..data_size {
                    let range = 0..32;
                    let inserted = range
                        .clone()
                        .filter(|i| tree_clone.insert(*i, thread_id).is_ok())
                        .count();
                    let found = range
                        .clone()
                        .filter(|i| {
                            tree_clone
                                .peek_with(i, |_, v| *v == thread_id)
                                .map_or(false, |t| t)
                        })
                        .count();
                    let removed = range
                        .clone()
                        .filter(|i| tree_clone.remove_if(i, |v| *v == thread_id))
                        .count();
                    let removed_again = range
                        .clone()
                        .filter(|i| tree_clone.remove_if(i, |v| *v == thread_id))
                        .count();
                    assert_eq!(removed_again, 0);
                    assert_eq!(found, removed, "{inserted} {found} {removed}");
                    assert_eq!(inserted, found, "{inserted} {found} {removed}");
                }
            }));
        }
        for handle in thread_handles {
            handle.join().unwrap();
        }
        assert_eq!(tree.len(), 0);
        assert_eq!(tree.depth(), 0);
    }

    #[test]
    fn string_key() {
        let tree1: TreeIndex<String, u32> = TreeIndex::default();
        let tree2: TreeIndex<u32, String> = TreeIndex::default();
        let mut checker1 = BTreeSet::new();
        let mut checker2 = BTreeSet::new();
        let mut runner = TestRunner::default();
        let test_size = if cfg!(miri) { 16 } else { 1024 };
        for i in 0..test_size {
            let prop_str = "[a-z]{1,16}".new_tree(&mut runner).unwrap();
            let str_val = prop_str.current();
            if tree1.insert(str_val.clone(), i).is_ok() {
                checker1.insert((str_val.clone(), i));
            }
            let str_borrowed = str_val.as_str();
            assert!(tree1.peek_with(str_borrowed, |_, _| ()).is_some());

            if tree2.insert(i, str_val.clone()).is_ok() {
                checker2.insert((i, str_val.clone()));
            }
        }
        for iter in &checker1 {
            let v = tree1.peek_with(iter.0.as_str(), |_, v| *v);
            assert_eq!(v.unwrap(), iter.1);
        }
        for iter in &checker2 {
            let v = tree2.peek_with(&iter.0, |_, v| v.clone());
            assert_eq!(v.unwrap(), iter.1);
        }
    }

    #[test]
    fn scanner() {
        let data_size = if cfg!(miri) { 128 } else { 4096 };
        let iteration = if cfg!(miri) { 4 } else { 64 };
        for _ in 0..iteration {
            let tree: Arc<TreeIndex<usize, u64>> = Arc::new(TreeIndex::default());
            let barrier = Arc::new(Barrier::new(3));
            let inserted = Arc::new(AtomicUsize::new(0));
            let removed = Arc::new(AtomicUsize::new(data_size));
            let mut thread_handles = Vec::new();
            for _ in 0..2 {
                let tree_clone = tree.clone();
                let barrier_clone = barrier.clone();
                let inserted_clone = inserted.clone();
                let removed_clone = removed.clone();
                let thread_handle = thread::spawn(move || {
                    // test insert
                    for _ in 0..2 {
                        barrier_clone.wait();
                        let max = inserted_clone.load(Acquire);
                        let mut prev = 0;
                        let mut iterated = 0;
                        let guard = Guard::new();
                        for iter in tree_clone.iter(&guard) {
                            assert!(
                                prev == 0
                                    || (*iter.0 <= max && prev + 1 == *iter.0)
                                    || *iter.0 > prev
                            );
                            prev = *iter.0;
                            iterated += 1;
                        }
                        assert!(iterated >= max);
                    }
                    // test remove
                    for _ in 0..2 {
                        barrier_clone.wait();
                        let mut prev = 0;
                        let max = removed_clone.load(Acquire);
                        let guard = Guard::new();
                        for iter in tree_clone.iter(&guard) {
                            let current = *iter.0;
                            assert!(current < max);
                            assert!(prev + 1 == current || prev == 0);
                            prev = current;
                        }
                    }
                });
                thread_handles.push(thread_handle);
            }
            // insert
            barrier.wait();
            for i in 0..data_size {
                if i == data_size / 2 {
                    barrier.wait();
                }
                assert!(tree.insert(i, 0).is_ok());
                inserted.store(i, Release);
            }
            // remove
            barrier.wait();
            for i in (0..data_size).rev() {
                if i == data_size / 2 {
                    barrier.wait();
                }
                assert!(tree.remove(&i));
                removed.store(i, Release);
            }
            for t in thread_handles {
                t.join().unwrap();
            }
        }
    }

    #[test]
    fn range() {
        let tree: TreeIndex<String, usize> = TreeIndex::default();
        assert!(tree.insert("Ape".to_owned(), 0).is_ok());
        assert!(tree.insert("Apple".to_owned(), 1).is_ok());
        assert!(tree.insert("Banana".to_owned(), 3).is_ok());
        assert!(tree.insert("Badezimmer".to_owned(), 2).is_ok());
        assert_eq!(tree.range(..="Ball".to_owned(), &Guard::new()).count(), 3);
        assert_eq!(
            tree.range("Ape".to_owned()..="Ball".to_owned(), &Guard::new())
                .count(),
            3
        );
        assert_eq!(
            tree.range("Apex".to_owned()..="Ball".to_owned(), &Guard::new())
                .count(),
            2
        );
        assert_eq!(
            tree.range("Ace".to_owned()..="Ball".to_owned(), &Guard::new())
                .count(),
            3
        );
        assert_eq!(tree.range(..="Z".to_owned(), &Guard::new()).count(), 4);
        assert_eq!(
            tree.range("Ape".to_owned()..="Z".to_owned(), &Guard::new())
                .count(),
            4
        );
        assert_eq!(
            tree.range("Apex".to_owned()..="Z".to_owned(), &Guard::new())
                .count(),
            3
        );
        assert_eq!(
            tree.range("Ace".to_owned()..="Z".to_owned(), &Guard::new())
                .count(),
            4
        );
        assert_eq!(tree.range(.."Banana".to_owned(), &Guard::new()).count(), 3);
        assert_eq!(
            tree.range("Ape".to_owned().."Banana".to_owned(), &Guard::new())
                .count(),
            3
        );
        assert_eq!(
            tree.range("Apex".to_owned().."Banana".to_owned(), &Guard::new())
                .count(),
            2
        );
        assert_eq!(
            tree.range("Ace".to_owned().."Banana".to_owned(), &Guard::new())
                .count(),
            3
        );
    }

    proptest! {
        #[cfg_attr(miri, ignore)]
        #[test]
        fn prop_remove_range(lower in 0_usize..4096_usize, range in 0_usize..4096_usize) {
            let remove_range = lower..lower + range;
            let insert_range = (256_usize, 4095_usize);
            let tree = TreeIndex::default();
            for k in insert_range.0..=insert_range.1 {
                prop_assert!(tree.insert(k, k).is_ok());
            }
            if usize::BITS == 32 {
                prop_assert_eq!(tree.depth(), 4);
            } else {
                prop_assert_eq!(tree.depth(), 3);
            }
            tree.remove_range(remove_range.clone());
            if remove_range.contains(&insert_range.0) && remove_range.contains(&insert_range.1) {
                prop_assert!(tree.is_empty());
            }
            for (k, v) in tree.iter(&Guard::new()) {
                prop_assert_eq!(k, v);
                prop_assert!(!remove_range.contains(k), "{k}");
            }
            for k in 0_usize..4096_usize {
                if tree.peek_with(&k, |_, _|()).is_some() {
                    prop_assert!(!remove_range.contains(&k), "{k}");
                }
            }
            for k in remove_range.clone() {
                prop_assert!(tree.insert(k, k).is_ok());
            }
            let mut cnt = 0;
            for (k, v) in tree.iter(&Guard::new()) {
                prop_assert_eq!(k, v);
                if remove_range.contains(k) {
                    cnt += 1;
                }
            }
            assert_eq!(cnt, range);
        }
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod bag_test {
    use crate::bag::IterMut;
    use crate::Bag;
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::AtomicUsize;
    use std::sync::atomic::Ordering::Relaxed;
    use std::sync::Arc;
    use tokio::sync::Barrier as AsyncBarrier;
    use tokio::task;

    static_assertions::assert_not_impl_all!(Bag<Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(Bag<String>: Send, Sync, UnwindSafe);
    static_assertions::assert_impl_all!(IterMut<'static, String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Bag<*const String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(IterMut<'static, *const String>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize);
    impl R {
        fn new(cnt: &'static AtomicUsize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            self.0.fetch_add(1, Relaxed);
            R(self.0)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    #[test]
    fn reclaim() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        for workload_size in [2, 18, 32, 40, 120] {
            let mut bag: Bag<R> = Bag::default();
            for _ in 0..workload_size {
                bag.push(R::new(&INST_CNT));
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size);
            assert_eq!(bag.iter_mut().count(), workload_size);
            bag.iter_mut().for_each(|e| {
                *e = R::new(&INST_CNT);
            });

            for _ in 0..workload_size / 2 {
                bag.pop();
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size / 2);
            drop(bag);
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    #[test]
    fn from_iter() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let workload_size = 16;
        let bag = (0..workload_size)
            .map(|_| R::new(&INST_CNT))
            .collect::<Bag<R>>();
        assert_eq!(bag.len(), workload_size);
        drop(bag);
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }

    #[test]
    fn into_iter() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        for workload_size in [2, 18, 32, 40, 120] {
            let mut bag: Bag<R> = Bag::default();
            for _ in 0..workload_size {
                bag.push(R::new(&INST_CNT));
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size);
            assert_eq!(bag.len(), workload_size);
            assert_eq!(bag.iter_mut().count(), workload_size);

            for v in &mut bag {
                assert_eq!(v.0.load(Relaxed), INST_CNT.load(Relaxed));
            }
            assert_eq!(INST_CNT.load(Relaxed), workload_size);

            for v in bag {
                assert_eq!(v.0.load(Relaxed), INST_CNT.load(Relaxed));
            }
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 12)]
    async fn mpmc() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        const NUM_TASKS: usize = 6;
        for _ in 0..4 {
            let workload_size = 64;
            let bag_default: Arc<Bag<R>> = Arc::new(Bag::default());
            let bag_half: Arc<Bag<R, 15>> = Arc::new(Bag::new());
            for _ in 0..256 {
                let mut task_handles = Vec::with_capacity(NUM_TASKS);
                let barrier = Arc::new(AsyncBarrier::new(NUM_TASKS));
                for _ in 0..NUM_TASKS {
                    let barrier_clone = barrier.clone();
                    let bag32_clone = bag_default.clone();
                    let bag_half_clone = bag_half.clone();
                    task_handles.push(tokio::task::spawn(async move {
                        barrier_clone.wait().await;
                        for _ in 0..4 {
                            for _ in 0..workload_size {
                                bag32_clone.push(R::new(&INST_CNT));
                                bag_half_clone.push(R::new(&INST_CNT));
                            }
                            for _ in 0..workload_size {
                                while bag32_clone.pop().is_none() {
                                    crate::ebr::Guard::new().accelerate();
                                    task::yield_now().await;
                                }
                                while bag_half_clone.pop().is_none() {
                                    crate::ebr::Guard::new().accelerate();
                                    task::yield_now().await;
                                }
                            }
                        }
                    }));
                }

                for r in futures::future::join_all(task_handles).await {
                    assert!(r.is_ok());
                }
                assert!(bag_default.pop().is_none());
                assert!(bag_default.is_empty());
                assert!(bag_half.pop().is_none());
                assert!(bag_half.is_empty());
            }
            assert_eq!(INST_CNT.load(Relaxed), 0);
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn mpsc() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        const NUM_TASKS: usize = 6;
        let workload_size = 256;
        let bag32: Arc<Bag<R>> = Arc::new(Bag::default());
        let bag7: Arc<Bag<R, 7>> = Arc::new(Bag::new());
        for _ in 0..256 {
            let mut task_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(AsyncBarrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let bag32_clone = bag32.clone();
                let bag7_clone = bag7.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let mut cnt = 0;
                    while task_id == 0 && cnt < workload_size * (NUM_TASKS - 1) * 2 {
                        cnt += bag32_clone.pop_all(0, |a, _| a + 1);
                        cnt += bag7_clone.pop_all(0, |a, _| a + 1);
                        tokio::task::yield_now().await;
                    }
                    if task_id != 0 {
                        for _ in 0..workload_size {
                            bag32_clone.push(R::new(&INST_CNT));
                            bag7_clone.push(R::new(&INST_CNT));
                        }
                        for _ in 0..workload_size / 16 {
                            if bag32_clone.pop().is_some() {
                                bag32_clone.push(R::new(&INST_CNT));
                            }
                            if bag7_clone.pop().is_some() {
                                bag7_clone.push(R::new(&INST_CNT));
                            }
                        }
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
            assert!(bag32.pop().is_none());
            assert!(bag32.is_empty());
            assert!(bag7.pop().is_none());
            assert!(bag7.is_empty());
        }
        assert_eq!(INST_CNT.load(Relaxed), 0);
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod queue_test {
    use crate::ebr::Guard;
    use crate::Queue;
    use std::panic::UnwindSafe;
    use std::rc::Rc;
    use std::sync::atomic::AtomicUsize;
    use std::sync::atomic::Ordering::Relaxed;
    use std::sync::{Arc, Barrier};
    use std::thread;

    static_assertions::assert_not_impl_all!(Queue<Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(Queue<String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Queue<*const String>: Send, Sync, UnwindSafe);

    struct R(&'static AtomicUsize, usize, usize);
    impl R {
        fn new(cnt: &'static AtomicUsize, task_id: usize, seq: usize) -> R {
            cnt.fetch_add(1, Relaxed);
            R(cnt, task_id, seq)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, Relaxed);
        }
    }

    #[test]
    fn clone() {
        let queue = Queue::default();
        queue.push(37);
        queue.push(3);
        queue.push(1);

        let queue_clone = queue.clone();

        assert_eq!(queue.pop().map(|e| **e), Some(37));
        assert_eq!(queue.pop().map(|e| **e), Some(3));
        assert_eq!(queue.pop().map(|e| **e), Some(1));
        assert!(queue.pop().is_none());

        assert_eq!(queue_clone.pop().map(|e| **e), Some(37));
        assert_eq!(queue_clone.pop().map(|e| **e), Some(3));
        assert_eq!(queue_clone.pop().map(|e| **e), Some(1));
        assert!(queue_clone.pop().is_none());
    }

    #[test]
    fn from_iter() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let workload_size = 16;
        let queue = (0..workload_size)
            .map(|i| R::new(&INST_CNT, i, i))
            .collect::<Queue<R>>();
        assert_eq!(queue.len(), workload_size);
        drop(queue);

        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
        }
    }

    #[test]
    fn pop_all() {
        const NUM_ENTRIES: usize = 256;
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let queue = Queue::default();

        for i in 0..NUM_ENTRIES {
            queue.push(R::new(&INST_CNT, i, i));
        }

        let mut expected = 0;
        while let Some(e) = queue.pop() {
            assert_eq!(e.1, expected);
            expected += 1;
        }
        assert_eq!(expected, NUM_ENTRIES);
        assert!(queue.is_empty());

        let mut cnt = 0;
        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
            cnt += 1;
        }

        // Expect `cnt <= 10`.
        println!("{cnt}");
    }

    #[test]
    fn iter_push_pop() {
        const NUM_TASKS: usize = 4;
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let queue: Arc<Queue<R>> = Arc::new(Queue::default());
        let workload_size = if cfg!(miri) { 16 } else { 256 };
        for _ in 0..16 {
            let mut thread_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(Barrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let queue_clone = queue.clone();
                thread_handles.push(thread::spawn(move || {
                    if task_id == 0 {
                        for seq in 0..workload_size {
                            if seq == workload_size / 2 {
                                barrier_clone.wait();
                            }
                            assert_eq!(queue_clone.push(R::new(&INST_CNT, task_id, seq)).2, seq);
                        }
                        let mut last = 0;
                        while let Some(popped) = queue_clone.pop() {
                            let current = popped.1;
                            assert!(last == 0 || last + 1 == current);
                            last = current;
                        }
                    } else {
                        let mut last = 0;

                        barrier_clone.wait();
                        let guard = Guard::new();
                        let iter = queue_clone.iter(&guard);
                        for current in iter {
                            let current = current.1;
                            assert!(current == 0 || last + 1 == current);
                            last = current;
                        }
                    }
                }));
            }

            for t in thread_handles {
                assert!(t.join().is_ok());
            }
        }
        assert!(queue.is_empty());

        let mut cnt = 0;
        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
            cnt += 1;
        }

        // Expect `cnt <= 10`.
        println!("{cnt}");
    }

    #[test]
    fn mpmc() {
        const NUM_TASKS: usize = if cfg!(miri) { 3 } else { 6 };
        const NUM_PRODUCERS: usize = NUM_TASKS / 2;
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);

        let queue: Arc<Queue<R>> = Arc::new(Queue::default());
        let workload_size = if cfg!(miri) { 16 } else { 256 };
        for _ in 0..16 {
            let num_popped: Arc<AtomicUsize> = Arc::new(AtomicUsize::default());
            let mut thread_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(Barrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let queue_clone = queue.clone();
                let num_popped_clone = num_popped.clone();
                thread_handles.push(thread::spawn(move || {
                    barrier_clone.wait();
                    if task_id < NUM_PRODUCERS {
                        for seq in 1..=workload_size {
                            assert_eq!(queue_clone.push(R::new(&INST_CNT, task_id, seq)).2, seq);
                        }
                    } else {
                        let mut popped_acc: [usize; NUM_PRODUCERS] = Default::default();
                        loop {
                            let mut cnt = 0;
                            while let Some(popped) = queue_clone.pop() {
                                cnt += 1;
                                assert!(popped_acc[popped.1] < popped.2);
                                popped_acc[popped.1] = popped.2;
                            }
                            if num_popped_clone.fetch_add(cnt, Relaxed) + cnt
                                == workload_size * NUM_PRODUCERS
                            {
                                break;
                            }
                            thread::yield_now();
                        }
                    }
                }));
            }

            for t in thread_handles {
                assert!(t.join().is_ok());
            }
        }
        assert!(queue.is_empty());

        let mut cnt = 0;
        while INST_CNT.load(Relaxed) != 0 {
            Guard::new().accelerate();
            thread::yield_now();
            cnt += 1;
        }

        // Expect `cnt <= 50`.
        println!("{cnt}");
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod stack_test {
    use crate::ebr::Guard;
    use crate::Stack;
    use std::{panic::UnwindSafe, rc::Rc, sync::Arc};
    use tokio::sync::Barrier as AsyncBarrier;

    static_assertions::assert_not_impl_all!(Stack<Rc<String>>: Send, Sync);
    static_assertions::assert_impl_all!(Stack<String>: Send, Sync, UnwindSafe);
    static_assertions::assert_not_impl_all!(Stack<*const String>: Send, Sync, UnwindSafe);

    #[derive(Debug)]
    struct R(usize, usize);
    impl R {
        fn new(task_id: usize, seq: usize) -> R {
            R(task_id, seq)
        }
    }

    #[test]
    fn clone() {
        let stack = Stack::default();
        stack.push(37);
        stack.push(3);
        stack.push(1);

        let stack_clone = stack.clone();

        assert_eq!(stack.pop().map(|e| **e), Some(1));
        assert_eq!(stack.pop().map(|e| **e), Some(3));
        assert_eq!(stack.pop().map(|e| **e), Some(37));
        assert!(stack.pop().is_none());

        assert_eq!(stack_clone.pop().map(|e| **e), Some(1));
        assert_eq!(stack_clone.pop().map(|e| **e), Some(3));
        assert_eq!(stack_clone.pop().map(|e| **e), Some(37));
        assert!(stack_clone.pop().is_none());
    }

    #[test]
    fn from_iter() {
        let workload_size = 16;
        let stack = (0..workload_size).collect::<Stack<usize>>();
        assert_eq!(stack.len(), workload_size);
        for i in (0..workload_size).rev() {
            assert_eq!(stack.pop().map(|e| **e), Some(i));
        }
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 2)]
    async fn iterator() {
        const NUM_TASKS: usize = 2;
        let stack: Arc<Stack<R>> = Arc::new(Stack::default());
        let workload_size = 256;
        for _ in 0..16 {
            let mut task_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(AsyncBarrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let stack_clone = stack.clone();
                task_handles.push(tokio::task::spawn(async move {
                    if task_id == 0 {
                        for seq in 0..workload_size {
                            if seq == workload_size / 2 {
                                barrier_clone.wait().await;
                            }
                            assert_eq!(stack_clone.push(R::new(task_id, seq)).1, seq);
                        }
                        let mut last = workload_size;
                        while let Some(popped) = stack_clone.pop() {
                            let current = popped.1;
                            assert_eq!(current + 1, last);
                            last = current;
                        }
                    } else {
                        let mut last = workload_size;

                        barrier_clone.wait().await;
                        let guard = Guard::new();
                        let iter = stack_clone.iter(&guard);
                        for current in iter {
                            let current = current.1;
                            assert!(last == workload_size || last - 1 == current);
                            last = current;
                        }
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
        }
        assert!(stack.is_empty());
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn mpmc() {
        const NUM_TASKS: usize = 12;
        let stack: Arc<Stack<R>> = Arc::new(Stack::default());
        let workload_size = 256;
        for _ in 0..16 {
            let mut task_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(AsyncBarrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let stack_clone = stack.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    for seq in 0..workload_size {
                        assert_eq!(stack_clone.push(R::new(task_id, seq)).1, seq);
                    }
                    let mut last_popped = usize::MAX;
                    let mut cnt = 0;
                    while cnt < workload_size {
                        while let Ok(Some(popped)) = stack_clone.pop_if(|e| e.0 == task_id) {
                            assert_eq!(popped.0, task_id);
                            assert!(last_popped > popped.1);
                            last_popped = popped.1;
                            cnt += 1;
                        }
                        tokio::task::yield_now().await;
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
        }
        assert!(stack.is_empty());
    }

    #[cfg_attr(miri, ignore)]
    #[tokio::test(flavor = "multi_thread", worker_threads = 16)]
    async fn mpsc() {
        const NUM_TASKS: usize = 12;
        let stack: Arc<Stack<R>> = Arc::new(Stack::default());
        let workload_size = 256;
        for _ in 0..16 {
            let mut task_handles = Vec::with_capacity(NUM_TASKS);
            let barrier = Arc::new(AsyncBarrier::new(NUM_TASKS));
            for task_id in 0..NUM_TASKS {
                let barrier_clone = barrier.clone();
                let stack_clone = stack.clone();
                task_handles.push(tokio::task::spawn(async move {
                    barrier_clone.wait().await;
                    let mut cnt = 0;
                    while task_id == 0 && cnt < workload_size * (NUM_TASKS - 1) {
                        // Consumer.
                        let popped = stack_clone.pop_all();
                        while let Some(e) = popped.pop() {
                            assert_ne!(e.0, 0);
                            cnt += 1;
                        }
                        tokio::task::yield_now().await;
                    }
                    if task_id != 0 {
                        for seq in 0..workload_size {
                            assert_eq!(stack_clone.push(R::new(task_id, seq)).1, seq);
                        }
                        for seq in 0..workload_size / 16 {
                            if stack_clone.pop().is_some() {
                                assert_eq!(stack_clone.push(R::new(task_id, seq)).1, seq);
                            }
                        }
                    }
                }));
            }

            for r in futures::future::join_all(task_handles).await {
                assert!(r.is_ok());
            }
        }
        assert!(stack.is_empty());
    }
}

#[cfg(not(feature = "loom"))]
#[cfg(test)]
mod random_failure_test {
    use crate::ebr::{Guard, Shared};
    use crate::hash_map::Entry;
    use crate::{HashCache, HashIndex, HashMap, TreeIndex};
    use std::any::Any;
    use std::panic::catch_unwind;
    use std::sync::atomic::Ordering::{AcqRel, Relaxed};
    use std::sync::atomic::{AtomicBool, AtomicUsize};

    struct R(&'static AtomicUsize, &'static AtomicBool, bool);
    impl R {
        fn new(cnt: &'static AtomicUsize, never_panic: &'static AtomicBool) -> R {
            assert!(never_panic.load(Relaxed) || rand::random::<u8>() % 4 != 0);
            cnt.fetch_add(1, AcqRel);
            R(cnt, never_panic, false)
        }
        fn new_panic_free_drop(cnt: &'static AtomicUsize, never_panic: &'static AtomicBool) -> R {
            cnt.fetch_add(1, AcqRel);
            R(cnt, never_panic, true)
        }
    }
    impl Clone for R {
        fn clone(&self) -> Self {
            assert!(self.1.load(Relaxed) || rand::random::<u8>() % 8 != 0);
            self.0.fetch_add(1, AcqRel);
            Self(self.0, self.1, self.2)
        }
    }
    impl Drop for R {
        fn drop(&mut self) {
            self.0.fetch_sub(1, AcqRel);
            assert!(self.1.load(Relaxed) || self.2 || rand::random::<u8>() % 16 != 0);
        }
    }

    #[allow(clippy::too_many_lines)]
    #[cfg_attr(miri, ignore)]
    #[test]
    fn panic_safety() {
        static INST_CNT: AtomicUsize = AtomicUsize::new(0);
        static NEVER_PANIC: AtomicBool = AtomicBool::new(false);

        let workload_size = u8::MAX;

        // EBR.
        for _ in 0..workload_size {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                let r = Shared::new(R::new(&INST_CNT, &NEVER_PANIC));
                assert_ne!(INST_CNT.load(Relaxed), 0);
                drop(r);
            });
        }
        while INST_CNT.load(Relaxed) != 0 {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                drop(Guard::new());
            });
            std::thread::yield_now();
        }

        // HashMap.
        let hashmap: HashMap<usize, R> = HashMap::default();
        for k in 0..workload_size {
            let result: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                hashmap.entry(k as usize).or_insert_with(|| {
                    let mut r = R::new(&INST_CNT, &NEVER_PANIC);
                    r.2 = true;
                    r
                });
            });
            NEVER_PANIC.store(true, Relaxed);
            assert_eq!(
                hashmap.read(&(k as usize), |_, _| ()).is_some(),
                result.is_ok()
            );
            NEVER_PANIC.store(false, Relaxed);
        }
        drop(hashmap);

        // HashMap.
        let hashmap: HashMap<usize, R> = HashMap::default();
        for k in 0..workload_size {
            let result: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                let Entry::Vacant(entry) = hashmap.entry(k as usize) else {
                    return;
                };
                entry
                    .insert_entry(R::new(&INST_CNT, &NEVER_PANIC))
                    .get_mut()
                    .2 = true;
            });
            assert_eq!(
                hashmap.read(&(k as usize), |_, _| ()).is_some(),
                result.is_ok()
            );
        }
        drop(hashmap);

        while INST_CNT.load(Relaxed) != 0 {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                drop(Guard::new());
            });
            std::thread::yield_now();
        }

        // HashIndex.
        let hashindex: HashIndex<usize, R> = HashIndex::default();
        for k in 0..workload_size {
            let result: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                assert!(hashindex
                    .insert(k as usize, R::new_panic_free_drop(&INST_CNT, &NEVER_PANIC))
                    .is_ok());
            });
            NEVER_PANIC.store(true, Relaxed);
            assert_eq!(
                hashindex.peek_with(&(k as usize), |_, _| ()).is_some(),
                result.is_ok()
            );
            NEVER_PANIC.store(false, Relaxed);
        }
        drop(hashindex);

        while INST_CNT.load(Relaxed) != 0 {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                drop(Guard::new());
            });
            std::thread::yield_now();
        }

        // HashCache.
        let hashcache: HashCache<usize, R> = HashCache::default();
        for k in 0..workload_size {
            let result: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                assert!(hashcache
                    .put(k as usize, R::new(&INST_CNT, &NEVER_PANIC))
                    .is_ok());
                if let Some(mut o) = hashcache.get(&(k as usize)) {
                    o.get_mut().2 = true;
                }
            });
            assert_eq!(hashcache.get(&(k as usize)).is_some(), result.is_ok());
        }
        drop(hashcache);

        while INST_CNT.load(Relaxed) != 0 {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                drop(Guard::new());
            });
            std::thread::yield_now();
        }

        // TreeIndex.
        let treeindex: TreeIndex<usize, R> = TreeIndex::default();
        for k in 0..14 * 14 * 14 {
            let result: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                assert!(treeindex
                    .insert(k, R::new_panic_free_drop(&INST_CNT, &NEVER_PANIC))
                    .is_ok());
                assert!(treeindex.peek_with(&k, |_, _| ()).is_some());
            });
            assert_eq!(treeindex.peek_with(&k, |_, _| ()).is_some(), result.is_ok());
        }
        drop(treeindex);

        while INST_CNT.load(Relaxed) != 0 {
            let _: Result<(), Box<dyn Any + Send>> = catch_unwind(|| {
                drop(Guard::new());
            });
            std::thread::yield_now();
        }
    }
}

#[cfg(feature = "serde")]
#[cfg(test)]
mod serde_test {
    use crate::{HashCache, HashIndex, HashMap, HashSet, TreeIndex};

    use serde_test::{assert_tokens, Token};

    #[test]
    fn hashmap() {
        let hashmap: HashMap<u64, i16> = HashMap::new();
        assert!(hashmap.insert(2, -6).is_ok());
        assert_tokens(
            &hashmap,
            &[
                Token::Map { len: Some(1) },
                Token::U64(2),
                Token::I16(-6),
                Token::MapEnd,
            ],
        );
    }

    #[test]
    fn hashset() {
        let hashset: HashSet<u64> = HashSet::new();
        assert!(hashset.insert(2).is_ok());
        assert_tokens(
            &hashset,
            &[Token::Seq { len: Some(1) }, Token::U64(2), Token::SeqEnd],
        );
    }

    #[test]
    fn hashindex() {
        let hashindex: HashIndex<u64, i16> = HashIndex::new();
        assert!(hashindex.insert(2, -6).is_ok());
        assert_tokens(
            &hashindex,
            &[
                Token::Map { len: Some(1) },
                Token::U64(2),
                Token::I16(-6),
                Token::MapEnd,
            ],
        );
    }

    #[test]
    fn hashcache() {
        let hashcache: HashCache<u64, i16> = HashCache::new();
        let capacity_range = hashcache.capacity_range();
        assert!(hashcache.put(2, -6).is_ok());
        assert_tokens(
            &hashcache,
            &[
                Token::Map {
                    len: Some(*capacity_range.end()),
                },
                Token::U64(2),
                Token::I16(-6),
                Token::MapEnd,
            ],
        );
    }

    #[test]
    fn treeindex() {
        let treeindex: TreeIndex<u64, i16> = TreeIndex::new();
        assert!(treeindex.insert(4, -4).is_ok());
        assert!(treeindex.insert(2, -6).is_ok());
        assert!(treeindex.insert(3, -5).is_ok());
        assert_tokens(
            &treeindex,
            &[
                Token::Map { len: Some(3) },
                Token::U64(2),
                Token::I16(-6),
                Token::U64(3),
                Token::I16(-5),
                Token::U64(4),
                Token::I16(-4),
                Token::MapEnd,
            ],
        );
    }
}
