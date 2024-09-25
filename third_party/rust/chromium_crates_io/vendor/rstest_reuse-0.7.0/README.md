[![Crate][crate-image]][crate-link]
[![Status][test-action-image]][test-action-link]
[![Apache 2.0 Licensed][license-apache-image]][license-apache-link]
[![MIT Licensed][license-mit-image]][license-mit-link]

# Reuse `rstest`'s parametrized cases

This crate gives a way to define tests set and apply them to every case you need to
test. With `rstest` crate you can define a tests list but if you want to apply the same tests
to another test function you must rewrite all cases or write some macros that do the job.

Both solutions have some drawbreak:

- introduce duplication
- macros makes code harder to read and shift out the focus from tests core

The aim of this crate is solve this problem. `rstest_reuse` expose two attributes:

- `#[template]`: to define a template
- `#[apply]`: to apply a defined template to create tests

Here is a simple example:

```rust
use rstest::rstest;
use rstest_reuse::{self, *};
// Here we define the template. This define
// * The test list name to `two_simple_cases`
// * cases: here two cases
#[template]
#[rstest]
#[case(2, 2)]
#[case(4/2, 2)]
// Define a and b as cases arguments
fn two_simple_cases(#[case] a: u32, #[case] b: u32) {}
// Here we apply the `two_simple_cases` template: That is expanded in
// #[template]
// #[rstest]
// #[case(2, 2)]
// #[case(4/2, 2)]
// fn it_works(#[case] a: u32,#[case] b: u32) {
//     assert!(a == b);
// }
#[apply(two_simple_cases)]
fn it_works(a: u32, b: u32) {
    assert!(a == b);
}
// Here we reuse the `two_simple_cases` template to create two 
// other tests
#[apply(two_simple_cases)]
fn it_fail(a: u32, b: u32) {
    assert!(a != b);
}
```

If we run `cargo test` we have:

```text
    Finished test [unoptimized + debuginfo] target(s) in 0.05s
     Running target/debug/deps/playground-8a1212f8b5eb00ce
running 4 tests
test it_fail::case_1 ... FAILED
test it_works::case_1 ... ok
test it_works::case_2 ... ok
test it_fail::case_2 ... FAILED
failures:
---- it_fail::case_1 stdout ----
thread 'it_fail::case_1' panicked at 'assertion failed: a != b', src/main.rs:34:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
---- it_fail::case_2 stdout ----
thread 'it_fail::case_2' panicked at 'assertion failed: a != b', src/main.rs:34:5
failures:
    it_fail::case_1
    it_fail::case_2
test result: FAILED. 2 passed; 2 failed; 0 ignored; 0 measured; 0 filtered out
error: test failed, to rerun pass '--bin playground'
```

Simple and neat!

Note that if the test arguments names match the template's ones you can don't
repeat the arguments attributes.

## Composition and Values

If you need to add some cases or values when apply a template you can leverage on
composition. Here a simple example:

```rust
#[template]
#[rstest]
#[case(2, 2)]
#[case(4/2, 2)]
fn base(#[case] a: u32, #[case] b: u32) {}

// Here we add a new case and an argument in a value list:
#[apply(base)]
#[case(9/3, 3)]
fn it_works(a: u32, b: u32, #[values("a", "b")] t: &str) {
    assert!(a == b);
    assert!("abcd".contains(t))
}
```

run 6 tests:

```text
running 6 tests
test it_works::case_1::t_2 ... ok
test it_works::case_2::t_2 ... ok
test it_works::case_2::t_1 ... ok
test it_works::case_3::t_2 ... ok
test it_works::case_3::t_1 ... ok
test it_works::case_1::t_1 ... ok
```

Template can also be used for values and with arguments if you need:

```rust
#[template]
#[rstest]
fn base(#[with(42)] fix: u32, #[values(1,2,3)] v: u32) {}

#[fixture]
fn fix(#[default(0)] inner: u32) -> u32 {
    inner
}

#[apply(base)]
fn use_it_with_fixture(fix: u32, v: u32) {
    assert!(fix%v == 0);
}

#[apply(base)]
fn use_it_without_fixture(v: u32) {
    assert!(24 % v == 0);
}
```

Run also 6 tests:

```text
running 6 tests
test use_it_with_fixture::v_1 ... ok
test use_it_without_fixture::v_1 ... ok
test use_it_with_fixture::v_3 ... ok
test use_it_without_fixture::v_2 ... ok
test use_it_without_fixture::v_3 ... ok
test use_it_with_fixture::v_2 ... ok
```

## `#[export]` Attribute

Now `#[export]` attribute give you the possibility to export your template across crates
but don't lift the macro definition at the top of your crate (that was the default behavior
prior the 0.5.0 version).

Now if you want to put your template at the root of your crate you can define it in the root
module or reexport it at the top with something like the following line at the top of
your crate:

```rust
pub use my::modules::path::of::my::template::my_template;
```

When you want to export your template you should also take care to declare `rstest_reuse` as `pub`
at the top of your crate to enable to use it from the modules that would import the template.

So in this case in the crate that would export template you should put at the root of your
crate

```rust
#[cfg(test)]
pub use rstest_reuse;
```

And not just `use rstest_reuse` like in the standard cases.

## Disclamer

This crate is in a development stage. I don't know if I'll include it in `rstest` or change some syntax in the future.

I didn't test it in a lot of cases: if you have some cases where it doesn't work file a ticket on [`rstest`][rstest-link]

## License

Licensed under either of

- Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
[license-apache-link])

- MIT license [LICENSE-MIT](LICENSE-MIT) or [license-MIT-link]
at your option.

[//]: # (links)

[crate-image]: https://img.shields.io/crates/v/rstest_reuse.svg
[crate-link]: https://crates.io/crates/rstest_reuse
[test-action-image]: https://github.com/la10736/rstest/workflows/Test/badge.svg
[test-action-link]: https://github.com/la10736/rstest/actions?query=workflow:Test
[license-apache-image]: https://img.shields.io/badge/license-Apache2.0-blue.svg
[license-mit-image]: https://img.shields.io/badge/license-MIT-blue.svg
[license-apache-link]: http://www.apache.org/licenses/LICENSE-2.0
[license-MIT-link]: http://opensource.org/licenses/MIT
[rstest-link]: https://github.com/la10736/rstest
