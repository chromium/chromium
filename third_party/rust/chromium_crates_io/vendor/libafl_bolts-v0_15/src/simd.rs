//! Module for SIMD assisted methods.

#[cfg(feature = "alloc")]
use alloc::{vec, vec::Vec};
use core::ops::{BitAnd, BitOr};

#[cfg(feature = "wide")]
use wide::CmpEq;

/// Re-export our vector types
#[cfg(feature = "wide")]
pub mod vector {
    pub use wide::{u8x16, u8x32};
}

/// The SIMD based reducer implementation
#[cfg(feature = "wide")]
pub trait SimdReducer<T>: Reducer<T> {
    /// The associated primitive reducer
    type PrimitiveReducer: Reducer<u8>;
}

/// A `Reducer` function is used to aggregate values for the novelty search
pub trait Reducer<T> {
    /// Reduce two values to one value, with the current [`Reducer`].
    fn reduce(first: T, second: T) -> T;
}

#[cfg(feature = "wide")]
trait HasMax: Sized {
    fn max_(self, rhs: Self) -> Self;
}

#[cfg(feature = "wide")]
impl HasMax for wide::u8x16 {
    fn max_(self, rhs: Self) -> Self {
        self.max(rhs)
    }
}

#[cfg(feature = "wide")]
impl HasMax for wide::u8x32 {
    fn max_(self, rhs: Self) -> Self {
        self.max(rhs)
    }
}

#[cfg(feature = "wide")]
trait HasMin: Sized {
    fn min_(self, rhs: Self) -> Self;
}

#[cfg(feature = "wide")]
impl HasMin for wide::u8x16 {
    fn min_(self, rhs: Self) -> Self {
        self.min(rhs)
    }
}

#[cfg(feature = "wide")]
impl HasMin for wide::u8x32 {
    fn min_(self, rhs: Self) -> Self {
        self.min(rhs)
    }
}

/// A [`MaxReducer`] reduces int values and returns their maximum.
#[derive(Debug, Clone)]
pub struct MaxReducer {}

impl<T> Reducer<T> for MaxReducer
where
    T: PartialOrd,
{
    #[inline]
    fn reduce(first: T, second: T) -> T {
        if first > second { first } else { second }
    }
}

/// Unforunately we have to keep this type due to [`wide`] might not `PartialOrd`
#[cfg(feature = "wide")]
#[derive(Debug)]
pub struct SimdMaxReducer;

#[cfg(feature = "wide")]
impl<T> Reducer<T> for SimdMaxReducer
where
    T: HasMax,
{
    fn reduce(first: T, second: T) -> T {
        first.max_(second)
    }
}

#[cfg(feature = "wide")]
impl<T> SimdReducer<T> for SimdMaxReducer
where
    T: HasMax,
{
    type PrimitiveReducer = MaxReducer;
}

/// A [`NopReducer`] does nothing, and just "reduces" to the second/`new` value.
#[derive(Debug, Copy, Clone)]
pub struct NopReducer {}

impl<T> Reducer<T> for NopReducer {
    #[inline]
    fn reduce(_history: T, new: T) -> T {
        new
    }
}

#[cfg(feature = "wide")]
impl<T> SimdReducer<T> for NopReducer {
    type PrimitiveReducer = NopReducer;
}

/// A [`MinReducer`] reduces int values and returns their minimum.
#[derive(Debug, Clone)]
pub struct MinReducer {}

impl<T> Reducer<T> for MinReducer
where
    T: PartialOrd,
{
    #[inline]
    fn reduce(first: T, second: T) -> T {
        if first < second { first } else { second }
    }
}

/// Unforunately we have to keep this type due to [`wide`] might not `PartialOrd`
#[cfg(feature = "wide")]
#[derive(Debug)]
pub struct SimdMinReducer;

#[cfg(feature = "wide")]
impl<T> Reducer<T> for SimdMinReducer
where
    T: HasMin,
{
    fn reduce(first: T, second: T) -> T {
        first.min_(second)
    }
}

#[cfg(feature = "wide")]
impl<T> SimdReducer<T> for SimdMinReducer
where
    T: HasMin,
{
    type PrimitiveReducer = MinReducer;
}

/// A [`OrReducer`] reduces the values returning the bitwise OR with the old value
#[derive(Debug, Clone)]
pub struct OrReducer {}

impl<T> Reducer<T> for OrReducer
where
    T: BitOr<Output = T>,
{
    #[inline]
    fn reduce(history: T, new: T) -> T {
        history | new
    }
}

#[cfg(feature = "wide")]
impl<T> SimdReducer<T> for OrReducer
where
    T: BitOr<Output = T>,
{
    type PrimitiveReducer = OrReducer;
}

/// SIMD based [`OrReducer`], alias for consistency
#[cfg(feature = "wide")]
pub type SimdOrReducer = OrReducer;

/// A [`AndReducer`] reduces the values returning the bitwise AND with the old value
#[derive(Debug, Clone)]
pub struct AndReducer {}

impl<T> Reducer<T> for AndReducer
where
    T: BitAnd<Output = T>,
{
    #[inline]
    fn reduce(history: T, new: T) -> T {
        history & new
    }
}

#[cfg(feature = "wide")]
impl<T> SimdReducer<T> for AndReducer
where
    T: BitAnd<Output = T>,
{
    type PrimitiveReducer = AndReducer;
}

/// SIMD based [`AndReducer`], alias for consistency
#[cfg(feature = "wide")]
pub type SimdAndReducer = AndReducer;

#[cfg(feature = "wide")]
/// The vector type that can be used with coverage map
pub trait VectorType {
    /// Number of bytes
    const N: usize;
    /// Zero vector
    const ZERO: Self;
    /// One vector
    const ONE: Self;
    /// 0x80 vector
    const EIGHTY: Self;

    /// Construct vector from slice. Can't use N unless const generics is stablized.
    fn from_slice(arr: &[u8]) -> Self;

    /// Collect novelties. We pass in base to avoid redo calculate for novelties indice.
    fn novelties(hist: &[u8], map: &[u8], base: usize, novelties: &mut Vec<usize>);

    /// Do blending
    #[must_use]
    fn blend(self, lhs: Self, rhs: Self) -> Self;

    /// Can't reuse [`crate::AsSlice`] due to [`wide`] might implement `Deref`
    fn as_slice(&self) -> &[u8];
}

#[cfg(feature = "wide")]
impl VectorType for wide::u8x16 {
    const N: usize = Self::LANES as usize;
    const ZERO: Self = Self::ZERO;
    const ONE: Self = Self::new([0x1u8; Self::N]);
    const EIGHTY: Self = Self::new([0x80u8; Self::N]);

    fn from_slice(arr: &[u8]) -> Self {
        Self::new(arr[0..Self::N].try_into().unwrap())
    }

    fn novelties(hist: &[u8], map: &[u8], base: usize, novelties: &mut Vec<usize>) {
        unsafe {
            for j in base..(base + Self::N) {
                let item = *map.get_unchecked(j);
                if item > *hist.get_unchecked(j) {
                    novelties.push(j);
                }
            }
        }
    }

    fn blend(self, lhs: Self, rhs: Self) -> Self {
        self.blend(lhs, rhs)
    }

    fn as_slice(&self) -> &[u8] {
        self.as_array_ref()
    }
}

#[cfg(feature = "wide")]
impl VectorType for wide::u8x32 {
    const N: usize = Self::LANES as usize;
    const ZERO: Self = Self::ZERO;
    const ONE: Self = Self::new([0x1u8; Self::N]);
    const EIGHTY: Self = Self::new([0x80u8; Self::N]);

    fn from_slice(arr: &[u8]) -> Self {
        Self::new(arr[0..Self::N].try_into().unwrap())
    }

    fn novelties(hist: &[u8], map: &[u8], base: usize, novelties: &mut Vec<usize>) {
        unsafe {
            // Break into two loops so that LLVM will vectorize both loops.
            // Or LLVM won't vectorize them and is super slow. We need a few
            // extra intrinsic to wide and safe_arch to vectorize this manually.
            for j in base..(base + Self::N / 2) {
                let item = *map.get_unchecked(j);
                if item > *hist.get_unchecked(j) {
                    novelties.push(j);
                }
            }

            for j in (base + Self::N / 2)..(base + Self::N) {
                let item = *map.get_unchecked(j);
                if item > *hist.get_unchecked(j) {
                    novelties.push(j);
                }
            }
        }
    }

    fn blend(self, lhs: Self, rhs: Self) -> Self {
        self.blend(lhs, rhs)
    }

    fn as_slice(&self) -> &[u8] {
        self.as_array_ref()
    }
}

/// `simplify_map` naive implementaion. In most cases, this can be auto-vectorized.
pub fn simplify_map_naive(map: &mut [u8]) {
    for it in map.iter_mut() {
        *it = if *it == 0 { 0x1 } else { 0x80 };
    }
}

/// `simplify_map` implementation by u8x16, worse performance compared to LLVM
/// auto-vectorization but faster if LLVM doesn't vectorize.
#[cfg(feature = "wide")]
pub fn simplify_map_simd<V>(map: &mut [u8])
where
    V: VectorType + Copy + Eq + CmpEq<Output = V>,
{
    let size = map.len();
    let steps = size / V::N;
    let left = size % V::N;
    let lhs = V::ONE;
    let rhs = V::EIGHTY;

    for step in 0..steps {
        let i = step * V::N;
        let mp = V::from_slice(&map[i..]);

        let mask = mp.cmp_eq(V::ZERO);
        let out = mask.blend(lhs, rhs);
        map[i..i + V::N].copy_from_slice(out.as_slice());
    }

    #[allow(clippy::needless_range_loop)]
    for j in (size - left)..size {
        map[j] = if map[j] == 0 { 0x1 } else { 0x80 }
    }
}

/// The std implementation of `simplify_map`. Use the fastest implementation by benchamrk by default.
pub fn std_simplify_map(map: &mut [u8]) {
    #[cfg(not(feature = "wide"))]
    simplify_map_naive(map);

    #[cfg(feature = "wide")]
    simplify_map_simd::<wide::u8x32>(map);
}

/// Coverage map insteresting implementation by u8x16. Slightly faster than nightly simd.
///
/// # Safety
///
/// The caller must ensure that `hist.len() >= map.len()` so all reads from `hist`
/// performed by this function remain in-bounds.
#[cfg(all(feature = "alloc", feature = "wide"))]
#[must_use]
pub unsafe fn covmap_is_interesting_simd<R, V>(
    hist: &[u8],
    map: &[u8],
    collect_novelties: bool,
) -> (bool, Vec<usize>)
where
    V: VectorType + Eq + Copy,
    R: SimdReducer<V>,
{
    debug_assert!(hist.len() >= map.len());

    let mut novelties = vec![];
    let mut interesting = false;
    let size = map.len();
    let steps = size / V::N;
    let left = size % V::N;

    if collect_novelties {
        for step in 0..steps {
            let i = step * V::N;
            let history = V::from_slice(&hist[i..]);
            let items = V::from_slice(&map[i..]);

            let out = R::reduce(history, items);
            if out != history {
                interesting = true;
                V::novelties(hist, map, i, &mut novelties);
            }
        }

        for j in (size - left)..size {
            unsafe {
                let item = *map.get_unchecked(j);
                let history = *hist.get_unchecked(j);
                let out = R::PrimitiveReducer::reduce(item, history);
                if out != history {
                    interesting = true;
                    novelties.push(j);
                }
            }
        }
    } else {
        for step in 0..steps {
            let i = step * V::N;
            let history = V::from_slice(&hist[i..]);
            let items = V::from_slice(&map[i..]);

            let out = R::reduce(history, items);
            if out != history {
                interesting = true;
                break;
            }
        }

        if !interesting {
            for j in (size - left)..size {
                unsafe {
                    let item = *map.get_unchecked(j);
                    let history = *hist.get_unchecked(j);
                    let out = R::PrimitiveReducer::reduce(item, history);
                    if out != history {
                        interesting = true;
                        break;
                    }
                }
            }
        }
    }

    (interesting, novelties)
}

/// Coverage map insteresting naive implementation. Do not use it unless you have strong reasons to do.
///
/// # Safety
///
/// The caller must ensure that `hist.len() >= map.len()` so all reads from `hist`
/// performed by this function remain in-bounds.
#[cfg(feature = "alloc")]
#[must_use]
pub unsafe fn covmap_is_interesting_naive<R>(
    hist: &[u8],
    map: &[u8],
    collect_novelties: bool,
) -> (bool, Vec<usize>)
where
    R: Reducer<u8>,
{
    debug_assert!(hist.len() >= map.len());

    let mut novelties = vec![];
    let mut interesting = false;
    let initial = 0;
    if collect_novelties {
        for (i, item) in map.iter().enumerate().filter(|(_, item)| **item != initial) {
            let existing = unsafe { *hist.get_unchecked(i) };
            let reduced = R::reduce(existing, *item);
            if existing != reduced {
                interesting = true;
                novelties.push(i);
            }
        }
    } else {
        for (i, item) in map.iter().enumerate().filter(|(_, item)| **item != initial) {
            let existing = unsafe { *hist.get_unchecked(i) };
            let reduced = R::reduce(existing, *item);
            if existing != reduced {
                interesting = true;
                break;
            }
        }
    }

    (interesting, novelties)
}
