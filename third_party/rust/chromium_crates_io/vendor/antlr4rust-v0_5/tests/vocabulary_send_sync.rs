//! Tests for Vocabulary trait Send and Sync implementations
//!
//! This module verifies that the Vocabulary trait and its implementations
//! properly implement Send and Sync traits for thread-safe usage.

use antlr4rust::vocabulary::{Vocabulary, VocabularyImpl};
use std::sync::Arc;
use std::thread;

/// Test that Vocabulary trait object implements Send and Sync
#[test]
fn test_vocabulary_trait_send_sync() {
    fn assert_send<T: Send>() {}
    fn assert_sync<T: Sync>() {}

    // Test trait objects
    assert_send::<Box<dyn Vocabulary>>();
    assert_sync::<Box<dyn Vocabulary>>();
    assert_send::<Arc<dyn Vocabulary>>();
    assert_sync::<Arc<dyn Vocabulary>>();
}

/// Test that VocabularyImpl implements Send and Sync
#[test]
fn test_vocabulary_impl_send_sync() {
    fn assert_send<T: Send>() {}
    fn assert_sync<T: Sync>() {}

    assert_send::<VocabularyImpl>();
    assert_sync::<VocabularyImpl>();
    assert_send::<Box<VocabularyImpl>>();
    assert_sync::<Box<VocabularyImpl>>();
    assert_send::<Arc<VocabularyImpl>>();
    assert_sync::<Arc<VocabularyImpl>>();
}

/// Test cross-thread usage of Vocabulary
#[test]
fn test_vocabulary_cross_thread() {
    let vocab = Arc::new(VocabularyImpl::from_token_names(&[
        Some("INT"),
        Some("PLUS"),
        Some("EOF"),
    ]));

    let vocab_clone = vocab.clone();

    // Test moving vocabulary to another thread
    let handle = thread::spawn(move || {
        assert_eq!(vocab_clone.get_max_token_type(), 2);
        assert_eq!(vocab_clone.get_symbolic_name(0), Some("INT"));
        assert_eq!(vocab_clone.get_symbolic_name(1), Some("PLUS"));
        assert_eq!(vocab_clone.get_symbolic_name(-1), Some("EOF"));
    });

    // Test original vocabulary still works
    assert_eq!(vocab.get_max_token_type(), 2);

    handle.join().unwrap();
}

/// Test Vocabulary in lazy_static context
#[test]
fn test_vocabulary_lazy_static() {
    use std::sync::LazyLock;

    static TEST_VOCAB: LazyLock<Arc<dyn Vocabulary>> = LazyLock::new(|| {
        Arc::new(VocabularyImpl::from_token_names(&[
            Some("PROGRAM"),
            Some("VAR"),
            Some("EOF"),
        ]))
    });

    // Test access from multiple threads
    let mut handles = vec![];

    for _ in 0..3 {
        let handle = thread::spawn(|| {
            let vocab = &*TEST_VOCAB;
            assert_eq!(vocab.get_max_token_type(), 2);
            assert_eq!(vocab.get_symbolic_name(0), Some("PROGRAM"));
            assert_eq!(vocab.get_symbolic_name(1), Some("VAR"));
            assert_eq!(vocab.get_symbolic_name(-1), Some("EOF"));
        });
        handles.push(handle);
    }

    for handle in handles {
        handle.join().unwrap();
    }
}

/// Test Vocabulary with panic handling
#[test]
fn test_vocabulary_panic_safe() {
    let vocab = Arc::new(VocabularyImpl::from_token_names(&[
        Some("SAFE_TOKEN"),
        Some("EOF"),
    ]));

    let vocab_clone = vocab.clone();

    // Test that Vocabulary can be used in panic-safe context
    let result = std::panic::catch_unwind(move || {
        assert_eq!(vocab_clone.get_max_token_type(), 1);
        assert_eq!(vocab_clone.get_symbolic_name(0), Some("SAFE_TOKEN"));
        "success"
    });

    assert!(result.is_ok());
    assert_eq!(result.unwrap(), "success");
}

/// Test Vocabulary ownership transfer between threads
#[test]
fn test_vocabulary_ownership_transfer() {
    let vocab = VocabularyImpl::from_token_names(&[Some("TRANSFER_TOKEN"), Some("EOF")]);

    // Test moving vocabulary to another thread
    let handle = thread::spawn(move || {
        assert_eq!(vocab.get_max_token_type(), 1);
        assert_eq!(vocab.get_symbolic_name(0), Some("TRANSFER_TOKEN"));

        // Test creating trait object in new thread
        let boxed_vocab: Box<dyn Vocabulary> = Box::new(vocab);
        assert_eq!(boxed_vocab.get_max_token_type(), 1);
    });

    handle.join().unwrap();
}
