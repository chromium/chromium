//! Provides the [`GeneralPurpose`] engine and associated config types.
//!
//! See preconfigured engines like [`STANDARD_NO_PAD`] or [`STANDARD_NO_PAD_INDIFFERENT`].
use crate::alphabet::Symbol;
use crate::{
    alphabet,
    alphabet::Alphabet,
    engine::{Config, DecodeMetadata, DecodePaddingMode},
    DecodeSliceError,
};
use core::convert::TryInto;

pub(crate) mod decode;
pub(crate) mod decode_suffix;

pub use decode::GeneralPurposeEstimate;

pub(crate) const INVALID_VALUE: u8 = 255;

/// A general-purpose base64 engine.
///
/// - It uses no vector CPU instructions, so it will work on any system. For a version that uses
///   SIMD where available, see the SIMD engines behind the `simd-unsafe` feature.
/// - It is reasonably fast (~2-3GiB/s).
/// - It is not constant-time, though, so it is vulnerable to timing side-channel attacks. For loading cryptographic keys, etc, it is suggested to use the forthcoming constant-time implementation.

#[derive(Debug, Clone)]
pub struct GeneralPurpose {
    encode_table: [u8; 64],
    decode_table: [u8; 256],
    pub(crate) padding: Symbol,
    config: GeneralPurposeConfig,
}

/// A purely scalar base64 engine that never uses hardware-specific vector instructions.
///
/// This is an alias for [`GeneralPurpose`], giving an explicit name for callers who want to
/// guarantee a scalar-only implementation.
pub type Scalar = GeneralPurpose;

impl GeneralPurpose {
    /// Create a `GeneralPurpose` engine from an [Alphabet].
    ///
    /// While not very expensive to initialize, ideally these should be cached
    /// if the engine will be used repeatedly.
    #[must_use]
    pub const fn new(alphabet: &Alphabet, config: GeneralPurposeConfig) -> Self {
        Self {
            encode_table: encode_table(alphabet),
            decode_table: decode_table(alphabet),
            padding: alphabet.padding,
            config,
        }
    }

    /// The 6-bit-index-to-ASCII encode table.
    #[cfg(all(
        feature = "simd-unsafe",
        any(
            target_arch = "x86_64",
            all(target_arch = "aarch64", target_feature = "neon")
        )
    ))]
    pub(crate) fn encode_table(&self) -> &[u8; 64] {
        &self.encode_table
    }

    /// The ASCII-to-6-bit-value decode table.
    #[cfg(all(
        feature = "simd-unsafe",
        any(
            target_arch = "x86_64",
            all(target_arch = "aarch64", target_feature = "neon")
        )
    ))]
    pub(crate) fn decode_table(&self) -> &[u8; 256] {
        &self.decode_table
    }
}

impl super::Engine for GeneralPurpose {
    type Config = GeneralPurposeConfig;
    type DecodeEstimate = GeneralPurposeEstimate;

    fn internal_encode(&self, input: &[u8], output: &mut [u8]) -> usize {
        encode_helper(&self.encode_table, input, output, |_, _| (0, 0))
    }

    fn internal_decoded_len_estimate(&self, input_len: usize) -> Self::DecodeEstimate {
        GeneralPurposeEstimate::new(input_len)
    }

    fn internal_decode(
        &self,
        input: &[u8],
        output: &mut [u8],
        estimate: Self::DecodeEstimate,
    ) -> Result<DecodeMetadata, DecodeSliceError> {
        decode::decode_helper(
            input,
            &estimate,
            output,
            &self.decode_table,
            self.config.decode_allow_trailing_bits,
            self.padding,
            self.config.decode_padding_mode,
            |_, _, _| (0, 0),
        )
    }

    fn config(&self) -> &Self::Config {
        &self.config
    }

    fn padding(&self) -> Symbol {
        self.padding
    }
}

/// Scalar base64 encode of `input` into `output`, returning the number of bytes written.
///
/// `simd_prefix` gets first crack at the input, returning `(input_consumed, output_written)`. It
/// must consume whole 3-byte groups: `input_consumed % 3 == 0`, `output_written == input_consumed /
/// 3 * 4`, both in bounds, and only those `output_written` bytes are written. `(0, 0)` (pure scalar)
/// is always valid.
#[inline]
pub(crate) fn encode_helper(
    encode_table: &[u8; 64],
    input: &[u8],
    output: &mut [u8],
    simd_prefix: impl FnOnce(&[u8], &mut [u8]) -> (usize, usize),
) -> usize {
    let (input_index, output_index) = simd_prefix(input, output);

    debug_assert!(
        input_index % 3 == 0,
        "prefix must consume whole 3-byte groups"
    );
    debug_assert!(
        output_index == input_index / 3 * 4,
        "prefix output must match consumed input"
    );
    debug_assert!(input_index <= input.len());
    debug_assert!(output_index <= output.len());

    encode_scalar_tail(encode_table, input, output, input_index, output_index)
}

/// Scalar encode of `input[input_index..]` into `output[output_index..]`, resuming from a 3-byte
/// group boundary. Returns the total number of output bytes written.
fn encode_scalar_tail(
    encode_table: &[u8; 64],
    input: &[u8],
    output: &mut [u8],
    mut input_index: usize,
    mut output_index: usize,
) -> usize {
    const BLOCKS_PER_FAST_LOOP: usize = 4;
    const LOW_SIX_BITS: u64 = 0x3F;

    // we read 8 bytes at a time (u64) but only actually consume 6 of those bytes. Thus, we need
    // 2 trailing bytes to be available to read..
    let last_fast_index = input.len().saturating_sub(BLOCKS_PER_FAST_LOOP * 6 + 2);

    if last_fast_index > 0 {
        while input_index <= last_fast_index {
            // Major performance wins from letting the optimizer do the bounds check once, mostly
            // on the output side
            let input_chunk = &input[input_index..(input_index + (BLOCKS_PER_FAST_LOOP * 6 + 2))];
            let output_chunk = &mut output[output_index..(output_index + BLOCKS_PER_FAST_LOOP * 8)];

            // Hand-unrolling for 32 vs 16 or 8 bytes produces yields performance about equivalent
            // to unsafe pointer code on a Xeon E5-1650v3. 64 byte unrolling was slightly better for
            // large inputs but significantly worse for 50-byte input, unsurprisingly. I suspect
            // that it's a not uncommon use case to encode smallish chunks of data (e.g. a 64-byte
            // SHA-512 digest), so it would be nice if that fit in the unrolled loop at least once.
            // Plus, single-digit percentage performance differences might well be quite different
            // on different hardware.

            let input_u64 = read_u64(&input_chunk[0..]);

            output_chunk[0] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[1] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[2] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[3] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[4] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[5] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[6] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[7] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = read_u64(&input_chunk[6..]);

            output_chunk[8] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[9] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[10] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[11] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[12] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[13] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[14] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[15] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = read_u64(&input_chunk[12..]);

            output_chunk[16] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[17] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[18] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[19] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[20] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[21] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[22] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[23] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            let input_u64 = read_u64(&input_chunk[18..]);

            output_chunk[24] = encode_table[((input_u64 >> 58) & LOW_SIX_BITS) as usize];
            output_chunk[25] = encode_table[((input_u64 >> 52) & LOW_SIX_BITS) as usize];
            output_chunk[26] = encode_table[((input_u64 >> 46) & LOW_SIX_BITS) as usize];
            output_chunk[27] = encode_table[((input_u64 >> 40) & LOW_SIX_BITS) as usize];
            output_chunk[28] = encode_table[((input_u64 >> 34) & LOW_SIX_BITS) as usize];
            output_chunk[29] = encode_table[((input_u64 >> 28) & LOW_SIX_BITS) as usize];
            output_chunk[30] = encode_table[((input_u64 >> 22) & LOW_SIX_BITS) as usize];
            output_chunk[31] = encode_table[((input_u64 >> 16) & LOW_SIX_BITS) as usize];

            output_index += BLOCKS_PER_FAST_LOOP * 8;
            input_index += BLOCKS_PER_FAST_LOOP * 6;
        }
    }

    // Encode what's left after the fast loop.

    const LOW_SIX_BITS_U8: u8 = 0x3F;

    let rem = input.len() % 3;
    let start_of_rem = input.len() - rem;

    // start at the first index not handled by fast loop, which may be 0.

    while input_index < start_of_rem {
        let input_chunk = &input[input_index..(input_index + 3)];
        let output_chunk = &mut output[output_index..(output_index + 4)];

        output_chunk[0] = encode_table[(input_chunk[0] >> 2) as usize];
        output_chunk[1] =
            encode_table[((input_chunk[0] << 4 | input_chunk[1] >> 4) & LOW_SIX_BITS_U8) as usize];
        output_chunk[2] =
            encode_table[((input_chunk[1] << 2 | input_chunk[2] >> 6) & LOW_SIX_BITS_U8) as usize];
        output_chunk[3] = encode_table[(input_chunk[2] & LOW_SIX_BITS_U8) as usize];

        input_index += 3;
        output_index += 4;
    }

    if rem == 2 {
        output[output_index] = encode_table[(input[start_of_rem] >> 2) as usize];
        output[output_index + 1] = encode_table[((input[start_of_rem] << 4
            | input[start_of_rem + 1] >> 4)
            & LOW_SIX_BITS_U8) as usize];
        output[output_index + 2] =
            encode_table[((input[start_of_rem + 1] << 2) & LOW_SIX_BITS_U8) as usize];
        output_index += 3;
    } else if rem == 1 {
        output[output_index] = encode_table[(input[start_of_rem] >> 2) as usize];
        output[output_index + 1] =
            encode_table[((input[start_of_rem] << 4) & LOW_SIX_BITS_U8) as usize];
        output_index += 2;
    }

    output_index
}

/// Returns a table mapping a 6-bit index to the ASCII byte encoding of the index
pub(crate) const fn encode_table(alphabet: &Alphabet) -> [u8; 64] {
    // the encode table is just the alphabet:
    // 6-bit index lookup -> printable byte
    let mut encode_table = [0_u8; 64];
    {
        let mut index = 0;
        while index < 64 {
            encode_table[index] = alphabet.symbols[index];
            index += 1;
        }
    }

    encode_table
}

/// Returns a table mapping base64 bytes as the lookup index to either:
/// - [`INVALID_VALUE`] for bytes that aren't members of the alphabet
/// - a byte whose lower 6 bits are the value that was encoded into the index byte
pub(crate) const fn decode_table(alphabet: &Alphabet) -> [u8; 256] {
    let mut decode_table = [INVALID_VALUE; 256];

    // Since the table is full of `INVALID_VALUE` already, we only need to overwrite
    // the parts that are valid.
    let mut index = 0_usize;
    while index < 64 {
        // The index in the alphabet is the 6-bit value we care about.
        // Since the index is in 0-63, it is safe to cast to u8.
        decode_table[alphabet.symbols[index] as usize] = index as u8;
        index += 1;
    }

    decode_table
}

#[inline]
fn read_u64(s: &[u8]) -> u64 {
    u64::from_be_bytes(s[..8].try_into().unwrap())
}

/// Contains configuration parameters for base64 encoding and decoding.
///
/// ```
/// # use base64::engine::GeneralPurposeConfig;
/// let config = GeneralPurposeConfig::new()
///     .with_encode_padding(false);
///     // further customize using `.with_*` methods as needed
/// ```
///
/// The constants [PAD] and [`NO_PAD`] cover most use cases.
///
/// To specify the characters used, see [Alphabet].
#[derive(Clone, Copy, Debug)]
pub struct GeneralPurposeConfig {
    encode_padding: bool,
    decode_allow_trailing_bits: bool,
    decode_padding_mode: DecodePaddingMode,
}

impl GeneralPurposeConfig {
    /// Create a new config with `padding` = `true`, `decode_allow_trailing_bits` = `false`, and
    /// `decode_padding_mode = DecodePaddingMode::RequireCanonicalPadding`.
    ///
    /// This probably matches most people's expectations, but consider disabling padding to save
    /// a few bytes unless you specifically need it for compatibility with some legacy system.
    #[must_use]
    pub const fn new() -> Self {
        Self {
            // RFC states that padding must be applied by default
            encode_padding: true,
            decode_allow_trailing_bits: false,
            decode_padding_mode: DecodePaddingMode::RequireCanonical,
        }
    }

    /// Create a new config based on `self` with an updated `padding` setting.
    ///
    /// If `padding` is `true`, encoding will append either 1 or 2 `=` padding characters as needed
    /// to produce an output whose length is a multiple of 4.
    ///
    /// Padding is not needed for correct decoding and only serves to waste bytes, but it's in the
    /// [spec](https://datatracker.ietf.org/doc/html/rfc4648#section-3.2).
    ///
    /// For new applications, consider not using padding if the decoders you're using don't require
    /// padding to be present.
    #[must_use]
    pub const fn with_encode_padding(self, padding: bool) -> Self {
        Self {
            encode_padding: padding,
            ..self
        }
    }

    /// Create a new config based on `self` with an updated `decode_allow_trailing_bits` setting.
    ///
    /// Most users will not need to configure this. It's useful if you need to decode base64
    /// produced by a buggy encoder that has bits set in the unused space on the last base64
    /// character as per [forgiving-base64 decode](https://infra.spec.whatwg.org/#forgiving-base64-decode).
    /// If invalid trailing bits are present and this is `true`, those bits will
    /// be silently ignored, else `DecodeError::InvalidLastSymbol` will be emitted.
    #[must_use]
    pub const fn with_decode_allow_trailing_bits(self, allow: bool) -> Self {
        Self {
            decode_allow_trailing_bits: allow,
            ..self
        }
    }

    /// Create a new config based on `self` with an updated `decode_padding_mode` setting.
    ///
    /// Padding is not useful in terms of representing encoded data -- it makes no difference to
    /// the decoder if padding is present or not, so if you have some un-padded input to decode, it
    /// is perfectly fine to use `DecodePaddingMode::Indifferent` to prevent errors from being
    /// emitted.
    ///
    /// However, since in practice
    /// [people who learned nothing from BER vs DER seem to expect base64 to have one canonical encoding](https://eprint.iacr.org/2022/361),
    /// the default setting is the stricter `DecodePaddingMode::RequireCanonicalPadding`.
    ///
    /// Or, if "canonical" in your circumstance means _no_ padding rather than padding to the
    /// next multiple of four, there's `DecodePaddingMode::RequireNoPadding`.
    #[must_use]
    pub const fn with_decode_padding_mode(self, mode: DecodePaddingMode) -> Self {
        Self {
            decode_padding_mode: mode,
            ..self
        }
    }
}

impl Default for GeneralPurposeConfig {
    /// Delegates to [`GeneralPurposeConfig::new`].
    fn default() -> Self {
        Self::new()
    }
}

impl Config for GeneralPurposeConfig {
    fn encode_padding(&self) -> bool {
        self.encode_padding
    }
}

#[cfg(all(
    feature = "simd-unsafe",
    any(
        target_arch = "x86_64",
        all(target_arch = "aarch64", target_feature = "neon")
    )
))]
impl GeneralPurposeConfig {
    /// Whether trailing bits are allowed when decoding.
    pub(crate) fn decode_allow_trailing_bits(&self) -> bool {
        self.decode_allow_trailing_bits
    }

    /// The decode padding mode.
    pub(crate) fn decode_padding_mode(&self) -> DecodePaddingMode {
        self.decode_padding_mode
    }
}

/// A [`GeneralPurpose`] engine using the [`alphabet::STANDARD`] base64 alphabet and [`PAD`] config.
///
/// Does not allow trailing bits when decoding.
pub const STANDARD: GeneralPurpose = GeneralPurpose::new(&alphabet::STANDARD, PAD);

/// A [`GeneralPurpose`] engine using the [`alphabet::STANDARD`] base64 alphabet and
/// [`PAD_INDIFFERENT`] config.
///
/// Does not allow trailing bits when decoding.
pub const STANDARD_PAD_INDIFFERENT: GeneralPurpose =
    GeneralPurpose::new(&alphabet::STANDARD, PAD_INDIFFERENT);

/// A [`GeneralPurpose`] engine using the [`alphabet::STANDARD`] base64 alphabet and [`NO_PAD`] config.
///
/// Does not allow trailing bits when decoding.
pub const STANDARD_NO_PAD: GeneralPurpose = GeneralPurpose::new(&alphabet::STANDARD, NO_PAD);

/// A [`GeneralPurpose`] engine using the [`alphabet::STANDARD`] base64 alphabet and
/// [`NO_PAD_INDIFFERENT`] config.
///
/// Does not allow trailing bits when decoding.
pub const STANDARD_NO_PAD_INDIFFERENT: GeneralPurpose =
    GeneralPurpose::new(&alphabet::STANDARD, NO_PAD_INDIFFERENT);

/// A [`GeneralPurpose`] engine using the [`alphabet::URL_SAFE`] base64 alphabet and [`PAD`] config.
///
/// Does not allow trailing bits when decoding.
pub const URL_SAFE: GeneralPurpose = GeneralPurpose::new(&alphabet::URL_SAFE, PAD);

/// A [`GeneralPurpose`] engine using the [`alphabet::URL_SAFE`] base64 alphabet and
/// [`PAD_INDIFFERENT`] config.
///
/// Does not allow trailing bits when decoding.
pub const URL_SAFE_PAD_INDIFFERENT: GeneralPurpose =
    GeneralPurpose::new(&alphabet::URL_SAFE, PAD_INDIFFERENT);

/// A [`GeneralPurpose`] engine using the [`alphabet::URL_SAFE`] base64 alphabet and [`NO_PAD`] config.
///
/// Does not allow trailing bits when decoding.
pub const URL_SAFE_NO_PAD: GeneralPurpose = GeneralPurpose::new(&alphabet::URL_SAFE, NO_PAD);

/// A [`GeneralPurpose`] engine using the [`alphabet::URL_SAFE`] base64 alphabet and
/// [`NO_PAD_INDIFFERENT`] config.
///
/// Does not allow trailing bits when decoding.
pub const URL_SAFE_NO_PAD_INDIFFERENT: GeneralPurpose =
    GeneralPurpose::new(&alphabet::URL_SAFE, NO_PAD_INDIFFERENT);

/// Include padding bytes when encoding, and require that they be present when decoding.
///
/// Does not allow trailing bits when decoding.
///
/// This is the standard per the base64 RFC, but consider using [`NO_PAD`] or [`NO_PAD_INDIFFERENT`]
/// instead as padding serves little purpose in practice.
pub const PAD: GeneralPurposeConfig = GeneralPurposeConfig::new();

/// Include padding bytes when encoding, but allow input with or without padding when decoding.
///
/// Does not allow trailing bits when decoding.
pub const PAD_INDIFFERENT: GeneralPurposeConfig = GeneralPurposeConfig::new()
    .with_encode_padding(true)
    .with_decode_padding_mode(DecodePaddingMode::Indifferent);

/// Don't add padding when encoding, and require that there is no padding when decoding.
///
/// Does not allow trailing bits when decoding.
pub const NO_PAD: GeneralPurposeConfig = GeneralPurposeConfig::new()
    .with_encode_padding(false)
    .with_decode_padding_mode(DecodePaddingMode::RequireNone);

/// Don't add padding when encoding, and allow input with or without padding when decoding.
///
/// Does not allow trailing bits when decoding.
pub const NO_PAD_INDIFFERENT: GeneralPurposeConfig = GeneralPurposeConfig::new()
    .with_encode_padding(false)
    .with_decode_padding_mode(DecodePaddingMode::Indifferent);
