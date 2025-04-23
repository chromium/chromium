use std::env;

fn main() {
    println!("cargo:rerun-if-changed=llguidance.h");

    let required_vars = [
        "CARGO",
        "CARGO_MANIFEST_PATH",
        "CARGO_PKG_NAME",
        "CARGO_PKG_VERSION",
        "OUT_DIR",
    ];

    let missing: Vec<_> = required_vars
        .iter()
        .filter(|&&var| env::var(var).is_err())
        .collect();

    if !missing.is_empty() {
        // this means we're running outside of cargo
        println!("cargo:warning=Missing variables: {:?}", missing);
    } else {
        // otherwise, assume OUT_DIR is target/debug/build/llguidance-<hash>/out
        let copy_path = format!("{}/../../../llguidance.h", env::var("OUT_DIR").unwrap());
        std::fs::copy("llguidance.h", copy_path).unwrap();
    }
}
