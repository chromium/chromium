# Fuzzing `bindgen` with `csmith`

[`csmith`][csmith] generates random C and C++ programs that can be used as test
cases for compilers. When testing `bindgen` with `csmith`, we interpret the
generated programs as header files, and emit Rust bindings to them. If `bindgen`
panics, the emitted bindings won't compile with `rustc`, or the generated layout
tests in the bindings fail, then we report an issue containing the test case!

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Prerequisites](#prerequisites)
- [Running the Fuzzer](#running-the-fuzzer)
- [Reporting Issues](#reporting-issues)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## Prerequisites

Requires `python3`, `csmith`, and `creduce` to be in `$PATH`.

Many OS package managers have `csmith` and `creduce` packages:

```
$ sudo apt install csmith creduce
$ brew install csmith creduce
$ # Etc...
```

## Running the Fuzzer

Run `csmith` and test `bindgen` on the generated test cases with this command:

```
$ ./driver.py
```

The driver will keep running until it encounters an error in `bindgen`.

Each invocation of `./driver.py` will use its own temporary directories, so
running it in multiple terminals in parallel is supported.

`csmith` is run with `--no-checksum --nomain --max-block-size 1
--max-block-depth 1` which disables the `main` function, and makes function
bodies as simple as possible as `bindgen` does not care about them, but they
cannot be completely disabled in `csmith`. Run `csmith --help` to see what
exactly those options do.

## Reporting Issues

Once the fuzz driver finds a test case that causes some kind of error in
`bindgen` or its emitted bindings, it is helpful to
[run C-Reduce on the test case][creducing] to remove the parts that are
irrelevant to reproducing the error. This is ***very*** helpful for the folks
who further investigate the issue and come up with a fix!

Additionally, mention that you discovered the issue via `csmith` and we will add
the `A-csmith` label. You can find all the issues discovered with `csmith`, and
related to fuzzing with `csmith`, by looking up
[all issues tagged with the `A-csmith` label][csmith-issues].

[csmith]: https://github.com/csmith-project/csmith
[creducing]: ../CONTRIBUTING.md#using-creduce-to-minimize-test-cases
[csmith-issues]: https://github.com/rust-lang/rust-bindgen/issues?q=label%3AA-csmith
