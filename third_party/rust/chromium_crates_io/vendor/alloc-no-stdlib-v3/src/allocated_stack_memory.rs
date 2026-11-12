extern crate core;

use super::allocated_memory::SliceWrapper;
use super::allocated_memory::SliceWrapperMut;

pub struct AllocatedStackMemory<'a, T:'a> {
    pub mem : &'a mut [T],
}

define_index_ops_mut!(a, T, AllocatedStackMemory<'a, T>);

impl<'a, T: 'a> core::default::Default for AllocatedStackMemory<'a, T> {
    fn default() -> Self {
        return AllocatedStackMemory::<'a, T>{mem : &mut[]};
    }
}


impl<'a, T: 'a> SliceWrapper<T> for AllocatedStackMemory<'a, T> {
    fn slice(& self) -> & [T] {
        return & self.mem;
    }
}

impl<'a, T: 'a> SliceWrapperMut<T> for AllocatedStackMemory<'a, T> {
    fn slice_mut(& mut self) ->& mut [T] {
        return &mut self.mem;
    }
}



