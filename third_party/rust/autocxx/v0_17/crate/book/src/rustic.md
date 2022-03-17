# Using the generated bindings

Congratulations, you've built some bindings using `autocxx`!

But are they Rustic? How can you ensure that users of the bindings get Rust-like safety?

The C++ API may have documented usage invariants. Your ideal is to encode as many as possible of those into compile-time checks in Rust.

Some options to consider:

* Wrap the bindings in a newtype wrapper which enforces compile-time variants in its APIs; for example, taking a mutable reference to enforce exclusive access.
* Add extra `impl` blocks to add methods with a more Rustic API.
* Read [the C++ to Rust design FAQ](https://cppfaq.rs).
