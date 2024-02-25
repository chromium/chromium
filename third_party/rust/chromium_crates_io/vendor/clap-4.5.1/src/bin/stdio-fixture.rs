fn main() {
    #[allow(unused_mut)]
    let mut cmd = clap::Command::new("stdio-fixture")
        .version("1.0")
        .long_version("1.0 - a2132c")
        .arg_required_else_help(true)
        .subcommand(clap::Command::new("more"))
        .arg(
            clap::Arg::new("verbose")
                .long("verbose")
                .help("log")
                .action(clap::ArgAction::SetTrue)
                .long_help("more log"),
        );
    #[cfg(feature = "color")]
    {
        use clap::builder::styling;
        let styles = styling::Styles::styled()
            .header(styling::AnsiColor::Green.on_default() | styling::Effects::BOLD)
            .usage(styling::AnsiColor::Green.on_default() | styling::Effects::BOLD)
            .literal(styling::AnsiColor::Blue.on_default() | styling::Effects::BOLD)
            .placeholder(styling::AnsiColor::Cyan.on_default());
        cmd = cmd.styles(styles);
    }
    cmd.get_matches();
}
