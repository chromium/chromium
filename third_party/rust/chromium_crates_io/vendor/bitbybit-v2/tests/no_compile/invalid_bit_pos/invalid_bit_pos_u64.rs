use bitbybit::bitfield;

#[bitfield(u64, default = 0)]
struct Test {
    #[bit(64, rw)]
    field: bool,
}

fn main() {}
