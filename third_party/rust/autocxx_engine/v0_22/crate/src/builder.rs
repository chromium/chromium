// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use autocxx_parser::file_locations::FileLocationStrategy;
use miette::Diagnostic;
use thiserror::Error;

use crate::generate_rs_single;
use crate::{strip_system_headers, CppCodegenOptions, ParseError, RebuildDependencyRecorder};
use std::ffi::OsStr;
use std::ffi::OsString;
use std::fs::File;
use std::io::Write;
use std::marker::PhantomData;
use std::path::{Path, PathBuf};

/// Errors returned during creation of a [`cc::Build`] from an include_cxx
/// macro.
#[derive(Error, Diagnostic, Debug)]
#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub enum BuilderError {
    #[error("cxx couldn't handle our generated bindings - could be a bug in autocxx: {0}")]
    InvalidCxx(cxx_gen::Error),
    #[error(transparent)]
    #[diagnostic(transparent)]
    ParseError(ParseError),
    #[error("we couldn't write the generated code to disk at {1}: {0}")]
    FileWriteFail(std::io::Error, PathBuf),
    #[error("no include_cpp! macro was found")]
    NoIncludeCxxMacrosFound,
    #[error("could not create a directory {1}: {0}")]
    UnableToCreateDirectory(std::io::Error, PathBuf),
}

#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub type BuilderBuild = cc::Build;

/// For test purposes only, a [`cc::Build`] and lists of Rust and C++
/// files generated.
#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub struct BuilderSuccess(pub BuilderBuild, pub Vec<PathBuf>, pub Vec<PathBuf>);

/// Results of a build.
#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub type BuilderResult = Result<BuilderSuccess, BuilderError>;

/// The context in which a builder object lives. Callbacks for various
/// purposes.
#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub trait BuilderContext {
    /// Perform any initialization specific to the context in which this
    /// builder lives.
    fn setup() {}

    /// Create a dependency recorder, if any.
    fn get_dependency_recorder() -> Option<Box<dyn RebuildDependencyRecorder>>;
}

/// An object to allow building of bindings from a `build.rs` file.
///
/// It would be unusual to use this directly - see the `autocxx_build` or
/// `autocxx_gen` crates.
#[cfg_attr(feature = "nightly", doc(cfg(feature = "build")))]
pub struct Builder<'a, BuilderContext> {
    rs_file: PathBuf,
    autocxx_incs: Vec<OsString>,
    extra_clang_args: Vec<String>,
    dependency_recorder: Option<Box<dyn RebuildDependencyRecorder>>,
    custom_gendir: Option<PathBuf>,
    auto_allowlist: bool,
    cpp_codegen_options: CppCodegenOptions<'a>,
    // This member is to ensure that this type is parameterized
    // by a BuilderContext. The goal is to balance three needs:
    // (1) have most of the functionality over in autocxx_engine,
    // (2) expose this type to users of autocxx_build and to
    //     make it easy for callers simply to call Builder::new,
    // (3) ensure that such a Builder does a few tasks specific to its use
    // in a cargo environment.
    ctx: PhantomData<BuilderContext>,
}

impl<CTX: BuilderContext> Builder<'_, CTX> {
    /// Create a new Builder object. You'll need to pass in the Rust file
    /// which contains the bindings (typically an `include_cpp!` macro
    /// though `autocxx` can also handle manually-crafted `cxx::bridge`
    /// bindings), and a list of include directories which should be searched
    /// by autocxx as it tries to hunt for the include files specified
    /// within the `include_cpp!` macro.
    ///
    /// Usually after this you'd call [`build`].
    pub fn new(
        rs_file: impl AsRef<Path>,
        autocxx_incs: impl IntoIterator<Item = impl AsRef<OsStr>>,
    ) -> Self {
        CTX::setup();
        Self {
            rs_file: rs_file.as_ref().to_path_buf(),
            autocxx_incs: autocxx_incs
                .into_iter()
                .map(|s| s.as_ref().to_os_string())
                .collect(),
            extra_clang_args: Vec::new(),
            dependency_recorder: CTX::get_dependency_recorder(),
            custom_gendir: None,
            auto_allowlist: false,
            cpp_codegen_options: CppCodegenOptions::default(),
            ctx: PhantomData,
        }
    }

    /// Specify extra arguments for clang.
    pub fn extra_clang_args(mut self, extra_clang_args: &[&str]) -> Self {
        self.extra_clang_args = extra_clang_args.iter().map(|s| s.to_string()).collect();
        self
    }

    /// Where to generate the code.
    pub fn custom_gendir(mut self, custom_gendir: PathBuf) -> Self {
        self.custom_gendir = Some(custom_gendir);
        self
    }

    /// Update C++ code generation options. See [`CppCodegenOptions`] for details.
    pub fn cpp_codegen_options<F>(mut self, modifier: F) -> Self
    where
        F: FnOnce(&mut CppCodegenOptions),
    {
        modifier(&mut self.cpp_codegen_options);
        self
    }

    /// Automatically discover uses of the C++ `ffi` mod and generate the allowlist
    /// from that.
    /// This is a highly experimental option, not currently recommended.
    /// It doesn't work in the following cases:
    /// * Static function calls on types within the FFI mod.
    /// * Anything inside a macro invocation.
    /// * You're using a different name for your `ffi` mod
    /// * You're using multiple FFI mods
    /// * You've got usages scattered across files beyond that with the
    ///   `include_cpp` invocation
    /// * You're using `use` statements to rename mods or items. If this
    /// proves to be a promising or helpful direction, autocxx would be happy
    /// to accept pull requests to remove some of these limitations.
    pub fn auto_allowlist(mut self, do_it: bool) -> Self {
        self.auto_allowlist = do_it;
        self
    }

    /// Whether to suppress inclusion of system headers (`memory`, `string` etc.)
    /// from generated C++ bindings code. This should not normally be used,
    /// but can occasionally be useful if you're reducing a test case and you
    /// have a preprocessed header file which already contains absolutely everything
    /// that the bindings could ever need.
    pub fn suppress_system_headers(mut self, do_it: bool) -> Self {
        self.cpp_codegen_options.suppress_system_headers = do_it;
        self
    }

    /// An annotation optionally to include on each C++ function.
    /// For example to export the symbol from a library.
    pub fn cxx_impl_annotations(mut self, cxx_impl_annotations: Option<String>) -> Self {
        self.cpp_codegen_options.cxx_impl_annotations = cxx_impl_annotations;
        self
    }

    /// Build autocxx C++ files and return a [`cc::Build`] you can use to build
    /// more from a build.rs file.
    ///
    /// The error type returned by this function supports [`miette::Diagnostic`],
    /// so if you use the `miette` crate and its `fancy` feature, then simply
    /// return a `miette::Result` from your main function, you should get nicely
    /// printed diagnostics.
    pub fn build(self) -> Result<BuilderBuild, BuilderError> {
        self.build_listing_files().map(|r| r.0)
    }

    /// For use in tests only, this does the build and returns additional information
    /// about the files generated which can subsequently be examined for correctness.
    /// In production, please use simply [`build`].
    pub fn build_listing_files(self) -> Result<BuilderSuccess, BuilderError> {
        let clang_args = &self
            .extra_clang_args
            .iter()
            .map(|s| &s[..])
            .collect::<Vec<_>>();
        rust_version_check();
        let gen_location_strategy = match self.custom_gendir {
            None => FileLocationStrategy::new(),
            Some(custom_dir) => FileLocationStrategy::Custom(custom_dir),
        };
        let incdir = gen_location_strategy.get_include_dir();
        ensure_created(&incdir)?;
        let cxxdir = gen_location_strategy.get_cxx_dir();
        ensure_created(&cxxdir)?;
        let rsdir = gen_location_strategy.get_rs_dir();
        ensure_created(&rsdir)?;
        // We are incredibly unsophisticated in our directory arrangement here
        // compared to cxx. I have no doubt that we will need to replicate just
        // about everything cxx does, in due course...
        // Write cxx.h to that location, as it may be needed by
        // some of our generated code.
        write_to_file(
            &incdir,
            "cxx.h",
            &Self::get_cxx_header_bytes(self.cpp_codegen_options.suppress_system_headers),
        )?;

        let autocxx_inc = build_autocxx_inc(self.autocxx_incs, &incdir);
        gen_location_strategy.set_cargo_env_vars_for_build();

        let mut parsed_file = crate::parse_file(self.rs_file, self.auto_allowlist)
            .map_err(BuilderError::ParseError)?;
        parsed_file
            .resolve_all(
                autocxx_inc,
                clang_args,
                self.dependency_recorder,
                &self.cpp_codegen_options,
            )
            .map_err(BuilderError::ParseError)?;
        let mut counter = 0;
        let mut builder = cc::Build::new();
        builder.cpp(true);
        if std::env::var_os("AUTOCXX_ASAN").is_some() {
            builder.flag_if_supported("-fsanitize=address");
        }
        let mut generated_rs = Vec::new();
        let mut generated_cpp = Vec::new();
        builder.includes(parsed_file.include_dirs());
        for include_cpp in parsed_file.get_cpp_buildables() {
            let generated_code = include_cpp
                .generate_h_and_cxx(&self.cpp_codegen_options)
                .map_err(BuilderError::InvalidCxx)?;
            for filepair in generated_code.0 {
                let fname = format!("gen{}.cxx", counter);
                counter += 1;
                if let Some(implementation) = &filepair.implementation {
                    let gen_cxx_path = write_to_file(&cxxdir, &fname, implementation)?;
                    builder.file(&gen_cxx_path);
                    generated_cpp.push(gen_cxx_path);
                }
                write_to_file(&incdir, &filepair.header_name, &filepair.header)?;
                generated_cpp.push(incdir.join(filepair.header_name));
            }
        }

        for rs_output in parsed_file.get_rs_outputs() {
            let rs = generate_rs_single(rs_output);
            generated_rs.push(write_to_file(&rsdir, &rs.filename, rs.code.as_bytes())?);
        }
        if counter == 0 {
            Err(BuilderError::NoIncludeCxxMacrosFound)
        } else {
            Ok(BuilderSuccess(builder, generated_rs, generated_cpp))
        }
    }

    fn get_cxx_header_bytes(suppress_system_headers: bool) -> Vec<u8> {
        strip_system_headers(crate::HEADER.as_bytes().to_vec(), suppress_system_headers)
    }
}

fn ensure_created(dir: &Path) -> Result<(), BuilderError> {
    std::fs::create_dir_all(dir)
        .map_err(|e| BuilderError::UnableToCreateDirectory(e, dir.to_path_buf()))
}

fn build_autocxx_inc<I, T>(paths: I, extra_path: &Path) -> Vec<PathBuf>
where
    I: IntoIterator<Item = T>,
    T: AsRef<OsStr>,
{
    paths
        .into_iter()
        .map(|p| PathBuf::from(p.as_ref()))
        .chain(std::iter::once(extra_path.to_path_buf()))
        .collect()
}

fn write_to_file(dir: &Path, filename: &str, content: &[u8]) -> Result<PathBuf, BuilderError> {
    let path = dir.join(filename);
    try_write_to_file(&path, content).map_err(|e| BuilderError::FileWriteFail(e, path.clone()))?;
    Ok(path)
}

fn try_write_to_file(path: &Path, content: &[u8]) -> std::io::Result<()> {
    let mut f = File::create(path)?;
    f.write_all(content)
}

fn rust_version_check() {
    if !version_check::is_min_version("1.54.0").unwrap_or(false) {
        panic!("Rust 1.54 or later is required.")
    }
}
