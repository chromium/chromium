# `shmem_providers`: Cross-Platform Shared Memory

<img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

This crate abstracts away the platform-specific details of shared memory implementation, allowing developers to write portable code that works across `Windows`, `Linux`, `macOS`, `Android`, and other `Unix`-like systems.

## Usage

Here is a basic example of how to use `shmem_providers` to create and use a shared memory region. This example demonstrates the single-process case. For sharing between processes, the `ShMemId` would be passed to the other process, which would then use `shmem_from_id` to map the shared memory.

```rust
use shmem_providers::{ShMemProvider, StdShMemProvider};

// Create a new shared memory provider.
let mut provider = StdShMemProvider::new().unwrap();

// Create a new shared memory region of 128 bytes.
let mut shared_mem = provider.new_shmem(128).unwrap();

// Write a message to the shared memory.
let message = "Hello, shared memory!";
shared_mem[..message.len()].copy_from_slice(message.as_bytes());
println!("Wrote: '{}'", message);

// Read the message back from shared memory.
let read_message = std::str::from_utf8(&shared_mem[..message.len()]).unwrap();
println!("Read:  '{}'", read_message);
assert_eq!(message, read_message);
```

## Core Abstractions

The crate exposes two main traits:

- **`ShMem`**: Represents a segment of shared memory. It behaves like a mutable byte slice (`&mut [u8]`), allowing direct memory manipulation. Each segment has a unique `ShMemId` that can be used to reopen it from another process.
- **`ShMemProvider`**: A factory for creating and accessing `ShMem` segments. It handles the OS-specific logic for allocating, mapping, and managing shared memory regions.

By using the `StdShMemProvider` type alias, you can use the default, recommended provider for the current target platform, simplifying the creation of cross-platform tools.

## Platform Implementations

`shmem_providers` automatically selects the best implementation for your target OS:

- **Windows**: Uses `CreateFileMappingA` and `MapViewOfFile` for shared memory.
- **Linux**: Prefers modern mechanisms like `memfd_create` where available, falling back to POSIX (`shm_open` and `mmap`) or System V IPC (`shmget`/`shmat`).
- **macOS**: Uses `shm_open` and `mmap`. Due to kernel behavior on macOS, it often requires a server-based approach (`ShMemService`) to reliably manage and clean up shared memory segments between processes.
- **Android**: Uses the `ashmem` subsystem, which is Android's specialized shared memory mechanism.

## Advanced Features

The crate also includes advanced abstractions for complex scenarios:

- **`ShMemService`**: A server-based provider for platforms that require a central process to manage shared memory lifecycles. This is particularly useful on macOS.
- **`RcShMemProvider`**: A reference-counted wrapper that allows a provider to be safely shared across threads and handles the necessary setup and teardown logic across `fork()` calls in multi-process applications.

## The `LibAFL` Project

The `LibAFL` project is part of [`AFLplusplus`](https://github.com/AFLplusplus) and maintained by

* [Andrea Fioraldi](https://twitter.com/andreafioraldi) <andrea@aflplus.plus>
* [Dominik Maier](https://bsky.app/profile/dmnk.bsky.social) <dominik@aflplus.plus>
* [s1341](https://twitter.com/srubenst1341) <github@shmarya.net>
* [Dongjia Zhang](https://github.com/tokatoka) <toka@aflplus.plus>
* [Addison Crump](https://github.com/addisoncrump) <me@addisoncrump.info>

## Contributing

For bugs, feel free to open issues or contact us directly. Thank you for your support. <3

Even though we will gladly assist you in finishing up your PR, try to

* keep all the crates compiling with *stable* rust (hide the eventual non-stable code under `cfg`s.)
* run `cargo nightly fmt` on your code before pushing
* check the output of `cargo clippy --all` or `./clippy.sh`
* run `cargo build --no-default-features` to check for `no_std` compatibility (and possibly add `#[cfg(feature = "std")]`) to hide parts of your code.

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