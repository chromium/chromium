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

use std::env;
use std::fs::File;
use std::io::{self, Error, ErrorKind, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};

use super::common;

/// Returns the ELF class from the ELF header in the supplied file.
fn parse_elf_header(path: &Path) -> io::Result<u8> {
    let mut file = File::open(path)?;
    let mut buffer = [0; 5];
    file.read_exact(&mut buffer)?;
    if buffer[..4] == [127, 69, 76, 70] {
        Ok(buffer[4])
    } else {
        Err(Error::new(ErrorKind::InvalidData, "invalid ELF header"))
    }
}

/// Returns the magic number from the PE header in the supplied file.
fn parse_pe_header(path: &Path) -> io::Result<u16> {
    let mut file = File::open(path)?;

    // Determine the header offset.
    let mut buffer = [0; 4];
    let start = SeekFrom::Start(0x3C);
    file.seek(start)?;
    file.read_exact(&mut buffer)?;
    let offset = i32::from_le_bytes(buffer);

    // Determine the validity of the header.
    file.seek(SeekFrom::Start(offset as u64))?;
    file.read_exact(&mut buffer)?;
    if buffer != [80, 69, 0, 0] {
        return Err(Error::new(ErrorKind::InvalidData, "invalid PE header"));
    }

    // Find the magic number.
    let mut buffer = [0; 2];
    file.seek(SeekFrom::Current(20))?;
    file.read_exact(&mut buffer)?;
    Ok(u16::from_le_bytes(buffer))
}

/// Validates the header for the supplied `libclang` shared library.
fn validate_header(path: &Path) -> Result<(), String> {
    if cfg!(any(target_os = "freebsd", target_os = "linux")) {
        let class = parse_elf_header(path).map_err(|e| e.to_string())?;

        if cfg!(target_pointer_width = "32") && class != 1 {
            return Err("invalid ELF class (64-bit)".into());
        }

        if cfg!(target_pointer_width = "64") && class != 2 {
            return Err("invalid ELF class (32-bit)".into());
        }

        Ok(())
    } else if cfg!(target_os = "windows") {
        let magic = parse_pe_header(path).map_err(|e| e.to_string())?;

        if cfg!(target_pointer_width = "32") && magic != 267 {
            return Err("invalid DLL (64-bit)".into());
        }

        if cfg!(target_pointer_width = "64") && magic != 523 {
            return Err("invalid DLL (32-bit)".into());
        }

        Ok(())
    } else {
        Ok(())
    }
}

/// Returns the components of the version in the supplied `libclang` shared
// library filename.
fn parse_version(filename: &str) -> Vec<u32> {
    let version = if let Some(version) = filename.strip_prefix("libclang.so.") {
        version
    } else if filename.starts_with("libclang-") {
        &filename[9..filename.len() - 3]
    } else {
        return vec![];
    };

    version.split('.').map(|s| s.parse().unwrap_or(0)).collect()
}

/// Returns the paths to, the filenames, and the versions of the `libclang`
// shared libraries.
fn search_libclang_directories(runtime: bool) -> Result<Vec<(PathBuf, String, Vec<u32>)>, String> {
    let mut files = vec![format!(
        "{}clang{}",
        env::consts::DLL_PREFIX,
        env::consts::DLL_SUFFIX
    )];

    if cfg!(target_os = "linux") {
        // Some Linux distributions don't create a `libclang.so` symlink, so we
        // need to look for versioned files (e.g., `libclang-3.9.so`).
        files.push("libclang-*.so".into());

        // Some Linux distributions don't create a `libclang.so` symlink and
        // don't have versioned files as described above, so we need to look for
        // suffix versioned files (e.g., `libclang.so.1`). However, `ld` cannot
        // link to these files, so this will only be included when linking at
        // runtime.
        if runtime {
            files.push("libclang.so.*".into());
            files.push("libclang-*.so.*".into());
        }
    }

    if cfg!(any(
        target_os = "openbsd",
        target_os = "freebsd",
        target_os = "netbsd",
        target_os = "haiku"
    )) {
        // Some BSD distributions don't create a `libclang.so` symlink either,
        // but use a different naming scheme for versioned files (e.g.,
        // `libclang.so.7.0`).
        files.push("libclang.so.*".into());
    }

    if cfg!(target_os = "windows") {
        // The official LLVM build uses `libclang.dll` on Windows instead of
        // `clang.dll`. However, unofficial builds such as MinGW use `clang.dll`.
        files.push("libclang.dll".into());
    }

    // Validate the `libclang` shared libraries and collect the versions.
    let mut valid = vec![];
    let mut invalid = vec![];
    for (directory, filename) in common::search_libclang_directories(&files, "LIBCLANG_PATH") {
        let path = directory.join(&filename);
        match validate_header(&path) {
            Ok(()) => {
                let version = parse_version(&filename);
                valid.push((directory, filename, version))
            }
            Err(message) => invalid.push(format!("({}: {})", path.display(), message)),
        }
    }

    if !valid.is_empty() {
        return Ok(valid);
    }

    let message = format!(
        "couldn't find any valid shared libraries matching: [{}], set the \
         `LIBCLANG_PATH` environment variable to a path where one of these files \
         can be found (invalid: [{}])",
        files
            .iter()
            .map(|f| format!("'{}'", f))
            .collect::<Vec<_>>()
            .join(", "),
        invalid.join(", "),
    );

    Err(message)
}

/// Returns the directory and filename of the "best" available `libclang` shared
/// library.
pub fn find(runtime: bool) -> Result<(PathBuf, String), String> {
    search_libclang_directories(runtime)?
        .iter()
        // We want to find the `libclang` shared library with the highest
        // version number, hence `max_by_key` below.
        //
        // However, in the case where there are multiple such `libclang` shared
        // libraries, we want to use the order in which they appeared in the
        // list returned by `search_libclang_directories` as a tiebreaker since
        // that function returns `libclang` shared libraries in descending order
        // of preference by how they were found.
        //
        // `max_by_key`, perhaps surprisingly, returns the *last* item with the
        // maximum key rather than the first which results in the opposite of
        // the tiebreaking behavior we want. This is easily fixed by reversing
        // the list first.
        .rev()
        .max_by_key(|f| &f.2)
        .cloned()
        .map(|(path, filename, _)| (path, filename))
        .ok_or_else(|| "unreachable".into())
}

/// Find and link to `libclang` dynamically.
#[cfg(not(feature = "runtime"))]
pub fn link() {
    let cep = common::CommandErrorPrinter::default();

    use std::fs;

    let (directory, filename) = find(false).unwrap();
    println!("cargo:rustc-link-search={}", directory.display());

    if cfg!(all(target_os = "windows", target_env = "msvc")) {
        // Find the `libclang` stub static library required for the MSVC
        // toolchain.
        let lib = if !directory.ends_with("bin") {
            directory
        } else {
            directory.parent().unwrap().join("lib")
        };

        if lib.join("libclang.lib").exists() {
            println!("cargo:rustc-link-search={}", lib.display());
        } else if lib.join("libclang.dll.a").exists() {
            // MSYS and MinGW use `libclang.dll.a` instead of `libclang.lib`.
            // It is linkable with the MSVC linker, but Rust doesn't recognize
            // the `.a` suffix, so we need to copy it with a different name.
            //
            // FIXME: Maybe we can just hardlink or symlink it?
            let out = env::var("OUT_DIR").unwrap();
            fs::copy(
                lib.join("libclang.dll.a"),
                Path::new(&out).join("libclang.lib"),
            )
            .unwrap();
            println!("cargo:rustc-link-search=native={}", out);
        } else {
            panic!(
                "using '{}', so 'libclang.lib' or 'libclang.dll.a' must be \
                 available in {}",
                filename,
                lib.display(),
            );
        }

        println!("cargo:rustc-link-lib=dylib=libclang");
    } else {
        let name = filename.trim_start_matches("lib");

        // Strip extensions and trailing version numbers (e.g., the `.so.7.0` in
        // `libclang.so.7.0`).
        let name = match name.find(".dylib").or_else(|| name.find(".so")) {
            Some(index) => &name[0..index],
            None => name,
        };

        println!("cargo:rustc-link-lib=dylib={}", name);
    }

    cep.discard();
}
