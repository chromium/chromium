# skera

`skera` is a Rust library and binary for subsetting a font file according to provided input.

## Installation

### Library

To use `skera` in your Rust project, add it via `cargo`:

```bash
cargo add skera
```

### CLI

To install the `skera` command-line tool, use `cargo install` with the `cli` feature enabled:

```bash
cargo install skera --features cli
```

## Usage

### CLI

To subset a font using the command-line tool:

```bash
skera --path <INPUT_PATH> --unicodes <UNICODES> --output-file <OUTPUT_PATH>
```

For a full list of available options and flags, run:

```bash
skera --help
```
