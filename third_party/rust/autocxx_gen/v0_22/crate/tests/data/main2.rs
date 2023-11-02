// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mod directive1;
mod directive2;

fn main() {
    println!("C++ says {} then {}", directive1::get_hello().as_ref().unwrap().to_string_lossy(),
        directive2::get_goodbye().as_ref().unwrap().to_string_lossy());
}
