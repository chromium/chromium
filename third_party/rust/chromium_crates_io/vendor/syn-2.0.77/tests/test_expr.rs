#![allow(clippy::single_element_loop, clippy::uninlined_format_args)]

#[macro_use]
mod macros;

use proc_macro2::{Delimiter, Group};
use quote::{quote, ToTokens as _};
use std::mem;
use syn::punctuated::Punctuated;
use syn::visit_mut::{self, VisitMut};
use syn::{parse_quote, token, Expr, ExprRange, ExprTuple, Stmt, Token};

#[test]
fn test_expr_parse() {
    let tokens = quote!(..100u32);
    snapshot!(tokens as Expr, @r###"
    Expr::Range {
        limits: RangeLimits::HalfOpen,
        end: Some(Expr::Lit {
            lit: 100u32,
        }),
    }
    "###);

    let tokens = quote!(..100u32);
    snapshot!(tokens as ExprRange, @r###"
    ExprRange {
        limits: RangeLimits::HalfOpen,
        end: Some(Expr::Lit {
            lit: 100u32,
        }),
    }
    "###);
}

#[test]
fn test_await() {
    // Must not parse as Expr::Field.
    let tokens = quote!(fut.await);

    snapshot!(tokens as Expr, @r###"
    Expr::Await {
        base: Expr::Path {
            path: Path {
                segments: [
                    PathSegment {
                        ident: "fut",
                    },
                ],
            },
        },
    }
    "###);
}

#[rustfmt::skip]
#[test]
fn test_tuple_multi_index() {
    let expected = snapshot!("tuple.0.0" as Expr, @r###"
    Expr::Field {
        base: Expr::Field {
            base: Expr::Path {
                path: Path {
                    segments: [
                        PathSegment {
                            ident: "tuple",
                        },
                    ],
                },
            },
            member: Member::Unnamed(Index {
                index: 0,
            }),
        },
        member: Member::Unnamed(Index {
            index: 0,
        }),
    }
    "###);

    for &input in &[
        "tuple .0.0",
        "tuple. 0.0",
        "tuple.0 .0",
        "tuple.0. 0",
        "tuple . 0 . 0",
    ] {
        assert_eq!(expected, syn::parse_str(input).unwrap());
    }

    for tokens in [
        quote!(tuple.0.0),
        quote!(tuple .0.0),
        quote!(tuple. 0.0),
        quote!(tuple.0 .0),
        quote!(tuple.0. 0),
        quote!(tuple . 0 . 0),
    ] {
        assert_eq!(expected, syn::parse2(tokens).unwrap());
    }
}

#[test]
fn test_macro_variable_func() {
    // mimics the token stream corresponding to `$fn()`
    let path = Group::new(Delimiter::None, quote!(f));
    let tokens = quote!(#path());

    snapshot!(tokens as Expr, @r###"
    Expr::Call {
        func: Expr::Group {
            expr: Expr::Path {
                path: Path {
                    segments: [
                        PathSegment {
                            ident: "f",
                        },
                    ],
                },
            },
        },
    }
    "###);

    let path = Group::new(Delimiter::None, quote! { #[inside] f });
    let tokens = quote!(#[outside] #path());

    snapshot!(tokens as Expr, @r###"
    Expr::Call {
        attrs: [
            Attribute {
                style: AttrStyle::Outer,
                meta: Meta::Path {
                    segments: [
                        PathSegment {
                            ident: "outside",
                        },
                    ],
                },
            },
        ],
        func: Expr::Group {
            expr: Expr::Path {
                attrs: [
                    Attribute {
                        style: AttrStyle::Outer,
                        meta: Meta::Path {
                            segments: [
                                PathSegment {
                                    ident: "inside",
                                },
                            ],
                        },
                    },
                ],
                path: Path {
                    segments: [
                        PathSegment {
                            ident: "f",
                        },
                    ],
                },
            },
        },
    }
    "###);
}

#[test]
fn test_macro_variable_macro() {
    // mimics the token stream corresponding to `$macro!()`
    let mac = Group::new(Delimiter::None, quote!(m));
    let tokens = quote!(#mac!());

    snapshot!(tokens as Expr, @r###"
    Expr::Macro {
        mac: Macro {
            path: Path {
                segments: [
                    PathSegment {
                        ident: "m",
                    },
                ],
            },
            delimiter: MacroDelimiter::Paren,
            tokens: TokenStream(``),
        },
    }
    "###);
}

#[test]
fn test_macro_variable_struct() {
    // mimics the token stream corresponding to `$struct {}`
    let s = Group::new(Delimiter::None, quote! { S });
    let tokens = quote!(#s {});

    snapshot!(tokens as Expr, @r###"
    Expr::Struct {
        path: Path {
            segments: [
                PathSegment {
                    ident: "S",
                },
            ],
        },
    }
    "###);
}

#[test]
fn test_macro_variable_unary() {
    // mimics the token stream corresponding to `$expr.method()` where expr is `&self`
    let inner = Group::new(Delimiter::None, quote!(&self));
    let tokens = quote!(#inner.method());
    snapshot!(tokens as Expr, @r###"
    Expr::MethodCall {
        receiver: Expr::Group {
            expr: Expr::Reference {
                expr: Expr::Path {
                    path: Path {
                        segments: [
                            PathSegment {
                                ident: "self",
                            },
                        ],
                    },
                },
            },
        },
        method: "method",
    }
    "###);
}

#[test]
fn test_macro_variable_match_arm() {
    // mimics the token stream corresponding to `match v { _ => $expr }`
    let expr = Group::new(Delimiter::None, quote! { #[a] () });
    let tokens = quote!(match v { _ => #expr });
    snapshot!(tokens as Expr, @r###"
    Expr::Match {
        expr: Expr::Path {
            path: Path {
                segments: [
                    PathSegment {
                        ident: "v",
                    },
                ],
            },
        },
        arms: [
            Arm {
                pat: Pat::Wild,
                body: Expr::Group {
                    expr: Expr::Tuple {
                        attrs: [
                            Attribute {
                                style: AttrStyle::Outer,
                                meta: Meta::Path {
                                    segments: [
                                        PathSegment {
                                            ident: "a",
                                        },
                                    ],
                                },
                            },
                        ],
                    },
                },
            },
        ],
    }
    "###);

    let expr = Group::new(Delimiter::None, quote!(loop {} + 1));
    let tokens = quote!(match v { _ => #expr });
    snapshot!(tokens as Expr, @r###"
    Expr::Match {
        expr: Expr::Path {
            path: Path {
                segments: [
                    PathSegment {
                        ident: "v",
                    },
                ],
            },
        },
        arms: [
            Arm {
                pat: Pat::Wild,
                body: Expr::Group {
                    expr: Expr::Binary {
                        left: Expr::Loop {
                            body: Block {
                                stmts: [],
                            },
                        },
                        op: BinOp::Add,
                        right: Expr::Lit {
                            lit: 1,
                        },
                    },
                },
            },
        ],
    }
    "###);
}

// https://github.com/dtolnay/syn/issues/1019
#[test]
fn test_closure_vs_rangefull() {
    #[rustfmt::skip] // rustfmt bug: https://github.com/rust-lang/rustfmt/issues/4808
    let tokens = quote!(|| .. .method());
    snapshot!(tokens as Expr, @r###"
    Expr::MethodCall {
        receiver: Expr::Closure {
            output: ReturnType::Default,
            body: Expr::Range {
                limits: RangeLimits::HalfOpen,
            },
        },
        method: "method",
    }
    "###);
}

#[test]
fn test_postfix_operator_after_cast() {
    syn::parse_str::<Expr>("|| &x as T[0]").unwrap_err();
    syn::parse_str::<Expr>("|| () as ()()").unwrap_err();
}

#[test]
fn test_range_kinds() {
    syn::parse_str::<Expr>("..").unwrap();
    syn::parse_str::<Expr>("..hi").unwrap();
    syn::parse_str::<Expr>("lo..").unwrap();
    syn::parse_str::<Expr>("lo..hi").unwrap();

    syn::parse_str::<Expr>("..=").unwrap_err();
    syn::parse_str::<Expr>("..=hi").unwrap();
    syn::parse_str::<Expr>("lo..=").unwrap_err();
    syn::parse_str::<Expr>("lo..=hi").unwrap();

    syn::parse_str::<Expr>("...").unwrap_err();
    syn::parse_str::<Expr>("...hi").unwrap_err();
    syn::parse_str::<Expr>("lo...").unwrap_err();
    syn::parse_str::<Expr>("lo...hi").unwrap_err();
}

#[test]
fn test_range_precedence() {
    snapshot!(".. .." as Expr, @r###"
    Expr::Range {
        limits: RangeLimits::HalfOpen,
        end: Some(Expr::Range {
            limits: RangeLimits::HalfOpen,
        }),
    }
    "###);

    snapshot!(".. .. ()" as Expr, @r###"
    Expr::Range {
        limits: RangeLimits::HalfOpen,
        end: Some(Expr::Range {
            limits: RangeLimits::HalfOpen,
            end: Some(Expr::Tuple),
        }),
    }
    "###);

    snapshot!("() .. .." as Expr, @r###"
    Expr::Range {
        start: Some(Expr::Tuple),
        limits: RangeLimits::HalfOpen,
        end: Some(Expr::Range {
            limits: RangeLimits::HalfOpen,
        }),
    }
    "###);

    // A range with a lower bound cannot be the upper bound of another range,
    // and a range with an upper bound cannot be the lower bound of another
    // range.
    syn::parse_str::<Expr>(".. x ..").unwrap_err();
    syn::parse_str::<Expr>("x .. x ..").unwrap_err();
}

#[test]
fn test_ambiguous_label() {
    for stmt in [
        quote! {
            return 'label: loop { break 'label 42; };
        },
        quote! {
            break ('label: loop { break 'label 42; });
        },
        quote! {
            break 1 + 'label: loop { break 'label 42; };
        },
        quote! {
            break 'outer 'inner: loop { break 'inner 42; };
        },
    ] {
        syn::parse2::<Stmt>(stmt).unwrap();
    }

    for stmt in [
        // Parentheses required. See https://github.com/rust-lang/rust/pull/87026.
        quote! {
            break 'label: loop { break 'label 42; };
        },
    ] {
        syn::parse2::<Stmt>(stmt).unwrap_err();
    }
}

#[test]
fn test_extended_interpolated_path() {
    let path = Group::new(Delimiter::None, quote!(a::b));

    let tokens = quote!(if #path {});
    snapshot!(tokens as Expr, @r###"
    Expr::If {
        cond: Expr::Group {
            expr: Expr::Path {
                path: Path {
                    segments: [
                        PathSegment {
                            ident: "a",
                        },
                        Token![::],
                        PathSegment {
                            ident: "b",
                        },
                    ],
                },
            },
        },
        then_branch: Block {
            stmts: [],
        },
    }
    "###);

    let tokens = quote!(#path {});
    snapshot!(tokens as Expr, @r###"
    Expr::Struct {
        path: Path {
            segments: [
                PathSegment {
                    ident: "a",
                },
                Token![::],
                PathSegment {
                    ident: "b",
                },
            ],
        },
    }
    "###);

    let tokens = quote!(#path :: c);
    snapshot!(tokens as Expr, @r###"
    Expr::Path {
        path: Path {
            segments: [
                PathSegment {
                    ident: "a",
                },
                Token![::],
                PathSegment {
                    ident: "b",
                },
                Token![::],
                PathSegment {
                    ident: "c",
                },
            ],
        },
    }
    "###);

    let nested = Group::new(Delimiter::None, quote!(a::b || true));
    let tokens = quote!(if #nested && false {});
    snapshot!(tokens as Expr, @r###"
    Expr::If {
        cond: Expr::Binary {
            left: Expr::Group {
                expr: Expr::Binary {
                    left: Expr::Path {
                        path: Path {
                            segments: [
                                PathSegment {
                                    ident: "a",
                                },
                                Token![::],
                                PathSegment {
                                    ident: "b",
                                },
                            ],
                        },
                    },
                    op: BinOp::Or,
                    right: Expr::Lit {
                        lit: Lit::Bool {
                            value: true,
                        },
                    },
                },
            },
            op: BinOp::And,
            right: Expr::Lit {
                lit: Lit::Bool {
                    value: false,
                },
            },
        },
        then_branch: Block {
            stmts: [],
        },
    }
    "###);
}

#[test]
fn test_tuple_comma() {
    let mut expr = ExprTuple {
        attrs: Vec::new(),
        paren_token: token::Paren::default(),
        elems: Punctuated::new(),
    };
    snapshot!(expr.to_token_stream() as Expr, @"Expr::Tuple");

    expr.elems.push_value(parse_quote!(continue));
    // Must not parse to Expr::Paren
    snapshot!(expr.to_token_stream() as Expr, @r###"
    Expr::Tuple {
        elems: [
            Expr::Continue,
            Token![,],
        ],
    }
    "###);

    expr.elems.push_punct(<Token![,]>::default());
    snapshot!(expr.to_token_stream() as Expr, @r###"
    Expr::Tuple {
        elems: [
            Expr::Continue,
            Token![,],
        ],
    }
    "###);

    expr.elems.push_value(parse_quote!(continue));
    snapshot!(expr.to_token_stream() as Expr, @r###"
    Expr::Tuple {
        elems: [
            Expr::Continue,
            Token![,],
            Expr::Continue,
        ],
    }
    "###);

    expr.elems.push_punct(<Token![,]>::default());
    snapshot!(expr.to_token_stream() as Expr, @r###"
    Expr::Tuple {
        elems: [
            Expr::Continue,
            Token![,],
            Expr::Continue,
            Token![,],
        ],
    }
    "###);
}

#[test]
fn test_binop_associativity() {
    // Left to right.
    snapshot!("() + () + ()" as Expr, @r###"
    Expr::Binary {
        left: Expr::Binary {
            left: Expr::Tuple,
            op: BinOp::Add,
            right: Expr::Tuple,
        },
        op: BinOp::Add,
        right: Expr::Tuple,
    }
    "###);

    // Right to left.
    snapshot!("() += () += ()" as Expr, @r###"
    Expr::Binary {
        left: Expr::Tuple,
        op: BinOp::AddAssign,
        right: Expr::Binary {
            left: Expr::Tuple,
            op: BinOp::AddAssign,
            right: Expr::Tuple,
        },
    }
    "###);

    // Parenthesization is required.
    syn::parse_str::<Expr>("() == () == ()").unwrap_err();
}

#[test]
fn test_assign_range_precedence() {
    // Range has higher precedence as the right-hand of an assignment, but
    // ambiguous precedence as the left-hand of an assignment.
    snapshot!("() = () .. ()" as Expr, @r###"
    Expr::Assign {
        left: Expr::Tuple,
        right: Expr::Range {
            start: Some(Expr::Tuple),
            limits: RangeLimits::HalfOpen,
            end: Some(Expr::Tuple),
        },
    }
    "###);

    snapshot!("() += () .. ()" as Expr, @r###"
    Expr::Binary {
        left: Expr::Tuple,
        op: BinOp::AddAssign,
        right: Expr::Range {
            start: Some(Expr::Tuple),
            limits: RangeLimits::HalfOpen,
            end: Some(Expr::Tuple),
        },
    }
    "###);

    syn::parse_str::<Expr>("() .. () = ()").unwrap_err();
    syn::parse_str::<Expr>("() .. () += ()").unwrap_err();
}

#[test]
fn test_fixup() {
    struct FlattenParens;

    impl VisitMut for FlattenParens {
        fn visit_expr_mut(&mut self, e: &mut Expr) {
            while let Expr::Paren(paren) = e {
                *e = mem::replace(&mut *paren.expr, Expr::PLACEHOLDER);
            }
            visit_mut::visit_expr_mut(self, e);
        }
    }

    for tokens in [
        quote! { 2 * (1 + 1) },
        quote! { 0 + (0 + 0) },
        quote! { (a = b) = c },
        quote! { (x as i32) < 0 },
        quote! { 1 + (x as i32) < 0 },
        quote! { (1 + 1).abs() },
        quote! { (lo..hi)[..] },
        quote! { (a..b)..(c..d) },
        quote! { (&mut fut).await },
        quote! { &mut (x as i32) },
        quote! { -(x as i32) },
        quote! { if (S {} == 1) {} },
        quote! { { (m! {}) - 1 } },
        quote! { match m { _ => ({}) - 1 } },
        quote! { if let _ = (a && b) && c {} },
        quote! { if let _ = (S {}) {} },
        quote! { break ('a: loop { break 'a 1 } + 1) },
        quote! { a + (|| b) + c },
        quote! { if let _ = ((break) - 1 || true) {} },
        quote! { if let _ = (break + 1 || true) {} },
        quote! { (break)() },
        quote! { (..) = () },
        quote! { (..) += () },
        quote! { (1 < 2) == (3 < 4) },
        quote! { { (let _ = ()) } },
    ] {
        let original: Expr = syn::parse2(tokens).unwrap();

        let mut flat = original.clone();
        FlattenParens.visit_expr_mut(&mut flat);
        let reconstructed: Expr = match syn::parse2(flat.to_token_stream()) {
            Ok(reconstructed) => reconstructed,
            Err(err) => panic!("failed to parse `{}`: {}", flat.to_token_stream(), err),
        };

        assert!(
            original == reconstructed,
            "original: {}\nreconstructed: {}",
            original.to_token_stream(),
            reconstructed.to_token_stream(),
        );
    }
}
