use bitbybit::bitfield;

#[bitfield(u16, default = 0)]
struct Test {
    #[bit(16, rw)]
    field: bool,
}

fn main() {}
