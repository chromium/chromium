use rustc_version::{Version, version};

fn main() {
    let ver = version().unwrap();
    assert!(ver.major >= 1);

    if ver <= Version::parse("1.50.0").unwrap() {
        println!("cargo:rustc-cfg=sanitize_multiple_should_panic_compiler_bug");
    }
}
