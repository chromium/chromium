// Note: this requires the `cargo` feature

use std::ops::RangeInclusive;

use clap::{arg, command};

fn main() {
    let matches = command!()
        .arg(
            arg!(<PORT>)
                .help("Network port to use")
                .validator(port_in_range),
        )
        .get_matches();

    // Note, it's safe to call unwrap() because the arg is required
    let port: usize = matches
        .value_of_t("PORT")
        .expect("'PORT' is required and parsing will fail if its missing");
    println!("PORT = {}", port);
}

const PORT_RANGE: RangeInclusive<usize> = 1..=65535;

fn port_in_range(s: &str) -> Result<(), String> {
    let port: usize = s
        .parse()
        .map_err(|_| format!("`{}` isn't a port number", s))?;
    if PORT_RANGE.contains(&port) {
        Ok(())
    } else {
        Err(format!(
            "Port not in range {}-{}",
            PORT_RANGE.start(),
            PORT_RANGE.end()
        ))
    }
}
