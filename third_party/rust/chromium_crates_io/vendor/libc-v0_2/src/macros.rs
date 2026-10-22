/// A macro for defining #[cfg] if-else statements.
///
/// This is similar to the `if/elif` C preprocessor macro by allowing definition of a cascade of
/// `#[cfg]` cases, emitting the implementation which matches first.
///
/// This allows you to conveniently provide a long list #[cfg]'d blocks of code without having to
/// rewrite each clause multiple times.
macro_rules! cfg_if {
    // match if/else chains with a final `else`
    ($(
        if #[cfg($($meta:meta),*)] { $($it:item)* }
    ) else * else {
        $($it2:item)*
    }) => {
        cfg_if! {
            @__items
            () ;
            $( ( ($($meta),*) ($($it)*) ), )*
            ( () ($($it2)*) ),
        }
    };

    // match if/else chains lacking a final `else`
    (
        if #[cfg($($i_met:meta),*)] { $($i_it:item)* }
        $(
            else if #[cfg($($e_met:meta),*)] { $($e_it:item)* }
        )*
    ) => {
        cfg_if! {
            @__items
            () ;
            ( ($($i_met),*) ($($i_it)*) ),
            $( ( ($($e_met),*) ($($e_it)*) ), )*
            ( () () ),
        }
    };

    // Internal and recursive macro to emit all the items
    //
    // Collects all the negated `cfg`s in a list at the beginning and after the
    // semicolon is all the remaining items
    (@__items ($($not:meta,)*) ; ) => {};
    (@__items ($($not:meta,)*) ; ( ($($m:meta),*) ($($it:item)*) ),
     $($rest:tt)*) => {
        // Emit all items within one block, applying an appropriate #[cfg]. The
        // #[cfg] will require all `$m` matchers specified and must also negate
        // all previous matchers.
        cfg_if! { @__apply cfg(all($($m,)* not(any($($not),*)))), $($it)* }

        // Recurse to emit all other items in `$rest`, and when we do so add all
        // our `$m` matchers to the list of `$not` matchers as future emissions
        // will have to negate everything we just matched as well.
        cfg_if! { @__items ($($not,)* $($m,)*) ; $($rest)* }
    };

    // Internal macro to Apply a cfg attribute to a list of items
    (@__apply $m:meta, $($it:item)*) => {
        $(#[$m] $it)*
    };
}

/// Create an internal crate prelude with `core` reexports and common types.
macro_rules! prelude {
    () => {
        mod types;

        /// Frequently-used types that are available on all platforms
        ///
        /// We need to reexport the core types so this works with `rust-dep-of-std`.
        mod prelude {
            // Exports from `core`
            #[allow(unused_imports)]
            pub(crate) use core::clone::Clone;
            #[allow(unused_imports)]
            pub(crate) use core::cmp::{
                Eq,
                PartialEq,
            };
            #[allow(unused_imports)]
            pub(crate) use core::default::Default;
            #[allow(unused_imports)]
            pub(crate) use core::iter::Iterator;
            #[allow(unused_imports)]
            pub(crate) use core::marker::{
                Copy,
                Send,
                Sync,
            };
            #[allow(unused_imports)]
            pub(crate) use core::option::Option::{
                self,
                None,
                Some,
            };
            #[allow(unused_imports)]
            pub(crate) use core::prelude::v1::derive;
            #[allow(unused_imports)]
            pub(crate) use core::{
                assert,
                cfg,
                compile_error,
                debug_assert,
                fmt,
                hash,
                iter,
                mem,
                panic,
                ptr,
                unimplemented,
            };

            #[allow(unused_imports)]
            pub(crate) use fmt::Debug;
            #[allow(unused_imports)]
            pub(crate) use mem::{
                align_of,
                align_of_val,
                size_of,
                size_of_val,
            };

            #[allow(unused_imports)]
            #[cfg(any(target_os = "linux", target_os = "android", target_os = "l4re"))]
            pub(crate) use crate::types::u32_cast_ioctl;
            #[allow(unused_imports)]
            pub(crate) use crate::types::{
                cstr,
                replace_array_items,
                u16_cast_short,
                u32_cast_int,
                u32_cast_long,
                u8_slice_cast_char_slice,
                ulong_cast_int,
                ulong_cast_uint,
                CEnumRepr,
                Padding,
            };
            // Commonly used types defined in this crate
            #[allow(unused_imports)]
            pub(crate) use crate::{
                c_char,
                c_double,
                c_float,
                c_int,
                c_long,
                c_longlong,
                c_short,
                c_uchar,
                c_uint,
                c_ulong,
                c_ulonglong,
                c_ushort,
                c_void,
                intptr_t,
                size_t,
                ssize_t,
                uintptr_t,
            };
        }
    };
}

/// Implement `Clone`, `Copy`, and `Debug` for one or more structs, as well as `PartialEq`, `Eq`,
/// and `Hash` if the `extra_traits` feature is enabled.
///
/// Also mark the type with `repr(C)`.
///
/// Use [`s_no_extra_traits`] for structs where the `extra_traits` feature does not make sense, and
/// for unions.
macro_rules! s {
    ($(
        $(#[$attr:meta])*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s!(it: $(#[$attr])* $pub $t $i { $($field)* });
    )*);

    (it: $(#[$attr:meta])* $pub:vis union $i:ident { $($field:tt)* }) => (
        compile_error!("unions cannot derive extra traits, use s_no_extra_traits instead");
    );

    (it: $(#[$attr:meta])* $pub:vis struct $i:ident { $($field:tt)* }) => (
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(
                ::core::cmp::PartialEq,
                ::core::cmp::Eq,
                ::core::hash::Hash,
            )
        )]
        #[allow(deprecated)]
        $(#[$attr])*
        $pub struct $i { $($field)* }
    );
}

/// Implement `Clone`, `Copy`, and `Debug` for a tuple struct, as well as `PartialEq`, `Eq`, and
/// `Hash` if the `extra_traits` feature is enabled.
///
/// Unlike `s!`, this does *not* mark the type with `repr(C)`. Users should provide their own
/// `repr` attribute via `$attr` as necessary.
macro_rules! s_paren {
    ($(
        $(#[$attr:meta])*
        $pub:vis struct $i:ident ( $($field:tt)* );
    )*) => ($(
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(
                ::core::cmp::PartialEq,
                ::core::cmp::Eq,
                ::core::hash::Hash,
            )
        )]
        $(#[$attr])*
        $pub struct $i ( $($field)* );
    )*);
}

/// Implement `Clone`, `Copy`, and `Debug` for one or more structs/unions, but exclude `PartialEq`,
/// `Eq`, and `Hash`.
///
/// Also mark the type with `repr(C)`.
///
/// Most structs will prefer to use [`s`].
macro_rules! s_no_extra_traits {
    ($(
        $(#$attr:tt)*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s_no_extra_traits!(it: $(#$attr)* $pub $t $i { $($field)* });
    )*);

    (it: $(#$attr:tt)* $pub:vis union $i:ident { $($field:tt)* }) => (
        union_with_debug! {
            $(#$attr)* $pub union $i { $($field)* }
        }
    );

    (it: $(#$attr:tt)* $pub:vis struct $i:ident { $($field:tt)* }) => (
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        $(#$attr)*
        $pub struct $i { $($field)* }
    );
}

/// Like [`s`], but (1) generates a `Default` impl for every struct in the block, and (2) adds a
/// private field to the struct to replicate the effects of the `non_exhaustive` attribute while
/// rust-lang/rust#132699 gets sorted out.
///
/// To opt out of having the private field added, annotate the struct with an `exhaustive`
/// attribute, as in:
///
/// ```ignore
/// s2! {
///     #[exhaustive]
///     struct Something {
///         ...
///     }
/// }
/// ```
///
/// See [`custom_struct`] for details.
macro_rules! s2 {
    ($(
        $(#$attr:tt)*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s2!(it: $(#$attr)* $pub $t $i { $($field)* });
    )*);

    (it: $(#$attr:tt)* $pub:vis union $i:ident { $($field:tt)* }) => (
        compile_error!(
            "unions cannot derive extra traits, use `s_no_extra_traits2` \
             instead"
        );
    );

    (it: $(#$attr:tt)* $pub:vis struct $i:ident { $($field:tt)* }) => (
        custom_struct! {
            attrs: {
                #[repr(C)]
                #[::core::prelude::v1::derive(
                    ::core::clone::Clone,
                    ::core::marker::Copy,
                    ::core::fmt::Debug,
                )]
                #[cfg_attr(
                    feature = "extra_traits",
                    ::core::prelude::v1::derive(
                        ::core::cmp::PartialEq,
                        ::core::cmp::Eq,
                        ::core::hash::Hash,
                    )
                )]
                #[allow(deprecated)]
            }
            $(#$attr)* $pub struct $i { $($field)* }
        }
    );
}

/// Like [`s_no_extra_traits`], but (1) generates a `Default` impl for every struct in the block,
/// and (2) adds a private field to replicate the effects of the built-in `non_exhaustive` attribute
/// while rust-lang/rust#132699 gets sorted out.
///
/// Unions are emitted just like `s_no_extra_traits!` does, with no `Default`.  A struct field of
/// union type supplies its own default via `#[custom_default(...)]`.
///
/// To opt out of having the private field added, annotate the struct with an `exhaustive`
/// attribute, as in:
///
/// ```ignore
/// s_no_extra_traits2! {
///     #[exhaustive]
///     struct Something {
///         ...
///     }
/// }
/// ```
macro_rules! s_no_extra_traits2 {
    ($(
        $(#$attr:tt)*
        $pub:vis $t:ident $i:ident { $($field:tt)* }
    )*) => ($(
        s_no_extra_traits2!(it: $(#$attr)* $pub $t $i { $($field)* });
    )*);

    (it: $(#$attr:tt)* $pub:vis union $i:ident { $($field:tt)* }) => (
        union_with_debug! {
            $(#$attr)* $pub union $i { $($field)* }
        }
    );

    (it: $(#$attr:tt)* $pub:vis struct $i:ident { $($field:tt)* }) => (
        custom_struct! {
            attrs: {
                #[repr(C)]
                #[::core::prelude::v1::derive(
                    ::core::clone::Clone,
                    ::core::marker::Copy,
                    ::core::fmt::Debug,
                )]
            }
            $(#$attr)* $pub struct $i { $($field)* }
        }
    );
}

/// Emit a union plus its `Debug` impl.
///
/// Unions can't derive `Debug`, so it is written out here. Attributes are split like
/// [`custom_struct`] does. Everything goes on the union, but only the `cfg`s are repeated on the
/// impl, otherwise a union that is configured out leaves an impl behind.
macro_rules! union_with_debug {
    (
        $(#$attr:tt)*
        $vis:vis union $name:ident { $($body:tt)* }
    ) => {
        union_with_debug! {
            @split_attrs,
            cfg_attrs: { },
            other_attrs: { },
            remaining_attrs: { $(#$attr)* },
            found_default_via_unsafe_zeroed: false,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // a `cfg` also has to gate the impl
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #[cfg($($cfg:tt)*)]
            $($tail:tt)*
        },
        found_default_via_unsafe_zeroed: $found_default:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        union_with_debug! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* #[cfg($($cfg)*)] },
            other_attrs: { $($other_attrs)* },
            remaining_attrs: { $($tail)* },
            found_default_via_unsafe_zeroed: $found_default,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // a `cfg` also has to gate the impl
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #[unsafe(union_default_via_zeroed)]
            $($tail:tt)*
        },
        found_default_via_unsafe_zeroed: $found_default:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        union_with_debug! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* },
            remaining_attrs: { $($tail)* },
            found_default_via_unsafe_zeroed: true,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // anything else belongs to the union only
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #$other:tt
            $($tail:tt)*
        },
        found_default_via_unsafe_zeroed: $found_default:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        union_with_debug! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* #$other },
            remaining_attrs: { $($tail)* },
            found_default_via_unsafe_zeroed: $found_default,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // done
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: { },
        found_default_via_unsafe_zeroed: $found_default:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        #[repr(C)]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
        )]
        $($other_attrs)*
        $($cfg_attrs)*
        $vis union $name { $($body)* }

        $($cfg_attrs)*
        #[allow(deprecated)]
        impl ::core::fmt::Debug for $name {
            fn fmt(&self, f: &mut ::core::fmt::Formatter<'_>) -> ::core::fmt::Result {
                f.debug_struct(::core::stringify!($name)).finish_non_exhaustive()
            }
        }

        emit_union_default_via_unsafe_zeroed! {
            found_default_via_unsafe_zeroed: $found_default,
            cfg_attrs: { $($cfg_attrs)* },
            name: $name,
        }
    };
}

/// Add a `Default` impl that is just `mem::zeroed`.
macro_rules! emit_union_default_via_unsafe_zeroed {
    (
        found_default_via_unsafe_zeroed: false,
        cfg_attrs: { $($cfg_attrs:tt)* },
        name: $name:ident,
    ) => {
    };

    (
        found_default_via_unsafe_zeroed: true,
        cfg_attrs: { $($cfg_attrs:tt)* },
        name: $name:ident,
    ) => {
        $($cfg_attrs)*
        impl ::core::default::Default for $name {
            fn default() -> Self {
                unsafe { ::core::mem::zeroed() }
            }
        }
    };
}

/// Emit a struct with the given derive attributes plus a generated `Default` impl. Ensure that the
/// record has an additional private field added to replicate `#[non_exhaustive]`, unless it is
/// annotated with `#[exhaustive]`.
///
/// Fields default to `Default::default()`. A field whose default can't be derived must carry
/// `#[custom_default(EXPR)]` as its *first* attribute, and `EXPR` is used instead.
///
/// This works by scanning each field for `#[custom_default]` attributes. If one exists, the
/// attribute's contents are added to `processed_field_defaults` and will be used in the expansion
/// for `Default`. If it does not exist, `Default::default()` is used instead. In either case, the
/// field is added to `processed_fields` with `#[custom_default]` stripped if necessary, and
/// `custom_struct` is invoked again with the remaining fields.
///
/// Attributes are split into `cfg_attrs` and `other_attrs` before the fields are scanned. Both go
/// on the struct, but only the `cfg`s are repeated on the `Default` impl. A `cfg` decides whether
/// the type exists at all, so without it a configured-out struct leaves an impl behind referring to
/// a type that isn't there.
///
/// Out of `other_attrs`, we scan for `#[exhaustive]`. If found, we remove it but take into account
/// that the record should be expanded _without_ an additional private field. The scan for both
/// `cfg` attributes and the (non-existent) `exhaustive` attribute is done in one linear pass.
macro_rules! custom_struct {
    // entry; `attrs` is the attribute block the caller wants on the struct
    // (repr, derives, etc.), which is merged with the struct's own attributes.
    (
        attrs: { $($attrs:tt)* }
        $(#$attr:tt)*
        $vis:vis struct $name:ident { $($body:tt)* }
    ) => {
        custom_struct! {
            @split_attrs,
            cfg_attrs: { },
            other_attrs: { },
            remaining_attrs: { $($attrs)* $(#$attr)* },
            found_exhaustive_attr: false,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // a `cfg` also has to gate the impl
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #[cfg($($cfg:tt)*)]
            $($tail:tt)*
        },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        custom_struct! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* #[cfg($($cfg)*)] },
            other_attrs: { $($other_attrs)* },
            remaining_attrs: { $($tail)* },
            found_exhaustive_attr: $found_exhaustive,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // `exhaustive` must be taken into account as many times as it appears,
    // though the effect is the same with a single annotation.
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #[exhaustive]
            $($tail:tt)*
        },
        found_exhaustive_attr: $_:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        custom_struct! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* },
            remaining_attrs: { $($tail)* },
            found_exhaustive_attr: true,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // anything else belongs to the struct only
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: {
            #$other:tt
            $($tail:tt)*
        },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        custom_struct! {
            @split_attrs,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* #$other },
            remaining_attrs: { $($tail)* },
            found_exhaustive_attr: $found_exhaustive,
            vis: $vis,
            name: $name,
            body: { $($body)* },
        }
    };

    // attributes are split, move on to the fields
    (
        @split_attrs,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        remaining_attrs: { },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        body: { $($body:tt)* },
    ) => {
        custom_struct! {
            @struct,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* },
            found_exhaustive_attr: $found_exhaustive,
            vis: $vis,
            name: $name,
            processed_fields: { },
            processed_field_defaults: { },
            remaining_fields: { $($body)* },
        }
    };

    // field led by #[custom_default(...)]
    (
        @struct,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        processed_fields: { $($processed_fields:tt)* },
        processed_field_defaults: { $($processed_field_defaults:tt)* },
        remaining_fields: {
            #[custom_default($default:expr)]
            $(#[$fattr:meta])*
            $fvis:vis $fname:ident: $fty:ty,
            $($tail:tt)*
        },
    ) => {
        custom_struct! {
            @struct,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* },
            found_exhaustive_attr: $found_exhaustive,
            vis: $vis,
            name: $name,
            processed_fields: { $($processed_fields)* $(#[$fattr])* $fvis $fname: $fty, },
            processed_field_defaults: {
                $($processed_field_defaults)*
                $(#[$fattr])* $fname: $default,
            },
            remaining_fields: { $($tail)* },
        }
    };

    // plain field
    (
        @struct,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        processed_fields: { $($processed_fields:tt)* },
        processed_field_defaults: { $($processed_field_defaults:tt)* },
        remaining_fields: {
            $(#[$fattr:meta])*
            $fvis:vis $fname:ident: $fty:ty,
            $($tail:tt)*
        },
    ) => {
        custom_struct! {
            @struct,
            cfg_attrs: { $($cfg_attrs)* },
            other_attrs: { $($other_attrs)* },
            found_exhaustive_attr: $found_exhaustive,
            vis: $vis,
            name: $name,
            processed_fields: { $($processed_fields)* $(#[$fattr])* $fvis $fname: $fty, },
            processed_field_defaults: {
                $($processed_field_defaults)*
                $(#[$fattr])* $fname: ::core::default::Default::default(),
            },
            remaining_fields: { $($tail)* },
        }
    };

    // done
    (
        @struct,
        cfg_attrs: { $($cfg_attrs:tt)* },
        other_attrs: { $($other_attrs:tt)* },
        found_exhaustive_attr: $found_exhaustive:tt,
        vis: $vis:vis,
        name: $name:ident,
        processed_fields: { $($processed_fields:tt)* },
        processed_field_defaults: { $($processed_field_defaults:tt)* },
        remaining_fields: { },
    ) => {
        emit_struct_definition! {
            found_exhaustive_attr: $found_exhaustive,
            body: {
                $($other_attrs)*
                $($cfg_attrs)*
                $vis $name { $($processed_fields)* }
            }
        }

        $($cfg_attrs)*
        // The impl names the type and its fields, which warns if either is
        // deprecated.
        #[allow(deprecated)]
        impl ::core::default::Default for $name {
            // Field attributes (`#[cfg]`, doc comments) get forwarded to the
            // initializer too. Docs are harmless there but trip the lint, so
            // silence it.
            #[allow(unused_doc_comments)]
            fn default() -> Self {
                emit_struct_default_body! {
                    found_exhaustive_attr: $found_exhaustive,
                    body: { $($processed_field_defaults)* }
                }
            }
        }
    };
}

/// Expands the definition of the record defined at [`custom_struct`], with either one of an
/// additional private field or with its verbatim fields.
macro_rules! emit_struct_definition {
    (
        found_exhaustive_attr: false,
        body: { $(#[$attr:meta])* $vis:vis $name:ident { $($field:tt)* } }
    ) => {
        #[allow(clippy::manual_non_exhaustive)]
        $(#[$attr])*
        $vis struct $name { $($field)* __non_exhaustive: () }
    };

    (
        found_exhaustive_attr: true,
        body: { $(#[$attr:meta])* $vis:vis $name:ident { $($field:tt)* } }
    ) => {
        $(#[$attr])*
        $vis struct $name { $($field)* }
    };
}

/// Expands the `Default` implementation of the record defined at [`custom_struct`], with either one
/// of an additional private field initialized to the unit value, or the record's fields verbatim.
macro_rules! emit_struct_default_body {
    (
        found_exhaustive_attr: false,
        body: { $($field_default:tt)* }
    ) => {
        Self { $($field_default)* __non_exhaustive: () }
    };

    (
        found_exhaustive_attr: true,
        body: { $($field_default:tt)* }
    ) => {
        Self { $($field_default)* }
    };
}

/// Create an uninhabited type that can't be constructed. It implements `Debug`, `Clone`, and
/// `Copy`, but these aren't meaningful for extern types so they should eventually be removed.
///
/// Really what we want here is something that also can't be named without indirection (in ADTs or
/// function signatures), but this doesn't exist.
macro_rules! extern_ty {
    ($(
        $(#[$attr:meta])*
        $vis:vis type $i:ident;
    )*) => ($(
        $(#[$attr])*
        /// This is an extern type ("opaque" or "incomplete" type in C).
        ///
        /// <div class="warning">
        /// This type's current representation allows inspecting some properties, such as via
        /// <code>size_of</code>, and it is technically possible to construct the type within
        /// <code>MaybeUninit</code>, However, this <strong>MUST NOT</strong> be relied upon
        /// because a future version of <code>libc</code> may switch to a proper
        /// <a href="https://rust-lang.github.io/rfcs/1861-extern-types.html">extern type</a>
        /// representation when available.
        /// </div>
        // ^ unfortunately warning blocks currently don't render markdown so we need to
        // use raw HTML.
        //
        // Representation based on the Nomicon:
        // <https://doc.rust-lang.org/nomicon/ffi.html#representing-opaque-structs>.
        //
        // FIXME(1.0): These traits are unreachable and should be removed.
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        #[repr(C)]
        $vis struct $i {
            _data: (),
            _marker: ::core::marker::PhantomData<(*mut u8, ::core::marker::PhantomPinned)>,
        }
    )*);
}

/// Implement `Clone` and `Copy` for an enum, as well as `Debug`, `Eq`, `Hash`, and
/// `PartialEq` if the `extra_traits` feature is enabled.
// FIXME(#4419): Replace all uses of `e!` with `c_enum!`
macro_rules! e {
    ($(
        $(#[$attr:meta])*
        pub enum $i:ident { $($field:tt)* }
    )*) => ($(
        #[cfg_attr(
            feature = "extra_traits",
            ::core::prelude::v1::derive(Eq, Hash, PartialEq)
        )]
        #[::core::prelude::v1::derive(
            ::core::clone::Clone,
            ::core::marker::Copy,
            ::core::fmt::Debug,
        )]
        $(#[$attr])*
        pub enum $i { $($field)* }
    )*);
}

/// Represent a C enum as Rust constants and a type.
///
/// C enums can't soundly be mapped to Rust enums since C enums are allowed to have duplicates or
/// unlisted values, but this is UB in Rust. This enum doesn't implement any traits, its main
/// purpose is to calculate the correct enum values.
///
/// Use the magic name `#anon` if the C enum doesn't create a type.
///
/// See <https://github.com/rust-lang/libc/issues/4419> for more.
macro_rules! c_enum {
    // Matcher for multiple enums
    ($(
        $(#[repr($repr:ty)])?
        $vis:vis enum $($ty_name:ident)? $(#$anon:ident)? {
            $(
                $(#[$meta:meta])*
                $field_vis:vis $variant:ident $(= $value:expr)?,
            )+
        }
    )+) => {
        $(c_enum!(@single;
            $(#[repr($repr)])?
            $vis enum $($ty_name)? $(#$anon)? {
                $(
                    $(#[$meta])*
                    $field_vis $variant $(= $value)?,
                )+
            }
        );)+
    };

    // Matcher for a single enum
    (@single;
        $(#[repr($repr:ty)])?
        $vis:vis enum $ty_name:ident {
            $(
                $(#[$meta:meta])*
                $field_vis:vis $variant:ident $(= $value:expr)?,
            )+
        }
    ) => {
        $vis type $ty_name = c_enum!(@ty $($repr)?);
        c_enum! {
            @variant;
            ty: $ty_name;
            default: 0;
            variants: [$(
                $(#[$meta])*
                $field_vis $variant $(= $value)?,
            )+]
        }
    };

    // Matcher for a single anonymous enum
    (@single;
        $(#[repr($repr:ty)])?
        $vis:vis enum #anon {
            $(
                $(#[$meta:meta])*
                $field_vis:vis $variant:ident $(= $value:expr)?,
            )+
        }
    ) => {
        c_enum! {
            @variant;
            ty: c_enum!(@ty $($repr)?);
            default: 0;
            variants: [$(
                $(#[$meta])*
                $field_vis $variant $(= $value)?,
            )+]
        }
    };

    // Matcher for variants: eats a single variant then recurses with the rest
    (@variant;
        ty: $_ty_name:ty;
        default: $_idx:expr;
        variants: []
    ) => { /* end of the chain */ };
    (
        @variant;
        ty: $ty_name:ty;
        default: $default_val:expr;
        variants: [
            $(#[$meta:meta])*
            $field_vis:vis $variant:ident $(= $value:expr)?,
            $($tail:tt)*
        ]
    ) => {
        $(#[$meta])*
        #[allow(deprecated)]
        $field_vis const $variant: $ty_name = {
            #[allow(unused_variables)]
            let r = $default_val;
            $(let r = $value;)?
            r
        };

        // The next value is always one more than the previous value, unless
        // set explicitly.
        c_enum! {
            @variant;
            ty: $ty_name;
            default: $variant + 1;
            variants: [$($tail)*]
        }
    };

    // Use a specific type if provided, otherwise default to `CEnumRepr`
    (@ty $repr:ty) => { $repr };
    (@ty) => { $crate::prelude::CEnumRepr };
}

/// Define a function that can be either `safe` or `unsafe` and optionally `const`. This always
/// marks the function inline and adds `extern "C"`.
macro_rules! f {
    ($(
        $(#[$attr:meta])*
        pub $(const $($const_dummy:literal)?)?
        $(unsafe $($unsafe_dummy:literal)?)? $(safe $($safe_dummy:literal)?)?
        fn $i:ident ($($arg:ident: $argty:ty),* $(,)?) -> $ret:ty
            $body:block
    )+) => {$(
        f! {
            @single
            $(#[$attr])*
            pub $(const $($const_dummy)?)?
            $(unsafe $($unsafe_dummy)?)? $(safe $($safe_dummy)?)?
            fn $i ($($arg: $argty),*) -> $ret
                $body
        }
    )+};

    (@single
        $(#[$attr:meta])*
        pub $(const $($const_dummy:literal)?)? unsafe
        fn $i:ident ($($arg:ident: $argty:ty),* $(,)?) -> $ret:ty
            $body:block
    ) => {
        #[inline]
        $(#[$attr])*
        pub $(const $($const_dummy)?)? unsafe extern "C"
        fn $i ($($arg: $argty),*) -> $ret
            $body
    };

    (@single
        $(#[$attr:meta])*
        pub $(const $($const_dummy:literal)?)? safe
        fn $i:ident ($($arg:ident: $argty:ty),* $(,)?) -> $ret:ty
            $body:block
    ) => {
        #[inline]
        $(#[$attr])*
        pub $(const $($const_dummy)?)? extern "C"
        fn $i ($($arg: $argty),*) -> $ret
            $body
    };

    (@single
        pub $(const $($const_dummy:literal)?)?
        fn $i:ident ($($arg:ident: $argty:ty),* $(,)?) -> $ret:ty
            $body:block
    ) => {
        compile_error!("either `safe` or `unsafe` must be specified");
    };
}

// This macro is used to deprecate items that should be accessed via the mach2 crate
macro_rules! deprecated_mach {
    (pub const $id:ident: $ty:ty = $expr:expr;) => {
        #[deprecated(
            since = "0.2.55",
            note = "Use the `mach2` crate instead",
        )]
        #[allow(deprecated)]
        pub const $id: $ty = $expr;
    };
    ($(pub const $id:ident: $ty:ty = $expr:expr;)*) => {
        $(
            deprecated_mach!(
                pub const $id: $ty = $expr;
            );
        )*
    };
    (pub type $id:ident = $ty:ty;) => {
        #[deprecated(
            since = "0.2.55",
            note = "Use the `mach2` crate instead",
        )]
        #[allow(deprecated)]
        pub type $id = $ty;
    };
    ($(pub type $id:ident = $ty:ty;)*) => {
        $(
            deprecated_mach!(
                pub type $id = $ty;
            );
        )*
    }
}

/// Polyfill for std's `offset_of`.
// FIXME(msrv): stabilized in std in 1.77
macro_rules! offset_of {
    ($Ty:path, $field:ident) => {{
        // Taken from bytemuck, avoids accidentally calling on deref
        #[allow(clippy::unneeded_wildcard_pattern)]
        let $Ty { $field: _, .. };
        let data = core::mem::MaybeUninit::<$Ty>::uninit();
        let ptr = data.as_ptr();
        // nested unsafe, see f!
        #[allow(unused_unsafe)]
        // SAFETY: computed address is inbounds since we have a stack alloc for
        // T
        let fptr = unsafe { core::ptr::addr_of!((*ptr).$field) };
        let off = (fptr as usize).checked_sub(ptr as usize).unwrap();
        core::assert!(off <= core::mem::size_of::<$Ty>());
        off
    }};
}

#[cfg(test)]
mod tests {
    use core::any::TypeId;
    use core::prelude::v1::*;

    use crate::types::CEnumRepr;

    #[test]
    fn c_enum_basic() {
        // By default, variants get sequential values.
        c_enum! {
            pub enum e {
                VAR0,
                VAR1,
                VAR2,
            }

            // Also check enums that don't create a type.
            pub enum #anon {
                ANON0,
                ANON1,
                ANON2,
            }

            // No visibility required.
            enum #anon {
                ANON3,
                ANON4,
                ANON5,
            }
        }

        assert_eq!(TypeId::of::<e>(), TypeId::of::<CEnumRepr>());
        assert_eq!(VAR0, 0 as CEnumRepr);
        assert_eq!(VAR1, 1 as CEnumRepr);
        assert_eq!(VAR2, 2 as CEnumRepr);

        assert_eq!(type_id_of_val(&ANON0), TypeId::of::<CEnumRepr>());
        assert_eq!(ANON0, 0 as CEnumRepr);
        assert_eq!(ANON1, 1 as CEnumRepr);
        assert_eq!(ANON2, 2 as CEnumRepr);

        assert_eq!(type_id_of_val(&ANON3), TypeId::of::<CEnumRepr>());
        assert_eq!(ANON3, 0 as CEnumRepr);
        assert_eq!(ANON4, 1 as CEnumRepr);
        assert_eq!(ANON5, 2 as CEnumRepr);
    }

    #[test]
    fn c_enum_repr() {
        // Check specifying the integer representation
        c_enum! {
            #[repr(u16)]
            pub enum e {
                VAR0,
            }

            #[repr(u16)]
            pub enum #anon {
                ANON0,
            }
        }

        assert_eq!(TypeId::of::<e>(), TypeId::of::<u16>());
        assert_eq!(VAR0, 0_u16);

        assert_eq!(type_id_of_val(&ANON0), TypeId::of::<u16>());
        assert_eq!(ANON0, 0_u16);
    }

    #[test]
    fn c_enum_set_value() {
        // Setting an explicit value resets the count.
        c_enum! {
            pub enum e {
                VAR2 = 2,
                VAR3,
                VAR4,
            }
        }

        assert_eq!(VAR2, 2 as CEnumRepr);
        assert_eq!(VAR3, 3 as CEnumRepr);
        assert_eq!(VAR4, 4 as CEnumRepr);
    }

    #[test]
    fn c_enum_multiple_set_value() {
        // C enums always take one more than the previous value, unless set to a
        // specific value. Duplicates are allowed.
        c_enum! {
            pub enum e {
                VAR0,
                VAR2_0 = 2,
                VAR3_0,
                VAR4_0,
                VAR2_1 = 2,
                VAR3_1,
                VAR4_1,
            }
        }

        assert_eq!(VAR0, 0 as CEnumRepr);
        assert_eq!(VAR2_0, 2 as CEnumRepr);
        assert_eq!(VAR3_0, 3 as CEnumRepr);
        assert_eq!(VAR4_0, 4 as CEnumRepr);
        assert_eq!(VAR2_1, 2 as CEnumRepr);
        assert_eq!(VAR3_1, 3 as CEnumRepr);
        assert_eq!(VAR4_1, 4 as CEnumRepr);
    }

    #[test]
    fn c_enum_vis() {
        mod priv1 {
            c_enum! {
                #[repr(u8)]
                pub enum e1 {
                    PRIV_ON_1 = 10,
                    // Variant should still be usable within its visibility
                    pub PUB1 = PRIV_ON_1 * 2,
                }
            }
        }
        mod priv2 {
            c_enum! {
                #[repr(u16)]
                pub enum e2 {
                    pub PRIV_ON_1 = 42,
                    pub PUB2 = PRIV_ON_1 * 2,
                }
            }
        }

        use priv1::*;
        use priv2::*;

        assert_eq!(TypeId::of::<e1>(), TypeId::of::<u8>());
        assert_eq!(TypeId::of::<e2>(), TypeId::of::<u16>());
        assert_eq!(PUB1, 10u8 * 2);
        assert_eq!(PUB2, 42u16 * 2);
        // Verify that the default is private. If `PRIV_ON_1` was actually
        // public in `priv1`, this would be an ambiguous import and/or type
        // mismatch error.
        assert_eq!(PRIV_ON_1, 42u16);
    }

    #[test]
    fn c_enum_attrs() {
        // Note this can't work with `#[cfg]` currently because our expansion uses `previous + 1`
        c_enum! {
            pub enum e {
                VAR0,
                WITH_CFG = if cfg!(target_arch = "x86_64") { 86 } else { 1234 },
                #[deprecated]
                DEPRECATED,
                NOT_DEPRECATED,
            }
        }

        if cfg!(target_arch = "x86_64") {
            assert_eq!(WITH_CFG, 86);
        } else {
            assert_eq!(WITH_CFG, 1234);
        }

        #[expect(deprecated)]
        let _ = DEPRECATED;
        #[deny(deprecated)]
        let _ = NOT_DEPRECATED;
    }

    #[test]
    #[deny(unused_unsafe)]
    fn f_safety() {
        // Enusure the created functions are safe / unsafe / const as expected
        f! {
            pub unsafe fn unsafe_foo() -> u32 { 100 }
            pub const unsafe fn const_unsafe_foo() -> u32 { 101 }
            pub safe fn safe_foo() -> u32 { 200 }
            pub const safe fn const_safe_foo() -> u32 { 201 }
        }

        assert_eq!(unsafe { unsafe_foo() }, 100u32);
        assert_eq!(const { unsafe { const_unsafe_foo() } }, 101u32);
        assert_eq!(safe_foo(), 200u32);
        assert_eq!(const { const_safe_foo() }, 201u32);

        // Check the ABI
        let _: unsafe extern "C" fn() -> u32 = unsafe_foo;
        let _: unsafe extern "C" fn() -> u32 = const_unsafe_foo;
        let _: extern "C" fn() -> u32 = safe_foo;
        let _: extern "C" fn() -> u32 = const_safe_foo;
    }

    fn type_id_of_val<T: 'static>(_: &T) -> TypeId {
        TypeId::of::<T>()
    }

    #[test]
    fn test_offset_of() {
        #[repr(C)]
        struct Off1 {
            a: u8,
            b: u32,
            c: Off2,
            d: u64,
        }

        #[repr(C)]
        #[repr(align(128))]
        struct Off2 {}

        assert_eq!(core::mem::offset_of!(Off1, a), offset_of!(Off1, a));
        assert_eq!(core::mem::offset_of!(Off1, b), offset_of!(Off1, b));
        assert_eq!(core::mem::offset_of!(Off1, c), offset_of!(Off1, c));
        assert_eq!(core::mem::offset_of!(Off1, d), offset_of!(Off1, d));
    }

    #[test]
    fn s2_is_non_exhaustive() {
        // Without `#[exhaustive]`, the record should have an additional field
        // added at the end. If this test compiles, it has it.
        s2! {
            struct Something {
                a: u32,
            }
        }

        let s = Something::default();
        assert_eq!(s.__non_exhaustive, ());
    }

    #[test]
    fn s2_uses_exhaustive() {
        // With `#[exhaustive]`, the record should be regurgitated as-is. If
        // this test compiles, then it works.
        s2! {
            #[exhaustive]
            struct Something {
                a: u32,
            }
        }

        #[allow(unused)]
        let s = Something {
            a: Default::default(),
        };
    }

    #[test]
    fn s2_uses_mixed_exhaustive() {
        // `#[exhaustive]` should work when sandwiched between attributes. If
        // the test compiles, then it works.
        s2! {
            #[repr(align(8))]
            #[exhaustive]
            #[repr(align(2))]
            struct Something {
                a: u32,
            }
        }

        #[allow(unused)]
        let s = Something {
            a: Default::default(),
        };
    }

    #[test]
    fn s2_uses_custom_default() {
        // A non-default value proves `custom_default` is used rather than a
        // derived default.
        s2! {
            struct CustomDefault {
                a: u32,
                #[custom_default([1; 64])]
                buf: [u8; 64],
            }
        }

        let s = CustomDefault::default();
        assert_eq!(s.a, 0);
        assert_eq!(s.buf, [1u8; 64]);
    }

    #[test]
    fn s2_keeps_field_attrs() {
        // If `custom_default` stripping ate the other field attributes, the two
        // `a` fields would collide.
        s2! {
            struct FieldAttrs {
                #[cfg(target_arch = "x86_64")]
                a: u8,
                #[cfg(not(target_arch = "x86_64"))]
                a: u64,
            }
        }

        let s = FieldAttrs::default();
        #[cfg(target_arch = "x86_64")]
        assert_eq!(s.a, 0u8);
        #[cfg(not(target_arch = "x86_64"))]
        assert_eq!(s.a, 0u64);
    }

    #[test]
    fn s2_single_cfg_field() {
        // This field only exists on x86_64, so its default init needs the same
        // `cfg` or `Default` won't build on other arches.
        s2! {
            struct SingleCfg {
                common: u32,
                #[cfg(target_arch = "x86_64")]
                x86_only: u64,
            }
        }

        let s = SingleCfg::default();
        assert_eq!(s.common, 0);
        #[cfg(target_arch = "x86_64")]
        assert_eq!(s.x86_only, 0);
    }

    #[test]
    fn s_no_extra_traits2_zeroes_union() {
        // A union field's default is supplied by
        // `custom_default(unsafe { mem::zeroed })`.
        s_no_extra_traits2! {
            union U {
                a: u32,
                b: f32,
            }

            struct HasUnion {
                x: u16,
                #[custom_default(unsafe { ::core::mem::zeroed::<U>() })]
                u: U,
            }
        }

        let s = HasUnion::default();
        assert_eq!(s.x, 0);
        assert_eq!(unsafe { s.u.a }, 0);
    }

    #[test]
    fn s2_keeps_struct_cfg() {
        // The opposite of the configured-out types in `macro_checks`. With the
        // `cfg` true the type and its `Default` both exist, and the other
        // attributes still apply.
        s2! {
            #[cfg(true)]
            #[repr(align(8))]
            /// a doc comment
            struct EnabledCfg {
                a: u32,
                #[cfg(target_arch = "x86_64")]
                x86_only: u8,
                #[custom_default([1; 40])]
                buf: [u8; 40],
            }
        }

        let s = EnabledCfg::default();
        assert_eq!(s.a, 0);
        assert_eq!(s.buf, [1u8; 40]);
        assert_eq!(align_of::<EnabledCfg>(), 8);
        #[cfg(target_arch = "x86_64")]
        assert_eq!(s.x86_only, 0);
    }

    #[test]
    fn s2_union_default() {
        s_no_extra_traits2! {
            #[unsafe(union_default_via_zeroed)]
            union Foo {
                a: u32,
                b: u64
            }

            #[expect(unused)]
            union Bar {
                a: u32,
                b: u64
            }
        }

        let f: Foo = Foo::default();
        assert_eq!(unsafe { f.b }, 0);

        // Ensure it isn't always implemented, otherwise we would get a conflict.
        impl Default for Bar {
            fn default() -> Self {
                unimplemented!()
            }
        }
    }
}

#[cfg(test)]
#[allow(unused)]
mod macro_checks {
    use core::prelude::v1::*;

    s! {
        pub struct S1 {
            pub a: u32,
            b: u32,
        }

        struct S1Priv {
            pub a: u32,
            b: u32,
        }
    }

    s_no_extra_traits! {
        pub struct S2 {
            pub a: u32,
            b: u32,
        }

        struct S2Priv {
            pub a: u32,
            b: u32,
        }

        pub union U2 {
            pub a: u32,
            b: f32,
        }

        union U2Priv {
            pub a: u32,
            b: f32,
        }
    }

    extern_ty! {
        type Foo;
        pub type Bar;
    }

    s2! {
        pub struct S3 {
            pub a: u32,
            #[custom_default([1; 64])]
            pub buf: [u8; 64],
        }

        struct S3Priv {
            pub a: u32,
            b: u32,
        }
    }

    s_no_extra_traits2! {
        pub union U3 {
            pub a: u32,
            b: f32,
        }

        pub struct S4 {
            pub a: u32,
            #[custom_default(unsafe { ::core::mem::zeroed::<U3>() })]
            pub u: U3,
        }
    }

    fn assert_impls_default<T: Default>() {}

    fn check_default() {
        assert_impls_default::<S3>();
        assert_impls_default::<S4>();
    }

    // Types configured out entirely, checking that the generated impls carry
    // the same `cfg` as the type. Without it they fail to compile with "cannot
    // find type".
    s2! {
        #[cfg(false)]
        pub struct S5 {
            pub a: u32,
            #[custom_default([1; 64])]
            pub buf: [u8; 64],
        }
    }

    s_no_extra_traits! {
        #[cfg(false)]
        pub union U4 {
            pub a: u32,
            b: f32,
        }
    }

    s_no_extra_traits2! {
        #[cfg(false)]
        pub union U5 {
            pub a: u32,
            b: f32,
        }

        #[cfg(false)]
        pub struct S6 {
            pub a: u32,
        }
    }

    // The generated impls name the type and its fields, so they need to allow
    // deprecation.  `deny` turns the warning into an error if that ever stops
    // being the case.
    mod deprecated_checks {
        #![deny(deprecated)]

        s2! {
            #[deprecated(since = "0.0.0", note = "check that generated impls don't warn")]
            pub struct S7 {
                pub a: u32,
            }
        }

        s_no_extra_traits! {
            #[deprecated(since = "0.0.0", note = "check that generated impls don't warn")]
            pub union U6 {
                pub a: u32,
                b: f32,
            }
        }

        s_no_extra_traits2! {
            #[deprecated(since = "0.0.0", note = "check that generated impls don't warn")]
            pub union U7 {
                pub a: u32,
                b: f32,
            }
        }
    }
}
