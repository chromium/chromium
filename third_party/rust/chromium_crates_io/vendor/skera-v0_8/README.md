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

## Profiling

How-To: Profile a Skera Binary using Samply

### Prerequisites
Install samply using Cargo:

```bash
cargo install samply
```

### Step 1: Enable Debug Symbols in Release Profiles
Add the following configuration to your root Cargo.toml:

```bash
[profile.release]
debug = true
```

### Step 2: Compile your crate using the release profile with the cli feature:

```bash
cargo build --release -p skera --features="cli"
```

### Step 3: Record and Profile the Binary
Run the generated release executable using samply record. Replace <bin> with your compiled binary's name and <args...> with any execution arguments:

```bash
samply record target/release/skera <args...>
```

example:

```bash
samply record target/release/skera font-file --unicodes=* --output-file=out
```

### Step 4: Analyze the Results
Once your binary finishes execution, samply automatically starts a local web server and prints a URL to the console:
```bash
Server listening on http://127.0.0.1:3000
Press Ctrl+C to stop.
```

Open the provided link in your web browser.
The browser will open the Firefox Profiler interface, populated with your recorded run's call tree, flame graph, and timeline views.

### Alternative Profiling Tools
If samply is not suitable for your target environment, you may consider these alternative options:

cargo-flamegraph: A tool that utilizes perf (on Linux) or dtrace (on macOS) to generate a static vector-graphic .svg flame graph.

```bash
cargo install cargo-flamegraph
cargo flamegraph -p skera --features="cli"
```

perf (Linux-only): The standard system profiler on Linux, useful for command-line profiling and capturing kernel-level events.

```bash
perf record -g -- target/release/skera
perf report
```

