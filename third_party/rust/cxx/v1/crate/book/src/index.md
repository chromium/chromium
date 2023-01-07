<div class="badges">
<a href="https://github.com/dtolnay/cxx"><img src="https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github" alt="github" height="28" class="badge"></a><a href="https://crates.io/crates/cxx"><img src="https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust" alt="crates-io" height="28" class="badge"></a><a href="https://docs.rs/cxx"><img src="https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" alt="docs-rs" height="28" class="badge"></a>
</div>

# CXX — safe interop between Rust and C++

This library provides a safe mechanism for calling C++ code from Rust and Rust
code from C++. It carves out a regime of commonality where Rust and C++ are
semantically very similar and guides the programmer to express their language
boundary effectively within this regime. CXX fills in the low level stuff so
that you get a safe binding, preventing the pitfalls of doing a foreign function
interface over unsafe C-style signatures.

<div style="height:190px;width=718px;padding:44px 0 44px">
<object type="image/svg+xml" data="overview.svg"></object>
</div>

From a high level description of the language boundary, CXX uses static analysis
of the types and function signatures to protect both Rust's and C++'s
invariants. Then it uses a pair of code generators to implement the boundary
efficiently on both sides together with any necessary static assertions for
later in the build process to verify correctness.

The resulting FFI bridge operates at zero or negligible overhead, i.e. no
copying, no serialization, no memory allocation, no runtime checks needed.

The FFI signatures are able to use native data structures from whichever side
they please. In addition, CXX provides builtin bindings for key standard library
types like strings, vectors, Box, unique\_ptr, etc to expose an idiomatic API on
those types to the other language.

## Example

In this example we are writing a Rust application that calls a C++ client of a
large-file blobstore service. The blobstore supports a `put` operation for a
discontiguous buffer upload. For example we might be uploading snapshots of a
circular buffer which would tend to consist of 2 pieces, or fragments of a file
spread across memory for some other reason (like a rope data structure).

```rust,noplayground
#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type MultiBuf;

        fn next_chunk(buf: &mut MultiBuf) -> &[u8];
    }

    unsafe extern "C++" {
        include!("example/include/blobstore.h");

        type BlobstoreClient;

        fn new_blobstore_client() -> UniquePtr<BlobstoreClient>;
        fn put(self: &BlobstoreClient, buf: &mut MultiBuf) -> Result<u64>;
    }
}
```

Now we simply provide Rust definitions of all the things in the `extern "Rust"`
block and C++ definitions of all the things in the `extern "C++"` block, and get
to call back and forth safely.

The [***Tutorial***](tutorial.md) chapter walks through a fleshed out version of
this blobstore example in full detail, including all of the Rust code and all of
the C++ code. The code is also provided in runnable form in the *demo* directory
of <https://github.com/dtolnay/cxx>. To try it out, run `cargo run` from that
directory.

- [demo/src/main.rs](https://github.com/dtolnay/cxx/blob/master/demo/src/main.rs)
- [demo/include/blobstore.h](https://github.com/dtolnay/cxx/blob/master/demo/include/blobstore.h)
- [demo/src/blobstore.cc](https://github.com/dtolnay/cxx/blob/master/demo/src/blobstore.cc)

The key takeaway, which is enabled by the CXX library, is that the Rust code in
main.rs is 100% ordinary safe Rust code working idiomatically with Rust types
while the C++ code in blobstore.cc is 100% ordinary C++ code working
idiomatically with C++ types. The Rust code feels like Rust and the C++ code
feels like C++, not like C-style "FFI glue".

<br>

***Chapter outline:** See the hamburger menu in the top left if you are on a
small screen and it didn't open with a sidebar by default.*
