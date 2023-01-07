use clap::Parser;

#[derive(Parser, Debug, PartialEq)]
#[clap(author, version, about, long_about = None)]
struct Opt {
    // Default parser for `try_from_str` is FromStr::from_str.
    // `impl FromStr for bool` parses `true` or `false` so this
    // works as expected.
    #[clap(long, parse(try_from_str))]
    foo: bool,

    // Of course, this could be done with an explicit parser function.
    #[clap(long, parse(try_from_str = true_or_false), default_value_t)]
    bar: bool,

    // `bool` can be positional only with explicit `parse(...)` annotation
    #[clap(parse(try_from_str))]
    boom: bool,
}

fn true_or_false(s: &str) -> Result<bool, &'static str> {
    match s {
        "true" => Ok(true),
        "false" => Ok(false),
        _ => Err("expected `true` or `false`"),
    }
}

fn main() {
    let opt = Opt::parse();
    dbg!(opt);
}
