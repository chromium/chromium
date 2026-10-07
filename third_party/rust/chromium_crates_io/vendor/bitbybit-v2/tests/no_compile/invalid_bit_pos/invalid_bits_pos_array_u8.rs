use bitbybit::bitfield;

#[bitfield(u8, default = 0)]
struct Test {
    #[bits([0, 7..=8], rw)]
    multi_bit_field: u3,
}

fn main() {}
