use itertools::{EitherOrBoth, Itertools};
use std::fmt::Debug;
use std::ops::BitXor;
use quickcheck::quickcheck;

struct Unspecialized<I>(I);
impl<I> Iterator for Unspecialized<I>
where
    I: Iterator,
{
    type Item = I::Item;

    #[inline(always)]
    fn next(&mut self) -> Option<I::Item> {
        self.0.next()
    }

    #[inline(always)]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.0.size_hint()
    }
}

fn check_specialized<'a, V, IterItem, Iter, F>(iterator: &Iter, mapper: F)
where
    V: Eq + Debug,
    IterItem: 'a,
    Iter: Iterator<Item = IterItem> + Clone + 'a,
    F: Fn(Box<dyn Iterator<Item = IterItem> + 'a>) -> V,
{
    assert_eq!(
        mapper(Box::new(Unspecialized(iterator.clone()))),
        mapper(Box::new(iterator.clone()))
    )
}

fn check_specialized_count_last_nth_sizeh<'a, IterItem, Iter>(
    it: &Iter,
    known_expected_size: Option<usize>,
) where
    IterItem: 'a + Eq + Debug,
    Iter: Iterator<Item = IterItem> + Clone + 'a,
{
    let size = it.clone().count();
    if let Some(expected_size) = known_expected_size {
        assert_eq!(size, expected_size);
    }
    check_specialized(it, |i| i.count());
    check_specialized(it, |i| i.last());
    for n in 0..size + 2 {
        check_specialized(it, |mut i| i.nth(n));
    }
    let mut it_sh = it.clone();
    for n in 0..size + 2 {
        let len = it_sh.clone().count();
        let (min, max) = it_sh.size_hint();
        assert_eq!((size - n.min(size)), len);
        assert!(min <= len);
        if let Some(max) = max {
            assert!(len <= max);
        }
        it_sh.next();
    }
}

fn check_specialized_fold_xor<'a, IterItem, Iter>(it: &Iter)
where
    IterItem: 'a
        + BitXor
        + Eq
        + Debug
        + BitXor<<IterItem as BitXor>::Output, Output = <IterItem as BitXor>::Output>
        + Clone,
    <IterItem as BitXor>::Output:
        BitXor<Output = <IterItem as BitXor>::Output> + Eq + Debug + Clone,
    Iter: Iterator<Item = IterItem> + Clone + 'a,
{
    check_specialized(it, |mut i| {
        let first = i.next().map(|f| f.clone() ^ (f.clone() ^ f));
        i.fold(first, |acc, v: IterItem| acc.map(move |a| v ^ a))
    });
}

fn put_back_test(test_vec: Vec<i32>, known_expected_size: Option<usize>) {
    {
        // Lexical lifetimes support
        let pb = itertools::put_back(test_vec.iter());
        check_specialized_count_last_nth_sizeh(&pb, known_expected_size);
        check_specialized_fold_xor(&pb);
    }

    let mut pb = itertools::put_back(test_vec.into_iter());
    pb.put_back(1);
    check_specialized_count_last_nth_sizeh(&pb, known_expected_size.map(|x| x + 1));
    check_specialized_fold_xor(&pb)
}

#[test]
fn put_back() {
    put_back_test(vec![7, 4, 1], Some(3));
}

quickcheck! {
    fn put_back_qc(test_vec: Vec<i32>) -> () {
        put_back_test(test_vec, None)
    }
}

fn merge_join_by_test(i1: Vec<usize>, i2: Vec<usize>, known_expected_size: Option<usize>) {
    let i1 = i1.into_iter();
    let i2 = i2.into_iter();
    let mjb = i1.clone().merge_join_by(i2.clone(), std::cmp::Ord::cmp);
    check_specialized_count_last_nth_sizeh(&mjb, known_expected_size);
    // Rust 1.24 compatibility:
    fn eob_left_z(eob: EitherOrBoth<usize, usize>) -> usize {
        eob.left().unwrap_or(0)
    }
    fn eob_right_z(eob: EitherOrBoth<usize, usize>) -> usize {
        eob.left().unwrap_or(0)
    }
    fn eob_both_z(eob: EitherOrBoth<usize, usize>) -> usize {
        let (a, b) = eob.both().unwrap_or((0, 0));
        assert_eq!(a, b);
        a
    }
    check_specialized_fold_xor(&mjb.clone().map(eob_left_z));
    check_specialized_fold_xor(&mjb.clone().map(eob_right_z));
    check_specialized_fold_xor(&mjb.clone().map(eob_both_z));

    // And the other way around
    let mjb = i2.merge_join_by(i1, std::cmp::Ord::cmp);
    check_specialized_count_last_nth_sizeh(&mjb, known_expected_size);
    check_specialized_fold_xor(&mjb.clone().map(eob_left_z));
    check_specialized_fold_xor(&mjb.clone().map(eob_right_z));
    check_specialized_fold_xor(&mjb.clone().map(eob_both_z));
}

#[test]
fn merge_join_by() {
    let i1 = vec![1, 3, 5, 7, 8, 9];
    let i2 = vec![0, 3, 4, 5];
    merge_join_by_test(i1, i2, Some(8));
}

quickcheck! {
    fn merge_join_by_qc(i1: Vec<usize>, i2: Vec<usize>) -> () {
        merge_join_by_test(i1, i2, None)
    }
}
