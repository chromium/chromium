/**
Collects an iterator constant into an array

This macro requires the `"rust_1_56"` feature.

# Iterator methods

This macro supports emulating iterator methods by expanding to equivalent code.

The supported iterator methods are documented in the [`iterator_dsl`] module,
because they are also supported by other `konst::iter` macros.

# Syntax

The syntax of this macro is:

```text
collect_const!(
    $Item:ty => $into_iterator:expr
        $(, $iterator_method:ident ($($method_args:tt)*) )*
        $(,)?
)
```
Where `$Item` is the type of the elements that'll be collected into an array.

Where `$into_iterator` is any type that can be converted into a const iterator, with 
[`konst::iter::into_iter`](crate::iter::into_iter).

Where `$iterator_method` is any of the supported methods described in 
the [`iterator_dsl`] module.

# Examples

### Iterating over a range

```rust
use konst::iter;

const ARR: [usize; 8] = iter::collect_const!(usize =>
    10..,
        filter(|n| *n % 2 == 0),
        skip(5),
        take(8),
);

assert_eq!(ARR, [20, 22, 24, 26, 28, 30, 32, 34]);
```

### Iterating over an array

```rust
use konst::iter;

const ARR: [u8; 6] = iter::collect_const!(u8 =>
    // the `&` is required here,
    // because by-value iteration over arrays is not supported.
    &[10, 20, 30],
        flat_map(|&n| {
            // To allow returning references to arrays, the macro extends 
            // the lifetime of borrows to temporaries in return position.
            // The lifetime of the array is extended to the entire iterator chain.
            &[n - 1, n + 1]
        }),
        copied()
);

assert_eq!(ARR, [9, 11, 19, 21, 29, 31]);
```



[`iterator_dsl`]: crate::iter::iterator_dsl
*/
#[cfg(feature = "rust_1_56")]
#[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_56")))]
pub use konst_macro_rules::iter_collect_const as collect_const;