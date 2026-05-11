use criterion::{criterion_group, criterion_main, Criterion};
use scc::HashCache;
use std::time::Instant;

fn get(c: &mut Criterion) {
    c.bench_function("HashCache: get", |b| {
        b.iter_custom(|iters| {
            let hashcache: HashCache<u64, u64> =
                HashCache::with_capacity(iters as usize * 2, iters as usize * 2);
            for i in 0..iters {
                assert!(hashcache.put(i, i).is_ok());
            }
            let start = Instant::now();
            for i in 0..iters {
                drop(hashcache.get(&i));
            }
            start.elapsed()
        })
    });
}

fn put_saturated(c: &mut Criterion) {
    let hashcache: HashCache<u64, u64> = HashCache::with_capacity(64, 64);
    for k in 0..256 {
        assert!(hashcache.put(k, k).is_ok());
    }
    let mut max_key = 256;
    c.bench_function("HashCache: put, saturated", |b| {
        b.iter_custom(|iters| {
            let start = Instant::now();
            for i in max_key..(max_key + iters) {
                assert!(hashcache.put(i, i).is_ok());
            }
            max_key += iters;
            start.elapsed()
        })
    });
}

criterion_group!(hash_cache, get, put_saturated);
criterion_main!(hash_cache);
