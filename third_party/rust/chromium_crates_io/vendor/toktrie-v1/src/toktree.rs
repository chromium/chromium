// use 8:24 encoding - num_ch:tok_id (ch_byte:ch_off)* - 8 bytes per tree node
// special case num_ch=0xff -> num_ch=0x100

use core::str;

use bytemuck_derive::{Pod, Zeroable};

use crate::{bytes::to_hex_string, tokenv::parse_numeric_token, SimpleVob};

pub type TokenId = u32;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Zeroable, Pod)]
#[repr(C)]
pub struct BinTokRxInfo {
    pub vocab_size: u32,
    pub tok_eos: TokenId,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct TokRxInfo {
    pub vocab_size: u32,
    pub tok_eos: TokenId,
    pub tok_bos: Option<TokenId>,
    pub tok_pad: Option<TokenId>,
    pub tok_unk: Option<TokenId>,
    pub tok_end_of_turn: Option<TokenId>,
}

impl TokRxInfo {
    pub fn new(vocab_size: u32, tok_eos: TokenId) -> Self {
        TokRxInfo {
            vocab_size,
            tok_eos,
            tok_bos: None,
            tok_pad: None,
            tok_unk: None,
            tok_end_of_turn: None,
        }
    }

    pub fn from_bin(info: &BinTokRxInfo) -> Self {
        TokRxInfo {
            vocab_size: info.vocab_size,
            tok_eos: info.tok_eos,
            tok_bos: None,
            tok_pad: None,
            tok_unk: None,
            tok_end_of_turn: None,
        }
    }

    pub fn to_bin(&self) -> BinTokRxInfo {
        BinTokRxInfo {
            vocab_size: self.vocab_size,
            tok_eos: self.tok_eos,
        }
    }
}

pub trait Recognizer {
    /// for _ in 0..num { stack.pop() }
    fn pop_bytes(&mut self, num: usize);
    /// "Collapse" the stack so that it consists only of its former
    /// top element.
    /// X = stack.top(); stack.empty(); stack.push(X)
    fn collapse(&mut self);
    /// check if stack.top() transitions via byte to a viable state
    fn byte_allowed(&mut self, byte: u8) -> bool {
        if self.try_push_byte(byte) {
            self.pop_bytes(1);
            true
        } else {
            false
        }
    }
    /// Called when iteration over the trie is finished
    /// Stack has exactly one element then, except when iteration started from non-root node.
    /// In that case, the stack may have more than one element, and trie_finished() needs to pop the excessive elements.
    fn trie_finished(&mut self);
    /// Called when iteration over the trie is started
    fn trie_started(&mut self, _dbg_lbl: &str) {}
    /// This combines `push_byte` and `byte_allowed` into one function for performance.
    fn try_push_byte(&mut self, byte: u8) -> bool;
    /// Check if there are any errors to be reported to the user.
    fn get_error(&mut self) -> Option<String> {
        None
    }
    fn save_stats(&mut self, _nodes_walked: usize) {}
}

#[derive(Clone, Copy)]
struct TokDesc {
    len: u32,
    off: u32,
}

#[derive(Clone)]
pub struct TokTrie {
    info: TokRxInfo,
    token_offsets: Vec<TokDesc>,
    token_data: Vec<u8>,
    nodes: Vec<TrieNode>,
    max_token_len: usize,
}

#[derive(Clone, Copy, Zeroable, Pod)]
#[repr(C)]
pub struct TrieNode {
    // byte:token
    bits: u32,
    bits2: u32,
}

pub const INVALID_TOKEN: TokenId = 0xffff_ffff;

const NO_TOKEN: u32 = 0xffffff;

// PARENT_BITS=10 allows for up to 1024 parents, which is likely enough for tokens up to 2k bytes
// this leaves 32-10 = 22 bits for subtree size, which allows for up to ~2M tokens
// (4M trie nodes)
// GLM4 tokenizer has a token with 1024 spaces - it requires PARENT_BITS >= 9
// Note that because of the ~2M limit, we have ~3 bits left free in 'bits' field
const PARENT_BITS: u32 = 10;
const PARENT_MASK: u32 = (1 << PARENT_BITS) - 1;

impl TrieNode {
    fn new(byte: u8, token_id: u32, num_parents: usize) -> TrieNode {
        assert!(num_parents > 0);
        assert!(num_parents <= (1 << PARENT_BITS) as usize);
        TrieNode {
            bits: (token_id << 8) | byte as u32,
            bits2: (num_parents - 1) as u32,
        }
    }

    #[inline(always)]
    pub fn byte(&self) -> u8 {
        (self.bits & 0xff) as u8
    }

    #[inline(always)]
    pub fn subtree_size(&self) -> usize {
        (self.bits2 >> PARENT_BITS) as usize
    }

    fn set_subtree_size(&mut self, size: usize) {
        assert!(size < (1 << (32 - PARENT_BITS)));
        self.bits2 = (self.bits2 & PARENT_MASK) | ((size as u32) << PARENT_BITS);
    }

    #[inline(always)]
    pub fn num_parents(&self) -> usize {
        ((self.bits2 & PARENT_MASK) + 1) as usize
    }

    #[inline(always)]
    pub fn token_id(&self) -> Option<u32> {
        let r = self.bits >> 8;
        if r == NO_TOKEN {
            None
        } else {
            Some(r)
        }
    }
}

impl TokTrie {
    // see https://github.com/microsoft/llguidance/blob/main/docs/special_tokens.md
    pub const SPECIAL_TOKEN_MARKER: u8 = 0xff;

    pub fn from(info: &TokRxInfo, words: &[Vec<u8>]) -> Self {
        let mut trie = TrieHash::new(0xff);
        let mut token_offsets = Vec::new();
        let mut token_data = Vec::new();
        assert!(info.vocab_size == words.len() as u32);
        let mut max_token_len = 0;
        for (idx, word) in words.iter().enumerate() {
            if !word.is_empty() {
                trie.insert(word, idx as u32);
                max_token_len = std::cmp::max(max_token_len, word.len());
            }
            let desc = TokDesc {
                len: word.len().try_into().unwrap(),
                off: token_data.len().try_into().unwrap(),
            };
            token_offsets.push(desc);
            token_data.extend_from_slice(word);
        }
        let mut nodes = Vec::new();
        trie.serialize(&mut nodes, 0);
        let r = TokTrie {
            info: *info,
            token_offsets,
            token_data,
            nodes,
            max_token_len,
        };
        r.validate();
        r
    }

    pub fn filter(&self, filter: &SimpleVob) -> Self {
        let mut words = vec![];
        for n in 0..(self.vocab_size() as TokenId) {
            let b = if filter.is_allowed(n) {
                self.token(n)
            } else {
                &[]
            };
            words.push(b.to_vec());
        }
        Self::from(self.info(), &words)
    }

    pub fn with_eos_token(&self, eos_token: TokenId) -> Self {
        self.with_info(TokRxInfo {
            tok_eos: eos_token,
            ..self.info
        })
    }

    pub fn with_info(&self, info: TokRxInfo) -> Self {
        let mut r = self.clone();
        r.info = info;
        r
    }

    pub fn build_chat_mode_trie(&self) -> Self {
        self.with_eos_token(self.info.tok_end_of_turn.unwrap_or(self.info.tok_eos))
    }

    fn node_offset(&self, n: &TrieNode) -> usize {
        let off = (n as *const _ as usize - self.root() as *const _ as usize)
            / std::mem::size_of::<TrieNode>();
        assert!(off < self.nodes.len());
        off
    }

    fn next_node(&self, n: &TrieNode) -> usize {
        self.node_offset(n) + n.subtree_size()
    }

    pub fn info(&self) -> &TokRxInfo {
        &self.info
    }

    pub fn eos_token(&self) -> TokenId {
        self.info.tok_eos
    }

    pub fn vocab_size(&self) -> usize {
        self.info.vocab_size as usize
    }

    pub fn alloc_token_set(&self) -> SimpleVob {
        SimpleVob::alloc_with_capacity(self.vocab_size(), self.vocab_size() + 1)
    }

    pub fn singleton_token_set(&self, tok: TokenId) -> SimpleVob {
        let mut r = self.alloc_token_set();
        r.allow_token(tok);
        r
    }

    pub fn token_set_dbg(&self, ts: &SimpleVob) -> String {
        let max_examples = 50;

        let ts_neg = ts.negated();
        let use_neg = ts_neg.num_set() * 10 < ts.num_set();
        let ts1 = if use_neg { &ts_neg } else { ts };
        let num_set = ts1.num_set();
        let max_tok = std::cmp::min(max_examples, num_set);
        let mut token_names = Vec::new();
        // make sure we include EOS first if it's allowed
        if self.info.tok_eos != INVALID_TOKEN && ts1.is_allowed(self.info.tok_eos) {
            token_names.push("EOS".to_string());
        }
        for idx in 0..self.vocab_size() {
            if idx as TokenId != self.info.tok_eos && ts1.is_allowed(idx as TokenId) {
                token_names.push(self.token_dbg(idx as TokenId));
                if token_names.len() >= max_tok {
                    break;
                }
            }
        }
        if token_names.len() < num_set {
            token_names.push("...".to_string());
        }
        format!(
            "TokenSet: {}/{}; {}{}",
            ts.num_set(),
            self.vocab_size(),
            if use_neg { "ALL EXCEPT " } else { "" },
            token_names.join(" ")
        )
    }

    pub fn alloc_logits(&self) -> Vec<f32> {
        vec![0.0; self.vocab_size() + 1]
    }

    pub fn test_trace_tokens(&self, toks: &[u32]) -> String {
        self.tokens_dbg_ext(toks, false)
    }

    pub const MAX_DBG_TOKENS: usize = 200;

    pub fn tokens_dbg(&self, toks: &[u32]) -> String {
        self.tokens_dbg_ext(toks, true)
    }

    fn tokens_dbg_ext(&self, toks: &[u32], quote: bool) -> String {
        // if the token list is too long, we are typically interested in the most recent ones
        let (limited, toks) = if toks.len() > Self::MAX_DBG_TOKENS {
            ("…", &toks[toks.len() - Self::MAX_DBG_TOKENS..])
        } else {
            ("", toks)
        };

        let joined = toks
            .iter()
            .map(|t| self.token_dbg_ext(*t, false))
            .collect::<Vec<_>>()
            .join("‧");

        if quote {
            format!("⟦{}{}⟧", limited, joined)
        } else if limited.is_empty() {
            joined
        } else {
            format!("{}{}", limited, joined)
        }
    }

    pub fn token_dbg(&self, idx: u32) -> String {
        self.token_dbg_ext(idx, true)
    }

    fn token_dbg_ext(&self, idx: u32, quote: bool) -> String {
        if idx == self.info.tok_eos {
            "≺EOS≻".to_string()
        } else if idx as usize >= self.vocab_size() {
            format!("≺OOB[{}]≻", idx)
        } else {
            // format!("{:?}[{}]", self.token_str(idx), idx)
            let bytes = self.token(idx);
            if bytes.len() > 1 && bytes[0] == TokTrie::SPECIAL_TOKEN_MARKER {
                String::from_utf8_lossy(&bytes[1..]).to_string()
            } else {
                let s = String::from_utf8_lossy(bytes);
                if s.is_empty() {
                    format!("≺EMPTY[{}]≻", idx)
                } else if !s.contains('\u{fffd}') {
                    let mut s = format!("{:?}", s).replace("\\\"", "\"");
                    s.remove(0);
                    s.pop();
                    if quote {
                        format!("⟨{}⟩", s)
                    } else {
                        s
                    }
                } else {
                    let bytes = self.token(idx);
                    format!("≺HEX[{}]≻", to_hex_string(bytes))
                }
            }
        }
    }

    pub fn token_str(&self, idx: u32) -> String {
        String::from_utf8_lossy(self.token(idx)).to_string()
    }

    pub fn token_len(&self, idx: u32) -> usize {
        let t = self.token(idx);
        if t.is_empty() || t[0] == TokTrie::SPECIAL_TOKEN_MARKER {
            let mut idx = idx;
            let mut len = 1;
            while idx >= 10 {
                idx /= 10;
                len += 1;
            }
            // token 1234 -> \xff [ 1234 ]
            len + 3
        } else {
            t.len()
        }
    }

    pub fn token(&self, idx: u32) -> &[u8] {
        if idx >= self.token_offsets.len() as u32 {
            return &[];
        }
        let desc = self.token_offsets[idx as usize];
        let len = desc.len as usize;
        let off = desc.off as usize;
        &self.token_data[off..(off + len)]
    }

    pub fn decode(&self, tokens: &[TokenId]) -> Vec<u8> {
        self.decode_ext(tokens, true)
    }

    pub fn decode_ext(&self, tokens: &[TokenId], include_special: bool) -> Vec<u8> {
        let mut res = Vec::with_capacity(tokens.len() * 6 + 32); // approximately
        for &tok in tokens {
            let t = self.token(tok);
            if t.is_empty() {
                if include_special {
                    res.extend_from_slice(format!("<[{}]>", tok).as_bytes());
                }
            } else if t[0] == TokTrie::SPECIAL_TOKEN_MARKER {
                if include_special {
                    res.extend_from_slice(&t[1..]);
                }
            } else {
                res.extend_from_slice(t);
            }
        }
        res
    }

    pub fn decode_as_special(&self, tok: TokenId) -> Vec<u8> {
        let mut res = Vec::with_capacity(9);
        res.push(TokTrie::SPECIAL_TOKEN_MARKER);
        res.extend_from_slice(format!("[{}]", tok).as_bytes());
        res
    }

    pub fn decode_raw(&self, tokens: &[TokenId]) -> Vec<u8> {
        let mut res = Vec::with_capacity(tokens.len() * 6 + 32); // approximately
        for &tok in tokens {
            let t = self.token(tok);
            if t.is_empty() || t[0] == TokTrie::SPECIAL_TOKEN_MARKER {
                res.push(TokTrie::SPECIAL_TOKEN_MARKER);
                res.extend_from_slice(format!("[{}]", tok).as_bytes());
            } else {
                res.extend_from_slice(t);
            }
        }
        res
    }

    pub fn decode_str(&self, tokens: &[TokenId]) -> String {
        String::from_utf8_lossy(&self.decode(tokens)).to_string()
    }

    pub fn decode_raw_to_decode(&self, bytes: &[u8]) -> Vec<u8> {
        let mut res = Vec::new();
        let mut idx = 0;
        while idx < bytes.len() {
            if bytes[idx] == TokTrie::SPECIAL_TOKEN_MARKER {
                if let Some((len, tok)) = parse_numeric_token(&bytes[(idx + 1)..]) {
                    res.extend_from_slice(&self.decode(&[tok]));
                    idx += len + 1;
                } else {
                    res.push(bytes[idx]);
                    idx += 1;
                }
            } else {
                res.push(bytes[idx]);
                idx += 1;
            }
        }
        res
    }

    pub fn is_special_token(&self, tok: TokenId) -> bool {
        let bytes = self.token(tok);
        !bytes.is_empty() && bytes[0] == TokTrie::SPECIAL_TOKEN_MARKER
    }

    pub fn get_special_token(&self, name: &str) -> Option<TokenId> {
        self.child_at_byte(self.root(), TokTrie::SPECIAL_TOKEN_MARKER)
            .and_then(|n| {
                self.child_at_bytes(n, name.as_bytes())
                    .and_then(|n| n.token_id())
            })
    }

    pub fn get_special_tokens(&self) -> Vec<TokenId> {
        let mut res = Vec::new();
        let pref_node = self
            .child_at_byte(self.root(), TokTrie::SPECIAL_TOKEN_MARKER)
            .expect("missing special token prefix");
        let mut stack = vec![pref_node];
        while let Some(n) = stack.pop() {
            for c in self.node_children(n) {
                if let Some(tok) = c.token_id() {
                    res.push(tok);
                    if res.len() > Self::MAX_DBG_TOKENS + 1 {
                        break;
                    }
                }
                stack.push(c);
            }
        }
        res.remove(0);
        res
    }

    pub fn greedy_tokenize(&self, bytes: &[u8]) -> Vec<TokenId> {
        let mut tokens = Vec::new();
        let mut i = 0;
        while i < bytes.len() {
            let mut node = self.root();
            let mut last_tok = None;
            let mut last_idx = i;
            #[allow(clippy::needless_range_loop)]
            for j in i..bytes.len() {
                if let Some(child) = self.child_at_byte(node, bytes[j]) {
                    node = child;
                    if let Some(tok) = node.token_id() {
                        last_tok = Some(tok);
                        last_idx = j;
                    }
                } else {
                    break;
                }
            }
            if let Some(t) = last_tok {
                tokens.push(t);
            } else {
                // whoops, there is a byte missing from the tokenizer
                // just carry on...
                // https://github.com/guidance-ai/llguidance/issues/138
            }
            i = last_idx + 1;
        }
        tokens
    }

    /// Tokenize a string, interpreting `<name>` as special tokens.
    pub fn tokenize_with_special<F>(&self, s: &str, str_tokenize: F) -> Vec<TokenId>
    where
        F: Fn(&str) -> Vec<TokenId>,
    {
        let max_len = 100;

        let bytes = s.as_bytes();
        let mut out = Vec::new();
        let mut last = 0; // byte‐offset of the next “raw” segment
        let mut i = 0; // current byte index

        while i < bytes.len() {
            if bytes[i] != b'<' {
                i += 1;
                continue;
            }
            // Potential start of `<...>`
            let mut valid = true;
            let mut j = i + 1;
            let mut len_inside = 0;
            // scan up to max_len chars or until we hit `>` or `<`
            while j < bytes.len() && len_inside < max_len {
                match bytes[j] {
                    b'<' => {
                        valid = false;
                        break;
                    }
                    b'>' => break,
                    _ => {
                        len_inside += 1;
                        j += 1;
                    }
                }
            }
            if !valid || j >= bytes.len() || bytes[j] != b'>' || len_inside == 0 {
                // treat this `<` as literal
                i += 1;
                continue;
            }

            let name = &s[i..=j];
            if let Some(special_tok) = self.get_special_token(name) {
                if last < i {
                    out.extend(str_tokenize(&s[last..i]));
                }
                out.push(special_tok);
            } else {
                // fallback: tokenize `<name>` literally
                out.extend(str_tokenize(&s[last..=j]));
            }
            // advance past the `>`
            i = j + 1;
            last = i;
        }
        // any trailing text:
        if last < bytes.len() {
            out.extend(str_tokenize(&s[last..]));
        }
        out
    }

    pub fn tokenize_with_greedy_fallback(
        &self,
        bytes: &[u8],
        str_tokenize: impl Fn(&str) -> Vec<TokenId>,
    ) -> Vec<TokenId> {
        match str::from_utf8(bytes) {
            Ok(s) => {
                // fast path
                str_tokenize(s)
            }
            Err(_) => {
                let mut res = vec![];
                for chunk in bytes.utf8_chunks() {
                    if !chunk.valid().is_empty() {
                        res.extend(str_tokenize(chunk.valid()));
                    }
                    if !chunk.invalid().is_empty() {
                        res.extend(self.greedy_tokenize(chunk.invalid()));
                    }
                }
                res
            }
        }
    }

    pub fn has_extensions(&self, bytes: &[u8]) -> bool {
        match self.child_at_bytes(self.root(), bytes) {
            None => false,
            Some(n) => n.subtree_size() > 1,
        }
    }

    pub fn token_id(&self, bytes: &[u8]) -> Option<TokenId> {
        let (tok, len) = self.prefix_token_id(bytes);
        // println!("tok_id {:?} {:?} {:?} ", bytes, tok, len);
        if len == bytes.len() {
            Some(tok)
        } else {
            None
        }
    }

    pub fn prefix_token_id(&self, bytes: &[u8]) -> (TokenId, usize) {
        assert!(!bytes.is_empty());
        let mut last = (0, 0);
        let mut n = self.root();
        for (idx, byte) in bytes.iter().enumerate() {
            n = match self.child_at_byte(n, *byte) {
                Some(n) => n,
                None => break,
            };
            if let Some(tok) = n.token_id() {
                last = (tok, idx + 1);
            }
        }
        last
    }

    pub fn max_token_len(&self) -> usize {
        self.max_token_len
    }

    fn validate_node(&self, n: &TrieNode, ep: usize, used: &mut [bool]) {
        if let Some(tok) = n.token_id() {
            assert!(tok < self.info.vocab_size);
            assert!(!used[tok as usize]);
            used[tok as usize] = true;
        }
        let endp = self.next_node(n);
        assert!(endp <= ep);
        for child in self.node_children(n) {
            self.validate_node(child, endp, used);
        }
    }

    fn validate(&self) {
        self.validate_node(
            self.root(),
            self.next_node(self.root()),
            &mut vec![false; self.info.vocab_size as usize],
        );
        for idx in 0..self.info.vocab_size {
            let _ = self.token(idx);
        }
    }

    pub fn root(&self) -> &TrieNode {
        &self.nodes[0]
    }

    pub fn check_against(&self, tokens: &[Vec<u8>]) {
        for (idx, bytes) in tokens.iter().enumerate() {
            let tid = idx as TokenId;
            assert!(bytes == self.token(tid));
            let root = self.root();
            if !bytes.is_empty() {
                let tid2 = self
                    .child_at_bytes(root, bytes)
                    .unwrap()
                    .token_id()
                    .unwrap();
                if tid != tid2 {
                    let par = self
                        .child_at_bytes(root, &bytes[0..bytes.len() - 1])
                        .unwrap();
                    let has_it = self.node_children(par).any(|n| {
                        n.subtree_size() == 1
                            && n.byte() == bytes[bytes.len() - 1]
                            && n.token_id() == Some(tid)
                    });
                    assert!(has_it);
                }
            }
        }
    }

    pub fn child_at_byte<'a>(&'a self, n: &'a TrieNode, byte: u8) -> Option<&'a TrieNode> {
        self.node_children(n).find(|&child| child.byte() == byte)
    }

    pub fn all_subtokens(&self, bytes: &[u8]) -> Vec<TokenId> {
        let mut r = Vec::new();
        for i in 0..bytes.len() {
            let mut n = self.root();
            for &b in &bytes[i..] {
                n = match self.child_at_byte(n, b) {
                    Some(n) => n,
                    None => break,
                };
                if let Some(tok) = n.token_id() {
                    r.push(tok);
                }
            }
        }
        r
    }

    pub fn node_children(&self, n: &TrieNode) -> NodeChildren {
        let off = self.node_offset(n);
        NodeChildren {
            trie: self,
            current_offset: off + 1,
            end_offset: off + n.subtree_size(),
        }
    }

    pub fn child_at_bytes<'a>(&'a self, mut n: &'a TrieNode, bytes: &[u8]) -> Option<&'a TrieNode> {
        for &byte in bytes {
            n = self.child_at_byte(n, byte)?
        }
        Some(n)
    }

    pub fn token_id_at_bytes(&self, bytes: &[u8]) -> Option<TokenId> {
        self.child_at_bytes(self.root(), bytes)
            .and_then(|n| n.token_id())
    }

    /// Return how many tokens and bytes need to chopped off tokens,
    /// so that we do not limit all possible future tokenizations matching the recognizer.
    pub fn chop_tokens(&self, r: &mut impl Recognizer, tokens: &[TokenId]) -> (usize, usize) {
        let max_token_lookback = 4;
        let suff_bytes =
            self.decode_raw(&tokens[tokens.len().saturating_sub(max_token_lookback)..]);
        let suff_bytes = &suff_bytes[suff_bytes.len().saturating_sub(self.max_token_len())..];
        // let suff_bytes = self.decode_raw(tokens);
        // let suff_bytes = &suff_bytes[suff_bytes.len().saturating_sub(6)..];

        // let mut anything_goes = StackRecognizer::from(AnythingGoes {});

        for idx in 0..suff_bytes.len() {
            let suff = &suff_bytes[idx..];
            if self.has_valid_extensions(r, suff) {
                let chop_bytes = suff.len();
                assert!(chop_bytes > 0);
                let mut curr_len = 0;
                for chop_idx in 1..=tokens.len() {
                    curr_len += self.token_len(tokens[tokens.len() - chop_idx]);
                    if curr_len >= chop_bytes {
                        return (chop_idx, curr_len);
                    }
                }
                unreachable!();
            }
        }

        (0, 0)
    }

    /// Check if add_bias() would have returned any tokens.
    #[inline(never)]
    pub fn has_valid_extensions(&self, r: &mut impl Recognizer, start: &[u8]) -> bool {
        let n = self.child_at_bytes(self.root(), start);
        if n.is_none() {
            return false;
        }
        let n = n.unwrap();
        r.trie_started("has_valid_extensions");
        let off = self.node_offset(n);
        let mut p = off + 1;
        let endp = off + n.subtree_size();
        let mut ok = false;
        let mut next_pop = 0;
        while p < endp {
            r.pop_bytes(next_pop);
            let n = &self.nodes[p];
            let b = n.byte();
            if r.try_push_byte(b) {
                if n.token_id().is_some() {
                    ok = true;
                    break;
                }
                next_pop = if n.subtree_size() == 1 {
                    n.num_parents()
                } else {
                    0
                };
                p += 1;
            } else {
                p += n.subtree_size();
                next_pop = n.num_parents() - 1;
            }
        }
        r.trie_finished();
        ok
    }

    pub fn all_prefixes(&self, bytes: &[u8]) -> Vec<TokenId> {
        let mut r = Vec::new();
        let mut n = self.root();
        for &b in bytes {
            if let Some(c) = self.child_at_byte(n, b) {
                n = c;
                if let Some(tok) = n.token_id() {
                    r.push(tok);
                }
            } else {
                break;
            }
        }
        r
    }

    pub fn add_bias(&self, r: &mut impl Recognizer, toks: &mut SimpleVob, start: &[u8]) {
        // all prefixes of 'start' are also allowed
        if !start.is_empty() {
            let mut fixed = FixedRecognizer::new(start);
            self.add_bias(&mut fixed, toks, &[]);
        }

        let n = self.child_at_bytes(self.root(), start);
        if n.is_none() {
            return;
        }
        let n = n.unwrap();
        r.trie_started("add_bias");
        let (next_pop, nodes_walked) = self.add_bias_inner(r, toks, n);
        if start.is_empty() {
            // if start was non-empty, trie_finished() is supposed to clean this up
            r.pop_bytes(next_pop);
        }
        r.trie_finished();
        r.save_stats(nodes_walked);
        // revert the fake token
        let defl_tok = self.vocab_size() as u32;
        toks.disallow_token(defl_tok);
    }

    #[inline(never)]
    fn add_bias_inner(
        &self,
        r: &mut impl Recognizer,
        toks: &mut SimpleVob,
        n: &TrieNode,
    ) -> (usize, usize) {
        let defl_tok = self.vocab_size() as u32;
        let off = self.node_offset(n);
        let total_nodes = n.subtree_size();
        let mut p = off + 1;
        let endp = off + total_nodes;
        let mut next_pop = 0;
        let mut num_skip = 0;
        while p < endp {
            r.pop_bytes(next_pop);
            let n = &self.nodes[p];
            let b = n.byte();
            if r.try_push_byte(b) {
                toks.allow_token(n.token_id().unwrap_or(defl_tok));
                next_pop = if n.subtree_size() == 1 {
                    n.num_parents()
                } else {
                    0
                };
                p += 1;
            } else {
                let subtree_size = n.subtree_size();
                p += subtree_size;
                // it's slightly faster to count skipped nodes, than walked nodes
                num_skip += subtree_size - 1;
                next_pop = n.num_parents() - 1;
            }
        }
        (next_pop, total_nodes - num_skip)
    }

    pub fn all_tokens(&self) -> Vec<Vec<u8>> {
        (0..self.vocab_size())
            .map(|idx| self.token(idx as u32).to_vec())
            .collect()
    }

    pub fn sorted_tokens(&self) -> Vec<(u32, Vec<u8>)> {
        let mut res = vec![];
        let n = self.root();
        let off = self.node_offset(n);
        let mut p = off + 1;
        let endp = off + n.subtree_size();
        let mut next_pop = 0;
        let mut bytes = vec![];
        while p < endp {
            bytes.drain(bytes.len() - next_pop..);
            let n = &self.nodes[p];
            let b = n.byte();
            bytes.push(b);
            if let Some(t) = n.token_id() {
                res.push((t, bytes.clone()));
            }
            next_pop = if n.subtree_size() == 1 {
                n.num_parents()
            } else {
                0
            };
            p += 1;
        }
        res
    }

    fn count_until_depth(&self, depth: usize) -> (usize, usize) {
        let mut count = 0;
        let mut num_tokens = 0;
        let mut stack = vec![(self.root(), 0)];
        while let Some((n, d)) = stack.pop() {
            if d == depth {
                continue;
            } else {
                for c in self.node_children(n) {
                    count += 1;
                    if c.token_id().is_some() {
                        num_tokens += 1;
                    }
                    stack.push((c, d + 1));
                }
            }
        }
        (count, num_tokens)
    }

    pub fn trie_stats(&self) -> String {
        let mut nodes_histogram = vec![0; 256];

        let mut token_nodes = 0;

        let n = self.root();
        let off = self.node_offset(n);
        let mut p = off + 1;
        let endp = off + n.subtree_size();
        while p < endp {
            let n = &self.nodes[p];

            if n.token_id().is_some() {
                token_nodes += 1;
            }

            let last_ch = self.next_node(n);
            let mut ch_p = p + 1;
            let mut num_children = 0;

            while ch_p < last_ch {
                let ch = &self.nodes[ch_p];
                ch_p += ch.subtree_size();
                num_children += 1;
            }

            nodes_histogram[std::cmp::min(9, num_children)] += 1;

            p += 1;
        }

        let mut histogram = String::new();

        if false {
            for (idx, num) in nodes_histogram.iter().enumerate() {
                if *num > 0 {
                    if !histogram.is_empty() {
                        histogram.push_str(", ");
                    }
                    histogram.push_str(&format!("{}:{}", idx, num));
                }
            }
        }

        if false {
            for n in self.node_children(self.root()) {
                histogram.push_str(&format!(
                    "\n{} => {} {}",
                    n.byte(),
                    self.node_children(n).count(),
                    n.subtree_size()
                ));
            }
        }

        if false {
            for depth in 0..30 {
                let (count, num_tokens) = self.count_until_depth(depth);
                histogram.push_str(&format!(
                    "\ndepth {}: {} nodes {} tokens",
                    depth, count, num_tokens
                ));
            }
        }

        if !histogram.is_empty() {
            histogram = format!("\n{}", histogram);
        }

        format!(
            "{}{} nodes, {} token nodes, {} token bytes, {} max len",
            histogram,
            self.nodes.len(),
            token_nodes,
            self.token_data.len(),
            self.max_token_len,
        )
    }
}

pub struct NodeChildren<'a> {
    trie: &'a TokTrie,
    current_offset: usize,
    end_offset: usize,
}

impl<'a> Iterator for NodeChildren<'a> {
    type Item = &'a TrieNode;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current_offset < self.end_offset {
            let node = &self.trie.nodes[self.current_offset];
            self.current_offset += node.subtree_size();
            Some(node)
        } else {
            None
        }
    }
}

struct TrieHash {
    token_id: u32,
    byte: u8,
    children: Vec<TrieHash>,
}

impl TrieHash {
    fn new(byte: u8) -> TrieHash {
        TrieHash {
            token_id: NO_TOKEN,
            byte,
            children: Vec::new(),
        }
    }
    fn insert(&mut self, word: &[u8], token_id: u32) {
        if word.is_empty() {
            // Some tokenizers have duplicate tokens...
            // we just override
            assert!(self.token_id == NO_TOKEN);
            self.token_id = token_id;
        } else {
            // if self.children.len() == 0x100 {
            //     // assert!(self.children[word[0] as usize].byte == word[0]);
            //     self.children[word[0] as usize].insert(&word[1..], token_id);
            //     return;
            // }

            for ch in &mut self.children {
                if ch.byte == word[0] {
                    if word.len() == 1 && ch.token_id != NO_TOKEN {
                        // this is duplicate token, proceed with adding a duplicate node
                    } else {
                        ch.insert(&word[1..], token_id);
                        return;
                    }
                }
            }

            let mut ch = TrieHash::new(word[0]);
            ch.insert(&word[1..], token_id);
            self.children.push(ch);

            // if it's getting dense, make it full
            // for cl100k threshold 60->15 nodes, 50->22, 40->45 30->94
            // for llama (32k) 50->5, 40->15
            // TODO remove this?
            // if self.children.len() > 250 {
            //     let mut v2 = (0..=255).map(TrieHash::new).collect::<Vec<_>>();
            //     for ch in self.children.drain(..) {
            //         let idx = ch.byte as usize;
            //         v2[idx] = ch;
            //     }
            //     self.children = v2;
            // }
        }
    }

    fn serialize(&mut self, data: &mut Vec<TrieNode>, num_parents: usize) {
        let idx = data.len();
        let mut num_ch = self.children.len();
        data.push(TrieNode::new(
            self.byte,
            self.token_id,
            if num_parents == 0 { 1 } else { num_parents },
        ));
        //self.children.reverse();
        self.children.sort_by_key(|e| e.byte);
        for entry in &mut self.children {
            num_ch -= 1;
            entry.serialize(data, if num_ch == 0 { num_parents + 1 } else { 1 });
        }
        let subtree_size = data.len() - idx;
        data[idx].set_subtree_size(subtree_size);
    }
}

struct FixedRecognizer {
    bytes: Vec<u8>,
    bytes_ptr: usize,
}

impl FixedRecognizer {
    fn new(bytes: &[u8]) -> FixedRecognizer {
        FixedRecognizer {
            bytes: bytes.to_vec(),
            bytes_ptr: 0,
        }
    }
}

impl Recognizer for FixedRecognizer {
    fn collapse(&mut self) {}
    fn trie_finished(&mut self) {}

    fn pop_bytes(&mut self, num: usize) {
        self.bytes_ptr -= num;
    }

    fn try_push_byte(&mut self, byte: u8) -> bool {
        if self.bytes_ptr < self.bytes.len() && self.bytes[self.bytes_ptr] == byte {
            self.bytes_ptr += 1;
            true
        } else {
            false
        }
    }
}

pub struct AnythingGoes;

impl Recognizer for AnythingGoes {
    fn collapse(&mut self) {}
    fn trie_finished(&mut self) {}
    fn pop_bytes(&mut self, _num: usize) {}
    fn try_push_byte(&mut self, _byte: u8) -> bool {
        true
    }
}
