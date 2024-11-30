# fend-core

This library implements most of the features of [fend](https://github.com/printfn/fend).

It requires no dependencies and can easily be used by other Rust programs.

## Example

```rust
extern crate fend_core;

fn main() {
    let mut context = fend_core::Context::new();
    let result = fend_core::evaluate("1 + 1", &mut context).unwrap();
    assert_eq!(result.get_main_result(), "2");
}
```
