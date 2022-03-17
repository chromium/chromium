// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![forbid(unsafe_code)]

use autocxx_engine::{parse_file, HeaderNamer};
use clap::{crate_authors, crate_version, App, Arg, ArgGroup};
use proc_macro2::TokenStream;
use quote::ToTokens;
use std::io::{Read, Write};
use std::path::PathBuf;
use std::{cell::Cell, fs::File, path::Path};

pub(crate) static BLANK: &str = "// Blank autocxx placeholder";

static LONG_HELP: &str = "
Command line utility to expand the Rust 'autocxx' include_cpp! directive.

This tool can generate both the C++ and Rust side binding code for
a Rust file containing an include_cpp! directive.

If you're using cargo, don't use this: use autocxx_build instead,
which is much easier to include in build.rs build scripts. You'd likely
use this tool only if you're using some non-Cargo build system. If
that's you, read on.

This tool has three modes: generate the C++; generate a new Rust file where
the include_cpp! directive is *replaced* with bindings, or generate
a Rust file which can be included by the autocxx_macro. You may specify
multiple modes, or of course, invoke the tool multiple times.

In any mode, you'll need to pass the source Rust file name and the C++
include path.

For generation of the Rust side bindings, here's how to choose between
the two modes. If you're copying the entire Rust crate to a different
location during your build process, you may as well use --gen-rs-complete
to generate a whole new replacement .rs file with the autocxx
include_cpp! macro expanded.

But in most build systems, you won't be copying all the crate source
to a new location. In such a case, you should use --gen-rs-include
which will generate a file that will be included by the autocxx_macro
crate.

The second decision you must make is naming of the output files.
If your build system is able to cope with autocxx_gen building
unpredictable filenames, then:
a) set AUTOCXX_RS when using autocxx_macro
b) build all *.cc files produced by this tool.

If your build system requires each build rule to make precise filenames
known in advance, then you will need to:
a) Use `--generate-exact <N> --gen-rs-complete`
b) Teach your build system that the C++ files to compile are named `gen0.h`,
   `gen0.cc`, `gen1.h`, `gen1.cc`, etc (through `N`), corresponding
   to each `include_cpp!` section, plus 'cxxgen.h`. `gen.complete.rs` will also
   be generated and should be compiled _instead of_ the original Rust file.
c) If `N` is bigger than the number of files needed, extra no-op files will
   be emitted. These may still be compiled normally, but won't do anything. If
   `N` is smaller than the number of files needed, generation will fail.

Note that there is currently no way to teach each `include_cpp!` section
which `.include.rs` file to use, so the only way to get fixed output paths is
with `--gen-rs-complete`. There are always multiple `.cc` files (even with just
a single `include_cpp!` section), and we always generate the same number of each
type of file.
";

fn main() {
    let matches = App::new("autocxx-gen")
        .version(crate_version!())
        .author(crate_authors!())
        .about("Generates bindings files from Rust files that contain include_cpp! macros")
        .long_about(LONG_HELP)
        .arg(
            Arg::with_name("INPUT")
                .help("Sets the input .rs file to use")
                .required(true)
                .index(1),
        )
        .arg(
            Arg::with_name("outdir")
                .short("o")
                .long("outdir")
                .value_name("PATH")
                .help("output directory path")
                .takes_value(true)
                .required(true),
        )
        .arg(
            Arg::with_name("inc")
                .short("I")
                .long("inc")
                .multiple(true)
                .number_of_values(1)
                .value_name("INCLUDE DIRS")
                .help("include path")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("cpp-extension")
                .long("cpp-extension")
                .value_name("EXTENSION")
                .default_value("cc")
                .help("C++ filename extension")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("gen-cpp")
                .long("gen-cpp")
                .help("whether to generate C++ implementation and header files")
        )
        .arg(
            Arg::with_name("gen-rs-complete")
                .long("gen-rs-complete")
                .help("whether to generate a Rust file replacing the original file (suffix will be .complete.rs)")
        )
        .arg(
            Arg::with_name("gen-rs-include")
                .long("gen-rs-include")
                .help("whether to generate Rust files for inclusion using autocxx_macro (suffix will be .include.rs)")
        )
        .group(ArgGroup::with_name("mode")
            .required(true)
            .multiple(true)
            .arg("gen-cpp")
            .arg("gen-rs-complete")
            .arg("gen-rs-include")
        )
        .arg(
            Arg::with_name("skip-cxx-gen")
                .long("skip-cxx-gen")
                .help("Skip performing C++ codegen for #[cxx::bridge] blocks. Only applies for --gen-cpp")
                .requires("gen-cpp")
        )
        .arg(
            Arg::with_name("generate-exact")
                .long("generate-exact")
                .value_name("NUM")
                .help("assume and ensure there are exactly NUM bridge blocks in the file. Only applies for --gen-cpp or --gen-rs-include")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("fix-rs-include-name")
                .long("fix-rs-include-name")
                .help("Make the name of the .rs file predictable. You must set AUTOCXX_RS_FILE during Rust build time to educate autocxx_macro about your choice.")
                .requires("gen-rs-include")
        )
        .arg(
            Arg::with_name("auto-allowlist")
                .long("auto-allowlist")
                .help("Dynamically construct allowlist from real uses of APIs.")
        )
        .arg(
            Arg::with_name("suppress-system-headers")
                .long("suppress-system-headers")
                .help("Do not refer to any system headers from generated code. May be useful for minimization.")
        )
        .arg(
            Arg::with_name("cxx-impl-annotations")
                .long("cxx-impl-annotations")
                .value_name("ANNOTATION")
                .help("prefix for symbols to be exported from C++ bindings, e.g. __attribute__ ((visibility (\"default\")))")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("cxx-h-path")
                .long("cxx-h-path")
                .value_name("PREFIX")
                .help("prefix for path to cxx.h (from the cxx crate) within #include statements. Must end in /")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("cxxgen-h-path")
                .long("cxxgen-h-path")
                .value_name("PREFIX")
                .help("prefix for path to cxxgen.h (which we generate into the output directory) within #include statements. Must end in /")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("clang-args")
                .last(true)
                .multiple(true)
                .help("Extra arguments to pass to Clang"),
        )
        .get_matches();

    env_logger::builder().init();
    let mut parsed_file = parse_file(
        matches.value_of("INPUT").unwrap(),
        matches.is_present("auto-allowlist"),
    )
    .expect("Unable to parse Rust file and interpret autocxx macro");
    let incs = matches
        .values_of("inc")
        .unwrap_or_default()
        .map(PathBuf::from)
        .collect();
    let extra_clang_args: Vec<_> = matches
        .values_of("clang-args")
        .unwrap_or_default()
        .collect();
    let suppress_system_headers = matches.is_present("suppress-system-headers");
    let desired_number = matches
        .value_of("generate-exact")
        .map(|s| s.parse::<usize>().unwrap());
    let header_counter = Cell::new(0);
    let header_namer = if desired_number.is_some() {
        HeaderNamer(Box::new(|_| {
            let r = format!("gen{}.h", header_counter.get());
            header_counter.set(header_counter.get() + 1);
            r
        }))
    } else {
        Default::default()
    };
    let cpp_codegen_options = autocxx_engine::CppCodegenOptions {
        suppress_system_headers,
        cxx_impl_annotations: get_option_string("cxx-impl-annotations", &matches),
        path_to_cxx_h: get_option_string("cxx-h-path", &matches),
        path_to_cxxgen_h: get_option_string("cxxgen-h-path", &matches),
        skip_cxx_gen: matches.is_present("skip-cxx-gen"),
        header_namer,
    };
    // In future, we should provide an option to write a .d file here
    // by passing a callback into the dep_recorder parameter here.
    // https://github.com/google/autocxx/issues/56
    parsed_file
        .resolve_all(incs, &extra_clang_args, None, &cpp_codegen_options)
        .expect("Unable to resolve macro");
    let outdir: PathBuf = matches.value_of_os("outdir").unwrap().into();
    if matches.is_present("gen-cpp") {
        let cpp = matches.value_of("cpp-extension").unwrap();
        let mut counter = 0usize;
        for include_cxx in parsed_file.get_cpp_buildables() {
            let generations = include_cxx
                .generate_h_and_cxx(&cpp_codegen_options)
                .expect("Unable to generate header and C++ code");
            for pair in generations.0 {
                let cppname = format!("gen{}.{}", counter, cpp);
                write_to_file(&outdir, cppname, &pair.implementation.unwrap_or_default());
                write_to_file(&outdir, pair.header_name, &pair.header);
                counter += 1;
            }
        }
        write_placeholders(&outdir, counter, desired_number, cpp);
    }
    drop(cpp_codegen_options);
    write_placeholders(&outdir, header_counter.into_inner(), desired_number, "h");
    if matches.is_present("gen-rs-complete") {
        let mut ts = TokenStream::new();
        parsed_file.to_tokens(&mut ts);
        write_to_file(
            &outdir,
            "gen.complete.rs".to_string(),
            ts.to_string().as_bytes(),
        );
    }
    if matches.is_present("gen-rs-include") {
        let autocxxes = parsed_file.get_rs_buildables();
        let mut counter = 0usize;
        for include_cxx in autocxxes {
            let ts = include_cxx.generate_rs();
            let fname = if matches.is_present("fix-rs-include-name") {
                format!("gen{}.include.rs", counter)
            } else {
                include_cxx.get_rs_filename()
            };
            write_to_file(&outdir, fname, ts.to_string().as_bytes());
            counter += 1;
        }
        if matches.is_present("fix-rs-include-name") {
            write_placeholders(&outdir, counter, desired_number, "include.rs");
        }
    }
}

fn get_option_string(option: &str, matches: &clap::ArgMatches) -> Option<String> {
    let cxx_impl_annotations = matches.value_of(option).map(|s| s.to_string());
    cxx_impl_annotations
}

fn write_placeholders(
    outdir: &Path,
    mut counter: usize,
    desired_number: Option<usize>,
    extension: &str,
) {
    if let Some(desired_number) = desired_number {
        if counter > desired_number {
            panic!("More .{} files were generated than expected. Increase the value passed to --generate-exact or reduce the number of include_cpp! sections.", extension);
        }
        while counter < desired_number {
            let fname = format!("gen{}.{}", counter, extension);
            write_to_file(outdir, fname, BLANK.as_bytes());
            counter += 1;
        }
    }
}

fn write_to_file(dir: &Path, filename: String, content: &[u8]) {
    let path = dir.join(filename);
    {
        let f = File::open(&path);
        if let Ok(mut f) = f {
            let mut existing_content = Vec::new();
            let r = f.read_to_end(&mut existing_content);
            if r.is_ok() && existing_content == content {
                return; // don't change timestamp on existing file unnecessarily
            }
        }
    }
    let mut f = File::create(&path).expect("Unable to create file");
    f.write_all(content).expect("Unable to write file");
}
