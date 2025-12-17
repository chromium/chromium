#![allow(nonstandard_style)]
// Generated from VisitorCalc.g4 by ANTLR 4.13.2
use antlr4rust::tree::{ParseTreeVisitor,ParseTreeVisitorCompat};
use super::visitorcalcparser::*;

/**
 * This interface defines a complete generic visitor for a parse tree produced
 * by {@link VisitorCalcParser}.
 */
pub trait VisitorCalcVisitor<'input>: ParseTreeVisitor<'input,VisitorCalcParserContextType>{
	/**
	 * Visit a parse tree produced by {@link VisitorCalcParser#s}.
	 * @param ctx the parse tree
	 */
	fn visit_s(&mut self, ctx: &SContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by the {@code add}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
	fn visit_add(&mut self, ctx: &AddContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by the {@code number}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
	fn visit_number(&mut self, ctx: &NumberContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by the {@code multiply}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
	fn visit_multiply(&mut self, ctx: &MultiplyContext<'input>) { self.visit_children(ctx) }

}

pub trait VisitorCalcVisitorCompat<'input>:ParseTreeVisitorCompat<'input, Node= VisitorCalcParserContextType>{
	/**
	 * Visit a parse tree produced by {@link VisitorCalcParser#s}.
	 * @param ctx the parse tree
	 */
		fn visit_s(&mut self, ctx: &SContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by the {@code add}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
		fn visit_add(&mut self, ctx: &AddContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by the {@code number}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
		fn visit_number(&mut self, ctx: &NumberContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by the {@code multiply}
	 * labeled alternative in {@link VisitorCalcParser#expr}.
	 * @param ctx the parse tree
	 */
		fn visit_multiply(&mut self, ctx: &MultiplyContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

}

impl<'input,T> VisitorCalcVisitor<'input> for T
where
	T: VisitorCalcVisitorCompat<'input>
{
	fn visit_s(&mut self, ctx: &SContext<'input>){
		let result = <Self as VisitorCalcVisitorCompat>::visit_s(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_add(&mut self, ctx: &AddContext<'input>){
		let result = <Self as VisitorCalcVisitorCompat>::visit_add(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_number(&mut self, ctx: &NumberContext<'input>){
		let result = <Self as VisitorCalcVisitorCompat>::visit_number(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_multiply(&mut self, ctx: &MultiplyContext<'input>){
		let result = <Self as VisitorCalcVisitorCompat>::visit_multiply(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

}