mod decode_signed;
mod decode_unsigned;
mod encode_signed;
mod encode_unsigned;

pub use self::{
    decode_signed::{
        varint_decode_i128, varint_decode_i16, varint_decode_i32, varint_decode_i64,
        varint_decode_isize,
    },
    decode_unsigned::{
        varint_decode_u128, varint_decode_u16, varint_decode_u32, varint_decode_u64,
        varint_decode_usize,
    },
    encode_signed::{
        varint_encode_i128, varint_encode_i16, varint_encode_i32, varint_encode_i64,
        varint_encode_isize,
    },
    encode_unsigned::{
        varint_encode_u128, varint_encode_u16, varint_encode_u32, varint_encode_u64,
        varint_encode_usize,
    },
};

const SINGLE_BYTE_MAX: u8 = 250;
const U16_BYTE: u8 = 251;
const U32_BYTE: u8 = 252;
const U64_BYTE: u8 = 253;
const U128_BYTE: u8 = 254;
