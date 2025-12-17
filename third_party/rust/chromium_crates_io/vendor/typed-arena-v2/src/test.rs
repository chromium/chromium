use super::*;
use std::cell::Cell;
use std::mem;
use std::panic::{self, AssertUnwindSafe};
use std::ptr;

struct DropTracker<'a>(&'a Cell<u32>);
impl<'a> Drop for DropTracker<'a> {
    fn drop(&mut self) {
        self.0.set(self.0.get() + 1);
    }
}

struct Node<'a, 'b: 'a>(Option<&'a Node<'a, 'b>>, u32, DropTracker<'b>);

#[test]
fn arena_as_intended() {
    let drop_counter = Cell::new(0);
    {
        let arena = Arena::with_capacity(2);

        let mut node: &Node = arena.alloc(Node(None, 1, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 0);

        node = arena.alloc(Node(Some(node), 2, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 0);

        node = arena.alloc(Node(Some(node), 3, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 1);

        node = arena.alloc(Node(Some(node), 4, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 1);

        assert_eq!(node.1, 4);
        assert_eq!(node.0.unwrap().1, 3);
        assert_eq!(node.0.unwrap().0.unwrap().1, 2);
        assert_eq!(node.0.unwrap().0.unwrap().0.unwrap().1, 1);
        assert!(node.0.unwrap().0.unwrap().0.unwrap().0.is_none());

        assert_eq!(arena.len(), 4);

        mem::drop(node);
        assert_eq!(drop_counter.get(), 0);

        let mut node: &Node = arena.alloc(Node(None, 5, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 1);

        node = arena.alloc(Node(Some(node), 6, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 1);

        node = arena.alloc(Node(Some(node), 7, DropTracker(&drop_counter)));
        assert_eq!(arena.chunks.borrow().rest.len(), 2);

        assert_eq!(drop_counter.get(), 0);

        assert_eq!(node.1, 7);
        assert_eq!(node.0.unwrap().1, 6);
        assert_eq!(node.0.unwrap().0.unwrap().1, 5);
        assert!(node.0.unwrap().0.unwrap().0.is_none());

        assert_eq!(drop_counter.get(), 0);
    }
    assert_eq!(drop_counter.get(), 7);
}

#[test]
fn ensure_into_vec_maintains_order_of_allocation() {
    let arena = Arena::with_capacity(1); // force multiple inner vecs
    for &s in &["t", "e", "s", "t"] {
        arena.alloc(String::from(s));
    }
    let vec = arena.into_vec();
    assert_eq!(vec, vec!["t", "e", "s", "t"]);
}

#[test]
fn test_zero_cap() {
    let arena = Arena::with_capacity(0);
    let a = arena.alloc(1);
    let b = arena.alloc(2);
    assert_eq!(*a, 1);
    assert_eq!(*b, 2);
    assert_eq!(arena.len(), 2);
}

#[test]
fn test_alloc_extend() {
    let arena = Arena::with_capacity(2);
    for i in 0..15 {
        let slice = arena.alloc_extend(0..i);
        for (j, &elem) in slice.iter().enumerate() {
            assert_eq!(j, elem);
        }
    }
}

#[test]
fn test_alloc_uninitialized() {
    const LIMIT: usize = 15;
    let drop_counter = Cell::new(0);
    unsafe {
        let arena: Arena<Node> = Arena::with_capacity(4);
        for i in 0..LIMIT {
            let slice = arena.alloc_uninitialized(i);
            for (j, elem) in slice.iter_mut().enumerate() {
                ptr::write(
                    elem.as_mut_ptr(),
                    Node(None, j as u32, DropTracker(&drop_counter)),
                );
            }
            assert_eq!(drop_counter.get(), 0);
        }
    }
    assert_eq!(drop_counter.get(), (0..LIMIT).fold(0, |a, e| a + e) as u32);
}

#[test]
fn test_alloc_extend_with_drop_counter() {
    let drop_counter = Cell::new(0);
    {
        let arena = Arena::with_capacity(2);
        let iter = (0..100).map(|j| Node(None, j as u32, DropTracker(&drop_counter)));
        let older_ref = Some(&arena.alloc_extend(iter)[0]);
        assert_eq!(drop_counter.get(), 0);
        let iter = (0..100).map(|j| Node(older_ref, j as u32, DropTracker(&drop_counter)));
        arena.alloc_extend(iter);
        assert_eq!(drop_counter.get(), 0);
    }
    assert_eq!(drop_counter.get(), 200);
}

/// Test with bools.
///
/// Bools, unlike integers, have invalid bit patterns. Therefore, ever having an uninitialized bool
/// is insta-UB. Make sure miri doesn't find any such thing.
#[test]
fn test_alloc_uninitialized_bools() {
    const LEN: usize = 20;
    unsafe {
        let arena: Arena<bool> = Arena::with_capacity(2);
        let slice = arena.alloc_uninitialized(LEN);
        for elem in slice.iter_mut() {
            ptr::write(elem.as_mut_ptr(), true);
        }
        // Now it is fully initialized, we can safely transmute the slice.
        let slice: &mut [bool] = mem::transmute(slice);
        assert_eq!(&[true; LEN], slice);
    }
}

/// Check nothing bad happens by panicking during initialization of borrowed slice.
#[test]
fn alloc_uninitialized_with_panic() {
    struct Dropper(bool);

    impl Drop for Dropper {
        fn drop(&mut self) {
            // Just make sure we touch the value, to make sure miri would bite if it was
            // unitialized
            if self.0 {
                panic!();
            }
        }
    }
    let mut reached_first_init = false;
    panic::catch_unwind(AssertUnwindSafe(|| unsafe {
        let arena: Arena<Dropper> = Arena::new();
        arena.reserve_extend(2);
        let uninitialized = arena.uninitialized_array();
        assert!((*uninitialized).len() >= 2);
        ptr::write((*uninitialized)[0].as_mut_ptr(), Dropper(false));
        reached_first_init = true;
        panic!("To drop the arena");
        // If it didn't panic, we would continue by initializing the second one and confirming by
        // .alloc_uninitialized();
    }))
    .unwrap_err();
    assert!(reached_first_init);
}

#[test]
fn test_uninitialized_array() {
    let arena = Arena::with_capacity(2);
    let uninit = arena.uninitialized_array();
    arena.alloc_extend(0..2);
    unsafe {
        for (&a, b) in (&*uninit).iter().zip(0..2) {
            assert_eq!(a.assume_init(), b);
        }
        assert!((&*arena.uninitialized_array()).as_ptr() != (&*uninit).as_ptr());
        arena.alloc(0);
        let uninit = arena.uninitialized_array();
        assert_eq!((&*uninit).len(), 3);
    }
}

#[test]
fn dont_trust_the_iterator_size() {
    use std::iter::repeat;

    struct WrongSizeIter<I>(I);
    impl<I> Iterator for WrongSizeIter<I>
    where
        I: Iterator,
    {
        type Item = I::Item;

        fn next(&mut self) -> Option<Self::Item> {
            self.0.next()
        }

        fn size_hint(&self) -> (usize, Option<usize>) {
            (0, Some(0))
        }
    }

    impl<I> ExactSizeIterator for WrongSizeIter<I> where I: Iterator {}

    let arena = Arena::with_capacity(2);
    arena.alloc(0);
    let slice = arena.alloc_extend(WrongSizeIter(repeat(1).take(1_000)));
    // Allocation of 1000 elements should have created a new chunk
    assert_eq!(arena.chunks.borrow().rest.len(), 1);
    assert_eq!(slice.len(), 1000);
}

#[test]
fn arena_is_send() {
    fn assert_is_send<T: Send>(_: T) {}

    // If `T` is `Send`, ...
    assert_is_send(42_u32);

    // Then `Arena<T>` is also `Send`.
    let arena: Arena<u32> = Arena::new();
    assert_is_send(arena);
}

#[test]
fn iter_mut_low_capacity() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = 1_000;
    const CAP: usize = 16;

    let mut arena = Arena::with_capacity(CAP);
    for i in 1..MAX {
        arena.alloc(NonCopy(i));
    }

    assert!(
        arena.chunks.borrow().rest.len() > 1,
        "expected multiple chunks"
    );

    let mut iter = arena.iter_mut();
    for i in 1..MAX {
        assert_eq!(Some(&mut NonCopy(i)), iter.next());
    }

    assert_eq!(None, iter.next());
}

#[test]
fn iter_mut_high_capacity() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = 1_000;
    const CAP: usize = 8192;

    let mut arena = Arena::with_capacity(CAP);
    for i in 1..MAX {
        arena.alloc(NonCopy(i));
    }

    assert!(
        arena.chunks.borrow().rest.is_empty(),
        "expected single chunk"
    );

    let mut iter = arena.iter_mut();
    for i in 1..MAX {
        assert_eq!(Some(&mut NonCopy(i)), iter.next());
    }

    assert_eq!(None, iter.next());
}

fn assert_size_hint<T>(arena_len: usize, iter: IterMut<'_, T>) {
    let (min, max) = iter.size_hint();

    assert!(max.is_some());
    let max = max.unwrap();

    // Check that the actual arena length lies between the estimated min and max
    assert!(min <= arena_len);
    assert!(max >= arena_len);

    // Check that the min and max estimates are within a factor of 3
    assert!(min >= arena_len / 3);
    assert!(max <= arena_len * 3);
}

#[test]
fn size_hint() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = 32;
    const CAP: usize = 0;

    for cap in CAP..(CAP + 16/* check some non-power-of-two capacities */) {
        let mut arena = Arena::with_capacity(cap);
        for i in 1..MAX {
            arena.alloc(NonCopy(i));
            let iter = arena.iter_mut();
            assert_size_hint(i, iter);
        }
    }
}

// Ensure that `alloc_extend` doesn't violate provenance of
// existing references. (Note: This test is pointless except
// under miri).
#[test]
fn check_extend_provenance() {
    let arena = Arena::new();
    let a = arena.alloc(0);
    arena.alloc_extend([0]);
    *a = 1;
}

#[test]
fn size_hint_low_initial_capacities() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = if cfg!(miri) { 100 } else { 25_000 };
    const CAP: usize = 0;

    for cap in CAP..(CAP + 128/* check some non-power-of-two capacities */) {
        let mut arena = Arena::with_capacity(cap);
        for i in 1..MAX {
            arena.alloc(NonCopy(i));
            let iter = arena.iter_mut();
            assert_size_hint(i, iter);
        }
    }
}

#[test]
fn size_hint_high_initial_capacities() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = if cfg!(miri) { 100 } else { 25_000 };
    const CAP: usize = 8164;

    for cap in CAP..(CAP + 128/* check some non-power-of-two capacities */) {
        let mut arena = Arena::with_capacity(cap);
        for i in 1..MAX {
            arena.alloc(NonCopy(i));
            let iter = arena.iter_mut();
            assert_size_hint(i, iter);
        }
    }
}

#[test]
fn size_hint_many_items() {
    #[derive(Debug, PartialEq, Eq)]
    struct NonCopy(usize);

    const MAX: usize = if cfg!(miri) { 500 } else { 5_000_000 };
    const CAP: usize = 16;

    let mut arena = Arena::with_capacity(CAP);
    for i in 1..MAX {
        arena.alloc(NonCopy(i));
        let iter = arena.iter_mut();
        assert_size_hint(i, iter);
    }
}
