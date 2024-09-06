#![cfg_attr(not(check_cfg), allow(unexpected_cfgs))]
#![allow(
    clippy::cast_sign_loss,
    clippy::cognitive_complexity,
    clippy::default_trait_access,
    clippy::derive_partial_eq_without_eq,
    clippy::enum_glob_use,
    clippy::if_same_then_else,
    clippy::inherent_to_string,
    clippy::into_iter_without_iter,
    clippy::items_after_statements,
    clippy::large_enum_variant,
    clippy::map_clone,
    clippy::match_bool,
    clippy::match_on_vec_items,
    clippy::match_same_arms,
    clippy::module_name_repetitions,
    clippy::needless_pass_by_value,
    clippy::new_without_default,
    clippy::nonminimal_bool,
    clippy::or_fun_call,
    clippy::redundant_else,
    clippy::shadow_unrelated,
    clippy::similar_names,
    clippy::single_match_else,
    clippy::struct_excessive_bools,
    clippy::struct_field_names,
    clippy::too_many_arguments,
    clippy::too_many_lines,
    clippy::toplevel_ref_arg
)]

mod app;
mod cfg;
mod gen;
mod output;
mod syntax;

use crate::cfg::{CfgValue, FlagsCfgEvaluator};
use crate::gen::error::{report, Result};
use crate::gen::fs;
use crate::gen::include::{self, Include};
use crate::output::Output;
use std::collections::{BTreeMap as Map, BTreeSet as Set};
use std::io::{self, Write};
use std::path::PathBuf;
use std::process;

#[derive(Debug)]
struct Opt {
    input: Option<PathBuf>,
    header: bool,
    cxx_impl_annotations: Option<String>,
    include: Vec<Include>,
    outputs: Vec<Output>,
    cfg: Map<String, Set<CfgValue>>,
}

fn main() {
    if let Err(err) = try_main() {
        let _ = writeln!(io::stderr(), "cxxbridge: {}", report(err));
        process::exit(1);
    }
}

enum Kind {
    GeneratedHeader,
    GeneratedImplementation,
    Header,
}

fn try_main() -> Result<()> {
    let opt = app::from_args();

    let mut outputs = Vec::new();
    let mut gen_header = false;
    let mut gen_implementation = false;
    for output in opt.outputs {
        let kind = if opt.input.is_none() {
            Kind::Header
        } else if opt.header
            || output.ends_with(".h")
            || output.ends_with(".hh")
            || output.ends_with(".hpp")
        {
            gen_header = true;
            Kind::GeneratedHeader
        } else {
            gen_implementation = true;
            Kind::GeneratedImplementation
        };
        outputs.push((output, kind));
    }

    let gen = gen::Opt {
        include: opt.include,
        cxx_impl_annotations: opt.cxx_impl_annotations,
        gen_header,
        gen_implementation,
        cfg_evaluator: Box::new(FlagsCfgEvaluator::new(opt.cfg)),
        ..Default::default()
    };

    let generated_code = if let Some(input) = opt.input {
        gen::generate_from_path(&input, &gen)
    } else {
        Default::default()
    };

    for (output, kind) in outputs {
        let content = match kind {
            Kind::GeneratedHeader => &generated_code.header,
            Kind::GeneratedImplementation => &generated_code.implementation,
            Kind::Header => include::HEADER.as_bytes(),
        };
        match output {
            Output::Stdout => drop(io::stdout().write_all(content)),
            Output::File(path) => fs::write(path, content)?,
        }
    }

    Ok(())
}
