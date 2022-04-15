# Autocxx

[![GitHub](https://img.shields.io/crates/l/autocxx)](https://github.com/google/autocxx)
[![crates.io](https://img.shields.io/crates/d/autocxx)](https://crates.io/crates/autocxx)
[![docs.rs](https://docs.rs/autocxx/badge.svg)](https://docs.rs/autocxx)

This project is a tool for calling C++ from Rust in a heavily automated, but safe, fashion.

The intention is that it has all the fluent safety from [cxx](https://cxx.rs) whilst generating interfaces automatically from existing C++ headers using a variant of [bindgen](https://docs.rs/bindgen/latest/bindgen/). Think of autocxx as glue which plugs bindgen into cxx.

For full documentation, see [the manual](https://google.github.io/autocxx/).

# Overview

```rust,ignore
autocxx::include_cpp! {
    #include "url/origin.h"
    generate!("url::Origin")
    safety!(unsafe_ffi)
}

fn main() {
    let o = ffi::url::Origin::CreateFromNormalizedTuple("https",
        "google.com", 443);
    let uri = o.Serialize();
    println!("URI is {}", uri.to_str().unwrap());
}
```

#### License and usage notes

This is not an officially supported Google product.

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>
