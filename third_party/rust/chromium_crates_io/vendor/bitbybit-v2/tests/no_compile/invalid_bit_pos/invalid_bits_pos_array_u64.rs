use bitbybit::bitfield;

#[bitfield(u64, default = 0)]
struct Test {
    #[bits([0, 63..=64], rw)]
    multi_bit_field: u3,
}

fn main() {}
