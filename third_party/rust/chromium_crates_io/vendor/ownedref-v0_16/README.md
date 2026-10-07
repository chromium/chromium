# `OwnedRef`: Owned references for the masses

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `ownedref` crate provides a set of wrappers that abstract over accessing data that can be either owned or borrowed (referenced). The primary purpose of these wrappers is to enable serialization of data structures that contain references or raw pointers.

A key feature is the ability to serialize references by transparently converting any borrowed data to an owned form. This is particularly useful in applications like serialization for IPC or saving state, where we can't save memory addresses but need to preserve the data they point to.

When you have a struct that contains references, you can't directly serialize it with `serde`. `ownedref` provides wrapper types that can be used in place of references. These wrappers are enums that can hold either a reference or an owned value. When serializing, they always serialize the underlying data, and when deserializing, they create an owned value.

This allows you to work with references for performance within your application, and seamlessly serialize and deserialize your data structures when needed.

## Wrappers

The crate provides the following wrappers:

- `OwnedRef<'a, T>`: Wraps a `&'a T` or `Box<T>`.
- `OwnedRefMut<'a, T>`: Wraps a `&'a mut T` or `Box<T>`.
- `OwnedSlice<'a, T>`: Wraps a `&'a [T]` or `Vec<T>`.
- `OwnedMutSlice<'a, T>`: Wraps a `&'a mut [T]` or `Vec<T>`.
- `OwnedMutSizedSlice<'a, T, const N: usize>`: Wraps a `&'a mut [T; N]` or `Box<[T; N]>`.
- `OwnedPtr<T>`: Wraps a `*const T` or `Box<T>`.
- `OwnedMutPtr<T>`: Wraps a `*mut T` or `Box<T>`.

## Usage

Here are some examples of how to use the `ownedref` wrappers.

### `OwnedRef`

`OwnedRef` can be used to hold either a reference to a value or an owned value.

```rust
use ownedref::OwnedRef;
use serde::{Serialize, Deserialize};

// A struct that can be either borrowed or owned.
#[derive(Serialize, Deserialize, Debug)]
struct MyStruct<'a> {
    data: OwnedRef<'a, [u8]>,
}

// Create an instance with a borrowed reference.
let data = vec![1, 2, 3, 4];
let borrowed_struct = MyStruct { data: OwnedRef::Ref(data.as_slice()) };

// You can access the data using `as_ref`.
assert_eq!(borrowed_struct.data.as_ref(), &[1, 2, 3, 4]);

// Serialize the struct. This will copy the data.
let serialized = serde_json::to_string(&borrowed_struct).unwrap();
println!("Serialized: {}", serialized);

// Deserialize the struct. This will create an owned value.
let deserialized_struct: MyStruct = serde_json::from_str(&serialized).unwrap();
assert_eq!(deserialized_struct.data.as_ref(), &[1, 2, 3, 4]);

// The deserialized struct now owns the data.
match deserialized_struct.data {
    OwnedRef::Owned(_) => println!("Data is owned."),
    _ => panic!("Data should be owned after deserialization."),
}
```

### `OwnedSlice`

`OwnedSlice` is useful for slices of data.

```rust
use ownedref::OwnedSlice;
use libafl_core::IntoOwned;
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug)]
struct DataContainer<'a> {
    slice: OwnedSlice<'a, u32>,
}

// With a borrowed slice
let original_data = vec![10, 20, 30];
let container_borrowed = DataContainer {
    slice: OwnedSlice::from(original_data.as_slice()),
};

assert_eq!(container_borrowed.slice.as_ref(), &[10, 20, 30]);

let serialized = serde_json::to_string(&container_borrowed).unwrap();
println!("Serialized: {}", serialized);

// Deserialize into an owned version
let container_deserialized: DataContainer = serde_json::from_str(&serialized).unwrap();
assert_eq!(container_deserialized.slice.as_ref(), &[10, 20, 30]);
assert!(container_deserialized.slice.is_owned());
```

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

* keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
* run `cargo nightly fmt` on your code before pushing
* check the output of `cargo clippy --all` or `./clippy.sh`
* run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some parts in this list may sound hard, but don't be afraid to open a PR if you cannot fix them by yourself. We will gladly assist.

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

<sub>
Dependencies under more restrictive licenses, such as GPL or AGPL, can be enabled
using the respective feature in each crate when it is present, such as the
'agpl' feature of the libafl crate.
</sub>
