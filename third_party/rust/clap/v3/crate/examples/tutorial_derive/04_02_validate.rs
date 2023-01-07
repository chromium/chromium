use std::ops::RangeInclusive;

use clap::Parser;

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
struct Cli {
    /// Network port to use
    #[clap(parse(try_from_str=port_in_range))]
    port: usize,
}

fn main() {
    let cli = Cli::parse();

    println!("PORT = {}", cli.port);
}

const PORT_RANGE: RangeInclusive<usize> = 1..=65535;

fn port_in_range(s: &str) -> Result<usize, String> {
    let port: usize = s
        .parse()
        .map_err(|_| format!("`{}` isn't a port number", s))?;
    if PORT_RANGE.contains(&port) {
        Ok(port)
    } else {
        Err(format!(
            "Port not in range {}-{}",
            PORT_RANGE.start(),
            PORT_RANGE.end()
        ))
    }
}
