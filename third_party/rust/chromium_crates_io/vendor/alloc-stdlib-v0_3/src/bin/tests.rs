#![allow(unused_imports)]
#[cfg(test)]
extern crate core;
use alloc_no_stdlib::{Allocator, SliceWrapper, SliceWrapperMut};
use super::{HeapAlloc, StandardAlloc};
#[test]
fn heap_test() {
  let mut halloc : HeapAlloc<u8> = HeapAlloc::<u8>{default_value: 0};
  for _i in 1..10 { // heap test
      let mut x = halloc.alloc_cell(100000);
      x.slice_mut()[0] = 4;
      let mut y = halloc.alloc_cell(110000);
      y.slice_mut()[0] = 5;
      let mut z = halloc.alloc_cell(120000);
      z.slice_mut()[0] = 6;
      assert_eq!(y.slice()[0], 5);
      halloc.free_cell(y);
      assert_eq!(x.slice()[0], 4);
      assert_eq!(x.slice()[9], 0);
      assert_eq!(z.slice()[0], 6);
  }

}

#[test]
fn std_test() {
  let mut halloc = StandardAlloc::default();
  for _i in 1..10 { // heap test
      let mut x = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut halloc, 100000);
      x.slice_mut()[0] = 4;
      let mut y = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut halloc, 110000);
      y.slice_mut()[0] = 5;
      let mut z = <StandardAlloc as Allocator<u8>>::alloc_cell(&mut halloc, 120000);
      z.slice_mut()[0] = 6;
      assert_eq!(y.slice()[0], 5);
      halloc.free_cell(y);
      assert_eq!(x.slice()[0], 4);
      assert_eq!(x.slice()[9], 0);
      assert_eq!(z.slice()[0], 6);
  }

}


#[cfg(feature="stdlib")]
#[test]
fn std_heap_test() {
  let mut halloc = HeapAlloc::<u8>::new(0);
  for _i in 1..10 { // heap test
      let mut x = halloc.alloc_cell(100000);
      x.slice_mut()[0] = 4;
      let mut y = halloc.alloc_cell(110000);
      y.slice_mut()[0] = 5;
      let mut z = halloc.alloc_cell(120000);
      z.slice_mut()[0] = 6;
      assert_eq!(y.slice()[0], 5);
      halloc.free_cell(y);
      assert_eq!(x.slice()[0], 4);
      assert_eq!(x.slice()[9], 0);
      assert_eq!(z.slice()[0], 6);
  }

}
