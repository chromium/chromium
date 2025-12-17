mod gen {
    mod csvlexer;
    mod csvlistener;
    mod csvparser;
    mod csvvisitor;
    mod visitorbasiclexer;
    mod visitorbasiclistener;
    mod visitorbasicparser;
    mod visitorbasicvisitor;
    mod visitorcalclexer;
    mod visitorcalclistener;
    mod visitorcalcparser;
    mod visitorcalcvisitor;

    use crate::gen::csvparser::CSVParserContextType;
    use crate::gen::visitorbasiclexer::VisitorBasicLexer;
    use crate::gen::visitorbasicparser::{VisitorBasicParser, VisitorBasicParserContextType};
    use crate::gen::visitorbasicvisitor::VisitorBasicVisitorCompat;
    use crate::gen::visitorcalclexer::VisitorCalcLexer;
    use antlr4rust::common_token_stream::CommonTokenStream;
    use antlr4rust::parser::ParserNodeType;
    use antlr4rust::token::Token;
    use antlr4rust::tree::{ErrorNode, ParseTree, ParseTreeVisitorCompat, TerminalNode, Visitable};
    use antlr4rust::InputStream;
    use visitorcalcparser::{
        AddContext, AddContextAttrs, MultiplyContext, MultiplyContextAttrs, NumberContext,
        NumberContextAttrs, SContext, SContextAttrs, VisitorCalcParser,
        VisitorCalcParserContextType,
    };
    use visitorcalcvisitor::VisitorCalcVisitorCompat;

    #[test]
    fn test_visit_terminal_node() {
        let lexer = VisitorBasicLexer::new(InputStream::new("A"));
        let mut parser = VisitorBasicParser::new(CommonTokenStream::new(lexer));

        let root = parser.s().unwrap();
        assert_eq!("(s A <EOF>)", root.to_string_tree(&*parser));

        struct TestVisitor(String);
        impl ParseTreeVisitorCompat<'_> for TestVisitor {
            type Node = VisitorBasicParserContextType;
            type Return = String;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn visit_terminal(&mut self, _node: &TerminalNode<'_, Self::Node>) -> Self::Return {
                _node.symbol.to_string() + "\n"
            }

            fn aggregate_results(
                &self,
                aggregate: Self::Return,
                next: Self::Return,
            ) -> Self::Return {
                aggregate + &next
            }
        }
        impl VisitorBasicVisitorCompat<'_> for TestVisitor {}

        let result = TestVisitor(String::new()).visit(&*root);
        let expected = "[@0,0:0='A',<1>,1:0]\n\
                              [@1,1:0='<EOF>',<-1>,1:1]\n";
        assert_eq!(result, expected)
    }

    #[test]
    fn test_visit_error_node() {
        let lexer = VisitorBasicLexer::new(InputStream::new(""));
        let mut parser = VisitorBasicParser::new(CommonTokenStream::new(lexer));

        let root = parser.s().unwrap();
        assert_eq!("(s <missing 'A'> <EOF>)", root.to_string_tree(&*parser));

        struct TestVisitor(String);
        impl ParseTreeVisitorCompat<'_> for TestVisitor {
            type Node = VisitorBasicParserContextType;
            type Return = String;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn visit_error_node(&mut self, _node: &ErrorNode<'_, Self::Node>) -> Self::Return {
                format!("Error encountered: {}", _node.symbol)
            }

            fn aggregate_results(
                &self,
                aggregate: Self::Return,
                next: Self::Return,
            ) -> Self::Return {
                aggregate + &next
            }
        }
        impl VisitorBasicVisitorCompat<'_> for TestVisitor {}

        let result = TestVisitor(String::new()).visit(&*root);
        let expected = "Error encountered: [@-1,-1:-1='<missing 'A'>',<1>,1:0]";
        assert_eq!(result, expected)
    }

    #[test]
    fn test_should_not_visit_EOF() {
        let lexer = VisitorBasicLexer::new(InputStream::new("A"));
        let mut parser = VisitorBasicParser::new(CommonTokenStream::new(lexer));

        let root = parser.s().unwrap();
        assert_eq!("(s A <EOF>)", root.to_string_tree(&*parser));

        struct TestVisitor(String);
        impl ParseTreeVisitorCompat<'_> for TestVisitor {
            type Node = VisitorBasicParserContextType;
            type Return = String;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn visit_terminal(&mut self, _node: &TerminalNode<'_, Self::Node>) -> Self::Return {
                _node.symbol.to_string() + "\n"
            }

            fn should_visit_next_child(
                &self,
                node: &<Self::Node as ParserNodeType<'_>>::Type,
                current: &Self::Return,
            ) -> bool {
                current.is_empty()
            }
        }
        impl VisitorBasicVisitorCompat<'_> for TestVisitor {}

        let result = TestVisitor(String::new()).visit(&*root);
        let expected = "[@0,0:0='A',<1>,1:0]\n";
        assert_eq!(result, expected);

        struct TestVisitorUnit(String);
        impl ParseTreeVisitorCompat<'_> for TestVisitorUnit {
            type Node = VisitorBasicParserContextType;
            type Return = ();

            fn temp_result(&mut self) -> &mut Self::Return {
                Box::leak(Box::new(()))
            }

            fn visit_terminal(&mut self, _node: &TerminalNode<'_, Self::Node>) -> Self::Return {
                self.0 += &_node.symbol.to_string();
            }
        }
        impl VisitorBasicVisitorCompat<'_> for TestVisitorUnit {}
    }

    #[test]
    fn test_should_not_visit_anything() {
        let lexer = VisitorBasicLexer::new(InputStream::new("A"));
        let mut parser = VisitorBasicParser::new(CommonTokenStream::new(lexer));

        let root = parser.s().unwrap();
        assert_eq!("(s A <EOF>)", root.to_string_tree(&*parser));

        struct TestVisitor(String);
        impl ParseTreeVisitorCompat<'_> for TestVisitor {
            type Node = VisitorBasicParserContextType;
            type Return = String;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn visit_terminal(&mut self, _node: &TerminalNode<'_, Self::Node>) -> Self::Return {
                unreachable!()
            }

            fn should_visit_next_child(
                &self,
                node: &<Self::Node as ParserNodeType<'_>>::Type,
                current: &Self::Return,
            ) -> bool {
                false
            }
        }
        impl VisitorBasicVisitorCompat<'_> for TestVisitor {}

        let result = TestVisitor(String::new()).visit(&*root);
        let expected = "";
        assert_eq!(result, expected)
    }

    #[test]
    fn test_visitor_with_return() {
        struct CalcVisitor(i32);

        impl ParseTreeVisitorCompat<'_> for CalcVisitor {
            type Node = VisitorCalcParserContextType;
            type Return = i32;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn aggregate_results(
                &self,
                aggregate: Self::Return,
                next: Self::Return,
            ) -> Self::Return {
                panic!("Should not be reachable")
            }
        }

        impl VisitorCalcVisitorCompat<'_> for CalcVisitor {
            fn visit_s(&mut self, ctx: &SContext<'_>) -> Self::Return {
                self.visit(&*ctx.expr().unwrap())
            }

            fn visit_add(&mut self, ctx: &AddContext<'_>) -> Self::Return {
                let left = self.visit(&*ctx.expr(0).unwrap());
                let right = self.visit(&*ctx.expr(1).unwrap());
                if ctx.ADD().is_some() {
                    left + right
                } else {
                    left - right
                }
            }

            fn visit_number(&mut self, ctx: &NumberContext<'_>) -> Self::Return {
                ctx.INT().unwrap().get_text().parse().unwrap()
            }

            fn visit_multiply(&mut self, ctx: &MultiplyContext<'_>) -> Self::Return {
                let left = self.visit(&*ctx.expr(0).unwrap());
                let right = self.visit(&*ctx.expr(1).unwrap());
                if ctx.MUL().is_some() {
                    left * right
                } else {
                    left / right
                }
            }
        }

        let mut _lexer = VisitorCalcLexer::new(InputStream::new("2 + 8 / 2"));
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = VisitorCalcParser::new(token_source);

        let root = parser.s().unwrap();

        assert_eq!(
            "(s (expr (expr 2) + (expr (expr 8) / (expr 2))) <EOF>)",
            root.to_string_tree(&*parser)
        );

        let mut visitor = CalcVisitor(0);

        let visitor_result = visitor.visit(&*root);
        assert_eq!(6, visitor_result)
    }

    // tests zero-copy parsing with non static visitor
    #[test]
    fn test_visitor_retrieve_reference() {
        use antlr4rust::token_factory::ArenaCommonFactory;
        use antlr4rust::tree::ParseTreeVisitor;
        use csvlexer::CSVLexer;
        use csvparser::{CSVParser, CsvFileContext, HdrContext, RowContext, RowContextAttrs};
        use csvvisitor::CSVVisitor;
        use std::borrow::Cow;
        use std::rc::Rc;

        // `T` here to ensure that visitor can have lifetime shorter that `'input` string
        struct MyCSVVisitor<'i, T>(Vec<&'i str>, T);

        impl<'i, T> ParseTreeVisitor<'i, CSVParserContextType> for MyCSVVisitor<'i, T> {
            fn visit_terminal(&mut self, node: &TerminalNode<'i, CSVParserContextType>) {
                if node.symbol.get_token_type() == csvparser::CSV_TEXT {
                    if let Cow::Borrowed(s) = node.symbol.text {
                        self.0.push(s);
                    }
                }
            }
        }

        impl<'i, T> CSVVisitor<'i> for MyCSVVisitor<'i, T> {
            fn visit_hdr(&mut self, _ctx: &HdrContext<'i>) {}

            fn visit_row(&mut self, ctx: &RowContext<'i>) {
                if ctx.field_all().len() > 1 {
                    self.visit_children(ctx)
                }
            }
        }

        fn parse<'a>(tf: &'a ArenaCommonFactory<'a>) -> Rc<CsvFileContext<'a>> {
            let mut _lexer =
                CSVLexer::new_with_token_factory(InputStream::new("h1,h2\nd1,d2\nd3\n"), tf);
            let token_source = CommonTokenStream::new(_lexer);
            let mut parser = CSVParser::new(token_source);
            let result = parser.csvFile().expect("parsed unsuccessfully");

            let mut test = 5;
            let mut visitor = MyCSVVisitor(Vec::new(), &mut test);
            result.accept(&mut visitor);
            assert_eq!(visitor.0, vec!["d1", "d2"]);

            result
        }
        let tf = ArenaCommonFactory::default();

        let _result = parse(&tf);
    }

    #[test]
    fn test_visitor_retrieve_reference_by_return() {
        use antlr4rust::token_factory::ArenaCommonFactory;
        use csvlexer::CSVLexer;
        use csvparser::{CSVParser, CsvFileContext, HdrContext, RowContext, RowContextAttrs};
        use csvvisitor::CSVVisitorCompat;
        use std::borrow::Cow;
        use std::rc::Rc;

        struct MyCSVVisitor<'i>(Vec<&'i str>);

        impl<'i> ParseTreeVisitorCompat<'i> for MyCSVVisitor<'i> {
            type Node = CSVParserContextType;
            type Return = Vec<&'i str>;

            fn temp_result(&mut self) -> &mut Self::Return {
                &mut self.0
            }

            fn visit_terminal(
                &mut self,
                node: &TerminalNode<'i, CSVParserContextType>,
            ) -> Self::Return {
                if node.symbol.get_token_type() == csvparser::CSV_TEXT {
                    if let Cow::Borrowed(s) = node.symbol.text {
                        return vec![s];
                    }
                }
                vec![]
            }

            fn aggregate_results(
                &self,
                mut aggregate: Self::Return,
                next: Self::Return,
            ) -> Self::Return {
                aggregate.extend(next);
                aggregate
            }
        }

        impl<'i> CSVVisitorCompat<'i> for MyCSVVisitor<'i> {
            fn visit_hdr(&mut self, _ctx: &HdrContext<'i>) -> Self::Return {
                vec![]
            }

            fn visit_row(&mut self, ctx: &RowContext<'i>) -> Self::Return {
                if ctx.field_all().len() > 1 {
                    self.visit_children(ctx)
                } else {
                    vec![]
                }
            }
        }

        fn parse<'a>(tf: &'a ArenaCommonFactory<'a>) -> Rc<CsvFileContext<'a>> {
            let mut _lexer =
                CSVLexer::new_with_token_factory(InputStream::new("h1,h2\nd1,d2\nd3\n"), tf);
            let token_source = CommonTokenStream::new(_lexer);
            let mut parser = CSVParser::new(token_source);
            let result = parser.csvFile().expect("parsed unsuccessfully");

            let mut visitor = MyCSVVisitor(Vec::new());
            let visitor_result = visitor.visit(&*result);
            assert_eq!(visitor_result, vec!["d1", "d2"]);

            result
        }
        let tf = ArenaCommonFactory::default();

        let _result = parse(&tf);
    }
}
