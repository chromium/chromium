// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// An example application which uses icu_plurals to construct a correct
// sentence for English based on the numerical value in Cardinal category.

#![no_main] // https://github.com/unicode-org/icu4x/issues/395
icu_benchmark_macros::instrument!();
use icu_benchmark_macros::println;

use icu_locale_core::locale;
use icu_plurals::{PluralCategory, PluralRules};

const VALUES: &[usize] = &[0, 2, 25, 1, 3, 2, 4, 10, 7, 0];

fn main() {
    println!("\n====== Unread Emails (en) example ============");
    let pr = PluralRules::try_new_cardinal(locale!("en").into())
        .expect("Failed to create a PluralRules instance.");

    for value in VALUES {
        match pr.category_for(*value) {
            PluralCategory::One => println!("You have one unread email."),
            _ => println!("You have {value} unread emails."),
        }
    }
}
