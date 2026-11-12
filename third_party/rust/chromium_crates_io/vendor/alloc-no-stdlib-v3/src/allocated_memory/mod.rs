extern crate core;
#[macro_use]
mod index_macro;
use core::default::Default;
pub use core::ops::IndexMut;
pub use core::ops::Index;
pub use core::ops::Range;
pub trait SliceWrapper<T> {
    fn slice(& self) -> & [T];
    fn len(&self) -> usize{
        self.slice().len()
    }
}

pub trait SliceWrapperMut<T> : SliceWrapper<T> {
  fn slice_mut (&mut self) -> & mut [T];
}

pub trait AllocatedSlice<T>
    : SliceWrapperMut<T> + SliceWrapper<T> + Default {
}

impl<T, U> AllocatedSlice<T> for U where U : SliceWrapperMut<T> + SliceWrapper<T> + Default {

}
