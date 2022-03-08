#![doc = include_str!("../README.md")]

// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// The crazy macro_rules magic in this file is thanks to dtolnay@
// and is a way of attaching rustdoc to each of the possible directives
// within the include_cpp outer macro. None of the directives actually
// do anything - all the magic is handled entirely by
// autocxx_macro::include_cpp_impl.

pub mod subclass;

#[allow(unused_imports)] // doc cross-reference only
use autocxx_engine::IncludeCppEngine;

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
/// crate, for example [cxx::UniquePtr] or [cxx::CxxString]. Due to the care and thought
/// that's gone into the [cxx] crate, such bindings are pleasant and idiomatic to use
/// from Rust, and usually don't require the `unsafe` keyword.
///
/// # User manual - introduction
///
/// [`include_cpp`] tries to make it possible to include C++ headers and use declared functions
/// and types as-is. The resulting bindings use wrappers for C++ STL types from the [cxx]
/// crate such as [cxx::UniquePtr] or [cxx::CxxString].
///
/// Why, then, do you need a manual? Three reasons:
///
/// * This manual will describe how to include `autocxx` in your build process.
/// * `autocxx` chooses to generate Rust bindings for C++ APIs in particular ways,
///   over which you have _some_ control. The manual discusses what and how.
/// * The combination of `autocxx` and [`cxx`] are not perfect. There are some STL
///   types and some fundamental C++ features which are not yet supported. Where that occurs,
///   you may need to create some manual bindings or otherwise workaround deficiencies.
///   This manual tells you how to spot such circumstances and work around them.
///
/// # Overview
///
/// Here's how to approach autocxx:
///
/// ```mermaid
/// flowchart TB
///     %%{init:{'flowchart':{'nodeSpacing': 60, 'rankSpacing': 30}}}%%
///     autocxx[Add a dependency on autocxx in your project]
///     which-build([Do you use cargo?])
///     autocxx--->which-build
///     autocxx-build[Add a dev dependency on autocxx-build]
///     build-rs[In your build.rs, tell autocxx-build about your header include path]
///     autocxx-build--->build-rs
///     which-build-- Yes -->autocxx-build
///     macro[Add include_cpp! macro: list headers and allowlist]
///     build-rs--->macro
///     autocxx-gen[Use autocxx-gen command line tool]
///     which-build-- No -->autocxx-gen
///     autocxx-gen--->macro
///     build[Build]
///     macro--->build
///     check[Confirm generation using cargo expand]
///     build--->check
///     manual[Add manual cxx::bridge for anything missing]
///     check--->manual
///     use[Use generated ffi mod APIs]
///     manual--->use
/// ```
///
/// # Configuring the build - if you're using cargo
///
/// You'll use the `autocxx-build` crate. Simply copy from the
/// [demo example](https://github.com/google/autocxx/blob/main/demo/build.rs).
/// You'll need to provide it:
/// * The list of `.rs` files which will have `include_cpp!` macros present
/// * Your C++ header include path.
///
/// # Configuring the build - if you're not using cargo
///
/// See the `autocxx-gen` crate. You'll need to:
///
/// * Run the `codegen` phase. You'll need to use the [autocxx-gen]
///   tool to process the .rs code into C++ header and
///   implementation files. This will also generate `.rs` side bindings.
/// * Educate the procedural macro about where to find the generated `.rs` bindings. Set the
///   `AUTOCXX_RS` environment variable to a list of directories to search.
///   If you use `autocxx-build`, this happens automatically. (You can alternatively
///   specify `AUTOCXX_RS_FILE` to give a precise filename as opposed to a directory to search,
///   though this isn't recommended unless your build system specifically requires it
///   because it allows only a single `include_cpp!` block per `.rs` file.)
///
/// ```mermaid
/// flowchart TB
///     s(Rust source with include_cpp!)
///     c(Existing C++ headers)
///     cg(autocxx-gen or autocxx-build)
///     genrs(Generated .rs file)
///     gencpp(Generated .cpp and .h files)
///     rsb(Rust/Cargo build)
///     cppb(C++ build)
///     l(Linker)
///     s --> cg
///     c --> cg
///     cg --> genrs
///     cg --> gencpp
///     m(autocxx-macro)
///     s --> m
///     genrs-. included .->m
///     m --> rsb
///     gencpp --> cppb
///     cppb --> l
///     rsb --> l
/// ```
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
/// # Did it work? How do I deal with failure?
///
/// Once you've achieved a successful build, you might wonder how to know what
/// bindings have been generated. `cargo expand` will show you. Alternatively,
/// you can get autocompletion within an IDE supported by Rust analyzer. You'll
/// need to enable _both_:
/// * Rust-analyzer: Proc Macro: Enable
/// * Rust-analyzer: Experimental: Proc Attr Macros
///
/// Either way, you'll find (for sure!) that `autocxx` hasn't been able to generate
/// bindings for all your C++ APIs. This may manifest as a hard failure or a soft
/// failure:
/// * If you specified such an item in a [`generate`] directive (or similar such
///   as [`generate_pod`]) then your build will fail.
/// * If such APIs are methods belonging to a type, `autocxx` will generate other
///   methods for the type but ignore those.
///
/// In this latter case, you should see helpful messages _in the generated bindings_
/// as rust documentation explaining what went wrong.
///
/// If this happens (and it will!) your options are:
/// * Add more, simpler C++ APIs which fulfil the same need but are compatible with
///   `autocxx`.
/// * Write manual bindings. This is most useful if a type is supported by [cxx]
///   but not `autocxx` (for example, at the time of writing `std::array`). See
///   the later section on 'combinining automatic and manual bindings'.
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
/// # The generated bindings
///
/// ## Pointers, references, and so-forth
///
/// `autocxx` knows how to deal with C++ APIs which take C++ types:
/// * By value
/// * By reference (const or not)
/// * By raw pointer
/// * By `std::unique_ptr`
/// * By `std::shared_ptr`
/// * By `std::weak_ptr`
///
/// (all of this is because the underlying [`cxx`] crate has such versatility).
/// Some of these have some quirks in the way they're exposed in Rust, described below.
///
/// ### Passing between C++ and Rust by value
///
/// Rust is free to move data around at any time. That's _not OK_ for some C++ types
/// which have non-trivial move constructors or destructors. Such types are common
/// in C++ (for example, even C++ `std::string`s) and these types commonly appear
/// in API declarations which we want to make available in Rust. Worse still, Rust
/// has no visibility into whether a C++ type meets these criteria. What do we do?
///
/// You have a choice:
/// * As standard, any C++ type passed by value will be `std::move`d on the C++ side
///   into a `std::unique_ptr` before being passed to Rust, and similarly moved out
///   of a `std::unique_ptr` when passed from Rust to C++.
/// * If you know that your C++ type can be safely byte-copied, then you can
///   override this behavior by using [`generate_pod`] instead of [`generate`].
///
/// There's not a significant ergonomic problem from the use of [`cxx::UniquePtr`].
/// The main negative of the automatic boxing into [`cxx::UniquePtr`] is performance:
/// specifically, the need to
/// allocate heap cells on the C++ side and move data into and out of them.
/// You don't want to be doing this inside a tight loop (but if you're calling
/// across the C++/Rust boundary in a tight loop, perhaps reconsider that boundary
/// anyway).
///
/// If you want your type to be transferred between Rust and C++ truly _by value_
/// then use [`generate_pod`] instead of [`generate`].
///
/// Specifically, to be compatible with [`generate_pod`], your C++ type must either:
/// * Lack a move constructor _and_ lack a destructor
/// * Or contain a human promise that it's relocatable, by implementing
///   the C++ trait `IsRelocatable` per the instructions in
///   [cxx.h](https://github.com/dtolnay/cxx/blob/master/include/cxx.h)
///
/// Otherwise, your build will fail.
///
/// This doesn't just make a difference to the generated code for the type;
/// it also makes a difference to any functions which take or return that type.
/// If there's a C++ function which takes a struct by value, but that struct
/// is not declared as POD-safe, then we'll generate wrapper functions to move
/// that type into and out of [`cxx::UniquePtr`]s.
///
/// There is one other option under construction. The `moveit` crate replicates
/// C++ value move and copying semantics in Rust. There is limited early support
/// for `moveit` within autocxx; specifically, you can call C++ constructors
/// to emplace even non-trivial objects on the Rust stack using `moveit`, and
/// then call methods on them or use references to them in other function calls.
/// At present, you can't copy or move such objects - once they're created,
/// that's it, until it's dropped. At present therefore such objects can't be
/// passed into C++ functions which take non-POD types by value. This facility
/// is therefore best avoided for now until it's more complete - but see
/// `examples/non-trivial-type-on-stack` if you want to see how to use it.
///
/// ### References and pointers
///
/// We follow [cxx] norms here. Specifically:
/// * A C++ reference becomes a Rust reference
/// * A C++ pointer becomes a Rust pointer.
/// * If a reference is returned with an ambiguous lifetime, we don't generate
///   code for the function
/// * Pointers require use of `unsafe`, references don't necessarily.
///
/// That last point is key. If your C++ API takes pointers, you're going
/// to have to use `unsafe`. Similarly, if your C++ API returns a pointer,
/// you'll have to use `unsafe` to do anything useful with the pointer in Rust.
/// This is intentional: a pointer from C++ might be subject to concurrent
/// mutation, or it might have a lifetime that could disappear at any moment.
/// As a human, you must promise that you understand the constraints around
/// use of that pointer and that's what the `unsafe` keyword is for.
///
/// Exactly the same issues apply to C++ references _in theory_, but in practice,
/// they usually don't. Therefore [cxx] has taken the view that we can "trust"
/// a C++ reference to a higher degree than a pointer, and autocxx follows that
/// lead. In practice, of course, references are rarely return values from C++
/// APIs so we rarely have to navel-gaze about the trustworthiness of a
/// reference.
///
/// (See also the discussion of [`safety`] - if you haven't specified
/// an unsafety policy, _all_ C++ APIs require `unsafe` so the discussion is moot.)
///
/// If you're given a C++ object by pointer, and you want to interact with it,
/// you'll need to figure out the guarantees attached to the C++ object - most
/// notably its lifetime. To see some of the decision making process involved
/// see the [Steam example](https://github.com/google/autocxx/tree/main/examples/steam-mini/src/main.rs).
///
/// ### [`cxx::UniquePtr`]s
///
/// We use [`cxx::UniquePtr`] in completely the normal way, but there are a few
/// quirks which you're more likely to run into with `autocxx`.
///
/// * Calling methods: you may need to use [`cxx::UniquePtr::pin_mut`] to get
///   a reference on which you can call a method.
/// * Getting a raw pointer in order to pass to some pre-existing function:
///   at present you need to do:
///   ```rust,ignore
///      let mut a = ffi::A::make_unique();
///      unsafe { ffi::TakePointerToA(std::pin::Pin::<&mut ffi::A>::into_inner_unchecked(a.pin_mut())) };
///   ```
///   This may be simplified in future.
///
/// ## Construction
///
/// Types gain a `make_unique` associated function. At present they only
/// gain this if they have an explicit C++ constructor; this is a limitation
/// which should be resolved in future.
/// This will (of course) return a [`cxx::UniquePtr`] containing that type.
///
/// ## Built-in types
///
/// The generated code uses `cxx` for interop: see that crate for many important
/// considerations including safety and the list of built-in types, for example
/// [`cxx::UniquePtr`] and [`cxx::CxxString`].
///
/// There are almost no `autocxx`-specific types. At present, we do have
/// [`c_int`] and similar, to wrap the integer types whose length
/// varies in C++. It's hoped to contribute full support here to [cxx]
/// in a future change.
///
/// ## Strings
///
/// `autocxx` uses [cxx::CxxString]. However, as noted above, we can't
/// just pass a C++ string by value, so we'll box and unbox it automatically
/// such that you're really dealing with `UniquePtr<CxxString>` on the Rust
/// side, even if the API just took or returned a plain old `std::string`.
///
/// However, to ease ergonomics, functions that accept a `std::string` will
/// actually accept anything that
/// implements a trait called `ffi::ToCppString`. That may either be a
/// `UniquePtr<CxxString>` or just a plain old Rust string - which will be
/// converted transparently to a C++ string.
///
/// This trait, and its implementations, are not present in the `autocxx`
/// documentation because they're dynamically generated in _your_ code
/// so that they can call through to a `make_string` implementation in
/// the C++ that we're injecting into your C++ build system.
///
/// (None of that happens if you use [exclude_utilities], so don't do that.)
///
/// If you need to create a blank `UniquePtr<CxxString>` in Rust, such that
/// (for example) you can pass its mutable reference or pointer into some
/// pre-existing C++ API, call `ffi::make_string("")` which will return
/// a blank `UniquePtr<CxxString>`.
///
/// Don't attempt to use [cxx::let_cpp_string] which will allocate the
/// string on the stack, and is generally incompatible with the
/// [cxx::UniquePtr]-based approaches we use here.
///
/// ## Preprocessor symbols
///
/// `#define` and other preprocessor symbols will appear as constants.
/// At present there is no way to do compile-time disablement of code
/// (equivalent of `#ifdef`).
///
/// ## Integer types
///
/// For C++ types with a defined size, just go ahead and use `u64`, `i32` etc.
/// For types such as `int` or `unsigned long`, the hope is that you can
/// eventually use `std::os::raw::c_int` oor `std::os::raw::c_ulong` etc.
/// For now, this doesn't quite work: instead you need to wrap these values
/// in a newtype wrapper such as [c_int] or [c_ulong] in this crate.
///
/// ## String constants
///
/// Whether from a preprocessor symbol or from a C++ `char*` constant,
/// strings appear as `[u8]` with a null terminator. To get a Rust string,
/// do this:
///
/// ```cpp
/// #define BOB "Hello"
/// ```
///
/// ```
/// # mod ffi { pub static BOB: [u8; 6] = [72u8, 101u8, 108u8, 108u8, 111u8, 0u8]; }
/// assert_eq!(std::str::from_utf8(&ffi::BOB).unwrap().trim_end_matches(char::from(0)), "Hello");
/// ```
///
/// ## Namespaces
///
/// The C++ namespace structure is reflected in mods within the generated
/// ffi mod. However, at present there is an internal limitation that
/// autocxx can't handle multiple symbols with the same identifier, even
/// if they're in different namespaces. This will be fixed in future.
///
/// ## Overloads - and identifiers ending in digits
///
/// C++ allows function overloads; Rust doesn't. `autocxx` follows the lead
/// of `bindgen` here and generating overloads as `func`, `func1`, `func2` etc.
/// This is essentially awful without `rust-analyzer` IDE support, which isn't
/// quite there yet.
///
/// `autocxx` doesn't yet support default paramters.
///
/// It's fairly likely we'll change the model here in the future, such that
/// we can pass tuples of different parameter types into a single function
/// implementation.
///
/// ## Forward declarations
///
/// A type which is incomplete in the C++ headers (i.e. represented only by a forward
/// declaration) can't be held in a `UniquePtr` within Rust (because Rust can't know
/// if it has a destructor that will need to be called if the object is `Drop`ped.)
/// Naturally, such an object can't be passed by value either; it can still be
/// referenced in Rust references.
///
/// ## Generic types
///
/// If you're using one of the generic types which is supported natively by cxx,
/// e.g. `std::unique_ptr`, it should work as you expect. For other generic types,
/// we synthesize a concrete Rust type, corresponding to a C++ typedef, for each
/// concrete instantiation of the type. Such generated types are always opaque,
/// and never have methods attached. That's therefore enough to pass them
/// between return types and parameters of other functions within [`cxx::UniquePtr`]s
/// but not really enough to do anything else with these types just yet. Hopefully,
/// this will be improved in future. At present such types have a name
/// `AutocxxConcrete{n}` but this may change in future.
///
/// ## Exceptions
///
/// Exceptions are not supported. If your C++ code is compiled with exceptions,
/// you can expect serious runtime explosions. The underlying [cxx] crate has
/// exception support, so it would be possible to add them.
///
/// # Subclasses
///
/// There is limited and experimental support for creating Rust subclasses of
/// C++ classes. (Yes, even more experimental than all the rest of this!)
/// See [`subclass::CppSubclass`] for information about how you do this.
/// This is useful primarily if you want to listen out for messages broadcast
/// using the C++ observer/listener pattern.
///
/// # Mixing manual and automated bindings
///
/// `autocxx` uses [cxx] underneath, and its build process will happily spot and
/// process and manually-crafted [`cxx::bridge`] mods which you include in your
/// Rust source code. A common pattern good be to use `autocxx` to generate
/// all the bindings possible, then hand-craft a [`cxx::bridge`] mod for the
/// remainder where `autocxx` falls short.
///
/// To do this, you'll need to use the [ability of one cxx::bridge mod to refer to types from another](https://cxx.rs/extern-c++.html#reusing-existing-binding-types),
/// for example:
///
/// ```rust,ignore
/// autocxx::include_cpp! {
///     #include "foo.h"
///     safety!(unsafe_ffi)
///     generate!("take_A")
///     generate!("A")
/// }
/// #[cxx::bridge]
/// mod ffi2 {
///     unsafe extern "C++" {
///         include!("foo.h");
///         type A = crate::ffi::A;
///         fn give_A() -> UniquePtr<A>; // in practice, autocxx could happily do this
///     }
/// }
/// fn main() {
///     let a = ffi2::give_A();
///     assert_eq!(ffi::take_A(&a), autocxx::c_int(5));
/// }
/// ```
///
/// # Safety
///
/// # Examples
///
/// * [Demo](https://github.com/google/autocxx/tree/main/demo) - simplest possible demo
/// * [S2 example](https://github.com/google/autocxx/tree/main/examples/s2) - example using S2 geometry library
/// * [Steam example](https://github.com/google/autocxx/tree/main/examples/steam-mini) - example using (something like) the Steam client library
/// * [Subclass example](https://github.com/google/autocxx/tree/main/examples/subclass) - example using subclasses
/// * [Integration tests](https://github.com/google/autocxx/blob/main/integration-tests/src/tests.rs)
///   - hundreds of small snippets
///
/// Contributions of more examples to the `examples` directory are much appreciated!
///
/// # Internals
///
/// For documentation on how this all actually _works_, see
/// [IncludeCppEngine].
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
/// instead, and own the type using [UniquePtr][autocxx_engine::cxx::UniquePtr].
/// A directive to be included inside
/// [include_cpp] - see [include_cpp] for general information.
#[macro_export]
macro_rules! generate_pod {
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

        unsafe impl autocxx_engine::cxx::ExternType for $r {
            type Id = autocxx_engine::cxx::type_id!($c);
            type Kind = autocxx_engine::cxx::kind::Trivial;
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

unsafe impl autocxx_engine::cxx::ExternType for c_void {
    type Id = autocxx_engine::cxx::type_id!(c_void);
    type Kind = autocxx_engine::cxx::kind::Trivial;
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

/// Imports which you're likely to want to use.
pub mod prelude {
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
    pub use moveit::moveit;
    pub use moveit::new::New;
}

/// Re-export moveit for ease of consumers.
pub use moveit;
