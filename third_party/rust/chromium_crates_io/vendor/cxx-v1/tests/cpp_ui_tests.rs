mod cpp_compile;

use indoc::indoc;
use quote::quote;

/// This is a regression test for `static_assert(::rust::is_complete...)`
/// which we started to emit in <https://github.com/dtolnay/cxx/commit/534627667>
#[test]
fn test_unique_ptr_of_incomplete_foward_declared_pointee() {
    let test = cpp_compile::Test::new(quote! {
        #[cxx::bridge]
        mod ffi {
            unsafe extern "C++" {
                include!("include.h");
                type ForwardDeclaredType;
            }
            impl UniquePtr<ForwardDeclaredType> {}
        }
    });
    test.write_file(
        "include.h",
        indoc! {"
            class ForwardDeclaredType;
        "},
    );
    let err_msg = test.compile().expect_single_error();
    assert!(err_msg.contains("definition of `::ForwardDeclaredType` is required"));
}

/// This is a regression test for returning a struct with reference members
/// across extern "C" without triggering Clang's `-Wreturn-type-c-linkage`. See
/// also: <https://github.com/dtolnay/cxx/issues/1753>
///
/// Note that the original repro required explicitly opting into extra C/C++
/// warnings (as already done by `.github/workflows/ci.yml`):
///
/// ```sh
/// $ CXX=clang++ CXXFLAGS="-Werror -Wall -Wpedantic" cargo test --test cpp_ui_tests
/// ```
///
/// This is a separate test from `tests/ffi/lib.rs` and `tests/ffi/module.rs` to
/// avoid the risk that some other input (e.g. callback functions) will suppress
/// the warning via `out.pragma.return_type_c_linkage = true` and `#pragma clang
/// diagnostic ignored "-Wreturn-type-c-linkage"`.
#[test]
fn test_return_struct_with_ref() {
    let test = cpp_compile::Test::new(quote! {
        #[cxx::bridge]
        mod ffi {
            struct StructWithRef<'a> {
                r: &'a usize,
            }

            unsafe extern "C++" {
                include!("include.h");
                fn c_return_struct_with_ref<'a>(r: &'a usize) -> StructWithRef<'a>;
            }
        }
    });
    test.write_file(
        "include.h",
        indoc! {"
            #pragma once
            #include <cstddef>
            struct StructWithRef;
            StructWithRef c_return_struct_with_ref(size_t const &r);
        "},
    );
    test.compile().assert_success();
}
