# Release 0.2.3 (2024-02-19)

## Fixes

 - Fixed an integer overflow that could lead to the out-of-place transpose function accepting invalid and unsafe inputs (#11)
 - Documentation corrections (#7)

# Release 0.2.2 (2022-11-07)

## Fixes

 - Added missing license files
 - Upgraded `criterion` dependency from 0.2 to 0.3

# Release 0.2.1 (2020-03-30)

## Improvements

 - Significantly improved the performance of the out-of-place transpose
 - Removed depenendence on `std` in the `num_integer` dependency.

# Release 0.2.0 (2019-01-04)

## Features

 - Implemented an in-place transpose.

### Breaking Changes

 - Documented minimum rust version to be 1.26

# Release 0.1.0 (2019-01-01)

 - Initial release. Support for an out-of-place transpose.
