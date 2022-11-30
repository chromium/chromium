use clap::{AppSettings, Parser};

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
#[clap(args_override_self = true)]
#[clap(allow_negative_numbers = true)]
#[clap(global_setting(AppSettings::DeriveDisplayOrder))]
struct Cli {
    #[clap(long)]
    two: String,
    #[clap(long)]
    one: String,
}

fn main() {
    let cli = Cli::parse();

    println!("two: {:?}", cli.two);
    println!("one: {:?}", cli.one);
}
