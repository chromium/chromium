use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bits(31..=32, rw)]
    field: u2,
}

fn main() {}
