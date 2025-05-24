use proc_macro2::{Delimiter, Group, TokenStream, TokenTree};
use std::mem;
use syn::visit_mut::{self, VisitMut};
use syn::{Expr, File, Generics, LifetimeParam, MacroDelimiter, Stmt, StmtMacro, TypeParam};

pub struct FlattenParens;

impl FlattenParens {
    pub fn visit_token_stream_mut(tokens: &mut TokenStream) {
        *tokens = mem::take(tokens)
            .into_iter()
            .flat_map(|tt| {
                if let TokenTree::Group(group) = tt {
                    let delimiter = group.delimiter();
                    let mut content = group.stream();
                    Self::visit_token_stream_mut(&mut content);
                    if let Delimiter::Parenthesis = delimiter {
                        content
                    } else {
                        TokenStream::from(TokenTree::Group(Group::new(delimiter, content)))
                    }
                } else {
                    TokenStream::from(tt)
                }
            })
            .collect();
    }
}

impl VisitMut for FlattenParens {
    fn visit_expr_mut(&mut self, e: &mut Expr) {
        while let Expr::Paren(paren) = e {
            *e = mem::replace(&mut *paren.expr, Expr::PLACEHOLDER);
        }
        visit_mut::visit_expr_mut(self, e);
    }
}

pub struct AsIfPrinted;

impl VisitMut for AsIfPrinted {
    fn visit_file_mut(&mut self, file: &mut File) {
        file.shebang = None;
        visit_mut::visit_file_mut(self, file);
    }

    fn visit_generics_mut(&mut self, generics: &mut Generics) {
        if generics.params.is_empty() {
            generics.lt_token = None;
            generics.gt_token = None;
        }
        if let Some(where_clause) = &generics.where_clause {
            if where_clause.predicates.is_empty() {
                generics.where_clause = None;
            }
        }
        visit_mut::visit_generics_mut(self, generics);
    }

    fn visit_lifetime_param_mut(&mut self, param: &mut LifetimeParam) {
        if param.bounds.is_empty() {
            param.colon_token = None;
        }
        visit_mut::visit_lifetime_param_mut(self, param);
    }

    fn visit_stmt_mut(&mut self, stmt: &mut Stmt) {
        if let Stmt::Expr(expr, semi) = stmt {
            if let Expr::Macro(e) = expr {
                if match e.mac.delimiter {
                    MacroDelimiter::Brace(_) => true,
                    MacroDelimiter::Paren(_) | MacroDelimiter::Bracket(_) => semi.is_some(),
                } {
                    let Expr::Macro(expr) = mem::replace(expr, Expr::PLACEHOLDER) else {
                        unreachable!();
                    };
                    *stmt = Stmt::Macro(StmtMacro {
                        attrs: expr.attrs,
                        mac: expr.mac,
                        semi_token: *semi,
                    });
                }
            }
        }
        visit_mut::visit_stmt_mut(self, stmt);
    }

    fn visit_type_param_mut(&mut self, param: &mut TypeParam) {
        if param.bounds.is_empty() {
            param.colon_token = None;
        }
        visit_mut::visit_type_param_mut(self, param);
    }
}
