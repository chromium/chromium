// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/*!
Generate types for C-style flags with ergonomic APIs.

# Getting started

Add `bitflags` to your `Cargo.toml`:

```toml
[dependencies.bitflags]
version = "2.4.2"
```

## Generating flags types

Use the [`bitflags`] macro to generate flags types:

```rust
use bitflags::bitflags;

bitflags! {
    pub struct Flags: u32 {
        const A = 0b00000001;
        const B = 0b00000010;
        const C = 0b00000100;
    }
}
```

See the docs for the `bitflags` macro for the full syntax.

Also see the [`example_generated`] module for an example of what the `bitflags` macro generates for a flags type.

### Externally defined flags

If you're generating flags types for an external source, such as a C API, you can define
an extra unnamed flag as a mask of all bits the external source may ever set. Usually this would be all bits (`!0`):

```rust
# use bitflags::bitflags;
bitflags! {
    pub struct Flags: u32 {
        const A = 0b00000001;
        const B = 0b00000010;
        const C = 0b00000100;

        // The source may set any bits
        const _ = !0;
    }
}
```

Why should you do this? Generated methods like `all` and truncating operators like `!` only consider
bits in defined flags. Adding an unnamed flag makes those methods consider additional bits,
without generating additional constants for them. It helps compatibility when the external source
may start setting additional bits at any time. The [known and unknown bits](#known-and-unknown-bits)
section has more details on this behavior.

### Custom derives

You can derive some traits on generated flags types if you enable Cargo features. The following
libraries are currently supported:

- `serde`: Support `#[derive(Serialize, Deserialize)]`, using text for human-readable formats,
and a raw number for binary formats.
- `arbitrary`: Support `#[derive(Arbitrary)]`, only generating flags values with known bits.
- `bytemuck`: Support `#[derive(Pod, Zeroable)]`, for casting between flags values and their
underlying bits values.

You can also define your own flags type outside of the [`bitflags`] macro and then use it to generate methods.
This can be useful if you need a custom `#[derive]` attribute for a library that `bitflags` doesn't
natively support:

```rust
# use std::fmt::Debug as SomeTrait;
# use bitflags::bitflags;
#[derive(SomeTrait)]
pub struct Flags(u32);

bitflags! {
    impl Flags: u32 {
        const A = 0b00000001;
        const B = 0b00000010;
        const C = 0b00000100;
    }
}
```

### Adding custom methods

The [`bitflags`] macro supports attributes on generated flags types within the macro itself, while
`impl` blocks can be added outside of it:

```rust
# use bitflags::bitflags;
bitflags! {
    // Attributes can be applied to flags types
    #[repr(transparent)]
    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    pub struct Flags: u32 {
        const A = 0b00000001;
        const B = 0b00000010;
        const C = 0b00000100;
    }
}

// Impl blocks can be added to flags types
impl Flags {
    pub fn as_u64(&self) -> u64 {
        self.bits() as u64
    }
}
```

## Working with flags values

Use generated constants and standard bitwise operators to interact with flags values:

```rust
# use bitflags::bitflags;
# bitflags! {
#     #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#     pub struct Flags: u32 {
#         const A = 0b00000001;
#         const B = 0b00000010;
#         const C = 0b00000100;
#     }
# }
// union
let ab = Flags::A | Flags::B;

// intersection
let a = ab & Flags::A;

// difference
let b = ab - Flags::A;

// complement
let c = !ab;
```

See the docs for the [`Flags`] trait for more details on operators and how they behave.

# Formatting and parsing

`bitflags` defines a text format that can be used to convert any flags value to and from strings.

See the [`parser`] module for more details.

# Specification

The terminology and behavior of generated flags types is
[specified in the source repository](https://github.com/bitflags/bitflags/blob/main/spec.md).
Details are repeated in these docs where appropriate, but is exhaustively listed in the spec. Some
things are worth calling out explicitly here.

## Flags types, flags values, flags

The spec and these docs use consistent terminology to refer to things in the bitflags domain:

- **Bits type**: A type that defines a fixed number of bits at specific locations.
- **Flag**: A set of bits in a bits type that may have a unique name.
- **Flags type**: A set of defined flags over a specific bits type.
- **Flags value**: An instance of a flags type using its specific bits value for storage.

```
# use bitflags::bitflags;
bitflags! {
    struct FlagsType: u8 {
//                    -- Bits type
//         --------- Flags type
        const A = 1;
//            ----- Flag
    }
}

let flag = FlagsType::A;
//  ---- Flags value
```

## Known and unknown bits

Any bits in a flag you define are called _known bits_. Any other bits are _unknown bits_.
In the following flags type:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const A = 1;
        const B = 1 << 1;
        const C = 1 << 2;
    }
}
```

The known bits are `0b0000_0111` and the unknown bits are `0b1111_1000`.

`bitflags` doesn't guarantee that a flags value will only ever have known bits set, but some operators
will unset any unknown bits they encounter. In a future version of `bitflags`, all operators will
unset unknown bits.

If you're using `bitflags` for flags types defined externally, such as from C, you probably want all
bits to be considered known, in case that external source changes. You can do this using an unnamed
flag, as described in [externally defined flags](#externally-defined-flags).

## Zero-bit flags

Flags with no bits set should be avoided because they interact strangely with [`Flags::contains`]
and [`Flags::intersects`]. A zero-bit flag is always contained, but is never intersected. The
names of zero-bit flags can be parsed, but are never formatted.

## Multi-bit flags

Flags that set multiple bits should be avoided unless each bit is also in a single-bit flag.
Take the following flags type as an example:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const A = 1;
        const B = 1 | 1 << 1;
    }
}
```

The result of `Flags::A ^ Flags::B` is `0b0000_0010`, which doesn't correspond to either
`Flags::A` or `Flags::B` even though it's still a known bit.
*/

#![cfg_attr(not(any(feature = "std", test)), no_std)]
#![cfg_attr(not(test), forbid(unsafe_code))]
#![cfg_attr(test, allow(mixed_script_confusables))]

#[doc(inline)]
pub use traits::{Bits, Flag, Flags};

pub mod iter;
pub mod parser;

mod traits;

#[doc(hidden)]
pub mod __private {
    #[allow(unused_imports)] // Easier than conditionally checking any optional external dependencies
    pub use crate::{external::__private::*, traits::__private::*};

    pub use core;
}

#[allow(unused_imports)]
pub use external::*;

#[allow(deprecated)]
pub use traits::BitFlags;

/*
How does the bitflags crate work?

This library generates a `struct` in the end-user's crate with a bunch of constants on it that represent flags.
The difference between `bitflags` and a lot of other libraries is that we don't actually control the generated `struct` in the end.
It's part of the end-user's crate, so it belongs to them. That makes it difficult to extend `bitflags` with new functionality
because we could end up breaking valid code that was already written.

Our solution is to split the type we generate into two: the public struct owned by the end-user, and an internal struct owned by `bitflags` (us).
To give you an example, let's say we had a crate that called `bitflags!`:

```rust
bitflags! {
    pub struct MyFlags: u32 {
        const A = 1;
        const B = 2;
    }
}
```

What they'd end up with looks something like this:

```rust
pub struct MyFlags(<MyFlags as PublicFlags>::InternalBitFlags);

const _: () = {
    #[repr(transparent)]
    #[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
    pub struct MyInternalBitFlags {
        bits: u32,
    }

    impl PublicFlags for MyFlags {
        type Internal = InternalBitFlags;
    }
};
```

If we want to expose something like a new trait impl for generated flags types, we add it to our generated `MyInternalBitFlags`,
and let `#[derive]` on `MyFlags` pick up that implementation, if an end-user chooses to add one.

The public API is generated in the `__impl_public_flags!` macro, and the internal API is generated in
the `__impl_internal_flags!` macro.

The macros are split into 3 modules:

- `public`: where the user-facing flags types are generated.
- `internal`: where the `bitflags`-facing flags types are generated.
- `external`: where external library traits are implemented conditionally.
*/

/**
Generate a flags type.

# `struct` mode

A declaration that begins with `$vis struct` will generate a `struct` for a flags type, along with
methods and trait implementations for it. The body of the declaration defines flags as constants,
where each constant is a flags value of the generated flags type.

## Examples

Generate a flags type using `u8` as the bits type:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const A = 1;
        const B = 1 << 1;
        const C = 0b0000_0100;
    }
}
```

Flags types are private by default and accept standard visibility modifiers. Flags themselves
are always public:

```
# use bitflags::bitflags;
bitflags! {
    pub struct Flags: u8 {
        // Constants are always `pub`
        const A = 1;
    }
}
```

Flags may refer to other flags using their [`Flags::bits`] value:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const A = 1;
        const B = 1 << 1;
        const AB = Flags::A.bits() | Flags::B.bits();
    }
}
```

A single `bitflags` invocation may include zero or more flags type declarations:

```
# use bitflags::bitflags;
bitflags! {}

bitflags! {
    struct Flags1: u8 {
        const A = 1;
    }

    struct Flags2: u8 {
        const A = 1;
    }
}
```

# `impl` mode

A declaration that begins with `impl` will only generate methods and trait implementations for the
`struct` defined outside of the `bitflags` macro.

The struct itself must be a newtype using the bits type as its field.

The syntax for `impl` mode is identical to `struct` mode besides the starting token.

## Examples

Implement flags methods and traits for a custom flags type using `u8` as its underlying bits type:

```
# use bitflags::bitflags;
struct Flags(u8);

bitflags! {
    impl Flags: u8 {
        const A = 1;
        const B = 1 << 1;
        const C = 0b0000_0100;
    }
}
```

# Named and unnamed flags

Constants in the body of a declaration are flags. The identifier of the constant is the name of
the flag. If the identifier is `_`, then the flag is unnamed. Unnamed flags don't appear in the
generated API, but affect how bits are truncated.

## Examples

Adding an unnamed flag that makes all bits known:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const A = 1;
        const B = 1 << 1;

        const _ = !0;
    }
}
```

Flags types may define multiple unnamed flags:

```
# use bitflags::bitflags;
bitflags! {
    struct Flags: u8 {
        const _ = 1;
        const _ = 1 << 1;
    }
}
```
*/
#[macro_export]
macro_rules! bitflags {
    (
        $(#[$outer:meta])*
        $vis:vis struct $BitFlags:ident: $T:ty {
            $(
                $(#[$inner:ident $($args:tt)*])*
                const $Flag:tt = $value:expr;
            )*
        }

        $($t:tt)*
    ) => {
        // Declared in the scope of the `bitflags!` call
        // This type appears in the end-user's API
        $crate::__declare_public_bitflags! {
            $(#[$outer])*
            $vis struct $BitFlags
        }

        // Workaround for: https://github.com/bitflags/bitflags/issues/320
        $crate::__impl_public_bitflags_consts! {
            $BitFlags: $T {
                $(
                    $(#[$inner $($args)*])*
                    const $Flag = $value;
                )*
            }
        }

        #[allow(
            dead_code,
            deprecated,
            unused_doc_comments,
            unused_attributes,
            unused_mut,
            unused_imports,
            non_upper_case_globals,
            clippy::assign_op_pattern,
            clippy::indexing_slicing,
            clippy::same_name_method,
            clippy::iter_without_into_iter,
        )]
        const _: () = {
            // Declared in a "hidden" scope that can't be reached directly
            // These types don't appear in the end-user's API
            $crate::__declare_internal_bitflags! {
                $vis struct InternalBitFlags: $T
            }

            $crate::__impl_internal_bitflags! {
                InternalBitFlags: $T, $BitFlags {
                    $(
                        $(#[$inner $($args)*])*
                        const $Flag = $value;
                    )*
                }
            }

            // This is where new library trait implementations can be added
            $crate::__impl_external_bitflags! {
                InternalBitFlags: $T, $BitFlags {
                    $(
                        $(#[$inner $($args)*])*
                        const $Flag;
                    )*
                }
            }

            $crate::__impl_public_bitflags_forward! {
                $BitFlags: $T, InternalBitFlags
            }

            $crate::__impl_public_bitflags_ops! {
                $BitFlags
            }

            $crate::__impl_public_bitflags_iter! {
                $BitFlags: $T, $BitFlags
            }
        };

        $crate::bitflags! {
            $($t)*
        }
    };
    (
        impl $BitFlags:ident: $T:ty {
            $(
                $(#[$inner:ident $($args:tt)*])*
                const $Flag:tt = $value:expr;
            )*
        }

        $($t:tt)*
    ) => {
        $crate::__impl_public_bitflags_consts! {
            $BitFlags: $T {
                $(
                    $(#[$inner $($args)*])*
                    const $Flag = $value;
                )*
            }
        }

        #[allow(
            dead_code,
            deprecated,
            unused_doc_comments,
            unused_attributes,
            unused_mut,
            unused_imports,
            non_upper_case_globals,
            clippy::assign_op_pattern,
            clippy::iter_without_into_iter,
        )]
        const _: () = {
            $crate::__impl_public_bitflags! {
                $BitFlags: $T, $BitFlags {
                    $(
                        $(#[$inner $($args)*])*
                        const $Flag = $value;
                    )*
                }
            }

            $crate::__impl_public_bitflags_ops! {
                $BitFlags
            }

            $crate::__impl_public_bitflags_iter! {
                $BitFlags: $T, $BitFlags
            }
        };

        $crate::bitflags! {
            $($t)*
        }
    };
    () => {};
}

/// Implement functions on bitflags types.
///
/// We need to be careful about adding new methods and trait implementations here because they
/// could conflict with items added by the end-user.
#[macro_export]
#[doc(hidden)]
macro_rules! __impl_bitflags {
    (
        $PublicBitFlags:ident: $T:ty {
            fn empty() $empty:block
            fn all() $all:block
            fn bits($bits0:ident) $bits:block
            fn from_bits($from_bits0:ident) $from_bits:block
            fn from_bits_truncate($from_bits_truncate0:ident) $from_bits_truncate:block
            fn from_bits_retain($from_bits_retain0:ident) $from_bits_retain:block
            fn from_name($from_name0:ident) $from_name:block
            fn is_empty($is_empty0:ident) $is_empty:block
            fn is_all($is_all0:ident) $is_all:block
            fn intersects($intersects0:ident, $intersects1:ident) $intersects:block
            fn contains($contains0:ident, $contains1:ident) $contains:block
            fn insert($insert0:ident, $insert1:ident) $insert:block
            fn remove($remove0:ident, $remove1:ident) $remove:block
            fn toggle($toggle0:ident, $toggle1:ident) $toggle:block
            fn set($set0:ident, $set1:ident, $set2:ident) $set:block
            fn intersection($intersection0:ident, $intersection1:ident) $intersection:block
            fn union($union0:ident, $union1:ident) $union:block
            fn difference($difference0:ident, $difference1:ident) $difference:block
            fn symmetric_difference($symmetric_difference0:ident, $symmetric_difference1:ident) $symmetric_difference:block
            fn complement($complement0:ident) $complement:block
        }
    ) => {
        #[allow(dead_code, deprecated, unused_attributes)]
        impl $PublicBitFlags {
            /// Get a flags value with all bits unset.
            #[inline]
            pub const fn empty() -> Self {
                $empty
            }

            /// Get a flags value with all known bits set.
            #[inline]
            pub const fn all() -> Self {
                $all
            }

            /// Get the underlying bits value.
            ///
            /// The returned value is exactly the bits set in this flags value.
            #[inline]
            pub const fn bits(&self) -> $T {
                let $bits0 = self;
                $bits
            }

            /// Convert from a bits value.
            ///
            /// This method will return `None` if any unknown bits are set.
            #[inline]
            pub const fn from_bits(bits: $T) -> $crate::__private::core::option::Option<Self> {
                let $from_bits0 = bits;
                $from_bits
            }

            /// Convert from a bits value, unsetting any unknown bits.
            #[inline]
            pub const fn from_bits_truncate(bits: $T) -> Self {
                let $from_bits_truncate0 = bits;
                $from_bits_truncate
            }

            /// Convert from a bits value exactly.
            #[inline]
            pub const fn from_bits_retain(bits: $T) -> Self {
                let $from_bits_retain0 = bits;
                $from_bits_retain
            }

            /// Get a flags value with the bits of a flag with the given name set.
            ///
            /// This method will return `None` if `name` is empty or doesn't
            /// correspond to any named flag.
            #[inline]
            pub fn from_name(name: &str) -> $crate::__private::core::option::Option<Self> {
                let $from_name0 = name;
                $from_name
            }

            /// Whether all bits in this flags value are unset.
            #[inline]
            pub const fn is_empty(&self) -> bool {
                let $is_empty0 = self;
                $is_empty
            }

            /// Whether all known bits in this flags value are set.
            #[inline]
            pub const fn is_all(&self) -> bool {
                let $is_all0 = self;
                $is_all
            }

            /// Whether any set bits in a source flags value are also set in a target flags value.
            #[inline]
            pub const fn intersects(&self, other: Self) -> bool {
                let $intersects0 = self;
                let $intersects1 = other;
                $intersects
            }

            /// Whether all set bits in a source flags value are also set in a target flags value.
            #[inline]
            pub const fn contains(&self, other: Self) -> bool {
                let $contains0 = self;
                let $contains1 = other;
                $contains
            }

            /// The bitwise or (`|`) of the bits in two flags values.
            #[inline]
            pub fn insert(&mut self, other: Self) {
                let $insert0 = self;
                let $insert1 = other;
                $insert
            }

            /// The intersection of a source flags value with the complement of a target flags value (`&!`).
            ///
            /// This method is not equivalent to `self & !other` when `other` has unknown bits set.
            /// `remove` won't truncate `other`, but the `!` operator will.
            #[inline]
            pub fn remove(&mut self, other: Self) {
                let $remove0 = self;
                let $remove1 = other;
                $remove
            }

            /// The bitwise exclusive-or (`^`) of the bits in two flags values.
            #[inline]
            pub fn toggle(&mut self, other: Self) {
                let $toggle0 = self;
                let $toggle1 = other;
                $toggle
            }

            /// Call `insert` when `value` is `true` or `remove` when `value` is `false`.
            #[inline]
            pub fn set(&mut self, other: Self, value: bool) {
                let $set0 = self;
                let $set1 = other;
                let $set2 = value;
                $set
            }

            /// The bitwise and (`&`) of the bits in two flags values.
            #[inline]
            #[must_use]
            pub const fn intersection(self, other: Self) -> Self {
                let $intersection0 = self;
                let $intersection1 = other;
                $intersection
            }

            /// The bitwise or (`|`) of the bits in two flags values.
            #[inline]
            #[must_use]
            pub const fn union(self, other: Self) -> Self {
                let $union0 = self;
                let $union1 = other;
                $union
            }

            /// The intersection of a source flags value with the complement of a target flags value (`&!`).
            ///
            /// This method is not equivalent to `self & !other` when `other` has unknown bits set.
            /// `difference` won't truncate `other`, but the `!` operator will.
            #[inline]
            #[must_use]
            pub const fn difference(self, other: Self) -> Self {
                let $difference0 = self;
                let $difference1 = other;
                $difference
            }

            /// The bitwise exclusive-or (`^`) of the bits in two flags values.
            #[inline]
            #[must_use]
            pub const fn symmetric_difference(self, other: Self) -> Self {
                let $symmetric_difference0 = self;
                let $symmetric_difference1 = other;
                $symmetric_difference
            }

            /// The bitwise negation (`!`) of the bits in a flags value, truncating the result.
            #[inline]
            #[must_use]
            pub const fn complement(self) -> Self {
                let $complement0 = self;
                $complement
            }
        }
    };
}

/// A macro that processed the input to `bitflags!` and shuffles attributes around
/// based on whether or not they're "expression-safe".
///
/// This macro is a token-tree muncher that works on 2 levels:
///
/// For each attribute, we explicitly match on its identifier, like `cfg` to determine
/// whether or not it should be considered expression-safe.
///
/// If you find yourself with an attribute that should be considered expression-safe
/// and isn't, it can be added here.
#[macro_export]
#[doc(hidden)]
macro_rules! __bitflags_expr_safe_attrs {
    // Entrypoint: Move all flags and all attributes into `unprocessed` lists
    // where they'll be munched one-at-a-time
    (
        $(#[$inner:ident $($args:tt)*])*
        { $e:expr }
    ) => {
        $crate::__bitflags_expr_safe_attrs! {
            expr: { $e },
            attrs: {
                // All attributes start here
                unprocessed: [$(#[$inner $($args)*])*],
                // Attributes that are safe on expressions go here
                processed: [],
            },
        }
    };
    // Process the next attribute on the current flag
    // `cfg`: The next flag should be propagated to expressions
    // NOTE: You can copy this rules block and replace `cfg` with
    // your attribute name that should be considered expression-safe
    (
        expr: { $e:expr },
            attrs: {
            unprocessed: [
                // cfg matched here
                #[cfg $($args:tt)*]
                $($attrs_rest:tt)*
            ],
            processed: [$($expr:tt)*],
        },
    ) => {
        $crate::__bitflags_expr_safe_attrs! {
            expr: { $e },
            attrs: {
                unprocessed: [
                    $($attrs_rest)*
                ],
                processed: [
                    $($expr)*
                    // cfg added here
                    #[cfg $($args)*]
                ],
            },
        }
    };
    // Process the next attribute on the current flag
    // `$other`: The next flag should not be propagated to expressions
    (
        expr: { $e:expr },
            attrs: {
            unprocessed: [
                // $other matched here
                #[$other:ident $($args:tt)*]
                $($attrs_rest:tt)*
            ],
            processed: [$($expr:tt)*],
        },
    ) => {
        $crate::__bitflags_expr_safe_attrs! {
            expr: { $e },
                attrs: {
                unprocessed: [
                    $($attrs_rest)*
                ],
                processed: [
                    // $other not added here
                    $($expr)*
                ],
            },
        }
    };
    // Once all attributes on all flags are processed, generate the actual code
    (
        expr: { $e:expr },
        attrs: {
            unprocessed: [],
            processed: [$(#[$expr:ident $($exprargs:tt)*])*],
        },
    ) => {
        $(#[$expr $($exprargs)*])*
        { $e }
    }
}

/// Implement a flag, which may be a wildcard `_`.
#[macro_export]
#[doc(hidden)]
macro_rules! __bitflags_flag {
    (
        {
            name: _,
            named: { $($named:tt)* },
            unnamed: { $($unnamed:tt)* },
        }
    ) => {
        $($unnamed)*
    };
    (
        {
            name: $Flag:ident,
            named: { $($named:tt)* },
            unnamed: { $($unnamed:tt)* },
        }
    ) => {
        $($named)*
    };
}

#[macro_use]
mod public;
#[macro_use]
mod internal;
#[macro_use]
mod external;

#[cfg(feature = "example_generated")]
pub mod example_generated;

#[cfg(test)]
mod tests;
