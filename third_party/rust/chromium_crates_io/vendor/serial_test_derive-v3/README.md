# serial_test
[![Version](https://img.shields.io/crates/v/serial_test.svg)](https://crates.io/crates/serial_test)
[![Downloads](https://img.shields.io/crates/d/serial_test)](https://crates.io/crates/serial_test)
[![Docs](https://docs.rs/serial_test/badge.svg)](https://docs.rs/serial_test/)
[![MIT license](https://img.shields.io/crates/l/serial_test.svg)](./LICENSE)
[![Build Status](https://github.com/palfrey/serial_test/actions/workflows/ci.yml/badge.svg)](https://github.com/palfrey/serial_test/actions)
[![MSRV: 1.68.2](https://flat.badgen.net/badge/MSRV/1.68.2/purple)](https://blog.rust-lang.org/2023/03/28/Rust-1.68.2.html)

`serial_test` allows for the creation of serialised Rust tests using the `serial` attribute
e.g.
```rust
use serial_test::serial;

#[test]
#[serial]
fn test_serial_one() {
  // Do things
}

#[test]
#[serial]
fn test_serial_another() {
  // Do things
}

#[tokio::test]
#[serial]
async fn test_serial_another() {
  // Do things asynchronously
}
```
Multiple tests with the `serial` attribute are guaranteed to be executed in serial. Ordering of the tests is not guaranteed however. Other tests with the `parallel` attribute may run at the same time as each other, but not at the same time as a test with `serial`. Tests with neither attribute may run at any time and no guarantees are made about their timing! Both support optional keys for defining subsets of tests to run in serial together, see docs for more details.

For cases like doctests and integration tests where the tests are run as separate processes, we also support `file_serial`, with
similar properties but based off file locking. Note that there are no guarantees about one test with `serial` and another with 
`file_serial` as they lock using different methods.

All of the attributes can also be applied at a `mod` level and will be automagically applied to all test functions in that block.

## Inner Attributes

You can apply attributes to an inner test function using `inner_attrs`. This is useful for applying attributes like `ntest::timeout` that should only affect the test body, not the mutex/lock acquisition:

```rust
#[test]
#[serial(inner_attrs = [ntest::timeout(1000)])]
fn test_with_timeout() {
  // The timeout only applies to this body, not the serial lock acquisition
}
```

This can be combined with keys: `#[serial(my_key, inner_attrs = [timeout(1000)])]`

## Usage
The minimum supported Rust version here is 1.68.2. Note this is minimum _supported_, as it may well compile with lower versions, but they're not supported at all. Upgrades to this will require at a major version bump. 1.x supports 1.51 if you need a lower version than that.

Add to your Cargo.toml
```toml
[dev-dependencies]
serial_test = "*"
```

plus `use serial_test::serial;` in your imports section.

You can then either add `#[serial]` or `#[serial(some_key)]` to tests as required.
