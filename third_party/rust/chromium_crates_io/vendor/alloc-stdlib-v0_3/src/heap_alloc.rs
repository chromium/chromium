use std;


use super::{SliceWrapper, SliceWrapperMut, Allocator};

use std::ops;
use std::ops::Range;
use std::boxed::Box;
use std::vec::Vec;
pub struct WrapBox<T>(std::boxed::Box<[T]>);

impl<T> From<Vec<T>> for WrapBox<T> {
    fn from(data: Vec<T>) -> Self {
        WrapBox(data.into_boxed_slice())
    }
}

impl<T> Into<Box<[T]>> for WrapBox<T> {
    fn into(self) -> Box<[T]> {
        self.0
    }
}

impl<T> Default for WrapBox<T> {
    fn default() -> Self {
       let v : std::vec::Vec<T> = std::vec::Vec::new();
       let b = v.into_boxed_slice();
       return WrapBox::<T>(b);
    }
}

impl<T> super::SliceWrapper<T> for WrapBox<T> {
    fn slice(&self) -> & [T] {
       return &*self.0
    }
}

impl<T> super::SliceWrapperMut<T> for WrapBox<T> {
    fn slice_mut(&mut self) -> &mut [T] {
       return &mut*self.0
    }
}
impl<T> ops::Index<usize> for WrapBox<T> {
  type Output = T;
  fn index(&self, index: usize) -> &T {
    &(*self.0)[index]
  }
}

impl<T> ops::IndexMut<usize> for WrapBox<T> {
  fn index_mut(&mut self, index: usize) -> &mut T {
    &mut (*self.0)[index]
  }
}

impl<T> ops::Index<Range<usize>> for WrapBox<T> {
  type Output = [T];
  fn index(&self, index: Range<usize>) -> &[T] {
    &(*self.0)[index]
  }
}

impl<T> ops::IndexMut<Range<usize>> for WrapBox<T> {
  fn index_mut(&mut self, index: Range<usize>) -> &mut [T] {
    &mut (*self.0)[index]
  }
}


pub struct HeapAlloc<T : Clone>{
   pub default_value : T,
}

impl<T: Clone+Default> Default for HeapAlloc<T> {
    fn default() -> Self {
        Self::new(T::default())
    }
}

impl<T : Clone> HeapAlloc<T> {
   pub fn new(data : T) -> HeapAlloc<T> {
      return HeapAlloc::<T>{default_value : data};
   }
}

impl<T : Clone> super::Allocator<T> for HeapAlloc<T> {
   type AllocatedMemory = WrapBox<T>;
   fn alloc_cell(self : &mut HeapAlloc<T>, len : usize) -> WrapBox<T> {

       let v : std::vec::Vec<T> = vec![self.default_value.clone();len];
       let b = v.into_boxed_slice();
       return WrapBox::<T>(b);
   }
   fn free_cell(self : &mut HeapAlloc<T>, _data : WrapBox<T>) {

   }
}

#[deprecated]
pub type HeapAllocUninitialized<T> = HeapAlloc<T>;


pub struct HeapPrealloc<'a, T : 'a> {
   freelist : std::boxed::Box<[&'a mut [T]]>,
}
define_stack_allocator_traits!(HeapPrealloc, heap);

impl<'a, T : Clone+'a> HeapPrealloc<'a, T> {
    fn make_freelist(freelist_size : usize) -> std::boxed::Box<[&'a mut[T]]> {
        let mut retval = Vec::<&'a mut[T]>::with_capacity(freelist_size);
        for _i in 0..freelist_size {
            retval.push(&mut[]);
        }
        return retval.into_boxed_slice();
    }
    pub fn new_allocator(freelist_size : usize,
                     memory_pool : &'a mut Box<[T]>,
                     initializer : fn(&mut[T])) -> super::StackAllocator<'a, T, HeapPrealloc<'a, T> > {
        let mut retval = super::StackAllocator::<T, HeapPrealloc<T> > {
            nop : &mut [],
            system_resources : HeapPrealloc::<T> {
                freelist : Self::make_freelist(freelist_size),
            },
            free_list_start : freelist_size,
            free_list_overflow_count : 0,
            initialize : initializer,
        };
        retval.free_cell(super::AllocatedStackMemory::<T>{mem:&mut*memory_pool});
        return retval;
    }
    #[cfg(feature="unsafe")]
    pub unsafe fn new_uninitialized_memory_pool(len : usize) -> Box<[T]> {
        let mut v : std::vec::Vec<T> = std::vec::Vec::with_capacity(len);
        v.set_len(len);
        return v.into_boxed_slice();
    }
}

