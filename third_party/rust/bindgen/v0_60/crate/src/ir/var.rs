//! Intermediate representation of variables.

use super::super::codegen::MacroTypeVariation;
use super::context::{BindgenContext, TypeId};
use super::dot::DotAttributes;
use super::function::cursor_mangling;
use super::int::IntKind;
use super::item::Item;
use super::ty::{FloatKind, TypeKind};
use crate::callbacks::MacroParsingBehavior;
use crate::clang;
use crate::clang::ClangToken;
use crate::parse::{
    ClangItemParser, ClangSubItemParser, ParseError, ParseResult,
};
use cexpr;
use std::io;
use std::num::Wrapping;

/// The type for a constant variable.
#[derive(Debug)]
pub enum VarType {
    /// A boolean.
    Bool(bool),
    /// An integer.
    Int(i64),
    /// A floating point number.
    Float(f64),
    /// A character.
    Char(u8),
    /// A string, not necessarily well-formed utf-8.
    String(Vec<u8>),
}

/// A `Var` is our intermediate representation of a variable.
#[derive(Debug)]
pub struct Var {
    /// The name of the variable.
    name: String,
    /// The mangled name of the variable.
    mangled_name: Option<String>,
    /// The type of the variable.
    ty: TypeId,
    /// The value of the variable, that needs to be suitable for `ty`.
    val: Option<VarType>,
    /// Whether this variable is const.
    is_const: bool,
}

impl Var {
    /// Construct a new `Var`.
    pub fn new(
        name: String,
        mangled_name: Option<String>,
        ty: TypeId,
        val: Option<VarType>,
        is_const: bool,
    ) -> Var {
        assert!(!name.is_empty());
        Var {
            name,
            mangled_name,
            ty,
            val,
            is_const,
        }
    }

    /// Is this variable `const` qualified?
    pub fn is_const(&self) -> bool {
        self.is_const
    }

    /// The value of this constant variable, if any.
    pub fn val(&self) -> Option<&VarType> {
        self.val.as_ref()
    }

    /// Get this variable's type.
    pub fn ty(&self) -> TypeId {
        self.ty
    }

    /// Get this variable's name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get this variable's mangled name.
    pub fn mangled_name(&self) -> Option<&str> {
        self.mangled_name.as_deref()
    }
}

impl DotAttributes for Var {
    fn dot_attributes<W>(
        &self,
        _ctx: &BindgenContext,
        out: &mut W,
    ) -> io::Result<()>
    where
        W: io::Write,
    {
        if self.is_const {
            writeln!(out, "<tr><td>const</td><td>true</td></tr>")?;
        }

        if let Some(ref mangled) = self.mangled_name {
            writeln!(
                out,
                "<tr><td>mangled name</td><td>{}</td></tr>",
                mangled
            )?;
        }

        Ok(())
    }
}

fn default_macro_constant_type(ctx: &BindgenContext, value: i64) -> IntKind {
    if value < 0 ||
        ctx.options().default_macro_constant_type ==
            MacroTypeVariation::Signed
    {
        if value < i32::min_value() as i64 || value > i32::max_value() as i64 {
            IntKind::I64
        } else if !ctx.options().fit_macro_constants ||
            value < i16::min_value() as i64 ||
            value > i16::max_value() as i64
        {
            IntKind::I32
        } else if value < i8::min_value() as i64 ||
            value > i8::max_value() as i64
        {
            IntKind::I16
        } else {
            IntKind::I8
        }
    } else if value > u32::max_value() as i64 {
        IntKind::U64
    } else if !ctx.options().fit_macro_constants ||
        value > u16::max_value() as i64
    {
        IntKind::U32
    } else if value > u8::max_value() as i64 {
        IntKind::U16
    } else {
        IntKind::U8
    }
}

/// Parses tokens from a CXCursor_MacroDefinition pointing into a function-like
/// macro, and calls the func_macro callback.
fn handle_function_macro(
    cursor: &clang::Cursor,
    callbacks: &dyn crate::callbacks::ParseCallbacks,
) {
    let is_closing_paren = |t: &ClangToken| {
        // Test cheap token kind before comparing exact spellings.
        t.kind == clang_sys::CXToken_Punctuation && t.spelling() == b")"
    };
    let tokens: Vec<_> = cursor.tokens().iter().collect();
    if let Some(boundary) = tokens.iter().position(is_closing_paren) {
        let mut spelled = tokens.iter().map(ClangToken::spelling);
        // Add 1, to convert index to length.
        let left = spelled.by_ref().take(boundary + 1);
        let left = left.collect::<Vec<_>>().concat();
        if let Ok(left) = String::from_utf8(left) {
            let right: Vec<_> = spelled.collect();
            callbacks.func_macro(&left, &right);
        }
    }
}

impl ClangSubItemParser for Var {
    fn parse(
        cursor: clang::Cursor,
        ctx: &mut BindgenContext,
    ) -> Result<ParseResult<Self>, ParseError> {
        use cexpr::expr::EvalResult;
        use cexpr::literal::CChar;
        use clang_sys::*;
        match cursor.kind() {
            CXCursor_MacroDefinition => {
                if let Some(callbacks) = ctx.parse_callbacks() {
                    match callbacks.will_parse_macro(&cursor.spelling()) {
                        MacroParsingBehavior::Ignore => {
                            return Err(ParseError::Continue);
                        }
                        MacroParsingBehavior::Default => {}
                    }

                    if cursor.is_macro_function_like() {
                        handle_function_macro(&cursor, callbacks);
                        // We handled the macro, skip macro processing below.
                        return Err(ParseError::Continue);
                    }
                }

                let value = parse_macro(ctx, &cursor);

                let (id, value) = match value {
                    Some(v) => v,
                    None => return Err(ParseError::Continue),
                };

                assert!(!id.is_empty(), "Empty macro name?");

                let previously_defined = ctx.parsed_macro(&id);

                // NB: It's important to "note" the macro even if the result is
                // not an integer, otherwise we might loose other kind of
                // derived macros.
                ctx.note_parsed_macro(id.clone(), value.clone());

                if previously_defined {
                    let name = String::from_utf8(id).unwrap();
                    warn!("Duplicated macro definition: {}", name);
                    return Err(ParseError::Continue);
                }

                // NOTE: Unwrapping, here and above, is safe, because the
                // identifier of a token comes straight from clang, and we
                // enforce utf8 there, so we should have already panicked at
                // this point.
                let name = String::from_utf8(id).unwrap();
                let (type_kind, val) = match value {
                    EvalResult::Invalid => return Err(ParseError::Continue),
                    EvalResult::Float(f) => {
                        (TypeKind::Float(FloatKind::Double), VarType::Float(f))
                    }
                    EvalResult::Char(c) => {
                        let c = match c {
                            CChar::Char(c) => {
                                assert_eq!(c.len_utf8(), 1);
                                c as u8
                            }
                            CChar::Raw(c) => {
                                assert!(c <= ::std::u8::MAX as u64);
                                c as u8
                            }
                        };

                        (TypeKind::Int(IntKind::U8), VarType::Char(c))
                    }
                    EvalResult::Str(val) => {
                        let char_ty = Item::builtin_type(
                            TypeKind::Int(IntKind::U8),
                            true,
                            ctx,
                        );
                        if let Some(callbacks) = ctx.parse_callbacks() {
                            callbacks.str_macro(&name, &val);
                        }
                        (TypeKind::Pointer(char_ty), VarType::String(val))
                    }
                    EvalResult::Int(Wrapping(value)) => {
                        let kind = ctx
                            .parse_callbacks()
                            .and_then(|c| c.int_macro(&name, value))
                            .unwrap_or_else(|| {
                                default_macro_constant_type(ctx, value)
                            });

                        (TypeKind::Int(kind), VarType::Int(value))
                    }
                };

                let ty = Item::builtin_type(type_kind, true, ctx);

                Ok(ParseResult::New(
                    Var::new(name, None, ty, Some(val), true),
                    Some(cursor),
                ))
            }
            CXCursor_VarDecl => {
                let name = cursor.spelling();
                if name.is_empty() {
                    warn!("Empty constant name?");
                    return Err(ParseError::Continue);
                }

                let ty = cursor.cur_type();

                // TODO(emilio): do we have to special-case constant arrays in
                // some other places?
                let is_const = ty.is_const() ||
                    (ty.kind() == CXType_ConstantArray &&
                        ty.elem_type()
                            .map_or(false, |element| element.is_const()));

                let ty = match Item::from_ty(&ty, cursor, None, ctx) {
                    Ok(ty) => ty,
                    Err(e) => {
                        assert_eq!(
                            ty.kind(),
                            CXType_Auto,
                            "Couldn't resolve constant type, and it \
                             wasn't an nondeductible auto type!"
                        );
                        return Err(e);
                    }
                };

                // Note: Ty might not be totally resolved yet, see
                // tests/headers/inner_const.hpp
                //
                // That's fine because in that case we know it's not a literal.
                let canonical_ty = ctx
                    .safe_resolve_type(ty)
                    .and_then(|t| t.safe_canonical_type(ctx));

                let is_integer = canonical_ty.map_or(false, |t| t.is_integer());
                let is_float = canonical_ty.map_or(false, |t| t.is_float());

                // TODO: We could handle `char` more gracefully.
                // TODO: Strings, though the lookup is a bit more hard (we need
                // to look at the canonical type of the pointee too, and check
                // is char, u8, or i8 I guess).
                let value = if is_integer {
                    let kind = match *canonical_ty.unwrap().kind() {
                        TypeKind::Int(kind) => kind,
                        _ => unreachable!(),
                    };

                    let mut val = cursor.evaluate().and_then(|v| v.as_int());
                    if val.is_none() || !kind.signedness_matches(val.unwrap()) {
                        let tu = ctx.translation_unit();
                        val = get_integer_literal_from_cursor(&cursor, tu);
                    }

                    val.map(|val| {
                        if kind == IntKind::Bool {
                            VarType::Bool(val != 0)
                        } else {
                            VarType::Int(val)
                        }
                    })
                } else if is_float {
                    cursor
                        .evaluate()
                        .and_then(|v| v.as_double())
                        .map(VarType::Float)
                } else {
                    cursor
                        .evaluate()
                        .and_then(|v| v.as_literal_string())
                        .map(VarType::String)
                };

                let mangling = cursor_mangling(ctx, &cursor);
                let var = Var::new(name, mangling, ty, value, is_const);

                Ok(ParseResult::New(var, Some(cursor)))
            }
            _ => {
                /* TODO */
                Err(ParseError::Continue)
            }
        }
    }
}

/// Try and parse a macro using all the macros parsed until now.
fn parse_macro(
    ctx: &BindgenContext,
    cursor: &clang::Cursor,
) -> Option<(Vec<u8>, cexpr::expr::EvalResult)> {
    use cexpr::expr;

    let cexpr_tokens = cursor.cexpr_tokens();

    let parser = expr::IdentifierParser::new(ctx.parsed_macros());

    match parser.macro_definition(&cexpr_tokens) {
        Ok((_, (id, val))) => Some((id.into(), val)),
        _ => None,
    }
}

fn parse_int_literal_tokens(cursor: &clang::Cursor) -> Option<i64> {
    use cexpr::expr;
    use cexpr::expr::EvalResult;

    let cexpr_tokens = cursor.cexpr_tokens();

    // TODO(emilio): We can try to parse other kinds of literals.
    match expr::expr(&cexpr_tokens) {
        Ok((_, EvalResult::Int(Wrapping(val)))) => Some(val),
        _ => None,
    }
}

fn get_integer_literal_from_cursor(
    cursor: &clang::Cursor,
    unit: &clang::TranslationUnit,
) -> Option<i64> {
    use clang_sys::*;
    let mut value = None;
    cursor.visit(|c| {
        match c.kind() {
            CXCursor_IntegerLiteral | CXCursor_UnaryOperator => {
                value = parse_int_literal_tokens(&c);
            }
            CXCursor_UnexposedExpr => {
                value = get_integer_literal_from_cursor(&c, unit);
            }
            _ => (),
        }
        if value.is_some() {
            CXChildVisit_Break
        } else {
            CXChildVisit_Continue
        }
    });
    value
}
