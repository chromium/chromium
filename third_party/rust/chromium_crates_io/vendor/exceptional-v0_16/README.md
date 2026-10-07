# `Exceptional`: A handy library to handle OS Signals and Exception

 <img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `exceptional` crate, part of [`LibAFL`](https://github.com/AFLplusplus/LibAFL), exposes (very!) low-level features to handle exceptions on Unix and Windows operating systems.

It provides a unified interface for handling signals on Unix-like systems and exceptions on Windows, allowing for cross-platform exception handling.

## Features

### Unix

- **Signal Handling**: The `Signal` enum and `SignalHandler` trait allow you to handle Unix signals like `SIGSEGV`, `SIGILL`, `SIGFPE`, etc.
- **Custom Signal Stack**: Automatically sets up a custom signal stack to handle stack overflows.
- **ucontext Access**: Provides access to the `ucontext_t` for inspecting the process state at the time of the signal.

### Windows

- **Exception Handling**: The `ExceptionCode` enum and `ExceptionHandler` trait allow you to handle Windows exceptions like `EXCEPTION_ACCESS_VIOLATION`, `EXCEPTION_ILLEGAL_INSTRUCTION`, etc.
- **Vectored Exception Handling (VEH)**: Uses Vectored Exception Handling to catch exceptions.
- **Console Control Events**: The `CtrlHandler` trait allows handling console events like `CTRL_C_EVENT`.

## Usage

### Unix Signal Handling

```rust,no_run
use exceptional::unix_signals::{Signal, SignalHandler, setup_signal_handler};
use libc::{siginfo_t, ucontext_t};
use std::ptr;

struct MySignalHandler;

impl SignalHandler for MySignalHandler {
    unsafe fn handle(&mut self, signal: Signal, _info: &mut siginfo_t, _context: Option<&mut ucontext_t>) {
        println!("Caught signal: {:?}", signal);
        // In a real scenario, you might longjmp or do something else to recover.
        std::process::exit(0);
    }

    fn signals(&self) -> Vec<Signal> {
        vec![Signal::SigSegmentationFault, Signal::SigIllegalInstruction]
    }
}

let mut handler = MySignalHandler;
unsafe {
    setup_signal_handler(&mut handler).expect("Failed to set up signal handler");
}

// Trigger a segfault
unsafe {
    let ptr = ptr::null_mut::<i32>();
    *ptr = 42;
}
```

### Windows Exception Handling

```rust,ignore
use exceptional::windows_exceptions::{ExceptionCode, ExceptionHandler, setup_exception_handler};
use windows::Win32::System::Diagnostics::Debug::EXCEPTION_POINTERS;
use std::ptr;

struct MyExceptionHandler;

impl ExceptionHandler for MyExceptionHandler {
    fn handle(&mut self, exception_code: ExceptionCode, _exception_pointers: *mut EXCEPTION_POINTERS) {
        println!("Caught exception: {:?}", exception_code);
        // In a real scenario, you might want to inspect the exception pointers and recover.
        std::process::exit(0);
    }

    fn exceptions(&self) -> Vec<ExceptionCode> {
        vec![ExceptionCode::AccessViolation, ExceptionCode::IllegalInstruction]
    }
}

let mut handler = MyExceptionHandler;
unsafe {
    setup_exception_handler(&mut handler).expect("Failed to set up exception handler");
}

// Trigger an access violation
unsafe {
    let ptr = ptr::null_mut::<i32>();
    *ptr = 42;
}
```

## `LibAFL` Project

The `LibAFL` project is written and maintained by

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
