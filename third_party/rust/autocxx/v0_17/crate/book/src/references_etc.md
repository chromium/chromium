# Pointers, references, values

`autocxx` knows how to deal with C++ APIs which take C++ types:
* By value
* By reference (const or not)
* By raw pointer
* By `std::unique_ptr`
* By `std::shared_ptr`
* By `std::weak_ptr`
* (Soon to come) By rvalue reference (that is, as a move parameter)

(all of this is because the underlying [`cxx`](https://cxx.rs) crate has such versatility).
Some of these have some quirks in the way they're exposed in Rust, described below.

## Passing between C++ and Rust by value

See the section on [C++ types](cpp_types.md) for the distinction between POD and non-POD types.
POD types can be passed around however you like. Non-POD types can be passed into functions
in various ways - see [calling C++ functions](cpp_functions.md) for more details.

## References and pointers

We follow [`cxx`](https://cxx.rs) norms here. Specifically:

* A C++ reference becomes a Rust reference
* A C++ pointer becomes a Rust pointer.
* If a reference is returned with an ambiguous lifetime, we don't generate
  code for the function
* Pointers require use of `unsafe`, references don't necessarily.

That last point is key. If your C++ API takes pointers, you're going
to have to use `unsafe`. Similarly, if your C++ API returns a pointer,
you'll have to use `unsafe` to do anything useful with the pointer in Rust.
This is intentional: a pointer from C++ might be subject to concurrent
mutation, or it might have a lifetime that could disappear at any moment.
As a human, you must promise that you understand the constraints around
use of that pointer and that's what the `unsafe` keyword is for.

Exactly the same issues apply to C++ references _in theory_, but in practice,
they usually don't. Therefore [`cxx`](https://cxx.rs) has taken the view that we can "trust"
a C++ reference to a higher degree than a pointer, and autocxx follows that
lead. In practice, of course, references are rarely return values from C++
APIs so we rarely have to navel-gaze about the trustworthiness of a
reference.

(See also the discussion of [`safety`](safety.md) - if you haven't specified
an unsafety policy, _all_ C++ APIs require `unsafe` so the discussion is moot.)

If you're given a C++ object by pointer, and you want to interact with it,
you'll need to figure out the guarantees attached to the C++ object - most
notably its lifetime. To see some of the decision making process involved
see the [Steam example](https://github.com/google/autocxx/tree/main/examples/steam-mini/src/main.rs).

## [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html)s tips

We use [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html) in completely the normal way, but there are a few
quirks which you're more likely to run into with `autocxx`.

* You'll need to use [`.pin_mut()`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html#method.pin_mut) a lot -
  see [the example at the bottom of C++ functions](cpp_functions.md).
* If you need to pass a raw pointer to a function, lots of unsafety is required - something like this:
  ```rust,ignore
     let mut a = ffi::A::make_unique();
     unsafe { ffi::TakePointerToA(std::pin::Pin::<&mut ffi::A>::into_inner_unchecked(a.pin_mut())) };
  ```
  This may be simplified in future.

## Rvalue references

Currently rvalue references (that is, move parameters) are not supported.