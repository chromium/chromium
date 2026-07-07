use alloc::vec::Vec;

// A [`BigDigit`] is a [`BigUint`]'s composing element.
cfg_digit!(
    pub(crate) type BigDigit = u32;
    pub(crate) type BigDigit = u64;
);

// A [`DoubleBigDigit`] is the internal type used to do the computations.  Its
// size is the double of the size of [`BigDigit`].
cfg_digit!(
    pub(crate) type DoubleBigDigit = u64;
    pub(crate) type DoubleBigDigit = u128;
);

pub(crate) const BITS: u8 = BigDigit::BITS as u8;
pub(crate) const HALF_BITS: u8 = BITS / 2;
pub(crate) const HALF: BigDigit = (1 << HALF_BITS) - 1;

pub(crate) const MAX: BigDigit = BigDigit::MAX;
const LO_MASK: DoubleBigDigit = MAX as DoubleBigDigit;

#[inline]
fn get_hi(n: DoubleBigDigit) -> BigDigit {
    (n >> BITS) as BigDigit
}
#[inline]
fn get_lo(n: DoubleBigDigit) -> BigDigit {
    (n & LO_MASK) as BigDigit
}

/// Split one [`DoubleBigDigit`] into two [`BigDigit`]s.
#[inline]
pub(crate) fn from_doublebigdigit(n: DoubleBigDigit) -> (BigDigit, BigDigit) {
    (get_hi(n), get_lo(n))
}

/// Join two [`BigDigit`]s into one [`DoubleBigDigit`].
#[inline]
pub(crate) fn to_doublebigdigit(hi: BigDigit, lo: BigDigit) -> DoubleBigDigit {
    DoubleBigDigit::from(lo) | (DoubleBigDigit::from(hi) << BITS)
}

pub(crate) enum BigDigits {
    Inline(Option<BigDigit>),
    Heap(Vec<BigDigit>),
}

impl BigDigits {
    pub(crate) const ZERO: Self = BigDigits::Inline(None);
    pub(crate) const ONE: Self = BigDigits::Inline(Some(1));

    #[inline]
    pub(crate) const fn from_digit(x: BigDigit) -> Self {
        if x == 0 {
            BigDigits::ZERO
        } else {
            BigDigits::Inline(Some(x))
        }
    }

    #[inline]
    pub(crate) fn from_slice(slice: &[BigDigit]) -> Self {
        match slice {
            &[] => BigDigits::ZERO,
            &[x] => BigDigits::Inline(Some(x)),
            xs => BigDigits::Heap(xs.to_vec()),
        }
    }

    #[inline]
    pub(crate) fn from_vec(xs: Vec<BigDigit>) -> Self {
        BigDigits::Heap(xs)
    }

    #[inline]
    pub(crate) fn clear(&mut self) {
        match self {
            BigDigits::Inline(x) => *x = None,
            BigDigits::Heap(xs) => xs.clear(),
        }
    }

    #[inline]
    pub(crate) fn push(&mut self, y: BigDigit) {
        match &mut *self {
            BigDigits::Inline(x @ None) => *x = Some(y),
            BigDigits::Inline(Some(x)) => *self = BigDigits::Heap([*x, y].to_vec()),
            BigDigits::Heap(xs) => xs.push(y),
        }
    }

    #[inline]
    pub(crate) fn pop(&mut self) -> Option<BigDigit> {
        match self {
            BigDigits::Inline(x) => x.take(),
            BigDigits::Heap(xs) => xs.pop(),
        }
    }

    #[inline]
    pub(crate) fn last(&self) -> Option<&BigDigit> {
        match self {
            BigDigits::Inline(x) => x.as_ref(),
            BigDigits::Heap(xs) => xs.last(),
        }
    }

    #[inline]
    pub(crate) fn len(&self) -> usize {
        match self {
            BigDigits::Inline(None) => 0,
            BigDigits::Inline(Some(_)) => 1,
            BigDigits::Heap(xs) => xs.len(),
        }
    }

    #[inline]
    pub(crate) fn is_empty(&self) -> bool {
        match self {
            BigDigits::Inline(None) => true,
            BigDigits::Inline(Some(_)) => false,
            BigDigits::Heap(xs) => xs.is_empty(),
        }
    }

    #[inline]
    pub(crate) fn capacity(&self) -> usize {
        match self {
            BigDigits::Inline(_) => 1,
            BigDigits::Heap(xs) => xs.capacity(),
        }
    }

    #[inline]
    pub(crate) fn shrink(&mut self) {
        if let BigDigits::Heap(xs) = self {
            if xs.len() < xs.capacity() / 2 {
                match **xs {
                    [] => *self = BigDigits::ZERO,
                    [x] => *self = BigDigits::Inline(Some(x)),
                    _ => xs.shrink_to(xs.len() + 1),
                }
            }
        }
    }

    /// Returns `true` if the most-significant digit (if any) is nonzero.
    #[inline]
    pub(crate) fn is_normal(&self) -> bool {
        match self {
            BigDigits::Inline(Some(0)) => false,
            BigDigits::Inline(_) => true,
            BigDigits::Heap(xs) => !matches!(**xs, [.., 0]),
        }
    }

    /// Strips off trailing zero bigdigits - most algorithms require
    /// the most significant digit in the number to be nonzero.
    #[inline]
    pub(crate) fn normalize(&mut self) {
        match self {
            BigDigits::Inline(x) => {
                if let Some(0) = *x {
                    *x = None;
                }
            }
            BigDigits::Heap(xs) => {
                if let [.., 0] = **xs {
                    let len = xs.iter().rposition(|&d| d != 0).map_or(0, |i| i + 1);
                    xs.truncate(len);
                }
                if xs.len() < xs.capacity() / 2 {
                    match **xs {
                        [] => *self = BigDigits::ZERO,
                        [x] => *self = BigDigits::Inline(Some(x)),
                        _ => xs.shrink_to(xs.len() + 1),
                    }
                }
            }
        }
    }

    #[inline]
    pub(crate) fn truncate(&mut self, len: usize) {
        match self {
            BigDigits::Inline(x) => {
                if len == 0 {
                    *x = None;
                }
            }
            BigDigits::Heap(xs) => xs.truncate(len),
        }
    }

    #[inline]
    pub(crate) fn drain_front(&mut self, len: usize) {
        match self {
            BigDigits::Inline(x) => {
                assert!(len <= 1);
                if len == 1 {
                    *x = None;
                }
            }
            BigDigits::Heap(xs) => {
                xs.drain(..len);
            }
        }
    }

    pub(crate) fn reserve(&mut self, additional: usize) {
        match &mut *self {
            BigDigits::Inline(opt_x) => {
                let capacity = usize::from(opt_x.is_some()) + additional;
                if capacity > 1 {
                    let mut vec = Vec::with_capacity(capacity);
                    if let Some(x) = *opt_x {
                        vec.push(x);
                    }
                    *self = BigDigits::Heap(vec);
                }
            }
            BigDigits::Heap(xs) => xs.reserve(additional),
        }
    }

    pub(crate) fn resize(&mut self, len: usize, value: BigDigit) {
        match &mut *self {
            BigDigits::Inline(x) => match len {
                0 => *x = None,
                1 => {
                    if x.is_none() {
                        *x = Some(value);
                    }
                }
                _ => {
                    let mut xs = Vec::with_capacity(len);
                    if let Some(x) = *x {
                        xs.push(x);
                    }
                    xs.resize(len, value);
                    *self = BigDigits::Heap(xs);
                }
            },
            BigDigits::Heap(xs) => xs.resize(len, value),
        }
    }

    pub(crate) fn extend_from_slice(&mut self, ys: &[BigDigit]) {
        match &mut *self {
            BigDigits::Inline(None) => *self = BigDigits::from_slice(ys),
            BigDigits::Inline(Some(x)) => {
                let len = ys.len() + 1;
                if len > 1 {
                    let mut xs = Vec::with_capacity(len);
                    xs.push(*x);
                    xs.extend_from_slice(ys);
                    *self = BigDigits::Heap(xs);
                }
            }
            BigDigits::Heap(xs) => xs.extend_from_slice(ys),
        }
    }

    pub(crate) fn extend<I>(&mut self, mut iter: I)
    where
        I: ExactSizeIterator<Item = BigDigit>,
    {
        match &mut *self {
            BigDigits::Inline(x) => {
                if x.is_none() {
                    match iter.next() {
                        Some(y) => *x = Some(y),
                        None => return,
                    }
                }
                if let Some(y) = iter.next() {
                    let len = iter.len().saturating_add(2);
                    let mut xs = Vec::with_capacity(len);
                    xs.push(x.unwrap());
                    xs.push(y);
                    xs.extend(iter);
                    *self = BigDigits::Heap(xs);
                }
            }
            BigDigits::Heap(xs) => xs.extend(iter),
        }
    }
}

impl Clone for BigDigits {
    #[inline]
    fn clone(&self) -> Self {
        match self {
            BigDigits::Inline(x) => BigDigits::Inline(*x),
            BigDigits::Heap(xs) => BigDigits::from_slice(xs),
        }
    }

    #[inline]
    fn clone_from(&mut self, source: &Self) {
        match &mut *self {
            // Reuse the existing heap allocation if we have one.
            BigDigits::Heap(xs) if xs.capacity() != 0 => {
                xs.clear();
                xs.extend_from_slice(source);
            }
            #[allow(clippy::assigning_clones)]
            _ => *self = source.clone(),
        }
    }
}

impl core::ops::Deref for BigDigits {
    type Target = [BigDigit];

    #[inline]
    fn deref(&self) -> &Self::Target {
        match self {
            BigDigits::Inline(None) => &[],
            BigDigits::Inline(Some(x)) => core::slice::from_ref(x),
            BigDigits::Heap(xs) => xs,
        }
    }
}

impl core::ops::DerefMut for BigDigits {
    #[inline]
    fn deref_mut(&mut self) -> &mut Self::Target {
        match self {
            BigDigits::Inline(None) => &mut [],
            BigDigits::Inline(Some(x)) => core::slice::from_mut(x),
            BigDigits::Heap(xs) => xs,
        }
    }
}
