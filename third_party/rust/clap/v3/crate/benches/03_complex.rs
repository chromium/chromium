use clap::{arg, Arg, Command};
use criterion::{criterion_group, criterion_main, Criterion};

static OPT3_VALS: [&str; 2] = ["fast", "slow"];
static POS3_VALS: [&str; 2] = ["vi", "emacs"];

macro_rules! create_app {
    () => {{
        Command::new("claptests")
            .version("0.1")
            .about("tests clap library")
            .author("Kevin K. <kbknapp@gmail.com>")
            .arg(arg!(-o --option <opt> ... "tests options").required(false))
            .arg(arg!([positional] "tests positionals"))
            .arg(arg!(-f --flag ... "tests flags").global(true))
            .args(&[
                arg!(flag2: -F "tests flags with exclusions")
                    .conflicts_with("flag")
                    .requires("option2"),
                arg!(option2: --"long-option-2" <option2> "tests long options with exclusions")
                    .required(false)
                    .conflicts_with("option")
                    .requires("positional2"),
                arg!([positional2] "tests positionals with exclusions"),
                arg!(-O --Option <option3> "tests options with specific value sets")
                .required(false)
                    .possible_values(OPT3_VALS),
                arg!([positional3] ... "tests positionals with specific values")
                    .possible_values(POS3_VALS),
                arg!(--multvals "Tests multiple values not mult occs").required(false).value_names(&["one", "two"]),
                arg!(
                    --multvalsmo "Tests multiple values, not mult occs"
                ).multiple_values(true).required(false).value_names(&["one", "two"]),
                arg!(--minvals2 <minvals> ... "Tests 2 min vals").min_values(2).multiple_values(true).required(false),
                arg!(--maxvals3 <maxvals> ... "Tests 3 max vals").max_values(3).multiple_values(true).required(false),
            ])
            .subcommand(
                Command::new("subcmd")
                    .about("tests subcommands")
                    .version("0.1")
                    .author("Kevin K. <kbknapp@gmail.com>")
                    .arg(arg!(-o --option <scoption> ... "tests options").required(false))
                    .arg(arg!([scpositional] "tests positionals"))
            )
    }};
}

pub fn build_from_builder(c: &mut Criterion) {
    c.bench_function("build_from_builder", |b| {
        b.iter(|| {
            Command::new("claptests")
                .version("0.1")
                .about("tests clap library")
                .author("Kevin K. <kbknapp@gmail.com>")
                .arg(
                    Arg::new("opt")
                        .help("tests options")
                        .short('o')
                        .long("option")
                        .takes_value(true)
                        .multiple_values(true)
                        .multiple_occurrences(true),
                )
                .arg(Arg::new("positional").help("tests positionals").index(1))
                .arg(
                    Arg::new("flag")
                        .short('f')
                        .help("tests flags")
                        .long("flag")
                        .global(true)
                        .multiple_occurrences(true),
                )
                .arg(
                    Arg::new("flag2")
                        .short('F')
                        .help("tests flags with exclusions")
                        .conflicts_with("flag")
                        .requires("option2"),
                )
                .arg(
                    Arg::new("option2")
                        .help("tests long options with exclusions")
                        .conflicts_with("option")
                        .requires("positional2")
                        .takes_value(true)
                        .long("long-option-2"),
                )
                .arg(
                    Arg::new("positional2")
                        .index(3)
                        .help("tests positionals with exclusions"),
                )
                .arg(
                    Arg::new("option3")
                        .short('O')
                        .long("Option")
                        .takes_value(true)
                        .help("tests options with specific value sets")
                        .possible_values(OPT3_VALS),
                )
                .arg(
                    Arg::new("positional3")
                        .takes_value(true)
                        .multiple_values(true)
                        .multiple_occurrences(true)
                        .help("tests positionals with specific values")
                        .index(4)
                        .possible_values(POS3_VALS),
                )
                .arg(
                    Arg::new("multvals")
                        .long("multvals")
                        .help("Tests multiple values, not mult occs")
                        .value_names(&["one", "two"]),
                )
                .arg(
                    Arg::new("multvalsmo")
                        .long("multvalsmo")
                        .takes_value(true)
                        .multiple_values(true)
                        .multiple_occurrences(true)
                        .help("Tests multiple values, not mult occs")
                        .value_names(&["one", "two"]),
                )
                .arg(
                    Arg::new("minvals")
                        .long("minvals2")
                        .takes_value(true)
                        .multiple_values(true)
                        .multiple_occurrences(true)
                        .help("Tests 2 min vals")
                        .min_values(2),
                )
                .arg(
                    Arg::new("maxvals")
                        .long("maxvals3")
                        .takes_value(true)
                        .multiple_values(true)
                        .multiple_occurrences(true)
                        .help("Tests 3 max vals")
                        .max_values(3),
                )
                .subcommand(
                    Command::new("subcmd")
                        .about("tests subcommands")
                        .version("0.1")
                        .author("Kevin K. <kbknapp@gmail.com>")
                        .arg(
                            Arg::new("scoption")
                                .short('o')
                                .long("option")
                                .takes_value(true)
                                .multiple_values(true)
                                .multiple_occurrences(true)
                                .help("tests options"),
                        )
                        .arg(Arg::new("scpositional").index(1).help("tests positionals")),
                )
        })
    });
}

pub fn parse_complex(c: &mut Criterion) {
    c.bench_function("parse_complex", |b| {
        b.iter(|| create_app!().get_matches_from(vec![""]))
    });
}

pub fn parse_complex_with_flag(c: &mut Criterion) {
    c.bench_function("parse_complex_with_flag", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "-f"]))
    });
}

pub fn parse_complex_with_opt(c: &mut Criterion) {
    c.bench_function("parse_complex_with_opt", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "-o", "option1"]))
    });
}

pub fn parse_complex_with_pos(c: &mut Criterion) {
    c.bench_function("parse_complex_with_pos", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "arg1"]))
    });
}

pub fn parse_complex_with_sc(c: &mut Criterion) {
    c.bench_function("parse_complex_with_sc", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "subcmd"]))
    });
}

pub fn parse_complex_with_sc_flag(c: &mut Criterion) {
    c.bench_function("parse_complex_with_sc_flag", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "subcmd", "-f"]))
    });
}

pub fn parse_complex_with_sc_opt(c: &mut Criterion) {
    c.bench_function("parse_complex_with_sc_opt", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "subcmd", "-o", "option1"]))
    });
}

pub fn parse_complex_with_sc_pos(c: &mut Criterion) {
    c.bench_function("parse_complex_with_sc_pos", |b| {
        b.iter(|| create_app!().get_matches_from(vec!["myprog", "subcmd", "arg1"]))
    });
}

pub fn parse_complex1(c: &mut Criterion) {
    c.bench_function("parse_complex1", |b| {
        b.iter(|| {
            create_app!().get_matches_from(vec![
                "myprog",
                "-ff",
                "-o",
                "option1",
                "arg1",
                "-O",
                "fast",
                "arg2",
                "--multvals",
                "one",
                "two",
                "emacs",
            ])
        })
    });
}

pub fn parse_complex2(c: &mut Criterion) {
    c.bench_function("parse_complex2", |b| {
        b.iter(|| {
            create_app!().get_matches_from(vec![
                "myprog",
                "arg1",
                "-f",
                "arg2",
                "--long-option-2",
                "some",
                "-O",
                "slow",
                "--multvalsmo",
                "one",
                "two",
                "--minvals2",
                "3",
                "2",
                "1",
            ])
        })
    });
}

pub fn parse_args_negate_scs(c: &mut Criterion) {
    c.bench_function("parse_args_negate_scs", |b| {
        b.iter(|| {
            create_app!()
                .args_conflicts_with_subcommands(true)
                .get_matches_from(vec![
                    "myprog",
                    "arg1",
                    "-f",
                    "arg2",
                    "--long-option-2",
                    "some",
                    "-O",
                    "slow",
                    "--multvalsmo",
                    "one",
                    "two",
                    "--minvals2",
                    "3",
                    "2",
                    "1",
                ])
        })
    });
}

pub fn parse_complex_with_sc_complex(c: &mut Criterion) {
    c.bench_function("parse_complex_with_sc_complex", |b| {
        b.iter(|| {
            create_app!().get_matches_from(vec!["myprog", "subcmd", "-f", "-o", "option1", "arg1"])
        })
    });
}

criterion_group!(
    benches,
    build_from_builder,
    parse_complex,
    parse_complex_with_flag,
    parse_complex_with_opt,
    parse_complex_with_pos,
    parse_complex_with_sc,
    parse_complex_with_sc_flag,
    parse_complex_with_sc_opt,
    parse_complex_with_sc_pos,
    parse_complex1,
    parse_complex2,
    parse_args_negate_scs,
    parse_complex_with_sc_complex
);

criterion_main!(benches);
