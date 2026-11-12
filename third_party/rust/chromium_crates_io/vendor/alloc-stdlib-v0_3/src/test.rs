#![cfg(test)]

use super::{SliceWrapperMut, SliceWrapper};
use std::vec::Vec;
use core;
use super::super::Allocator;
#[derive(Debug)]
pub struct ItemVec<Item>(Vec<Item>);
define_index_ops_mut!(T, ItemVec<T>);

impl<Item> Default for ItemVec<Item> {
    fn default() -> Self {
        ItemVec(Vec::<Item>::new())
    }
}
impl<Item> SliceWrapper<Item> for ItemVec<Item> {
    fn slice(&self) -> &[Item] {
        &self.0[..]
    }
}

impl<Item> SliceWrapperMut<Item> for ItemVec<Item> {
    fn slice_mut(&mut self) -> &mut [Item] {
        &mut self.0[..]
    }
}
/*
impl<Item> core::ops::Index<usize> for ItemVec<Item> {
    type Output = Item;
    fn index(&self, index:usize) -> &Item {
        &self.0[index]
    }
}

impl<Item> core::ops::IndexMut<usize> for ItemVec<Item> {

    fn index_mut(&mut self, index:usize) -> &mut Item {
        &mut self.0[index]
    }
}
*/
#[derive(Default)]
struct ItemVecAllocator<Item> {
    _item: core::marker::PhantomData<Item>,
}
impl<Item:Default+Clone> Allocator<Item> for ItemVecAllocator<Item> {
    type AllocatedMemory = ItemVec<Item>;
    fn alloc_cell(&mut self, size:usize) ->ItemVec<Item>{
        //eprint!("A:{}\n", size);
        ItemVec(vec![Item::default();size])
    }
    fn free_cell(&mut self, _bv:ItemVec<Item>) {
        //eprint!("F:{}\n", _bv.slice().len());
    }
}



#[derive(Copy,Clone)]
pub struct SliceReference<'a, T:'a> {
    data: &'a[T],
    start: usize,
    len: usize,
}

impl<'a, T:'a> SliceReference<'a, T> {
    pub fn new(input: &'a[T], start: usize, len: usize) -> SliceReference<'a, T> {
        SliceReference::<T> {
            data: input.split_at(start).1.split_at(len).0,
            start: start,
            len: len,
        }
    }
}

impl<'a, T:'a> SliceWrapper<T> for SliceReference<'a, T> {
    fn slice(&self) -> &[T]{
        self.data
    }
}

impl<'a, T> Default for SliceReference<'a, T> {
    fn default() ->SliceReference<'a, T> {
        SliceReference::<T> {
            data:&[],
            start:0,
            len:0,
        }
    }
}

define_index_ops!(a, T, SliceReference<'a, T>);

#[test]
fn test_index_ops() {
    let array = [255u8, 0u8, 1u8,2u8,3u8,4u8,5u8, 6u8];
    let sl = SliceReference::<u8>::new(&array[..], 1, 5);
    let val = sl[0];
    assert_eq!(val, 0);
    assert_eq!(&sl[1..5], &[1u8,2u8,3u8,4u8]);
    let mut al =ItemVecAllocator::<u64>::default();
    let mut dat = al.alloc_cell(1024);
    dat[0] = 0;
    dat[1] = 1;
    dat[2] = 2;
    dat[3] = 3;
    assert_eq!(dat[1], 1);
    assert_eq!(&dat[1..5], &[1u64,2u64,3u64,0u64]);
    assert_eq!(dat.len(), 1024);
    al.free_cell(dat);
   
}
