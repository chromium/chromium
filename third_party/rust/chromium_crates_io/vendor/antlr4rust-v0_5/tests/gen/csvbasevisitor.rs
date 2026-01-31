
// Generated from CSV.g4 by ANTLR 4.13.2

use antlr4rust::tree::ParseTreeVisitor;
use super::csvparser::*;

// A complete Visitor for a parse tree produced by CSVParser.

pub trait CSVBaseVisitor<'input>:
    ParseTreeVisitor<'input, CSVParserContextType> {
	// Visit a parse tree produced by CSVParser#csvFile.
	fn visit_csvfile(&mut self, ctx: &CsvFileContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by CSVParser#hdr.
	fn visit_hdr(&mut self, ctx: &HdrContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by CSVParser#row.
	fn visit_row(&mut self, ctx: &RowContext<'input>) {
            self.visit_children(ctx)
        }

	// Visit a parse tree produced by CSVParser#field.
	fn visit_field(&mut self, ctx: &FieldContext<'input>) {
            self.visit_children(ctx)
        }

}