# moveit

A library for safe, in-place construction of Rust (and C++!) objects.

## How It Works

`moveit` revolves around `unsafe trait`s that impose additional guarantees
on `!Unpin` types, such that they can be moved in the C++ sense. There are
two senses of "move" frequently used:
- The Rust sense, which is a blind memcpy and analogous-ish to the
  C++ "std::is_trivially_moveable` type-trait. Rust moves also render the
  moved-from object inaccessible.
- The C++ sense, where a move is really like a mutating `Clone` operation,
  which leave the moved-from value accessible to be destroyed at the end of
  the scope.

C++ also has *constructors*, which are special functions that produce a new
value in a particular location. In particular, C++ constructors may assume
that the address of `*this` will not change; all C++ objects are effectively
pinned and new objects must be constructed using copy or move constructors.

The [`New`], [`CopyNew`], and [`MoveNew`] traits bring these concepts
into Rust. A [`New`] is like a nilary [`FnOnce`], except that instead of
returning its result, it writes it to a `Pin<&mut MaybeUninit<T>>`, which is
in the "memory may be repurposed" state described in the
[`Pin` documentation] (i.e., either it is freshly allocated or the
destructor was recently run). This allows a [`New`] to rely on the
pointer's address remaining stable, much like `*this` in C++.

Types that implement [`CopyNew`] may be *copy-constructed*: given any
pointer to `T: CopyNew`, we can generate a constructor that constructs a
new, identical `T` at a designated location. [`MoveNew`] types may be
*move-constructed*: given an *owning* pointer (see [`DerefMove`]) to `T`,
we can generate a similar constructor, except that it also destroys the
`T` and the owning pointer's storage.

None of this violates the existing `Pin` guarantees: moving out of a
`Pin<P>` does not perform a move in the Rust sense, but rather in the C++
sense: it mutates through the pinned pointer in a safe manner to construct
a new `P::Target`, and then destroys the pointer and its contents.

In general, move-constructible types are going to want to be `!Unpin` so
that they can be self-referential. Self-referential types are one of the
primary motivations for move constructors.

## Constructors

A constructor is any type that implements [`New`]. Constructors are like
closures that have guaranteed RVO, which can be used to construct a
self-referential type in-place. To use the example from the `Pin<T>` docs:
```rust
use std::marker::PhantomPinned;
use std::mem::MaybeUninit;
use std::pin::Pin;
use std::ptr;
use std::ptr::NonNull;

use moveit::new;
use moveit::new::New;
use moveit::moveit;

// This is a self-referential struct because the slice field points to the
// data field. We cannot inform the compiler about that with a normal
// reference, as this pattern cannot be described with the usual borrowing
// rules. Instead we use a raw pointer, though one which is known not to be
// null, as we know it's pointing at the string.
struct Unmovable {
  data: String,
  slice: NonNull<String>,
  _pin: PhantomPinned,
}

impl Unmovable {
  // Defer construction until the final location is known.
  fn new(data: String) -> impl New<Output = Self> {
    new::of(Unmovable {
      data,
      // We only create the pointer once the data is in place
      // otherwise it will have already moved before we even started.
      slice: NonNull::dangling(),
      _pin: PhantomPinned,
    }).with(|this| unsafe {
      let this = this.get_unchecked_mut();
      this.slice = NonNull::from(&this.data);
    })

    // It is also possible to use other `new::` helpers, such as
    // `new::by` and `new::by_raw`, to configure construction behavior.
  }
}

// The constructor can't be used directly, and needs to be emplaced.
moveit! {
  let unmoved = Unmovable::new("hello".to_string());
}
// The pointer should point to the correct location,
// so long as the struct hasn't moved.
// Meanwhile, we are free to move the pointer around.
let mut still_unmoved = unmoved;
assert_eq!(still_unmoved.slice, NonNull::from(&still_unmoved.data));

// Since our type doesn't implement Unpin, this will fail to compile:
// let mut new_unmoved = Unmovable::new("world".to_string());
// std::mem::swap(&mut *still_unmoved, &mut *new_unmoved);

// However, we can implement `MoveNew` to allow it to be "moved" again.
```

The [`new`] module provides various helpers for making constructors. As a
rule, functions which, in Rust, would normally construct and return a value
should return `impl New` instead. This is analogous to have `async fn`s and
`.iter()` functions work.

## Emplacement

The example above makes use of the [`moveit!()`] macro, one of many ways to
turn a constructor into a value. `moveit` gives you two choices for running
a constructor:
- On the stack, using the [`MoveRef`] type (this is what [`moveit!()`]
  generates).
- On the heap, using the extension methods from the [`Emplace`] trait.

For example, we could have placed the above in a `Box` by writing
`Box::emplace(Unmovable::new())`.

[`Pin` documentation]: https://doc.rust-lang.org/std/pin/index.html#drop-guarantee

License: Apache-2.0

This is not an officially supported Google product.
