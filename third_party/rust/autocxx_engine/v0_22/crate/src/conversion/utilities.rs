// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use autocxx_parser::IncludeCppConfig;

use super::{
    api::{ApiName, NullPhase, UnanalyzedApi},
    apivec::ApiVec,
};
use crate::types::{make_ident, Namespace};

/// Adds items which we always add, cos they're useful.
/// Any APIs or techniques which do not involve actual C++ interop
/// shouldn't go here, but instead should go into the main autocxx
/// src/lib.rs.
pub(crate) fn generate_utilities(apis: &mut ApiVec<NullPhase>, config: &IncludeCppConfig) {
    // Unless we've been specifically asked not to do so, we always
    // generate a 'make_string' function. That pretty much *always* means
    // we run two passes through bindgen. i.e. the next 'if' is always true,
    // and we always generate an additional C++ file for our bindings additions,
    // unless the include_cpp macro has specified ExcludeUtilities.
    apis.push(UnanalyzedApi::StringConstructor {
        name: ApiName::new(&Namespace::new(), make_ident(config.get_makestring_name())),
    });
}
