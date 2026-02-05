// Test that the Digest trait is implemented for Hash
// Run with: cargo test --test test_digest011 --features traits011

use digest011::Digest;
use hmac_sha256::Hash;

#[test]
fn test_digest_trait() {
    // Test using the Digest trait
    let mut hasher = Hash::new();
    Digest::update(&mut hasher, b"hello world");
    let result = hasher.finalize();

    // Compare with one-shot hash
    let expected = Hash::hash(b"hello world");
    assert_eq!(&result[..], &expected[..]);
}

#[test]
fn test_digest_chain() {
    // Test chaining
    let result = Hash::new()
        .chain_update(b"hello ")
        .chain_update(b"world")
        .finalize();

    let expected = Hash::hash(b"hello world");
    assert_eq!(&result[..], &expected[..]);
}

#[test]
fn test_digest_one_shot() {
    // Test one-shot digest
    let result = <Hash as Digest>::digest(b"hello world");
    let expected = Hash::hash(b"hello world");
    assert_eq!(&result[..], &expected[..]);
}

#[test]
fn test_digest_reset() {
    use digest011::Reset;

    let mut hasher = Hash::new();
    Digest::update(&mut hasher, b"hello world");
    Reset::reset(&mut hasher);
    Digest::update(&mut hasher, b"goodbye world");
    let result = hasher.finalize();

    let expected = Hash::hash(b"goodbye world");
    assert_eq!(&result[..], &expected[..]);
}

#[test]
fn test_finalize_reset() {
    let mut hasher = Hash::new();
    Digest::update(&mut hasher, b"hello world");
    let result1 = Digest::finalize_reset(&mut hasher);

    Digest::update(&mut hasher, b"goodbye world");
    let result2 = hasher.finalize();

    let expected1 = Hash::hash(b"hello world");
    let expected2 = Hash::hash(b"goodbye world");

    assert_eq!(&result1[..], &expected1[..]);
    assert_eq!(&result2[..], &expected2[..]);
}
