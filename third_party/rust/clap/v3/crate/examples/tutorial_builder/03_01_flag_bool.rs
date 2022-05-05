// Note: this requires the `cargo` feature

use clap::{arg, command};

fn main() {
    let matches = command!().arg(arg!(-v - -verbose)).get_matches();

    println!("verbose: {:?}", matches.is_present("verbose"));
}
