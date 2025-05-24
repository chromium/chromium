#![allow(clippy::needless_range_loop)]

use std::panic::AssertUnwindSafe;

use toktrie::SimpleVob;

fn bools_to_bin_string(bits: &[bool]) -> String {
    bits.iter().map(|b| if *b { '1' } else { '0' }).collect()
}

#[test]
fn test_allow_range_empty() {
    let mut v = SimpleVob::alloc(32);
    #[allow(clippy::reversed_empty_ranges)]
    v.allow_range(10..=9); // no effect
    assert_eq!(v.num_set(), 0);
    assert_eq!(v.to_bin_string(), "00000000000000000000000000000000");
}

#[test]
fn test_allow_range_single_bit() {
    let mut v = SimpleVob::alloc(32);
    v.allow_range(5..=5);
    // Only bit 5 set
    let mut bits = vec![false; 32];
    bits[5] = true;
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_exact_word_boundary() {
    // BITS = 32, so check transitions around 32
    let mut v = SimpleVob::alloc(64);
    // set range [28..36], bridging two words
    v.allow_range(28..=35);
    // bits 28..31 in word 0, and 32..35 in word 1 set
    let mut bits = vec![false; 64];
    for i in 28..=35 {
        bits[i] = true;
    }
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_multiple_words() {
    let mut v = SimpleVob::alloc(96);
    // set range [10..85], spanning multiple 32-bit words
    v.allow_range(10..=84);
    let mut bits = vec![false; 96];
    #[allow(clippy::needless_range_loop)]
    for i in 10..85 {
        bits[i] = true;
    }
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_start_and_end_midword() {
    let mut v = SimpleVob::alloc(96);
    // set [5..60], both start and end are midword
    v.allow_range(5..=59);
    let mut bits = vec![false; 96];
    for i in 5..60 {
        bits[i] = true;
    }
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_end_at_word_boundary() {
    let mut v = SimpleVob::alloc(64);
    // set range [5..32]
    v.allow_range(5..=31);
    let mut bits = vec![false; 64];
    for i in 5..32 {
        bits[i] = true;
    }
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_entire_capacity() {
    // set entire range from 0..size
    let mut v = SimpleVob::alloc(64);
    v.allow_range(0..=63);
    // all bits should be set
    let bits = vec![true; 64];
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_allow_range_partial_out_of_bounds() {
    // check that assertion triggers if range.end > self.size
    let mut v = SimpleVob::alloc(32);
    let result = std::panic::catch_unwind(AssertUnwindSafe(|| {
        v.allow_range(0..=32); // out of bounds
    }));
    assert!(result.is_err());
}

#[test]
fn test_allow_range_upper_edge() {
    // set the last bit in the buffer
    let mut v = SimpleVob::alloc(32);
    v.allow_range(31..=31);
    let mut bits = vec![false; 32];
    bits[31] = true;
    assert_eq!(v.to_bin_string(), bools_to_bin_string(&bits));
}

#[test]
fn test_new() {
    let v = SimpleVob::new();
    assert_eq!(v.len(), 0);
    assert_eq!(v.as_slice().len(), 0);
    assert!(v.is_zero());
}

#[test]
fn test_default() {
    let v = SimpleVob::default();
    assert_eq!(v.len(), 0);
    assert_eq!(v.as_slice().len(), 0);
    assert!(v.is_zero());
}

#[test]
fn test_from_slice() {
    let bits = [true, false, true, true, false];
    let v = SimpleVob::from_slice(&bits);
    assert_eq!(v.len(), bits.len());
    for (idx, &bit) in bits.iter().enumerate() {
        assert_eq!(v.get(idx), bit);
    }
}

#[test]
fn test_alloc() {
    let v = SimpleVob::alloc(50);
    assert_eq!(v.len(), 50);
    assert!((v.as_slice().len() * 32) >= 50);
    assert!(v.is_zero());
}

#[test]
fn test_alloc_ones() {
    let v = SimpleVob::alloc_ones(45);
    assert_eq!(v.len(), 45);
    assert_eq!(v.num_set(), 45);
    for i in 0..45 {
        assert!(v.get(i));
    }
}

#[test]
fn test_alloc_with_capacity() {
    let v = SimpleVob::alloc_with_capacity(10, 32);
    assert_eq!(v.len(), 10);
    // capacity is 32, but only 10 "active" bits
    assert_eq!(v.num_set(), 0);
}

#[test]
fn test_len_and_num_set() {
    let bits = [true, true, false, true];
    let v = SimpleVob::from_slice(&bits);
    assert_eq!(v.len(), 4);
    assert_eq!(v.num_set(), 3);
}

#[test]
fn test_to_bin_string() {
    let bits = [true, false, true];
    let v = SimpleVob::from_slice(&bits);
    assert_eq!(v.to_bin_string(), "101");
}

#[test]
fn test_negated() {
    let bits = [true, false, true, false];
    let v = SimpleVob::from_slice(&bits);
    let n = v.negated();
    for i in 0..bits.len() {
        assert_eq!(v.get(i), !n.get(i));
    }
}

#[test]
fn test_iter_set_entries() {
    let bits = [false, true, false, true, true, false];
    let v = SimpleVob::from_slice(&bits);
    let mut set_positions = Vec::new();
    v.iter_set_entries(|idx| set_positions.push(idx));
    assert_eq!(set_positions, vec![1, 3, 4]);
}

#[test]
fn test_iter_unset_entries() {
    let bits = [true, false, true, false, false, true];
    let v = SimpleVob::from_slice(&bits);
    let mut unset_positions = Vec::new();
    v.iter_unset_entries(|idx| unset_positions.push(idx));
    assert_eq!(unset_positions, vec![1, 3, 4]);
}

#[test]
fn test_iter_entries() {
    let bits = [true, false, true];
    let v = SimpleVob::from_slice(&bits);
    let mut pairs = Vec::new();
    v.iter_entries(|val, idx| pairs.push((val, idx)));
    assert_eq!(pairs, vec![(true, 0), (false, 1), (true, 2)]);
}

#[test]
fn test_write_to() {
    let mut v = SimpleVob::alloc(64);
    v.allow_range(0..=31);
    // write out first two u32's
    let mut buf = [0u8; 8];
    v.write_to(&mut buf);
    let words: &[u32] = bytemuck::cast_slice(&buf);
    // first word should be 0xffffffff if the first 32 bits are set
    assert_eq!(words[0], 0xffffffff);
    // second word should be 0 (no bits set from 32..64)
    assert_eq!(words[1], 0);
}

#[test]
fn test_allow_token_disallow_token() {
    let mut v = SimpleVob::alloc(16);
    v.allow_token(3);
    assert!(v.is_allowed(3));
    v.disallow_token(3);
    assert!(!v.is_allowed(3));
}

#[test]
fn test_set() {
    let mut v = SimpleVob::alloc(8);
    v.set(2, true);
    v.set(5, true);
    assert!(v.get(2));
    assert!(v.get(5));
}

#[test]
fn test_resize() {
    let mut v = SimpleVob::alloc(32);
    v.allow_range(0..=7);
    assert_eq!(v.num_set(), 8);
    // resize to a bigger size
    v.resize(64);
    assert_eq!(v.len(), 64);
    // old bits preserved
    for i in 0..8 {
        assert!(v.get(i));
    }
    // newly added bits are unset
    for i in 8..64 {
        assert!(!v.get(i));
    }
}

#[test]
fn test_get_and_is_allowed() {
    let bits = [false, true, true, false];
    let v = SimpleVob::from_slice(&bits);
    assert!(v.get(1));
    assert!(v.is_allowed(2));
    assert!(!v.is_allowed(0));
}

#[test]
fn test_set_all() {
    let mut v = SimpleVob::alloc(10);
    v.set_all(true);
    assert_eq!(v.num_set(), 10);
    v.set_all(false);
    assert!(v.is_zero());
}

#[test]
fn test_apply_to() {
    // any "allowed" bit sets logits[idx] to 0.0
    let mut v = SimpleVob::alloc(10);
    v.allow_range(3..=4);
    let mut logits = vec![1.0; 10];
    v.apply_to(&mut logits);
    // bits 3 and 4 should be 0.0
    for (i, &val) in logits.iter().enumerate() {
        if i == 3 || i == 4 {
            assert_eq!(val, 0.0);
        } else {
            assert_eq!(val, 1.0);
        }
    }
}

#[test]
fn test_iter() {
    let bits = [false, true, true, false, true];
    let v = SimpleVob::from_slice(&bits);
    let collected: Vec<u32> = v.iter().collect();
    assert_eq!(collected, vec![1, 2, 4]);
}

#[test]
fn test_set_from() {
    let bits = [true, false, true];
    let src = SimpleVob::from_slice(&bits);
    let mut dst = SimpleVob::alloc(bits.len());
    dst.set_from(&src);
    for (i, &bit) in bits.iter().enumerate() {
        assert_eq!(dst.get(i), bit);
    }
}

#[test]
fn test_or() {
    let mut v1 = SimpleVob::from_slice(&[true, false, false, true]);
    let v2 = SimpleVob::from_slice(&[false, true, false, true]);
    v1.or(&v2);
    // expected: [true, true, false, true]
    assert!(v1.get(0));
    assert!(v1.get(1));
    assert!(!v1.get(2));
    assert!(v1.get(3));
}

#[test]
fn test_trim_trailing_zeros() {
    // add 64 bits, set bits near start
    let mut v = SimpleVob::alloc(64);
    v.allow_range(0..=4);
    // trim
    v.trim_trailing_zeros();
    assert_eq!(v.as_slice().len(), 1);
    assert_eq!(v.len(), 32);
    assert_eq!(v.num_set(), 5);
}

#[test]
fn test_or_minus() {
    // self |= other & !minus
    let mut self_v = SimpleVob::from_slice(&[false, false, false, false]);
    let other = SimpleVob::from_slice(&[true, true, false, true]);
    let minus = SimpleVob::from_slice(&[false, true, false, false]);
    self_v.or_minus(&other, &minus);
    // expect: bits 0,3 from 'other' minus bit 1
    assert!(self_v.get(0));
    assert!(!self_v.get(1));
    assert!(!self_v.get(2));
    assert!(self_v.get(3));
}

#[test]
fn test_and() {
    let mut v1 = SimpleVob::from_slice(&[true, true, false, true]);
    let v2 = SimpleVob::from_slice(&[true, false, true, true]);
    v1.and(&v2);
    // expected: [true, false, false, true]
    assert!(v1.get(0));
    assert!(!v1.get(1));
    assert!(!v1.get(2));
    assert!(v1.get(3));
}

#[test]
fn test_is_zero() {
    let v = SimpleVob::alloc(10);
    assert!(v.is_zero());
}

#[test]
fn test_and_is_zero() {
    let v1 = SimpleVob::from_slice(&[false, true, false]);
    let v2 = SimpleVob::from_slice(&[true, false, false]);
    // no overlapping bits set
    assert!(v1.and_is_zero(&v2));
}

#[test]
fn test_sub() {
    let mut v1 = SimpleVob::from_slice(&[true, true, true]);
    let v2 = SimpleVob::from_slice(&[false, true, false]);
    // result: [true, false, true]
    v1.sub(&v2);
    assert!(v1.get(0));
    assert!(!v1.get(1));
    assert!(v1.get(2));
}

#[test]
fn test_first_bit_set_here_and_in() {
    let v1 = SimpleVob::from_slice(&[false, true, false, true]);
    let v2 = SimpleVob::from_slice(&[true, true, true, false]);
    // overlap is bit 1
    let idx = v1.first_bit_set_here_and_in(&v2).unwrap();
    assert_eq!(idx, 1);
}

#[test]
fn test_first_bit_set() {
    let v = SimpleVob::from_slice(&[false, false, true, false]);
    let idx = v.first_bit_set().unwrap();
    assert_eq!(idx, 2);
}

#[test]
fn test_into_vec() {
    let v = SimpleVob::from_slice(&[true, false, true, false]);
    let data: Vec<u32> = v.into();
    // size is 4, so one word is enough
    assert_eq!(data.len(), 1);
    assert_eq!(data[0], 0b101);
}
