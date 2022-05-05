// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use autocxx_parser::{IncludeCppConfig, MultiBindings};
use proc_macro2::TokenStream;

/// Opaque structure representing the Rust which needs to be generated
/// for a given `include_cpp!` macro. You will want to pass this into
/// either [`generate_rs_single`] or [`generate_rs_archive`].
pub struct RsOutput<'a> {
    pub(crate) config: &'a IncludeCppConfig,
    pub(crate) rs: TokenStream,
}

/// Creates an on-disk archive (actually a JSON file) of the Rust side of the bindings
/// for multiple `include_cpp` macros. If you use this, you will want to tell
/// `autocxx_macro` how to find this file using the `AUTOCXX_RS_ARCHIVE`
/// environment variable.
pub fn generate_rs_archive<'a>(rs_outputs: impl Iterator<Item = RsOutput<'a>>) -> String {
    let mut multi_bindings = MultiBindings::default();
    for rs in rs_outputs {
        multi_bindings.insert(rs.config, rs.rs);
    }
    serde_json::to_string(&multi_bindings).expect("Unable to encode JSON archive")
}

/// A single Rust file to be written to disk.
pub struct RsInclude {
    pub code: String,
    pub filename: String,
}

/// Gets the Rust code corresponding to a single [`RsOutput`]. You can write this
/// to a file which can simply be `include!`ed by `autocxx_macro` when you give
/// it the `AUTOCXX_RS_FILE` environment variable.
pub fn generate_rs_single(rs_output: RsOutput) -> RsInclude {
    RsInclude {
        code: rs_output.rs.to_string(),
        filename: rs_output.config.get_rs_filename(),
    }
}
