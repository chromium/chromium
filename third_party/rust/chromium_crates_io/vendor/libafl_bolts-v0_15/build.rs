#[rustversion::nightly]
fn nightly() {
    println!("cargo:rustc-cfg=nightly");
}

#[rustversion::not(nightly)]
fn nightly() {}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rustc-check-cfg=cfg(nightly)");
    nightly();
    #[cfg(target_env = "musl")]
    println!("cargo:rustc-link-lib=ucontext");
}
