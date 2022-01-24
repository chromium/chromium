#[cfg(test)]
#[path = "test.rs"]
mod test;

use super::{Opt, Output};
use crate::gen::include::Include;
use crate::syntax::IncludeKind;
use clap::AppSettings;
use std::ffi::{OsStr, OsString};
use std::path::PathBuf;

type App = clap::App<'static, 'static>;
type Arg = clap::Arg<'static, 'static>;

const USAGE: &str = "\
    cxxbridge <input>.rs              Emit .cc file for bridge to stdout
    cxxbridge <input>.rs --header     Emit .h file for bridge to stdout
    cxxbridge --header                Emit \"rust/cxx.h\" header to stdout\
";

const TEMPLATE: &str = "\
{bin} {version}
David Tolnay <dtolnay@gmail.com>
https://github.com/dtolnay/cxx

USAGE:
    {usage}

ARGS:
{positionals}
OPTIONS:
{unified}\
";

fn app() -> App {
    let app = App::new("cxxbridge")
        .usage(USAGE)
        .template(TEMPLATE)
        .setting(AppSettings::NextLineHelp)
        .arg(arg_input())
        .arg(arg_cxx_impl_annotations())
        .arg(arg_header())
        .arg(arg_include())
        .arg(arg_output())
        .help_message("Print help information.")
        .version_message("Print version information.");
    app
}

const INPUT: &str = "input";
const CXX_IMPL_ANNOTATIONS: &str = "cxx-impl-annotations";
const HEADER: &str = "header";
const INCLUDE: &str = "include";
const OUTPUT: &str = "output";

pub(super) fn from_args() -> Opt {
    let matches = app().get_matches();

    let input = matches.value_of_os(INPUT).map(PathBuf::from);
    let cxx_impl_annotations = matches.value_of(CXX_IMPL_ANNOTATIONS).map(str::to_owned);
    let header = matches.is_present(HEADER);
    let include = matches
        .values_of(INCLUDE)
        .unwrap_or_default()
        .map(|include| {
            if include.starts_with('<') && include.ends_with('>') {
                Include {
                    path: include[1..include.len() - 1].to_owned(),
                    kind: IncludeKind::Bracketed,
                }
            } else {
                Include {
                    path: include.to_owned(),
                    kind: IncludeKind::Quoted,
                }
            }
        })
        .collect();

    let mut outputs = Vec::new();
    for path in matches.values_of_os(OUTPUT).unwrap_or_default() {
        outputs.push(if path == "-" {
            Output::Stdout
        } else {
            Output::File(PathBuf::from(path))
        });
    }
    if outputs.is_empty() {
        outputs.push(Output::Stdout);
    }

    Opt {
        input,
        header,
        cxx_impl_annotations,
        include,
        outputs,
    }
}

fn validate_utf8(arg: &OsStr) -> Result<(), OsString> {
    if arg.to_str().is_some() {
        Ok(())
    } else {
        Err(OsString::from("invalid utf-8 sequence"))
    }
}

fn arg_input() -> Arg {
    Arg::with_name(INPUT)
        .help("Input Rust source file containing #[cxx::bridge].")
        .required_unless(HEADER)
}

fn arg_cxx_impl_annotations() -> Arg {
    const HELP: &str = "\
Optional annotation for implementations of C++ function wrappers
that may be exposed to Rust. You may for example need to provide
__declspec(dllexport) or __attribute__((visibility(\"default\")))
if Rust code from one shared object or executable depends on
these C++ functions in another.
    ";
    Arg::with_name(CXX_IMPL_ANNOTATIONS)
        .long(CXX_IMPL_ANNOTATIONS)
        .takes_value(true)
        .value_name("annotation")
        .validator_os(validate_utf8)
        .help(HELP)
}

fn arg_header() -> Arg {
    const HELP: &str = "\
Emit header with declarations only. Optional if using `-o` with
a path ending in `.h`.
    ";
    Arg::with_name(HEADER).long(HEADER).help(HELP)
}

fn arg_include() -> Arg {
    const HELP: &str = "\
Any additional headers to #include. The cxxbridge tool does not
parse or even require the given paths to exist; they simply go
into the generated C++ code as #include lines.
    ";
    Arg::with_name(INCLUDE)
        .long(INCLUDE)
        .short("i")
        .takes_value(true)
        .multiple(true)
        .number_of_values(1)
        .validator_os(validate_utf8)
        .help(HELP)
}

fn arg_output() -> Arg {
    const HELP: &str = "\
Path of file to write as output. Output goes to stdout if -o is
not specified.
    ";
    Arg::with_name(OUTPUT)
        .long(OUTPUT)
        .short("o")
        .takes_value(true)
        .multiple(true)
        .number_of_values(1)
        .validator_os(validate_utf8)
        .help(HELP)
}
