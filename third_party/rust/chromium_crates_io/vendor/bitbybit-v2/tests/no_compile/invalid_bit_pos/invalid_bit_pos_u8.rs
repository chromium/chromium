use bitbybit::bitfield;

#[bitfield(u8, default = 0)]
struct Test {
    #[bit(8, rw)]
    field: bool,
}

fn main() {}
