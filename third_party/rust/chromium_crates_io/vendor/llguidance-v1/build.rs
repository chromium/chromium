use std::env;
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=llguidance.h");

    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let header_path = PathBuf::from(&crate_dir).join("llguidance.h");

    #[cfg(feature = "generate-header")]
    {
        println!("cargo:rerun-if-changed=cbindgen.toml");
        println!("cargo:rerun-if-changed=src");

        let config = cbindgen::Config::from_file(PathBuf::from(&crate_dir).join("cbindgen.toml"))
            .expect("Failed to read cbindgen.toml");

        let bindings = cbindgen::Builder::new()
            .with_crate(&crate_dir)
            .with_config(config)
            .generate()
            .expect("Failed to generate C bindings");

        let mut raw = Vec::new();
        bindings.write(&mut raw);
        let header = String::from_utf8(raw).unwrap();

        // Filter out Rust-specific doc comment lines
        let filtered: String = header
            .lines()
            .filter(|line| !line.contains("* # Safety"))
            .filter(|line| !line.contains("* This function should only be called from C code"))
            .map(|line| format!("{line}\n"))
            .collect();

        let existing = std::fs::read_to_string(&header_path).unwrap_or_default();
        if existing != filtered {
            std::fs::write(&header_path, &filtered).expect("Failed to write llguidance.h");
            println!("cargo:warning=Updated llguidance.h");
        }
    }

    // Place header alongside compiled library in target/{profile}/
    if let Ok(out_dir) = env::var("OUT_DIR") {
        let dest = PathBuf::from(out_dir)
            .ancestors()
            .nth(3)
            .expect(
                "unexpected OUT_DIR structure, expected target/{profile}/build/{crate}-{hash}/out",
            )
            .join("llguidance.h");
        std::fs::copy(&header_path, &dest)
            .unwrap_or_else(|e| panic!("Failed to copy llguidance.h to {}: {e}", dest.display()));
    }
}
