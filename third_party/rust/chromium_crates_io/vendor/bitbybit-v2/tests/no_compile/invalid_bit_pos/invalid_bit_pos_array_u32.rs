use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bits([0, 32], rw)]
    multi_bit_field: u2,
}

fn main() {}
