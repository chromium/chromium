#![allow(nonstandard_style)]
// Generated from CSV.g4 by ANTLR 4.13.2
use antlr4rust::tree::{ParseTreeVisitor,ParseTreeVisitorCompat};
use super::csvparser::*;

/**
 * This interface defines a complete generic visitor for a parse tree produced
 * by {@link CSVParser}.
 */
pub trait CSVVisitor<'input>: ParseTreeVisitor<'input,CSVParserContextType>{
	/**
	 * Visit a parse tree produced by {@link CSVParser#csvFile}.
	 * @param ctx the parse tree
	 */
	fn visit_csvFile(&mut self, ctx: &CsvFileContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by {@link CSVParser#hdr}.
	 * @param ctx the parse tree
	 */
	fn visit_hdr(&mut self, ctx: &HdrContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by {@link CSVParser#row}.
	 * @param ctx the parse tree
	 */
	fn visit_row(&mut self, ctx: &RowContext<'input>) { self.visit_children(ctx) }

	/**
	 * Visit a parse tree produced by {@link CSVParser#field}.
	 * @param ctx the parse tree
	 */
	fn visit_field(&mut self, ctx: &FieldContext<'input>) { self.visit_children(ctx) }

}

pub trait CSVVisitorCompat<'input>:ParseTreeVisitorCompat<'input, Node= CSVParserContextType>{
	/**
	 * Visit a parse tree produced by {@link CSVParser#csvFile}.
	 * @param ctx the parse tree
	 */
		fn visit_csvFile(&mut self, ctx: &CsvFileContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by {@link CSVParser#hdr}.
	 * @param ctx the parse tree
	 */
		fn visit_hdr(&mut self, ctx: &HdrContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by {@link CSVParser#row}.
	 * @param ctx the parse tree
	 */
		fn visit_row(&mut self, ctx: &RowContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

	/**
	 * Visit a parse tree produced by {@link CSVParser#field}.
	 * @param ctx the parse tree
	 */
		fn visit_field(&mut self, ctx: &FieldContext<'input>) -> Self::Return {
			self.visit_children(ctx)
		}

}

impl<'input,T> CSVVisitor<'input> for T
where
	T: CSVVisitorCompat<'input>
{
	fn visit_csvFile(&mut self, ctx: &CsvFileContext<'input>){
		let result = <Self as CSVVisitorCompat>::visit_csvFile(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_hdr(&mut self, ctx: &HdrContext<'input>){
		let result = <Self as CSVVisitorCompat>::visit_hdr(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_row(&mut self, ctx: &RowContext<'input>){
		let result = <Self as CSVVisitorCompat>::visit_row(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

	fn visit_field(&mut self, ctx: &FieldContext<'input>){
		let result = <Self as CSVVisitorCompat>::visit_field(self, ctx);
        *<Self as ParseTreeVisitorCompat>::temp_result(self) = result;
	}

}