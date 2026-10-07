use bitbybit::bitfield;

#[bitfield(u64, default = 0)]
struct Test {
    #[bits([0, 64], rw)]
    multi_bit_field: u2,
}

fn main() {}
