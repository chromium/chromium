use bitbybit::bitfield;

#[bitfield(u16, default = 0)]
struct Test {
    #[bits([0, 16], rw)]
    multi_bit_field: u2,
}

fn main() {}
