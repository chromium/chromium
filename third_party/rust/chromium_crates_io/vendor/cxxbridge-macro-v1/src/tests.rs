use crate::expand;
use crate::syntax::file::Module;
use proc_macro2::TokenStream;
use quote::quote;
use syn::File;

fn bridge(cxx_bridge: TokenStream) -> String {
    let module = syn::parse2::<Module>(cxx_bridge).unwrap();
    let tokens = expand::bridge(module).unwrap();
    let file = match syn::parse2::<File>(tokens.clone()) {
        Ok(file) => file,
        Err(err) => {
            eprintln!("The code below is syntactically invalid: {err}:");
            eprintln!("{tokens}");
            panic!("`expand::bridge` should generate syntactically valid code");
        }
    };
    let pretty = prettyplease::unparse(&file);
    eprintln!("{0:/<80}\n{pretty}{0:/<80}", "");
    pretty
}

#[test]
fn test_unique_ptr_with_elided_lifetime_implicit_impl() {
    let rs = bridge(quote! {
        mod ffi {
            unsafe extern "C++" {
                type Borrowed<'a>;
                fn borrowed(arg: &i32) -> UniquePtr<Borrowed>;
            }
        }
    });

    // It is okay that the return type elides Borrowed's lifetime parameter.
    assert!(rs.contains("pub fn borrowed(arg: &i32) -> ::cxx::UniquePtr<Borrowed>"));

    // But in impl blocks, the lifetime parameter needs to be present.
    assert!(rs.contains("unsafe impl<'a> ::cxx::ExternType for Borrowed<'a> {"));
    assert!(rs.contains("unsafe impl<'a> ::cxx::memory::UniquePtrTarget for Borrowed<'a> {"));

    // Wrong.
    assert!(!rs.contains("unsafe impl ::cxx::ExternType for Borrowed {"));
    assert!(!rs.contains("unsafe impl ::cxx::memory::UniquePtrTarget for Borrowed {"));

    // Potentially okay, but not what we currently do.
    assert!(!rs.contains("unsafe impl ::cxx::ExternType for Borrowed<'_> {"));
    assert!(!rs.contains("unsafe impl ::cxx::memory::UniquePtrTarget for Borrowed<'_> {"));
}

#[test]
fn test_unique_ptr_lifetimes_from_explicit_impl() {
    let rs = bridge(quote! {
        mod ffi {
            unsafe extern "C++" {
                type Borrowed<'a>;
            }
            impl<'b> UniquePtr<Borrowed<'c>> {}
        }
    });

    // Lifetimes use the name from the extern type.
    assert!(rs.contains("unsafe impl<'a> ::cxx::ExternType for Borrowed<'a>"));

    // Lifetimes use the names written in the explicit impl if one is present.
    assert!(rs.contains("unsafe impl<'b> ::cxx::memory::UniquePtrTarget for Borrowed<'c>"));
}

#[test]
fn test_vec_string() {
    let rs = bridge(quote! {
        mod ffi {
            extern "Rust" {
                fn foo() -> Vec<String>;
            }
        }
    });

    // No substitution of String <=> ::cxx::private::RustString.
    assert!(rs.contains("__return: *mut ::cxx::private::RustVec<::cxx::alloc::string::String>"));
    assert!(rs.contains("fn __foo() -> ::cxx::alloc::vec::Vec<::cxx::alloc::string::String>"));

    let rs = bridge(quote! {
        mod ffi {
            extern "Rust" {
                fn foo(v: &Vec<String>);
            }
        }
    });

    // No substitution of String <=> ::cxx::private::RustString.
    assert!(rs.contains("v: &::cxx::private::RustVec<::cxx::alloc::string::String>"));
    assert!(rs.contains("fn __foo(v: &::cxx::alloc::vec::Vec<::cxx::alloc::string::String>)"));
}

#[test]
fn test_mangling_covers_cpp_namespace_of_vec_elements() {
    let rs = bridge(quote! {
        mod ffi {
            #[namespace = "test_namespace"]
            struct Context { x: i32 }
            impl Vec<Context> {}
        }
    });

    // Mangling must include Context's C++ namespace to avoid colliding the
    // symbol names for two identically named structs in different namespaces.
    assert!(rs.contains("export_name = \"cxxbridge1$rust_vec$test_namespace$Context$set_len\""));
}

#[test]
fn test_struct_with_lifetime() {
    let rs = bridge(quote! {
        mod ffi {
            struct StructWithLifetime<'a> {
                s: &'a str,
            }
            extern "Rust" {
                fn f(_: UniquePtr<StructWithLifetime<>>);
            }
        }
    });

    // Regression test for <https://github.com/dtolnay/cxx/pull/1658#discussion_r2529463814>
    // which generated this invalid code:
    //
    //     impl<'a> ::cxx::memory::UniquePtrTarget for StructWithLifetime < > < 'a > {
    //
    // Invalid syntax in the output code would already have caused the test
    // helper `bridge` to panic above. But for completeness this assertion
    // verifies the intended code has been generated.
    assert!(rs.contains("impl<'a> ::cxx::memory::UniquePtrTarget for StructWithLifetime<'a> {"));

    // Assertions for other places that refer to `StructWithLifetime`.
    assert!(rs.contains("pub struct StructWithLifetime<'a> {"));
    assert!(rs.contains("cast::<StructWithLifetime<'a>>()"));
    assert!(rs.contains("fn __f(arg0: ::cxx::UniquePtr<StructWithLifetime>) {"));
    assert!(rs.contains("impl<'a> self::Drop for super::StructWithLifetime<'a>"));
}

#[test]
fn test_original_lifetimes_used_in_impls() {
    let rs = bridge(quote! {
        mod ffi {
            struct Context<'sess> {
                session: &'sess str,
            }
            struct Server<'srv> {
                ctx: UniquePtr<Context<'srv>>,
            }
            struct Client<'clt> {
                ctx: UniquePtr<Context<'clt>>,
            }
        }
    });

    // Verify which lifetime name ('sess, 'srv, 'clt) gets used for this impl.
    assert!(rs.contains("impl<'sess> ::cxx::memory::UniquePtrTarget for Context<'sess> {"));
}
