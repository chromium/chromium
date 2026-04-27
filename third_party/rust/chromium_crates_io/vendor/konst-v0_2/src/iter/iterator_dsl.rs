/*!
Documentation on the iterator DSL that some `konst::iter` macros support.

The macros from this crate don't directly invoke any of the method listed below,
they expand to equivalent code, this allows the macros to work on stable.

The `konst::iter::collect_const` macro is used in examples here purely for simplicity.

# Methods

Every iterator method below behaves the same as in Iterator,
unless specified otherwise.

The methods listed alphabetically:
- [`copied`](#copied)
- [`enumerate`](#enumerate)
- [`filter_map`](#filter_map)
- [`filter`](#filter)
- [`flat_map`](#flat_map)
- [`flatten`](#flatten)
- [`map`](#map)
- [`rev`](#rev)
- [`skip_while`](#skip_while)
- [`skip`](#skip)
- [`take_while`](#take_while)
- [`take`](#take)
- [`zip`](#zip)


### `zip`

Limitation: the iterator DSL can't passed as an argument to this method.

```rust
use konst::iter;

const ARR: [(&u8, usize); 4] =
    iter::collect_const!((&u8, usize) => &[3u8, 5, 8, 13],zip(100..));

assert_eq!(ARR, [(&3, 100), (&5, 101), (&8, 102), (&13, 103)]);
```

### `enumerate`

`enumerate` always counts from `0`, regardless of whether the iterator is reversed.

```rust
use konst::iter;

const ARR: [(usize, &u8); 4] =
    iter::collect_const!((usize, &u8) => &[3u8, 5, 8, 13],enumerate());

assert_eq!(ARR, [(0, &3), (1, &5), (2, &8), (3, &13)]);
```

### `filter`

```rust
use konst::iter;

const ARR: [&u8; 4] = iter::collect_const!(&u8 =>
    &[3u8, 5, 8, 13, 21],
        filter(|e| !e.is_power_of_two()),
);

assert_eq!(ARR, [&3, &5, &13, &21]);
```

### `map`

```rust
use konst::iter;

const ARR: [usize; 4] = iter::collect_const!(usize => (1..=4),map(|e| e * 3));

assert_eq!(ARR, [3, 6, 9, 12]);
```

### `filter_map`

```rust
use konst::iter;

use std::num::NonZeroU8;

const ARR: [NonZeroU8; 4] = iter::collect_const!(NonZeroU8 =>
    &[3, 0, 1, 5, 6],
        filter_map(|x| NonZeroU8::new(*x)),
);

assert_eq!(ARR, [3, 1, 5, 6].map(|n| NonZeroU8::new(n).unwrap()));
```

### `flat_map`

Limitation: the iterator DSL can't passed as an argument to this method.

```rust
use konst::iter;

const ARR: [usize; 9] = iter::collect_const!(usize =>
    &[3, 5, 8],
        flat_map(|x| {
            let x10 = *x * 10;
            x10..x10 + 3
        }),
);

assert_eq!(ARR, [30, 31, 32, 50, 51, 52, 80, 81, 82]);
```

### `flatten`

```rust
use konst::iter;

const ARR: [&u8; 4] =
    iter::collect_const!(&u8 => &[&[3, 5], &[8, 13]], flatten());

assert_eq!(ARR, [&3, &5, &8, &13]);
```

### `copied`

```rust
use konst::iter;

const ARR: [u8; 3] = iter::collect_const!(u8 =>
    &[2, 3, 4, 5, 6],
        copied(),
        filter(|n| *n % 2 == 0)
);

assert_eq!(ARR, [2, 4, 6]);
```

### `rev`

Limitation: iterator-reversing methods can't be called more than once in
the same macro invocation.

```rust
use konst::iter;

const ARR: [&u8; 3] = iter::collect_const!(&u8 => &[2, 3, 5],rev());

assert_eq!(ARR, [&5, &3, &2]);
```

### `take`

```rust
use konst::iter;

const ARR: [usize; 3] = iter::collect_const!(usize => 10..,take(3));

assert_eq!(ARR, [10, 11, 12]);
```

### `take_while`

```rust
use konst::iter;

const ARR: [&u8; 4] = iter::collect_const!(&u8 =>
    &[3, 5, 8, 13, 21, 34, 55],take_while(|elem| **elem < 20 )
);

assert_eq!(ARR, [&3, &5, &8, &13]);
```

### `skip`

```rust
use konst::iter;

const ARR: [usize; 3] = iter::collect_const!(usize => 10..=15,skip(3));

assert_eq!(ARR, [13, 14, 15]);
```

### `skip_while`

```rust
use konst::iter;

const ARR: [&u8; 3] = iter::collect_const!(&u8 =>
    &[3, 5, 8, 13, 21, 34, 55],skip_while(|elem| **elem < 20 )
);

assert_eq!(ARR, [&21, &34, &55]);
```





*/
