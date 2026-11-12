#[macro_export]
macro_rules! static_array {
    (@accum (0, $($_ignored:expr),*) -> ($($body:tt)*))
        => {static_array!(@as_expr [$($body)*])};
    (@accum (1, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (0, $($expr),*) -> ($($body)* $($expr,)*))};
    (@accum (2, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (0, $($expr),*) -> ($($body)* $($expr,)* $($expr,)*))};
    (@accum (4, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (2, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (8, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (4, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (16, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (8, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (32, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (16, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (64, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (32, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (128, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (64, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (256, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (128, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (512, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (256, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (1024, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (512, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (2048, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (1024, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (4096, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (2048, $($expr,)* $($expr),*) -> ($($body)*))};
    (@accum (8192, $($expr:expr),*) -> ($($body:tt)*))
        => {static_array!(@accum (4096, $($expr,)* $($expr),*) -> ($($body)*))};

    (@as_expr $expr:expr) => {$expr};

    ($expr:expr; $n:tt) => { static_array!(@accum ($n, $expr) -> ()) };
}


#[macro_export]
macro_rules! define_stack_allocator_traits(
    ($name : ident, global) => {
        impl<'a, T: 'a> Default for $name<'a, T> {
            fn default() -> Self {
                return $name::<'a, T>{freelist : &mut[],};
            }
        }
        define_stack_allocator_traits!($name, generic);
    };
    ($name : ident, $freelist_size : tt, stack) => {
        impl<'a, T: 'a> Default for $name<'a, T> {
            fn default() -> Self {
                return $name::<'a, T>{freelist : static_array!(&mut[]; $freelist_size)};
            }
        }
        define_stack_allocator_traits!($name, generic);
    };
    ($name : ident, heap) => {
        impl<'a, T: 'a> Default for $name<'a, T> {
            fn default() -> Self {
                let v : Vec<&mut [T]> = Vec::new();
                let b = v.into_boxed_slice();
                return $name::<'a, T>{freelist : b};
            }
        }
        define_stack_allocator_traits!($name, generic);
    };
    ($name : ident, $freelist_size : tt, malloc) => {
        define_stack_allocator_traits!($name, calloc);
    };
    ($name : ident, $freelist_size : tt, calloc) => {
    
        impl<'a, T: 'a> Default for $name<'a, T> {
            fn default() -> Self {
                return $name::<'a, T>{freelist : static_array!(&mut[]; $freelist_size)};
            }
        }
        define_stack_allocator_traits!($name, generic);
    };
    ($name : ident, generic) => {
        impl<'a, T: 'a> SliceWrapper<&'a mut[T]> for $name<'a, T> {
            fn slice(& self) -> & [&'a mut[T]] {
                return & self.freelist;
            }
        }
        impl<'a, T: 'a> SliceWrapperMut<&'a mut [T]> for $name<'a, T> {
            fn slice_mut(& mut self) ->&mut [&'a mut [T]] {
                return &mut self.freelist;
            }
        }
        impl<'a, T: 'a> ops::Index<usize> for $name<'a, T> {
            type Output = [T];
            fn index<'b> (&'b self, _index : usize) -> &'b [T] {
                return &self.freelist[_index];
            }
        }

        impl<'a, T: 'a> ops::IndexMut<usize> for $name<'a, T> {
            fn index_mut<'b>(&'b mut self, _index : usize) -> &'b mut [T] {
                return &mut self.freelist[_index];
            }
        }
    };
);

#[macro_export]
macro_rules! declare_stack_allocator_struct(
    (@as_expr $expr : expr) => {$expr};
    (@new_method $name : ident, $freelist_size : tt) => {
        impl<'a, T: 'a> $name<'a, T> {
          fn new_allocator(global_buffer : &'a mut [T],
                           initializer : fn(&mut[T])) -> StackAllocator<'a, T, $name<'a, T> > {
              let mut retval = StackAllocator::<T, $name<T> > {
                  nop : &mut [],
                  system_resources : $name::<T>::default(),
                  free_list_start : declare_stack_allocator_struct!(@as_expr $freelist_size),
                  free_list_overflow_count : 0,
                  initialize : initializer,
              };
              retval.free_cell(AllocatedStackMemory::<T>{mem:global_buffer});
              return retval;
          }
        }
    };

    (@new_calloc_method $name : ident, $freelist_size : tt) => {
        impl<'a, T: 'a> $name<'a, T> {
          fn new_allocator(mut global_buffer : &'a mut [T],
                           initializer : fn(&mut[T])) -> StackAllocator<'a, T, $name<'a, T> > {
              let mut retval = StackAllocator::<T, $name<T> > {
                  nop : &mut [],
                  system_resources : $name::<T>::default(),
                  free_list_start : declare_stack_allocator_struct!(@as_expr $freelist_size),
                  free_list_overflow_count : 0,
                  initialize : initializer,
              };
              retval.free_cell(AllocatedStackMemory::<T>{mem:core::mem::replace(&mut global_buffer, &mut[])});
              return retval;
          }
        }
    };
    ($name :ident, $freelist_size : tt, malloc) => {
        declare_stack_allocator_struct!($name, $freelist_size, calloc);
    };
    ($name :ident, $freelist_size : tt, calloc) => {
        struct $name<'a, T : 'a> {
            freelist : [&'a mut [T]; declare_stack_allocator_struct!(@as_expr $freelist_size)],
        }
        define_stack_allocator_traits!($name,
                                       $freelist_size,
                                       calloc);
        declare_stack_allocator_struct!( @new_calloc_method $name, $freelist_size);
    };
    ($name :ident, $freelist_size : tt, stack) => {
        struct $name<'a, T : 'a> {
            freelist : [&'a mut [T];declare_stack_allocator_struct!(@as_expr $freelist_size)],
            // can't borrow here: make it on stack-- heap : core::cell::RefCell<[T; $heap_size]>
        }
        define_stack_allocator_traits!($name,
                                       $freelist_size,
                                       stack);
        declare_stack_allocator_struct!( @new_method $name, $freelist_size);
    };
    ($name :ident, $freelist_size : expr, global) => {
       struct $name <'a, T: 'a> {freelist : &'a mut [&'a mut [T]]}
       define_stack_allocator_traits!($name, global);
       impl<'a, T: 'a> $name<'a, T> {
          fn new_allocator(initializer : fn (&mut[T])) -> StackAllocator<'a, T, $name<'a, T> > {
              return StackAllocator::<T, $name<T> > {
                  nop : &mut [],
                  system_resources : $name::<T>::default(),
                  free_list_start : 0,
                  free_list_overflow_count : 0,
                  initialize : initializer,
              };
          }
       }
    };
);
#[macro_export]
macro_rules! bind_global_buffers_to_allocator(
    ($allocator : expr, $buffer : ident, $T : ty) => {
        $allocator.free_list_start = $buffer::FREELIST.len();
        $allocator.system_resources.freelist = &mut $buffer::FREELIST;
        $allocator.free_cell(AllocatedStackMemory::<$T>{mem:&mut $buffer::HEAP});
    };
);

#[macro_export]
macro_rules! define_allocator_memory_pool(
    (@as_expr $expr:expr) => {$expr};


    ($freelist_size : tt, $T : ty, [0; $heap_size : expr], calloc) => {
      alloc_no_stdlib::CallocBackingStore::<$T>::new($heap_size, alloc_no_stdlib::AllocatorC::Calloc(calloc), free, true);
    };
    ($freelist_size : tt, $T : ty, [0; $heap_size : expr], calloc_no_free) => {
       alloc_no_stdlib::CallocBackingStore::<$T>::new($heap_size, alloc_no_stdlib::AllocatorC::Calloc(calloc), free, false);
    };
    ($freelist_size : tt, $T : ty, [0; $heap_size : expr], malloc) => {
      alloc_no_stdlib::CallocBackingStore::<$T>::new($heap_size, alloc_no_stdlib::AllocatorC::Malloc(malloc), free, true);
    };
    ($freelist_size : tt, $T : ty, [0; $heap_size : expr], malloc_no_free) => {
       alloc_no_stdlib::CallocBackingStore::<$T>::new($heap_size, alloc_no_stdlib::AllocatorC::Malloc(malloc), free, false);
    };
    ($freelist_size : tt, $T : ty, [$default_value : expr; $heap_size : expr], heap) => {
       (vec![$default_value; $heap_size]).into_boxed_slice();
    };
    ($freelist_size : tt, $T : ty, [$default_value : expr; $heap_size : expr], stack) => {
       [$default_value; $heap_size];
    };
    ($freelist_size : tt, $T : ty, [$default_value : expr; $heap_size : expr], global, $name : ident) => {
       pub mod $name {
           pub static mut FREELIST : [&'static mut [$T];
                                  define_allocator_memory_pool!(@as_expr $freelist_size)]
               = static_array!(&mut[]; $freelist_size);
           pub static mut HEAP : [$T; $heap_size] = [$default_value; $heap_size];
       }
    };

);


