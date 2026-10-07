use bitbybit::bitfield;

#[bitfield(u7, default = 0)]
struct Test {
    #[bits([0, 7], rw)]
    multi_bit_field: u2,
}

fn main() {}
