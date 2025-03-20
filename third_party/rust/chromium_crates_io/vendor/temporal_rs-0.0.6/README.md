# Temporal in Rust

Temporal is a calendar and timezone aware date/time builtin currently
proposed for edition to the ECMAScript specification.

`temporal_rs` is an implementation of Temporal in Rust that aims to be
100% test compliant. While initially developed for [Boa][boa-repo], the
crate has been externalized as we intended to make an engine agnostic
and general usage implementation of Temporal and its algorithms.

## Example usage

```rust
use temporal_rs::{PlainDate, Calendar};
use tinystr::tinystr;
use core::str::FromStr;

// Create a date with an ISO calendar
let iso8601_date = PlainDate::try_new(2025, 3, 3, Calendar::default()).unwrap();

// Create a new date with the japanese calendar
let japanese_date = iso8601_date.with_calendar(Calendar::from_str("japanese").unwrap()).unwrap();
assert_eq!(japanese_date.era(), Some(tinystr!(16, "reiwa")));
assert_eq!(japanese_date.era_year(), Some(7));
assert_eq!(japanese_date.month(), 3)
```

## Temporal proposal

Relevent links regarding Temporal can be found below.

- [Temporal Documentation](https://tc39.es/proposal-temporal/docs/)
- [Temporal Proposal Specification](https://tc39.es/proposal-temporal/)
- [Temporal Proposal Repository](https://github.com/tc39/proposal-temporal)

## Core maintainers

- Jason Williams
  ([jasonwilliams](https://github.com/orgs/boa-dev/people/jasonwilliams))
- José Julián Espina
  ([jedel1043](https://github.com/orgs/boa-dev/people/jedel1043))
- Kevin Ness ([nekevss](https://github.com/orgs/boa-dev/people/nekevss))
- Boa Developers

## Contributing

This project is open source and welcomes anyone interested to
participate. Please see [CONTRIBUTING.md](./CONTRIBUTING.md) for more
information.

## Communication

Feel free to contact us on
[Matrix](https://matrix.to/#/#boa:matrix.org).

## License

This project is licensed under the [Apache](./LICENSE-Apache) or
[MIT](./LICENSE-MIT) licenses, at your option.

[boa-repo]: https://github.com/boa-dev/boa
