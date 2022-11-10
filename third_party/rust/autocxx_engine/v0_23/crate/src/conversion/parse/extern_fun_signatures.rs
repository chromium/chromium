// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::IndexSet as HashSet;

use syn::{
    spanned::Spanned, AngleBracketedGenericArguments, GenericArgument, PatType, PathArguments,
    PathSegment, ReturnType, Signature, Type, TypePath, TypeReference,
};

use crate::{
    conversion::convert_error::{ConvertErrorFromRust, LocatedConvertErrorFromRust},
    types::QualifiedName,
};

pub(super) fn assemble_extern_fun_deps(
    sig: &Signature,
    file: &str,
) -> Result<Vec<QualifiedName>, LocatedConvertErrorFromRust> {
    let mut deps = HashSet::new();
    // It's possible that this will need to be implemented using TypeConverter
    // and the encountered_types field on its annotated results.
    // But the design of that code is intended to convert from C++ types
    // (via bindgen) to cxx types, and instead here we're starting with pure
    // Rust types as written by a Rustacean human. It may therefore not
    // be quite right to go via TypeConverter.
    // Also, by doing it ourselves here, we're in a better place to emit
    // meaningful errors about types which can't be supported within
    // extern_rust_fun.
    if let ReturnType::Type(_, ty) = &sig.output {
        add_type_to_deps(ty, &mut deps, file)?;
    }
    for input in &sig.inputs {
        match input {
            syn::FnArg::Receiver(_) => {
                return Err(LocatedConvertErrorFromRust::new(
                    ConvertErrorFromRust::ExternRustFunRequiresFullyQualifiedReceiver,
                    &input.span(),
                    file,
                ))
            }
            syn::FnArg::Typed(PatType { ty, .. }) => add_type_to_deps(ty, &mut deps, file)?,
        }
    }
    Ok(deps.into_iter().collect())
}

/// For all types within an extern_rust_function signature, add them to the deps
/// hash, or raise an appropriate error.
fn add_type_to_deps(
    ty: &Type,
    deps: &mut HashSet<QualifiedName>,
    file: &str,
) -> Result<(), LocatedConvertErrorFromRust> {
    match ty {
        Type::Reference(TypeReference {
            mutability: Some(_),
            ..
        }) => {
            return Err(LocatedConvertErrorFromRust::new(
                ConvertErrorFromRust::PinnedReferencesRequiredForExternFun,
                &ty.span(),
                file,
            ))
        }
        Type::Reference(TypeReference { elem, .. }) => match &**elem {
            Type::Path(tp) => add_path_to_deps(tp, deps, file)?,
            _ => {
                return Err(LocatedConvertErrorFromRust::new(
                    ConvertErrorFromRust::UnsupportedTypeForExternFun,
                    &ty.span(),
                    file,
                ))
            }
        },
        Type::Path(tp) => {
            if tp.path.segments.len() != 1 {
                return Err(LocatedConvertErrorFromRust::new(
                    ConvertErrorFromRust::NamespacesNotSupportedForExternFun,
                    &tp.span(),
                    file,
                ));
            }
            if let Some(PathSegment {
                ident,
                arguments:
                    PathArguments::AngleBracketed(AngleBracketedGenericArguments { args, .. }),
            }) = tp.path.segments.last()
            {
                if ident == "Pin" {
                    if args.len() != 1 {
                        return Err(LocatedConvertErrorFromRust::new(
                            ConvertErrorFromRust::UnsupportedTypeForExternFun,
                            &tp.span(),
                            file,
                        ));
                    }

                    if let Some(GenericArgument::Type(Type::Reference(TypeReference {
                        mutability: Some(_),
                        elem,
                        ..
                    }))) = args.first()
                    {
                        if let Type::Path(tp) = &**elem {
                            add_path_to_deps(tp, deps, file)?
                        } else {
                            return Err(LocatedConvertErrorFromRust::new(
                                ConvertErrorFromRust::UnsupportedTypeForExternFun,
                                &elem.span(),
                                file,
                            ));
                        }
                    } else {
                        return Err(LocatedConvertErrorFromRust::new(
                            ConvertErrorFromRust::UnsupportedTypeForExternFun,
                            &ty.span(),
                            file,
                        ));
                    }
                } else if ident == "Box" || ident == "Vec" {
                    if args.len() != 1 {
                        return Err(LocatedConvertErrorFromRust::new(
                            ConvertErrorFromRust::UnsupportedTypeForExternFun,
                            &tp.span(),
                            file,
                        ));
                    }
                    if let Some(GenericArgument::Type(Type::Path(tp))) = args.first() {
                        add_path_to_deps(tp, deps, file)?
                    } else {
                        return Err(LocatedConvertErrorFromRust::new(
                            ConvertErrorFromRust::UnsupportedTypeForExternFun,
                            &ty.span(),
                            file,
                        ));
                    }
                } else {
                    return Err(LocatedConvertErrorFromRust::new(
                        ConvertErrorFromRust::UnsupportedTypeForExternFun,
                        &ident.span(),
                        file,
                    ));
                }
            } else {
                add_path_to_deps(tp, deps, file)?
            }
        }
        _ => {
            return Err(LocatedConvertErrorFromRust::new(
                ConvertErrorFromRust::UnsupportedTypeForExternFun,
                &ty.span(),
                file,
            ))
        }
    };
    Ok(())
}

fn add_path_to_deps(
    type_path: &TypePath,
    deps: &mut HashSet<QualifiedName>,
    file: &str,
) -> Result<(), LocatedConvertErrorFromRust> {
    if let Some(PathSegment {
        arguments: PathArguments::AngleBracketed(..) | PathArguments::Parenthesized(..),
        ..
    }) = type_path.path.segments.last()
    {
        return Err(LocatedConvertErrorFromRust::new(
            ConvertErrorFromRust::UnsupportedTypeForExternFun,
            &type_path.span(),
            file,
        ));
    }
    let qn = QualifiedName::from_type_path(type_path);
    if !qn.get_namespace().is_empty() {
        return Err(LocatedConvertErrorFromRust::new(
            ConvertErrorFromRust::NamespacesNotSupportedForExternFun,
            &type_path.span(),
            file,
        ));
    }
    if qn.get_final_item() == "Self" {
        return Err(LocatedConvertErrorFromRust::new(
            ConvertErrorFromRust::ExplicitSelf,
            &type_path.span(),
            file,
        ));
    }
    deps.insert(qn);
    Ok(())
}

#[cfg(test)]
mod tests {
    use syn::parse_quote;

    use super::*;

    fn run_test_expect_ok(sig: Signature, expected_deps: &[&str]) {
        let expected_as_set: HashSet<QualifiedName> = expected_deps
            .iter()
            .cloned()
            .map(QualifiedName::new_from_cpp_name)
            .collect();
        let result = assemble_extern_fun_deps(&sig, "").unwrap();
        let actual_as_set: HashSet<QualifiedName> = result.into_iter().collect();
        assert_eq!(expected_as_set, actual_as_set);
    }

    fn run_test_expect_fail(sig: Signature) {
        assert!(assemble_extern_fun_deps(&sig, "").is_err())
    }

    #[test]
    fn test_assemble_extern_fun_deps() {
        run_test_expect_fail(parse_quote! { fn function(self: A::B)});
        run_test_expect_fail(parse_quote! { fn function(self: Self)});
        run_test_expect_fail(parse_quote! { fn function(self: Self)});
        run_test_expect_fail(parse_quote! { fn function(self)});
        run_test_expect_fail(parse_quote! { fn function(&self)});
        run_test_expect_fail(parse_quote! { fn function(&mut self)});
        run_test_expect_fail(parse_quote! { fn function(self: Pin<&mut Self>)});
        run_test_expect_fail(parse_quote! { fn function(self: Pin<&mut A::B>)});
        run_test_expect_fail(parse_quote! { fn function(a: Pin<A>)});
        run_test_expect_fail(parse_quote! { fn function(a: Pin<A::B>)});
        run_test_expect_fail(parse_quote! { fn function(a: A::B)});
        run_test_expect_fail(parse_quote! { fn function(a: &mut A)});
        run_test_expect_fail(parse_quote! { fn function() -> A::B});
        run_test_expect_fail(parse_quote! { fn function() -> &A::B});
        run_test_expect_fail(parse_quote! { fn function(a: ())});
        run_test_expect_fail(parse_quote! { fn function(a: &[A])});
        run_test_expect_fail(parse_quote! { fn function(a: Bob<A>)});
        run_test_expect_fail(parse_quote! { fn function(a: Box<A, B>)});
        run_test_expect_fail(parse_quote! { fn function(a: a::Pin<&mut A>)});
        run_test_expect_fail(parse_quote! { fn function(a: Pin<&A>)});
        run_test_expect_ok(parse_quote! { fn function(a: A, b: B)}, &["A", "B"]);
        run_test_expect_ok(parse_quote! { fn function(a: Box<A>)}, &["A"]);
        run_test_expect_ok(parse_quote! { fn function(a: Vec<A>)}, &["A"]);
        run_test_expect_ok(parse_quote! { fn function(a: &A)}, &["A"]);
        run_test_expect_ok(parse_quote! { fn function(a: Pin<&mut A>)}, &["A"]);
        run_test_expect_ok(
            parse_quote! { fn function(a: A, b: B) -> Box<C>},
            &["A", "B", "C"],
        );
    }
}
