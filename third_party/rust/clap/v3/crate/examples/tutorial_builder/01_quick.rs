// Note: this requires the `cargo` feature

use clap::{arg, command, Command};
use std::path::Path;

fn main() {
    let matches = command!()
        .arg(arg!([name] "Optional name to operate on"))
        .arg(
            arg!(
                -c --config <FILE> "Sets a custom config file"
            )
            // We don't have syntax yet for optional options, so manually calling `required`
            .required(false)
            // Support non-UTF8 paths
            .allow_invalid_utf8(true),
        )
        .arg(arg!(
            -d --debug ... "Turn debugging information on"
        ))
        .subcommand(
            Command::new("test")
                .about("does testing things")
                .arg(arg!(-l --list "lists test values")),
        )
        .get_matches();

    // You can check the value provided by positional arguments, or option arguments
    if let Some(name) = matches.value_of("name") {
        println!("Value for name: {}", name);
    }

    if let Some(raw_config) = matches.value_of_os("config") {
        let config_path = Path::new(raw_config);
        println!("Value for config: {}", config_path.display());
    }

    // You can see how many times a particular flag or argument occurred
    // Note, only flags can have multiple occurrences
    match matches.occurrences_of("debug") {
        0 => println!("Debug mode is off"),
        1 => println!("Debug mode is kind of on"),
        2 => println!("Debug mode is on"),
        _ => println!("Don't be crazy"),
    }

    // You can check for the existence of subcommands, and if found use their
    // matches just as you would the top level cmd
    if let Some(matches) = matches.subcommand_matches("test") {
        // "$ myapp test" was run
        if matches.is_present("list") {
            // "$ myapp test -l" was run
            println!("Printing testing lists...");
        } else {
            println!("Not printing testing lists...");
        }
    }

    // Continued program logic goes here...
}
