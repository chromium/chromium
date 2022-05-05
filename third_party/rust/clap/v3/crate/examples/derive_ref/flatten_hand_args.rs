use clap::error::Error;
use clap::{Arg, ArgMatches, Args, Command, FromArgMatches, Parser};

#[derive(Debug)]
struct CliArgs {
    foo: bool,
    bar: bool,
    quuz: Option<String>,
}

impl FromArgMatches for CliArgs {
    fn from_arg_matches(matches: &ArgMatches) -> Result<Self, Error> {
        Ok(Self {
            foo: matches.is_present("foo"),
            bar: matches.is_present("bar"),
            quuz: matches.value_of("quuz").map(|quuz| quuz.to_owned()),
        })
    }
    fn update_from_arg_matches(&mut self, matches: &ArgMatches) -> Result<(), Error> {
        self.foo |= matches.is_present("foo");
        self.bar |= matches.is_present("bar");
        if let Some(quuz) = matches.value_of("quuz") {
            self.quuz = Some(quuz.to_owned());
        }
        Ok(())
    }
}

impl Args for CliArgs {
    fn augment_args(cmd: Command<'_>) -> Command<'_> {
        cmd.arg(Arg::new("foo").short('f').long("foo"))
            .arg(Arg::new("bar").short('b').long("bar"))
            .arg(Arg::new("quuz").short('q').long("quuz").takes_value(true))
    }
    fn augment_args_for_update(cmd: Command<'_>) -> Command<'_> {
        cmd.arg(Arg::new("foo").short('f').long("foo"))
            .arg(Arg::new("bar").short('b').long("bar"))
            .arg(Arg::new("quuz").short('q').long("quuz").takes_value(true))
    }
}

#[derive(Parser, Debug)]
struct Cli {
    #[clap(short, long)]
    top_level: bool,
    #[clap(flatten)]
    more_args: CliArgs,
}

fn main() {
    let args = Cli::parse();
    println!("{:#?}", args);
}
