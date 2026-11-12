//#![feature(trace_macros)]
#[macro_use]
extern crate alloc_no_stdlib;
extern crate core;
use core::ops;
mod heap_alloc;

pub use heap_alloc::HeapAllocator;
mod tests;
extern {
  fn calloc(n_elem : usize, el_size : usize) -> *mut u8;
}
extern {
  fn free(ptr : *mut u8);
}


//use alloc::AllocatedSlice;
use alloc_no_stdlib::SliceWrapper;
use alloc_no_stdlib::SliceWrapperMut;
use alloc_no_stdlib::AllocatedStackMemory;
use alloc_no_stdlib::Allocator;
use alloc_no_stdlib::StackAllocator;

use alloc_no_stdlib::bzero;
declare_stack_allocator_struct!(CallocAllocatedFreelist4, 4, calloc);
declare_stack_allocator_struct!(StackAllocatedFreelist16, 16, stack);
#[repr(C)]

#[derive(PartialEq, Copy, Clone, Debug)]
pub struct HuffmanCode {
  pub value: u16, // symbol value or table offset
  pub bits: u8, // number of bits used for this symbol
}


impl Default for HuffmanCode {
  fn default() -> Self {
    HuffmanCode {
      value: 0,
      bits: 0,
    }
  }
}

fn main() {

  let mut global_buffer = unsafe {define_allocator_memory_pool!(4, u8, [0; 1024 * 1024 * 200], calloc)};
  {
  let gbref = &mut global_buffer;
{
  let mut ags = CallocAllocatedFreelist4::<u8>::new_allocator(gbref.data, bzero);

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
  println!("x[0] = {:?} z[0] = {:?}  z[1] = {:?} r3[0] = {:?} r3[1] = {:?}", x.mem[0], z.mem[0], z.mem[1], reget_three[0], reget_three.slice()[1]);
  let mut _z = ags.alloc_cell(1);
  }
  }
  }


  let mut stack_global_buffer = define_allocator_memory_pool!(16, u8, [0; 1024 * 1024], stack);
  let mut stack_global_buffer_hc = define_allocator_memory_pool!(16, HuffmanCode, [HuffmanCode::default(); 1024 * 1024], stack);
  {
  let mut stackallocatorhc = StackAllocatedFreelist16::<HuffmanCode>::new_allocator(&mut stack_global_buffer_hc, bzero);
    stackallocatorhc.alloc_cell(9999);
  }
  let mut stackallocator = StackAllocatedFreelist16::<u8>::new_allocator(&mut stack_global_buffer, bzero);
  {
    let mut x = stackallocator.alloc_cell(9999);
    x.slice_mut()[0] = 3;
    let mut y = stackallocator.alloc_cell(4);
    y[0] = 5;
    stackallocator.free_cell(y);

    let mut three = stackallocator.alloc_cell(3);
    three[0] = 6;
    stackallocator.free_cell(three);

    let mut z = stackallocator.alloc_cell(4);
    z.slice_mut()[1] = 8;
    let mut reget_three = stackallocator.alloc_cell(4);
    reget_three.slice_mut()[1] = 9;
    //y.mem[0] = 6; // <-- this is an error (use after free)
    println!("x[0] = {:?} z[0] = {:?}  z[1] = {:?} r3[0] = {:?} r3[1] = {:?}", x.mem[0], z.mem[0], z.mem[1], reget_three[0], reget_three.slice()[1]);
    let mut _z = stackallocator.alloc_cell(1);
  }

  let mut halloc : HeapAllocator<u8> = HeapAllocator::<u8>{default_value: 0};
  for _i in 1..10 { // heap test
      let mut x = halloc.alloc_cell(100000);
      x[0] = 4;
      let mut y = halloc.alloc_cell(110000);
      y[0] = 5;
      let mut z = halloc.alloc_cell(120000);
      z[0] = 6;
      halloc.free_cell(y);
      println!("x[0] {:?} x[9] {:?} y[0] {:?} z[0] {:?}",
               x[0], x[9], -999, z[0]);
  }
}
