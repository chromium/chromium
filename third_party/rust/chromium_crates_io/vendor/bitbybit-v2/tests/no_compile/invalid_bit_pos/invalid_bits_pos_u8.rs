use bitbybit::bitfield;

#[bitfield(u8, default = 0)]
struct Test {
    #[bits(7..=8, rw)]
    field: u2,
}

fn main() {}
