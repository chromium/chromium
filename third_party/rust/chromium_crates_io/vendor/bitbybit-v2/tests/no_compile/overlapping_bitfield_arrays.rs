use bitbybit::bitfield;

#[bitfield(u32, default = 0, forbid_overlaps)]
struct Test {
    #[bits(8..=15, rw)]
    overlapping_bits: u8,
    #[bits(0..=7, rw)]
    u8_array: [u8; 2],
}

fn main() {}
