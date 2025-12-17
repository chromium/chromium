#![allow(nonstandard_style)]
// Generated from SimpleLR.g4 by ANTLR 4.13.2
use antlr4rust::tree::ParseTreeListener;
use super::simplelrparser::*;

pub trait SimpleLRListener<'input> : ParseTreeListener<'input,SimpleLRParserContextType>{
/**
 * Enter a parse tree produced by {@link SimpleLRParser#s}.
 * @param ctx the parse tree
 */
fn enter_s(&mut self, _ctx: &SContext<'input>) { }
/**
 * Exit a parse tree produced by {@link SimpleLRParser#s}.
 * @param ctx the parse tree
 */
fn exit_s(&mut self, _ctx: &SContext<'input>) { }
/**
 * Enter a parse tree produced by {@link SimpleLRParser#a}.
 * @param ctx the parse tree
 */
fn enter_a(&mut self, _ctx: &AContext<'input>) { }
/**
 * Exit a parse tree produced by {@link SimpleLRParser#a}.
 * @param ctx the parse tree
 */
fn exit_a(&mut self, _ctx: &AContext<'input>) { }

}

antlr4rust::coerce_from!{ 'input : SimpleLRListener<'input> }


