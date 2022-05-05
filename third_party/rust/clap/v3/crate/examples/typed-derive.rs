// Note: this requires the `derive` feature

use clap::Parser;
use std::error::Error;

#[derive(Parser, Debug)]
struct Args {
    /// Implicitly using `std::str::FromStr`
    #[clap(short = 'O')]
    optimization: Option<usize>,

    /// Allow invalid UTF-8 paths
    #[clap(short = 'I', parse(from_os_str), value_name = "DIR", value_hint = clap::ValueHint::DirPath)]
    include: Option<std::path::PathBuf>,

    /// Handle IP addresses
    #[clap(long)]
    bind: Option<std::net::IpAddr>,

    /// Allow human-readable durations
    #[clap(long)]
    sleep: Option<humantime::Duration>,

    /// Hand-written parser for tuples
    #[clap(short = 'D', parse(try_from_str = parse_key_val), multiple_occurrences(true))]
    defines: Vec<(String, i32)>,
}

/// Parse a single key-value pair
fn parse_key_val<T, U>(s: &str) -> Result<(T, U), Box<dyn Error + Send + Sync + 'static>>
where
    T: std::str::FromStr,
    T::Err: Error + Send + Sync + 'static,
    U: std::str::FromStr,
    U::Err: Error + Send + Sync + 'static,
{
    let pos = s
        .find('=')
        .ok_or_else(|| format!("invalid KEY=value: no `=` found in `{}`", s))?;
    Ok((s[..pos].parse()?, s[pos + 1..].parse()?))
}

fn main() {
    let args = Args::parse();
    println!("{:?}", args);
}
