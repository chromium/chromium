use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bits([0, 31..=32], rw)]
    multi_bit_field: u3,
}

fn main() {}
