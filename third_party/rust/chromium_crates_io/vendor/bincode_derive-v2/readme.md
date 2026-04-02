# Bincode-derive

The derive crate for bincode. Implements `bincode::Encode` and `bincode::Decode`.

This crate is roughly split into 2 parts:

# Parsing

Most of parsing is done in the `src/parse/` folder. This will generate the following types:
- `Attributes`, not being used currently
- `Visibility`, not being used currently
- `DataType` either `Struct` or `Enum`, with the name of the data type being parsed
- `Generics` the generics part of the type, e.g. `struct Foo<'a>`
- `GenericConstraints` the "where" part of the type

# Generate

Generating the code implementation is done in either `src/derive_enum.rs` and `src/derive_struct.rs`.

This is supported by the structs in `src/generate`. The most notable points of this module are:
- `StreamBuilder` is a thin but friendly wrapper around `TokenStream`
- `Generator` is the base type of the code generator. This has helper methods to generate implementations:
  - `ImplFor` is a helper struct for a single `impl A for B` construction. In this functions can be defined:
    - `GenerateFnBody` is a helper struct for a single function in the above `impl`. This is created with a callback to `FnBuilder` which helps set some properties. `GenerateFnBody` has a `stream()` function which returns ` StreamBuilder` for the function.

For additional derive testing, see the test cases in `../tests`

For testing purposes, all generated code is outputted to the current `target/generated/bincode` folder, under file name `<struct/enum name>_Encode.rs` and `<struct/enum name>_Decode.rs`. This can help with debugging.
