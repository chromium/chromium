/*!
Loaded Dice
============

A simple module that implements a random sampler implementing the [alias method](https://en.wikipedia.org/wiki/Alias_method). It can be used to sample from discrete probability distributions efficiently (`O(1)` per sample). One uses it by passing a vector of probabilities to the constructor. The constructor builds a data structure in `O(n*n*log(n))` (Note: It would be quite possible to implement this in `O(n*log(n))`, however for reasonable sized number of values this method is faster than using the more efficient data structures. If the construction is slow in your case, you might consider using min/max heaps instead of resorting the array after each construction step). This data structure can then be used to to sample a numbers between `0` and `n` with the corresponding probabilities.

Assume we want to sample from the following distribution: `p(0)=0.5, p(1)=0.3, p(2)=0.1, p(3)=0.1`:

```rust
# extern crate libafl_bolts;
use libafl_bolts::rands::{StdRand, loaded_dice::LoadedDiceSampler};
fn main() {
    let mut rand = StdRand::new();
    let mut sampler = LoadedDiceSampler::new(&[0.5, 0.3, 0.1, 0.1]).unwrap();
    let iter: usize = 100;
    for i in (0..iter) {
        println!("{}", sampler.sample(&mut rand));
    }
}
```

Original code by @eqv, see <https://github.com/eqv/loaded_dice>
*/

use alloc::vec::Vec;

use super::Rand;
use crate::Error;

/// Helper struct for [`LoadedDiceSampler`]
#[derive(Debug, Clone, PartialEq)]
struct AliasEntry {
    val: usize,
    alias: usize,
    prob_of_val: f64,
}

impl AliasEntry {
    /// Create a new [`AliasEntry`]
    pub fn new(val: usize, alias: usize, prob_of_val: f64) -> Self {
        AliasEntry {
            val,
            alias,
            prob_of_val,
        }
    }
}

/// A simple [`LoadedDiceSampler`]
#[derive(Debug, Clone, PartialEq)]
pub struct LoadedDiceSampler {
    entries: Vec<AliasEntry>,
}

impl LoadedDiceSampler {
    /// Create a new [`LoadedDiceSampler`] with the given probabilities
    pub fn new(probs: &[f64]) -> Result<Self, Error> {
        if probs.is_empty() {
            return Err(Error::illegal_argument(
                "Tried to construct LoadedDiceSampler with empty probs array",
            ));
        }
        let entries = LoadedDiceSampler::construct_table(probs);
        Ok(Self { entries })
    }

    /// Get one sample according to the predefined probabilities.
    pub fn sample<R: Rand>(&mut self, rand: &mut R) -> usize {
        let len = self.entries.len();
        debug_assert_ne!(len, 0, "Lenght should never be 0 here.");
        // # SAFETY
        // len can never be 0 here.
        let index = rand.below(unsafe { len.try_into().unwrap_unchecked() });
        let coin = rand.next_float();
        let entry = &self.entries[index];
        if coin > entry.prob_of_val {
            entry.alias
        } else {
            entry.val
        }
    }

    /// Create the table for this [`LoadedDiceSampler`]
    #[expect(clippy::cast_precision_loss)]
    fn construct_table(probs: &[f64]) -> Vec<AliasEntry> {
        let mut res = vec![];
        let n = probs.len() as f64;
        let inv_n = 1.0 / probs.len() as f64;

        let mut tmp = { probs.iter().copied().enumerate().collect::<Vec<_>>() };

        while tmp.len() > 1 {
            // eqv: rust sort ist optimized for nearly sorted cases, so I assume that a
            // better implementation with priority queues might actually be slower, however if you
            // run into performance troubles, replace tmp with a min/max heap
            tmp.sort_by(|&(_, p1), &(_, p2)| p2.partial_cmp(&p1).unwrap()); // [biggest-prob, ..., smallest-prob]
            let (min_i, min_p) = tmp.pop().unwrap();
            let &mut (ref max_i, ref mut max_p) = tmp.get_mut(0).unwrap();
            res.push(AliasEntry::new(min_i, *max_i, min_p * n));
            let used_prob = inv_n - min_p;
            *max_p -= used_prob;
        }
        let (last_i, last_p) = tmp.pop().unwrap();
        debug_assert!(0.999 < last_p * n && last_p * n < 1.001); // last value should always be exactly 1 but floats...
        res.push(AliasEntry::new(last_i, usize::MAX, 1.0));

        res
    }
}

#[cfg(all(test, feature = "std"))]
mod tests {
    use alloc::vec::Vec;

    use super::LoadedDiceSampler;
    use crate::rands::{Rand, StdRand};

    #[test]
    #[expect(clippy::cast_precision_loss)]
    fn test_loaded_dice() {
        let mut rng = StdRand::with_seed(1337);
        let len = rng.between(3, 9);
        let base = (0..len).map(|_| rng.next_float()).collect::<Vec<_>>();
        let sum: f64 = base.iter().sum();
        let base = base.iter().map(|v| v / sum).collect::<Vec<_>>();
        let mut sampler = LoadedDiceSampler::new(&base).unwrap();
        let mut res: Vec<usize> = vec![0; len];
        let iter: usize = 1000000;
        for _ in 0..iter {
            let i = sampler.sample(&mut rng);
            res[i] += 1;
        }
        let _res_p = res
            .iter()
            .map(|&f| f as f64 / iter as f64)
            .collect::<Vec<_>>();
        //println!("{:?}", res_p);
        for (i, c) in res.iter().enumerate() {
            let p_i = *c as f64 / iter as f64;
            assert!(base[i] * 0.99 < p_i && base[i] * 1.01 > p_i);
        }
    }
}
