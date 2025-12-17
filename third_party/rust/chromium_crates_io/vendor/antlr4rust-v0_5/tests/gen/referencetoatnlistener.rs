#![allow(nonstandard_style)]
// Generated from ReferenceToATN.g4 by ANTLR 4.13.2
use antlr4rust::tree::ParseTreeListener;
use super::referencetoatnparser::*;

pub trait ReferenceToATNListener<'input> : ParseTreeListener<'input,ReferenceToATNParserContextType>{
/**
 * Enter a parse tree produced by {@link ReferenceToATNParser#a}.
 * @param ctx the parse tree
 */
fn enter_a(&mut self, _ctx: &AContext<'input>) { }
/**
 * Exit a parse tree produced by {@link ReferenceToATNParser#a}.
 * @param ctx the parse tree
 */
fn exit_a(&mut self, _ctx: &AContext<'input>) { }

}

antlr4rust::coerce_from!{ 'input : ReferenceToATNListener<'input> }


