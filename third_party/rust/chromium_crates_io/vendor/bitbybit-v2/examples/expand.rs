//! The primary purpose of this example is to provide a file which can be expanded and used
//! to debug macro additions and maintenance.
//!
//! You can expand the example macro implementation by using `cargo expand --example expand`
use arbitrary_int::u4;
use bitbybit::{bitenum, bitfield};

#[bitenum(u2, exhaustive = true)]
pub enum BitenumU2 {
    Field0 = 0b00,
    Field1 = 0b01,
    Field2 = 0b10,
    Field3 = 0b11,
}

#[bitfield(u32, debug)]
pub struct BitfieldU32 {
    #[bits(28..=31, rw)]
    val3: u4,
    #[bits(24..=27, rw)]
    val2: u4,
    #[bits(16..=23, rw)]
    val1: u8,
    #[bits(0..=15, rw)]
    val0: u16,
}

pub fn main() {}
