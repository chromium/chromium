// Note: this requires the `cargo` feature

use clap::{arg, command};

fn main() {
    let matches = command!()
        .arg(
            arg!(<PORT>)
                .help("Network port to use")
                .validator(|s| s.parse::<usize>()),
        )
        .get_matches();

    // Note, it's safe to call unwrap() because the arg is required
    let port: usize = matches
        .value_of_t("PORT")
        .expect("'PORT' is required and parsing will fail if its missing");
    println!("PORT = {}", port);
}
