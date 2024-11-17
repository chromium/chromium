use rustc_version::{version, version_meta, Channel};

fn allow_features() -> Option<Vec<String>> {
    std::env::var("CARGO_ENCODED_RUSTFLAGS").ok().map(|args| {
        args.split('\u{001f}')
            .filter(|arg| arg.starts_with("-Zallow-features="))
            .map(|arg| arg.split('=').nth(1).unwrap())
            .flat_map(|features| features.split(','))
            .map(|f| f.to_owned())
            .collect()
    })
}

fn can_enable_proc_macro_diagnostic() -> bool {
    allow_features()
        .map(|f| f.iter().any(|f| f == "proc_macro_diagnostic"))
        .unwrap_or(true)
}

fn main() {
    let ver = version().unwrap();
    assert!(ver.major >= 1);

    match version_meta().unwrap().channel {
        Channel::Nightly | Channel::Dev if can_enable_proc_macro_diagnostic() => {
            println!("cargo:rustc-cfg=use_proc_macro_diagnostic");
        }
        _ => {}
    }
}
