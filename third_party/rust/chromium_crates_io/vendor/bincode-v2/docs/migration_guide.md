# Migrating from bincode 1 to 2

Bincode 2 now has an optional dependency on `serde`. You can either use `serde`, or use bincode's own `derive` feature and macros.

## From `Options` to `Configuration`

Bincode 1 had the [Options](https://docs.rs/bincode/1/bincode/config/trait.Options.html) trait. This has been replaced with the [Configuration](https://docs.rs/bincode/2/bincode/config/struct.Configuration.html) struct.

If you're using `Options`, you can change it like this:

```rust,ignore
# old
bincode_1::DefaultOptions::new().with_varint_encoding()

# new
bincode_2::config::legacy().with_variable_int_encoding()
```

If you want to be compatible with bincode 1, use the following table:

| Bincode 1                                                              | Bincode 2                                       |
| ---------------------------------------------------------------------- | ----------------------------------------------- |
| version 1.0 - 1.2 with `bincode_1::DefaultOptions::new().serialize(T)` | `config::legacy()`                              |
| version 1.3+ with `bincode_1::DefaultOptions::new().serialize(T)`      | `config::legacy().with_variable_int_encoding()` |
| No explicit `Options`, e.g. `bincode::serialize(T)`                    | `config::legacy()`                              |

If you do not care about compatibility with bincode 1, we recommend using `config::standard()`

The following changes have been made:

- `.with_limit(n)` has been changed to `.with_limit::<n>()`.
- `.with_native_endian()` has been removed. Use `.with_big_endian()` or `with_little_endian()` instead.
- `.with_varint_encoding()` has been renamed to `.with_variable_int_encoding()`.
- `.with_fixint_encoding()` has been renamed to `.with_fixed_int_encoding()`.
- `.reject_trailing_bytes()` has been removed.
- `.allow_trailing_bytes()` has been removed.
- You can no longer (de)serialize from the `Options` trait directly. Use one of the `encode_` or `decode_` methods.

Because of confusion with `Options` defaults in bincode 1, we have made `Configuration` mandatory in all calls in bincode 2.

## Migrating with `serde`

You may wish to stick with `serde` when migrating to bincode 2, for example if you are using serde-exclusive derive features such as `#[serde(deserialize_with)]`.

If so, make sure to include bincode 2 with the `serde` feature enabled, and use the `bincode::serde::*` functions instead of `bincode::*` as described below:

```toml
[dependencies]
bincode = { version = "2.0", features = ["serde"] }

# Optionally you can disable the `derive` feature:
# bincode = { version = "2.0", default-features = false, features = ["std", "serde"] }
```

Then replace the following functions: (`Configuration` is `bincode::config::legacy()` by default)

| Bincode 1                                       | Bincode 2                                                                                                                       |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| `bincode::deserialize(&[u8])`                   | `bincode::serde::decode_from_slice(&[u8], Configuration)`<br />`bincode::serde::borrow_decode_from_slice(&[u8], Configuration)` |
| `bincode::deserialize_from(std::io::Read)`      | `bincode::serde::decode_from_std_read(std::io::Read, Configuration)`                                                            |
| `bincode::deserialize_from_custom(BincodeRead)` | `bincode::serde::decode_from_reader(Reader, Configuration)`                                                                     |
|                                                 |                                                                                                                                 |
| `bincode::serialize(T)`                         | `bincode::serde::encode_to_vec(T, Configuration)`<br />`bincode::serde::encode_into_slice(T, &mut [u8], Configuration)`         |
| `bincode::serialize_into(std::io::Write, T)`    | `bincode::serde::encode_into_std_write(T, std::io::Write, Configuration)`                                                       |
| `bincode::serialized_size(T)`                   | Currently not implemented                                                                                                       |

## Migrating from `serde` to `bincode-derive`

`bincode-derive` is enabled by default. If you're using `default-features = false`, make sure to add `features = ["derive"]` to your `Cargo.toml`.

```toml,ignore
[dependencies]
bincode = "2.0"

# If you need `no_std` with `alloc`:
# bincode = { version = "2.0", default-features = false, features = ["derive", "alloc"] }

# If you need `no_std` and no `alloc`:
# bincode = { version = "2.0", default-features = false, features = ["derive"] }
```

Replace or add the following attributes. You are able to use both `serde-derive` and `bincode-derive` side-by-side.

| serde-derive                    | bincode-derive               |
| ------------------------------- | ---------------------------- |
| `#[derive(serde::Serialize)]`   | `#[derive(bincode::Encode)]` |
| `#[derive(serde::Deserialize)]` | `#[derive(bincode::Decode)]` |

**note:** To implement these traits manually, see the documentation of [Encode](https://docs.rs/bincode/2/bincode/enc/trait.Encode.html) and [Decode](https://docs.rs/bincode/2/bincode/de/trait.Decode.html).

**note:** For more information on using `bincode-derive` with external libraries, see [below](#bincode-derive-and-libraries).

Then replace the following functions: (`Configuration` is `bincode::config::legacy()` by default)

| Bincode 1                                       | Bincode 2                                                                                                          |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `bincode::deserialize(&[u8])`                   | `bincode::decode_from_slice(&bytes, Configuration)`<br />`bincode::borrow_decode_from_slice(&[u8], Configuration)` |
| `bincode::deserialize_from(std::io::Read)`      | `bincode::decode_from_std_read(std::io::Read, Configuration)`                                                      |
| `bincode::deserialize_from_custom(BincodeRead)` | `bincode::decode_from_reader(Reader, Configuration)`                                                               |
|                                                 |                                                                                                                    |
| `bincode::serialize(T)`                         | `bincode::encode_to_vec(T, Configuration)`<br />`bincode::encode_into_slice(t: T, &mut [u8], Configuration)`       |
| `bincode::serialize_into(std::io::Write, T)`    | `bincode::encode_into_std_write(T, std::io::Write, Configuration)`                                                 |
| `bincode::serialized_size(T)`                   | Currently not implemented                                                                                          |

### Bincode derive and libraries

Currently not many libraries support the traits `Encode` and `Decode`. There are a couple of options if you want to use `#[derive(bincode::Encode, bincode::Decode)]`:

- Enable the `serde` feature and add a `#[bincode(with_serde)]` above each field that implements `serde::Serialize/Deserialize` but not `Encode/Decode`
- Enable the `serde` feature and wrap your field in [bincode::serde::Compat](https://docs.rs/bincode/2/bincode/serde/struct.Compat.html) or [bincode::serde::BorrowCompat](https://docs.rs/bincode/2/bincode/serde/struct.BorrowCompat.html)
- Make a pull request to the library:
  - Make sure to be respectful, most of the developers are doing this in their free time.
  - Add a dependency `bincode = { version = "2.0", default-features = false, optional = true }` to the `Cargo.toml`
  - Implement [Encode](https://docs.rs/bincode/2/bincode/enc/trait.Encode.html)
  - Implement [Decode](https://docs.rs/bincode/2/bincode/de/trait.Decode.html)
  - Make sure both of these implementations have a `#[cfg(feature = "bincode")]` attribute.
