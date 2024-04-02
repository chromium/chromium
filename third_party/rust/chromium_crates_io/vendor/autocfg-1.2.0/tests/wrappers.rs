extern crate autocfg;

use std::env;

/// Tests that autocfg uses the RUSTC_WRAPPER and/or RUSTC_WORKSPACE_WRAPPER
/// environment variables when running rustc.
#[test]
#[cfg(unix)] // we're using system binaries as wrappers
fn test_wrappers() {
    fn set(name: &str, value: Option<bool>) {
        match value {
            Some(true) => env::set_var(name, "/usr/bin/env"),
            Some(false) => env::set_var(name, "/bin/false"),
            None => env::remove_var(name),
        }
    }

    // Use the same path as this test binary.
    let dir = env::current_exe().unwrap().parent().unwrap().to_path_buf();
    env::set_var("OUT_DIR", &format!("{}", dir.display()));

    // This is used as a heuristic to detect rust-lang/cargo#9601.
    env::set_var("CARGO_ENCODED_RUSTFLAGS", "");

    // No wrapper, a good pass-through wrapper, and a bad wrapper.
    let variants = [None, Some(true), Some(false)];

    for &workspace in &variants {
        for &rustc in &variants {
            set("RUSTC_WRAPPER", rustc);
            set("RUSTC_WORKSPACE_WRAPPER", workspace);

            let ac = autocfg::AutoCfg::new().unwrap();
            if rustc == Some(false) || workspace == Some(false) {
                // Everything should fail with bad wrappers.
                assert!(!ac.probe_type("usize"));
            } else {
                // Try known good and bad types for the wrapped rustc.
                assert!(ac.probe_type("usize"));
                assert!(!ac.probe_type("mesize"));
            }
        }
    }

    // Finally, make sure that `RUSTC_WRAPPER` is applied outermost
    // by using something that doesn't pass through at all.
    env::set_var("RUSTC_WRAPPER", "/bin/true");
    env::set_var("RUSTC_WORKSPACE_WRAPPER", "/bin/false");
    let ac = autocfg::AutoCfg::new().unwrap();
    assert!(ac.probe_type("mesize")); // anything goes!
}
