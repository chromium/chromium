use std::env::var;

fn main() {
    if var("CARGO_FEATURE_DEFLATE_MINIZ").is_ok() && var("CARGO_FEATURE__ALL_FEATURES").is_err() {
        println!("cargo:warning=Feature `deflate-miniz` is deprecated; replace it with `deflate`");
    }
}
