use criterion::{criterion_group, criterion_main, Criterion};
use scc::HashMap;
use std::time::{Duration, Instant};

fn insert_cold(c: &mut Criterion) {
    c.bench_function("HashMap: insert, cold", |b| {
        b.iter_custom(|iters| {
            let hashmap: HashMap<u64, u64> = HashMap::default();
            let start = Instant::now();
            for i in 0..iters {
                assert!(hashmap.insert(i, i).is_ok());
            }
            start.elapsed()
        })
    });
}

fn insert_warmed_up(c: &mut Criterion) {
    c.bench_function("HashMap: insert, warmed up", |b| {
        b.iter_custom(|iters| {
            let hashmap: HashMap<u64, u64> = HashMap::with_capacity(iters as usize * 2);
            let start = Instant::now();
            for i in 0..iters {
                assert!(hashmap.insert(i, i).is_ok());
            }
            start.elapsed()
        })
    });
}

fn read(c: &mut Criterion) {
    c.bench_function("HashMap: read", |b| {
        b.iter_custom(|iters| {
            let hashmap: HashMap<u64, u64> = HashMap::with_capacity(iters as usize * 2);
            for i in 0..iters {
                assert!(hashmap.insert(i, i).is_ok());
            }
            let start = Instant::now();
            for i in 0..iters {
                assert_eq!(hashmap.read(&i, |_, v| *v == i), Some(true));
            }
            start.elapsed()
        })
    });
}

fn insert_tail_latency(c: &mut Criterion) {
    c.bench_function("HashMap: insert, tail latency", move |b| {
        b.iter_custom(|iters| {
            let mut agg_max_latency = Duration::default();
            for _ in 0..iters {
                let hashmap: HashMap<u64, u64> = HashMap::default();
                let mut key = 0;
                let mut max_latency = Duration::default();
                (0..1048576).for_each(|_| {
                    key += 1;
                    let start = Instant::now();
                    assert!(hashmap.insert(key, key).is_ok());
                    let elapsed = start.elapsed();
                    if elapsed > max_latency {
                        max_latency = elapsed;
                    }
                });
                agg_max_latency += max_latency;
            }
            agg_max_latency
        })
    });
}

criterion_group!(
    hash_map,
    insert_cold,
    insert_tail_latency,
    insert_warmed_up,
    read
);
criterion_main!(hash_map);
