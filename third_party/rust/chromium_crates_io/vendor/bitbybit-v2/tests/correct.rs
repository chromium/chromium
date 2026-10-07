use bitbybit::bitfield;

#[bitfield(u32, default = 0, forbid_overlaps)]
#[derive(PartialEq, Eq, Debug)]
struct Test {
    #[bits(8..=15, rw)]
    foo: u8,
    #[bits(0..=7, r)]
    bar: u8,
}

fn main() {
    let val = Test::builder().with_foo(1).build();
    assert_eq!(val.raw_value, 256);
}
