#![doc = include_str!("../README.md")]

// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// The crazy macro_rules magic in this file is thanks to dtolnay@
// and is a way of attaching rustdoc to each of the possible directives
// within the include_cpp outer macro. None of the directives actually
// do anything - all the magic is handled entirely by
// autocxx_macro::include_cpp_impl.

pub mod subclass;
mod value_param;

#[cfg_attr(doc, aquamarine::aquamarine)]
/// Include some C++ headers in your Rust project.
///
/// This macro allows you to include one or more C++ headers within
/// your Rust code, and call their functions fairly naturally.
///
/// # Examples
///
/// C++ header (`input.h`):
/// ```cpp
/// #include <cstdint>
///
/// uint32_t do_math(uint32_t a);
/// ```
///
/// Rust code:
/// ```
/// # use autocxx_macro::include_cpp_impl as include_cpp;
/// include_cpp!(
/// #   parse_only!()
///     #include "input.h"
///     generate!("do_math")
///     safety!(unsafe)
/// );
///
/// # mod ffi { pub fn do_math(a: u32) -> u32 { a+3 } }
/// # fn main() {
/// ffi::do_math(3);
/// # }
/// ```
///
/// The resulting bindings will use idiomatic Rust wrappers for types from the [cxx]
/// crate, for example [`cxx::UniquePtr`] or [`cxx::CxxString`]. Due to the care and thought
/// that's gone into the [cxx] crate, such bindings are pleasant and idiomatic to use
/// from Rust, and usually don't require the `unsafe` keyword.
///
/// For full documentation, see [the manual](https://google.github.io/autocxx/).
///
/// # The [`include_cpp`] macro
///
/// Within the braces of the `include_cpp!{...}` macro, you should provide
/// a list of at least the following:
///
/// * `#include "cpp_header.h"`: a header filename to parse and include
/// * `generate!("type_or_function_name")`: a type or function name whose declaration
///   should be made available to C++. (See the section on Allowlisting, below).
/// * Optionally, `safety!(unsafe)` - see discussion of [`safety`].
///
/// Other directives are possible as documented in this crate.
///
/// Now, try to build your Rust project. `autocxx` may fail to generate bindings
/// for some of the items you specified with [generate] directives: remove
/// those directives for now, then see the next section for advice.
///
/// # Allowlisting
///
/// How do you inform autocxx which bindings to generate? There are three
/// strategies:
///
/// * *Recommended*: provide various [`generate`] directives in the
///   [`include_cpp`] macro. This can specify functions or types.
/// * *Not recommended*: in your `build.rs`, call [`Builder::auto_allowlist`].
///   This will attempt to spot _uses_ of FFI bindings anywhere in your Rust code
///   and build the allowlist that way. This is experimental and has known limitations.
/// * *Strongly not recommended*: use [`generate_all`]. This will attempt to
///   generate Rust bindings for _any_ C++ type or function discovered in the
///   header files. This is generally a disaster if you're including any
///   remotely complex header file: we'll try to generate bindings for all sorts
///   of STL types. This will be slow, and some may well cause problems.
///   Effectively this is just a debug option to discover such problems. Don't
///   use it!
///
/// # Internals
///
/// For documentation on how this all actually _works_, see
/// `IncludeCppEngine` within the `autocxx_engine` crate.
#[macro_export]
macro_rules! include_cpp {
    (
        $(#$include:ident $lit:literal)*
        $($mac:ident!($($arg:tt)*))*
    ) => {
        $($crate::$include!{__docs})*
        $($crate::$mac!{__docs})*
        $crate::include_cpp_impl! {
            $(#include $lit)*
            $($mac!($($arg)*))*
        }
    };
}

/// Include a C++ header. A directive to be included inside
/// [include_cpp] - see [include_cpp] for details
#[macro_export]
macro_rules! include {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Generate Rust bindings for the given C++ type or function.
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
/// See also [generate_pod].
#[macro_export]
macro_rules! generate {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Generate as "plain old data" and add to allowlist.
/// Generate Rust bindings for the given C++ type such that
/// it can be passed and owned by value in Rust. This only works
/// for C++ types which have trivial move constructors and no
/// destructor - you'll encounter a compile error otherwise.
/// If your type doesn't match that description, use [generate]
/// instead, and own the type using [UniquePtr][cxx::UniquePtr].
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! generate_pod {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Generate Rust bindings for all C++ types and functions
/// in a given namespace.
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
/// See also [generate].
#[macro_export]
macro_rules! generate_ns {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Generate Rust bindings for all C++ types and functions
/// found. Highly experimental and not recommended.
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
/// See also [generate].
#[macro_export]
macro_rules! generate_all {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Generate as "plain old data". For use with [generate_all]
/// and similarly experimental.
#[macro_export]
macro_rules! pod {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Skip the normal generation of a `make_string` function
/// and other utilities which we might generate normally.
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! exclude_utilities {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Entirely block some type from appearing in the generated
/// code. This can be useful if there is a type which is not
/// understood by bindgen or autocxx, and incorrect code is
/// otherwise generated.
/// This is 'greedy' in the sense that any functions/methods
/// which take or return such a type will _also_ be blocked.
///
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! block {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Avoid generating implicit constructors for this type.
/// The rules for when to generate C++ implicit constructors
/// are complex, and if autocxx gets it wrong, you can block
/// such constructors using this.
///
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! block_constructors {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// The name of the mod to be generated with the FFI code.
/// The default is `ffi`.
///
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! name {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Specifies a global safety policy for functions generated
/// from these headers. By default (without such a `safety!`
/// directive) all such functions are marked as `unsafe` and
/// therefore can only be called within an `unsafe {}` block
/// or some `unsafe` function which you create.
///
/// Alternatively, by specifying a `safety!` block you can
/// declare that most generated functions are in fact safe.
/// Specifically, you'd specify:
/// `safety!(unsafe)`
/// or
/// `safety!(unsafe_ffi)`
/// These two options are functionally identical. If you're
/// unsure, simply use `unsafe`. The reason for the
/// latter option is if you have code review policies which
/// might want to give a different level of scrutiny to
/// C++ interop as opposed to other types of unsafe Rust code.
/// Maybe in your organization, C++ interop is less scary than
/// a low-level Rust data structure using pointer manipulation.
/// Or maybe it's more scary. Either way, using `unsafe` for
/// the data structure and using `unsafe_ffi` for the C++
/// interop allows you to apply different linting tools and
/// policies to the different options.
///
/// Irrespective, C++ code is of course unsafe. It's worth
/// noting that use of C++ can cause unexpected unsafety at
/// a distance in faraway Rust code. As with any use of the
/// `unsafe` keyword in Rust, *you the human* are declaring
/// that you've analyzed all possible ways that the code
/// can be used and you are guaranteeing to the compiler that
/// no badness can occur. Good luck.
///
/// Generated C++ APIs which use raw pointers remain `unsafe`
/// no matter what policy you choose.
#[macro_export]
macro_rules! safety {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Whether to avoid generating [`cxx::UniquePtr`] and [`cxx::Vector`]
/// implementations. This is primarily useful for reducing test cases and
/// shouldn't be used in normal operation.
///
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! exclude_impls {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// Deprecated - use [`extern_rust_type`] instead.
#[macro_export]
#[deprecated]
macro_rules! rust_type {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// See [`extern_rust::extern_rust_type`].
#[macro_export]
macro_rules! extern_rust_type {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

/// See [`subclass::subclass`].
#[macro_export]
macro_rules! subclass {
    ($($tt:tt)*) => { $crate::usage!{$($tt)*} };
}

#[doc(hidden)]
#[macro_export]
macro_rules! usage {
    (__docs) => {};
    ($($tt:tt)*) => {
        compile_error! {r#"usage:  include_cpp! {
                   #include "path/to/header.h"
                   generate!(...)
                   generate_pod!(...)
               }
"#}
    };
}

#[doc(hidden)]
pub use autocxx_macro::include_cpp_impl;

#[doc(hidden)]
pub use autocxx_macro::cpp_semantics;

macro_rules! ctype_wrapper {
    ($r:ident, $c:expr, $d:expr) => {
        #[doc=$d]
        #[derive(Debug, Eq, Copy, Clone, PartialEq, Hash)]
        #[allow(non_camel_case_types)]
        #[repr(transparent)]
        pub struct $r(pub ::std::os::raw::$r);

        /// # Safety
        ///
        /// We assert that the namespace and type ID refer to a C++
        /// type which is equivalent to this Rust type.
        unsafe impl cxx::ExternType for $r {
            type Id = cxx::type_id!($c);
            type Kind = cxx::kind::Trivial;
        }

        impl From<::std::os::raw::$r> for $r {
            fn from(val: ::std::os::raw::$r) -> Self {
                Self(val)
            }
        }

        impl From<$r> for ::std::os::raw::$r {
            fn from(val: $r) -> Self {
                val.0
            }
        }
    };
}

ctype_wrapper!(
    c_ulonglong,
    "c_ulonglong",
    "Newtype wrapper for an unsigned long long"
);
ctype_wrapper!(c_longlong, "c_longlong", "Newtype wrapper for a long long");
ctype_wrapper!(c_ulong, "c_ulong", "Newtype wrapper for an unsigned long");
ctype_wrapper!(c_long, "c_long", "Newtype wrapper for a long");
ctype_wrapper!(
    c_ushort,
    "c_ushort",
    "Newtype wrapper for an unsigned short"
);
ctype_wrapper!(c_short, "c_short", "Newtype wrapper for an short");
ctype_wrapper!(c_uint, "c_uint", "Newtype wrapper for an unsigned int");
ctype_wrapper!(c_int, "c_int", "Newtype wrapper for an int");
ctype_wrapper!(c_uchar, "c_uchar", "Newtype wrapper for an unsigned char");

/// Newtype wrapper for a C void. Only useful as a `*c_void`
#[allow(non_camel_case_types)]
#[repr(transparent)]
pub struct c_void(pub ::std::os::raw::c_void);

/// # Safety
///
/// We assert that the namespace and type ID refer to a C++
/// type which is equivalent to this Rust type.
unsafe impl cxx::ExternType for c_void {
    type Id = cxx::type_id!(c_void);
    type Kind = cxx::kind::Trivial;
}

/// autocxx couldn't generate these bindings.
/// If you come across a method, type or function which refers to this type,
/// it indicates that autocxx couldn't generate that binding. A documentation
/// comment should be attached indicating the reason.
pub struct BindingGenerationFailure {
    _unallocatable: [*const u8; 0],
    _pinned: core::marker::PhantomData<core::marker::PhantomPinned>,
}

/// Tools to export Rust code to C++.
// These are in a mod to avoid shadowing the definitions of the
// directives above, which, being macro_rules, are unavoidably
// in the crate root but must be function-style macros to keep
// the include_cpp impl happy.
pub mod extern_rust {

    /// Declare that this is a Rust type which is to be exported to C++.
    /// You can use this in two ways:
    /// * as an attribute macro on a Rust type, for instance:
    ///   ```
    ///   # use autocxx_macro::extern_rust_type as extern_rust_type;
    ///   #[extern_rust_type]
    ///   struct Bar;
    ///   ```
    /// * as a directive within the [include_cpp] macro, in which case
    ///   provide the type path in brackets:
    ///   ```
    ///   # use autocxx_macro::include_cpp_impl as include_cpp;
    ///   include_cpp!(
    ///   #   parse_only!()
    ///       #include "input.h"
    ///       extern_rust_type!(Bar)
    ///       safety!(unsafe)
    ///   );
    ///   struct Bar;
    ///   ```
    /// These may be used within references in the signatures of C++ functions,
    /// for instance. This will contribute to an `extern "Rust"` section of the
    /// generated `cxx` bindings, and this type will appear in the C++ header
    /// generated for use in C++.
    pub use autocxx_macro::extern_rust_type;

    /// Declare that a given function is a Rust function which is to be exported
    /// to C++. This is used as an attribute macro on a Rust function, for instance:
    /// ```
    /// # use autocxx_macro::extern_rust_function as extern_rust_function;
    /// #[extern_rust_function]
    /// pub fn call_me_from_cpp() { }
    /// ```
    pub use autocxx_macro::extern_rust_function;
}

/// Equivalent to [`std::convert::AsMut`], but returns a pinned mutable reference
/// such that cxx methods can be called on it.
pub trait PinMut<T>: AsRef<T> {
    /// Return a pinned mutable reference to a type.
    fn pin_mut(&mut self) -> std::pin::Pin<&mut T>;
}

pub use value_param::as_copy;
pub use value_param::as_mov;
pub use value_param::as_new;
pub use value_param::ValueParam;
pub use value_param::ValueParamHandler;

/// Imports which you're likely to want to use.
pub mod prelude {
    pub use crate::as_copy;
    pub use crate::as_mov;
    pub use crate::as_new;
    pub use crate::c_int;
    pub use crate::c_long;
    pub use crate::c_longlong;
    pub use crate::c_short;
    pub use crate::c_uchar;
    pub use crate::c_uint;
    pub use crate::c_ulong;
    pub use crate::c_ulonglong;
    pub use crate::c_ushort;
    pub use crate::c_void;
    pub use crate::cpp_semantics;
    pub use crate::include_cpp;
    pub use crate::PinMut;
    pub use crate::ValueParam;
    pub use moveit::moveit;
    pub use moveit::new::New;
}

/// Re-export moveit for ease of consumers.
pub use moveit;

/// Re-export cxx such that clients can use the same version as
/// us. This doesn't enable clients to avoid depending on the cxx
/// crate too, unfortunately, since generated cxx::bridge code
/// refers explicitly to ::cxx. See
/// <https://github.com/google/autocxx/issues/36>
pub use cxx;
