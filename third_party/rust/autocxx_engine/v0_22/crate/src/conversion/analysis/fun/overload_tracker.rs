// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::HashMap;

type Offsets = HashMap<String, usize>;

/// Registry of all the overloads of a function found within a given
/// namespace (i.e. mod in bindgen's output). If necessary we'll append
/// a _nnn suffix to a function's Rust name to disambiguate overloads.
/// Note that this is NOT necessarily the same as the suffix added by
/// bindgen to disambiguate overloads it discovers. Its suffix is
/// global across all functions, whereas ours is local within a given
/// type.
/// If bindgen adds a suffix it will be included in 'found_name'
/// but not 'original_name' which is an annotation added by our autocxx-bindgen
/// fork.
#[derive(Default)]
pub(crate) struct OverloadTracker {
    offset_by_name: Offsets,
    offset_by_type_and_name: HashMap<String, Offsets>,
}

impl OverloadTracker {
    pub(crate) fn get_function_real_name(&mut self, found_name: String) -> String {
        self.get_name(None, found_name)
    }

    pub(crate) fn get_method_real_name(&mut self, type_name: &str, found_name: String) -> String {
        self.get_name(Some(type_name), found_name)
    }

    fn get_name(&mut self, type_name: Option<&str>, cpp_method_name: String) -> String {
        let registry = match type_name {
            Some(type_name) => self
                .offset_by_type_and_name
                .entry(type_name.to_string())
                .or_default(),
            None => &mut self.offset_by_name,
        };
        let offset = registry.entry(cpp_method_name.clone()).or_default();
        let this_offset = *offset;
        *offset += 1;
        if this_offset == 0 {
            cpp_method_name
        } else {
            format!("{}{}", cpp_method_name, this_offset)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::OverloadTracker;

    #[test]
    fn test_by_function() {
        let mut ot = OverloadTracker::default();
        assert_eq!(ot.get_function_real_name("bob".into()), "bob");
        assert_eq!(ot.get_function_real_name("bob".into()), "bob1");
        assert_eq!(ot.get_function_real_name("bob".into()), "bob2");
    }

    #[test]
    fn test_by_method() {
        let mut ot = OverloadTracker::default();
        assert_eq!(ot.get_method_real_name("Ty1", "bob".into()), "bob");
        assert_eq!(ot.get_method_real_name("Ty1", "bob".into()), "bob1");
        assert_eq!(ot.get_method_real_name("Ty2", "bob".into()), "bob");
        assert_eq!(ot.get_method_real_name("Ty2", "bob".into()), "bob1");
    }
}
