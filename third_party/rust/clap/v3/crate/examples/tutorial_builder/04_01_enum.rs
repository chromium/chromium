// Note: this requires the `cargo` feature

use clap::{arg, command, ArgEnum, PossibleValue};

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ArgEnum)]
enum Mode {
    Fast,
    Slow,
}

impl Mode {
    pub fn possible_values() -> impl Iterator<Item = PossibleValue<'static>> {
        Mode::value_variants()
            .iter()
            .filter_map(ArgEnum::to_possible_value)
    }
}

impl std::fmt::Display for Mode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.to_possible_value()
            .expect("no values are skipped")
            .get_name()
            .fmt(f)
    }
}

impl std::str::FromStr for Mode {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        for variant in Self::value_variants() {
            if variant.to_possible_value().unwrap().matches(s, false) {
                return Ok(*variant);
            }
        }
        Err(format!("Invalid variant: {}", s))
    }
}

fn main() {
    let matches = command!()
        .arg(
            arg!(<MODE>)
                .help("What mode to run the program in")
                .possible_values(Mode::possible_values()),
        )
        .get_matches();

    // Note, it's safe to call unwrap() because the arg is required
    match matches
        .value_of_t("MODE")
        .expect("'MODE' is required and parsing will fail if its missing")
    {
        Mode::Fast => {
            println!("Hare");
        }
        Mode::Slow => {
            println!("Tortoise");
        }
    }
}
