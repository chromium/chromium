# Changelog

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
