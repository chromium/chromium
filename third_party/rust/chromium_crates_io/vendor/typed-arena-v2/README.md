# `typed-arena`

[![](https://docs.rs/typed-arena/badge.svg)](https://docs.rs/typed-arena/)
[![](https://img.shields.io/crates/v/typed-arena.svg)](https://crates.io/crates/typed-arena)
[![](https://img.shields.io/crates/d/typed-arena.svg)](https://crates.io/crates/typed-arena)
[![Github Actions Build Status](https://github.com/SimonSapin/rust-typed-arena/workflows/CI/badge.svg)](https://github.com/SimonSapin/rust-typed-arena/actions)

**A fast (but limited) allocation arena for values of a single type.**

Allocated objects are destroyed all at once, when the arena itself is destroyed.
There is no deallocation of individual objects while the arena itself is still
alive. The flipside is that allocation is fast: typically just a vector push.

There is also a method `into_vec()` to recover ownership of allocated objects
when the arena is no longer required, instead of destroying everything.

## Example

```rust
use typed_arena::Arena;

struct Monster {
    level: u32,
}

let monsters = Arena::new();

let goku = monsters.alloc(Monster { level: 9001 });
assert!(goku.level > 9000);
```

## Safe Cycles

All allocated objects get the same lifetime, so you can safely create cycles
between them. This can be useful for certain data structures, such as graphs
and trees with parent pointers.

```rust
use std::cell::Cell;
use typed_arena::Arena;

struct CycleParticipant<'a> {
    other: Cell<Option<&'a CycleParticipant<'a>>>,
}

let arena = Arena::new();

let a = arena.alloc(CycleParticipant { other: Cell::new(None) });
let b = arena.alloc(CycleParticipant { other: Cell::new(None) });

a.other.set(Some(b));
b.other.set(Some(a));
```

## Alternatives

### Need to allocate many different types of values?

Use multiple arenas if you have only a couple different types or try
[`bumpalo`](https://crates.io/crates/bumpalo), which is a bump-allocation arena
can allocate heterogenous types of values.

### Want allocation to return identifiers instead of references and dealing with references and lifetimes everywhere?

Check out [`id-arena`](https://crates.io/crates/id-arena) or
[`generational-arena`](https://crates.io/crates/generational-arena).

### Need to deallocate individual objects at a time?

Check out [`generational-arena`](https://crates.io/crates/generational-arena)
for an arena-style crate or look for a more traditional allocator.
