use bitbybit::bitfield;

#[bitfield(u32, default = 0, forbid_overlaps)]
struct Test {
    #[bits(8..=15, rw)]
    foo: u8,
    #[bits(0..=7, rw)]
    bar: u8,
}

fn main() {
    Test::builder().with_foo(1).build();
    Test::builder().with_bar(1).build();
    Test::builder().with_bar(1).with_foo(1).build();
    Test::builder().with_bar(1).with_foo(1).with_bar(2).build();
}
