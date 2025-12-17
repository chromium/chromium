#![allow(nonstandard_style)]
// Generated from Labels.g4 by ANTLR 4.13.2
use antlr4rust::tree::ParseTreeListener;
use super::labelsparser::*;

pub trait LabelsListener<'input> : ParseTreeListener<'input,LabelsParserContextType>{
/**
 * Enter a parse tree produced by {@link LabelsParser#s}.
 * @param ctx the parse tree
 */
fn enter_s(&mut self, _ctx: &SContext<'input>) { }
/**
 * Exit a parse tree produced by {@link LabelsParser#s}.
 * @param ctx the parse tree
 */
fn exit_s(&mut self, _ctx: &SContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code add}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_add(&mut self, _ctx: &AddContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code add}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_add(&mut self, _ctx: &AddContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code parens}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_parens(&mut self, _ctx: &ParensContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code parens}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_parens(&mut self, _ctx: &ParensContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code mult}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_mult(&mut self, _ctx: &MultContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code mult}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_mult(&mut self, _ctx: &MultContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code dec}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_dec(&mut self, _ctx: &DecContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code dec}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_dec(&mut self, _ctx: &DecContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code anID}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_anID(&mut self, _ctx: &AnIDContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code anID}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_anID(&mut self, _ctx: &AnIDContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code anInt}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_anInt(&mut self, _ctx: &AnIntContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code anInt}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_anInt(&mut self, _ctx: &AnIntContext<'input>) { }
/**
 * Enter a parse tree produced by the {@code inc}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn enter_inc(&mut self, _ctx: &IncContext<'input>) { }
/**
 * Exit a parse tree produced by the {@code inc}
 * labeled alternative in {@link LabelsParser#e}.
 * @param ctx the parse tree
 */
fn exit_inc(&mut self, _ctx: &IncContext<'input>) { }

}

antlr4rust::coerce_from!{ 'input : LabelsListener<'input> }


