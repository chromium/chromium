use clap::Command;
use clap::{arg, Arg};
use criterion::{criterion_group, criterion_main, Criterion};
use std::io::Cursor;

fn build_help(cmd: &mut Command) -> String {
    let mut buf = Cursor::new(Vec::with_capacity(50));
    cmd.write_help(&mut buf).unwrap();
    let content = buf.into_inner();
    String::from_utf8(content).unwrap()
}

fn app_example1<'c>() -> Command<'c> {
    Command::new("MyApp")
        .version("1.0")
        .author("Kevin K. <kbknapp@gmail.com>")
        .about("Does awesome things")
        .arg(
            arg!(
                -c --config <FILE> "Sets a custom config file"
            )
            .required(false),
        )
        .arg(arg!(<output> "Sets an optional output file"))
        .arg(arg!(d: -d ... "Turn debugging information on"))
        .subcommand(
            Command::new("test")
                .about("does testing things")
                .arg(arg!(-l --list "lists test values")),
        )
}

fn app_example2<'c>() -> Command<'c> {
    Command::new("MyApp")
        .version("1.0")
        .author("Kevin K. <kbknapp@gmail.com>")
        .about("Does awesome things")
}

fn app_example3<'c>() -> Command<'c> {
    Command::new("MyApp")
        .arg(
            Arg::new("debug")
                .help("turn on debugging information")
                .short('d'),
        )
        .args(&[
            Arg::new("config")
                .help("sets the config file to use")
                .takes_value(true)
                .short('c')
                .long("config"),
            Arg::new("input")
                .help("the input file to use")
                .required(true),
        ])
        .arg(arg!(--license "display the license file"))
        .arg(arg!([output] "Supply an output file to use"))
        .arg(
            arg!(
                -i --int <IFACE> "Set an interface to use"
            )
            .required(false),
        )
}

fn app_example4<'c>() -> Command<'c> {
    Command::new("MyApp")
        .about("Parses an input file to do awesome things")
        .version("1.0")
        .author("Kevin K. <kbknapp@gmail.com>")
        .arg(
            Arg::new("debug")
                .help("turn on debugging information")
                .short('d')
                .long("debug"),
        )
        .arg(
            Arg::new("config")
                .help("sets the config file to use")
                .short('c')
                .long("config"),
        )
        .arg(
            Arg::new("input")
                .help("the input file to use")
                .index(1)
                .required(true),
        )
}

fn app_example5<'c>() -> Command<'c> {
    Command::new("MyApp").arg(
        Arg::new("awesome")
            .help("turns up the awesome")
            .short('a')
            .long("awesome")
            .multiple_occurrences(true),
    )
}

fn app_example6<'c>() -> Command<'c> {
    Command::new("MyApp")
        .arg(
            Arg::new("input")
                .help("the input file to use")
                .index(1)
                .requires("config")
                .required(true),
        )
        .arg(Arg::new("config").help("the config file to use").index(2))
}

fn app_example7<'c>() -> Command<'c> {
    Command::new("MyApp")
        .arg(Arg::new("config"))
        .arg(Arg::new("output"))
        .arg(
            Arg::new("input")
                .help("the input file to use")
                .takes_value(true)
                .multiple_values(true)
                .multiple_occurrences(true)
                .required(true)
                .short('i')
                .long("input")
                .requires("config")
                .conflicts_with("output"),
        )
}

fn app_example8<'c>() -> Command<'c> {
    Command::new("MyApp")
        .arg(Arg::new("config"))
        .arg(Arg::new("output"))
        .arg(
            Arg::new("input")
                .help("the input file to use")
                .takes_value(true)
                .multiple_values(true)
                .multiple_occurrences(true)
                .required(true)
                .short('i')
                .long("input")
                .requires("config")
                .conflicts_with("output"),
        )
}

fn app_example10<'c>() -> Command<'c> {
    Command::new("myapp").about("does awesome things").arg(
        Arg::new("CONFIG")
            .help("The config file to use (default is \"config.json\")")
            .short('c')
            .takes_value(true),
    )
}

pub fn example1(c: &mut Criterion) {
    let mut cmd = app_example1();
    c.bench_function("example1", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example2(c: &mut Criterion) {
    let mut cmd = app_example2();
    c.bench_function("example2", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example3(c: &mut Criterion) {
    let mut cmd = app_example3();
    c.bench_function("example3", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example4(c: &mut Criterion) {
    let mut cmd = app_example4();
    c.bench_function("example4", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example5(c: &mut Criterion) {
    let mut cmd = app_example5();
    c.bench_function("example5", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example6(c: &mut Criterion) {
    let mut cmd = app_example6();
    c.bench_function("example6", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example7(c: &mut Criterion) {
    let mut cmd = app_example7();
    c.bench_function("example7", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example8(c: &mut Criterion) {
    let mut cmd = app_example8();
    c.bench_function("example8", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example10(c: &mut Criterion) {
    let mut cmd = app_example10();
    c.bench_function("example10", |b| b.iter(|| build_help(&mut cmd)));
}

pub fn example4_template(c: &mut Criterion) {
    let mut cmd = app_example4().help_template("{bin} {version}\n{author}\n{about}\n\nUSAGE:\n    {usage}\n\nOPTIONS:\n{options}\n\nARGS:\n{args}\n");
    c.bench_function("example4_template", |b| b.iter(|| build_help(&mut cmd)));
}

criterion_group!(
    benches,
    example1,
    example2,
    example3,
    example4,
    example5,
    example6,
    example7,
    example8,
    example10,
    example4_template
);

criterion_main!(benches);
