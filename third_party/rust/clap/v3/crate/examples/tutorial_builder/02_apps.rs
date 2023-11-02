use clap::{arg, Command};

fn main() {
    let matches = Command::new("MyApp")
        .version("1.0")
        .author("Kevin K. <kbknapp@gmail.com>")
        .about("Does awesome things")
        .arg(arg!(--two <VALUE>))
        .arg(arg!(--one <VALUE>))
        .get_matches();

    println!("two: {:?}", matches.value_of("two").expect("required"));
    println!("one: {:?}", matches.value_of("one").expect("required"));
}
