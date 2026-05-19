# arbitrary-int

This crate implements arbitrary numbers for Rust. Once included, you can use types like `u5` or `u120`.

## Why yet another arbitrary integer crate?

There are quite a few similar crates to this one (the most famous being https://crates.io/crates/ux). After trying out a
few of them I just realized that they are all very heavy: They create a ton of classes and take seconds to compile.

This crate is designed to be very short, using const generics. Instead of introducing ~123 new structs, this crates only
introduces 5 (one for `u8`, `u16`, `u32`, `u64`, `u128`) and uses const generics for the specific bit depth.
It does introduce 123 new type aliases (`u1`, `u2`, etc.), but these don't stress the compiler nearly as much.

Additionally, most of its functions are const, so that they can be used in const contexts.

## How to use

Unlike primitive data types like `u32`, there is no intrinsic syntax (Rust does not allow that). An instance is created as
follows:

```rust
let value9 = u9::new(30);
```

This will create a value with 9 bits. If the value passed into `new()` doesn't fit, a panic! will be raised. This means
that a function that accepts a `u9` as an argument can be certain that its contents are never larger than an `u9`.

Standard operators are all overloaded, so it is possible to perform calculations using this type. Note that addition
and subtraction (at least in debug mode) performs bounds check. If this is undesired, see chapter num-traits below.

Internally, `u9` will hold its data in an `u16`. It is possible to get this value:

```rust
let value9 = u9::new(30).value();
```

## Underlying data type

This crate defines types `u1`, `u2`, .., `u126`, `u127` (skipping the normal `u8`, `u16`, `u32`, `u64`, `u128`). Each of those types holds
its actual data in the next larger data type (e.g. a `u14` internally has an `u16`, a `u120` internally has an `u128`). However,
`uXX` are just type aliases; it is also possible to use the actual underlying generic struct:

```rust
let a = UInt::<u8, 5>::new(0b10101));
let b = UInt::<u32, 5>::new(0b10101));
```

In this example, `a` will have 5 bits and be represented by a `u8`. This is identical to `u5`. `b` however is represented by a
`u32`, so it is a different type from `u5`.

## Extract

A common source for arbitrary integers is by extracting them from bitfields. For example, if data contained 32 bits and
we want to extract bits `4..=9`, we could perform the following:

```rust
let a = u6::new(((data >> 4) & 0b111111) as u8);
```

This is a pretty common operation, but it's easy to get it wrong: The number of 1s and `u6` have to match. Also, `new()`
will internally perform a bounds-check, which can panic. Thirdly, a type-cast is often needed.
To make this easier, various extract methods exist that handle shifting and masking, for example:

```rust
let a = u6::extract_u32(data, 4);
let b = u12::extract_u128(data2, 63);
```

## num-traits

By default, arbitrary-int doesn't require any other traits. It has optional support for num-traits however. It
implements `WrappingAdd`, `WrappingSub`, which (unlike the regular addition and subtraction) don't perform bounds checks.
