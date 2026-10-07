use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bit(32, rw)]
    field: bool,
}

fn main() {}
