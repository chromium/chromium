extern crate bindgen;
#[cfg(feature = "logging")]
extern crate env_logger;
#[macro_use]
#[cfg(feature = "logging")]
extern crate log;
extern crate clap;

use bindgen::clang_version;
use std::env;
use std::panic;

#[macro_use]
#[cfg(not(feature = "logging"))]
mod log_stubs;

mod options;
use crate::options::builder_from_flags;

fn clang_version_check() {
    let version = clang_version();
    let expected_version = if cfg!(feature = "testing_only_libclang_9") {
        Some((9, 0))
    } else if cfg!(feature = "testing_only_libclang_5") {
        Some((5, 0))
    } else {
        None
    };

    info!(
        "Clang Version: {}, parsed: {:?}",
        version.full, version.parsed
    );

    if expected_version.is_some() {
        // assert_eq!(version.parsed, version.parsed);
    }
}

pub fn main() {
    #[cfg(feature = "logging")]
    env_logger::init();

    match builder_from_flags(env::args()) {
        Ok((builder, output, verbose)) => {
            clang_version_check();
            let builder_result = panic::catch_unwind(|| {
                builder.generate().expect("Unable to generate bindings")
            });

            if builder_result.is_err() {
                if verbose {
                    print_verbose_err();
                }
                std::process::exit(1);
            }

            let bindings = builder_result.unwrap();
            bindings.write(output).expect("Unable to write output");
        }
        Err(error) => {
            println!("{}", error);
            std::process::exit(1);
        }
    };
}

fn print_verbose_err() {
    println!("Bindgen unexpectedly panicked");
    println!(
        "This may be caused by one of the known-unsupported \
         things (https://rust-lang.github.io/rust-bindgen/cpp.html), \
         please modify the bindgen flags to work around it as \
         described in https://rust-lang.github.io/rust-bindgen/cpp.html"
    );
    println!(
        "Otherwise, please file an issue at \
         https://github.com/rust-lang/rust-bindgen/issues/new"
    );
}

#[cfg(test)]
mod test {
    fn build_flags_output_helper(builder: &bindgen::Builder) {
        let mut command_line_flags = builder.command_line_flags();
        command_line_flags.insert(0, "bindgen".to_string());

        let flags_quoted: Vec<String> = command_line_flags
            .iter()
            .map(|x| format!("{}", shlex::quote(x)))
            .collect();
        let flags_str = flags_quoted.join(" ");
        println!("{}", flags_str);

        let (builder, _output, _verbose) =
            crate::options::builder_from_flags(command_line_flags.into_iter())
                .unwrap();
        builder.generate().expect("failed to generate bindings");
    }

    #[test]
    fn commandline_multiple_headers() {
        let bindings = bindgen::Builder::default()
            .header("tests/headers/char.h")
            .header("tests/headers/func_ptr.h")
            .header("tests/headers/16-byte-alignment.h");
        build_flags_output_helper(&bindings);
    }
}
