# Storage - stack and heaps

Ensure you understand the distinction between [POD and non-POD types described in the C++ types section before proceeding](cpp_types.md).

## POD types

POD types are just regular Rust types! Store them on the stack, heap, in a `Vec`, a `HashMap`, whatever you want.

## Non-POD types

Non-POD types can be stored:

* In a [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html). This is cxx's Rust wrapper for `std::unique_ptr` - so the object is stored in the C++ heap. Most of the time you handle a C++ object from `autocxx`, it will be stored in one of these.
* In a [`Box`](https://doc.rust-lang.org/std/boxed/struct.Box.html) - so the object is stored on the Rust heap. This is kind of pointless. Don't do this.
* On the Rust stack, using the [`autocxx::moveit`](https://docs.rs/moveit/latest/moveit/macro.moveit.html) macro.

If in doubt, use [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html). It's simple and ergonomic.

See [C++ types](cpp_types.md) for a code example showing a type existing on both the stack and the heap.

## Whose heap is it anyway?

Specifically [`cxx::UniquePtr`](https://docs.rs/cxx/latest/cxx/struct.UniquePtr.html) is a binding to `std::unique_ptr<T,std::default_delete<T>>` which means the object will be deleted using the C++ `delete` operator. This will respect any overridden `operator delete` on the type, and similarly, the functions which `autocxx` provides to _construct_ types should respect overridden `operator new`. This means: if your C++ type has code to create itself in some special or unusual heap partition, that should work fine.

