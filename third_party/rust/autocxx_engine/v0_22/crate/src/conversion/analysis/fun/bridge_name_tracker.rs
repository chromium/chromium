// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::types::Namespace;
use itertools::Itertools;
use std::collections::HashMap;

/// Type to generate unique names for entries in the [cxx::bridge]
/// mod which is flat.
///
/// # All about the names involved in autocxx
///
/// A given function may have many, many names. From C++ to Rust...
///
/// 1. The actual C++ name. Fixed, obviously.
/// 2. The name reported by bindgen. Not always the same, as
///    bindgen may generate a different name for different overloads.
///    See `overload_tracker` for the conversion here.
/// 3. The name in the cxx::bridge mod. This is a flat namespace,
///    and it's the responsibility of this type to generate a
///    suitable name here.
///    If this is different from the C++ name in (1), we'll
///    add a #[cxx_name] attribute to the cxx::bridge declaration.
/// 4. The name we wish to present to Rust users. Again, we have
///    to take stock of the fact Rust doesn't support overloading
///    so two different functions called 'get' may end up being
///    'get' and 'get1'. Yet, this may not be the same as the
///    bindgen name in (2) because we wish to generate such number
///    sequences on a per-type basis, whilst bindgen does it globally.
///
/// This fourth name, the final Rust user name, may be finagled
/// into place in three different ways:
/// 1. For methods, we generate an 'impl' block where the
///    method name is the intended name, but it calls the
///    cxxbridge name.
/// 2. For simple functions, we use the #[rust_name] attribute.
/// 3. Occasionally, there can be conflicts in the rust_name
///    namespace (if there are two identically named functions
///    in different C++ namespaces). That's detected by
///    rust_name_tracker.rs. In such a case, we'll omit the
///    #[rust_name] attribute and instead generate a 'use A = B;'
///    declaration in the mod which we generate for the output
///    namespace.
#[derive(Default)]
pub(crate) struct BridgeNameTracker {
    next_cxx_bridge_name_for_prefix: HashMap<String, usize>,
}

impl BridgeNameTracker {
    pub(crate) fn new() -> Self {
        Self::default()
    }

    /// Figure out the least confusing unique name for this function in the
    /// cxx::bridge section, which has a flat namespace.
    /// We mostly just qualify the name with the namespace_with_underscores.
    /// It doesn't really matter; we'll rebind these things to
    /// better Rust-side names so it's really just a matter of how it shows up
    /// in stack traces and for our own sanity as maintainers of autocxx.
    /// This may become unnecessary if and when cxx supports hierarchic
    /// namespace mods.
    /// There is a slight advantage in using the same name as either the
    /// Rust or C++ symbols as it reduces the amount of rebinding required
    /// by means of cxx_name or rust_name attributes. In extreme cases it
    /// may even allow us to remove whole impl blocks. So we may wish to try
    /// harder to find better names in future instead of always prepending
    /// the namespace.
    pub(crate) fn get_unique_cxx_bridge_name(
        &mut self,
        type_name: Option<&str>,
        found_name: &str,
        ns: &Namespace,
    ) -> String {
        let found_name = if found_name == "new" {
            "new_autocxx"
        } else {
            found_name
        };
        let count = self
            .next_cxx_bridge_name_for_prefix
            .entry(found_name.to_string())
            .or_default();
        if *count == 0 {
            // Oh, good, we can use this function name as-is.
            *count += 1;
            return found_name.to_string();
        }
        let prefix = ns
            .iter()
            .cloned()
            .chain(type_name.iter().map(|x| x.to_string()))
            .chain(std::iter::once(found_name.to_string()))
            .join("_");
        let count = self
            .next_cxx_bridge_name_for_prefix
            .entry(prefix.clone())
            .or_default();
        if *count == 0 {
            *count += 1;
            prefix
        } else {
            let r = format!("{}_autocxx{}", prefix, count);
            *count += 1;
            r
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::types::Namespace;

    use super::BridgeNameTracker;

    #[test]
    fn test() {
        let mut bnt = BridgeNameTracker::new();
        let ns_root = Namespace::new();
        let ns_a = Namespace::from_user_input("A");
        let ns_b = Namespace::from_user_input("B");
        let ns_ab = Namespace::from_user_input("A::B");
        assert_eq!(bnt.get_unique_cxx_bridge_name(None, "do", &ns_root), "do");
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(None, "do", &ns_root),
            "do_autocxx1"
        );
        assert_eq!(bnt.get_unique_cxx_bridge_name(None, "did", &ns_root), "did");
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty1"), "do", &ns_root),
            "ty1_do"
        );
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty1"), "do", &ns_root),
            "ty1_do_autocxx1"
        );
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty2"), "do", &ns_root),
            "ty2_do"
        );
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty"), "do", &ns_a),
            "A_ty_do"
        );
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty"), "do", &ns_b),
            "B_ty_do"
        );
        assert_eq!(
            bnt.get_unique_cxx_bridge_name(Some("ty"), "do", &ns_ab),
            "A_B_ty_do"
        );
    }
}
