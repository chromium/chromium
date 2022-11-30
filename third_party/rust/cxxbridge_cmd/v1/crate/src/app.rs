#[cfg(test)]
#[path = "test.rs"]
mod test;

use super::{Opt, Output};
use crate::cfg::{self, CfgValue};
use crate::gen::include::Include;
use crate::syntax::IncludeKind;
use clap::{Arg, Command};
use std::collections::{BTreeMap as Map, BTreeSet as Set};
use std::ffi::OsStr;
use std::path::PathBuf;
use syn::parse::Parser;

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
{options}\
";

fn app() -> Command<'static> {
    let app = Command::new("cxxbridge")
        .override_usage(USAGE)
        .help_template(TEMPLATE)
        .next_line_help(true)
        .arg(arg_input())
        .arg(arg_cxx_impl_annotations())
        .arg(arg_header())
        .arg(arg_include())
        .arg(arg_output())
        .arg(arg_cfg())
        .mut_arg("help", |a| a.help("Print help information."));
    // Remove for Chromium.
    // if let Some(version) = option_env!("CARGO_PKG_VERSION") {
    //     app = app
    //         .version(version)
    //         .mut_arg("version", |a| a.help("Print version information."));
    // }
    app
}

const INPUT: &str = "input";
const CXX_IMPL_ANNOTATIONS: &str = "cxx-impl-annotations";
const HEADER: &str = "header";
const INCLUDE: &str = "include";
const OUTPUT: &str = "output";
const CFG: &str = "cfg";

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

    let mut cfg = Map::new();
    for arg in matches.values_of(CFG).unwrap_or_default() {
        let (name, value) = cfg::parse.parse_str(arg).unwrap();
        cfg.entry(name).or_insert_with(Set::new).insert(value);
    }

    Opt {
        input,
        header,
        cxx_impl_annotations,
        include,
        outputs,
        cfg,
    }
}

fn validate_utf8(arg: &OsStr) -> Result<(), &'static str> {
    if arg.to_str().is_some() {
        Ok(())
    } else {
        Err("invalid utf-8 sequence")
    }
}

fn arg_input() -> Arg<'static> {
    Arg::new(INPUT)
        .help("Input Rust source file containing #[cxx::bridge].")
        .required_unless_present(HEADER)
        .allow_invalid_utf8(true)
}

fn arg_cxx_impl_annotations() -> Arg<'static> {
    const HELP: &str = "\
Optional annotation for implementations of C++ function wrappers
that may be exposed to Rust. You may for example need to provide
__declspec(dllexport) or __attribute__((visibility(\"default\")))
if Rust code from one shared object or executable depends on
these C++ functions in another.";
    Arg::new(CXX_IMPL_ANNOTATIONS)
        .long(CXX_IMPL_ANNOTATIONS)
        .takes_value(true)
        .value_name("annotation")
        .allow_invalid_utf8(true)
        .validator_os(validate_utf8)
        .help(HELP)
}

fn arg_header() -> Arg<'static> {
    const HELP: &str = "\
Emit header with declarations only. Optional if using `-o` with
a path ending in `.h`.";
    Arg::new(HEADER).long(HEADER).help(HELP)
}

fn arg_include() -> Arg<'static> {
    const HELP: &str = "\
Any additional headers to #include. The cxxbridge tool does not
parse or even require the given paths to exist; they simply go
into the generated C++ code as #include lines.";
    Arg::new(INCLUDE)
        .long(INCLUDE)
        .short('i')
        .takes_value(true)
        .multiple_occurrences(true)
        .allow_invalid_utf8(true)
        .validator_os(validate_utf8)
        .help(HELP)
}

fn arg_output() -> Arg<'static> {
    const HELP: &str = "\
Path of file to write as output. Output goes to stdout if -o is
not specified.";
    Arg::new(OUTPUT)
        .long(OUTPUT)
        .short('o')
        .takes_value(true)
        .multiple_occurrences(true)
        .allow_invalid_utf8(true)
        .validator_os(validate_utf8)
        .help(HELP)
}

fn arg_cfg() -> Arg<'static> {
    const HELP: &str = "\
Compilation configuration matching what will be used to build
the Rust side of the bridge.";
    let mut bool_cfgs = Map::<String, bool>::new();
    Arg::new(CFG)
        .long(CFG)
        .takes_value(true)
        .value_name("name=\"value\" | name[=true] | name=false")
        .multiple_occurrences(true)
        .validator(move |arg| match cfg::parse.parse_str(arg) {
            Ok((_, CfgValue::Str(_))) => Ok(()),
            Ok((name, CfgValue::Bool(value))) => {
                if let Some(&prev) = bool_cfgs.get(&name) {
                    if prev != value {
                        return Err(format!("cannot have both {0}=false and {0}=true", name));
                    }
                }
                bool_cfgs.insert(name, value);
                Ok(())
            }
            Err(_) => Err("expected name=\"value\", name=true, or name=false".to_owned()),
        })
        .help(HELP)
}
