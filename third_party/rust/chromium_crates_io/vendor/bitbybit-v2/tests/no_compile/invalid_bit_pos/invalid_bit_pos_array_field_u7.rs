use bitbybit::bitfield;

#[bitfield(u7, default = 0)]
struct Test {
    #[bit(0, rw)]
    array_field: [bool; 8],
}

fn main() {}
