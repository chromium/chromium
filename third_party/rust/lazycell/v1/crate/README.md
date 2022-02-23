# lazycell

<table>
    <tr>
        <td><strong>Linux</strong></td>
        <td><a href="https://travis-ci.org/indiv0/lazycell" title="Travis Build Status"><img src="https://travis-ci.org/indiv0/lazycell.svg?branch=master" alt="travis-badge"></img></a></td>
    </tr>
    <tr>
        <td colspan="2">
            <a href="https://crates.io/crates/lazycell" title="Crates.io downloads"><img src="https://img.shields.io/crates/d/lazycell.svg" alt="cargo-downloads-badge"></img></a>
            <a href="https://indiv0.github.io/lazycell/lazycell" title="API Docs"><img src="https://img.shields.io/badge/API-docs-blue.svg" alt="api-docs-badge"></img></a>
            <a href="https://crates.io/crates/lazycell" title="Crates.io"><img src="https://img.shields.io/crates/v/lazycell.svg" alt="crates-io"></img></a>
            <a href="#license" title="License: MIT/Apache-2.0"><img src="https://img.shields.io/crates/l/lazycell.svg" alt="license-badge"></img></a>
            <a href="https://coveralls.io/github/indiv0/lazycell?branch=master" title="Coverage Status"><img src="https://coveralls.io/repos/github/indiv0/lazycell/badge.svg?branch=master" alt="coveralls-badge"></img></a>
        </td>
    </tr>
</table>

Rust library providing a lazily filled Cell.

# Table of Contents

* [Usage](#usage)
* [Contributing](#contributing)
* [Credits](#credits)
* [License](#license)

## Usage

Add the following to your `Cargo.toml`:

```toml
[dependencies]
lazycell = "1.3"
```

And in your `lib.rs` or `main.rs`:

```rust
extern crate lazycell;
```

See the [API docs][api-docs] for information on using the crate in your library.

## Contributing

Contributions are always welcome!
If you have an idea for something to add (code, documentation, tests, examples,
etc.) feel free to give it a shot.

Please read [CONTRIBUTING.md][contributing] before you start contributing.

## Credits

The LazyCell library is based originally on work by The Rust Project Developers
for the project [crates.io][crates-io-repo].

The list of contributors to this project can be found at
[CONTRIBUTORS.md][contributors].

## License

LazyCell is distributed under the terms of both the MIT license and the Apache
License (Version 2.0).

See [LICENSE-APACHE][license-apache], and [LICENSE-MIT][license-mit] for details.

[api-docs]: https://indiv0.github.io/lazycell/lazycell
[contributing]: https://github.com/indiv0/lazycell/blob/master/CONTRIBUTING.md "Contribution Guide"
[contributors]: https://github.com/indiv0/lazycell/blob/master/CONTRIBUTORS.md "List of Contributors"
[crates-io-repo]: https://github.com/rust-lang/crates.io "rust-lang/crates.io: Source code for crates.io"
[license-apache]: https://github.com/indiv0/lazycell/blob/master/LICENSE-APACHE "Apache-2.0 License"
[license-mit]: https://github.com/indiv0/lazycell/blob/master/LICENSE-MIT "MIT License"
