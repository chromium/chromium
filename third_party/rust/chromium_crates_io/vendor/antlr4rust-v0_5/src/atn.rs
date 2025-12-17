use std::collections::HashMap;

use crate::atn_state::ATNState;
use crate::atn_state::ATNStateRef;
use crate::atn_type::ATNType;
use crate::dfa::ScopeExt;
use crate::interval_set::IntervalSet;
use crate::lexer_action::LexerAction;
use crate::ll1_analyzer::LL1Analyzer;
use crate::parser::ParserNodeType;
use crate::rule_context::EmptyContextType;
use crate::token::{TOKEN_EOF, TOKEN_EPSILON};
use crate::token_factory::CommonTokenFactory;
use crate::transition::RuleTransition;
use std::fmt::{Debug, Formatter};

pub const INVALID_ALT: i32 = 0;

/// Augmented Transition Network
///
/// Basically NFA(graph) of states and possible(maybe multiple) transitions on a given particular symbol.
///
/// Public mostly because of implementations reasons. From user side is only useful for advanced error handling
pub struct ATN {
    pub decision_to_state: Vec<ATNStateRef>,

    pub grammar_type: ATNType,

    pub(crate) lexer_actions: Vec<LexerAction>,

    pub max_token_type: i32,

    pub mode_name_to_start_state: HashMap<String, ATNStateRef>,

    pub mode_to_start_state: Vec<ATNStateRef>,

    pub rule_to_start_state: Vec<ATNStateRef>,

    pub rule_to_stop_state: Vec<ATNStateRef>,

    pub rule_to_token_type: Vec<i32>,

    pub states: Vec<Box<dyn ATNState>>,
}

impl Debug for ATN {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ATN")
            .field("grammar_type", &self.grammar_type)
            .field("max_token_type", &self.max_token_type)
            .field("states count", &self.states.len())
            .field("..", &"..")
            .finish()
    }
}

impl ATN {
    pub(crate) fn new_atn(grammar_type: ATNType, max_token_type: i32) -> ATN {
        ATN {
            decision_to_state: Vec::new(),
            grammar_type,
            lexer_actions: vec![],
            max_token_type,
            mode_name_to_start_state: HashMap::new(),
            mode_to_start_state: Vec::new(),
            rule_to_start_state: Vec::new(),
            rule_to_stop_state: Vec::new(),
            rule_to_token_type: Vec::new(),
            states: Vec::new(),
        }
    }

    ///Compute the set of valid tokens that can occur starting in `s` and
    ///staying in same rule. `Token::EPSILON` is in set if we reach end of
    ///rule.
    pub fn next_tokens<'a>(&self, s: &'a dyn ATNState) -> &'a IntervalSet {
        s.get_next_tokens_within_rule().get_or_init(|| {
            self.next_tokens_in_ctx::<EmptyContextType<'_, CommonTokenFactory>>(s, None)
                .modify_with(|r| r.read_only = true)
        })
    }

    /// Compute the set of valid tokens that can occur starting in state `s`.
    /// If `ctx` is null, the set of tokens will not include what can follow
    /// the rule surrounding `s`. In other words, the set will be
    /// restricted to tokens reachable staying within `s`'s rule.
    pub fn next_tokens_in_ctx<'a, Ctx: ParserNodeType<'a>>(
        &self,
        s: &dyn ATNState,
        _ctx: Option<&Ctx::Type>,
    ) -> IntervalSet {
        let analyzer = LL1Analyzer::new(self);
        analyzer.look::<Ctx>(s, None, _ctx)
    }

    pub(crate) fn add_state(&mut self, state: Box<dyn ATNState>) {
        debug_assert_eq!(state.get_state_number() as usize, self.states.len());
        self.states.push(state)
    }

    // fn remove_state(&self, _state: ATNStateRef) { unimplemented!() }

    // fn define_decision_state(&self, _s: ATNStateRef) -> i32 { unimplemented!() }

    pub fn get_decision_state(&self, decision: i32) -> ATNStateRef {
        self.decision_to_state[decision as usize]
    }

    /// Computes the set of input symbols which could follow ATN state number
    /// {@code stateNumber} in the specified full {@code context}. This method
    /// considers the complete parser context, but does not evaluate semantic
    /// predicates (i.e. all predicates encountered during the calculation are
    /// assumed true). If a path in the ATN exists from the starting state to the
    /// {@link RuleStopState} of the outermost context without matching any
    /// symbols, {@link Token#EOF} is added to the returned set.
    ///
    /// <p>If {@code context} is {@code null}, it is treated as {@link ParserRuleContext#EMPTY}.</p>
    ///
    /// Note that this does NOT give you the set of all tokens that could
    /// appear at a given token position in the input phrase.  In other words,
    /// it does not answer:
    ///
    ///   "Given a specific partial input phrase, return the set of all tokens
    ///    that can follow the last token in the input phrase."
    ///
    /// The big difference is that with just the input, the parser could
    /// land right in the middle of a lookahead decision. Getting
    /// all *possible* tokens given a partial input stream is a separate
    /// computation. See https://github.com/antlr/antlr4/issues/1428
    ///
    /// For this function, we are specifying an ATN state and call stack to compute
    /// what token(s) can come next and specifically: outside of a lookahead decision.
    /// That is what you want for error reporting and recovery upon parse error.
    ///
    /// @param stateNumber the ATN state number
    /// @param context the full parse context
    /// @return The set of potentially valid input symbols which could follow the
    /// specified state in the specified context.
    /// Panics if the ATN does not contain a state with
    /// number {@code stateNumber}
    pub fn get_expected_tokens(
        &self,
        state_number: i32,
        states_stack: impl Iterator<Item = i32>, // _ctx: &Rc<Ctx::Type>,
    ) -> IntervalSet {
        let s = self.states[state_number as usize].as_ref();
        let mut following = self.next_tokens(s);
        if !following.contains(TOKEN_EPSILON) {
            return following.clone();
        }
        let mut expected = IntervalSet::new();
        expected.add_set(following);
        expected.remove_one(TOKEN_EPSILON);
        // let mut ctx = Some(Rc::clone(_ctx));

        for state in states_stack {
            if !following.contains(TOKEN_EPSILON) {
                break;
            }

            let invoking_state = self.states[state as usize].as_ref();
            let tr = invoking_state.get_transitions().first().unwrap().as_ref();
            let tr = tr.cast::<RuleTransition>();
            following = self.next_tokens(self.states[tr.follow_state as usize].as_ref());
            expected.add_set(following);
            expected.remove_one(TOKEN_EPSILON);
            // ctx = c.get_parent_ctx();
        }

        if following.contains(TOKEN_EPSILON) {
            expected.add_one(TOKEN_EOF);
        }
        expected
    }
}
