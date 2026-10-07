# rand_core: core random number generation traits

<div class="badges">
  
[![crate][crate-badge]][crate-link]
[![Docs][docs-image]][docs-link]
[![Apache2/MIT licensed][license-image]][license-link]
[![Build Status][build-image]][build-link]

</div>

This crate provides a collection of traits used by implementations of Random Number Generation (RNG)
algorithms. Additionally, it includes helper utilities that assist with the implementation
of these traits.

Note that the traits focus solely on the core RNG functionality. Most users should prefer
the [`rand`] crate, which offers more advanced RNG capabilities built on these core traits,
such as sampling from restricted ranges, generating floating-point numbers, list permutations,
and more.

[`rand`]: https://docs.rs/rand

## License

The crate is licensed under either of:

* [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
* [MIT license](http://opensource.org/licenses/MIT)

at your option.

[//]: # (badges)

[crate-badge]: https://img.shields.io/crates/v/rand_core.svg
[crate-link]: https://crates.io/crates/rand_core
[docs-image]: https://docs.rs/rand_core/badge.svg
[docs-link]: https://docs.rs/rand_core
[license-image]: https://img.shields.io/badge/license-Apache2.0/MIT-blue.svg
[build-image]: https://github.com/rust-random/rand_core/actions/workflows/test.yml/badge.svg?branch=master
[license-link]: #license
[build-link]: https://github.com/rust-random/rand_core/actions/workflows/test.yml?query=branch:master
