# Changelog

3.0.10

* Minor epoch update policiy optimization.
* Minor `NonNull` optimization on `Owned` and `Shared`.

3.0.9

* Fix unsound `Sync` implementations of `AtomicShared` and `Shared`; previously, the `Sync` implementation allowed an arbitrary thread to own/drop the contained instance.

3.0.8

* Minor `const` optimization.

3.0.7

* Fix a use-after-free issue when thread-local storage is dropped.

3.0.5

* Fix minor linting errors.

3.0.4

* Adjust tests to be more Miri friendly.

3.0.3

* Fix a rare memory ordering issue when dropping thread-local storage.

3.0.2

* Make `SDD` much more friendly to Miri.

3.0.1

* Compatible with the [`Miri`](https://github.com/rust-lang/miri) memory leak checker.
* Make `Collectible` private since it is unsafe.
* Remove `Guard::defer` which depends on `Collectible`.
* Remove `prepare`.

2.1.0

* Minor performance optimization.
* Remove `Owned::release`.

2.0.0

* `{Owned, Shared}::release` no longer receives a `Guard`.
* `Link` is now public.

1.7.0

* Add `loom` support.

1.6.0

* Add `Guard::accelerate`.

1.5.0

* Fix `Guard::epoch` to return the correct epoch value.

1.4.0

* `Epoch` is now a 4-state type (3 -> 4).

1.3.0

* Add `Epoch`
* Add `Guard::epoch`.

1.2.0

* Remove `Collectible::drop_and_dealloc`.

1.1.0

* Add `prepare`.

1.0.1

* Relax trait bounds of `Guard::defer_execute`.

1.0.0

* Minor code cleanup.

0.2.0

* Make `Guard` `UnwindSafe`.

0.1.0

* Minor optimization.

0.0.1

* Initial commit: code copied from [`scalable-concurrent-containers`](https://github.com/wvwwvwwv/scalable-concurrent-containers).
