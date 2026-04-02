// https://github.com/bincode-org/bincode/issues/618

use bincode::{Decode, Encode};
use criterion::{black_box, criterion_group, criterion_main, Criterion};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Default, Encode, Decode)]
pub struct MyStruct {
    pub v: Vec<String>,
    pub string: String,
    pub number: usize,
}

impl MyStruct {
    #[inline]
    pub fn new(v: Vec<String>, string: String, number: usize) -> Self {
        Self { v, string, number }
    }
}

fn build_data(size: usize) -> Vec<MyStruct> {
    (0..size)
        .map(|i| {
            let vec: Vec<String> = (0..i).map(|i| i.to_string().repeat(100)).collect();
            MyStruct::new(vec, size.to_string(), size)
        })
        .collect()
}

fn index_item_decode(c: &mut Criterion) {
    let data = build_data(100);

    c.bench_function("bench v1", |b| {
        b.iter(|| {
            let _ = black_box(bincode_1::serialize(black_box(&data))).unwrap();
        });
    });

    let config = bincode::config::standard();
    c.bench_function("bench v2 (standard)", |b| {
        b.iter(|| {
            let _ = black_box(bincode::encode_to_vec(black_box(&data), config)).unwrap();
        });
    });

    let config = bincode::config::legacy();
    c.bench_function("bench v2 (legacy)", |b| {
        b.iter(|| {
            let _ = black_box(bincode::encode_to_vec(black_box(&data), config)).unwrap();
        });
    });

    let encodedv1 = bincode_1::serialize(&data).unwrap();
    let encodedv2 = bincode::encode_to_vec(&data, config).unwrap();
    assert_eq!(encodedv1, encodedv2);

    c.bench_function("bench v1 decode", |b| {
        b.iter(|| {
            let _: Vec<MyStruct> =
                black_box(bincode_1::deserialize(black_box(&encodedv1))).unwrap();
        });
    });

    c.bench_function("bench v2 decode (legacy)", |b| {
        b.iter(|| {
            let _: (Vec<MyStruct>, _) =
                black_box(bincode::decode_from_slice(black_box(&encodedv1), config)).unwrap();
        });
    });
}

criterion_group!(benches, index_item_decode);
criterion_main!(benches);
