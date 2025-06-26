# tzif [![crates.io](https://img.shields.io/crates/v/tzif)](https://crates.io/crates/tzif)

<!-- cargo-rdme start -->

A parser for [Time Zone Information Format (`TZif`)](https://tools.ietf.org/id/draft-murchison-tzdist-tzif-00.html) files.

Also includes a parser for [POSIX time-zone strings](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html),
which is used by the TZif parser, but also available separately.

Resources to generate `TZif` files are provided by the [IANA database](https://www.iana.org/time-zones).
`TZif` files are also included in some operating systems.

## Examples

#### Parse TZif Files
```rust
let data = tzif::parse_tzif_file(Path::new("path_to_file")).unwrap();
```

#### Parse POSIX time-zone strings
```rust
let data =
    tzif::parse_posix_tz_string(b"WGT3WGST,M3.5.0/-2,M10.5.0/-1").unwrap();
```

<!-- cargo-rdme end -->

## More Information

For more information on development, authorship, contributing etc. please visit [`ICU4X home page`](https://github.com/unicode-org/icu4x).
