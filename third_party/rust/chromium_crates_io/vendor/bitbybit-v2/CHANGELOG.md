# Changelog

## bitbybit 2.0.1

Note: The 2.0.0 release on crates.io was accidentally published from a stale commit and is missing
some of the changes listed under 2.0.0 below (most notably, a builder was still provided for types
whose entire range wouldn't be covered). This release supersedes it; the full 2.0.0 changelog
applies as of 2.0.1.

### Fixed

- The builder now works for bitfields that have a field named `value`; internal names no longer
  clash with user field names. (@robamu)

## bitbybit 2.0.0

This version expects arbitrary-int 2.x.

### Added

- Support for signed arbitrary-int integers as field types, e.g. `i3`, `i24`, etc.
- Added `forbid_overlaps` attribute argument which checks and denies overlaps of `bitfield` fields. (@robamu)
- Array support for `debug` implementation. (@robamu)
- Builder `with_*` setter methods can now be called in any order. This is a breaking change if you
  rely on the builder's type signature not changing. The builder now has one boolean const parameter
  per field in the bitfield, instead of a single numeric const parameter. (@estebank)
- Builder is now available even when different fields have overlapping ranges, as long as there is
  a valid subset of fields that can be independently set to cover the whole range. (@estebank)

### Fixed

- `debug` and `defmt` implementations now also allow write-only fields. However, they are skipped
  with the exception of `defmt` bitfields. (@robamu)
- Use fully qualified trait syntax in `defmt_bitfields` macro implementation, avoiding the need
  for users to import the `arbitrary_int::traits::Integer` trait for auto-generated code. (@robamu)
- Introduced a check to deny `bitfield` configurations where non-array fields could have out of
  range offsets. (@robamu)
- Breaking change: bitenum's new_with_raw_value, when used with an arbitrary-int type would previously
  return the next larger whole primitive integer instead of the arbitrary-int, which was
  inconsistent. This is a breaking change.

```rs
// Old type
let foo: Result<MyEnum, u8> = MyEnum::new_with_raw_value(u2::new(0b10));

// New type (notice the u2 instead of u8)
let foo: Result<MyEnum, u2> = MyEnum::new_with_raw_value(u2::new(0b10));
```

- Generated functions in the builder now have a filled out docstring. (@estebank)
- Write-only fields in debug and defmt implementations are allowed. (@robamu)
- No longer allows out-of-bounds bit access when the underlying data type is an arbitrary-int
  (e.g. a `bitfield(u24)` no longer allows accessing bit 26). (@estebank)

## bitbybit 1.4.0

This is the final version to support arbitrary-int 1.x. Future versions will require arbitrary-int 2.x.

### Added

- `*_mask()`, as well as `*_BITS`, `*_COUNT`, and `*_STRIDE` constants for fields that provide some information on a
  field's structure. Enable with the `introspect` attribute on a struct, or globally with the `introspect` feature.
- Added `defmt` support by providing two bitfield macro arguments: `defmt_bitfields` and
  `defmt_fields` which generate `defmt` implementations for the bitfield.
  `defmt_bitfields` supports the efficient [bitfield](https://defmt.ferrous-systems.com/bitfields)
  feature provided by `defmt`, while the `defmt_fields` attribute simply forwards to the `defmt`
  implementations of the inner fields. These attribute macros arguments allow specifying a feature
  gate as well.
- Support for implicit `bitenum` discriminants.

### Fixed

- Allow qualified paths for `arbitrary_int` fields as well as (optional) `bitenum` fields.
- Fix the build for users that `#[deny(missing_docs)]`
- Moved LICENSE into the macro library's code so that it's available when distributed over crates.io

## bitbybit 1.3.3

### Added

- `ZERO` constant as a shorthand for `new_with_raw_value(0)` is now provided, even for bitfields without a default
  value.
- Introduces new `set_foo(&mut self, x)` methods as an alternative to `with_foo(&self, x)`

### Fixed

- `with_` methods in the builder now produce `///` documentation.

### Changed

- Bump to arbitrary_int 1.3.0

## bitbybit 1.3.2

### Fixed

- Fixed macro behavior when used within an IDE. The macro sees empty identifiers which don't happen
  during regular compilation. These are now ignored to allow proper autocomplete again.

## bitbybit 1.3.1

### Fixed

- Fixed a compilation error when non-contiguous ranges would produce a regular int (u8, u16, etc.).

## bitbybit 1.3.0

### Changed

- Support for non-contiguous bitranges
- Removed experimental_builder_syntax feature; this is now always enabled
- Switched default attribute argument syntax from field type to assignment type (colon field style
  is still allowed, but might be deprecated in the future):

```rs
#[bitenum(u2, exhaustive = true)]
enum ExhaustiveEnum {
    Zero = 0b00,
    One = 0b01,
    Two = 0b10,
    Three = 0b11,
}

#[bitfield(u64, default = 0)]
struct BitfieldWithEnum {
    #[bits(2..=3, rw)]
    e2: Option<NonExhaustiveEnum>,

    #[bits(0..=1, rw)]
    e1: ExhaustiveEnum,
}
```

## bitbybit 1.2.2

### Added

- Bitfields can support any arbitrary-int as a base-data-type, not just built-ins. For example, this
  is now supported:

```rs
#[bitfield(u12)]
struct Bitfield {
  // bits...
}
```

### Changed

### Fixed

- Multi-line doc-comments on fields are now fully put into the resulting accessors (previously, just
  the last line was)
- Masking of signed fields setters is now correct

## bitbybit 1.2.1

### Added

- Experimental new `builder()`...`build()` syntax, which allows setting all values without the risk
  of forgetting any. Requires opt-in via new `experimental_builder_syntax` feature
- Bump to [arbitrary-int](https://crates.io/crates/arbitrary-int) version 1.2.6

### Changed

### Fixed

- Accessors for array fields now assert that the index is within the size of the array.
- Most usage errors are now associated correctly to the line where they happen, instead of at the
  top of the declaration.

## bitbybit 1.2.0

### Added

### Changed

- `new()` has caused some confusion - it's a harmless way to create a default. In practice, this
  wasn't really clear and people thought the function might read e.g. from hardware. `new()` is now
  deprecated. `default()` (or `DEFAULT` in const contexts) take its place.

### Fixed

- Reserved identifiers like `r#enum` or `r#priv` can now be used for field names
