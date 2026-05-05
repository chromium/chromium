// Note: We can rely on `brotlic-sys` once https://github.com/AronParker/brotlic/pull/5 is released
// and merged.
fn main() {
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    println!("cargo:rerun-if-changed={}/build.rs", manifest_dir);

    if std::env::var("DOCS_RS").is_ok() {
        return;
    }

    #[cfg(feature = "c-brotli")]
    c_brotli::build_brotli();
}

#[cfg(feature = "c-brotli")]
mod c_brotli {
    use std::path::{Path, PathBuf};
    use std::process::Command;

    pub fn build_brotli() {
        let out_dir = PathBuf::from(std::env::var("OUT_DIR").unwrap());
        let brotli_dir = out_dir.join("brotli");

        if !brotli_dir.exists() {
            let status = Command::new("git")
                .args([
                    "clone",
                    "--depth",
                    "1",
                    "https://github.com/google/brotli.git",
                    brotli_dir.to_str().unwrap(),
                ])
                .status()
                .expect("Failed to execute git clone");

            if !status.success() {
                panic!("Failed to clone brotli repository");
            }
        }

        compile_brotli(&brotli_dir);
    }

    fn compile_brotli(brotli_dir: &Path) {
        cc::Build::new()
            .include(brotli_dir.join("c/include"))
            .files([
                brotli_dir.join("c/common/constants.c"),
                brotli_dir.join("c/common/context.c"),
                brotli_dir.join("c/common/dictionary.c"),
                brotli_dir.join("c/common/platform.c"),
                brotli_dir.join("c/common/shared_dictionary.c"),
                brotli_dir.join("c/common/transform.c"),
                brotli_dir.join("c/dec/bit_reader.c"),
                brotli_dir.join("c/dec/decode.c"),
                brotli_dir.join("c/dec/huffman.c"),
                brotli_dir.join("c/dec/state.c"),
                brotli_dir.join("c/dec/prefix.c"),
                brotli_dir.join("c/dec/static_init.c"),
            ])
            .compile("brotli");
    }
}
