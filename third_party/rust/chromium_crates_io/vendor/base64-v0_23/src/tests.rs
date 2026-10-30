use std::str;

use rand::{
    distr,
    distr::{Distribution as _, Uniform},
    rngs, Rng, RngExt,
};

use crate::alphabet::{is_valid_b64_symbol, Symbol};
use crate::{
    alphabet,
    encode::encoded_len,
    engine::{
        general_purpose::{GeneralPurpose, GeneralPurposeConfig},
        Config, DecodePaddingMode, Engine,
    },
};

#[test]
fn roundtrip_random_config_short() {
    // exercise the slower encode/decode routines that operate on shorter buffers more vigorously
    roundtrip_random_config(Uniform::new(0, 50).unwrap(), 10_000);
}

#[test]
fn roundtrip_random_config_long() {
    roundtrip_random_config(Uniform::new(0, 1000).unwrap(), 10_000);
}

pub fn assert_encode_sanity(encoded: &str, engine: &impl Engine, input_len: usize) {
    let expect_padding = engine.config().encode_padding();
    let padding_symbol = engine.padding();

    assert_encode_sanity_core(encoded, expect_padding, padding_symbol, input_len)
}

/// [`assert_encode_sanity`] when you want separate padding config & padding symbol.
pub fn assert_encode_sanity_core(
    encoded: &str,
    expect_padding: bool,
    padding_symbol: Symbol,
    input_len: usize,
) {
    let input_rem = input_len % 3;

    let expected_padding_len = if input_rem > 0 {
        if expect_padding {
            3 - input_rem
        } else {
            0
        }
    } else {
        0
    };

    let expected_encoded_len = encoded_len(input_len, expect_padding).unwrap();

    assert_eq!(expected_encoded_len, encoded.len());

    let padding_len = encoded
        .bytes()
        .filter(|&b| b == padding_symbol.as_u8())
        .count();

    assert_eq!(expected_padding_len, padding_len);

    let _ = str::from_utf8(encoded.as_bytes()).expect("Base64 should be valid utf8");
}

fn roundtrip_random_config(input_len_range: Uniform<usize>, iterations: u32) {
    let mut input_buf: Vec<u8> = Vec::new();
    let mut encoded_buf = String::new();
    let mut rng = rand::make_rng::<rngs::SmallRng>();

    for _ in 0..iterations {
        input_buf.clear();
        encoded_buf.clear();

        let input_len = input_len_range.sample(&mut rng);

        let engine = random_engine(&mut rng);

        for _ in 0..input_len {
            input_buf.push(rng.random());
        }

        engine.encode_string(&input_buf, &mut encoded_buf);

        assert_encode_sanity(&encoded_buf, &engine, input_len);

        assert_eq!(input_buf, engine.decode(&encoded_buf).unwrap());
    }
}

pub fn random_config<R: Rng>(rng: &mut R) -> GeneralPurposeConfig {
    let mode = rng.random();
    GeneralPurposeConfig::new()
        .with_encode_padding(match mode {
            DecodePaddingMode::Indifferent => rng.random(),
            DecodePaddingMode::RequireCanonical => true,
            DecodePaddingMode::RequireNone => false,
        })
        .with_decode_padding_mode(mode)
        .with_decode_allow_trailing_bits(rng.random())
}

impl distr::Distribution<DecodePaddingMode> for distr::StandardUniform {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> DecodePaddingMode {
        match rng.random_range(0..=2) {
            0 => DecodePaddingMode::Indifferent,
            1 => DecodePaddingMode::RequireCanonical,
            _ => DecodePaddingMode::RequireNone,
        }
    }
}

pub fn random_alphabet<R: Rng>(rng: &mut R) -> alphabet::Alphabet {
    // 65 symbols for alphabet + padding
    let mut symbols = Vec::with_capacity(65);
    while symbols.len() < 65 {
        let t = rng.random();
        if is_valid_b64_symbol(t) && !symbols.contains(&t) {
            symbols.push(t);
        }
    }

    alphabet::Alphabet::new_with_padding(
        str::from_utf8(&symbols[..64]).unwrap(),
        Symbol::new(symbols[64]).unwrap(),
    )
    .unwrap()
}

pub fn random_engine<R: Rng>(rng: &mut R) -> GeneralPurpose {
    let alphabet = random_alphabet(rng);
    let config = random_config(rng);
    GeneralPurpose::new(&alphabet, config)
}
