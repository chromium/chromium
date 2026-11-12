use super::*;

// A bit-at-a-time oracle, independent of the mask, word loads and refill policy.
fn reference_bits(input: &[u8], position: usize, count: u32) -> u32 {
  let mut value = 0;
  for bit in 0..count {
    let offset = position + bit as usize;
    value |= (((input[offset / 8] >> (offset % 8)) & 1) as u32) << bit;
  }
  value
}

fn reference_word(input: &[u8]) -> u64 {
  let mut value = 0;
  for (index, &byte) in input.iter().enumerate() {
    value |= (byte as u64) << (8 * index);
  }
  value
}

fn pattern(input: &mut [u8], mut seed: u32) {
  for byte in input {
    seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
    *byte = (seed >> 24) as u8;
  }
}

// Width of the bit register in bytes: 8 on 64-bit targets, 4 on 32-bit ones.
const REG_BYTES: usize = ::core::mem::size_of::<reg_t>();

// Widest read that `BrotliGetBits` can serve from one refill: 32 bits with a
// 64-bit register, 16 with a 32-bit one.
const MAX_PEEK_BITS: u32 = if REG_BYTES == 8 { 32 } else { 16 };

fn consumed_bits(reader: &BrotliBitReader) -> usize {
  reader.next_in as usize * 8 - BrotliGetAvailableBits(reader) as usize
}

#[test]
fn little_endian_loads_accept_unaligned_and_exact_length_slices() {
  let mut input = [0u8; 32];
  for &seed in &[0, 1, 0x89abcdef, 0xffffffff] {
    pattern(&mut input, seed);
    for offset in 0..16 {
      // End the slice at the final byte of the load: no implicit padding.
      assert_eq!(
        BrotliLoad32LE(&input[..offset + 4], offset as u32) as u64,
        reference_word(&input[offset..offset + 4])
      );
      assert_eq!(
        BrotliLoad64LE(&input[..offset + 8], offset as u32),
        reference_word(&input[offset..offset + 8])
      );
    }
  }
}

#[test]
fn variable_reads_match_reference_bits_and_cursors() {
  let mut storage = [0u8; 40];
  for &seed in &[0, 1, 0x89abcdef, 0xffffffff] {
    pattern(&mut storage, seed);
    for alignment in 0..8 {
      let input = &storage[alignment..];
      // Fast reads require a warmed-up register (bit_pos < register width).
      for position in 0..(REG_BYTES as u32 * 8) {
        for count in 0..33 {
          let mut reader = super::tests::old_layout(
            reference_word(&input[..REG_BYTES]) as reg_t,
            position,
            (input.len() - REG_BYTES) as u32,
            REG_BYTES as u32,
          );
          let expected = reference_bits(input, position as usize, count);
          assert_eq!(
            BrotliReadBits(&mut reader, count, input),
            expected,
            "position {}, width {}, alignment {}",
            position,
            count,
            alignment
          );
          let consumed = position as usize + count as usize;
          assert_eq!(consumed_bits(&reader), consumed);
          assert_eq!(
            BrotliGetRemainingBytes(&reader) as usize,
            (input.len() * 8 - consumed) / 8
          );

          // Returning prefetched whole bytes must restore the logical cursor,
          // regardless of how many bytes the refill policy chose to read.
          BrotliBitReaderUnload(&mut reader);
          let next_byte = (consumed + 7) / 8;
          let residual_bits = (next_byte * 8 - consumed) as u32;
          assert_eq!(reader.next_in as usize, next_byte);
          assert_eq!(reader.avail_in as usize, input.len() - next_byte);
          assert_eq!(BrotliGetAvailableBits(&reader), residual_bits);
          if residual_bits != 0 {
            let mut value = 0;
            BrotliTakeBits(&mut reader, residual_bits, &mut value);
            assert_eq!(value, reference_bits(input, consumed, residual_bits));
          }
        }
      }
    }
  }
}

#[test]
fn mixed_reads_preserve_bits_across_refills_unload_and_safe_tail() {
  let mut storage = [0u8; 1031];
  pattern(&mut storage, 0x12345678);
  for alignment in 0..8 {
    let input = &storage[alignment..];
    let mut reader = BrotliBitReader {
      avail_in: input.len() as u32,
      ..BrotliBitReader::default()
    };
    BrotliInitBitReader(&mut reader);
    let mut position = 0;
    let mut step = 0;
    while input.len() * 8 - position >= 32 {
      assert!(BrotliWarmupBitReader(&mut reader, input));
      let count = ((step * 19 + alignment) % 33) as u32;
      let expected = reference_bits(input, position, count);
      let saved = BrotliBitReaderSaveState(&reader);
      let value = if reader.avail_in >= 8 {
        match step % 3 {
          0 => BrotliReadBits(&mut reader, count, input),
          1 => BrotliReadConstantNBits(&mut reader, count, input),
          _ if count <= MAX_PEEK_BITS => {
            assert_eq!(BrotliGetBits(&mut reader, count, input), expected);
            BrotliReadBits(&mut reader, count, input)
          }
          _ => BrotliReadConstantNBits(&mut reader, count, input),
        }
      } else {
        let mut value = 0;
        assert!(BrotliSafeReadBits(&mut reader, count, &mut value, input));
        value
      };
      assert_eq!(value, expected);
      position += count as usize;
      assert_eq!(consumed_bits(&reader), position);
      if step % 7 == 0 {
        // Rollback and retry through the byte-at-a-time path.
        BrotliBitReaderRestoreState(&mut reader, &saved);
        let mut value = 0;
        assert!(BrotliSafeReadBits(&mut reader, count, &mut value, input));
        assert_eq!(value, expected);
      }
      if step % 5 == 0 {
        BrotliBitReaderUnload(&mut reader);
        assert_eq!(reader.next_in as usize, (position + 7) / 8);
      }
      step += 1;
    }
    while position < input.len() * 8 {
      let mut value = 0;
      assert!(BrotliSafeReadBits(&mut reader, 1, &mut value, input));
      assert_eq!(value, reference_bits(input, position, 1));
      position += 1;
    }
    let mut sentinel = 0xdeadbeef;
    assert!(!BrotliSafeReadBits(&mut reader, 1, &mut sentinel, input));
    assert_eq!(sentinel, 0xdeadbeef);
    assert_eq!(BrotliGetRemainingBytes(&reader), 0);
  }
}

#[test]
fn truncated_safe_reads_resume_without_losing_bits() {
  let input = [0xa5, 0x5a, 0x87, 0x91, 0x23, 0xed, 0xcb, 0xfe];
  for position in 0..8 {
    for count in 1..33 {
      for supplied in 1..8 {
        let mut reader = BrotliBitReader {
          avail_in: supplied,
          ..BrotliBitReader::default()
        };
        BrotliInitBitReader(&mut reader);
        assert!(BrotliWarmupBitReader(
          &mut reader,
          &input[..supplied as usize]
        ));
        BrotliDropBits(&mut reader, position);
        let mut value = 0xdeadbeef;
        let enough =
          BrotliSafeReadBits(&mut reader, count, &mut value, &input[..supplied as usize]);
        assert_eq!(enough, supplied * 8 - position >= count);
        if !enough {
          assert_eq!(value, 0xdeadbeef);
          assert_eq!(consumed_bits(&reader), position as usize);
          reader.avail_in += input.len() as u32 - supplied;
          assert!(BrotliSafeReadBits(&mut reader, count, &mut value, &input));
        }
        assert_eq!(value, reference_bits(&input, position as usize, count));
        assert_eq!(consumed_bits(&reader), (position + count) as usize);
      }
    }
  }
}

#[test]
fn fast_bit_reader_matches_standard_reader_and_reference_bits() {
  let mut storage = [0u8; 256];
  for &seed in &[0, 1, 0x89abcdef, 0xffffffff] {
    pattern(&mut storage, seed);
    for alignment in 0..8 {
      let input = &storage[alignment..];
      for position in 0..(REG_BYTES as u32 * 8) {
        for count in 0..25 {
          let mut std_reader = super::tests::old_layout(
            reference_word(&input[..REG_BYTES]) as reg_t,
            position,
            (input.len() - REG_BYTES) as u32,
            REG_BYTES as u32,
          );
          let mut fast_backing = std_reader;
          let mut fast_reader = BitReaderSlice::new(&fast_backing, input).unwrap();

          let expected = reference_bits(input, position as usize, count);
          let std_val = BrotliReadBits(&mut std_reader, count, input);
          let fast_val = fast_reader.read_bits(count);
          assert_eq!(fast_val, expected);
          assert_eq!(fast_val, std_val);
          assert_eq!(fast_reader.avail_in(), std_reader.avail_in);

          fast_reader.sync_to(&mut fast_backing);
          BrotliBitReaderClearUnclaimed(&mut std_reader);
          let mut fast_cleared = fast_backing;
          BrotliBitReaderClearUnclaimed(&mut fast_cleared);
          assert_eq!(fast_cleared.val_, std_reader.val_);
          assert_eq!(fast_backing.bit_pos_, std_reader.bit_pos_);
          assert_eq!(fast_backing.next_in, std_reader.next_in);
          assert_eq!(fast_backing.avail_in, std_reader.avail_in);
        }
      }

      // Multi-step sequence mixing `fill16`, `fill_short`, `drop_bits`,
      // `read_bits`, and periodic `sync_to` / reload.
      let mut std_reader = super::tests::old_layout(
        reference_word(&input[..REG_BYTES]) as reg_t,
        0,
        (input.len() - REG_BYTES) as u32,
        REG_BYTES as u32,
      );
      let mut fast_backing = std_reader;
      let mut fast_reader = BitReaderSlice::new(&fast_backing, input).unwrap();
      let mut position = 0usize;
      let mut step = 0usize;
      while fast_reader.avail_in() >= 16 {
        let count = ((step * 13 + alignment) % 25) as u32;
        let expected = reference_bits(input, position, count);
        match step % 3 {
          0 => {
            let std_peek = BrotliGet16BitsUnmasked(&mut std_reader, input);
            fast_reader.fill16();
            let fast_peek = fast_reader.val as u32;
            assert_eq!(fast_peek & 0xffff, std_peek & 0xffff);
            let width = count.min(16);
            assert_eq!(fast_peek & BitMask(width), reference_bits(input, position, width));
            BrotliDropBits(&mut std_reader, width);
            fast_reader.drop_bits(width);
            position += width as usize;
          }
          1 => {
            let width = count.min(16);
            BrotliFillBitWindow16(&mut std_reader, input);
            fast_reader.fill_short();
            let std_val = (BrotliGetBitsUnmasked(&std_reader) as u32) & BitMask(width);
            let fast_val = fast_reader.take_bits(width);
            assert_eq!(fast_val, std_val);
            assert_eq!(fast_val, reference_bits(input, position, width));
            BrotliDropBits(&mut std_reader, width);
            position += width as usize;
          }
          _ => {
            let std_val = BrotliReadBits(&mut std_reader, count, input);
            let fast_val = fast_reader.read_bits(count);
            assert_eq!(fast_val, std_val);
            assert_eq!(fast_val, expected);
            position += count as usize;
          }
        }
        assert_eq!(fast_reader.avail_in(), std_reader.avail_in);
        if step % 4 == 0 {
          fast_reader.sync_to(&mut fast_backing);
          BrotliBitReaderClearUnclaimed(&mut std_reader);
          let mut fast_cleared = fast_backing;
          BrotliBitReaderClearUnclaimed(&mut fast_cleared);
          assert_eq!(fast_cleared.val_, std_reader.val_);
          assert_eq!(fast_backing.bit_pos_, std_reader.bit_pos_);
          assert_eq!(fast_backing.next_in, std_reader.next_in);
          assert_eq!(fast_backing.avail_in, std_reader.avail_in);
          fast_reader = BitReaderSlice::new(&fast_backing, input).unwrap();
        }
        step += 1;
      }
    }
  }
}
