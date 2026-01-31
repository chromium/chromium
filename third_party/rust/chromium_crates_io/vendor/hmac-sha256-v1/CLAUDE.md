# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This repository contains a small, self-contained implementation of SHA256, HMAC-SHA256, and HKDF-SHA256 in Rust with no_std support. The codebase is deliberately minimal and optimized for both size and performance.

## Build Commands

```bash
# Build the library
cargo build

# Build with release optimizations
cargo build --release

# Run tests
cargo test

# Run benchmarks (if applicable)
cargo bench

# Build with specific features
cargo build --features traits      # Enable Digest trait support
cargo build --features opt_size    # Enable size optimizations
cargo build --features traits,opt_size # Enable both features
```

## Features

The library supports the following optional features:

- `traits`: Enables support for the `Digest` trait from the `digest` crate (both version 0.9.0 and 0.10.7)
- `opt_size`: Enables size optimizations, reducing the `.text` section size by approximately 75% at the cost of about 16% performance

## Project Structure

The codebase is minimal:

- `src/lib.rs`: Contains the entire implementation of SHA256, HMAC-SHA256, and HKDF-SHA256
- `Cargo.toml`: Defines the project dependencies and configuration

## Code Architecture

The library is designed to be minimal and self-contained with no external dependencies unless the `traits` feature is enabled. It implements:

1. `Hash`: A SHA-256 hasher that can process input incrementally or all at once
2. `HMAC`: HMAC-SHA256 implementation with both one-shot and streaming APIs
3. `HKDF`: HKDF-SHA256 implementation with extract and expand operations

When the `traits` feature is enabled, the library implements the `Digest` trait from the `digest` crate, with compatibility for both version 0.9.0 and 0.10.7.

## Pull Request Guidelines

When submitting changes to this codebase:

1. Ensure all tests pass with `cargo test`
2. If adding new functionality, include appropriate tests
3. Maintain no_std compatibility
4. Be mindful of performance and code size impacts, especially if changes affect code paths with the `opt_size` feature