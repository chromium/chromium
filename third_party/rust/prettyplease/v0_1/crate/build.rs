fn main() {
    println!(concat!("cargo:VERSION=", env!("CARGO_PKG_VERSION")));
}
