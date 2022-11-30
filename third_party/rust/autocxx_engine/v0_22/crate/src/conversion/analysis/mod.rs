// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

pub(crate) mod abstract_types;
pub(crate) mod allocators;
pub(crate) mod casts;
pub(crate) mod constructor_deps;
pub(crate) mod ctypes;
pub(crate) mod deps;
mod depth_first;
mod doc_label;
pub(crate) mod fun;
pub(crate) mod gc;
mod name_check;
pub(crate) mod pod; // hey, that rhymes
pub(crate) mod remove_ignored;
mod replace_hopeless_typedef_targets;
pub(crate) mod tdef;
mod type_converter;

pub(crate) use name_check::check_names;
pub(crate) use replace_hopeless_typedef_targets::replace_hopeless_typedef_targets;
pub(crate) use type_converter::PointerTreatment;
