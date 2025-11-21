// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::TokenStream;
use syn::visit::Visit;
use syn::{ExprUnsafe, File, ItemImpl, ItemMod, Meta};

/// Checks if the `input` Rust code contains `unsafe` code.
///
/// The checks are heuristics-based - they may be inaccurate in some scenarios.
/// In particular, testing against ~150 vendored crates on Nov 20, the following
/// inaccuracies were noted in 4 crates (~2-3% of crates):
///
/// - Undetected `unsafe`:
///     - Will be auto-detected later, when `allow_unsafe = false` results in a
///       build error. Hopefully `//docs/rust/build_errors_guide.md` will guide
///       toward changing the `allow_unsafe` setting to `true`.
///     - Examples (IIUC in both cases `unsafe` is hidden inside
///       `syn::Macro::tokens`):
///         - `num-traits-v0_2/src/cast.rs`
///         - `timezone_provider-v0_1/src/data/compiled_zoneinfo_provider.rs.
///           data`
/// - Incorrectly detected `unsafe`:
///     - Requires manual detection.  Hopefully `//third_party/rust/OWNERS`
///       review will detect that `allow_unsafe = true` is unnecessary.
///     - Examples (IIUC `unsafe` is behind a turned-off `#[cfg(...)]`):
///         - `serde-1.0.228`
///         - `subtle-2.6.1`
pub fn contains_unsafe_code(input: &str) -> bool {
    let Some(file) =
        input.parse::<TokenStream>().ok().and_then(|tokens| syn::parse2::<File>(tokens).ok())
    else {
        // Not a valid Rust source file => no `unsafe`.
        return false;
    };

    struct UnsafeVisitor {
        found_unsafe: bool,
    }

    impl<'ast> Visit<'ast> for UnsafeVisitor {
        /// Detecting `unsafe { ... }` expression blocks.
        fn visit_expr_unsafe(&mut self, _: &'ast ExprUnsafe) {
            self.found_unsafe = true;
        }

        /// Detecting `unsafe impl TraitThatHasSafetyRequirements ...`.
        fn visit_item_impl(&mut self, i: &'ast ItemImpl) {
            if i.unsafety.is_some() {
                self.found_unsafe = true;
            }
            syn::visit::visit_item_impl(self, i);
        }

        /// Ignoring test code like this:
        ///
        /// ```
        /// #[cfg(test)]
        /// mod test {
        ///    // ...
        /// }
        /// ```
        fn visit_item_mod(&mut self, i: &'ast ItemMod) {
            if i.ident == "test" {
                return;
            }

            let is_cfg_test_attr_present = i.attrs.iter().any(|attr| match &attr.meta {
                Meta::List(list) => {
                    if !list.path.segments.iter().any(|segment| segment.ident == "cfg") {
                        return false;
                    }
                    list.tokens.to_string() == "test"
                }
                _ => false,
            });
            if is_cfg_test_attr_present {
                return;
            }

            syn::visit::visit_item_mod(self, i);
        }
    }

    let mut visitor = UnsafeVisitor { found_unsafe: false };
    visitor.visit_file(&file);
    visitor.found_unsafe
}

#[cfg(test)]
mod test {
    use super::does_contain_unsafe_code;

    #[test]
    fn test_unsafe_expr() {
        assert!(does_contain_unsafe_code(
            r#"
                fn foo() {
                    // SAFETY: safety requirements of `some_unsafe_fn` are met, because...
                    unsafe { some_crate::some_unsafe_fn() }
                }
            "#
        ));
    }

    #[test]
    fn test_ignore_unsafe_fn_declarations() {
        // Calling an unsafe function is unsafe.  Declaring one is not.
        assert!(!does_contain_unsafe_code(
            r#"
                /// # Safety
                ///
                /// `some_unsafe_fn` requires callers to guarantee that...
                unsafe fn some_unsafe_fn() { todo!() }
            "#
        ));
    }

    #[test]
    fn test_unsafe_impl() {
        assert!(does_contain_unsafe_code(
            r#"
                // SAFETY: safety requirements of `SomeUnsafeTrait` are met, because...
                unsafe impl SomeUnsafeTrait for SomeStruct {}
            "#
        ));
    }

    #[test]
    fn test_ignore_unsafe_trait_declarations() {
        // Implementing an unsafe trait is unsafe.  Declaring one is not.
        assert!(!does_contain_unsafe_code(
            r#"
                /// # Safety
                ///
                /// Users of `SomeUnsafeTrait` can assume that all implementations
                /// meet the following safety requirements: ...
                unsafe trait SomeUnsafeTrait {
                    // ...
                }
            "#
        ));
    }

    #[test]
    fn test_ignore_comments_and_strings_etc() {
        assert!(!does_contain_unsafe_code(
            r#"
                // This is not unsafe { 2 + 2 }
                fn foo_unchecked() {
                    let _ = "This is not unsafe { 2 + 2 }";
                    let r#unsafe = 123;
                }
            "#
        ));
    }

    #[test]
    fn test_ignore_test_modules1() {
        assert!(!does_contain_unsafe_code(
            r#"
                #[cfg(test)]
                mod bar {
                    fn foo() {
                        unsafe { some_crate::some_unsafe_fn() }
                    }
                }
            "#
        ));
    }

    #[test]
    fn test_ignore_test_modules2() {
        assert!(!does_contain_unsafe_code(
            r#"
                mod test {
                    fn foo() {
                        unsafe { some_crate::some_unsafe_fn() }
                    }
                }
            "#
        ));
    }
}
