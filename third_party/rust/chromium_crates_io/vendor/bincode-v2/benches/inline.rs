use bincode::config;
use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn inline_decoder_claim_bytes_read(c: &mut Criterion) {
    let config = config::standard().with_limit::<100000>();
    let slice = bincode::encode_to_vec(vec![String::from("Hello world"); 1000], config).unwrap();

    c.bench_function("inline_decoder_claim_bytes_read", |b| {
        b.iter(|| {
            let _: (Vec<String>, usize) =
                black_box(bincode::decode_from_slice(black_box(&slice), config).unwrap());
        })
    });
}

criterion_group!(benches, inline_decoder_claim_bytes_read);
criterion_main!(benches);
