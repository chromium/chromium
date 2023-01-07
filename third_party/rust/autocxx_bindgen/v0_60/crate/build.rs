mod target {
    use std::env;
    use std::fs::File;
    use std::io::Write;
    use std::path::{Path, PathBuf};

    pub fn main() {
        let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());

        let mut dst =
            File::create(Path::new(&out_dir).join("host-target.txt")).unwrap();
        dst.write_all(env::var("TARGET").unwrap().as_bytes())
            .unwrap();
    }
}

mod testgen {
    use std::char;
    use std::env;
    use std::ffi::OsStr;
    use std::fs::{self, File};
    use std::io::Write;
    use std::path::{Path, PathBuf};

    pub fn main() {
        let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
        let mut dst =
            File::create(Path::new(&out_dir).join("tests.rs")).unwrap();

        let manifest_dir =
            PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        let headers_dir = manifest_dir.join("tests").join("headers");

        let headers = match fs::read_dir(headers_dir) {
            Ok(dir) => dir,
            // We may not have headers directory after packaging.
            Err(..) => return,
        };

        let entries =
            headers.map(|result| result.expect("Couldn't read header file"));

        println!("cargo:rerun-if-changed=tests/headers");

        for entry in entries {
            match entry.path().extension().and_then(OsStr::to_str) {
                Some("h") | Some("hpp") => {
                    let func = entry
                        .file_name()
                        .to_str()
                        .unwrap()
                        .replace(|c| !char::is_alphanumeric(c), "_")
                        .replace("__", "_")
                        .to_lowercase();
                    writeln!(
                        dst,
                        "test_header!(header_{}, {:?});",
                        func,
                        entry.path(),
                    )
                    .unwrap();
                }
                _ => {}
            }
        }

        dst.flush().unwrap();
    }
}

fn main() {
    target::main();
    testgen::main();

    // On behalf of clang_sys, rebuild ourselves if important configuration
    // variables change, to ensure that bindings get rebuilt if the
    // underlying libclang changes.
    println!("cargo:rerun-if-env-changed=LLVM_CONFIG_PATH");
    println!("cargo:rerun-if-env-changed=LIBCLANG_PATH");
    println!("cargo:rerun-if-env-changed=LIBCLANG_STATIC_PATH");
    println!("cargo:rerun-if-env-changed=BINDGEN_EXTRA_CLANG_ARGS");
    println!(
        "cargo:rerun-if-env-changed=BINDGEN_EXTRA_CLANG_ARGS_{}",
        std::env::var("TARGET").unwrap()
    );
    println!(
        "cargo:rerun-if-env-changed=BINDGEN_EXTRA_CLANG_ARGS_{}",
        std::env::var("TARGET").unwrap().replace('-', "_")
    );
}
