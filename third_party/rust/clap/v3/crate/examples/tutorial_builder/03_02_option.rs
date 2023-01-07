// Note: this requires the `cargo` feature

use clap::{arg, command};

fn main() {
    let matches = command!()
        .arg(arg!(-n --name <NAME>).required(false))
        .get_matches();

    println!("name: {:?}", matches.value_of("name"));
}
