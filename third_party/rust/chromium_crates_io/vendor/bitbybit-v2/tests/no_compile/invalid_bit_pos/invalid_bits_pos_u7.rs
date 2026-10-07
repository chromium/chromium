use bitbybit::bitfield;

#[bitfield(u7, default = 0)]
struct Test {
    #[bits(6..=7, rw)]
    field: u2,
}

fn main() {}
