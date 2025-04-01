// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu::casemap::CaseMapper;
use icu_locale_core::langid;

fn main() {
    let cm = CaseMapper::new();

    println!(
        r#"The uppercase of "hello world" is "{}""#,
        cm.uppercase_to_string("hello world", &langid!("und"))
    );
    println!(
        r#"The lowercase of "Γειά σου Κόσμε" is "{}""#,
        cm.lowercase_to_string("Γειά σου Κόσμε", &langid!("und"))
    );
}
