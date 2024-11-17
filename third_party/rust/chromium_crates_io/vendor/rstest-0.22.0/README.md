[![Crate][crate-image]][crate-link]
[![Docs][docs-image]][docs-link]
[![Status][test-action-image]][test-action-link]
[![Apache 2.0 Licensed][license-apache-image]][license-apache-link]
[![MIT Licensed][license-mit-image]][license-mit-link]

# Fixture-based test framework for Rust

## Introduction

`rstest` uses procedural macros to help you on writing
fixtures and table-based tests. To use it, add the
following lines to your `Cargo.toml` file:

```toml
[dev-dependencies]
rstest = "0.22.0"
```

### Features

- `async-timeout`: `timeout` for `async` tests (Default enabled)
- `crate-name`: Import `rstest` package with different name (Default enabled)

### Fixture

The core idea is that you can inject your test dependencies
by passing them as test arguments. In the following example,
a `fixture` is defined and then used in two tests,
simply providing it as an argument:

```rust
use rstest::*;

#[fixture]
pub fn fixture() -> u32 { 42 }

#[rstest]
fn should_success(fixture: u32) {
    assert_eq!(fixture, 42);
}

#[rstest]
fn should_fail(fixture: u32) {
    assert_ne!(fixture, 42);
}
```

### Parametrize

You can also inject values in some other ways. For instance, you can
create a set of tests by simply providing the injected values for each
case: `rstest` will generate an independent test for each case.

```rust
use rstest::rstest;

#[rstest]
#[case(0, 0)]
#[case(1, 1)]
#[case(2, 1)]
#[case(3, 2)]
#[case(4, 3)]
fn fibonacci_test(#[case] input: u32, #[case] expected: u32) {
    assert_eq!(expected, fibonacci(input))
}
```

Running `cargo test` in this case executes five tests:

```bash
running 5 tests
test fibonacci_test::case_1 ... ok
test fibonacci_test::case_2 ... ok
test fibonacci_test::case_3 ... ok
test fibonacci_test::case_4 ... ok
test fibonacci_test::case_5 ... ok

test result: ok. 5 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

If you need to just providing a bunch of values for which you
need to run your test, you can use `#[values(list, of, values)]`
argument attribute:

```rust
use rstest::rstest;

#[rstest]
fn should_be_invalid(
    #[values(None, Some(""), Some("    "))]
    value: Option<&str>
) {
    assert!(!valid(value))
}
```

Or create a _matrix_ test by using _list of values_ for some
variables that will generate the cartesian product of all the
values.

#### Use Parametrize definition in more tests

If you need to use a test list for more than one test you can use [`rstest_reuse`][reuse-crate-link]
crate. With this helper crate you can define a template and use it everywhere.

```rust
use rstest::rstest;
use rstest_reuse::{self, *};

#[template]
#[rstest]
#[case(2, 2)]
#[case(4/2, 2)]
fn two_simple_cases(#[case] a: u32, #[case] b: u32) {}

#[apply(two_simple_cases)]
fn it_works(#[case] a: u32, #[case] b: u32) {
    assert!(a == b);
}
```

See [`rstest_reuse`][reuse-crate-link] for more details.

#### Feature flagged cases

In case you want certain test cases to only be present if a certain feature is
enabled, use `#[cfg_attr(feature = …, case(…))]`:

```rust
use rstest::rstest;

#[rstest]
#[case(2, 2)]
#[cfg_attr(feature = "frac", case(4/2, 2))]
#[case(4/2, 2)]
fn it_works(#[case] a: u32, #[case] b: u32) {
    assert!(a == b);
}
```

This also works with [`rstest_reuse`][reuse-crate-link].

### Magic Conversion

If you need a value where its type implement `FromStr()` trait you can use a literal
string to build it:

```rust
# use rstest::rstest;
# use std::net::SocketAddr;
#[rstest]
#[case("1.2.3.4:8080", 8080)]
#[case("127.0.0.1:9000", 9000)]
fn check_port(#[case] addr: SocketAddr, #[case] expected: u16) {
    assert_eq!(expected, addr.port());
}
```

You can use this feature also in value list and in fixture default value.

### Async

`rstest` provides out of the box `async` support. Just mark your
test function as `async`, and it'll use `#[async-std::test]` to
annotate it. This feature can be really useful to build async
parametric tests using a tidy syntax:

```rust
use rstest::*;

#[rstest]
#[case(5, 2, 3)]
#[should_panic]
#[case(42, 40, 1)]
async fn my_async_test(#[case] expected: u32, #[case] a: u32, #[case] b: u32) {
    assert_eq!(expected, async_sum(a, b).await);
}
```

Currently, only `async-std` is supported out of the box. But if you need to use
another runtime that provide its own test attribute (i.e. `tokio::test` or
`actix_rt::test`) you can use it in your `async` test like described in
[Inject Test Attribute](#inject-test-attribute).

To use this feature, you need to enable `attributes` in the `async-std`
features list in your `Cargo.toml`:

```toml
async-std = { version = "1.5", features = ["attributes"] }
```

If your test input is an async value (fixture or test parameter) you can use `#[future]`
attribute to remove `impl Future<Output = T>` boilerplate and just use `T`:

```rust
use rstest::*;
#[fixture]
async fn base() -> u32 { 42 }

#[rstest]
#[case(21, async { 2 })]
#[case(6, async { 7 })]
async fn my_async_test(#[future] base: u32, #[case] expected: u32, #[future] #[case] div: u32) {
    assert_eq!(expected, base.await / div.await);
}
```

As you noted you should `.await` all _future_ values and this sometimes can be really boring.
In this case you can use `#[future(awt)]` to _awaiting_ an input or annotating your function
with `#[awt]` attributes to globally `.await` all your _future_ inputs. Previous code can be
simplified like follow:

```rust
use rstest::*;
# #[fixture]
# async fn base() -> u32 { 42 }
#[rstest]
#[case(21, async { 2 })]
#[case(6, async { 7 })]
#[awt]
async fn global(#[future] base: u32, #[case] expected: u32, #[future] #[case] div: u32) {
    assert_eq!(expected, base / div);
}
#[rstest]
#[case(21, async { 2 })]
#[case(6, async { 7 })]
async fn single(#[future] base: u32, #[case] expected: u32, #[future(awt)] #[case] div: u32) {
    assert_eq!(expected, base.await / div);
}
```

### Files path as input arguments

If you need to create a test for each file in a given location you can use
`#[files("glob path syntax")]` attribute to generate a test for each file that
satisfy the given glob path.

```rust
#[rstest]
fn for_each_file(#[files("src/**/*.rs")] #[exclude("test")] path: PathBuf) {
    assert!(check_file(&path))
}
```

The default behavior is to ignore the files that start with `"."`, but you can
modify this by use `#[include_dot_files]` attribute. The `files` attribute can be
used more than once on the same variable, and you can also create some custom
exclusion rules with the `#[exclude("regex")]` attributes that filter out all
paths that verify the regular expression.

### Default timeout

You can set a default timeout for test using the `RSTEST_TIMEOUT` environment variable.
The value is in seconds and is evaluated on test compile time.

### Test `#[timeout()]`

You can define an execution timeout for your tests with `#[timeout(<duration>)]` attribute. Timeout
works both for sync and async tests and is runtime agnostic. `#[timeout(<duration>)]` take an
expression that should return a `std::time::Duration`. Follow a simple async example:

```rust
use rstest::*;
use std::time::Duration;

async fn delayed_sum(a: u32, b: u32,delay: Duration) -> u32 {
    async_std::task::sleep(delay).await;
    a + b
}

#[rstest]
#[timeout(Duration::from_millis(80))]
async fn single_pass() {
    assert_eq!(4, delayed_sum(2, 2, ms(10)).await);
}
```

In this case test pass because the delay is just 10 milliseconds and timeout is
80 milliseconds.

You can use `timeout` attribute like any other attribute in your tests, and you can
override a group timeout with a case specific one. In the follow example we have
3 tests where first and third use 100 milliseconds but the second one use 10 milliseconds.
Another valuable point in this example is to use an expression to compute the
duration.

```rust
fn ms(ms: u32) -> Duration {
    Duration::from_millis(ms.into())
}

#[rstest]
#[case::pass(ms(1), 4)]
#[timeout(ms(10))]
#[case::fail_timeout(ms(60), 4)]
#[case::fail_value(ms(1), 5)]
#[timeout(ms(100))]
async fn group_one_timeout_override(#[case] delay: Duration, #[case] expected: u32) {
    assert_eq!(expected, delayed_sum(2, 2, delay).await);
}
```

If you want to use `timeout` for `async` test you need to use `async-timeout`
feature (enabled by default).

### Inject Test Attribute

If you would like to use another `test` attribute for your test you can simply
indicate it in your test function's attributes. For instance if you want
to test some async function with use `actix_rt::test` attribute you can just write:

```rust
use rstest::*;
use actix_rt;
use std::future::Future;

#[rstest]
#[case(2, async { 4 })]
#[case(21, async { 42 })]
#[actix_rt::test]
async fn my_async_test(#[case] a: u32, #[case] #[future] result: u32) {
    assert_eq!(2 * a, result.await);
}
```

Just the attributes that ends with `test` (last path segment) can be injected.

### Use `#[once]` Fixture

If you need to a fixture that should be initialized just once for all tests
you can use `#[once]` attribute. `rstest` call your fixture function just once and
return a reference to your function result to all your tests:

```rust
#[fixture]
#[once]
fn once_fixture() -> i32 { 42 }

#[rstest]
fn single(once_fixture: &i32) {
    // All tests that use once_fixture will share the same reference to once_fixture() 
    // function result.
    assert_eq!(&42, once_fixture)
}
```

## Local lifetime and `#[by_ref]` attribute

In some cases you may want to use a local lifetime for some arguments of your test.
In these cases you can use the `#[by_ref]` attribute then use the reference instead
the value.

```rust
enum E<'a> {
    A(bool),
    B(&'a Cell<E<'a>>),
}

fn make_e_from_bool<'a>(_bump: &'a (), b: bool) -> E<'a> {
    E::A(b)
}

#[fixture]
fn bump() -> () {}
 
#[rstest]
#[case(true, E::A(true))]
fn it_works<'a>(#[by_ref] bump: &'a (), #[case] b: bool, #[case] expected: E<'a>) {
    let actual = make_e_from_bool(&bump, b);
    assert_eq!(actual, expected);
}
```

You can use `#[by_ref]` attribute for all arguments of your test and not just for fixture
but also for cases, values and files.

## Complete Example

All these features can be used together with a mixture of fixture variables,
fixed cases and a bunch of values. For instance, you might need two
test cases which test for panics, one for a logged-in user and one for a guest user.

```rust
use rstest::*;

#[fixture]
fn repository() -> InMemoryRepository {
    let mut r = InMemoryRepository::default();
    // fill repository with some data
    r
}

#[fixture]
fn alice() -> User {
    User::logged("Alice", "2001-10-04", "London", "UK")
}

#[rstest]
#[case::authorized_user(alice())] // We can use `fixture` also as standard function
#[case::guest(User::Guest)]   // We can give a name to every case : `guest` in this case
                              // and `authorized_user`
#[should_panic(expected = "Invalid query error")] // We would test a panic
fn should_be_invalid_query_error(
    repository: impl Repository,
    #[case] user: User,
    #[values("     ", "^%$some#@invalid!chars", ".n.o.d.o.t.s.")] query: &str,
) {
    repository.find_items(&user, query).unwrap();
}
```

This example will generate exactly 6 tests grouped by 2 different cases:

```text
running 6 tests
test should_be_invalid_query_error::case_1_authorized_user::query_1_____ - should panic ... ok
test should_be_invalid_query_error::case_2_guest::query_2_____someinvalid_chars__ - should panic ... ok
test should_be_invalid_query_error::case_1_authorized_user::query_2_____someinvalid_chars__ - should panic ... ok
test should_be_invalid_query_error::case_2_guest::query_3____n_o_d_o_t_s___ - should panic ... ok
test should_be_invalid_query_error::case_1_authorized_user::query_3____n_o_d_o_t_s___ - should panic ... ok
test should_be_invalid_query_error::case_2_guest::query_1_____ - should panic ... ok

test result: ok. 6 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
```

Note that the names of the values _try_ to convert the input expression in a
Rust valid identifier name to help you find which tests fail.

## More

Is that all? Not quite yet!

A fixture can be injected by another fixture, and they can be called
using just some of its arguments.

```rust
#[fixture]
fn user(#[default("Alice")] name: &str, #[default(22)] age: u8) -> User {
    User::new(name, age)
}

#[rstest]
fn is_alice(user: User) {
    assert_eq!(user.name(), "Alice")
}

#[rstest]
fn is_22(user: User) {
    assert_eq!(user.age(), 22)
}

#[rstest]
fn is_bob(#[with("Bob")] user: User) {
    assert_eq!(user.name(), "Bob")
}

#[rstest]
fn is_42(#[with("", 42)] user: User) {
    assert_eq!(user.age(), 42)
}
```

As you noted you can provide default values without the need of a fixture
to define it.

Finally, if you need tracing the input values you can just
add the `trace` attribute to your test to enable the dump of all input
variables.

```rust
#[rstest]
#[case(42, "FortyTwo", ("minus twelve", -12))]
#[case(24, "TwentyFour", ("minus twentyfour", -24))]
#[trace] //This attribute enable tracing
fn should_fail(#[case] number: u32, #[case] name: &str, #[case] tuple: (&str, i32)) {
    assert!(false); // <- stdout come out just for failed tests
}
```

```text
running 2 tests
test should_fail::case_1 ... FAILED
test should_fail::case_2 ... FAILED

failures:

---- should_fail::case_1 stdout ----
------------ TEST ARGUMENTS ------------
number = 42
name = "FortyTwo"
tuple = ("minus twelve", -12)
-------------- TEST START --------------
thread 'should_fail::case_1' panicked at 'assertion failed: false', src/main.rs:64:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace.

---- should_fail::case_2 stdout ----
------------ TEST ARGUMENTS ------------
number = 24
name = "TwentyFour"
tuple = ("minus twentyfour", -24)
-------------- TEST START --------------
thread 'should_fail::case_2' panicked at 'assertion failed: false', src/main.rs:64:5


failures:
    should_fail::case_1
    should_fail::case_2

test result: FAILED. 0 passed; 2 failed; 0 ignored; 0 measured; 0 filtered out
```

In case one or more variables don't implement the `Debug` trait, an error
is raised, but it's also possible to exclude a variable using the
`#[notrace]` argument attribute.

You can learn more on [Docs][docs-link] and find more examples in
[`tests/resources`](/rstest/tests/resources) directory.

## Rust version compatibility

The minimum supported Rust version is 1.67.1.

## Changelog

See [CHANGELOG.md](/CHANGELOG.md)

## License

Licensed under either of

* Apache License, Version 2.0, ([LICENSE-APACHE](/LICENSE-APACHE) or
[license-apache-link])

* MIT license [LICENSE-MIT](/LICENSE-MIT) or [license-MIT-link]
at your option.

[//]: # (links)

[crate-image]: https://img.shields.io/crates/v/rstest.svg
[crate-link]: https://crates.io/crates/rstest
[docs-image]: https://docs.rs/rstest/badge.svg
[docs-link]: https://docs.rs/rstest/
[test-action-image]: https://github.com/la10736/rstest/workflows/Test/badge.svg
[test-action-link]: https://github.com/la10736/rstest/actions?query=workflow:Test
[license-apache-image]: https://img.shields.io/badge/license-Apache2.0-blue.svg
[license-mit-image]: https://img.shields.io/badge/license-MIT-blue.svg
[license-apache-link]: http://www.apache.org/licenses/LICENSE-2.0
[license-MIT-link]: http://opensource.org/licenses/MIT
[reuse-crate-link]: https://crates.io/crates/rstest_reuse
