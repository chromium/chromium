//#![feature(trace_macros)]
extern crate alloc_stdlib;
#[macro_use]
extern crate alloc_no_stdlib;

extern crate core;
use core::ops;

pub use alloc_stdlib::{HeapAlloc, StandardAlloc};
use alloc_stdlib::HeapPrealloc;
mod tests;
#[cfg(feature="unsafe")]
extern {
  fn calloc(n_elem : usize, el_size : usize) -> *mut u8;
}
#[cfg(feature="unsafe")]
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
#[cfg(feature="unsafe")]
declare_stack_allocator_struct!(CallocAllocatedFreelist4, 4, calloc);
declare_stack_allocator_struct!(StackAllocatedFreelist16, 16, stack);

fn show_heap_prealloc() {
  let mut zero_global_buffer = define_allocator_memory_pool!(4, u8, [0; 1024 * 1024 * 20], heap);

  let mut boxallocator = HeapPrealloc::<u8>::new_allocator(1024 * 1024, &mut zero_global_buffer, bzero);

  {
    let mut x = boxallocator.alloc_cell(9999);
    x.slice_mut()[0] = 3;
    let mut y = boxallocator.alloc_cell(4);
    y[0] = 5;
    boxallocator.free_cell(y);

    let mut three = boxallocator.alloc_cell(3);
    three[0] = 6;
    boxallocator.free_cell(three);

    let mut z = boxallocator.alloc_cell(4);
    z.slice_mut()[1] = 8;
    let mut reget_three = boxallocator.alloc_cell(4);
    reget_three.slice_mut()[1] = 9;
    //y.mem[0] = 6; // <-- this is an error (use after free)
    println!("x[0] = {:?} z[0] = {:?}  z[1] = {:?} r3[0] = {:?} r3[1] = {:?}", x.mem[0], z.mem[0], z.mem[1], reget_three[0], reget_three.slice()[1]);
    let mut _z = boxallocator.alloc_cell(1);
  }
}

#[cfg(feature="unsafe")]
fn show_calloc() {
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
}

#[cfg(not(feature="unsafe"))]
fn show_calloc() {}

fn main() {
  show_calloc();

  let mut stack_global_buffer = define_allocator_memory_pool!(16, u8, [0; 1024 * 1024], stack);
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

  let mut halloc = HeapAlloc::<u8>::default();
  for _i in 1..10 { // heap test
      let mut x = halloc.alloc_cell(100000);
      x.slice_mut()[0] = 4;
      let mut y = halloc.alloc_cell(110000);
      y.slice_mut()[0] = 5;
      let mut z = halloc.alloc_cell(120000);
      z.slice_mut()[0] = 6;
      halloc.free_cell(y);
      println!("x[0] {:?} x[9] {:?} y[0] {:?} z[0] {:?}",
               x.slice()[0], x.slice()[9], -999, z.slice()[0]);
  }
  show_heap_prealloc();
}
