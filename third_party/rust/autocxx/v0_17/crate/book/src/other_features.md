# Other C++ features

You can make Rust subclasses of C++ classes - as these are mostly used to
implement the Observer pattern, they're documented under [calls from C++ to Rust](rust_calls.md).

## Exceptions

Exceptions are not supported. If your C++ code is compiled with exceptions,
you can expect serious runtime explosions. The underlying [`cxx`](https://cxx.rs) crate has
exception support, so it would be possible to add them.

## Preprocessor symbols

`#define` and other preprocessor symbols will appear as constants.
At present there is no way to do compile-time disablement of code
(equivalent of `#ifdef`)[^ifdef].

[^ifdef]: [This feature](https://github.com/google/autocxx/issues/57) should add ifdef support.

## String constants

Whether from a preprocessor symbol or from a C++ `char*` constant,
strings appear as `[u8]` with a null terminator. To get a Rust string,
do this:

```cpp
#define BOB "Hello"
```

```
# mod ffi { pub static BOB: [u8; 6] = [72u8, 101u8, 108u8, 108u8, 111u8, 0u8]; }
assert_eq!(std::str::from_utf8(&ffi::BOB).unwrap().trim_end_matches(char::from(0)), "Hello");
```

