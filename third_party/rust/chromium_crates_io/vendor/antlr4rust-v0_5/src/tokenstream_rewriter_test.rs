// fn test_insert_before_index0(t *testing.T) { unimplemented!() }
// 
// fn prepare_rewriter(str String) -> * TokenStreamRewriter { unimplemented!() }
// 
// pub struct LexerTest {
//     input: String,
//     expected: String,
//     description: String,
//     expected_exception: Vec<String>,
//     ops func( * TokenStreamRewriter),
// }
// 
// impl LexerTest {
//     fn new_lexer_test(input, expected: String, desc: String, ops: func(* TokenStreamRewriter)) LexerTest { unimplemented ! () }
// 
//     fn new_lexer_exception_test(input String, expected_err Vec<String>, desc: String, ops: func(* TokenStreamRewriter)) LexerTest { unimplemented ! () }
// 
//     fn panic_tester(t *testing.T, expected_msg Vec<String>, r: * TokenStreamRewriter) { unimplemented!() }
// 
//     fn test_lexer_a(t *testing.T) { unimplemented!() }
// 
// 
//     var _ = fmt.Printf
//     var _ = unicode.IsLetter
// 
//     var serializedLexerAtn = Vec < uint16 > {
//     3, 24715, 42794, 33075, 47597, 16764, 15335, 30598, 22884, 2, 5, 15, 8,
//     1, 4, 2, 9, 2, 4, 3, 9, 3, 4, 4, 9, 4, 3, 2, 3, 2, 3, 3, 3, 3, 3, 4, 3,
//     4, 2, 2, 5, 3, 3, 5, 4, 7, 5, 3, 2, 2, 2, 14, 2, 3, 3, 2, 2, 2, 2, 5, 3,
//     2, 2, 2, 2, 7, 3, 2, 2, 2, 3, 9, 3, 2, 2, 2, 5, 11, 3, 2, 2, 2, 7, 13,
//     3, 2, 2, 2, 9, 10, 7, 99, 2, 2, 10, 4, 3, 2, 2, 2, 11, 12, 7, 100, 2, 2,
//     12, 6, 3, 2, 2, 2, 13, 14, 7, 101, 2, 2, 14, 8, 3, 2, 2, 2, 3, 2, 2,
//     }
// 
//     var lexerDeserializer = NewATNDeserializer(nil)
//     var lexerAtn = lexerDeserializer.DeserializeFromUInt16(serializedLexerAtn)
// 
//     var lexerChannelNames = Vec< String > {
//     "DEFAULT_TOKEN_CHANNEL", "HIDDEN",
//     }
// 
//     var lexerModeNames = Vec < String > {
//     "DEFAULT_MODE",
//     }
// 
//     var lexerLiteralNames = Vec < String > {
//     "", "'a'", "'b'", "'c'",
//     }
// 
//     var lexerSymbolicNames = Vec < String > {
//     "", "A", "B", "C",
//     }
// 
//     var lexerRuleNames = Vec < String > {
//     "A", "B", "C",
//     }
// 
//     pub struct LexerA {
//     base: BaseLexer,
//     channel_names: Vec < String > ,
//     mode_names: Vec < String >,
//     }
// 
//     var lexerDecisionToDFA = make( & Vec < DFA >, len(lexerAtn.DecisionToState))
// 
//     fn init() { unimplemented!() }
// 
//     fn new_lexer_a(input CharStream) -> * LexerA { unimplemented!() }
// 
//     const (
//     lexer_aa = 1
//     lexer_ab = 2
//     lexer_ac = 3
//     )
// }
//  