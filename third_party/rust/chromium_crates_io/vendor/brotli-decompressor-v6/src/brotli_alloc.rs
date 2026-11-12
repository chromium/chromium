#![cfg(feature="std")]
use core;
use core::ops;
use std::boxed::Box;
use std::vec::Vec;
pub struct WrapBox<T> {
   v : Vec<T>,
}

impl<T> core::default::Default for WrapBox<T> {
    fn default() -> Self {
       let v : Vec<T> = Vec::new();
       return WrapBox::<T>{v : v};
    }
}

impl<T> ops::Index<usize> for WrapBox<T>{
    type Output = T;
    fn index(&self, index : usize) -> &T {
        return &self.v[index]
    }
}

impl<T> ops::IndexMut<usize> for WrapBox<T>{
    fn index_mut(&mut self, index : usize) -> &mut T {
        return &mut self.v[index]
    }
}

impl<T> super::SliceWrapper<T> for WrapBox<T> {
    fn slice(&self) -> & [T] {
       return &self.v[..]
    }
}

impl<T> super::SliceWrapperMut<T> for WrapBox<T> {
    fn slice_mut(&mut self) -> &mut [T] {
       return &mut self.v[..]
    }
}

pub struct BrotliAlloc<T : core::clone::Clone>{
   pub default_value : T,
}
impl<T : core::clone::Clone+Default> BrotliAlloc<T> {
   pub fn new() -> BrotliAlloc<T> {
      return BrotliAlloc::<T>{default_value : T::default()};
   }
   pub fn take_ownership(&self, data: Vec<T>) -> WrapBox<T>{
      WrapBox::<T>{v:data}
   }
}

impl<T : core::clone::Clone> super::Allocator<T> for BrotliAlloc<T> {
   type AllocatedMemory = WrapBox<T>;
   fn alloc_cell(self : &mut BrotliAlloc<T>, len : usize) -> WrapBox<T> {

       let v : Vec<T> = vec![self.default_value.clone();len];
       return WrapBox::<T>{v : v};
   }
   fn free_cell(self : &mut BrotliAlloc<T>, _data : WrapBox<T>) {}
}
