use std::collections::hash_map::Entry;
use std::hash::Hash;

use crate::HashMap;

use crate::{
    ast::{byteset_contains, Expr, ExprSet},
    ExprRef,
};

pub struct ByteCompressor {
    pub mapping: Vec<u8>,
    pub alphabet_size: usize,
    bytesets: Vec<Vec<u32>>,
}

const INVALID_MAPPING: u8 = 0xff;

impl ByteCompressor {
    pub fn new() -> Self {
        ByteCompressor {
            mapping: Vec::new(),
            alphabet_size: 0,
            bytesets: Vec::new(),
        }
    }

    fn add_single_byte(&mut self, b: u8) {
        if self.mapping[b as usize] == INVALID_MAPPING {
            self.mapping[b as usize] = self.alphabet_size as u8;
            self.alphabet_size += 1;
        }
    }

    pub fn compress(&mut self, exprset: ExprSet, rx_list: &[ExprRef]) -> (ExprSet, Vec<ExprRef>) {
        self.mapping = vec![INVALID_MAPPING; exprset.alphabet_size];

        let mut todo = rx_list.to_vec();
        let mut visited = vec![false; exprset.len()];
        while let Some(e) = todo.pop() {
            if visited[e.as_usize()] {
                continue;
            }
            visited[e.as_usize()] = true;
            todo.extend_from_slice(exprset.get_args(e));
            match exprset.get(e) {
                Expr::Byte(b) => self.add_single_byte(b),
                Expr::ByteConcat(_, bytes, _) => {
                    for b in bytes {
                        self.add_single_byte(*b);
                    }
                }
                Expr::ByteSet(bs) => self.bytesets.push(bs.to_vec()),
                Expr::RemainderIs { scale, .. } => {
                    for b in exprset.digits {
                        self.add_single_byte(b);
                    }
                    // if scale==0 then it will only match integers
                    // and we don't need to distinguish the dot
                    if scale > 0 {
                        self.add_single_byte(exprset.digit_dot);
                    }
                }
                _ => {}
            }
        }

        let num = self.bytesets.len();
        if num <= 64 {
            self.compress_bytesets(|_| 0u64, |v, idx| *v |= 1 << idx);
        } else {
            self.compress_bytesets(
                |size| vec![0u32; size.div_ceil(32)],
                |v, idx| v[idx / 32] |= 1 << (idx % 32),
            );
        }

        let mut trg = exprset;

        // this disables Or->Trie conversion; the input should be already optimized this way
        trg.disable_optimizations();
        (trg, rx_list.to_vec())
    }

    #[inline(always)]
    fn compress_bytesets<T: Eq + Hash>(
        &mut self,
        alloc: impl Fn(usize) -> T,
        set_true: impl Fn(&mut T, usize),
    ) {
        let mut byte_mapping = HashMap::default();
        for b in 0..self.mapping.len() {
            if self.mapping[b] == INVALID_MAPPING {
                let mut v = alloc(self.bytesets.len());
                for (idx, bs) in self.bytesets.iter().enumerate() {
                    if byteset_contains(bs, b) {
                        set_true(&mut v, idx);
                    }
                }
                match byte_mapping.entry(v) {
                    Entry::Occupied(e) => {
                        self.mapping[b] = *e.get();
                    }
                    Entry::Vacant(e) => {
                        self.mapping[b] = self.alphabet_size as u8;
                        self.alphabet_size += 1;
                        e.insert(self.mapping[b]);
                    }
                }
            }
        }
    }
}
