#![no_std]
// The only unsafe code in this crate is the C calloc/malloc backing store,
// which is gated behind the "unsafe" feature. Assert that the default build
// contains no unsafe code at all.
#![cfg_attr(not(feature="unsafe"), forbid(unsafe_code))]

#[macro_use]
mod allocated_memory;
mod stack_allocator;
mod allocated_stack_memory;
#[macro_use]
pub mod init;
pub use allocated_memory::SliceWrapper;
pub use allocated_memory::SliceWrapperMut;
pub use allocated_memory::AllocatedSlice;

pub use allocated_stack_memory::AllocatedStackMemory;
pub use stack_allocator::Allocator;
pub use stack_allocator::StackAllocator;

use core::default::Default;
pub fn bzero<T : Default> (data : &mut [T]) {
    for iter in data.iter_mut() {
        *iter = T::default();
    }
}

pub fn uninitialized<T> (_data : &mut[T]) {}



#[cfg(feature="unsafe")]
#[derive(Debug)]
pub struct CallocBackingStore<'a, T : 'a> {
    pub raw_data : *mut u8,
    pub data : &'a mut[T],
    free : unsafe extern "C" fn(*mut u8),
}

#[cfg(feature="unsafe")]
pub enum AllocatorC {
   Calloc(unsafe extern "C" fn(usize, usize) -> *mut u8),
   Malloc(unsafe extern "C" fn(usize) -> *mut u8),
   Custom(fn(usize) -> *mut u8),
}
#[cfg(feature="unsafe")]
impl<'a, T : 'a> CallocBackingStore<'a, T> {
  pub unsafe fn new(num_elements : usize, alloc : AllocatorC, free : unsafe extern "C" fn (*mut u8), should_free : bool) -> Self{
     let retval : *mut u8 = if num_elements == 0 {core::ptr::null_mut()} else {
        match alloc {
           AllocatorC::Calloc(calloc) => calloc(num_elements, core::mem::size_of::<T>()),
           AllocatorC::Malloc(malloc) => malloc(num_elements *core::mem::size_of::<T>()),
           AllocatorC::Custom(malloc) => malloc(num_elements *core::mem::size_of::<T>()),
        }
     };
     if num_elements == 0 || retval.is_null() {
        return CallocBackingStore::<'a, T>{
         raw_data : core::ptr::null_mut(),
         data : &mut[],
         free : free,
       }
     }
     let raw_data : *mut T = core::mem::transmute(retval);
     if should_free {
       return CallocBackingStore::<'a, T>{
         raw_data : retval,
         data : core::slice::from_raw_parts_mut(raw_data,
                                                num_elements),
         free : free,
       };
     } else {
       let null_ptr : *const u8 = core::ptr::null();
       return CallocBackingStore::<'a, T>{
         raw_data : core::mem::transmute(null_ptr),//retval,
         data : core::slice::from_raw_parts_mut(raw_data,
                                                num_elements),
         free : free,
       };
    }
  }
}
#[cfg(feature="unsafe")]
impl<'a, T:'a> Drop for CallocBackingStore<'a, T> {
  fn drop(self :&mut Self) {
//      core::mem::forget(core::mem::replace(self.data, &mut[]));
    core::mem::forget(core::mem::replace(&mut self.data, &mut[]));
    if !self.raw_data.is_null() {
      let local_free = self.free;
      unsafe {(local_free)(self.raw_data)};

    }
  }
}
