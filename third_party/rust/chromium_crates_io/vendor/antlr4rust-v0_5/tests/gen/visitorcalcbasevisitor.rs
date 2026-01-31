
// Generated from VisitorCalc.g4 by ANTLR 4.13.2

use antlr4rust::tree::ParseTreeVisitor;
use super::visitorcalcparser::*;

// A complete Visitor for a parse tree produced by VisitorCalcParser.

pub trait VisitorCalcBaseVisitor<'input>:
    ParseTreeVisitor<'input, VisitorCalcParserContextType> {
	// Visit a parse tree produced by VisitorCalcParser#s.
	fn visit_s(&mut self, ctx: &SContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by VisitorCalcParser#add.
	fn visit_add(&mut self, ctx: &AddContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by VisitorCalcParser#number.
	fn visit_number(&mut self, ctx: &NumberContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by VisitorCalcParser#multiply.
	fn visit_multiply(&mut self, ctx: &MultiplyContext<'input>) {
            self.visit_children(ctx)
        }

}