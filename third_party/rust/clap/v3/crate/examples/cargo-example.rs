// Note: this requires the `cargo` feature

fn main() {
    let cmd = clap::Command::new("cargo")
        .bin_name("cargo")
        .subcommand_required(true)
        .subcommand(
            clap::command!("example").arg(
                clap::arg!(--"manifest-path" <PATH>)
                    .required(false)
                    .allow_invalid_utf8(true),
            ),
        );
    let matches = cmd.get_matches();
    let matches = match matches.subcommand() {
        Some(("example", matches)) => matches,
        _ => unreachable!("clap should ensure we don't get here"),
    };
    let manifest_path = matches
        .value_of_os("manifest-path")
        .map(std::path::PathBuf::from);
    println!("{:?}", manifest_path);
}
