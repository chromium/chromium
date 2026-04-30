# Release 0.2.4 (2022-11-07)

### Fixes

- Fixed broken MIT license file link
- Added missing license files to repository
- Upgraded dependencies to proptest 1.0.0, num-bigint 0.4, rand 0.8
- Typo fixes in docs

# Release 0.2.3 (2019-12-27)

### Fixes

- Fixed a compile error on rustc 1.26
- Added `StrengthReducedU128`

# Release 0.2.2 (2019-06-12)

### Changes

- Rewrote the strength reduction algorithm to require less code and less branching.
- Significantly reduced the setup cost of StrengthReducedU64. It now breaks even at 3-4 divisions, instead of 30-40.

# Release 0.2.1 (2019-01-04)

### Fixes

- Fixed a class of bugs for certain divisors with very large numerators, where the returned quotient was off by one.

# Release 0.2.0 (2019-01-03)

### Breaking Changes

- `strength_reduce` is now marked `#[!no_std]`

# Release 0.1.1 (2019-01-03)

 - Added the readme to cargo.tom, so that it can be rendered directly from crates.io

# Release 0.1.0 (2019-01-03)

 - Initial release. Support for computing strength-reduced division and modulo for unsigned integers:
 - `u8`: `StrengthReducedU8`
 - `u16`: `StrengthReducedU16`
 - `u32`: `StrengthReducedU32`
 - `u64`: `StrengthReducedU64`
 - `usize`: `StrengthReducedUsize`
