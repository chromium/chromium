// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fn main() {
    println!(
        "Hello, world! Origin is: {}",
        autocxx_basic_test_lib::serialize_url("https", "foo.com", 443)
    );
    println!("CPU vendor is: {}", autocxx_basic_test_lib::get_cpu_vendor());
}
