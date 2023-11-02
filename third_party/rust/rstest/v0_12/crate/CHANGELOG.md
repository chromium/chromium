# Changelog

## [0.12.0] 2021/12/12

### Add

- Add `#[once]` fixture attribute to create static fixtures (See #119)

### Fixed

- Fixed check of available features before to enable macro diagnostic (See #126)

## [0.11.0] 2021/08/01

### Fixed

- use mutable fixture in in cases and value list (See #121)

## [0.10.0] 2021/05/16

### Add

- Rename fixture (See #107 and #108)  

### Fixed

- Wired behaviour in `#[fixture]` with generics types that have transitive
reference (See #116)  

## [0.9.0] 2021/05/2

### Add

- `#[future]` arg attribute to remove `impl Future<>` boilerplate. (See #98) 

## [0.8.0] 2021/4/25

### Add

- Magic Conversion: use literal string for define values where type implements 
`FromStr` trait (See #111)

### Changed

- `#[default]` arg attribute cannot use key = arbitrary rust expression syntax 
(is unstable https://github.com/rust-lang/rust/issues/78835). So we switched
to `#[default(expression)]` syntax. (See #117) 

### Fixed

- #117 introduced an unstable syntax

## [0.7.0] 2021/3/21

This version intruduce the new more composable syntax. And async 
fixtures (thanks to @rubdos)

### Add
- New syntax that leverage on function and argument attributes
to implement all features (See #99, #100, #101, #103, #109 and #102)
- `async` fixtures (See #86, #96. Thanks to @rubdos).

### Changed
- Moved integration tests resouces in `test` directory (See #97)

## [0.6.4] 2020/6/20

### Add
- Implemented reusable test list with `rstest_reuse` external crate (See #80)

## [0.6.3] 2020/4/19

### Add
- Define default values instead use trivial fixtures (See #72).

## [0.6.2] 2020/4/13

### Add
- Injecting test attribute. You can choose your own test attribute (should be something like `*::test`) 
for each test. This feature enable every async runtime (See #91).

### Changed
- Start to use `rstest` to test `rstest` (On going task #92) 

## [0.6.1] 2020/4/5

### Add
- Introducing async tests support. Leverage on `async_std::test` to automatically switch to
async test (both for single, cases and matrix) (See #73)

## [0.6.0] 2020/3/5

### Add
- Hook argument name to fixture by remove starting `_` (See #70)
- Every `case` can have a specific set of attributes (See #82)

### Changed

- Removed useless `rstest_parametrize` and `rstest_matrix` (See #81). From 0.5.0 you
can use just `rstest` to create cases and values list.
- Removed `cargo-edit` test dependecy (See #61)

## [0.5.3] 2020/1/23

### Fixed

- Fixed  a false _unused mut_ warning regression introduced by
partial fixtures (See [8a0ff08](https://github.com/la10736/rstest/commit/8a0ff0874dc8186edfaefb1ddef64d53666b94da))

## [0.5.2] 2019/12/29

### Fixed

- Fixed _unused attribute_ warning when use `should_panic`
attribute (See #79)

## [0.5.1] 2019/12/14

### Fixed

- `README.md` links
- License files

## [0.5.0] 2019/12/13

### Added

- Use just `rstest` for implementing all kind of tests (See #42)
- New matrix tests render: indicate argument name and nest groups
in modules (See #68 for details)
- CI (github actions) build and tests (See #46)

### Changed

- Better `README.md` that introduce all features
- (From rustc 1.40) Deprecated `rstest_parametrize` and `rstest_matrix`:
`rstest` now cover all features
- Refactored

### Fixed

- Error message if fixture or value are used more than once

## [0.4.1] 2019-10-05

### Changed

- Fixed README, crate description, changelog dates

## [0.4.0] 2019-10-04

### Added

- Injecting fixture with partial values in all tests (See #48)
- Add new `rstest_matrix` macro to build tests by carthesian product of
input arguments (See #38)

### Fixed

- Just bugs in tests

### Changed

- Use `unindent` crate instead the home made `Deindent` trait
- Use `itertools`
- Refactor parsing

## [0.3.0] 2019-06-28

### Added

- Introduced `fixture` macro: Now you must annotate your fixture by
this tag. See #5
- Support for arbitrary rust code without use `Unwrap(str_lit)` trick.
See #19 and #20 (deprecate `Unwrap()`)
- Support for tests that return `Result()`, See #23
- Support for dump test arguments
- `rstest_parametrize` use module to group cases (See #13)
- You can optionally give a an name for each test case (See #11)
- Docs
- Descriptive error handling: See #12, #15
- `rstest_parametrize` can leave comma after last case

### Fixed

- `rstest_parametrize` should catch error in input! See #1, #14
- Use negative literal. See #18

### Changed

- You need to use `fixture` to tag all your fixtures.
- Migrate to 2018 Epoch
- Tests: Refactoring and speed up

## [0.2.2] 2018-10-18

### Fixed

- Better error handling

## [0.2.1] 2018-10-15

### Changed

- crate.io categories

## [0.2.0] 2018-10-14

First Public release.

## [0.1.0] ...

Just my testing and private use.
