//! Error reporting
use std::cell::Ref;
use std::ops::Deref;

use bit_set::BitSet;

use crate::atn_config_set::ATNConfigSet;
use crate::dfa::DFA;
use crate::errors::ANTLRError;

use crate::parser::Parser;
use crate::recognizer::Recognizer;

use crate::token_factory::TokenFactory;
use std::borrow::Cow;
use std::fmt::Debug;

/// Describes interface for listening on parser/lexer errors.
/// Should only listen for errors, for processing/recovering from errors use `ErrorStrategy`
pub trait ErrorListener<'a, T: Recognizer<'a>> {
    /// Called when parser/lexer encounter hard error.
    ///
    /// The `_error` is not None for all syntax errors except
    /// when we discover mismatched token errors that we can recover from
    /// in-line, without returning from the surrounding rule (via the single
    /// token insertion and deletion mechanism)
    fn syntax_error(
        &self,
        _recognizer: &T,
        _offending_symbol: Option<&<T::TF as TokenFactory<'a>>::Inner>,
        _line: isize,
        _column: isize,
        _msg: &str,
        _error: Option<&ANTLRError>,
    ) {
    }

    /// This method is called by the parser when a full-context prediction
    /// results in an ambiguity.
    fn report_ambiguity(
        &self,
        _recognizer: &T,
        _dfa: &DFA,
        _start_index: isize,
        _stop_index: isize,
        _exact: bool,
        _ambig_alts: &BitSet,
        _configs: &ATNConfigSet,
    ) {
    }

    /// This method is called when an SLL conflict occurs and the parser is about
    /// to use the full context information to make an LL decision.
    fn report_attempting_full_context(
        &self,
        _recognizer: &T,
        _dfa: &DFA,
        _start_index: isize,
        _stop_index: isize,
        _conflicting_alts: &BitSet,
        _configs: &ATNConfigSet,
    ) {
    }

    /// This method is called by the parser when a full-context prediction has a
    /// unique result.
    fn report_context_sensitivity(
        &self,
        _recognizer: &T,
        _dfa: &DFA,
        _start_index: isize,
        _stop_index: isize,
        _prediction: i32,
        _configs: &ATNConfigSet,
    ) {
    }
}

/// Default error listener that outputs errors to stderr
#[derive(Debug)]
pub struct ConsoleErrorListener {}

impl<'a, T: Recognizer<'a>> ErrorListener<'a, T> for ConsoleErrorListener {
    fn syntax_error(
        &self,
        _recognizer: &T,
        _offending_symbol: Option<&<T::TF as TokenFactory<'a>>::Inner>,
        line: isize,
        column: isize,
        msg: &str,
        _e: Option<&ANTLRError>,
    ) {
        eprintln!("line {}:{} {}", line, column, msg);
    }
}

// #[derive(Debug)]
pub(crate) struct ProxyErrorListener<'b, 'a, T> {
    pub delegates: Ref<'b, Vec<Box<dyn ErrorListener<'a, T>>>>,
}

impl<'a, T: Recognizer<'a>> ErrorListener<'a, T> for ProxyErrorListener<'_, 'a, T> {
    fn syntax_error(
        &self,
        _recognizer: &T,
        offending_symbol: Option<&<T::TF as TokenFactory<'a>>::Inner>,
        line: isize,
        column: isize,
        msg: &str,
        e: Option<&ANTLRError>,
    ) {
        for listener in self.delegates.deref() {
            listener.syntax_error(_recognizer, offending_symbol, line, column, msg, e)
        }
    }

    fn report_ambiguity(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        exact: bool,
        ambig_alts: &BitSet<u32>,
        configs: &ATNConfigSet,
    ) {
        for listener in self.delegates.deref() {
            listener.report_ambiguity(
                recognizer,
                dfa,
                start_index,
                stop_index,
                exact,
                ambig_alts,
                configs,
            )
        }
    }

    fn report_attempting_full_context(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        conflicting_alts: &BitSet<u32>,
        configs: &ATNConfigSet,
    ) {
        for listener in self.delegates.deref() {
            listener.report_attempting_full_context(
                recognizer,
                dfa,
                start_index,
                stop_index,
                conflicting_alts,
                configs,
            )
        }
    }

    fn report_context_sensitivity(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        prediction: i32,
        configs: &ATNConfigSet,
    ) {
        for listener in self.delegates.deref() {
            listener.report_context_sensitivity(
                recognizer,
                dfa,
                start_index,
                stop_index,
                prediction,
                configs,
            )
        }
    }
}

/// This implementation of `ErrorListener` can be used to identify
/// certain potential correctness and performance problems in grammars. "Reports"
/// are made by calling `Parser::notify_error_listeners` with the appropriate
/// message.
///
///  - Ambiguities: These are cases where more than one path through the
/// grammar can match the input.
///  - Weak context sensitivity</b>: These are cases where full-context
/// prediction resolved an SLL conflict to a unique alternative which equaled the
/// minimum alternative of the SLL conflict.
///  - Strong (forced) context sensitivity: These are cases where the
/// full-context prediction resolved an SLL conflict to a unique alternative,
/// *and* the minimum alternative of the SLL conflict was found to not be
/// a truly viable alternative. Two-stage parsing cannot be used for inputs where
/// this situation occurs.
#[derive(Debug)]
pub struct DiagnosticErrorListener {
    exact_only: bool,
}

impl DiagnosticErrorListener {
    /// When `exact_only` is true, only exactly known ambiguities are reported.
    pub fn new(exact_only: bool) -> Self {
        Self { exact_only }
    }

    fn get_decision_description<'a, T: Parser<'a>>(&self, recog: &T, dfa: &DFA) -> String {
        let decision = dfa.decision;
        let rule_index = recog.get_atn().states[dfa.atn_start_state as usize].get_rule_index();

        let rule_names = recog.get_rule_names();
        if let Some(&rule_name) = rule_names.get(rule_index as usize) {
            format!("{} ({})", decision, rule_name)
        } else {
            decision.to_string()
        }
    }
    /// Computes the set of conflicting or ambiguous alternatives from a
    /// configuration set, if that information was not already provided by the
    /// parser in `alts`.
    pub fn get_conflicting_alts<'a>(
        &self,
        alts: Option<&'a BitSet>,
        _configs: &ATNConfigSet,
    ) -> Cow<'a, BitSet> {
        match alts {
            Some(alts) => Cow::Borrowed(alts),
            None => Cow::Owned(
                _configs
                    .configs
                    .iter()
                    .map(|config| config.get_alt() as usize)
                    .collect::<BitSet>(),
            ),
        }
    }
}

impl<'a, T: Parser<'a>> ErrorListener<'a, T> for DiagnosticErrorListener {
    fn report_ambiguity(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        exact: bool,
        ambig_alts: &BitSet<u32>,
        _configs: &ATNConfigSet,
    ) {
        if self.exact_only && !exact {
            return;
        }
        let msg = format!(
            "reportAmbiguity d={}: ambigAlts={:?}, input='{}'",
            self.get_decision_description(recognizer, dfa),
            ambig_alts,
            recognizer
                .get_input_stream()
                .get_text_from_interval(start_index, stop_index)
        );
        recognizer.notify_error_listeners(msg, None, None);
    }

    fn report_attempting_full_context(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        _conflicting_alts: &BitSet<u32>,
        _configs: &ATNConfigSet,
    ) {
        let msg = format!(
            "reportAttemptingFullContext d={}, input='{}'",
            self.get_decision_description(recognizer, dfa),
            recognizer
                .get_input_stream()
                .get_text_from_interval(start_index, stop_index)
        );
        recognizer.notify_error_listeners(msg, None, None);
    }

    fn report_context_sensitivity(
        &self,
        recognizer: &T,
        dfa: &DFA,
        start_index: isize,
        stop_index: isize,
        _prediction: i32,
        _configs: &ATNConfigSet,
    ) {
        let msg = format!(
            "reportContextSensitivity d={}, input='{}'",
            self.get_decision_description(recognizer, dfa),
            recognizer
                .get_input_stream()
                .get_text_from_interval(start_index, stop_index)
        );
        recognizer.notify_error_listeners(msg, None, None);
    }
}
/*
impl DefaultErrorListener {
    fn new_default_error_listener() -> * DefaultErrorListener { unimplemented!() }

    fn syntax_error(&self, recognizer: Recognizer, offendingSymbol: interface {
    }, line: isize, column: isize, msg: String, e: RecognitionError) { unimplemented!() }

    fn report_ambiguity(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, exact: bool, ambigAlts: * BitSet, configs: ATNConfigSet) { unimplemented!() }

    fn report_attempting_full_context(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, conflictingAlts: * BitSet, configs: ATNConfigSet) { unimplemented!() }

    fn report_context_sensitivity(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, prediction: isize, configs: ATNConfigSet) { unimplemented!() }

    pub struct ConsoleErrorListener {
    base: DefaultErrorListener,
    }

    fn new_console_error_listener() -> * ConsoleErrorListener { unimplemented!() }

    var ConsoleErrorListenerINSTANCE = NewConsoleErrorListener()

    fn syntax_error(&self, recognizer: Recognizer, offendingSymbol: interface {
    }, line: isize, column: isize, msg: String, e: RecognitionError) {
        fmt.Fprintln(os.Stderr, "line " + strconv.Itoa(line) + ":" + strconv.Itoa(column) + " " + msg)
    }

    pub struct ProxyErrorListener {
    base: DefaultErrorListener,
    delegates: Vec < ErrorListener > ,
    }

    fn new_proxy_error_listener(delegates Vec<ErrorListener>) -> * ProxyErrorListener { unimplemented!() }

    fn syntax_error(&self, recognizer: Recognizer, offendingSymbol: interface {
    }, line: isize, column: isize, msg: String, e: RecognitionError) {
        for _, d: = range p.delegates {
            d.SyntaxError(recognizer, offendingSymbol, line, column, msg, e)
        }
    }

    fn report_ambiguity(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, exact: bool, ambigAlts: * BitSet, configs: ATNConfigSet) { unimplemented!() }

    fn report_attempting_full_context(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, conflictingAlts: * BitSet, configs: ATNConfigSet) { unimplemented!() }

    fn report_context_sensitivity(&self, recognizer: Parser, dfa: * DFA, startIndex: isize, stopIndex: isize, prediction: isize, configs: ATNConfigSet) { unimplemented!() }
}
 */
