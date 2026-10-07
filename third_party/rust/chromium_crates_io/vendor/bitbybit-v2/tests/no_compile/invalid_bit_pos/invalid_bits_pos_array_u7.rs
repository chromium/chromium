use bitbybit::bitfield;

#[bitfield(u7, default = 0)]
struct Test {
    #[bits([0, 6..=7], rw)]
    multi_bit_field: u3,
}

fn main() {}
