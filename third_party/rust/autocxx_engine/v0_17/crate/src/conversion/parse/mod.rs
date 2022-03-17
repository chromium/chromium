// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod bindgen_semantic_attributes;
mod parse_bindgen;
mod parse_foreign_mod;

pub(crate) use bindgen_semantic_attributes::BindgenSemanticAttributes;
pub(crate) use parse_bindgen::ParseBindgen;
