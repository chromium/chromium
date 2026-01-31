#![allow(nonstandard_style)]
// Generated from VisitorBasic.g4 by ANTLR 4.13.2
use antlr4rust::tree::{ParseTreeVisitor,ParseTreeVisitorCompat};
use super::visitorbasicparser::*;

/**
 * This interface defines a complete generic visitor for a parse tree produced
 * by {@link VisitorBasicParser}.
 */
pub trait VisitorBasicVisitor<'input>: ParseTreeVisitor<'input,VisitorBasicParserContextType>{
	/**
	 * Visit a parse tree produced by {@link VisitorBasicParser#s}.
	 * @param ctx the parse tree
	 */
	fn visit_s(&mut self, ctx: &SContext<'input>) { self.visit_children(ctx) }

}

pub trait VisitorBasicVisitorCompat<'input>:ParseTreeVisitorCompat<'input, Node= VisitorBasicParserContextType>{
	/**
	 * Visit a parse tree produced by {@link VisitorBasicParser#s}.
	 * @param ctx the parse tree
	 */
		fn visit_s(&mut self, ctx: &SContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

}

impl<'input,T> VisitorBasicVisitor<'input> for T
where
	T: VisitorBasicVisitorCompat<'input>
{
	fn visit_s(&mut self, ctx: &SContext<'input>){
		let result = <Self as VisitorBasicVisitorCompat>::visit_s(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

}