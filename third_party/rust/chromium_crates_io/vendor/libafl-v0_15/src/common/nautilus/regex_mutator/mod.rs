use alloc::vec::Vec;
use core::num::NonZero;

use libafl_bolts::rands::Rand;
use regex_syntax::hir::{Class, ClassBytesRange, ClassUnicodeRange, Hir, Literal};

#[derive(Debug)]
pub struct RegexScript {
    remaining: usize,
}

impl RegexScript {
    pub fn new<R: Rand>(rand: &mut R) -> Self {
        let len = if rand.next().is_multiple_of(256) {
            rand.next() % 0xffff
        } else {
            let len = 1 << (rand.next() % 8);
            rand.next() % len
        };
        RegexScript {
            remaining: len as usize,
        }
    }

    pub fn get_mod<R: Rand>(&mut self, rand: &mut R, val: usize) -> usize {
        if self.remaining == 0 || val == 0 {
            0
        } else {
            // # Safety
            // This is checked above to be non-null.
            rand.below(unsafe { NonZero::new(val).unwrap_unchecked() })
        }
    }

    pub fn get_range<R: Rand>(&mut self, rand: &mut R, min: usize, max: usize) -> usize {
        self.get_mod(rand, max - min) + min
    }
}

fn append_char(res: &mut Vec<u8>, chr: char) {
    let mut buf = [0; 4];
    res.extend_from_slice(chr.encode_utf8(&mut buf).as_bytes());
}

fn append_lit(res: &mut Vec<u8>, lit: &Literal) {
    res.extend_from_slice(&lit.0);
}

fn append_unicode_range<R: Rand>(
    rand: &mut R,
    res: &mut Vec<u8>,
    scr: &mut RegexScript,
    cls: ClassUnicodeRange,
) {
    let mut chr_a_buf = [0; 4];
    let mut chr_b_buf = [0; 4];
    cls.start().encode_utf8(&mut chr_a_buf);
    cls.end().encode_utf8(&mut chr_b_buf);
    let a = u32::from_le_bytes(chr_a_buf);
    let b = u32::from_le_bytes(chr_b_buf);
    let c = scr.get_range(rand, a as usize, (b + 1) as usize) as u32;
    append_char(res, core::char::from_u32(c).unwrap());
}

fn append_byte_range<R: Rand>(
    rand: &mut R,
    res: &mut Vec<u8>,
    scr: &mut RegexScript,
    cls: ClassBytesRange,
) {
    res.push(scr.get_range(rand, cls.start() as usize, (cls.end() + 1) as usize) as u8);
}

fn append_class<R: Rand>(rand: &mut R, res: &mut Vec<u8>, scr: &mut RegexScript, cls: &Class) {
    use regex_syntax::hir::Class::{Bytes, Unicode};
    match cls {
        Unicode(cls) => {
            let rngs = cls.ranges();
            let rng = rngs[scr.get_mod(rand, rngs.len())];
            append_unicode_range(rand, res, scr, rng);
        }
        Bytes(cls) => {
            let rngs = cls.ranges();
            let rng = rngs[scr.get_mod(rand, rngs.len())];
            append_byte_range(rand, res, scr, rng);
        }
    }
}

fn get_length<R: Rand>(rand: &mut R, scr: &mut RegexScript) -> usize {
    let bits = scr.get_mod(rand, 8);
    scr.get_mod(rand, 2 << bits)
}

fn get_repetition_range<R: Rand>(
    rand: &mut R,
    min: u32,
    max: Option<u32>,
    scr: &mut RegexScript,
) -> usize {
    match (min, max) {
        (a, None) => get_length(rand, scr) + (a as usize),
        (a, Some(b)) if a == b => a as usize,
        (a, Some(b)) => scr.get_range(rand, a as usize, b as usize),
    }
}

fn get_repetitions<R: Rand>(
    rand: &mut R,
    min: u32,
    max: Option<u32>,
    scr: &mut RegexScript,
) -> usize {
    match (min, max) {
        (0, Some(1)) => scr.get_mod(rand, 2),
        (0, _) => get_length(rand, scr),
        (1, _) => 1 + get_length(rand, scr),
        (min, max) => get_repetition_range(rand, min, max, scr),
    }
}

pub fn generate<R: Rand>(rand: &mut R, hir: &Hir) -> Vec<u8> {
    use regex_syntax::hir::HirKind;
    let mut scr = RegexScript::new(rand);
    let mut stack = vec![hir];
    let mut res = vec![];
    while !stack.is_empty() {
        match stack.pop().unwrap().kind() {
            HirKind::Empty => {}
            HirKind::Literal(lit) => append_lit(&mut res, lit),
            HirKind::Class(cls) => append_class(rand, &mut res, &mut scr, cls),
            HirKind::Repetition(repetition) => {
                let num = get_repetitions(rand, repetition.min, repetition.max, &mut scr);
                for _ in 0..num {
                    stack.push(&repetition.sub);
                }
            }
            HirKind::Capture(grp) => stack.push(&grp.sub),
            HirKind::Concat(hirs) => hirs.iter().rev().for_each(|h| stack.push(h)),
            HirKind::Alternation(hirs) => stack.push(&hirs[scr.get_mod(rand, hirs.len())]),
            HirKind::Look(_) => (),
        }
    }
    res
}
