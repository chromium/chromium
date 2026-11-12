#![allow(non_snake_case)]
use core::default::Default;
macro_rules! xprintln (
  ($a : expr) => ();
  ($a : expr, $b : expr) => ();
  ($a : expr, $b : expr, $c : expr) => ();
  ($a : expr, $b : expr, $c : expr, $d : expr) => ();
  ($a : expr, $b : expr, $c : expr, $d : expr, $e : expr) => ();
  ($a : expr, $b : expr, $c : expr, $d : expr, $e : expr, $f : expr) => ();
);

#[cfg(target_pointer_width = "64")]
#[allow(non_camel_case_types)]
pub type reg_t = u64;

#[cfg(not(target_pointer_width = "64"))]
#[allow(non_camel_case_types)]
pub type reg_t = u32;

pub const BROTLI_SHORT_FILL_BIT_WINDOW_READ: u32 = (::core::mem::size_of::<reg_t>() >> 1) as u32;
const REG_BITS: u32 = (::core::mem::size_of::<reg_t>() as u32) << 3;
const REG_SHIFT_MASK: u32 = REG_BITS - 1;

#[cfg(test)]
mod regression_tests;

#[cfg(test)]
#[allow(non_upper_case_globals)]
const kBitMask: [u32; 33] =
  [0x0000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
   0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
   0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
   0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
   0xFFFFFFFF];

// Equals kBitMask[n] without the table load and its bounds check; C computes
// it the same way wherever it has a bit-field extract instruction.
// On 64-bit targets it is computed in 64 bits, so it holds for every n in
// 0..=32. On 32-bit targets a 64-bit shift is a multi-instruction sequence,
// and making n == 32 work costs the hot paths an extra compare and select
// (+1.4% instructions on Cortex-A53), so it holds for n in 0..=31 only: the
// 32-bit reader never needs a 32-bit mask, as it splits reads wider than 16
// bits into two (`BrotliReadBits`, `BrotliSafeReadBits32Slow`) and peeks at
// most 16 bits.
#[inline(always)]
pub fn BitMask(n: u32) -> u32 {
  #[cfg(target_pointer_width = "64")]
  {
    (!(u64::MAX << (n & 63))) as u32
  }
  #[cfg(not(target_pointer_width = "64"))]
  {
    debug_assert!(n < 32, "BitMask({}) on a 32-bit target", n);
    !(u32::MAX << (n & 31))
  }
}

// The unread bits are kept right-aligned in val_, the next bit lowest, as in
// the C decoder: peeking is a plain read of val_, and only dropping bits
// shifts.  bit_pos_ is the number of valid bits in val_.  Bits of val_ above
// bit_pos_ may hold bits of the bytes that follow, which a refill writes over
// with the same values.
#[derive(Clone, Copy)]
pub struct BrotliBitReader {
  pub val_: reg_t, // pre-fetched bits, next bit lowest
  pub bit_pos_: u32, // number of valid bits in val_
  pub next_in: u32, // the byte we're reading from
  pub avail_in: u32,
}

impl Default for BrotliBitReader {
  fn default() -> Self {
    BrotliBitReader {
      val_: 0,
      bit_pos_: 0,
      next_in: 0,
      avail_in: 0,
    }
  }
}

pub struct BrotliBitReaderState {
  pub val_: reg_t,
  pub bit_pos_: u32,
  pub next_in: u32,
  pub avail_in: u32,
}
impl Default for BrotliBitReaderState {
  #[inline]
  fn default() -> Self {
    BrotliBitReaderState {
      val_: 0,
      bit_pos_: 0,
      next_in: 0,
      avail_in: 0,
    }
  }
}

pub fn BrotliBitReaderSaveState(from: &BrotliBitReader) -> BrotliBitReaderState {
  BrotliBitReaderState {
    val_: from.val_,
    bit_pos_: from.bit_pos_,
    next_in: from.next_in,
    avail_in: from.avail_in,
  }
}

pub fn BrotliBitReaderRestoreState(to: &mut BrotliBitReader, from: &BrotliBitReaderState) {
  to.val_ = from.val_;
  to.bit_pos_ = from.bit_pos_;
  to.next_in = from.next_in;
  to.avail_in = from.avail_in;
}

#[inline(always)]
pub fn BrotliGetAvailableBits(br: &BrotliBitReader) -> u32 {
  br.bit_pos_
}

// The bit reader keeps its cursor in u32, so the one-past-the-end offset must
// fit too: bounding the offset and the length separately would still let
// consuming the last byte overflow next_in. usize may be 64 bits wide, so the
// sum saturates rather than wraps.
#[inline]
pub fn is_valid_input_range(input_offset: usize, available_in: usize, input_len: usize) -> bool {
  let end = (input_offset as u64).saturating_add(available_in as u64);
  end <= input_len as u64 && end <= u64::from(u32::MAX)
}

pub fn is_valid_bit_reader(br: &BrotliBitReader, input: &[u8]) -> bool {
  let register_bits = (::core::mem::size_of::<reg_t>() as u32) << 3;
  br.bit_pos_ <= register_bits &&
    u64::from(br.next_in) + u64::from(br.avail_in) <= input.len() as u64
}

// Returns amount of unread bytes the bit reader still has buffered from the
// BrotliInput, including whole bytes in br->val_.
pub fn BrotliGetRemainingBytes(br: &BrotliBitReader) -> u32 {
  br.avail_in + (BrotliGetAvailableBits(br) >> 3)
}

// Checks if there is at least num bytes left in the input ringbuffer (excluding
// the bits remaining in br->val_).
#[inline(always)]
pub fn BrotliCheckInputAmount(br: &BrotliBitReader, num: u32) -> bool {
  br.avail_in >= num
}

#[inline(always)]
// Callers refill only when the caller checked enough input is available, so
// the zero fallback is unreachable; it replaces a panicking index.
#[allow(dead_code)]
fn BrotliLoad16LE(input: &[u8], next_in_u32: u32) -> u16 {
  use core::convert::TryInto;
  match input
    .get(next_in_u32 as usize..)
    .and_then(|rest| rest.get(..2))
    .and_then(|bytes| bytes.try_into().ok())
  {
    Some(bytes) => u16::from_le_bytes(bytes),
    None => {
      debug_assert!(false, "BrotliLoad16LE past the end of the input");
      0
    }
  }
}

// from_le_bytes compiles to one unaligned load; assembling the value byte by
// byte let LLVM split the load into pieces once callers shift out high bytes.
#[inline(always)]
fn BrotliLoad32LE(input: &[u8], next_in_u32: u32) -> u32 {
  let next_in: usize = next_in_u32 as usize;
  let mut four_byte: [u8; 4] = fast_uninitialized![4];
  four_byte.clone_from_slice(fast!((input)[next_in ; next_in + 4]));
  u32::from_le_bytes(four_byte)
}

#[allow(dead_code)]
#[inline(always)]
fn BrotliLoad64LE(input: &[u8], next_in_u32: u32) -> u64 {
  let next_in: usize = next_in_u32 as usize;
  let mut eight_byte: [u8; 8] = fast_uninitialized![8];
  eight_byte.clone_from_slice(fast!((input)[next_in ; next_in + 8]));
  u64::from_le_bytes(eight_byte)
}

#[cold]
#[inline(always)]
fn cold_bit_reader_path() {}

// Makes at least n_bits + 1 bits valid for n_bits <= 16, and at least 32 for
// n_bits <= 32 on 64-bit targets (exactly 32 when the accumulator was empty).
// The narrower cases load 64 bits (32 bits on 32-bit targets) and claim 7 or 6
// bytes (3 bytes on 32-bit targets) of them; the bits of the byte past those
// land above bit_pos_, where the next refill writes the same bits again.
#[inline(always)]
pub fn BrotliFillBitWindow(br: &mut BrotliBitReader, n_bits: u32, input: &[u8]) {
  #[cfg(target_pointer_width = "64")]
  {
    if n_bits <= 8 {
      if br.bit_pos_ <= 8 {
        br.val_ |= BrotliLoad64LE(input, br.next_in) << br.bit_pos_;
        br.bit_pos_ += 56;
        br.avail_in -= 7;
        br.next_in += 7;
      }
    } else if n_bits <= 16 {
      if br.bit_pos_ <= 16 {
        br.val_ |= BrotliLoad64LE(input, br.next_in) << br.bit_pos_;
        br.bit_pos_ += 48;
        br.avail_in -= 6;
        br.next_in += 6;
      }
    } else if br.bit_pos_ <= 32 {
      br.val_ |= (BrotliLoad32LE(input, br.next_in) as reg_t) << br.bit_pos_;
      br.bit_pos_ += 32;
      br.avail_in -= BROTLI_SHORT_FILL_BIT_WINDOW_READ;
      br.next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
  #[cfg(not(target_pointer_width = "64"))]
  {
    if n_bits <= 8 {
      if br.bit_pos_ <= 8 {
        br.val_ |= (BrotliLoad32LE(input, br.next_in) as reg_t) << br.bit_pos_;
        br.bit_pos_ += 24;
        br.avail_in -= 3;
        br.next_in += 3;
      }
    } else if br.bit_pos_ <= 16 {
      br.val_ |= (BrotliLoad16LE(input, br.next_in) as reg_t) << br.bit_pos_;
      br.bit_pos_ += 16;
      br.avail_in -= BROTLI_SHORT_FILL_BIT_WINDOW_READ;
      br.next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
}

// Mostly like BrotliFillBitWindow, but guarantees only 16 bits and reads no
// more than BROTLI_SHORT_FILL_BIT_WINDOW_READ bytes of input.
#[allow(dead_code)]
pub fn BrotliFillBitWindow16(br: &mut BrotliBitReader, input: &[u8]) {
  BrotliFillBitWindow(br, 17, input);
}

// Pulls one byte of input to accumulator.
#[inline(always)]
pub fn BrotliPullByte(br: &mut BrotliBitReader, input: &[u8]) -> bool {
  if br.avail_in == 0 {
    return false;
  }
  let byte = match input.get(br.next_in as usize) {
    Some(&b) => b as reg_t,
    None => return false,
  };
  br.val_ |= byte << br.bit_pos_;
  br.bit_pos_ += 8;
  br.avail_in -= 1;
  br.next_in += 1;
  true
}

// Returns currently available bits, next bit lowest; the valid ones are the
// low BrotliGetAvailableBits of them.
#[inline(always)]
pub fn BrotliGetBitsUnmasked(br: &BrotliBitReader) -> reg_t {
  br.val_
}

// Like BrotliGetBits, but does not mask the result.
// The result contains at least 16 valid bits.
#[inline(always)]
pub fn BrotliGet16BitsUnmasked(br: &mut BrotliBitReader, input: &[u8]) -> u32 {
  BrotliFillBitWindow(br, 16, input);
  BrotliGetBitsUnmasked(br) as u32
}

// Returns the specified number of bits from `br` without advancing bit pos.
// On 32-bit targets a single refill guarantees at most 16 bits, so `n_bits`
// must be `<= 16` (`<= 32` on 64-bit targets).
#[inline(always)]
pub fn BrotliGetBits(br: &mut BrotliBitReader, n_bits: u32, input: &[u8]) -> u32 {
  debug_assert!(::core::mem::size_of::<reg_t>() == 8 || n_bits <= 16);
  BrotliFillBitWindow(br, n_bits, input);
  (BrotliGetBitsUnmasked(br) as u32) & BitMask(n_bits)
}

// Returns the specified number of bits from `br` without advancing bit pos.
// On 32-bit targets a single refill guarantees at most 16 bits, so `n_bits`
// must be `<= 16` (`<= 32` on 64-bit targets).
#[allow (dead_code)]
pub fn BrotliGetConstantNBits(br: &mut BrotliBitReader, n_bits: u32, input: &[u8]) -> u32 {
  BrotliGetBits(br, n_bits, input)
}

// Tries to peek the specified amount of bits. Returns 0, if there is not
// enough input.
#[inline(always)]
pub fn BrotliSafeGetBits(br: &mut BrotliBitReader,
                         n_bits: u32,
                         val: &mut u32,
                         input: &[u8])
                         -> bool {
  while BrotliGetAvailableBits(br) < n_bits {
    if !BrotliPullByte(br, input) {
      return false;
    }
  }
  *val = (BrotliGetBitsUnmasked(br) as u32) & BitMask(n_bits);
  true
}

// Advances the bit pos by n_bits, at most 32.
#[inline(always)]
pub fn BrotliDropBits(br: &mut BrotliBitReader, n_bits: u32) {
  // `& REG_SHIFT_MASK` turns a full-width drop into a no-op, so callers
  // never drop all `REG_BITS` bits at once (wide reads are split).
  debug_assert!(n_bits < REG_BITS && n_bits <= br.bit_pos_);
  br.val_ >>= n_bits & REG_SHIFT_MASK;
  br.bit_pos_ -= n_bits;
}

// Clears the bits above bit_pos_.  Callers that move next_in other than by
// loading through val_ do it, since those bits belong to the byte at next_in.
#[inline(always)]
pub fn BrotliBitReaderClearUnclaimed(br: &mut BrotliBitReader) {
  if br.bit_pos_ < REG_BITS {
    br.val_ &= ((1 as reg_t) << br.bit_pos_) - 1;
  }
}

pub fn BrotliBitReaderUnload(br: &mut BrotliBitReader) {
  let unused_bytes: u32 = BrotliGetAvailableBits(br) >> 3;
  br.avail_in += unused_bytes;
  br.next_in -= unused_bytes;
  br.bit_pos_ -= unused_bytes << 3;
  BrotliBitReaderClearUnclaimed(br);
}

// Reads the specified number of bits from br and advances the bit pos.
// Precondition: accumulator MUST contain at least n_bits.
#[inline(always)]
pub fn BrotliTakeBits(br: &mut BrotliBitReader, n_bits: u32, val: &mut u32) {
  *val = (BrotliGetBitsUnmasked(br) as u32) & BitMask(n_bits);
  xprintln!("[BrotliReadBits]  {:?} {:?} {:?} val: {:x}\n",
           br.avail_in, br.bit_pos_, n_bits, *val);
  BrotliDropBits(br, n_bits);
}

// Reads the specified number of bits from br and advances the bit pos.
// Assumes that there is enough input to perform BrotliFillBitWindow.
//
// Callers pass a data-dependent n_bits (extra bits of lengths and distances),
// for which C's BrotliReadBits24 compiles BrotliFillBitWindow to its generic
// case alone: refill 32 bits once half the window is consumed, which leaves at
// least 33 bits for any n_bits up to 32. Choosing among the narrower n_bits
// cases at run time costs extra unpredictable branches.
#[inline(always)]
pub fn BrotliReadBits(br: &mut BrotliBitReader, n_bits: u32, input: &[u8]) -> u32 {
  if ::core::mem::size_of::<reg_t>() == 8 {
    let mut val: u32 = 0;
    BrotliFillBitWindow(br, 32, input);
    BrotliTakeBits(br, n_bits, &mut val);
    val
  } else if n_bits <= 16 {
    let mut val: u32 = 0;
    BrotliFillBitWindow(br, 16, input);
    BrotliTakeBits(br, n_bits, &mut val);
    val
  } else {
    cold_bit_reader_path();
    let mut low_val: u32 = 0;
    let mut high_val: u32 = 0;
    BrotliFillBitWindow(br, 16, input);
    BrotliTakeBits(br, 16, &mut low_val);
    BrotliFillBitWindow(br, 16, input);
    BrotliTakeBits(br, n_bits - 16, &mut high_val);
    low_val | (high_val << 16)
  }
}

// Reads the specified number of bits from br and advances the bit pos.
// Assumes that there is enough input to perform BrotliFillBitWindow.
#[allow (dead_code)]
pub fn BrotliReadConstantNBits(br: &mut BrotliBitReader, n_bits: u32, input: &[u8]) -> u32 {
  if ::core::mem::size_of::<reg_t>() == 8 || (n_bits <= 16) {
    let mut val: u32 = 0;
    BrotliFillBitWindow(br, n_bits, input);
    BrotliTakeBits(br, n_bits, &mut val);
    val
  } else {
    let mut low_val: u32 = 0;
    let mut high_val: u32 = 0;
    BrotliFillBitWindow(br, 16, input);
    BrotliTakeBits(br, 16, &mut low_val);
    // Refill 16 bits (not 8) so `n_bits` up to 32 (`n_bits - 16` up to 16) has
    // enough bits buffered on 32-bit targets.
    BrotliFillBitWindow(br, 16, input);
    BrotliTakeBits(br, n_bits - 16, &mut high_val);
    low_val | (high_val << 16)
  }
}

#[cold]
#[inline(never)]
fn BrotliSafeReadBits32Slow(br: &mut BrotliBitReader,
                            n_bits: u32,
                            val: &mut u32,
                            input: &[u8])
                            -> bool {
  let mut low_val: u32 = 0;
  let mut high_val: u32 = 0;
  let memento = BrotliBitReaderSaveState(br);
  if !BrotliSafeReadBits(br, 16, &mut low_val, input) ||
     !BrotliSafeReadBits(br, n_bits - 16, &mut high_val, input) {
    BrotliBitReaderRestoreState(br, &memento);
    return false;
  }
  *val = low_val | (high_val << 16);
  true
}

// Tries to read the specified amount of bits. Returns 0, if there is not
// enough input. n_bits MUST be positive.
#[inline(always)]
pub fn BrotliSafeReadBits(br: &mut BrotliBitReader,
                          n_bits: u32,
                          val: &mut u32,
                          input: &[u8])
                          -> bool {
  if ::core::mem::size_of::<reg_t>() < 8 && n_bits > 24 {
    return BrotliSafeReadBits32Slow(br, n_bits, val, input);
  }
  while BrotliGetAvailableBits(br) < n_bits {
    if !BrotliPullByte(br, input) {
      return false;
    }
  }
  BrotliTakeBits(br, n_bits, val);
  true
}

// Advances the bit reader position to the next byte boundary and verifies
// that any skipped bits are set to zero.
pub fn BrotliJumpToByteBoundary(br: &mut BrotliBitReader) -> bool {
  let pad_bits_count: u32 = BrotliGetAvailableBits(br) & 0x7;
  let mut pad_bits: u32 = 0;
  if pad_bits_count != 0 {
    BrotliTakeBits(br, pad_bits_count, &mut pad_bits);
  }
  pad_bits == 0
}

// Peeks a byte at specified offset.
// Precondition: bit reader is parked to a byte boundary.
// Returns -1 if operation is not feasible.
pub fn BrotliPeekByte(br: &mut BrotliBitReader, mut offset: u32, input: &[u8]) -> i32 {
  if !is_valid_bit_reader(br, input) {
    return -1;
  }
  let available_bits: u32 = BrotliGetAvailableBits(br);
  let bytes_left: u32 = (available_bits >> 3);
  if (available_bits & 7) != 0 {
    return -1;
  }
  if offset < bytes_left {
    return ((BrotliGetBitsUnmasked(br) >> ((offset << 3)) as u32) & 0xFF) as i32;
  }
  offset -= bytes_left;
  if offset < br.avail_in {
    let input_index = u64::from(br.next_in) + u64::from(offset);
    return match input.get(input_index as usize) {
      Some(value) => *value as i32,
      None => -1,
    };
  }
  -1
}

// Copies remaining input bytes stored in the bit reader to the output. Value
// num may not be larger than BrotliGetRemainingBytes. The bit reader must be
// warmed up again after this.
pub fn BrotliCopyBytes(dest: &mut [u8], br: &mut BrotliBitReader, mut num: u32, input: &[u8]) {
  let mut offset: u32 = 0;
  while BrotliGetAvailableBits(br) >= 8 && num > 0 {
    fast_mut!((dest)[offset as usize]) = BrotliGetBitsUnmasked(br) as u8;
    BrotliDropBits(br, 8);
    offset += 1;
    num -= 1;
  }
  // One bulk copy of the same bytes the per-byte loop used to copy.
  let dest_start = offset as usize;
  let input_start = br.next_in as usize;
  fast_mut!((dest)[dest_start ; dest_start + num as usize])
    .clone_from_slice(fast!((input)[input_start ; input_start + num as usize]));
  br.avail_in -= num;
  br.next_in += num;
  BrotliBitReaderClearUnclaimed(br);
}

pub fn BrotliInitBitReader(br: &mut BrotliBitReader) {
  br.val_ = 0;
  br.bit_pos_ = 0;
}

pub fn BrotliWarmupBitReader(br: &mut BrotliBitReader, input: &[u8]) -> bool {
  if BrotliGetAvailableBits(br) == 0 && !BrotliPullByte(br, input) {
    return false;
  }
  true
}

// Register-friendly view over `(BrotliBitReader, &[u8])` for the non-streaming
// hot loops (`ProcessCommands` and `ReadSymbolCodeLengths`): holds the
// remaining input slice directly instead of separate `(input, next_in,
// avail_in)` cursors, so refills need no index bounds checks. The bits have
// the reader's layout, unread bits right-aligned with the count in
// `avail_bits`, and bits above `avail_bits` may hold the bytes that follow,
// which a refill writes over with the same values.
#[derive(Copy, Clone)]
pub(crate) struct BitReaderSlice<'a> {
  pub val: reg_t,
  pub avail_bits: u32,
  pub rem: &'a [u8],
}

// Shift mask for `val`: `& REG_SHIFT_MASK` keeps every shift in range
// without changing any valid shift amount.
pub(crate) const SLICE_SHIFT_MASK: u32 = REG_SHIFT_MASK;

impl<'a> BitReaderSlice<'a> {
  #[inline(always)]
  pub fn new(br: &BrotliBitReader, input: &'a [u8]) -> Option<Self> {
    let rem = input
      .get(br.next_in as usize..)
      .and_then(|s| s.get(..br.avail_in as usize))?;
    Some(BitReaderSlice {
      val: br.val_,
      avail_bits: br.bit_pos_,
      rem: rem,
    })
  }

  #[inline(always)]
  pub fn empty() -> Self {
    BitReaderSlice {
      val: 0,
      avail_bits: 0,
      rem: &[],
    }
  }

  #[inline(always)]
  pub fn sync_to(self, br: &mut BrotliBitReader) {
    br.val_ = self.val;
    br.bit_pos_ = self.avail_bits;
    let avail_in = self.rem.len() as u32;
    let consumed = br.avail_in - avail_in;
    br.next_in += consumed;
    br.avail_in = avail_in;
  }

  #[inline(always)]
  pub fn has_input(&self) -> bool {
    self.rem.len() >= 28
  }

  #[inline(always)]
  pub fn avail_in(&self) -> u32 {
    self.rem.len() as u32
  }

  // Guarantees at least 16 valid bits (17 when any were left), as long as at
  // least 8 (64-bit) or 2 (32-bit) bytes of input remain.
  #[cfg(target_pointer_width = "64")]
  #[inline(always)]
  pub fn fill16(&mut self) {
    if self.avail_bits <= 16 {
      if let &[b0, b1, b2, b3, b4, b5, b6, b7, ..] = self.rem {
        let next = u64::from_le_bytes([b0, b1, b2, b3, b4, b5, b6, b7]);
        self.rem = &self.rem[6..];
        self.val |= next << (self.avail_bits & 63);
        self.avail_bits += 48;
      } else {
        debug_assert!(false, "BitReaderSlice::fill16 past the end of the input");
      }
    }
  }

  // On 32-bit targets `val` is a single register: refill 16 bits with one
  // halfword load (`ldrh`) once at most 16 bits are left, instead of the
  // 64-bit layout's two-register 8-byte load and 64-bit variable shift.
  #[cfg(not(target_pointer_width = "64"))]
  #[inline(always)]
  pub fn fill16(&mut self) {
    use core::convert::TryInto;
    if self.avail_bits <= 16 {
      if let Some(bytes) = self.rem.get(..2).and_then(|s| s.try_into().ok()) {
        self.rem = &self.rem[2..];
        self.val |= (u16::from_le_bytes(bytes) as reg_t) << (self.avail_bits & REG_SHIFT_MASK);
        self.avail_bits += 16;
      } else {
        debug_assert!(false, "BitReaderSlice::fill16 past the end of the input");
      }
    }
  }

  // Same as `BrotliFillBitWindow16`: guarantees at least 16 bits while reading
  // at most `BROTLI_SHORT_FILL_BIT_WINDOW_READ` bytes (4 on 64-bit, 2 on
  // 32-bit).
  #[inline(always)]
  pub fn fill_short(&mut self) {
    #[cfg(target_pointer_width = "64")]
    {
      self.fill32();
    }
    #[cfg(not(target_pointer_width = "64"))]
    {
      self.fill16();
    }
  }

  // Separating the `< 8` failure exit from the hot `avail_bits > 16`
  // fallthrough prevents SimplifyCFG from fusing `rem.len() >= 8` into every
  // non-refill literal iteration.
  #[cfg(target_pointer_width = "64")]
  #[inline(always)]
  pub fn fill16_checked(&mut self) -> bool {
    #[cold]
    #[inline(always)]
    fn cold_underflow() {}

    if self.avail_bits <= 16 {
      let next = match *self.rem {
        [b0, b1, b2, b3, b4, b5, b6, b7, ..] => u64::from_le_bytes([b0, b1, b2, b3, b4, b5, b6, b7]),
        _ => {
          cold_underflow();
          return false;
        }
      };
      self.rem = &self.rem[6..];
      self.val |= next << (self.avail_bits & 63);
      self.avail_bits += 48;
    }
    true
  }

  #[cfg(not(target_pointer_width = "64"))]
  #[inline(always)]
  pub fn fill16_checked(&mut self) -> bool {
    use core::convert::TryInto;
    #[cold]
    #[inline(always)]
    fn cold_underflow() {}

    if self.avail_bits <= 16 {
      let bytes: [u8; 2] = match self.rem.get(..2).and_then(|s| s.try_into().ok()) {
        Some(bytes) => bytes,
        None => {
          cold_underflow();
          return false;
        }
      };
      self.rem = &self.rem[2..];
      self.val |= (u16::from_le_bytes(bytes) as reg_t) << (self.avail_bits & REG_SHIFT_MASK);
      self.avail_bits += 16;
    }
    true
  }

  #[cfg(target_pointer_width = "64")]
  #[inline(always)]
  pub fn fill32(&mut self) {
    if self.avail_bits <= 32 {
      if let &[b0, b1, b2, b3, ref rest @ ..] = self.rem {
        let next = u32::from_le_bytes([b0, b1, b2, b3]) as u64;
        self.rem = rest;
        self.val |= next << (self.avail_bits & 63);
        self.avail_bits += 32;
      } else {
        debug_assert!(false, "BitReaderSlice::fill32 past the end of the input");
      }
    }
  }

  #[inline(always)]
  pub fn drop_bits(&mut self, n_bits: u32) {
    debug_assert!(n_bits <= self.avail_bits);
    self.avail_bits = self.avail_bits.wrapping_sub(n_bits);
    self.val >>= n_bits & REG_SHIFT_MASK;
  }

  #[inline(always)]
  pub fn take_bits(&mut self, n_bits: u32) -> u32 {
    let out = (self.val as u32) & BitMask(n_bits);
    self.drop_bits(n_bits);
    out
  }

  #[cfg(target_pointer_width = "64")]
  #[inline(always)]
  pub fn read_bits(&mut self, n_bits: u32) -> u32 {
    self.fill32();
    self.take_bits(n_bits)
  }

  // A 32-bit register holds at least 16 valid bits after a refill, so reads
  // wider than 16 bits (insert, copy and distance extra bits up to 24, or 30
  // with large windows) are split in two, as in `BrotliReadBits`.
  #[cfg(not(target_pointer_width = "64"))]
  #[inline(always)]
  pub fn read_bits(&mut self, n_bits: u32) -> u32 {
    self.fill16();
    if n_bits <= 16 {
      self.take_bits(n_bits)
    } else {
      cold_bit_reader_path();
      let low = self.take_bits(16);
      self.fill16();
      let high = self.take_bits(n_bits - 16);
      low | (high << 16)
    }
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  // The tests below were written for a reader that kept consumed bits in val_
  // and counted them in bit_pos_; these convert between the two layouts.
  pub(super) fn old_layout(val: reg_t, consumed: u32, avail_in: u32, next_in: u32) -> BrotliBitReader {
    BrotliBitReader {
      val_: if consumed >= REG_BITS { 0 } else { val >> consumed },
      bit_pos_: REG_BITS - consumed,
      avail_in: avail_in,
      next_in: next_in,
    }
  }

  #[cfg(target_pointer_width = "64")]
  fn consumed(br: &BrotliBitReader) -> u32 {
    REG_BITS - br.bit_pos_
  }

  #[cfg(target_pointer_width = "64")]
  fn valid_bits(br: &BrotliBitReader) -> reg_t {
    let mut copy = *br;
    BrotliBitReaderClearUnclaimed(&mut copy);
    copy.val_
  }

  #[test]
  fn bit_mask_matches_the_table() {
    // 32-bit targets support n <= 31 only; see `BitMask`.
    let max = if ::core::mem::size_of::<reg_t>() == 8 { 32 } else { 31 };
    for n in 0..max + 1 {
      assert_eq!(BitMask(n), kBitMask[n as usize]);
    }
  }

  #[test]
  fn input_range_checks_the_end_cursor() {
    // Exercise the range arithmetic without allocating a multi-GiB input.
    let max_cursor = u32::MAX as usize;
    assert!(is_valid_input_range(0, 0, 0));
    assert!(is_valid_input_range(3, 5, 8));
    assert!(!is_valid_input_range(3, 6, 8));
    assert!(!is_valid_input_range(9, 0, 8));
    assert!(is_valid_input_range(0, max_cursor, max_cursor));
    assert!(is_valid_input_range(max_cursor - 1, 1, max_cursor));
    assert!(is_valid_input_range(max_cursor, 0, max_cursor));
    assert!(!is_valid_input_range(max_cursor, 1, usize::MAX));
    assert!(!is_valid_input_range(max_cursor - 1, 2, usize::MAX));
    assert!(!is_valid_input_range(usize::MAX, 1, usize::MAX));
  }

  #[cfg(target_pointer_width="64")]
  #[test]
  fn a_large_input_slice_does_not_widen_the_cursor() {
    let input_len = 1usize << 32;
    assert!(is_valid_input_range(0, input_len - 1, input_len));
    assert!(!is_valid_input_range(input_len - 1, 1, input_len));
    assert!(!is_valid_input_range(1, input_len - 1, input_len));
    assert!(!is_valid_input_range(0, input_len, input_len));
    assert!(!is_valid_input_range(input_len, 0, input_len));
  }

  #[test]
  fn peek_byte_rejects_invalid_state() {
    let data: [u8; 1] = [0x7f];
    let mut unaligned_reader = old_layout(0, 1, 0, 0);
    assert_eq!(BrotliPeekByte(&mut unaligned_reader, 0, &data), -1);

    let mut out_of_bounds_reader = old_layout(0, REG_BITS, 1, 1);
    assert_eq!(BrotliPeekByte(&mut out_of_bounds_reader, 0, &data), -1);
  }

  // Hard-coded vectors for the 64-bit register; regression_tests.rs
  // checks both register widths against a bit-at-a-time oracle.
  #[cfg(target_pointer_width = "64")]
  #[test]
  fn warmup_works() {
    let data: [u8; 37] = [0xde, 0xad, 0xbe, 0xef /* 4 bytes garbage at beginning */, 0x21,
                          0xfd, 0xff, 0x87, 0x03, 0x6c, 0x63, 0x90, 0xde, 0xe9, 0x36, 0x04, 0xf8,
                          0x89, 0xb4, 0x4d, 0x57, 0x96, 0x51, 0x0a, 0x54, 0xb7, 0x54, 0xee, 0xf4,
                          0xdc, 0xa0, 0x05, 0x9b, 0xd2, 0x2b, 0x30, 0x8f]; // 33 b goods
    let mut bit_reader = old_layout(0x0, 64, 33, 4);
    let ret = BrotliWarmupBitReader(&mut bit_reader, &data[..]);
    assert_eq!(ret, true);
    assert_eq!(valid_bits(&bit_reader), 0x2100000000000000 >> 56);
    assert_eq!(consumed(&bit_reader), 56);
    assert_eq!(bit_reader.avail_in, 32);
    assert_eq!(bit_reader.next_in, 5);
  }
  // Hard-coded vectors for the 64-bit register; regression_tests.rs
  // checks both register widths against a bit-at-a-time oracle.
  #[cfg(target_pointer_width = "64")]
  #[test]
  fn warmup_errcheck() {
    let data: [u8; 3] = [0xde, 0xad, 0xeb]; // 3 bytes garbage at beginning
    let mut bit_reader = old_layout(0x86e884e1ffff577b, 64, 0, 3);
    let ret = BrotliWarmupBitReader(&mut bit_reader, &data[..]);
    assert_eq!(ret, false);
    assert_eq!(valid_bits(&bit_reader), 0);
    assert_eq!(consumed(&bit_reader), 64);
    assert_eq!(bit_reader.avail_in, 0);
    assert_eq!(bit_reader.next_in, 3);
  }
  // Hard-coded vectors for the 64-bit register; regression_tests.rs
  // checks both register widths against a bit-at-a-time oracle.
  #[cfg(target_pointer_width = "64")]
  #[test]
  fn safe_read_tests() {
    {
      let data: [u8; 12] = [0x50, 0x3b, 0xbb, 0x5e, 0xc5, 0x96, 0x81, 0xb7, 0x52, 0x89, 0xea, 0x3d];
      let mut bit_reader = old_layout(0xe1e56a736e04fbf5, 6, 12, 0);
      let mut val: u32 = 0;
      let ret = BrotliSafeReadBits(&mut bit_reader, 7, &mut val, &data[..]);
      assert_eq!(ret, true);
      assert_eq!(val, 0x6f);
      assert_eq!(bit_reader.avail_in, 12);
      assert_eq!(bit_reader.next_in, 0);
      assert_eq!(consumed(&bit_reader), 13);
    }
    {
      let data: [u8; 7] = [0xba, 0xd0, 0xf0, 0x0d, 0xc8, 0xcd, 0xcc];
      let mut bit_reader = old_layout(0x17f115ae26916f0, 57, 3, 4);
      let mut val: u32 = 0;
      let ret = BrotliSafeReadBits(&mut bit_reader, 15, &mut val, &data[..]);
      assert_eq!(ret, true);
      assert_eq!(val, 0x6400);
      assert_eq!(bit_reader.avail_in, 2);
      assert_eq!(bit_reader.next_in, 5);
      assert_eq!(consumed(&bit_reader), 64);
    }
    {
      let data: [u8; 4] = [0xee, 0xee, 0xf0, 0xd5];
      let mut bit_reader = old_layout(0x5f43f252c027e447, 53, 2, 2);
      let mut val: u32 = 0;
      let ret = BrotliSafeReadBits(&mut bit_reader, 14, &mut val, &data[..]);
      assert_eq!(ret, true);
      assert_eq!(bit_reader.avail_in, 1);
      assert_eq!(bit_reader.next_in, 3);
      assert_eq!(consumed(&bit_reader), 59);
      assert_eq!(valid_bits(&bit_reader), 0xf05f43f252c027e4 >> 59);
      assert_eq!(val, 0x2fa);
    }
    {
      let data: [u8; 4] = [0xee, 0xee, 0xf0, 0xd5];
      let mut bit_reader = old_layout(0x2f902339697460, 57, 0, 4);
      let mut val: u32 = 0x74eca3f0;
      let ret = BrotliSafeReadBits(&mut bit_reader, 14, &mut val, &data[..]);
      assert_eq!(ret, false);
      assert_eq!(bit_reader.avail_in, 0);
      assert_eq!(bit_reader.next_in, 4);
      assert_eq!(consumed(&bit_reader), 57);
      assert_eq!(valid_bits(&bit_reader), 0x2f902339697460 >> 57);
      assert_eq!(val, 0x74eca3f0);
    }
  }
  // Hard-coded vectors for the 64-bit register; regression_tests.rs
  // checks both register widths against a bit-at-a-time oracle.
  #[cfg(target_pointer_width = "64")]
  #[test]
  fn bit_read_tests() {
    {
      let data: [u8; 32] = [0xba, 0xaa, 0xad, 0xdd, 0x57, 0x5c, 0xd9, 0xa3, 0x3e, 0xb3, 0x77,
                            0xe7, 0xa0, 0x1e, 0x09, 0xd3, 0x12, 0xa1, 0x3f, 0xb8, 0x7e, 0x5a,
                            0x06, 0x86, 0xe5, 0x36, 0xef, 0x9c, 0x9f, 0x6d, 0x9b, 0xcc];
      let mut bit_reader = old_layout(0xf5917f07daaaeabb, 33, 29, 3);
      let ret = BrotliReadBits(&mut bit_reader, 8, &data[..]);
      assert_eq!(ret, 0x83);
      assert_eq!(consumed(&bit_reader), 9);
      assert_eq!(bit_reader.avail_in, 25);
      assert_eq!(bit_reader.next_in, 7);
    }
    {
      let data: [u8; 28] = [0xba, 0xaa, 0xaa, 0xad, 0x74, 0x40, 0x8e, 0xee, 0xd2, 0x38, 0xf1,
                            0xf4, 0xf8, 0x1d, 0x9f, 0x24, 0x48, 0x1e, 0x82, 0xce, 0x48, 0x88,
                            0xd7, 0x25, 0x74, 0xaf, 0xe3, 0xea];
      let mut bit_reader = old_layout(0x27e33b2440d3feaf, 18, 24, 4);
      let ret = BrotliReadBits(&mut bit_reader, 15, &data[..]);
      assert_eq!(ret, 0x1034);
      assert_eq!(consumed(&bit_reader), 33);
      assert_eq!(bit_reader.avail_in, 24);
      assert_eq!(bit_reader.next_in, 4);
    }
  }
  // Hard-coded vectors for the 64-bit register; regression_tests.rs
  // checks both register widths against a bit-at-a-time oracle.
  #[cfg(target_pointer_width = "64")]
  #[test]
  fn const_fill_bit_window_tests() {
    {
      let data: [u8; 36] = [0xff, 0x9a, 0xa0, 0xde, 0x50, 0x99, 0x67, 0x67, 0x69, 0x87, 0x0e,
                            0x69, 0xeb, 0x6a, 0xd1, 0x56, 0xc0, 0x32, 0x96, 0xed, 0x78, 0x0e,
                            0x19, 0xdd, 0x0b, 0xe8, 0xf8, 0x33, 0x9f, 0xe0, 0x69, 0x55, 0x59,
                            0x3f, 0x5d, 0xc8];
      let mut bit_reader = old_layout(0xb3fc441e0181dc4, 59, 33, 1);
      let ret = BrotliReadConstantNBits(&mut bit_reader, 4, &data[..]);
      assert_eq!(ret, 0x1);
      assert_eq!(consumed(&bit_reader), 7);
      assert_eq!(bit_reader.avail_in, 26);
      assert_eq!(bit_reader.next_in, 8);
    }
    {
      let data: [u8; 67] = [0xf0, 0x0d, 0x7e, 0x18, 0x70, 0x1c, 0x18, 0x57, 0xbd, 0x73, 0x47,
                            0xc1, 0xb4, 0xf7, 0xe2, 0xbe, 0x17, 0x6e, 0x26, 0x01, 0xb2, 0xd5,
                            0x55, 0xd8, 0x68, 0x1b, 0xc2, 0x87, 0xb4, 0xb1, 0xd9, 0x42, 0xac,
                            0x0d, 0x67, 0xb1, 0x93, 0x54, 0x49, 0xa4, 0x69, 0xf8, 0x16, 0x0e,
                            0x61, 0xb3, 0xdb, 0x98, 0xbb, 0xeb, 0xfa, 0xcb, 0x14, 0xcd, 0x68,
                            0x77, 0xa1, 0x33, 0x6c, 0x49, 0xfa, 0x35, 0xbb, 0xeb, 0xee, 0x7b, 0xae];
      let mut bit_reader = old_layout(0x655b1fe0dd6f1e78, 63, 65, 2);
      let ret = BrotliGet16BitsUnmasked(&mut bit_reader, &data[..]);
      assert_eq!(ret, 0x38e030fc);
      assert_eq!(consumed(&bit_reader), 15);
      assert_eq!(bit_reader.avail_in, 59);
      assert_eq!(bit_reader.next_in, 8);
    }
  }
}
