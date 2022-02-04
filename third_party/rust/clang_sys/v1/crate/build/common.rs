// Copyright 2018 Kyle Mayes
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

extern crate glob;

use std::cell::RefCell;
use std::collections::HashMap;
use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

use glob::{MatchOptions, Pattern};

/// `libclang` directory patterns for FreeBSD and Linux.
const DIRECTORIES_LINUX: &[&str] = &[
    "/usr/lib*",
    "/usr/lib*/*",
    "/usr/lib*/*/*",
    "/usr/local/lib*",
    "/usr/local/lib*/*",
    "/usr/local/lib*/*/*",
    "/usr/local/llvm*/lib*",
];

/// `libclang` directory patterns for macOS.
const DIRECTORIES_MACOS: &[&str] = &[
    "/usr/local/opt/llvm*/lib",
    "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib",
    "/Library/Developer/CommandLineTools/usr/lib",
    "/usr/local/opt/llvm*/lib/llvm*/lib",
];

/// `libclang` directory patterns for Windows.
const DIRECTORIES_WINDOWS: &[&str] = &[
    "C:\\LLVM\\lib",
    "C:\\Program Files*\\LLVM\\lib",
    "C:\\MSYS*\\MinGW*\\lib",
    // LLVM + Clang can be installed as a component of Visual Studio.
    // https://github.com/KyleMayes/clang-sys/issues/121
    "C:\\Program Files*\\Microsoft Visual Studio\\*\\BuildTools\\VC\\Tools\\Llvm\\**\\bin",
    // Scoop user installation https://scoop.sh/.
    // Chocolatey, WinGet and other installers use to the system wide dir listed above
    "C:\\Users\\*\\scoop\\apps\\llvm\\current\\bin",
];

/// `libclang` directory patterns for Haiku.
const DIRECTORIES_HAIKU: &[&str] = &[
    "/boot/system/lib",
    "/boot/system/develop/lib",
    "/boot/system/non-packaged/lib",
    "/boot/system/non-packaged/develop/lib",
    "/boot/home/config/non-packaged/lib",
    "/boot/home/config/non-packaged/develop/lib",
];

thread_local! {
    /// The errors encountered when attempting to execute console commands.
    static COMMAND_ERRORS: RefCell<HashMap<String, Vec<String>>> = RefCell::default();
}

/// Executes the supplied console command, returning the `stdout` output if the
/// command was successfully executed.
fn run_command(name: &str, command: &str, arguments: &[&str]) -> Option<String> {
    macro_rules! error {
        ($error:expr) => {{
            COMMAND_ERRORS.with(|e| {
                e.borrow_mut()
                    .entry(name.into())
                    .or_insert_with(Vec::new)
                    .push(format!(
                        "couldn't execute `{} {}` ({})",
                        command,
                        arguments.join(" "),
                        $error,
                    ))
            });
        }};
    }

    let output = match Command::new(command).args(arguments).output() {
        Ok(output) => output,
        Err(error) => {
            error!(format!("error: {}", error));
            return None;
        }
    };

    if !output.status.success() {
        error!(format!("exit code: {}", output.status));
        return None;
    }

    Some(String::from_utf8_lossy(&output.stdout).into_owned())
}

/// Executes `llvm-config`, returning the `stdout` output if the command was
/// successfully executed.
pub fn run_llvm_config(arguments: &[&str]) -> Option<String> {
    let path = env::var("LLVM_CONFIG_PATH").unwrap_or_else(|_| "llvm-config".into());
    run_command("llvm-config", &path, arguments)
}

/// A struct that prints errors encountered when attempting to execute console
/// commands on drop if not discarded.
#[derive(Default)]
pub struct CommandErrorPrinter {
    discard: bool,
}

impl CommandErrorPrinter {
    pub fn discard(mut self) {
        self.discard = true;
    }
}

impl Drop for CommandErrorPrinter {
    fn drop(&mut self) {
        if self.discard {
            return;
        }

        let errors = COMMAND_ERRORS.with(|e| e.borrow().clone());

        if let Some(errors) = errors.get("llvm-config") {
            println!(
                "cargo:warning=could not execute `llvm-config` one or more \
                times, if the LLVM_CONFIG_PATH environment variable is set to \
                a full path to valid `llvm-config` executable it will be used \
                to try to find an instance of `libclang` on your system: {}",
                errors
                    .iter()
                    .map(|e| format!("\"{}\"", e))
                    .collect::<Vec<_>>()
                    .join("\n  "),
            )
        }

        if let Some(errors) = errors.get("xcode-select") {
            println!(
                "cargo:warning=could not execute `xcode-select` one or more \
                times, if a valid instance of this executable is on your PATH \
                it will be used to try to find an instance of `libclang` on \
                your system: {}",
                errors
                    .iter()
                    .map(|e| format!("\"{}\"", e))
                    .collect::<Vec<_>>()
                    .join("\n  "),
            )
        }
    }
}

/// Returns the paths to and the filenames of the files matching the supplied
/// filename patterns in the supplied directory.
fn search_directory(directory: &Path, filenames: &[String]) -> Vec<(PathBuf, String)> {
    // Escape the directory in case it contains characters that have special
    // meaning in glob patterns (e.g., `[` or `]`).
    let directory = Pattern::escape(directory.to_str().unwrap());
    let directory = Path::new(&directory);

    // Join the directory to the filename patterns to obtain the path patterns.
    let paths = filenames
        .iter()
        .map(|f| directory.join(f).to_str().unwrap().to_owned());

    // Prevent wildcards from matching path separators.
    let mut options = MatchOptions::new();
    options.require_literal_separator = true;

    paths
        .flat_map(|p| {
            if let Ok(paths) = glob::glob_with(&p, options) {
                paths.filter_map(Result::ok).collect()
            } else {
                vec![]
            }
        })
        .filter_map(|p| {
            let filename = p.file_name()?.to_str().unwrap();

            // The `libclang_shared` library has been renamed to `libclang-cpp`
            // in Clang 10. This can cause instances of this library (e.g.,
            // `libclang-cpp.so.10`) to be matched by patterns looking for
            // instances of `libclang`.
            if filename.contains("-cpp.") {
                return None;
            }

            Some((directory.to_owned(), filename.into()))
        })
        .collect::<Vec<_>>()
}

/// Returns the paths to and the filenames of the files matching the supplied
/// filename patterns in the supplied directory, checking any relevant sibling
/// directories.
fn search_directories(directory: &Path, filenames: &[String]) -> Vec<(PathBuf, String)> {
    let mut results = search_directory(directory, filenames);

    // On Windows, `libclang.dll` is usually found in the LLVM `bin` directory
    // while `libclang.lib` is usually found in the LLVM `lib` directory. To
    // keep things consistent with other platforms, only LLVM `lib` directories
    // are included in the backup search directory globs so we need to search
    // the LLVM `bin` directory here.
    if cfg!(target_os = "windows") && directory.ends_with("lib") {
        let sibling = directory.parent().unwrap().join("bin");
        results.extend(search_directory(&sibling, filenames).into_iter());
    }

    results
}

/// Returns the paths to and the filenames of the `libclang` static or dynamic
/// libraries matching the supplied filename patterns.
pub fn search_libclang_directories(files: &[String], variable: &str) -> Vec<(PathBuf, String)> {
    // Use the path provided by the relevant environment variable.
    if let Ok(path) = env::var(variable).map(|d| Path::new(&d).to_path_buf()) {
        // Check if the path is referring to a matching file already.
        if let Some(parent) = path.parent() {
            let filename = path.file_name().unwrap().to_str().unwrap();
            let libraries = search_directories(parent, files);
            if libraries.iter().any(|(_, f)| f == filename) {
                return vec![(parent.into(), filename.into())];
            }
        }

        return search_directories(&path, files);
    }

    let mut found = vec![];

    // Search the `bin` and `lib` directories in directory provided by
    // `llvm-config --prefix`.
    if let Some(output) = run_llvm_config(&["--prefix"]) {
        let directory = Path::new(output.lines().next().unwrap()).to_path_buf();
        found.extend(search_directories(&directory.join("bin"), files));
        found.extend(search_directories(&directory.join("lib"), files));
        found.extend(search_directories(&directory.join("lib64"), files));
    }

    // Search the toolchain directory in the directory provided by
    // `xcode-select --print-path`.
    if cfg!(target_os = "macos") {
        if let Some(output) = run_command("xcode-select", "xcode-select", &["--print-path"]) {
            let directory = Path::new(output.lines().next().unwrap()).to_path_buf();
            let directory = directory.join("Toolchains/XcodeDefault.xctoolchain/usr/lib");
            found.extend(search_directories(&directory, files));
        }
    }

    // Search the directories provided by the `LD_LIBRARY_PATH` environment
    // variable.
    if let Ok(path) = env::var("LD_LIBRARY_PATH") {
        for directory in env::split_paths(&path) {
            found.extend(search_directories(&directory, files));
        }
    }

    // Determine the `libclang` directory patterns.
    let directories = if cfg!(any(target_os = "freebsd", target_os = "linux")) {
        DIRECTORIES_LINUX
    } else if cfg!(target_os = "macos") {
        DIRECTORIES_MACOS
    } else if cfg!(target_os = "windows") {
        DIRECTORIES_WINDOWS
    } else if cfg!(target_os = "haiku") {
        DIRECTORIES_HAIKU
    } else {
        &[]
    };

    // Search the directories provided by the `libclang` directory patterns.
    let mut options = MatchOptions::new();
    options.case_sensitive = false;
    options.require_literal_separator = true;
    for directory in directories.iter().rev() {
        if let Ok(directories) = glob::glob_with(directory, options) {
            for directory in directories.filter_map(Result::ok).filter(|p| p.is_dir()) {
                found.extend(search_directories(&directory, files));
            }
        }
    }

    found
}
