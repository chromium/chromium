#![allow(nonstandard_style)]
// Generated from VisitorBasic.g4 by ANTLR 4.13.2
use antlr4rust::tree::ParseTreeListener;
use super::visitorbasicparser::*;

pub trait VisitorBasicListener<'input> : ParseTreeListener<'input,VisitorBasicParserContextType>{
/**
 * Enter a parse tree produced by {@link VisitorBasicParser#s}.
 * @param ctx the parse tree
 */
fn enter_s(&mut self, _ctx: &SContext<'input>) { }
/**
 * Exit a parse tree produced by {@link VisitorBasicParser#s}.
 * @param ctx the parse tree
 */
fn exit_s(&mut self, _ctx: &SContext<'input>) { }

}

antlr4rust::coerce_from!{ 'input : VisitorBasicListener<'input> }


