// Generated from CSV.g4 by ANTLR 4.13.2

use super::csvparser::*;
use antlr4rust::tree::ParseTreeListener;

// A complete Visitor for a parse tree produced by CSVParser.

pub trait CSVBaseListener<'input>:
    ParseTreeListener<'input, CSVParserContextType> {

    /**
     * Enter a parse tree produced by \{@link CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_csvfile(&mut self, _ctx: &CsvFileContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_csvfile(&mut self, _ctx: &CsvFileContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_hdr(&mut self, _ctx: &HdrContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_hdr(&mut self, _ctx: &HdrContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_row(&mut self, _ctx: &RowContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_row(&mut self, _ctx: &RowContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_field(&mut self, _ctx: &FieldContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  CSVBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_field(&mut self, _ctx: &FieldContext<'input>) {}


}