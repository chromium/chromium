# Framework for allocating memory in #![no_std] modules.

[![crates.io](http://meritbadge.herokuapp.com/alloc-no-stdlib)](https://crates.io/crates/alloc-no-stdlib)
[![Build Status](https://travis-ci.org/dropbox/rust-alloc-no-stdlib.svg?branch=master)](https://travis-ci.org/dropbox/rust-alloc-no-stdlib)


## Requirements
 * Rust 1.6

## Documentation
Currently there is no standard way to allocate memory from within a module that is no_std.
This provides a mechanism to describe a memory allocation that can be satisfied entirely on
the stack, by unsafely linking to calloc, or by unsafely referencing a mutable global variable.
This library currently will leak memory if free_cell isn't specifically invoked on memory.

However, if linked by a library that actually can depend on the stdlib then that library
can simply pass in a few allocators and use the standard Box allocation and will free automatically.

This library should also make it possible to entirely jail a rust application that needs dynamic
allocations by preallocating a maximum limit of data upfront using calloc and
using seccomp to disallow future syscalls.

## Usage

There are 3 modes for allocating memory, each with advantages and disadvantages

### On the stack
This is possible without the stdlib at all
However, this eats into the natural ulimit on the stack depth and generally
limits the program to only a few megs of dynamically allocated data

Example:

```rust
// First define a struct to hold all the array on the stack.
declare_stack_allocator_struct!(StackAllocatedFreelist4, 4, stack);
// since generics cannot be used, the actual struct to hold the memory must be defined with a macro
...

// in the code where the memory must be used, first the array needs to be readied
let mut stack_buffer = define_allocator_memory_pool!(4, u8, [0; 65536], stack);
// then an allocator needs to be made and pointed to the stack_buffer on the stack
// the final argument tells the system if free'd data should be zero'd before being
// reused by a subsequent call to alloc_cell
let mut ags = StackAllocatedFreelist4::<u8>::new_allocator(&mut stack_buffer, bzero);
{
    // now we can get memory dynamically
    let mut x = ags.alloc_cell(9999);
    x.slice_mut()[0] = 4;
    // get more memory
    let mut y = ags.alloc_cell(4);
    y[0] = 5;
    // and free it, consuming the buffer
    ags.free_cell(y);

    //y.mem[0] = 6; // <-- this is an error: won't compile (use after free)
    assert_eq!(x[0], 4);
```

### On the heap
This uses the standard Box facilities to allocate memory

```rust
let mut halloc = HeapAlloc::<u8>::new(0);
for _i in 1..10 { // heap test
    let mut x = halloc.alloc_cell(100000);
    x[0] = 4;
    let mut y = halloc.alloc_cell(110000);
    y[0] = 5;
    let mut z = halloc.alloc_cell(120000);
    z[0] = 6;
    assert_eq!(y[0], 5);
    halloc.free_cell(y);
    assert_eq!(x[0], 4);
    assert_eq!(x[9], 0);
    assert_eq!(z[0], 6);
}
```

### On the heap, but uninitialized
This does allocate data every time it is requested, but it does not allocate the
memory, so naturally it is unsafe. The caller must initialize the memory properly
```rust
let mut halloc = unsafe{HeapAllocUninitialized::<u8>::new()};
{ // heap test
    let mut x = halloc.alloc_cell(100000);
    x[0] = 4;
    let mut y = halloc.alloc_cell(110000);
    y[0] = 5;
    let mut z = halloc.alloc_cell(120000);
    z[0] = 6;
    assert_eq!(y[0], 5);
    halloc.free_cell(y);
    assert_eq!(x[0], 4);
    assert_eq!(x[9], 0);
    assert_eq!(z[0], 6);
    ...
}
```


### On the heap in a single pool allocation
This does a single big allocation on the heap, after which no further usage of the stdlib
will happen. This can be useful for a jailed application that wishes to restrict syscalls
at this point

```rust
use alloc_no_stdlib::HeapPrealloc;
...
let mut heap_global_buffer = define_allocator_memory_pool!(4096, u8, [0; 6 * 1024 * 1024], heap);
let mut ags = HeapPrealloc::<u8>::new_allocator(4096, &mut heap_global_buffer, uninitialized);
{
    let mut x = ags.alloc_cell(9999);
    x.slice_mut()[0] = 4;
    let mut y = ags.alloc_cell(4);
    y[0] = 5;
    ags.free_cell(y);

    //y.mem[0] = 6; // <-- this is an error (use after free)
}
```



### On the heap, uninitialized
This does a single big allocation on the heap, after which no further usage of the stdlib
will happen. This can be useful for a jailed application that wishes to restrict syscalls
at this point. This option keep does not set the memory to a valid value, so it is
necessarily marked unsafe

```rust
use alloc_no_stdlib::HeapPrealloc;
...
let mut heap_global_buffer = unsafe{HeapPrealloc::<u8>::new_uninitialized_memory_pool(6 * 1024 * 1024)};
let mut ags = HeapPrealloc::<u8>::new_allocator(4096, &mut heap_global_buffer, uninitialized);
{
    let mut x = ags.alloc_cell(9999);
    x.slice_mut()[0] = 4;
    let mut y = ags.alloc_cell(4);
    y[0] = 5;
    ags.free_cell(y);

    //y.mem[0] = 6; // <-- this is an error (use after free)
}
```

### With calloc
This is the most efficient way to get a zero'd dynamically sized buffer without the stdlib
It does invoke the C calloc function and hence must invoke unsafe code.
In this version, the number of cells are fixed to the parameter specified in the struct definition
(4096 in this example)

```rust
extern {
    fn calloc(n_elem : usize, el_size : usize) -> *mut u8;
    fn malloc(len : usize) -> *mut u8;
    fn free(item : *mut u8);
}

declare_stack_allocator_struct!(CallocAllocatedFreelist4096, 4096, calloc);
...

// the buffer is defined with 200 megs of zero'd memory from calloc
let mut calloc_global_buffer = unsafe {define_allocator_memory_pool!(4096, u8, [0; 200 * 1024 * 1024], calloc)};
// and assigned to a new_allocator
let mut ags = CallocAllocatedFreelist4096::<u8>::new_allocator(&mut calloc_global_buffer.data, bzero);
{
    let mut x = ags.alloc_cell(9999);
    x.slice_mut()[0] = 4;
    let mut y = ags.alloc_cell(4);
    y[0] = 5;
    ags.free_cell(y);
    //y.mem[0] = 6; // <-- this is an error (use after free)
}
```

### With a static, mutable buffer
If a single buffer of data is needed for the entire span of the application
Then the simplest way to do so without a zero operation on
the memory and without using the stdlib is to simply have a global allocated
structure. Accessing mutable static variables requires unsafe code; however,
so this code will invoke an unsafe block.


Make sure to only reference global_buffer in a single place, at a single time in the code
If it is used from two places or at different times, undefined behavior may result,
since multiple allocators may get access to global_buffer.


```rust
declare_stack_allocator_struct!(GlobalAllocatedFreelist, 16, global);
define_allocator_memory_pool!(16, u8, [0; 1024 * 1024 * 100], global, global_buffer);

...
// this references a global buffer
let mut ags = GlobalAllocatedFreelist::<u8>::new_allocator(bzero);
unsafe {
    bind_global_buffers_to_allocator!(ags, global_buffer, u8);
}
{
    let mut x = ags.alloc_cell(9999);
    x.slice_mut()[0] = 4;
    let mut y = ags.alloc_cell(4);
    y[0] = 5;
    ags.free_cell(y);

    //y.mem[0] = 6; // <-- this is an error (use after free)
}
```

## What changed in 3.0.0

Forbid unsafe and moving of the Calloc malloc into the unsafe bin so we can
forbid unsafe using the compiler.

Note: this is a breaking change for downstreams that referenced
    CallocBackingStore/AllocatorC without the "unsafe" feature (e.g. brotli's
    "seccomp" feature, which should now also enable alloc-no-stdlib/unsafe).


## Contributors
- Daniel Reiter Horn
