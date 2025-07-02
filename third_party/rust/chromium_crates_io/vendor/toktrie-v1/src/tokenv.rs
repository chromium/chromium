use std::sync::Arc;

use crate::{TokRxInfo, TokTrie, TokenId};

pub trait TokenizerEnv: Send {
    /// Associated trie.
    fn tok_trie(&self) -> &TokTrie;

    /// Tokenize a given byte sequence.
    /// It may or may not interpret <|special_tokens|> as special.
    fn tokenize_bytes(&self, s: &[u8]) -> Vec<TokenId>;

    /// Tokenize a given byte sequence.
    /// It will interpret text starting with SPECIAL_TOKEN_MARKER as special tokens.
    /// Returns tokens, and number of tokens are should never be re-tokenized
    /// (because they were specified using the special token marker).
    fn tokenize_bytes_marker(&self, s: &[u8]) -> (Vec<TokenId>, usize) {
        let mut idx = 0;
        let ff = TokTrie::SPECIAL_TOKEN_MARKER;
        let mut result = Vec::new();
        let trie = self.tok_trie();
        let mut num_fixed_tokens = 0;
        while idx < s.len() {
            let normal_len = s[idx..]
                .iter()
                .position(|&x| x == ff)
                .unwrap_or(s.len() - idx);
            if normal_len != 0 {
                let new_tokens = self.tokenize_bytes(&s[idx..idx + normal_len]);
                for (idx, t) in new_tokens.iter().enumerate() {
                    if trie.is_special_token(*t) {
                        num_fixed_tokens = result.len() + idx + 1;
                    }
                }
                result.extend_from_slice(&new_tokens);
                idx += normal_len;
            }
            idx += 1; // skip ff
            if idx + 2 < s.len() && s[idx] == b'<' {
                // tokenize \xff<foobar> as special token <foobar>
                let spec_len = s[idx..std::cmp::min(s.len(), idx + 100)]
                    .iter()
                    .position(|&x| x == b'>');
                if let Some(mut spec_len) = spec_len {
                    spec_len += 1;
                    let spec_token = &s[idx - 1..idx + spec_len];
                    if let Some(id) = trie.token_id_at_bytes(spec_token) {
                        result.push(id);
                        num_fixed_tokens = result.len();
                        idx += spec_len;
                    }
                }
            } else if idx < s.len() {
                // tokenize \xff[1234] as token 1234
                if let Some((n_bytes, tok_id)) = parse_numeric_token(&s[idx..]) {
                    if tok_id < trie.vocab_size() as u32 {
                        result.push(tok_id);
                        num_fixed_tokens = result.len();
                        idx += n_bytes;
                    }
                }
            }
        }

        (result, num_fixed_tokens)
    }

    /// Tokenize a string coming from user. It may or may not interpret <|special_tokens|> as special.
    fn tokenize(&self, s: &str) -> Vec<TokenId> {
        self.tokenize_bytes(s.as_bytes())
    }

    /// Tokenize a string. It will interpret <|special_tokens|> as special.
    fn tokenize_special(&self, s: &str) -> Vec<TokenId> {
        self.tokenize_bytes_special(s.as_bytes())
    }

    /// Tokenize a byte slice. It will interpret <|special_tokens|> as special.
    fn tokenize_bytes_special(&self, s: &[u8]) -> Vec<TokenId> {
        self.tokenize_bytes(s)
    }

    /// End of sentence token
    fn eos_token(&self) -> TokenId {
        self.tok_trie().eos_token()
    }

    /// If this returns true, this tokenizer always returns canonical tokenizations
    /// and can be used for forcing tokens.
    /// Non-canonical tokenizers will typically just use TokTrie::greedy_tokenize().
    fn tokenize_is_canonical(&self) -> bool {
        true
    }
}

pub type TokEnv = Arc<dyn TokenizerEnv + Sync + 'static>;

pub struct TokEnvWithTrie {
    base_env: TokEnv,
    tok_trie: TokTrie,
}

impl TokEnvWithTrie {
    pub fn new(base_env: TokEnv, tok_trie: TokTrie) -> Self {
        Self { base_env, tok_trie }
    }
}

impl TokenizerEnv for TokEnvWithTrie {
    fn tok_trie(&self) -> &TokTrie {
        &self.tok_trie
    }

    fn tokenize_bytes(&self, s: &[u8]) -> Vec<TokenId> {
        self.base_env.tokenize_bytes(s)
    }
}

/// Parse a special token of the form \xFF [ 1 2 3 4 ]
/// The initial \xFF is not included in the input.
/// Returns the number of bytes consumed and the token id.
pub fn parse_numeric_token(s: &[u8]) -> Option<(usize, TokenId)> {
    let spec_len = s[0..std::cmp::min(s.len(), 20)]
        .iter()
        .position(|&x| x == b']');
    if let Some(spec_len) = spec_len {
        if s[0] != b'[' {
            return None;
        }
        let inner_bytes = &s[1..spec_len];
        if let Ok(inner_str) = std::str::from_utf8(inner_bytes) {
            if let Ok(id) = inner_str.parse::<u32>() {
                return Some((spec_len + 1, id as TokenId));
            }
        }
    }
    None
}

pub struct ApproximateTokEnv {
    trie: TokTrie,
    canonical: bool,
}

impl ApproximateTokEnv {
    pub fn new(trie: TokTrie) -> Self {
        Self {
            trie,
            canonical: false,
        }
    }

    // this is mostly used for testing
    pub fn single_byte() -> Self {
        let mut words = (0..=255).map(|x| vec![x]).collect::<Vec<_>>();
        // add some special tokens to play with
        words.push(b"\xFF<|tool|>".to_vec());
        words.push(b"\xFF<|/tool|>".to_vec());
        words.push(b"\xFF<|user|>".to_vec());
        words.push(b"\xFF<|system|>".to_vec());
        words.push(b"\xFF<|assistant|>".to_vec());
        words.push(b"\xFF<|end|>".to_vec());
        let info = TokRxInfo {
            vocab_size: words.len() as u32,
            tok_eos: words.len() as u32 - 1,
            tok_bos: None,
            tok_pad: None,
            tok_unk: None,
            tok_end_of_turn: None,
        };
        let mut r = ApproximateTokEnv::new(TokTrie::from(&info, &words));
        r.canonical = true;
        r
    }

    pub fn single_byte_env() -> TokEnv {
        Arc::new(Self::single_byte())
    }
}

impl TokenizerEnv for ApproximateTokEnv {
    fn tok_trie(&self) -> &TokTrie {
        &self.trie
    }

    fn tokenize_bytes(&self, s: &[u8]) -> Vec<TokenId> {
        self.trie.greedy_tokenize(s)
    }

    fn tokenize_is_canonical(&self) -> bool {
        self.canonical
    }
}
