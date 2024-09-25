/// Contains some unsorted functions used across others modules
///
use quote::format_ident;
use std::collections::{HashMap, HashSet};
use unicode_ident::is_xid_continue;

use crate::refident::{MaybeIdent, MaybePat};
use syn::{Attribute, Expr, FnArg, Generics, Ident, ItemFn, Pat, ReturnType, Type, WherePredicate};

/// Return an iterator over fn arguments items.
///
pub(crate) fn fn_args_pats(test: &ItemFn) -> impl Iterator<Item = &Pat> {
    fn_args(test).filter_map(MaybePat::maybe_pat)
}

pub(crate) fn compare_pat(a: &Pat, b: &Pat) -> bool {
    match (a, b) {
        (Pat::Ident(a), Pat::Ident(b)) => a.ident == b.ident,
        (Pat::Tuple(a), Pat::Tuple(b)) => a.elems == b.elems,
        (Pat::TupleStruct(a), Pat::TupleStruct(b)) => a.path == b.path && a.elems == b.elems,
        (Pat::Struct(a), Pat::Struct(b)) => a.path == b.path && a.fields == b.fields,
        _ => false,
    }
}

/// Return if function declaration has an ident
///
pub(crate) fn fn_args_has_pat(fn_decl: &ItemFn, pat: &Pat) -> bool {
    fn_args_pats(fn_decl).any(|id| compare_pat(id, pat))
}

/// Return an iterator over fn arguments.
///
pub(crate) fn fn_args(item_fn: &ItemFn) -> impl Iterator<Item = &FnArg> {
    item_fn.sig.inputs.iter()
}

pub(crate) fn attr_ends_with(attr: &Attribute, segment: &syn::PathSegment) -> bool {
    attr.path().segments.iter().last() == Some(segment)
}

pub(crate) fn attr_starts_with(attr: &Attribute, segment: &syn::PathSegment) -> bool {
    attr.path().segments.iter().next() == Some(segment)
}

pub(crate) fn attr_is(attr: &Attribute, name: &str) -> bool {
    attr.path().is_ident(&format_ident!("{}", name))
}

pub(crate) fn attr_in(attr: &Attribute, names: &[&str]) -> bool {
    names
        .iter()
        .any(|name| attr.path().is_ident(&format_ident!("{}", name)))
}

pub(crate) trait IsLiteralExpression {
    fn is_literal(&self) -> bool;
}

impl<E: AsRef<Expr>> IsLiteralExpression for E {
    fn is_literal(&self) -> bool {
        matches!(
            self.as_ref(),
            Expr::Lit(syn::ExprLit {
                lit: syn::Lit::Str(_),
                ..
            })
        )
    }
}

// Recoursive search id by reference till find one in ends
fn _is_used(
    visited: &mut HashSet<Ident>,
    id: &Ident,
    references: &HashMap<Ident, HashSet<Ident>>,
    ends: &HashSet<Ident>,
) -> bool {
    if visited.contains(id) {
        return false;
    }
    visited.insert(id.clone());
    if ends.contains(id) {
        return true;
    }
    if references.contains_key(id) {
        for referred in references.get(id).unwrap() {
            if _is_used(visited, referred, references, ends) {
                return true;
            }
        }
    }
    false
}

// Recoursive search id by reference till find one in ends
fn is_used(id: &Ident, references: &HashMap<Ident, HashSet<Ident>>, ends: &HashSet<Ident>) -> bool {
    let mut visited = Default::default();
    _is_used(&mut visited, id, references, ends)
}

impl MaybeIdent for syn::WherePredicate {
    fn maybe_ident(&self) -> Option<&Ident> {
        match self {
            WherePredicate::Type(syn::PredicateType { bounded_ty: t, .. }) => {
                first_type_path_segment_ident(t)
            }
            WherePredicate::Lifetime(syn::PredicateLifetime { lifetime, .. }) => {
                Some(&lifetime.ident)
            }
            _ => None,
        }
    }
}

#[derive(Default)]
struct SearchSimpleTypeName(HashSet<Ident>);

impl SearchSimpleTypeName {
    fn take(self) -> HashSet<Ident> {
        self.0
    }

    fn visit_inputs<'a>(&mut self, inputs: impl Iterator<Item = &'a FnArg>) {
        use syn::visit::Visit;
        inputs.for_each(|fn_arg| self.visit_fn_arg(fn_arg));
    }
    fn visit_output(&mut self, output: &ReturnType) {
        use syn::visit::Visit;
        self.visit_return_type(output);
    }

    fn collect_from_type_param(tp: &syn::TypeParam) -> Self {
        let mut s: Self = Default::default();
        use syn::visit::Visit;
        s.visit_type_param(tp);
        s
    }

    fn collect_from_where_predicate(wp: &syn::WherePredicate) -> Self {
        let mut s: Self = Default::default();
        use syn::visit::Visit;
        s.visit_where_predicate(wp);
        s
    }
}

impl<'ast> syn::visit::Visit<'ast> for SearchSimpleTypeName {
    fn visit_path(&mut self, p: &'ast syn::Path) {
        if let Some(id) = p.get_ident() {
            self.0.insert(id.clone());
        }
        syn::visit::visit_path(self, p)
    }

    fn visit_lifetime(&mut self, i: &'ast syn::Lifetime) {
        self.0.insert(i.ident.clone());
        syn::visit::visit_lifetime(self, i)
    }
}

// Take generics definitions and where clauses and return the
// a map from simple types (lifetime names or type with just names)
// to a set of all simple types that use it as some costrain.
fn extract_references_map(generics: &Generics) -> HashMap<Ident, HashSet<Ident>> {
    let mut references = HashMap::<Ident, HashSet<Ident>>::default();
    // Extracts references from types param
    generics.type_params().for_each(|tp| {
        SearchSimpleTypeName::collect_from_type_param(tp)
            .take()
            .into_iter()
            .for_each(|id| {
                references.entry(id).or_default().insert(tp.ident.clone());
            });
    });
    // Extracts references from where clauses
    generics
        .where_clause
        .iter()
        .flat_map(|wc| wc.predicates.iter())
        .filter_map(|wp| wp.maybe_ident().map(|id| (id, wp)))
        .for_each(|(ref_ident, wp)| {
            SearchSimpleTypeName::collect_from_where_predicate(wp)
                .take()
                .into_iter()
                .for_each(|id| {
                    references.entry(id).or_default().insert(ref_ident.clone());
                });
        });
    references
}

// Return a hash set that contains all types and lifetimes referenced
// in input/output expressed by a single ident.
fn references_ident_types<'a>(
    generics: &Generics,
    inputs: impl Iterator<Item = &'a FnArg>,
    output: &ReturnType,
) -> HashSet<Ident> {
    let mut used: SearchSimpleTypeName = Default::default();
    used.visit_output(output);
    used.visit_inputs(inputs);
    let references = extract_references_map(generics);
    let mut used = used.take();
    let input_output = used.clone();
    // Extend the input output collected ref with the transitive ones:
    used.extend(
        generics
            .params
            .iter()
            .filter_map(MaybeIdent::maybe_ident)
            .filter(|&id| is_used(id, &references, &input_output))
            .cloned(),
    );
    used
}

fn filtered_predicates(mut wc: syn::WhereClause, valids: &HashSet<Ident>) -> syn::WhereClause {
    wc.predicates = wc
        .predicates
        .clone()
        .into_iter()
        .filter(|wp| {
            wp.maybe_ident()
                .map(|t| valids.contains(t))
                .unwrap_or_default()
        })
        .collect();
    wc
}

fn filtered_generics<'a>(
    params: impl Iterator<Item = syn::GenericParam> + 'a,
    valids: &'a HashSet<Ident>,
) -> impl Iterator<Item = syn::GenericParam> + 'a {
    params.filter(move |p| match p.maybe_ident() {
        Some(id) => valids.contains(id),
        None => false,
    })
}

//noinspection RsTypeCheck
pub(crate) fn generics_clean_up<'a>(
    original: &Generics,
    inputs: impl Iterator<Item = &'a FnArg>,
    output: &ReturnType,
) -> syn::Generics {
    let used = references_ident_types(original, inputs, output);
    let mut result: Generics = original.clone();
    result.params = filtered_generics(result.params.into_iter(), &used).collect();
    result.where_clause = result.where_clause.map(|wc| filtered_predicates(wc, &used));
    result
}

// If type is not self and doesn't starts with :: return the first ident
// of its path segment: only if is a simple path.
// If type is a simple ident just return the this ident. That is useful to
// find the base type for associate type indication
fn first_type_path_segment_ident(t: &Type) -> Option<&Ident> {
    match t {
        Type::Path(tp) if tp.qself.is_none() && tp.path.leading_colon.is_none() => tp
            .path
            .segments
            .iter()
            .next()
            .and_then(|ps| match ps.arguments {
                syn::PathArguments::None => Some(&ps.ident),
                _ => None,
            }),
        _ => None,
    }
}

pub(crate) fn fn_arg_mutability(arg: &FnArg) -> Option<syn::token::Mut> {
    match arg {
        FnArg::Typed(syn::PatType { pat, .. }) => match pat.as_ref() {
            syn::Pat::Ident(syn::PatIdent { mutability, .. }) => *mutability,
            _ => None,
        },
        _ => None,
    }
}

pub(crate) fn sanitize_ident(name: &str) -> String {
    name.chars()
        .filter(|c| !c.is_whitespace())
        .map(|c| match c {
            '"' | '\'' => "__".to_owned(),
            ':' | '(' | ')' | '{' | '}' | '[' | ']' | ',' | '.' | '*' | '+' | '/' | '-' | '%'
            | '^' | '!' | '&' | '|' => "_".to_owned(),
            _ => c.to_string(),
        })
        .collect::<String>()
        .chars()
        .filter(|&c| is_xid_continue(c))
        .collect()
}

#[cfg(test)]
mod test {
    use syn::parse_quote;

    use super::*;
    use crate::test::{assert_eq, *};

    #[test]
    fn fn_args_has_pat_should() {
        let item_fn = parse_quote! {
            fn the_function(first: u32, second: u32) {}
        };

        assert!(fn_args_has_pat(&item_fn, &pat("first")));
        assert!(!fn_args_has_pat(&item_fn, &pat("third")));
    }

    #[rstest]
    #[case::base("fn foo<A, B, C>(a: A) -> B {}", &["A", "B"])]
    #[case::use_const_in_array("fn foo<A, const B: usize, C>(a: A) -> [u32; B] {}", &["A", "B", "u32"])]
    #[case::in_type_args("fn foo<A, const B: usize, C>(a: A) -> SomeType<B> {}", &["A", "B"])]
    #[case::in_type_args("fn foo<A, const B: usize, C>(a: SomeType<A>, b: SomeType<B>) {}", &["A", "B"])]
    #[case::pointers("fn foo<A, B, C>(a: *const A, b: &B) {}", &["A", "B"])]
    #[case::lifetime("fn foo<'a, A, B, C>(a: A, b: &'a B) {}", &["a", "A", "B"])]
    #[case::transitive_lifetime("fn foo<'a, A, B, C>(a: A, b: B) where B: Iterator<Item=A> + 'a {}", &["a", "A", "B"])]
    #[case::associated("fn foo<'a, A:Copy, C>(b: impl Iterator<Item=A> + 'a) {}", &["a", "A"])]
    #[case::transitive_in_defs("fn foo<A:Copy, B: Iterator<Item=A>>(b: B) {}", &["A", "B"])]
    #[case::transitive_in_where("fn foo<A:Copy, B>(b: B) where B: Iterator<Item=A> {}", &["A", "B"])]
    #[case::transitive_const("fn foo<const A: usize, B, C>(b: B) where B: Some<A> {}", &["A", "B"])]
    #[case::transitive_lifetime("fn foo<'a, A, B, C>(a: A, b: B) where B: Iterator<Item=A> + 'a {}", &["a", "A", "B"])]
    #[case::transitive_lifetime(r#"fn foo<'a, 'b, 'c, 'd, A, B, C>
        (a: A, b: B) 
        where B: Iterator<Item=A> + 'c, 
        'c: 'a + 'b {}"#, &["a", "b", "c", "A", "B"])]
    fn references_ident_types_should(#[case] f: &str, #[case] expected: &[&str]) {
        let f: ItemFn = f.ast();
        let used = references_ident_types(&f.sig.generics, f.sig.inputs.iter(), &f.sig.output);

        let expected = to_idents!(expected)
            .into_iter()
            .collect::<std::collections::HashSet<_>>();

        assert_eq!(expected, used);
    }

    #[rstest]
    #[case::remove_not_in_output(
        r#"fn test<R: AsRef<str>, B, F, H: Iterator<Item=u32>>() -> (H, B, String, &str)
                        where F: ToString,
                        B: Borrow<u32>
                        {}"#,
        r#"fn test<B, H: Iterator<Item=u32>>() -> (H, B, String, &str)
                        where B: Borrow<u32>
                {}"#
    )]
    #[case::not_remove_used_in_arguments(
        r#"fn test<R: AsRef<str>, B, F, H: Iterator<Item=u32>>
                    (h: H, it: impl Iterator<Item=R>, j: &[B])
                    where F: ToString,
                    B: Borrow<u32>
                {}"#,
        r#"fn test<R: AsRef<str>, B, H: Iterator<Item=u32>>
                    (h: H, it: impl Iterator<Item=R>, j: &[B])
                    where
                    B: Borrow<u32>
                {}"#
    )]
    #[case::dont_remove_transitive(
        r#"fn test<A, B, C, D, const F: usize, O>(a: A) where 
            B: AsRef<C>,
            A: Iterator<Item=[B; F]>,
            D: ArsRef<O> {}"#,
        r#"fn test<A, B, C, const F: usize>(a: A) where 
            B: AsRef<C>,
            A: Iterator<Item=[B; F]> {}"#
    )]
    #[case::remove_unused_lifetime(
        "fn test<'a, 'b, 'c, 'd, 'e, 'f, 'g, A>(a: &'a uint32, b: impl AsRef<A> + 'b) where 'b: 'c + 'd, A: Copy + 'e, 'f: 'g {}",
        "fn test<'a, 'b, 'c, 'd, 'e, A>(a: &'a uint32, b: impl AsRef<A> + 'b) where 'b: 'c + 'd, A: Copy + 'e {}"
    )]
    #[case::remove_unused_const(
        r#"fn test<const A: usize, const B: usize, const C: usize, const D: usize, T, O>
            (a: [u32; A], b: SomeType<B>, c: T) where 
            T: Iterator<Item=[i32; C]>,
            O: AsRef<D> 
            {}"#,
        r#"fn test<const A: usize, const B: usize, const C: usize, T>
            (a: [u32; A], b: SomeType<B>, c: T) where 
            T: Iterator<Item=[i32; C]>
            {}"#
    )]
    fn generics_cleaner(#[case] code: &str, #[case] expected: &str) {
        // Should remove all generics parameters that are not present in output
        let item_fn: ItemFn = code.ast();

        let expected: ItemFn = expected.ast();

        let cleaned = generics_clean_up(
            &item_fn.sig.generics,
            item_fn.sig.inputs.iter(),
            &item_fn.sig.output,
        );

        assert_eq!(expected.sig.generics, cleaned);
    }

    #[rstest]
    #[case("1", "1")]
    #[case(r#""1""#, "__1__")]
    #[case(r#"Some::SomeElse"#, "Some__SomeElse")]
    #[case(r#""minnie".to_owned()"#, "__minnie___to_owned__")]
    #[case(
        r#"vec![1 ,   2, 
    3]"#,
        "vec__1_2_3_"
    )]
    #[case(
        r#"some_macro!("first", {second}, [third])"#,
        "some_macro____first____second___third__"
    )]
    #[case(r#"'x'"#, "__x__")]
    #[case::ops(r#"a*b+c/d-e%f^g"#, "a_b_c_d_e_f_g")]
    fn sanitaze_ident_name(#[case] expression: impl AsRef<str>, #[case] expected: impl AsRef<str>) {
        assert_eq!(expected.as_ref(), sanitize_ident(expression.as_ref()));
    }
}
