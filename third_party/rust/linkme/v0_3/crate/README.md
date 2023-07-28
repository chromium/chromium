## Linkme: safe cross-platform linker shenanigans

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/linkme-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/linkme)
[<img alt="crates.io" src="https://img.shields.io/crates/v/linkme.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/linkme)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-linkme-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs" height="20">](https://docs.rs/linkme)
[<img alt="build status" src="https://img.shields.io/github/actions/workflow/status/dtolnay/linkme/ci.yml?branch=master&style=for-the-badge" height="20">](https://github.com/dtolnay/linkme/actions?query=branch%3Amaster)

| Component | Linux | macOS | Windows | FreeBSD | illumos | Other...<sup>†</sup> |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| [Distributed slice] | 💚 | 💚 | 💚 | 💚 | 💚 | |

<b><sup>†</sup></b> We welcome PRs adding support for any platforms not listed
here.

[Distributed slice]: #distributed-slice

```toml
[dependencies]
linkme = "0.3"
```

*Supports rustc 1.62+*

<br>

# Distributed slice

A distributed slice is a collection of static elements that are gathered into a
contiguous section of the binary by the linker. Slice elements may be defined
individually from anywhere in the dependency graph of the final binary.

The implementation is based on `link_section` attributes and platform-specific
linker support. It does not involve life-before-main or any other runtime
initialization on any platform. This is a zero-cost safe abstraction that
operates entirely during compilation and linking.

### Declaration

A static distributed slice is declared by writing `#[distributed_slice]` on a
static item whose type is `[T]` for some type `T`. The initializer expression
must be `[..]` to indicate that elements come from elsewhere.

```rust
use linkme::distributed_slice;

#[distributed_slice]
pub static BENCHMARKS: [fn(&mut Bencher)] = [..];
```

### Elements

Slice elements may be registered into a distributed slice by a
`#[distributed_slice(...)]` attribute in which the path to the distributed slice
is given in the parentheses. The initializer is required to be a const
expression.

```rust
use linkme::distributed_slice;
use other_crate::BENCHMARKS;

#[distributed_slice(BENCHMARKS)]
static BENCH_DESERIALIZE: fn(&mut Bencher) = bench_deserialize;

fn bench_deserialize(b: &mut Bencher) {
    /* ... */
}
```

Elements may be defined in the same crate that declares the distributed slice,
or in any downstream crate. Elements across all crates linked into the final
binary will be observed to be present in the slice at runtime.

The distributed slice behaves in all ways like `&'static [T]`.

```rust
fn main() {
    // Iterate the elements.
    for bench in BENCHMARKS {
        /* ... */
    }

    // Index into the elements.
    let first = BENCHMARKS[0];

    // Slice the elements.
    let except_first = &BENCHMARKS[1..];

    // Invoke methods on the underlying slice.
    let len = BENCHMARKS.len();
}
```

The compiler will require that the static element type matches with the element
type of the distributed slice. If the two do not match, the program will not
compile:

```rust
#[distributed_slice(BENCHMARKS)]
static BENCH_WTF: usize = 999;
```

```console
error[E0308]: mismatched types
  --> src/distributed_slice.rs:65:19
   |
17 | static BENCH_WTF: usize = 999;
   |                   ^^^^^ expected fn pointer, found `usize`
   |
   = note: expected fn pointer `fn(&mut other_crate::Bencher)`
                    found type `usize`
```

### Function elements

As a shorthand for the common case of distributed slices containing function
pointers, the distributed\_slice attribute may be applied directly to a function
definition to place a pointer to that function into a distributed slice.

```rust
use linkme::distributed_slice;

#[distributed_slice]
pub static BENCHMARKS: [fn(&mut Bencher)] = [..];

// Equivalent to:
//
//    #[distributed_slice(BENCHMARKS)]
//    static _: fn(&mut Bencher) = bench_deserialize;
//
#[distributed_slice(BENCHMARKS)]
fn bench_deserialize(b: &mut Bencher) {
    /* ... */
}
```

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
