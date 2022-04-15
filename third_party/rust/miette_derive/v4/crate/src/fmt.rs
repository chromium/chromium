// NOTE: Most code in this file is taken straight from `thiserror`.
use std::collections::HashSet as Set;
use std::iter::FromIterator;

use proc_macro2::{Delimiter, Group, TokenStream, TokenTree};
use quote::{format_ident, quote, quote_spanned, ToTokens};
use syn::ext::IdentExt;
use syn::parse::{ParseStream, Parser};
use syn::{braced, bracketed, parenthesized, Ident, Index, LitStr, Member, Result, Token};

#[derive(Clone)]
pub struct Display {
    pub fmt: LitStr,
    pub args: TokenStream,
    pub has_bonus_display: bool,
}

impl ToTokens for Display {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let fmt = &self.fmt;
        let args = &self.args;
        tokens.extend(quote! {
            write!(__formatter, #fmt #args)
        });
    }
}

impl Display {
    // Transform `"error {var}"` to `"error {}", var`.
    pub fn expand_shorthand(&mut self, members: &Set<Member>) {
        let raw_args = self.args.clone();
        let mut named_args = explicit_named_args.parse2(raw_args).unwrap();

        let span = self.fmt.span();
        let fmt = self.fmt.value();
        let mut read = fmt.as_str();
        let mut out = String::new();
        let mut args = self.args.clone();
        let mut has_bonus_display = false;

        let mut has_trailing_comma = false;
        if let Some(TokenTree::Punct(punct)) = args.clone().into_iter().last() {
            if punct.as_char() == ',' {
                has_trailing_comma = true;
            }
        }

        while let Some(brace) = read.find('{') {
            out += &read[..brace + 1];
            read = &read[brace + 1..];
            if read.starts_with('{') {
                out.push('{');
                read = &read[1..];
                continue;
            }
            let next = match read.chars().next() {
                Some(next) => next,
                None => return,
            };
            let member = match next {
                '0'..='9' => {
                    let int = take_int(&mut read);
                    let member = match int.parse::<u32>() {
                        Ok(index) => Member::Unnamed(Index { index, span }),
                        Err(_) => return,
                    };
                    if !members.contains(&member) {
                        out += &int;
                        continue;
                    }
                    member
                }
                'a'..='z' | 'A'..='Z' | '_' => {
                    let mut ident = take_ident(&mut read);
                    ident.set_span(span);
                    Member::Named(ident)
                }
                _ => continue,
            };
            let local = match &member {
                Member::Unnamed(index) => format_ident!("_{}", index),
                Member::Named(ident) => ident.clone(),
            };
            let mut formatvar = local.clone();
            if formatvar.to_string().starts_with("r#") {
                formatvar = format_ident!("r_{}", formatvar);
            }
            if formatvar.to_string().starts_with('_') {
                // Work around leading underscore being rejected by 1.40 and
                // older compilers. https://github.com/rust-lang/rust/pull/66847
                formatvar = format_ident!("field_{}", formatvar);
            }
            out += &formatvar.to_string();
            if !named_args.insert(formatvar.clone()) {
                // Already specified in the format argument list.
                continue;
            }
            if !has_trailing_comma {
                args.extend(quote_spanned!(span=> ,));
            }
            args.extend(quote_spanned!(span=> #formatvar = #local));
            if read.starts_with('}') && members.contains(&member) {
                has_bonus_display = true;
                // args.extend(quote_spanned!(span=> .as_display()));
            }
            has_trailing_comma = false;
        }

        out += read;
        self.fmt = LitStr::new(&out, self.fmt.span());
        self.args = args;
        self.has_bonus_display = has_bonus_display;
    }
}

fn explicit_named_args(input: ParseStream) -> Result<Set<Ident>> {
    let mut named_args = Set::new();

    while !input.is_empty() {
        if input.peek(Token![,]) && input.peek2(Ident::peek_any) && input.peek3(Token![=]) {
            input.parse::<Token![,]>()?;
            let ident = input.call(Ident::parse_any)?;
            input.parse::<Token![=]>()?;
            named_args.insert(ident);
        } else {
            input.parse::<TokenTree>()?;
        }
    }

    Ok(named_args)
}

fn take_int(read: &mut &str) -> String {
    let mut int = String::new();
    for (i, ch) in read.char_indices() {
        match ch {
            '0'..='9' => int.push(ch),
            _ => {
                *read = &read[i..];
                break;
            }
        }
    }
    int
}

fn take_ident(read: &mut &str) -> Ident {
    let mut ident = String::new();
    let raw = read.starts_with("r#");
    if raw {
        ident.push_str("r#");
        *read = &read[2..];
    }
    for (i, ch) in read.char_indices() {
        match ch {
            'a'..='z' | 'A'..='Z' | '0'..='9' | '_' => ident.push(ch),
            _ => {
                *read = &read[i..];
                break;
            }
        }
    }
    Ident::parse_any.parse_str(&ident).unwrap()
}

pub fn parse_token_expr(input: ParseStream, mut begin_expr: bool) -> Result<TokenStream> {
    let mut tokens = Vec::new();
    while !input.is_empty() {
        if begin_expr && input.peek(Token![.]) {
            if input.peek2(Ident) {
                input.parse::<Token![.]>()?;
                begin_expr = false;
                continue;
            }
            if input.peek2(syn::LitInt) {
                input.parse::<Token![.]>()?;
                let int: Index = input.parse()?;
                let ident = format_ident!("_{}", int.index, span = int.span);
                tokens.push(TokenTree::Ident(ident));
                begin_expr = false;
                continue;
            }
        }

        begin_expr = input.peek(Token![break])
            || input.peek(Token![continue])
            || input.peek(Token![if])
            || input.peek(Token![in])
            || input.peek(Token![match])
            || input.peek(Token![mut])
            || input.peek(Token![return])
            || input.peek(Token![while])
            || input.peek(Token![+])
            || input.peek(Token![&])
            || input.peek(Token![!])
            || input.peek(Token![^])
            || input.peek(Token![,])
            || input.peek(Token![/])
            || input.peek(Token![=])
            || input.peek(Token![>])
            || input.peek(Token![<])
            || input.peek(Token![|])
            || input.peek(Token![%])
            || input.peek(Token![;])
            || input.peek(Token![*])
            || input.peek(Token![-]);

        let token: TokenTree = if input.peek(syn::token::Paren) {
            let content;
            let delimiter = parenthesized!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Parenthesis, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else if input.peek(syn::token::Brace) {
            let content;
            let delimiter = braced!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Brace, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else if input.peek(syn::token::Bracket) {
            let content;
            let delimiter = bracketed!(content in input);
            let nested = parse_token_expr(&content, true)?;
            let mut group = Group::new(Delimiter::Bracket, nested);
            group.set_span(delimiter.span);
            TokenTree::Group(group)
        } else {
            input.parse()?
        };
        tokens.push(token);
    }
    Ok(TokenStream::from_iter(tokens))
}
