use std::env;

fn main() {
    // Chromium modification - don't check the Android version here to avoid
    // depending on cc. It was only ever used to determine whether to print
    // this... if API version ≥21. Chromium only supports 21+ anyway.
    println!("cargo:rustc-cfg=feature=\"dl_iterate_phdr\"");
}

