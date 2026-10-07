use bitbybit::bitfield;

#[bitfield(u32, default = 0, forbid_overlaps)]
struct Test {
    #[bit(0, rw)]
    overlapping_bit: bool,
    #[bit(0, rw)]
    bit: bool
}

#[bitfield(u32)]
pub struct TestNonExhaustiveOverlap {
    #[bits(0..=15, rw)]
    a: u16,

    #[bits(0..=15, rw)]
    b: u16,

    #[bits(16..=31, rw)]
    c: u16,
}

#[bitfield(u16)]
pub struct TestExhaustiveOverlap {
    #[bits(0..=15, rw)]
    a: u16,

    #[bits(0..=15, rw)]
    b: u16,
}

fn main() {
    // A struct with overlapping fields that doesn't cover the
    // whole bit range doesn't have `build()` available.
    let _ = TestNonExhaustiveOverlap::builder()
        .with_a(0)
        .build();
    let _ = TestNonExhaustiveOverlap::builder()
        .with_b(0)
        .build();
    // A struct with overlapping fields can't set the same range twice.
    let _ = TestNonExhaustiveOverlap::builder()
        .with_a(0)
        .with_b(0)
        .build();
    let _ = TestExhaustiveOverlap::builder()
        .with_a(0)
        .with_b(0)
        .build();
    // Fields must be set for the builder to be available.
    let _ = TestExhaustiveOverlap::builder()
        .build();
}
