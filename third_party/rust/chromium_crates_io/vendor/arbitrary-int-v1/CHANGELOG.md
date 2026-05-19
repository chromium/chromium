# Changelog

## arbitrary-int 1.3.0

### Added

- New optional feature `hint`, which tells the compiler that the returned `value()` can't exceed a maximum value. This
  allows the compiler to optimize faster code at the expense of unsafe code within arbitrary-int itself.
- Various new const constructors: `new_u8`, `new_u16`, ..., `new_u128` which allow creating an arbitrary int without
  type conversion, e.g. `u5::new_u32(i)` (where i is e.g. u32). This is shorter than writing
  `u5::new(i.try_into().unwrap())`,
  and combines two possible panic paths into one. Also, unlike `try_into().unwrap()`, the new constructors are usable in
  const contexts.
- For non-const contexts, `new_()` allows any Number argument to be passed through generics.
- `as_()` easily converts any Number to another. `as_u8()`, `as_u16()` for more control (and to implement the others).
- New optional feature `borsh` to support binary serialization using the borsh crate.

## arbitrary-int 1.2.7

### Added

- Support `Step` so that arbitrary-int can be used in a range expression, e.g.
  `for n in u3::MIN..=u3::MAX { println!("{n}") }`. Note this trait is currently unstable, and so is only usable in
  nightly. Enable this feature with `step_trait`.
- Support formatting via [defmt](https://crates.io/crates/defmt). Enable the option `defmt` feature
- Support serializing and deserializing via [serde](https://crates.io/crates/serde). Enable the option `serde` feature
- Support `Mul`, `MulAssign`, `Div`, `DivAssign`
- The following new methods were implemented to make arbitrary ints feel more like built-in types:
    * `wrapping_add`, `wrapping_sub`, `wrapping_mul`, `wrapping_div`, `wrapping_shl`, `wrapping_shr`
    * `saturating_add`, `saturating_sub`, `saturating_mul`, `saturating_div`, `saturating_pow`
    * `checked_add`, `checked_sub`, `checked_mul`, `checked_div`, `checked_shl`, `checked_shr`
    * `overflowing_add`, `overflowing_sub`, `overflowing_mul`, `overflowing_div`, `overflowing_shl`, `overflowing_shr`

### Changed

- In debug builds, `<<` (`Shl`, `ShlAssign`) and `>>` (`Shr`, `ShrAssign`) now bounds-check the shift amount using the
  same semantics as built-in shifts. For example, shifting a u5 by 5 or more bits will now panic as expected.

## arbitrary-int 1.2.6

### Added

- Support `LowerHex`, `UpperHex`, `Octal`, `Binary` so that arbitrary-int can be printed via e.g.
  `format!("{:x}", u4::new(12))`
- Support `Hash` so that arbitrary-int can be used in hash tables

### Changed

- As support for `[const_trait]` has recently been removed from structs like `From<T>` in upstream Rust, opting-in to
  the `nightly` feature no longer enables this behavior as that would break the build. To continue using this feature
  with older compiler versions, use `const_convert_and_const_trait_impl` instead.

## arbitrary-int 1.2.5

### Added

- Types that can be expressed as full bytes (e.g. u24, u48) have the following new methods:
    * `swap_bytes()`
    * `to_le_bytes()`
    * `to_be_bytes()`
    * `to_ne_bytes()`
    * `to_be()`
    * `to_le()`

### Changed

- `#[inline]` is specified in more places

### Fixed
