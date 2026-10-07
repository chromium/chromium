use bitbybit::bitfield;

#[bitfield(u7, default = 0)]
struct Test {
    #[bit(7, rw)]
    field: bool,
}

fn main() {}
