use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bits(0..=8, ro)]
    invalid_access: u8,
}

fn main() {}
