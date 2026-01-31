#![allow(nonstandard_style)]
// Generated from CSV.g4 by ANTLR 4.13.2
use antlr4rust::tree::ParseTreeListener;
use super::csvparser::*;

pub trait CSVListener<'input> : ParseTreeListener<'input,CSVParserContextType>{
/**
 * Enter a parse tree produced by {@link CSVParser#csvFile}.
 * @param ctx the parse tree
 */
fn enter_csvFile(&mut self, _ctx: &CsvFileContext<'input>) { }
/**
 * Exit a parse tree produced by {@link CSVParser#csvFile}.
 * @param ctx the parse tree
 */
fn exit_csvFile(&mut self, _ctx: &CsvFileContext<'input>) { }
/**
 * Enter a parse tree produced by {@link CSVParser#hdr}.
 * @param ctx the parse tree
 */
fn enter_hdr(&mut self, _ctx: &HdrContext<'input>) { }
/**
 * Exit a parse tree produced by {@link CSVParser#hdr}.
 * @param ctx the parse tree
 */
fn exit_hdr(&mut self, _ctx: &HdrContext<'input>) { }
/**
 * Enter a parse tree produced by {@link CSVParser#row}.
 * @param ctx the parse tree
 */
fn enter_row(&mut self, _ctx: &RowContext<'input>) { }
/**
 * Exit a parse tree produced by {@link CSVParser#row}.
 * @param ctx the parse tree
 */
fn exit_row(&mut self, _ctx: &RowContext<'input>) { }
/**
 * Enter a parse tree produced by {@link CSVParser#field}.
 * @param ctx the parse tree
 */
fn enter_field(&mut self, _ctx: &FieldContext<'input>) { }
/**
 * Exit a parse tree produced by {@link CSVParser#field}.
 * @param ctx the parse tree
 */
fn exit_field(&mut self, _ctx: &FieldContext<'input>) { }

}

antlr4rust::coerce_from!{ 'input : CSVListener<'input> }


