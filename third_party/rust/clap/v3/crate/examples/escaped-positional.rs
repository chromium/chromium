// Note: this requires the `cargo` feature

use clap::{arg, command};

fn main() {
    let matches = command!()
        .arg(arg!(eff: -f))
        .arg(arg!(pea: -p <PEAR>).required(false))
        .arg(
            arg!(slop: [SLOP]).multiple_occurrences(true).last(true), // Indicates that `slop` is only accessible after `--`.
        )
        .get_matches();

    // This is what will happen with `myprog -f -p=bob -- sloppy slop slop`...
    println!("-f used: {:?}", matches.is_present("eff")); // -f used: true
    println!("-p's value: {:?}", matches.value_of("pea")); // -p's value: Some("bob")
    println!(
        "'slops' values: {:?}",
        matches
            .values_of("slop")
            .map(|vals| vals.collect::<Vec<_>>())
            .unwrap_or_default()
    ); // 'slops' values: Some(["sloppy", "slop", "slop"])

    // Continued program logic goes here...
}
