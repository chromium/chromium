use bitbybit::bitfield;

#[bitfield(u8, default = 0)]
struct Test {
    #[bits([0, 8], rw)]
    multi_bit_field: u2,
}

fn main() {}
