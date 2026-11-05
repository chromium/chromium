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

    /// Repository-local git environment variables.
    ///
    /// This is the list printed by `git rev-parse --local-env-vars`. When cargo
    /// runs inside a git hook (e.g. a pre-commit hook running `cargo check`),
    /// git exports some of these, such as `GIT_DIR` and `GIT_INDEX_FILE`,
    /// pointing at the enclosing repository. If our `git` commands inherited
    /// them they would operate on that repository instead, for instance writing
    /// brotli's tree into its index.
    const GIT_LOCAL_ENV_VARS: &[&str] = &[
        "GIT_ALTERNATE_OBJECT_DIRECTORIES",
        "GIT_CONFIG",
        "GIT_CONFIG_PARAMETERS",
        "GIT_CONFIG_COUNT",
        "GIT_OBJECT_DIRECTORY",
        "GIT_DIR",
        "GIT_WORK_TREE",
        "GIT_IMPLICIT_WORK_TREE",
        "GIT_GRAFT_FILE",
        "GIT_INDEX_FILE",
        "GIT_NO_REPLACE_OBJECTS",
        "GIT_REPLACE_REF_BASE",
        "GIT_PREFIX",
        "GIT_SHALLOW_FILE",
        "GIT_COMMON_DIR",
    ];

    pub fn build_brotli() {
        let out_dir = PathBuf::from(std::env::var("OUT_DIR").unwrap());
        let brotli_dir = out_dir.join("brotli");

        if !brotli_dir.exists() {
            // Clone into a temporary directory and only move it into place once
            // the checkout has succeeded, so that a failed or interrupted build
            // can't leave behind a partial clone that later builds would trust.
            let tmp_dir = out_dir.join("brotli.tmp");
            if tmp_dir.exists() {
                std::fs::remove_dir_all(&tmp_dir).expect("Failed to remove stale brotli clone");
            }

            let status = git()
                .args([
                    "clone",
                    "https://github.com/google/brotli.git",
                    tmp_dir.to_str().unwrap(),
                ])
                .status()
                .expect("Failed to execute git clone");

            if !status.success() {
                panic!("Failed to clone brotli repository");
            }

            let status = git()
                .args(["checkout", "1736fe1d50b205f718281a94035fd66f14d72ef5"])
                .current_dir(&tmp_dir)
                .status()
                .expect("Failed to execute git checkout");

            if !status.success() {
                panic!("Failed to checkout specific commit");
            }

            std::fs::rename(&tmp_dir, &brotli_dir).expect("Failed to move brotli clone into place");
        }

        compile_brotli(&brotli_dir);
    }

    /// Create a `git` command that ignores any repository set in the environment.
    fn git() -> Command {
        let mut cmd = Command::new("git");
        for var in GIT_LOCAL_ENV_VARS {
            cmd.env_remove(var);
        }
        cmd
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
