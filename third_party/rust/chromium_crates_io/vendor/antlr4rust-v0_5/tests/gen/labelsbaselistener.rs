// Generated from Labels.g4 by ANTLR 4.13.2

use super::labelsparser::*;
use antlr4rust::tree::ParseTreeListener;

// A complete Visitor for a parse tree produced by LabelsParser.

pub trait LabelsBaseListener<'input>:
    ParseTreeListener<'input, LabelsParserContextType> {

    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_s(&mut self, _ctx: &SContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_s(&mut self, _ctx: &SContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_add(&mut self, _ctx: &AddContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_add(&mut self, _ctx: &AddContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_parens(&mut self, _ctx: &ParensContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_parens(&mut self, _ctx: &ParensContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_mult(&mut self, _ctx: &MultContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_mult(&mut self, _ctx: &MultContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_dec(&mut self, _ctx: &DecContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_dec(&mut self, _ctx: &DecContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_anid(&mut self, _ctx: &AnIDContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_anid(&mut self, _ctx: &AnIDContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_anint(&mut self, _ctx: &AnIntContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_anint(&mut self, _ctx: &AnIntContext<'input>) {}


    /**
     * Enter a parse tree produced by \{@link LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn enter_inc(&mut self, _ctx: &IncContext<'input>) {}
    /**
     * Exit a parse tree produced by \{@link  LabelsBaseParser#s}.
     * @param ctx the parse tree
     */
    fn exit_inc(&mut self, _ctx: &IncContext<'input>) {}


}