# rust-hmac-sha256

A small, self-contained SHA256, HMAC-SHA256, and HKDF-SHA256 implementation in Rust with no_std support.

## Features

* Pure Rust implementation
* No external dependencies (unless optional features are enabled)
* `no_std` compatible
* Both one-shot and streaming APIs
* Constant-time verification to prevent timing attacks
* HKDF key derivation (extraction and expansion)

## Optional Features

* `traits`: Enable support for the `Digest` trait from the `digest` crate (both version 0.9.0 and 0.10.7)
* `opt_size`: Enable size optimizations. Based on benchmarks, the `.text` section size is reduced by 75%, at the cost of approximately 16% performance.

## Usage Examples

### SHA-256

```rust
// One-shot hashing
let hash = hmac_sha256::Hash::hash(b"hello world");

// Incremental hashing
let mut hasher = hmac_sha256::Hash::new();
hasher.update(b"hello ");
hasher.update(b"world");
let hash = hasher.finalize();

// Constant-time verification
let expected = hmac_sha256::Hash::hash(b"hello world");
let mut hasher = hmac_sha256::Hash::new();
hasher.update(b"hello world");
assert!(hasher.verify(&expected));
```

### HMAC-SHA256

```rust
// One-shot HMAC
let mac = hmac_sha256::HMAC::mac(b"message", b"key");

// Incremental HMAC
let mut hmac = hmac_sha256::HMAC::new(b"key");
hmac.update(b"message part 1");
hmac.update(b"message part 2");
let mac = hmac.finalize();

// Constant-time verification
let expected = hmac_sha256::HMAC::mac(b"message", b"key");
let mut hmac = hmac_sha256::HMAC::new(b"key");
hmac.update(b"message");
assert!(hmac.verify(&expected));
```

### HKDF-SHA256

```rust
// Extract a pseudorandom key from input keying material
let prk = hmac_sha256::HKDF::extract(b"salt", b"input key material");

// Expand the pseudorandom key to the desired output length
let mut output = [0u8; 64];
hmac_sha256::HKDF::expand(&mut output, prk, b"application info");
```
