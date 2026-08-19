//! Misc utility functions

/// Iterator that iterates over a vector of iterators simultaneously.
///
/// Adapted from <https://stackoverflow.com/a/55292215>
pub struct MultiZip<I: Iterator>(Vec<I>);

impl<I: Iterator> MultiZip<I> {
    /// Create a new MultiZip from a vector of iterators
    pub fn new(vec_of_iters: Vec<I>) -> Self {
        Self(vec_of_iters)
    }
}

impl<I: Iterator> Iterator for MultiZip<I> {
    type Item = Vec<I::Item>;

    fn next(&mut self) -> Option<Self::Item> {
        self.0.iter_mut().map(Iterator::next).collect()
    }
}

/// Read next or previous, wrapping if we go out of bounds.
///
/// This is particularly useful when porting from Python where reading
/// idx - 1, with -1 meaning last, is common.
pub trait WrappingGet<T> {
    fn wrapping_next(&self, idx: usize) -> &T;
    fn wrapping_prev(&self, idx: usize) -> &T;
}

impl<T> WrappingGet<T> for &[T] {
    fn wrapping_next(&self, idx: usize) -> &T {
        &self[match idx {
            _ if idx == self.len() - 1 => 0,
            _ => idx + 1,
        }]
    }

    fn wrapping_prev(&self, idx: usize) -> &T {
        &self[match idx {
            _ if idx == 0 => self.len() - 1,
            _ => idx - 1,
        }]
    }
}

/// Compare two floats for equality using relative and absolute tolerances.
///
/// This is useful when porting from Python where `math.isclose` is common.
///
/// References:
/// - [PEP425](https://peps.python.org/pep-0485/)
/// - [math.isclose](https://docs.python.org/3/library/math.html#math.isclose)
#[derive(Clone, Copy, Debug)]
pub struct FloatComparator {
    // TODO: Make it generic over T: Float? Need to add num-traits as a dependency
    rel_tol: f64,
    abs_tol: f64,
}

impl FloatComparator {
    /// Create a new FloatComparator with the specified relative and absolute tolerances.
    pub fn new(rel_tol: f64, abs_tol: f64) -> Self {
        Self { rel_tol, abs_tol }
    }

    /// Return true if a and b are close according to the specified tolerances.
    #[inline]
    pub fn isclose(self, a: f64, b: f64) -> bool {
        if a == b {
            return true;
        }
        if !a.is_finite() || !b.is_finite() {
            return false;
        }
        // The https://peps.python.org/pep-0485/ describes the algorithm used as:
        //   abs(a-b) <= max( rel_tol * max(abs(a), abs(b)), abs_tol )
        // In Rust, that would literally be:
        //   diff <= f64::max(self.rel_tol * f64::max(f64::abs(a), f64::abs(b)), self.abs_tol)
        // However below I use || instead of max(), since the logic works out the same and
        // should be a bit faster. In the PEP it's referred to as Boost's 'weak' formulation.
        let diff = (a - b).abs();
        diff <= (self.rel_tol * b).abs() || diff <= (self.rel_tol * a).abs() || diff <= self.abs_tol
    }
}

impl Default for FloatComparator {
    /// Create a new FloatComparator with `rel_to=1e-9` and `abs_tol=0.0`.
    fn default() -> Self {
        // same defaults as Python's math.isclose so we can match fontTools
        Self::new(1e-9, 0.0)
    }
}

/// Compare two floats for equality using the default FloatComparator.
///
/// To use different relative or absolute tolerances, create a FloatComparator
/// and use its `isclose` method.
pub fn isclose(a: f64, b: f64) -> bool {
    FloatComparator::default().isclose(a, b)
}
