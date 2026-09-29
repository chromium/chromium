# `0044-trim-protoc-main.patch`

## Status: Keep as-is

This patch is intentionally kept as a permanent Chromium-local patch and will
not be upstreamed.

## What the patch does

Removes the `#include` directives and `cli.RegisterGenerator(...)` calls for C#,
Kotlin, Objective-C, PHP, Ruby, RBS, and Rust from
`src/google/protobuf/compiler/main.cc`. Only the generators that Chromium uses
remain registered:

- C++ (`--cpp_out`)
- Java (`--java_out`)
- Python (`--python_out`)
- Python `.pyi` typing stubs (`--pyi_out`)

## Why Chromium carries this patch

Chromium's `executable("protoc")` target in `//third_party/protobuf/BUILD.gn`
compiles upstream `src/google/protobuf/compiler/main.cc` directly, but only
builds and links the generator targets required by the Chromium build:

- `:protoc_cpp`
- `:protoc_java`
- `:protoc_python`
- `:protoc_lib`

Upstream `main.cc` unconditionally instantiates and registers code generators
for every language shipped in the protobuf repository. Without this patch,
linking `protoc` fails with undefined symbol errors for the unused generators.
Compiling the six unused language generators into Chromium's host `protoc`
binary would add 71 non-test `.cc` files (C#: 19, Kotlin: 5, Objective-C: 17,
PHP: 3, Ruby: 3, Rust: 24) to the critical path of every clean build for
languages that Chromium never generates.

## Why this patch is not upstreamed

Upstream's official Bazel and CMake builds always ship a full `protoc` binary
supporting all built-in languages. Adding per-language preprocessor guards (such
as `PROTOC_NO_CSHARP`, `PROTOC_NO_RUST`, etc.) to `main.cc` would clutter
upstream for a build configuration that only Chromium uses.

## Why local alternatives are worse

1. **Maintain a Chromium-owned `protoc_main.cc` referenced from `BUILD.gn`:**
   Replacing `src/google/protobuf/compiler/main.cc` in `BUILD.gn` with a custom
   50-line entry point would eliminate the patch file, but it trades a loud,
   easy-to-resolve patch conflict at roll time for silent behavioral drift. When
   upstream adds initialization logic to `ProtobufMain` (for example,
   `absl::InitializeLog()`, `internal::DisableAllowlistInternalOnly()`, or new
   plugin/edition flags), a forked `protoc_main.cc` would silently miss those
   updates unless a separate roll-time check verifies that upstream `main.cc`
   has not changed.
2. **Provide stub headers for dropped generators:**
   Maintaining stub generator headers requires more boilerplate than this patch
   and obscures what `protoc` actually supports in Chromium.
3. **Compile all unused language generators in `BUILD.gn`:**
   Unnecessarily compiles 71 extra C++ translation units on the host toolchain
   before any `.proto` target in Chromium can be compiled.
