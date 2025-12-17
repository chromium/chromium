// Generated from VisitorBasic.g4 by ANTLR 4.13.2

use super::visitorbasicparser::*;
use antlr4rust::tree::ParseTreeListener;

// A complete Visitor for a parse tree produced by VisitorBasicParser.

pub trait VisitorBasicBaseListener<'input>:
    ParseTreeListener<'input, VisitorBasicParserContextType> {

    /**
     * Enter a parse tree produced by \{@link VisitorBasicBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_s(&mut self, _ctx: &SContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  VisitorBasicBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_s(&mut self, _ctx: &SContext<'input>) {}


}