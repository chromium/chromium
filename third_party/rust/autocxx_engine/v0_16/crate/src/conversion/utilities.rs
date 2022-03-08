// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use autocxx_parser::IncludeCppConfig;

use super::api::{ApiName, UnanalyzedApi};
use crate::types::{make_ident, Namespace};

/// Adds items which we always add, cos they're useful.
/// Any APIs or techniques which do not involve actual C++ interop
/// shouldn't go here, but instead should go into the main autocxx
/// src/lib.rs.
pub(crate) fn generate_utilities(apis: &mut Vec<UnanalyzedApi>, config: &IncludeCppConfig) {
    // Unless we've been specifically asked not to do so, we always
    // generate a 'make_string' function. That pretty much *always* means
    // we run two passes through bindgen. i.e. the next 'if' is always true,
    // and we always generate an additional C++ file for our bindings additions,
    // unless the include_cpp macro has specified ExcludeUtilities.
    apis.push(UnanalyzedApi::StringConstructor {
        name: ApiName::new(&Namespace::new(), make_ident(config.get_makestring_name())),
    });
}
