# `MiniBSOD`: Create a dump of registers, stacktrace, and program state

<img align="right" src="https://raw.githubusercontent.com/AFLplusplus/Website/main/static/libafl_logo.svg" alt="LibAFL logo" width="250" heigh="250">

The `minibsod` crate provides a cross-platform library to generate a "mini blue screen of death" (`MinBSOD`) on program crashes. It is designed to provide developers with a quick overview of the program's state at the time of a critical failure. This is particularly useful for debugging and triaging crashes in complex applications, such as fuzzing targets.

`minibsod` is a part of the [LibAFL](https://github.com/AFLplusplus/LibAFL) project.

## Features

* **Crash Information:** Dumps the signal (on Unix) or exception code (on Windows) that caused the crash.
* **Register State:** Prints the content of all important CPU registers at the time of the crash.
* **Stack Backtrace:** Displays a stack backtrace to help identify the call sequence that led to the crash.
* **Memory Mappings:** Shows the process's memory mappings to provide context about the memory layout.
* **Cross-Platform:** Supports a wide range of operating systems and architectures.

## Usage

To use `minibsod`, you need to set up a signal handler (on Unix-like systems) or an exception handler (on Windows) that calls the `generate_minibsod` function.

Here is a conceptual example for Unix-like systems:

```rust,ignore
use std::io::{stdout, BufWriter};
use exceptional::unix_signals::{ucontext_t, Sig, Signal, SignalHandler, SignalHandlerFlags};
use libc::siginfo_t;
use minibsod::generate_minibsod;

extern "C" fn handle_crash(
    signal: Signal,
    siginfo: &mut siginfo_t,
    ucontext: &mut ucontext_t,
) {
    let mut writer = BufWriter::new(stdout());
    // The generate_minibsod function will print a detailed crash report to the writer.
    generate_minibsod(&mut writer, signal, siginfo, Some(ucontext)).unwrap();
}

fn setup_signal_handler() {
    let handler = SignalHandler::new(
        handle_crash,
        // A list of signals to handle.
        [
            Sig::Ill,
            Sig::Abrt,
            Sig::Bus,
            Sig::Segv,
            Sig::Trap,
            Sig::Sys,
        ],
        SignalHandlerFlags::empty(),
    );
    // Install the handler.
    handler.install().unwrap();
}

fn main() {
    setup_signal_handler();

    // Your application logic here.
    // If a handled signal occurs, the handle_crash function will be called.
    
    // For example, to trigger a crash:
    // unsafe {
    //     *(0xdeadbeef as *mut u32) = 0;
    // }
}
```

On Windows, you would use a similar approach with `SetUnhandledExceptionFilter` to set up a top-level exception handler.

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
