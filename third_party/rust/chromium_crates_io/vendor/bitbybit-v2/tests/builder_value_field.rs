use bitbybit::bitfield;

// No issues with the private internal field value when a field is named value.
#[bitfield(u32, default = 0x0, debug)]
pub struct Data {
    #[bit(15, rw)]
    dparity: bool,
    #[bits(0..=7, rw)]
    value: u8,
}

pub fn main() {}
