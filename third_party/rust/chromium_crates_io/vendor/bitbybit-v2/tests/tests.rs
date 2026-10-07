#[test]
fn all_tests() {
    let t = trybuild::TestCases::new();

    // tests that pass
    t.pass("tests/basic.rs");
    t.pass("tests/with_fields.rs");
    t.pass("tests/correct.rs");
    t.pass("tests/builder_value_field.rs");

    t.compile_fail("tests/no_compile/exhaustive_bitenum.rs");
    t.compile_fail("tests/no_compile/non_exhaustive_bitenum.rs");
    t.compile_fail("tests/no_compile/invalid_field_range.rs");
    t.compile_fail("tests/no_compile/invalid_field_access.rs");

    t.compile_fail("tests/no_compile/overlapping_bitfield_bits.rs");
    t.compile_fail("tests/no_compile/overlapping_bitfield_u8_fields.rs");
    t.compile_fail("tests/no_compile/overlapping_bitfield_arrays.rs");
    t.compile_fail("tests/no_compile/overlapping_bitfield_arrays_with_stride.rs");

    t.compile_fail("tests/no_compile/invalid_bit_pos/*.rs");

    t.compile_fail("tests/no_compile/missing_fields_in_builder.rs");
}
