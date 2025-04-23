use anyhow::Result;
use derivre::{RegexAst, RegexBuilder, StateID};
use toktrie::{TokEnv, TokTrie, TokenId};

use crate::{
    api::ParserLimits,
    earley::{
        lexerspec::LexemeIdx,
        regexvec::{LexemeSet, RegexVec, RxLexeme},
    },
};

struct StopRegex {
    dfa: RegexVec,
    state: StateID,
    initial_state: StateID,
}

pub struct StopController {
    tok_env: TokEnv,
    is_stopped: bool,
    stop_tokens: Vec<TokenId>,
    regex: Option<StopRegex>,
    pending_bytes: Vec<u8>,
}

impl StopController {
    pub fn new(
        tok_env: TokEnv,
        stop_tokens: Vec<TokenId>,
        stop_regex: Option<String>,
        stop_strings: Vec<String>,
    ) -> Result<Self> {
        let mut res = Self {
            tok_env,
            is_stopped: false,
            stop_tokens,
            regex: None,
            pending_bytes: Vec::new(),
        };

        let mut rx_ast = if let Some(rx) = stop_regex {
            RegexAst::Regex(rx)
        } else {
            RegexAst::NoMatch
        };
        if !stop_strings.is_empty() {
            let mut alts = stop_strings
                .iter()
                .map(|s| RegexAst::Regex(s.clone()))
                .collect::<Vec<_>>();
            alts.push(rx_ast);
            rx_ast = RegexAst::Or(alts);
        }

        if !matches!(rx_ast, RegexAst::NoMatch) {
            let fin = RegexAst::LookAhead(Box::new(rx_ast));
            let pref = RegexAst::Regex("(?s:.*)".to_string());
            let rx = RegexAst::Concat(vec![pref, fin]);
            let mut builder = RegexBuilder::new();
            let rx = builder.mk(&rx)?;
            let mut all_regex = LexemeSet::new(1);
            all_regex.add(LexemeIdx::new(0));
            let mut dfa = RegexVec::new_with_exprset(
                builder.into_exprset(),
                vec![RxLexeme {
                    rx,
                    lazy: true,
                    priority: 0,
                }],
                None,
                &mut ParserLimits::default(),
            )?;
            let initial_state = dfa.initial_state(&all_regex);
            res.regex = Some(StopRegex {
                dfa,
                state: initial_state,
                initial_state,
            });
        }
        Ok(res)
    }

    pub fn is_stopped(&self) -> bool {
        self.is_stopped
    }

    fn commit_token_u8(&mut self, tok_id: TokenId) -> Vec<u8> {
        let mut buf = std::mem::take(&mut self.pending_bytes);

        if self.stop_tokens.contains(&tok_id) {
            self.is_stopped = true;
        } else {
            let bytes = self.tok_env.tok_trie().token(tok_id);
            if !bytes.is_empty() && bytes[0] == TokTrie::SPECIAL_TOKEN_MARKER {
                if let Some(rx) = self.regex.as_mut() {
                    rx.state = rx.initial_state;
                }
                buf.extend_from_slice(&bytes[1..]);
            } else if bytes.is_empty() {
                if let Some(rx) = self.regex.as_mut() {
                    rx.state = rx.initial_state;
                }
                buf.extend_from_slice(format!("<[{}]>", tok_id).as_bytes());
            } else if let Some(rx) = self.regex.as_mut() {
                let mut state = rx.state;
                for &b in bytes {
                    buf.push(b);
                    let state2 = rx.dfa.transition(state, b);
                    // println!("state: {:?} -{:?}-> {:?}", state, b as char, state2);
                    state = state2;
                    assert!(!state.is_dead());
                    if state.has_lowest_match() {
                        self.is_stopped = true;
                        rx.state = state;
                        let stop_len = rx.dfa.lookahead_len_for_state(state).unwrap_or(0);
                        buf.truncate(buf.len().saturating_sub(stop_len));
                        return buf;
                    }
                }

                rx.state = state;
                let chop = rx.dfa.possible_lookahead_len(state);
                let to_return = buf.len().saturating_sub(chop);
                // println!("chop: {:?} {}", String::from_utf8_lossy(&buf), chop);
                let valid_len = valid_utf8_len(&buf[..to_return]);
                self.pending_bytes = (buf[valid_len..]).to_vec();
                buf.truncate(valid_len);
            } else {
                buf.extend_from_slice(bytes);
                let valid_len = valid_utf8_len(&buf);
                self.pending_bytes = (buf[valid_len..]).to_vec();
                buf.truncate(valid_len);
            }
        }

        buf
    }

    pub fn commit_token(&mut self, tok_id: TokenId) -> String {
        if self.is_stopped {
            return String::new();
        }

        let bytes = self.commit_token_u8(tok_id);
        match String::from_utf8(bytes) {
            Ok(s) => s,
            Err(s) => String::from_utf8_lossy(s.as_bytes()).to_string(),
        }
    }
}

fn valid_utf8_len(data: &[u8]) -> usize {
    if data.is_empty() {
        return 0;
    }

    // Find where the last valid UTF-8 sequence starts by scanning the final bytes
    let mut i = data.len() - 1;

    // Check if we have a continuation byte (0b10xxxxxx)
    while i > 0 && (data[i] & 0b1100_0000 == 0b1000_0000) {
        i -= 1;
    }

    // Check how many bytes the starting byte indicates for the UTF-8 sequence
    let first_byte = data[i];
    let expected_len = if first_byte & 0b1000_0000 == 0 {
        1 // Single-byte character (ASCII)
    } else if first_byte & 0b1110_0000 == 0b1100_0000 {
        2 // Two-byte character
    } else if first_byte & 0b1111_0000 == 0b1110_0000 {
        3 // Three-byte character
    } else if first_byte & 0b1111_1000 == 0b1111_0000 {
        4 // Four-byte character
    } else {
        1 // Invalid UTF-8, truncate it
    };

    // If there aren't enough bytes left for a valid character, truncate
    if i + expected_len <= data.len() {
        i + expected_len
    } else {
        i
    }
}
