# Changelog

## 2.1.1

### Added

- Added optional `bin-proto` support. Enable using the `bin-proto` feature (thanks wojciech-graj)

### Fixed

Fixed two bugs in signed-to-unsigned conversions:

- `_from()` on unsigned integers previously allowed negative values as inputs, which would produce
  valid unsigned integers.
- `masked_new()` on unsigned integers previously didn't correctly mask the input value, which would produce
  invalid unsigned integers.

## 2.1.0

### Added

- Added Rust MSRV v1.83
- Bumped `defmt` dependency to v1
- Implement [bytemuck] traits for `UInt` and `Int`.

[bytemuck]: https://github.com/Lokathor/bytemuck

- Removed compatibility for `const_convert_and_const_trait_impl`, which required an ancient nightly compiler.
- Implement `SaturatingAdd` and `SaturatingSub` traits from `num-traits` for `Uint` and `Int`.

### Fixed

- Fixed incorrect behavior in conversions between signed and unsigned integers of equal bit-widths.

## arbitrary-int 2.0.0

### Added

- New types for signed integers: `i1`, ..., `i127`.
- The old Number trait is now replaced with three traits: UnsignedInteger (equivalent to the old Number), SignedInteger
  and Integer (which can be either signed or unsigned).
- prelude: `use arbitrary-int::prelude::*` to get everything (except for the deprecated Number trait).
- Various new extract functions: `extract_i8`, `extract_i16`, ..., `extract_i128`. These are the same as the
  equivalent `extract_u<N>` functions, but work with signed integers instead.
- Add `quickcheck` and `arbitrary` support
- Support `core::iter::Sum`: `[u7::new(1); 10].iter().sum::<u7>() == u7::new(10)`
- Support `core::iter::Product`: `[i7::new(2); 4].iter().product::<i7>() == i7::new(16)`
- `Integer`, `SignedInteger` and `UnsignedInteger` now themselves implement various numeric traits such as `Add`,
  `BitAnd` etc. This both helps simplify the code but allows client code to operate on regular and arbitrary integers in
  a more generic way.

### Fixed

- `leading_zeros` and `trailing_zeros` now report the correct number of bits when a value of `MIN` is passed.
- The implementation of `BorshSerialize` and `BorshDeserialize` now correctly handle writers/readers that can only
  partially write/read all data after a single call to `borsh::io::Write::write()`/`borsh::io::Read::read()`.
  This can happen if for example an `TcpStream` is waiting on the other end to send more data. The value would
  previously be truncated, now it blocks until enough data is available.

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
