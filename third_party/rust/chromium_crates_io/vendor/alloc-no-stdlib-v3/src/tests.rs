#![allow(unused_imports)]
#![allow(dead_code)]
#[cfg(test)]
extern crate core;
use core::ops;
use super::{Allocator, SliceWrapperMut, SliceWrapper,
            StackAllocator, AllocatedStackMemory, CallocBackingStore};

struct StackAllocatedFreelist4<'a, T : 'a> {
   freelist : [&'a mut [T]; 4],
}


impl<'a, T: 'a> SliceWrapper<&'a mut[T]> for StackAllocatedFreelist4<'a, T> {
    fn slice(& self) -> & [&'a mut[T]] {
        return & self.freelist;
    }
}

impl<'a, T: 'a> SliceWrapperMut<&'a mut [T]> for StackAllocatedFreelist4<'a, T> {
    fn slice_mut(& mut self) ->&mut [&'a mut [T]] {
        return &mut self.freelist;
    }
}

impl<'a, T: 'a> ops::Index<usize> for StackAllocatedFreelist4<'a, T> {
    type Output = [T];
    fn index<'b> (&'b self, _index : usize) -> &'b [T] {
        return &self.freelist[_index];
    }
}

impl<'a, T: 'a> ops::IndexMut<usize> for StackAllocatedFreelist4<'a, T> {
    fn index_mut<'b>(&'b mut self, _index : usize) -> &'b mut [T] {
        return &mut self.freelist[_index];
    }
}


#[test]
fn integration_test() {
  let mut global_buffer : [u8; 65536] = [0; 65536];
  let mut ags = StackAllocator::<u8, StackAllocatedFreelist4<u8> > {
      nop : &mut [],
      system_resources :  StackAllocatedFreelist4::<u8> {
          freelist : [&mut[],&mut[],&mut[],&mut[],],
      },
      free_list_start : 4,
      free_list_overflow_count : 0,
      initializer : bzero,
  };
  ags.free_cell(AllocatedStackMemory::<u8>{mem:&mut global_buffer});

  {
  let mut x = ags.alloc_cell(9999);
  x.slice_mut()[0] = 4;
  let mut y = ags.alloc_cell(4);
  y[0] = 5;
  ags.free_cell(y);

  let mut three = ags.alloc_cell(3);
  three[0] = 6;
  ags.free_cell(three);

  let mut z = ags.alloc_cell(4);
  z.slice_mut()[1] = 8;
  let mut reget_three = ags.alloc_cell(4);
  reget_three.slice_mut()[1] = 9;
  //y.mem[0] = 6; // <-- this is an error (use after free)
  assert_eq!(x[0], 4);
  assert_eq!(z[0], 6);
  assert_eq!(z[1], 8);
  assert_eq!(reget_three[0], 0);
  assert_eq!(reget_three[1], 9);
  let mut _z = ags.alloc_cell(1);
  }

}
