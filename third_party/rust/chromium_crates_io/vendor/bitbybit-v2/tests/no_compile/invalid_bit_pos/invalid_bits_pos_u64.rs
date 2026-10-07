use bitbybit::bitfield;

#[bitfield(u64, default = 0)]
struct Test {
    #[bits(63..=64, rw)]
    field: u2,
}

fn main() {}
