use std::{collections::HashMap, sync::Arc};

use crate::{Fft, FftDirection};

pub(crate) struct FftCache<T> {
    forward_cache: HashMap<usize, Arc<dyn Fft<T>>>,
    inverse_cache: HashMap<usize, Arc<dyn Fft<T>>>,
}
impl<T> FftCache<T> {
    pub fn new() -> Self {
        Self {
            forward_cache: HashMap::new(),
            inverse_cache: HashMap::new(),
        }
    }
    #[allow(unused)]
    pub fn contains_fft(&self, len: usize, direction: FftDirection) -> bool {
        match direction {
            FftDirection::Forward => self.forward_cache.contains_key(&len),
            FftDirection::Inverse => self.inverse_cache.contains_key(&len),
        }
    }
    pub fn get(&self, len: usize, direction: FftDirection) -> Option<Arc<dyn Fft<T>>> {
        match direction {
            FftDirection::Forward => self.forward_cache.get(&len),
            FftDirection::Inverse => self.inverse_cache.get(&len),
        }
        .map(Arc::clone)
    }
    pub fn insert(&mut self, fft: &Arc<dyn Fft<T>>) {
        let cloned = Arc::clone(fft);
        let len = cloned.len();

        match cloned.fft_direction() {
            FftDirection::Forward => self.forward_cache.insert(len, cloned),
            FftDirection::Inverse => self.inverse_cache.insert(len, cloned),
        };
    }
}
