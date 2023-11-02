use clap::{arg, Arg, Command};
use criterion::{criterion_group, criterion_main, Criterion};

macro_rules! create_app {
    () => {{
        Command::new("claptests")
            .version("0.1")
            .about("tests clap library")
            .author("Kevin K. <kbknapp@gmail.com>")
            .arg(arg!(-f --flag         "tests flags"))
            .arg(arg!(-o --option <opt> "tests options").required(false))
            .arg(arg!([positional]      "tests positional"))
    }};
}

pub fn build_simple(c: &mut Criterion) {
    c.bench_function("build_simple", |b| b.iter(|| create_app!()));
}

pub fn build_with_flag(c: &mut Criterion) {
    c.bench_function("build_with_flag", |b| {
        b.iter(|| Command::new("claptests").arg(arg!(-s --some "something")))
    });
}

pub fn build_with_flag_ref(c: &mut Criterion) {
    c.bench_function("build_with_flag_ref", |b| {
        b.iter(|| {
            let arg = arg!(-s --some "something");
            Command::new("claptests").arg(&arg)
        })
    });
}

pub fn build_with_opt(c: &mut Criterion) {
    c.bench_function("build_with_opt", |b| {
        b.iter(|| Command::new("claptests").arg(arg!(-s --some <FILE> "something")))
    });
}

pub fn build_with_opt_ref(c: &mut Criterion) {
    c.bench_function("build_with_opt_ref", |b| {
        b.iter(|| {
            let arg = arg!(-s --some <FILE> "something");
            Command::new("claptests").arg(&arg)
        })
    });
}

pub fn build_with_pos(c: &mut Criterion) {
    c.bench_function("build_with_pos", |b| {
        b.iter(|| Command::new("claptests").arg(Arg::new("some")))
    });
}

pub fn build_with_pos_ref(c: &mut Criterion) {
    c.bench_function("build_with_pos_ref", |b| {
        b.iter(|| {
            let arg = Arg::new("some");
            Command::new("claptests").arg(&arg)
        })
    });
}

pub fn parse_simple_with_flag(c: &mut Criterion) {
    c.bench_function("parse_simple_with_flag", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "-f"]))
    });
}

pub fn parse_simple_with_opt(c: &mut Criterion) {
    c.bench_function("parse_simple_with_opt", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "-o", "option1"]))
    });
}

pub fn parse_simple_with_pos(c: &mut Criterion) {
    c.bench_function("parse_simple_with_pos", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "arg1"]))
    });
}

pub fn parse_simple_with_complex(c: &mut Criterion) {
    c.bench_function("parse_simple_with_complex", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "-o", "option1", "-f", "arg1"]))
    });
}

criterion_group!(
    benches,
    parse_simple_with_complex,
    parse_simple_with_pos,
    parse_simple_with_opt,
    parse_simple_with_flag,
    build_with_pos_ref,
    build_with_pos,
    build_with_opt_ref,
    build_with_opt,
    build_with_flag_ref,
    build_with_flag,
    build_simple
);

criterion_main!(benches);
