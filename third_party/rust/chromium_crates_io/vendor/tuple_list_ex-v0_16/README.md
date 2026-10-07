# `Tuple_List_Ex`: Added functionality for `tuple_lists`

<img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

This crate adds handy features to the Haskel-like [`tuple_list`](https://docs.rs/tuple_list/latest/tuple_list/) crate.

It's part of the `LibAFL` project.

## Usage

This crate provides a variety of traits that extend the functionality of `tuple_list`. Here are a few examples:

### `MatchFirstType`

Get the first element of a specific type from a tuple list.

```rust
use tuple_list::tuple_list;
use tuple_list_ex::MatchFirstType;

// Create a tuple list
let tuple = tuple_list!(1i32, "hello", 3.0f64);

// Get the first element of a specific type
let first_i32: Option<&i32> = tuple.match_first_type();
assert_eq!(first_i32, Some(&1));

let first_f64: Option<&f64> = tuple.match_first_type();
assert_eq!(first_f64, Some(&3.0));
```

### `Prepend`, `Append`, and `Merge`

Modify tuple lists by adding elements or merging them.

```rust
use tuple_list::tuple_list;
use tuple_list_ex::{Prepend, Append, Merge};

let tuple = tuple_list!(1i32, "hello");

// Prepend an element
let prepended = tuple.prepend(true);
assert_eq!(prepended, (true, (1, ("hello", ()))));

// Append an element
let appended = prepended.append(3.0f64);
assert_eq!(appended, ((true, (1, ("hello", ()))), 3.0f64));

// Merge two tuple lists
let other_tuple = tuple_list!(4u8, 5u16);
let merged = prepended.merge(other_tuple);
assert_eq!(merged, (true, (1, ("hello", (4, (5, ()))))));
```

### `NamedTuple` and `MatchName`

Access elements by name for tuples containing `Named` elements.

```rust
# #[cfg(feature = "alloc")]
# {
use tuple_list::tuple_list;
use tuple_list_ex::{NamedTuple, MatchName};
use libafl_core::Named;
use std::borrow::Cow;

struct MyNamed {
    name: Cow<'static, str>,
}

impl Named for MyNamed {
    fn name(&self) -> &Cow<'static, str> {
        &self.name
    }
}

let named_tuple = tuple_list!(MyNamed { name: Cow::Borrowed("first") }, MyNamed { name: Cow::Borrowed("second") });

// Get names
let names = named_tuple.names();
assert_eq!(names, vec!["first", "second"]);

// Get an element by name
let second: Option<&MyNamed> = named_tuple.match_name("second");
assert!(second.is_some());
assert_eq!(second.unwrap().name(), "second");
# }
```

## Features

This crate has the following features:

* `std`: (Default) Enables features that require the standard library.
* `alloc`: (Default) Enables features that require allocation, like `IntoVec` and `NamedTuple`.
* `serde`: Enables `serde` support for `Handle`.

## Provided Traits and Functionality

Here is a list of the traits and functionality provided by this crate:

* **`SplitBorrow`**: Borrows each member of the tuple, returning a new tuple of references.
* **`IntoVec`**: Converts a tuple list into a `Vec`.
* **`HasConstLen`**: Provides the length of the tuple list as a const `usize`.
* **`MatchFirstType`**: Gets the first element of a specific type.
* **`ExtractFirstRefType`** and **`ExtractFirstRefMutType`**: Takes the first element of a given type.
* **`SplitBorrowExtractFirstType`**: A combination of `SplitBorrow` and `ExtractFirstRefType`.
* **`MatchType`**: Applies a function to all elements of a specific type.
* **`NamedTuple`**: For tuples where each element has a name.
* **`MatchName`**: Finds an element by its name.
* **`Handled`**, **`Handle`**, **`MatchNameRef`**: A system for referencing tuple elements by a handle (name + type).
* **`GetAll`**: Retrieves multiple elements from a tuple list using a list of handles.
* **`RefIndexable`**: Allows indexing a tuple list with `[]` using handles.
* **`Prepend`**: Adds an element to the beginning of a tuple list.
* **`Append`**: Adds an element to the end of a tuple list.
* **`Merge`**: Merges two tuple lists.
* **`Map`**, **`MappingFunctor`**: Maps each element of a tuple list to a new type.
* **Macros**:
  * `tuple_for_each!`: Iterates over a tuple.
  * `tuple_for_each_mut!`: Iterates over a tuple with mutable access.
  * `map_tuple_list_type!`: Gets the resulting type of a map operation.
  * `merge_tuple_list_type!`: Gets the resulting type of a merge operation.

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
