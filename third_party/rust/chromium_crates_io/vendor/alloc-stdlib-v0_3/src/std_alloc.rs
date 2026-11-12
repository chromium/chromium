use super::heap_alloc::WrapBox;
use super::{Allocator};
#[derive(Default, Clone, Copy, Debug)]
pub struct StandardAlloc{}

impl<T: Clone+Default> Allocator<T> for StandardAlloc {
   type AllocatedMemory = WrapBox<T>;
   fn alloc_cell(&mut self, len : usize) -> WrapBox<T> {
       vec![T::default().clone();len].into()
   }
   fn free_cell(&mut self, _data : WrapBox<T>) {

   }
}
