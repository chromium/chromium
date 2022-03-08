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

use autocxx_parser::UnsafePolicy;
#[allow(unused_imports)]
use syn::parse_quote;
use syn::ItemMod;

use crate::CppCodegenOptions;

use super::BridgeConverter;

// This mod is for tests which take bindgen output directly.
// This should be avoided where possible, since these tests will
// become obsolete or have to change if and when we update
// bindgen. Instead, please add tests working directly from
// the original C++ in integration_tests.rs if possible.
// Also, if you're pasting in code from github issues, it's
// important to make sure that the underlying code has an
// acceptable license. That's why this file is currently blank.

#[allow(dead_code)]
fn do_test(input: ItemMod) {
    let tc = parse_quote! {};
    let bc = BridgeConverter::new(&[], &tc);
    let inclusions = "".into();
    bc.convert(
        input,
        UnsafePolicy::AllFunctionsSafe,
        inclusions,
        &CppCodegenOptions::default(),
    )
    .unwrap();
}

// How to add a test here
//
// #[test]
// fn test_xyz() {
//      do_test(parse_quote!{ /* paste bindgen output here */})
// }
