# nonzero_macros

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">


Macros for compile-time checked `NonZero` integers and safe `NonNull` pointer creation in `no_std` environments.

Far simpler than other crates, as it leverages the modern generic `core::num::NonZero` type.
Lightweight, `no_std`, and dependency-free.


## Macros

### `nonzero!`

Constructs a `NonZero*` type at compile-time. Panics if the value is zero.

```rust
use nonzero_macros::nonzero;
use core::num::NonZeroUsize;

const MY_VAL: NonZeroUsize = nonzero!(10);
```

### `try_nonzero!`

Constructs an `Option<NonZero*>` type. Works at both compile-time and runtime.
Returns `None` if the value is zero.

```rust
use nonzero_macros::try_nonzero;
use core::num::NonZeroUsize;

const MY_VAL: Option<NonZeroUsize> = try_nonzero!(10);
const MY_ZERO: Option<NonZeroUsize> = try_nonzero!(0); // None

let x = 10;
let val = try_nonzero!(x);
```

### `nonnull_raw_mut!`

Constructs a `core::ptr::NonNull` from a mutable reference expression. Useful for static muts.

```rust
use nonzero_macros::nonnull_raw_mut;

static mut MY_STATIC: i32 = 0;
let ptr = nonnull_raw_mut!(MY_STATIC);
```

#### License

<sup>
Licensed under either of <a href="../LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="../LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>

<br>
