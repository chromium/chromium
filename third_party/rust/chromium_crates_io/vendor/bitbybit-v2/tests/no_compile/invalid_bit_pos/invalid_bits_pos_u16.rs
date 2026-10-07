use bitbybit::bitfield;

#[bitfield(u16, default = 0)]
struct Test {
    #[bits(15..=16, rw)]
    field: u2,
}

fn main() {}
