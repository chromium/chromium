use bitbybit::bitfield;
use arbitrary_int::{u2, u4, u6, u12};

#[bitfield(u32, default = 0, forbid_overlaps)]
struct Test {
    #[bits(7..=14, rw)]
    overlapping_bits: u8,
    #[bits(0..=7, rw)]
    lower_bits: u8,
}

#[bitfield(u16)]
struct TestSometimesUnconstructable {
    #[bits(12..=15, rw)]
    bit_upper: u4,
    #[bits(2..=9, rw)]
    middle_bits: u8,
    #[bits(0..=7, rw)]
    lower_bits: u8,
}
#[bitfield(u16)]
struct TestUnconstructable {
    #[bits(0..=7, rw)]
    bits: u8,
}

#[bitfield(u16)]
struct TestAllowed {
    #[bits(12..=15, rw)]
    upper: u4,
    #[bits(0..=11, rw)]
    lower: u12,
    #[bits(10..=15, rw)]
    c: u6,
    #[bits(2..=9, rw)]
    b: u8,
    #[bits(0..=1, rw)]
    a: u2,
}

fn main() {
    // correct usage
    TestAllowed::new_with_raw_value(0x1F1F);
    TestAllowed::builder().with_upper(u4::new(0)).with_lower(u12::new(0)).build();
    TestAllowed::builder().with_a(u2::new(0)).with_b(0).with_c(u6::new(0)).build();
    TestSometimesUnconstructable::builder().with_lower_bits(0).with_bit_upper(u4::new(0)).build();
    // can't set overlapping fields at the same time
    TestAllowed::builder().with_upper(u4::new(0)).with_lower(u12::new(0)).with_a(u2::new(0)).build();
    TestSometimesUnconstructable::builder().with_lower_bits(0).with_bit_upper(u4::new(0)).with_middle_bits(0).build();
    // missing mandatory fields
    TestAllowed::builder().with_upper(u4::new(0)).build();
    TestAllowed::builder().with_b(0).build();
    // check we don't mention build as existing
    TestSometimesUnconstructable::builder().with_lower_bits(0).build();
    TestSometimesUnconstructable::builder().with_middle_bits(0).build();
    TestUnconstructable::builder().with_bits(0).build();
}
