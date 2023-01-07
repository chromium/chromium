use clap::Command;
use criterion::{criterion_group, criterion_main, Criterion};

pub fn build_empty(c: &mut Criterion) {
    c.bench_function("build_empty", |b| b.iter(|| Command::new("claptests")));
}

pub fn parse_empty(c: &mut Criterion) {
    c.bench_function("parse_empty", |b| {
        b.iter(|| Command::new("claptests").get_matches_from(vec![""]))
    });
}

criterion_group!(benches, build_empty, parse_empty);
criterion_main!(benches);
