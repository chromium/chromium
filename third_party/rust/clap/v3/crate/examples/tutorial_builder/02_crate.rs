// Note: this requires the `cargo` feature

use clap::{arg, command};

fn main() {
    let matches = command!()
        .arg(arg!(--two <VALUE>))
        .arg(arg!(--one <VALUE>))
        .get_matches();

    println!("two: {:?}", matches.value_of("two").expect("required"));
    println!("one: {:?}", matches.value_of("one").expect("required"));
}
