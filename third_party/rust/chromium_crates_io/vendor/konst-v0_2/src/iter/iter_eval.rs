/**
Emulates iterator method chains, by expanding to equivalent code.

For examples that use multiple methods [look here](#full-examples)

# Methods

### Consuming methods

Consuming methods are those that consume the iterator, returning a non-iterator.

The consuming methods listed alphabetically:
- [`all`](#all)
- [`any`](#any)
- [`count`](#count)
- [`find_map`](#find_map)
- [`find`](#find)
- [`fold`](#fold)
- [`for_each`](#for_each)
- [`next`](#next)
- [`nth`](#nth)
- [`position`](#position)
- [`rfind`](#rfind)
- [`rfold`](#rfold)
- [`rposition`](#rposition)

### Adaptor Methods

Adaptor methods are those that transform the iterator into a different iterator.
They are shared with other `konst::iter` macros and are documented
in the  [`iterator_dsl`] module.

The iterator adaptor methods, listed alphabetically
(these links go to the [`iterator_dsl`] module):
- [`copied`](./iterator_dsl/index.html#copied)
- [`enumerate`](./iterator_dsl/index.html#enumerate)
- [`filter_map`](./iterator_dsl/index.html#filter_map)
- [`filter`](./iterator_dsl/index.html#filter)
- [`flat_map`](./iterator_dsl/index.html#flat_map)
- [`flatten`](./iterator_dsl/index.html#flatten)
- [`map`](./iterator_dsl/index.html#map)
- [`rev`](./iterator_dsl/index.html#rev)
- [`skip_while`](./iterator_dsl/index.html#skip_while)
- [`skip`](./iterator_dsl/index.html#skip)
- [`take_while`](./iterator_dsl/index.html#take_while)
- [`take`](./iterator_dsl/index.html#take)
- [`zip`](./iterator_dsl/index.html#zip)


### `all`

Const equivalent of [`Iterator::all`]

```rust
use konst::iter;
const fn all_digits(s: &str) -> bool {
    iter::eval!(s.as_bytes(),all(|c| matches!(c, b'0'..=b'9')))
}

assert!(all_digits("123456"));
assert!(!all_digits("0x123456"));

```

### `any`

Const equivalent of [`Iterator::any`]


```rust
use konst::iter;

const fn contains_pow2(s: &[u64]) -> bool {
    iter::eval!(s,any(|c| c.is_power_of_two()))
}

assert!(contains_pow2(&[2, 3, 5]));
assert!(!contains_pow2(&[13, 21, 34]));

```

### `count`

Const equivalent of [`Iterator::count`]

This example requires the `"rust_1_64"` crate feature,
because it calls `string::split`.

*/
#[cfg_attr(not(feature = "rust_1_64"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_64", doc = "```rust")]
/**
use konst::{iter, string};

const fn count_csv(s: &str) -> usize {
    iter::eval!(string::split(s, ","),count())
}

assert_eq!(count_csv("foo"), 1);
assert_eq!(count_csv("foo,bar"), 2);
assert_eq!(count_csv("foo,bar,baz"), 3);

```

### `for_each`

Alternative way to write [`konst::iter::for_each`](crate::iter::for_each)

```rust
use konst::iter;

const fn sum_elems(s: &[u64]) -> u64 {
    let mut sum = 0u64;
    iter::eval!{s,copied(),for_each(|x| sum+=x)}
    sum
}

assert_eq!(sum_elems(&[]), 0);
assert_eq!(sum_elems(&[2]), 2);
assert_eq!(sum_elems(&[3, 5]), 8);

```
### `next`

Gets the first element in the iterator,
only needed when intermediate Iterator methods are used.

```rust
use konst::iter;

const fn first_elem(s: &[u64]) -> Option<u64> {
    iter::eval!(s,copied(),next())
}

assert_eq!(first_elem(&[]), None);
assert_eq!(first_elem(&[2]), Some(2));
assert_eq!(first_elem(&[3, 5]), Some(3));

```

### `nth`

Const equivalent of [`Iterator::nth`]

This example requires the `"rust_1_64"` crate feature,
because it calls `string::split`.

*/
#[cfg_attr(not(feature = "rust_1_64"), doc = "```ignore")]
#[cfg_attr(feature = "rust_1_64", doc = "```rust")]
/**
use konst::{iter, string};

const fn nth_csv(s: &str, nth: usize) -> Option<&str> {
    iter::eval!(string::split(s, ","),nth(nth))
}

assert_eq!(nth_csv("foo,bar,baz", 0), Some("foo"));
assert_eq!(nth_csv("foo,bar,baz", 1), Some("bar"));
assert_eq!(nth_csv("foo,bar,baz", 2), Some("baz"));
assert_eq!(nth_csv("foo,bar,baz", 3), None);

```

### `position`

Const equivalent of [`Iterator::position`]

```rust
use konst::iter;

const fn find_num(slice: &[u64], n: u64) -> Option<usize> {
    iter::eval!(slice,position(|&elem| elem == n))
}

assert_eq!(find_num(&[3, 5, 8], 0), None);
assert_eq!(find_num(&[3, 5, 8], 3), Some(0));
assert_eq!(find_num(&[3, 5, 8], 5), Some(1));
assert_eq!(find_num(&[3, 5, 8], 8), Some(2));

```

### `rposition`

Const equivalent of [`Iterator::rposition`]

Limitation: iterator-reversing methods can't be called more than once in 
the same macro invocation.

```rust
use konst::iter;

const fn rfind_num(slice: &[u64], n: u64) -> Option<usize> {
    iter::eval!(slice,rposition(|&elem| elem == n))
}

assert_eq!(rfind_num(&[3, 5, 8], 0), None);
assert_eq!(rfind_num(&[3, 5, 8], 3), Some(2));
assert_eq!(rfind_num(&[3, 5, 8], 5), Some(1));
assert_eq!(rfind_num(&[3, 5, 8], 8), Some(0));

```

### `find`

Const equivalent of [`Iterator::find`]

```rust
use konst::iter;

const fn find_odd(slice: &[u64], n: u64) -> Option<&u64> {
    iter::eval!(slice,find(|&&elem| elem % 2 == 1))
}

assert_eq!(find_odd(&[], 0), None);
assert_eq!(find_odd(&[2, 4], 0), None);
assert_eq!(find_odd(&[3, 5, 8], 3), Some(&3));
assert_eq!(find_odd(&[8, 12, 13], 3), Some(&13));

```

### `find_map`

Const equivalent of [`Iterator::find_map`]

This example requires the `"parsing_no_proc"` feature.

*/
#[cfg_attr(not(feature = "parsing_no_proc"), doc = "```ignore")]
#[cfg_attr(feature = "parsing_no_proc", doc = "```rust")]
/**
use konst::{iter, result};
use konst::primitive::parse_u64;

const fn find_parsable(slice: &[&str]) -> Option<u64> {
    iter::eval!(slice,find_map(|&s| result::ok!(parse_u64(s))))
}

assert_eq!(find_parsable(&[]), None);
assert_eq!(find_parsable(&["foo"]), None);
assert_eq!(find_parsable(&["foo", "10"]), Some(10));
assert_eq!(find_parsable(&["10", "20"]), Some(10));

```

### `rfind`

Const equivalent of [`DoubleEndedIterator::rfind`]

Limitation: iterator-reversing methods can't be called more than once in 
the same macro invocation.

```rust
use konst::iter;

const fn sum_u64(slice: &[u64]) -> Option<&u64> {
    iter::eval!(slice,rfind(|&elem| elem.is_power_of_two()))
}

assert_eq!(sum_u64(&[]), None);
assert_eq!(sum_u64(&[2]), Some(&2));
assert_eq!(sum_u64(&[2, 5, 8]), Some(&8));

```

### `fold`

Const equivalent of [`Iterator::fold`]

```rust
use konst::iter;

const fn sum_u64(slice: &[u64]) -> u64 {
    iter::eval!(slice,fold(0, |accum, &rhs| accum + rhs))
}

assert_eq!(sum_u64(&[]), 0);
assert_eq!(sum_u64(&[3]), 3);
assert_eq!(sum_u64(&[3, 5]), 8);
assert_eq!(sum_u64(&[3, 5, 8]), 16);


```

### `rfold`

Const equivalent of [`DoubleEndedIterator::rfold`]

Limitation: iterator-reversing methods can't be called more than once in 
the same macro invocation.

```rust
use konst::iter;     

const fn concat_u16s(slice: &[u16]) -> u128 {
    iter::eval!(slice,rfold(0, |accum, &rhs| (accum << 16) + (rhs as u128)))
}

assert_eq!(concat_u16s(&[1, 2, 3]), 0x0003_0002_0001);
assert_eq!(concat_u16s(&[3, 5, 8]), 0x0008_0005_0003);


```

<span id = "full-examples"></span>
# Examples

### Second number in CSV

This example demonstrates a function that gets the second number in a CSV string,
using iterators.

This example requires both the `"parsing_no_proc"` and `"rust_1_64"` features.

*/
#[cfg_attr(not(all(feature = "parsing_no_proc", feature = "rust_1_64")), doc = "```ignore")]
#[cfg_attr(all(feature = "parsing_no_proc", feature = "rust_1_64"), doc = "```rust")]
/**
use konst::{
    iter,
    primitive::parse_u64,
    result,
    string,
};

const fn second_number(s: &str) -> Option<u64> {
    iter::eval!(
        string::split(s, ","),
            filter_map(|s| result::ok!(parse_u64(s))),
            nth(1),
    )
}

assert_eq!(second_number("foo,bar,baz"), None);
assert_eq!(second_number("foo,3,bar"), None);
assert_eq!(second_number("foo,3,bar,5"), Some(5));
assert_eq!(second_number("foo,8,bar,13,baz,21"), Some(13));

```


[`iterator_dsl`]: ./iterator_dsl/index.html

*/
pub use konst_macro_rules::iter_eval as eval;