# rust-brotli-decompressor

[![crates.io](https://img.shields.io/crates/v/brotli-decompressor.svg)](https://crates.io/crates/brotli-decompressor)
[![Build Status](https://travis-ci.org/dropbox/rust-brotli-decompressor.svg?branch=master)](https://travis-ci.org/dropbox/rust-brotli-decompressor)

## What's new in version 6.3.1

* One-shot decoding uses output capacity as a bounded ring-buffer growth hint,
  avoiding a full-window allocation for tiny streams with oversized output
  buffers.

## What's new in version 6.3.0

* Faster decoding and setup, reduced table allocations, and 32-bit optimizations.
* C one-shot decoding and decoder creation now report allocation failures.
* C preallocated decoding ABI change: callers using BrotliDecoderDecompressPrealloc
  should rebuild with the updated header
  (`HuffmanCode` alignment is now 4 bytes; field order changes on 32-bit targets).
  Standard streaming and one-shot C APIs retain their ABI.

## What's new in version 6.2.0

* Configurable metadata metablock callback
* Return correct error code for inputs that would trigger CVE-2026-111497 on the C decoder


## What's new in version 6.1.1

* Performance improvements for dictionaries, small reads, and large streams

## What's new in version 6.1.0

* Dynamic ring buffer: ring buffer is only allocated as needed
* Ability to select previous mechanisms at init time.
* Copy performance optimization
* Optimize huffman tree sizes
* Bit reader optimizations

## What's new in version 6.0.1

* The C API's default Rust allocator now reports allocation failures as decoder
  errors (or null from its allocation helpers) instead of aborting the process.
* Fixed undefined behavior in mutable slice handling and ensured partially
  initialized fallback allocations are cleaned up if initialization panics.

## What's new in version 6.0.0

* Unlike previous code, these commits were effectively authored by Claude.
  This represents a change in stance of this project that, in order to keep
  up with fast changes to the main brotli repository, we will need to employ
  AI models add rely on guardrails like forbid(unsafe_code) and extensive
  testing and fuzz testing to ensure the quality of this library.
  Libraries that do not wish to depend on AI-authored code should remain on 5.x.
* Fix #42: custom dictionaries now remain addressable after the ring buffer
  wraps. Dictionaries are kept in their own buffers (the C implementation's
  "compound dictionary" scheme) instead of being copied into the ring buffer,
  so streams where output + dictionary exceed the window decode correctly,
  and dictionaries larger than the window are no longer truncated.
* New attach_dictionary API on BrotliState, Decompressor and
  DecompressorWriter: attach up to 15 raw LZ77 prefix dictionaries before
  decoding, equivalent to the C BrotliDecoderAttachDictionary with
  BROTLI_SHARED_DICTIONARY_RAW (also exported over the FFI).
* Issue #27: serialized shared dictionaries (the 0x91 0x00 container from
  draft-vandevenne-shared-brotli-format) are supported via
  attach_serialized_dictionary / BROTLI_SHARED_DICTIONARY_SERIALIZED,
  including embedded LZ77 prefix dictionaries, custom word lists, custom
  transform lists (including the parameterized SHIFT transforms) and
  context-based dictionary selection. The tool accepts
  -serialized_dict=<file>.

## What's new in version 5.0.3
Stricter downstream for alloc-no-stdlib and alloc-stdlib so they don't span
versions.

## What's new in version 5.0.2
forbid(unsafe)
Fixes for seccomp features.

## What's new in version 5.0.1
Fix PADDING_2 error being silently dropped at end of stream
Fix --no-default-features tests

## What's new in version 5.0.0
No longer seed the context by the end of the lz77 dictionary. Instead use
zero for the seed as would happen without a dictionary. This matches the
behavior of brotli with a custom dictionary as specified in
https://datatracker.ietf.org/doc/draft-vandevenne-shared-brotli-format/

## What's new in version 4.0.3
* Better handling of corrupt brotli files.

## What's new in version 4.0.2
* Better handling of corrupt brotli files.

## What's new in version 4.0.0
* The FFI is no longer active by default to avoid ODR issues if multiple versions of brotli are included in several dependent crates.

## What's new in version 3.0.0
* The FFI is feature gated behind the ffi-api feature flag (on-by-default)

## What's new in version 2.5.0
* If you call write with extra bytes it will only return the bytes consumed
* Further calls to write will return Ok(0)

## What's new in version 2.4.0
* If you call read until the file is ended, it will return 0 bytes.
* Further calls to read will error if there are extra unconsumed bytes in the file.

## What's new in version 2.3.5
* Fix bug in BrotliFillBitWindow

## What's new in version 2.3.4
* Check for fully consumed buffers in the decompressor reader/writer.

## What's new in version 2.3.x
* Error handling for Write and Read implementations.
* Fixed issue with small buffer sizes on certain files when repeatedly calling Decode stream
* Expose BrotliDecoderIsFinished

## What's new in version 2.2.x
* into_impl for reader and writer classes
* removed BrotliStateCleanup since it happens upon drop()

## What's new in version 2.1.2
* Better handling of transient errors ( fixes #4 )
* Do not panic in debug mode on unexpected bytes (handle arithmetic overflow)
* Smaller stack allocations
* Never create slice::from_raw_parts with nil
* Better panic reporting to C FFI
* Backport fixes to brotli issues 502 and 506
b
## What's new in version 2.0.0

* Legacy Custom dictionaries (mostly useful for testing multithreaded brotli encoding and experimentation)
* New mechanism to request that the library should only depend on custom allocations: the --no-default-features flag since --features=std is on by default.
* Fully compatible C FFI to match the https://github.com/google/brotli C API and become a drop-in replacement

## Differential testing against the C implementation

`cargo test` includes a corpus of fixtures generated by the reference C
encoder with raw and serialized dictionaries attached (testdata/dict_corpus,
regenerated by scripts/dict_corpus/generate.py from a google/brotli
checkout).

For deeper assurance there is a coverage-guided differential fuzzer that
links the system brotli (>= 1.1.0, e.g. the libbrotli-dev package) into the
fuzz binary and checks, on every input: that any stream the C encoder
produces with raw (custom) dictionaries attached decodes byte-identically;
and that mutated or truncated streams produce identical success/failure
verdicts and outputs. (The system library is built without
BROTLI_EXPERIMENTAL, so serialized shared dictionaries are exercised by the
fixture corpus above rather than by the differential fuzzer.)

```
cd fuzz
cargo +nightly fuzz run differential_dictionaries --features c-compat
```

## Project Requirements

Direct no-stdlib port of the C brotli decompressor to Rust if --no-default-features is passed into the build

no dependency on the Rust stdlib: this library would be ideal for decompressing within a rust kernel among other things.

This will be useful to see how C and Rust compare in an apples-to-apples
comparison where the same algorithms and data structures and
optimizations are employed.

The current expected performance losses come from

1. an extra indirection in the hgroups
2. array bounds checks on every access
3. no ability to load a full aligned 64 bit or 128 bit item from a [u8]

the system also enables all syscalls to be "frontloaded" in the initial generation
of a memory pool for the allocator. Afterwards, SECCOMP can be activated or
other mechanisms can be used to secure the application, if desired

## Linking rust-brotli-decompressor with C code using the zero-cost rust FFI abstraction

This library has FFI exports which comply with the original C interfaces.
To build them, enter the c directory and just type make there.
That will build a small example program and the cdylib with the appropriate ffi in place to link against

the example, called c/main.c shows how to decompress a program using the streaming interface and the nonstreaming interface.

If a nostdlib version is desired, then an unstable rust must be used (to enable the custom panic handler)
and then the BrotliDecoderDecompress function is deactivated since that has no facilities for specifying a custom malloc

a customized malloc must be used if a nostdlib build is chosen and additionally the no-stdlib-ffi-binding cargo feature must be set
eg

cargo build --features='no-stdlib no-stdlib-ffi-binding' --release


## Usage

### With the io::Read abstraction

```rust
let mut input = brotli_decompressor::Decompressor::new(&mut io::stdin(), 4096 /* buffer size */);
```
then you can simply read input as you would any other io::Read class

### With the Stream Copy abstraction

```rust
match brotli_decompressor::BrotliDecompress(&mut io::stdin(), &mut io::stdout(), 65536 /* buffer size */) {
    Ok(_) => {},
    Err(e) => panic!("Error {:?}", e),
}
```

### With manual memory management

There are 3 steps to using brotli without stdlib

1. setup the memory manager
2. setup the BrotliState
3. in a loop, call BrotliDecompressStream

in Detail

```rust
// at global scope declare a MemPool type -- in this case we'll choose the heap to
// avoid unsafe code, and avoid restrictions of the stack size

declare_stack_allocator_struct!(MemPool, heap);

// at local scope, make a heap allocated buffers to hold uint8's uint32's and huffman codes
let mut u8_buffer = define_allocator_memory_pool!(4096, u8, [0; 32 * 1024 * 1024], heap);
let mut u32_buffer = define_allocator_memory_pool!(4096, u32, [0; 1024 * 1024], heap);
let mut hc_buffer = define_allocator_memory_pool!(4096, HuffmanCode, [0; 4 * 1024 * 1024], heap);
let heap_u8_allocator = HeapPrealloc::<u8>::new_allocator(4096, &mut u8_buffer, bzero);
let heap_u32_allocator = HeapPrealloc::<u32>::new_allocator(4096, &mut u32_buffer, bzero);
let heap_hc_allocator = HeapPrealloc::<HuffmanCode>::new_allocator(4096, &mut hc_buffer, bzero);

// At this point no more syscalls are going to be needed since everything can come from the allocators.

// Feel free to activate SECCOMP jailing or other mechanisms to secure your application if you wish.

// Now it's possible to setup the decompressor state
let mut brotli_state = BrotliState::new(heap_u8_allocator, heap_u32_allocator, heap_hc_allocator);

// The pool never merges the blocks it gets back, so allocate the ring buffer's
// whole window at once rather than growing it (see below)
brotli_state.set_initial_ring_buffer_size(u32::MAX);

// at this point the decompressor simply needs an input and output buffer and the ability to track
// the available data left in each buffer
loop {
    result = BrotliDecompressStream(&mut available_in, &mut input_offset, &input.slice(),
                                    &mut available_out, &mut output_offset, &mut output.slice_mut(),
                                    &mut written, &mut brotli_state);

    // just end the decompression if result is BrotliResult::ResultSuccess or BrotliResult::ResultFailure
}
```

This interface is the same interface that the C brotli decompressor uses

Like the C decoder, the decoder starts its ring buffer at 1 KiB and grows it
on demand, up to the stream's window, so a short stream never pays for the
whole window. Each time the ring buffer grows, the old one goes back to the
allocator. A pool like the one above never merges the blocks it gets back,
so growing the ring buffer to the window can take up to twice the window
from it, and the pool panics when it runs out. Calling
`set_initial_ring_buffer_size(u32::MAX)` before decoding allocates the whole
window at once instead, so the pool needs only the window plus 566 bytes for
the ring buffer. A stream whose data is all in its last metablock still gets
a ring buffer of just its size, as does small or incompressible input, which
encoders store as an uncompressed metablock and an empty last one, when that
empty metablock is already in the input as the uncompressed one starts.
Other values set the smallest ring buffer to start from, rounded up to a
power of two. The C API's
`BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION`, set with
`set_parameter(BrotliDecoderParameter::BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION, 1)`,
always allocates the whole window, even for such streams, and overrides
`set_initial_ring_buffer_size`.
Both can only be set before decoding starts, and the `Decompressor` and
`DecompressorWriter` wrappers, including their custom allocator and I/O
variants, offer them too. `brotli_decode_prealloc` sizes the ring buffer for
its scratch pool by itself.

### With a dictionary that lives elsewhere

`attach_dictionary` takes memory from the decoder's own allocator, and
`attach_dictionary_borrowed` borrows only a `&'static [u8]`. A dictionary
owned elsewhere, such as a buffer read at runtime and shared by many decoders,
can instead be attached by its size alone and lent to every call, so it only
has to outlive each call:

```rust
brotli_state.attach_external_dictionary(dictionary.len()); // before decoding
loop {
    result = BrotliDecompressStreamWithDictionary(&mut available_in, &mut input_offset, &input,
                                                  &mut available_out, &mut output_offset, &mut output,
                                                  &mut written, &dictionary, &mut brotli_state);
    // ...
}
```

Every call must lend a dictionary of the attached size; any other size,
including none from `BrotliDecompressStream`, fails with
`BROTLI_DECODER_ERROR_INVALID_ARGUMENTS`. At most one dictionary can be
attached this way.

Also feel free to use custom allocators that invoke Box directly.
This example illustrates a mechanism to avoid subsequent syscalls after the initial allocation
