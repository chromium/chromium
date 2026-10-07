# `Core_Affinity2`: Manage CPU affinities even harder

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

A crate to manage CPU core affinity for threads, maintained as part of the [LibAFL](https://github.com/AFLplusplus/LibAFL) project.

`core_affinity2` allows you to get the list of available cores on a system and to pin the current thread to a specific core.

Pinning threads to cores can improve performance by reducing cache misses and avoiding thread migration between cores. This is particularly useful in performance-sensitive applications like fuzzing, scientific computing, and real-time systems.

This crate is a fork of the original [`core_affinity`](https://crates.io/crates/core_affinity) crate, with updates and continued maintenance.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
core_affinity2 = "0.16.1" # Replace with the latest version
```

### Example: Pinning threads to cores

Here is an example of how to get the available core IDs and spawn a thread for each, pinning it to the respective core.

```rust
use std::thread;
use core_affinity2::{get_core_ids, CoreId};

// Get the available core IDs
if let Ok(core_ids) = get_core_ids() {
    let core_count = core_ids.len();
    println!("Found {} cores:", core_count);

    let handles: Vec<_> = core_ids.into_iter().map(|id| {
        thread::spawn(move || {
            // Pin this thread to a single CPU core.
            if id.set_affinity().is_ok() {
                println!("Thread {:?} is running on core {:?}", thread::current().id(), id);
                // Do some work here
            } else {
                eprintln!("Could not pin thread to core {:?}", id);
            }
        })
    }).collect();

    for handle in handles {
        handle.join().unwrap();
    }
} else {
    println!("Could not get core IDs.");
}
```

### Parsing core ranges

The `Cores` struct provides a convenient way to work with a set of cores, including parsing from a command-line string.

```rust
use core_affinity2::Cores;

// Parse a comma-separated list of cores and ranges
let cores = Cores::from_cmdline("0,2-4,7").unwrap();
assert_eq!(cores.ids, vec![0.into(), 2.into(), 3.into(), 4.into(), 7.into()]);

// "all" will use all available cores
// let all_cores = Cores::from_cmdline("all").unwrap();
```

## Supported Platforms

`core_affinity2` is cross-platform and supports the following operating systems:

- Linux
- Windows
- macOS (`x86_64` and `aarch64`)
- FreeBSD
- NetBSD
- OpenBSD
- Dragonfly BSD
- Solaris / Illumos
- Haiku

Note that on some platforms (like macOS on aarch64), it's not possible to pin a thread to a specific core, but the library will still try to request the highest performance for the thread.

## `no_std` Support

This crate has `no_std` support.

## Contributing

Contributions are welcome! Please see the main [LibAFL `CONTRIBUTING.md`](https://github.com/AFLplusplus/LibAFL/blob/main/CONTRIBUTING.md) for more information.

## License

This crate is licensed under either of the [MIT License](LICENSE-MIT) or the [Apache License 2.0](LICENSE-APACHE).