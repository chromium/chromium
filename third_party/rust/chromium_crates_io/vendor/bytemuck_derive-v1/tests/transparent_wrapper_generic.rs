use bytemuck;

#[derive(Copy, Clone, bytemuck::NoUninit)]
#[repr(transparent)]
struct TransparentWrapper<T: bytemuck::NoUninit>(T);

#[derive(Copy, Clone, bytemuck::NoUninit)]
#[repr(packed(1))]
struct PackedWrapper<T: bytemuck::NoUninit> {
    value: T,
}

#[test]
fn test_nouninit_transparent_wrapper() {
    // This test just needs to compile to verify that NoUninit can be derived
    // for repr(transparent) generic types
    let w = TransparentWrapper(42u32);
    assert_eq!(w.0, 42);
}

#[test]
fn test_nouninit_packed_wrapper() {
    // This test just needs to compile to verify that NoUninit can be derived
    // for repr(packed(1)) generic types
    let w = PackedWrapper { value: 42u32 };
    // Read value to avoid unaligned reference warning
    let value = { w.value };
    assert_eq!(value, 42);
}
