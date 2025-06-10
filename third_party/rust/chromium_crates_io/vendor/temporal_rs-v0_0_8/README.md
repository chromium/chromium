# Temporal in Rust

Temporal is a calendar and timezone aware date/time builtin currently
proposed for addition to the ECMAScript specification.

`temporal_rs` is an implementation of Temporal in Rust that aims to be
100% test compliant. While initially developed for [Boa][boa-repo], the
crate has been externalized as we intended to make an engine agnostic
and general usage implementation of Temporal and its algorithms.

## Example usage

Below are a few examples to give an overview of `temporal_rs`'s current
API.

### Convert from an ISO8601 `PlainDate` into a Japanese `PlainDate`.

```rust
use temporal_rs::{PlainDate, Calendar};
use tinystr::tinystr;
use core::str::FromStr;
// Create a date with an ISO calendar
let iso8601_date = PlainDate::try_new_iso(2025, 3, 3).unwrap();
// Create a new date with the japanese calendar
let japanese_date = iso8601_date.with_calendar(Calendar::from_str("japanese").unwrap()).unwrap();
assert_eq!(japanese_date.era(), Some(tinystr!(16, "reiwa")));
assert_eq!(japanese_date.era_year(), Some(7));
assert_eq!(japanese_date.month(), 3)
```

### Create a `PlainDateTime` from a RFC9557 IXDTF string.

For more information on the Internet Extended DateTime Format (IXDTF),
see [RFC9557](https://www.rfc-editor.org/rfc/rfc9557.txt).

```rust
use temporal_rs::PlainDateTime;
use core::str::FromStr;

let pdt = PlainDateTime::from_str("2025-03-01T11:16:10[u-ca=gregory]").unwrap();
assert_eq!(pdt.calendar().identifier(), "gregory");
assert_eq!(pdt.year(), 2025);
assert_eq!(pdt.month(), 3);
assert_eq!(pdt.day(), 1);
assert_eq!(pdt.hour(), 11);
assert_eq!(pdt.minute(), 16);
assert_eq!(pdt.second(), 10);
```

### Create a `ZonedDateTime` for a RFC9557 IXDTF string.

**Important Note:** The below API is enabled with the `compiled_data` feature flag.

```rust
use temporal_rs::{ZonedDateTime, TimeZone};
use temporal_rs::options::{Disambiguation, OffsetDisambiguation};

let zdt = ZonedDateTime::from_str("2025-03-01T11:16:10Z[America/Chicago][u-ca=iso8601]", Disambiguation::Compatible, OffsetDisambiguation::Reject).unwrap();
assert_eq!(zdt.year().unwrap(), 2025);
assert_eq!(zdt.month().unwrap(), 3);
assert_eq!(zdt.day().unwrap(), 1);
assert_eq!(zdt.hour().unwrap(), 11);
assert_eq!(zdt.minute().unwrap(), 16);
assert_eq!(zdt.second().unwrap(), 10);

let zurich_zone = TimeZone::try_from_str("Europe/Zurich").unwrap();
let zdt_zurich = zdt.with_timezone(zurich_zone).unwrap();
assert_eq!(zdt_zurich.year().unwrap(), 2025);
assert_eq!(zdt_zurich.month().unwrap(), 3);
assert_eq!(zdt_zurich.day().unwrap(), 1);
assert_eq!(zdt_zurich.hour().unwrap(), 18);
assert_eq!(zdt_zurich.minute().unwrap(), 16);
assert_eq!(zdt_zurich.second().unwrap(), 10);
```

## Temporal proposal

Relevant links and information regarding Temporal can be found below.

- [Temporal MDN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal)
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

## Test262 Conformance

<!-- TODO: Potentially update with tests if a runner can be implemented -->

The `temporal_rs`'s current conformance results can be viewed on
Boa's [test262 conformance page](https://boajs.dev/conformance).

## FFI

`temporal_rs` currently has bindings for C++, available via the
`temporal_capi` crate.

## Communication

Feel free to contact us on
[Matrix](https://matrix.to/#/#boa:matrix.org).

## License

This project is licensed under the [Apache](./LICENSE-Apache) or
[MIT](./LICENSE-MIT) licenses, at your option.

[boa-repo]: https://github.com/boa-dev/boa
