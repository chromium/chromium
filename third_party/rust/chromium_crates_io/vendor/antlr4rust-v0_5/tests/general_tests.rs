// #![feature(try_blocks)]
// #![feature(inner_deref)]
// #![feature(specialization)]
// #![feature(coerce_unsized)]
//! Integration tests

// #[macro_use]
// extern crate lazy_static;

mod gen {
    use std::fmt::Write;
    use std::io::Read;
    use std::iter::FromIterator;

    use antlr4rust::common_token_stream::CommonTokenStream;
    use antlr4rust::errors::ANTLRError;
    use antlr4rust::int_stream::IntStream;
    use antlr4rust::lexer::Lexer;

    use antlr4rust::token::{Token, TOKEN_EOF};
    use antlr4rust::token_factory::{ArenaCommonFactory, OwningTokenFactory};
    use antlr4rust::token_stream::{TokenStream, UnbufferedTokenStream};
    use antlr4rust::tree::{ParseTree, ParseTreeListener, TerminalNode};
    use antlr4rust::InputStream;
    use csvlexer::*;
    use csvlistener::*;
    use csvparser::CSVParser;
    use referencetoatnlexer::ReferenceToATNLexer;
    use referencetoatnlistener::ReferenceToATNListener;
    use referencetoatnparser::ReferenceToATNParser;
    use xmllexer::XMLLexer;

    use crate::gen::csvparser::{CSVParserContext, CSVParserContextType};

    use crate::gen::labelslexer::LabelsLexer;
    use crate::gen::labelsparser::{EContextAll, LabelsParser};
    use crate::gen::referencetoatnparser::{
        ReferenceToATNParserContext, ReferenceToATNParserContextType,
    };
    use crate::gen::simplelrlexer::SimpleLRLexer;
    use crate::gen::simplelrlistener::SimpleLRListener;
    use crate::gen::simplelrparser::{
        SimpleLRParser, SimpleLRParserContext, SimpleLRParserContextType, SimpleLRTreeWalker,
    };

    mod csvlexer;
    mod csvlistener;
    mod csvparser;
    mod csvvisitor;
    mod referencetoatnlexer;
    mod referencetoatnlistener;
    mod referencetoatnparser;
    mod simplelrlexer;
    mod simplelrlistener;
    mod simplelrparser;
    mod visitorcalclexer;
    mod visitorcalclistener;
    mod visitorcalcparser;
    mod visitorcalcvisitor;
    mod xmllexer;

    fn test_static<T: 'static>(_arg: T) {}

    #[test]
    fn lexer_test_xml() -> std::io::Result<()> {
        let data = r#"<?xml version="1.0"?>
<!--comment-->>
<?xml-stylesheet type="text/css" href="nutrition.css"?>
<script>
<![CDATA[
function f(x) {
if (x < x && a > 0) then duh
}
]]>
</script>"#
            .to_owned();
        let mut _lexer = XMLLexer::new(InputStream::new(&*data));
        //        _lexer.base.add_error_listener();
        let _a = "a".to_owned() + "";
        let mut string = String::new();
        {
            let mut token_source = UnbufferedTokenStream::new_unbuffered(&mut _lexer);
            while token_source.la(1) != TOKEN_EOF {
                {
                    let token = token_source.lt(1).unwrap();

                    let len = token.get_stop() as usize + 1 - token.get_start() as usize;
                    string.extend(
                        format!(
                            "{},len {}:\n{}\n",
                            xmllexer::_SYMBOLIC_NAMES[token.get_token_type() as usize]
                                .unwrap_or(&format!("{}", token.get_token_type())),
                            len,
                            String::from_iter(
                                data.chars().skip(token.get_start() as usize).take(len)
                            )
                        )
                        .chars(),
                    );
                }
                token_source.consume();
            }
        }
        println!("{}", string);
        println!(
            "{}",
            _lexer
                .get_interpreter()
                .unwrap()
                .get_dfa()
                .read()
                .to_lexer_string()
        );
        Ok(())
    }

    #[test]
    fn lexer_test_csv() {
        println!("test started lexer_test_csv");
        let tf = ArenaCommonFactory::default();
        let mut _lexer = CSVLexer::new_with_token_factory(
            InputStream::new("V123,V2\nd1,d222"),
            // Box::new(UTF16InputStream::from_str("V123,V2\nd1,d222","".into())),
            &tf,
        );
        let mut token_source = UnbufferedTokenStream::new_buffered(_lexer);
        let mut token_source_iter = token_source.token_iter();
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@0,0:3='V123',<5>,1:0]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@1,4:4=',',<1>,1:4]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@2,5:6='V2',<5>,1:5]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@3,7:7='\\n',<3>,1:7]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@4,8:9='d1',<5>,2:0]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@5,10:10=',',<1>,2:2]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@6,11:14='d222',<5>,2:3]"
        );
        assert_eq!(
            token_source_iter.next().unwrap().to_string(),
            "[@7,15:14='<EOF>',<-1>,2:7]"
        );
        assert!(token_source_iter.next().is_none());
    }

    struct Listener {}

    impl<'input> ParseTreeListener<'input, CSVParserContextType> for Listener {
        fn enter_every_rule(&mut self, ctx: &dyn CSVParserContext<'input>) -> Result<(), ANTLRError> {
            println!(
                "rule entered {}",
                csvparser::ruleNames
                    .get(ctx.get_rule_index())
                    .unwrap_or(&"error")
            );
            Ok(())
        }
    }

    impl CSVListener<'_> for Listener {}

    #[test]
    fn parser_test_csv() {
        println!("test started");
        let tf = ArenaCommonFactory::default();
        let mut _lexer =
            CSVLexer::new_with_token_factory(InputStream::new("V123,V2\nd1,d2\n"), &tf);
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = CSVParser::new(token_source);
        parser.add_parse_listener(Box::new(Listener {}));
        println!("\nstart parsing parser_test_csv");
        let result = parser.csvFile();
        assert!(result.is_ok());
        assert_eq!(
            result.unwrap().to_string_tree(&*parser),
            "(csvFile (hdr (row (field V123) , (field V2) \\n)) (row (field d1) , (field d2) \\n))"
        );
    }

    struct Listener2 {}

    impl<'input> ParseTreeListener<'input, ReferenceToATNParserContextType> for Listener2 {
        fn enter_every_rule(&mut self, ctx: &dyn ReferenceToATNParserContext<'input>) -> Result<(), ANTLRError> {
            println!(
                "rule entered {}",
                referencetoatnparser::ruleNames
                    .get(ctx.get_rule_index())
                    .unwrap_or(&"error")
            );
            Ok(())
        }
    }

    impl ReferenceToATNListener<'_> for Listener2 {}

    static FACTORY: OwningTokenFactory = OwningTokenFactory;

    #[test]
    fn test_adaptive_predict_and_owned_tree() {
        let text = "a 34 b".to_owned();
        let mut _lexer = ReferenceToATNLexer::new_with_token_factory(
            InputStream::new_owned(text.into_boxed_str()),
            &FACTORY,
        );
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = ReferenceToATNParser::new(token_source);
        parser.add_parse_listener(Box::new(Listener2 {}));
        println!("\nstart parsing adaptive_predict_test");
        let result = parser.a();
        assert!(result.is_ok());
        test_static(result);
    }

    struct Listener3;

    impl<'input> ParseTreeListener<'input, SimpleLRParserContextType> for Listener3 {
        fn visit_terminal(&mut self, node: &TerminalNode<'input, SimpleLRParserContextType>) {
            println!("terminal node {}", node.symbol.get_text());
        }

        fn enter_every_rule(&mut self, ctx: &dyn SimpleLRParserContext<'input>) -> Result<(), ANTLRError> {
            println!(
                "rule entered {}",
                simplelrparser::ruleNames
                    .get(ctx.get_rule_index())
                    .unwrap_or(&"error")
            );
            Ok(())
        }

        fn exit_every_rule(&mut self, ctx: &dyn SimpleLRParserContext<'input>) -> Result<(), ANTLRError> {
            println!(
                "rule exited {}",
                simplelrparser::ruleNames
                    .get(ctx.get_rule_index())
                    .unwrap_or(&"error")
            );
            Ok(())
        }
    }

    impl SimpleLRListener<'_> for Listener3 {}

    #[test]
    fn test_lr() {
        let mut _lexer = SimpleLRLexer::new(InputStream::new("x y z"));
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = SimpleLRParser::new(token_source);
        parser.add_parse_listener(Box::new(Listener3));
        println!("\nstart parsing lr_test");
        let result = parser.s().expect("failed recursion parsion");
        assert_eq!(result.to_string_tree(&*parser), "(s (a (a (a x) y) z))");
    }

    #[test]
    fn test_immediate_lr() {
        let mut _lexer = SimpleLRLexer::new(InputStream::new("x y z"));
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = SimpleLRParser::new(token_source);
        parser.add_parse_listener(Box::new(Listener3));
        println!("\nstart parsing lr_test");
        let result = parser.a().expect("failed immediate recursion parsing");
        assert_eq!(result.to_string_tree(&*parser), "(a (a (a x) y) z)");
    }

    struct Listener4 {
        data: String,
    }

    impl<'input> ParseTreeListener<'input, SimpleLRParserContextType> for Listener4 {
        fn visit_terminal(&mut self, node: &TerminalNode<'input, SimpleLRParserContextType>) {
            println!("enter terminal");
            let _ = writeln!(&mut self.data, "terminal node {}", node.symbol.get_text());
        }
        fn enter_every_rule(&mut self, ctx: &dyn SimpleLRParserContext<'input>) -> Result<(), ANTLRError> {
            println!(
                "rule entered {}",
                simplelrparser::ruleNames
                    .get(ctx.get_rule_index())
                    .unwrap_or(&"error")
            );
            Ok(())
        }
    }

    impl SimpleLRListener<'_> for Listener4 {}

    #[test]
    fn test_remove_listener() {
        let mut _lexer = SimpleLRLexer::new(InputStream::new("x y z"));
        let token_source = CommonTokenStream::new(_lexer);
        let mut parser = SimpleLRParser::new(token_source);
        parser.add_parse_listener(Box::new(Listener3));
        let id = parser.add_parse_listener(Box::new(Listener4 {
            data: String::new(),
        }));
        let result = parser.s().expect("expected to parse successfully");

        let mut listener = parser.remove_parse_listener(id);
        assert_eq!(
            &listener.data,
            "terminal node x\nterminal node y\nterminal node z\n"
        );

        println!("--------");
        listener.data.clear();

        let listener = SimpleLRTreeWalker::walk(listener, &*result);
        assert_eq!(
            &listener.unwrap().data,
            "terminal node x\nterminal node y\nterminal node z\n"
        );
    }

    #[test]
    fn test_byte_parser() {}

    mod labelslexer;
    mod labelslistener;
    mod labelsparser;

    #[test]
    fn test_complex_convert() {
        let codepoints = "(a+4)*2".chars().map(|x| x as u32).collect::<Vec<_>>();
        // let codepoints = "(a+4)*2";
        let input = InputStream::new(&*codepoints);
        let lexer = LabelsLexer::new(input);
        let token_source = CommonTokenStream::new(lexer);
        let mut parser = LabelsParser::new(token_source);
        let result = parser.s().expect("parser error");
        let string = result.q.as_ref().unwrap().get_v();
        assert_eq!("* + a 4 2", string);
        let x = result.q.as_deref().unwrap();
        match x {
            EContextAll::MultContext(x) => assert_eq!("(a+4)", x.a.as_ref().unwrap().get_text()),
            _ => panic!("oops"),
        }
    }
}
