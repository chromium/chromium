use arbitrary_int::u4;
use bitbybit::bitfield;

#[bitfield(u32, default = 0)]
struct Test {
    #[bits(0..=8, rw)]
    invalid_range: u8,
}

#[bitfield(u8)]
struct Test {
    #[bits([0, 2, 4, 6], rw)]
    a: u4,
    #[bits([1, 3, 5, 7], rw)]
    b: u4,
    #[bit(4, rw)]
    c: bool,
}

#[bitfield(u8)]
struct Test2 {
    #[bits([0, 2, 4, 6], rw)]
    a: u4,
    #[bits([1, 2, 5, 7], rw)]
    b: u4,
}


fn main() {
    Test::builder().with_a(u4::new(1)).with_b(u4::new(1)).with_c(true).build();
    Test::builder().with_c(true).build();
    Test2::builder().with_a(u4::new(1)).with_b(u4::new(1)).build();
}
