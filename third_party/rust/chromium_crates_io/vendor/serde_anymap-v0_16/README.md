# `Serde_AnyMap`: A serializable map that stores and retrieves elements by type

<img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

`SerdeAnyMap` provides map-like data structures that can store values of different types and can be serialized and deserialized with `Serde`. The values are stored and retrieved using their `TypeId` as the key, making it a powerful tool for dynamic, type-safe data storage.

There are two main types provided by this crate:

- `SerdeAnyMap`: A simple map from `TypeId` to a value of that type.
- `NamedSerdeAnyMap`: A map from `TypeId` to a further map of `String` names to values, allowing you to store multiple instances of the same type under different names.

## How to Use

### 1. Add `serde_anymap` to your dependencies

```toml
[dependencies]
serde_anymap = "0.1.0"
# For automatic type registration (recommended)
serde_anymap = { version = "0.1.0", features = ["serdeany_autoreg"] }
```

### 2. Define Your Types and Implement `SerdeAny`

To be stored in a `SerdeAnyMap`, your types must implement the `SerdeAny` trait. The easiest way to do this is using the `#[derive(SerdeAny)]` macro from the [`LibAFL_Derive`](https://crates.io/crates/libafl_derive) crate or by using the `impl_serdeany!` macro on the type, instead. Your types must also derive `Serialize` and `Deserialize`.

```rust
use serde::{Serialize, Deserialize};
use serde_anymap::impl_serdeany;

#[derive(Debug, Serialize, Deserialize, PartialEq)]
struct MyConfig {
    is_enabled: bool,
    port: u16,
}
impl_serdeany!(MyConfig);

#[derive(Debug, Serialize, Deserialize, PartialEq)]
struct UserData(String);
impl_serdeany!(UserData);
```

### 3. Use `SerdeAnyMap`

You can now insert instances of your types into the map and retrieve them by type.

```rust
# use serde::{Serialize, Deserialize};
# use serde_anymap::impl_serdeany;
# #[derive(Debug, Serialize, Deserialize, PartialEq)]
# struct MyConfig { is_enabled: bool, port: u16 }
# impl_serdeany!(MyConfig);
# #[derive(Debug, Serialize, Deserialize, PartialEq)]
# struct UserData(String);
# impl_serdeany!(UserData);
use serde_anymap::serdeany::SerdeAnyMap;

let mut map = SerdeAnyMap::new();

map.insert(MyConfig { is_enabled: true, port: 8080 });
map.insert(UserData("John Doe".to_string()));

// Retrieve values by type
assert_eq!(map.get::<MyConfig>().unwrap().port, 8080);
assert_eq!(map.get::<UserData>().unwrap().0, "John Doe");

// You can also get mutable access
let config = map.get_mut::<MyConfig>().unwrap();
config.port = 9090;
assert_eq!(map.get::<MyConfig>().unwrap().port, 9090);
```

### 4. Serialization and Deserialization

The real power of `serde_anymap` is its ability to serialize and deserialize these heterogeneous maps.

**Important:** For deserialization to work, the types you are deserializing must be registered. See the "Type Registration" section below.

```rust
# use serde::{Serialize, Deserialize};
# use serde_anymap::{impl_serdeany, serdeany::{SerdeAnyMap, RegistryBuilder}};
# #[derive(Debug, Serialize, Deserialize, PartialEq)]
# struct MyConfig { is_enabled: bool, port: u16 }
# impl_serdeany!(MyConfig);
# #[derive(Debug, Serialize, Deserialize, PartialEq)]
# struct UserData(String);
# impl_serdeany!(UserData);
# let mut map = SerdeAnyMap::new();
# map.insert(MyConfig { is_enabled: true, port: 8080 });
# map.insert(UserData("John Doe".to_string()));
// This is only needed if you don't use the `serdeany_autoreg` feature
#[cfg(not(feature = "serdeany_autoreg"))]
unsafe {
    RegistryBuilder::register::<MyConfig>();
    RegistryBuilder::register::<UserData>();
}

// Serialize the map to a JSON string
let serialized = serde_json::to_string_pretty(&map).unwrap();
println!("{}", serialized);

// Deserialize it back
let deserialized: SerdeAnyMap = serde_json::from_str(&serialized).unwrap();

assert!(deserialized.get::<MyConfig>().is_some());
assert_eq!(deserialized.get::<MyConfig>().unwrap(), map.get::<MyConfig>().unwrap());
assert_eq!(deserialized.get::<UserData>().unwrap(), map.get::<UserData>().unwrap());
```

## Type Registration

For `serde_anymap` to deserialize a generic `dyn SerdeAny` trait object, it needs a way to map a serialized type identifier back to a concrete type. This is done via a global type registry.

### Automatic Registration (Recommended)

The easiest way to handle registration is to enable the `serdeany_autoreg` feature. This uses the [`ctor`](https://crates.io/crates/ctor) crate to automatically run registration code for each type when your program starts. The `impl_serdeany!` macro handles this for you.

```toml
# In your Cargo.toml
serde_anymap = { version = "0.1.0", features = ["serdeany_autoreg"] }
```

With this feature, you don't need to do anything else. Just use `impl_serdeany!` and it works.

### Manual Registration

If you cannot use `serdeany_autoreg`, you must register your types manually at the start of your program.

```rust,ignore
use serde_anymap::serdeany::RegistryBuilder;

// This must be done before any deserialization happens.
// It is safe to call multiple times.
unsafe {
    RegistryBuilder::register::<MyConfig>();
    RegistryBuilder::register::<UserData>();
}

// Your application logic...
```

## `NamedSerdeAnyMap`

If you need to store multiple objects of the same type, you can use `NamedSerdeAnyMap`, which adds a string name as a key.

```rust
# use serde::{Serialize, Deserialize};
# use serde_anymap::{impl_serdeany, serdeany::{NamedSerdeAnyMap, RegistryBuilder}};
# #[derive(Debug, Serialize, Deserialize, PartialEq)]
# struct UserData(String);
# impl_serdeany!(UserData);
# #[cfg(not(feature = "serdeany_autoreg"))]
# unsafe { RegistryBuilder::register::<UserData>(); }
let mut named_map = NamedSerdeAnyMap::new();

named_map.insert("user1", UserData("Alice".to_string()));
named_map.insert("user2", UserData("Bob".to_string()));

assert_eq!(named_map.get::<UserData>("user1").unwrap().0, "Alice");
assert_eq!(named_map.get::<UserData>("user2").unwrap().0, "Bob");

let serialized = serde_json::to_string(&named_map).unwrap();
let deserialized: NamedSerdeAnyMap = serde_json::from_str(&serialized).unwrap();

assert_eq!(deserialized.get::<UserData>("user1").unwrap().0, "Alice");
```

## Features

- `serdeany_autoreg`: Enables automatic type registration at program startup. Highly recommended.
- `stable_anymap`: Uses the type name (`&'static str`) as the key instead of `TypeId`. This makes the serialized output more stable across different compilations, but it can be slightly slower and may cause issues if you have types with the same name in different modules.

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

- [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
- [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
- [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
- [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
- [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

- keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
- run `cargo nightly fmt` on your code before pushing
- check the output of `cargo clippy --all` or `./clippy.sh`
- run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

Some parts in this list may sound hard, but don't be afraid to open a PR if you cannot fix them by yourself. We will gladly assist.

#### License

<sup>
Licensed under either of <a href="../LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="../LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>

<br>

<sub>
Dependencies under more restrictive licenses, such as GPL or AGPL, can be enabled
using the respective feature in each crate when it is present, such as the
'agpl' feature of the libafl crate.
</sub>
