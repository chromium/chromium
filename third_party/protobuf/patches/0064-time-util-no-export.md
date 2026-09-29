# `0064-time-util-no-export.patch`

## Status: Keep as-is

This patch is intentionally kept as a permanent Chromium-local patch and will
not be upstreamed.

## What the patch does

Immediately after `#include "google/protobuf/port_def.inc"` in
`src/google/protobuf/util/time_util.h`, this patch adds:

```cpp
#undef PROTOBUF_EXPORT
#define PROTOBUF_EXPORT
```

This strips the `PROTOBUF_EXPORT` attribute from `class PROTOBUF_EXPORT
TimeUtil` for the duration of `time_util.h` (before `port_undef.inc` undefines
the macro at the end of the header).

## Why Chromium carries this patch

In upstream CMake builds (the only upstream build configuration that produces
shared libraries/DLLs), `src/google/protobuf/util/time_util.cc` is compiled
directly into the `libprotobuf` shared library (`src/file_lists.cmake`).

In Chromium, `time_util.cc` cannot be compiled inside `protobuf_lite` because:

1. Chromium uses `protobuf_lite` rather than `protobuf_full` in production.
2. `time_util.h` includes the Well-Known Type (WKT) headers
   `google/protobuf/duration.pb.h` and `google/protobuf/timestamp.pb.h`.
3. In Chromium, lite WKT `.pb.{h,cc}` files are generated at build time by
   targets that depend on `protobuf_lite`. Putting `time_util.cc` inside
   `protobuf_lite` would create a circular dependency in GN.

Instead, Chromium defines a separate `source_set("time_util")` target in
`//third_party/protobuf/BUILD.gn` (used by `//third_party/federated_compute`).

On **Windows component builds** (`is_component_build = true`), consumers of
`protobuf_lite` inherit `defines = [ "PROTOBUF_USE_DLLS" ]` without
`LIBPROTOBUF_EXPORTS`. In `src/google/protobuf/port_def.inc`, this causes
`PROTOBUF_EXPORT` to expand to `__declspec(dllimport)`:

| Build configuration | `PROTOBUF_EXPORT` expansion in `time_util.cc` |
| --- | --- |
| Non-component build (all OSes) | *(empty)* (`PROTOBUF_USE_DLLS` is not defined) |
| Component build (Linux, macOS, Android) | *(empty)* (`LIBPROTOBUF_EXPORTS` is not defined) |
| Component build (Windows / MSVC ABI) | `__declspec(dllimport)` |

Without this patch, compiling `time_util.cc` in `source_set("time_util")` on a
Windows component build declares `class __declspec(dllimport) TimeUtil` when
parsing `time_util.h` and then defines its out-of-line member functions in the
same translation unit, failing with:

```text
error: 'google::protobuf::util::TimeUtil::...' redeclared without 'dllimport'
attribute: 'dllexport' attribute added [-Werror,-Winconsistent-dllimport]
```

## Why this patch is not upstreamed

Upstream CMake compiles `time_util.cc` inside `libprotobuf`, and upstream Bazel
only uses static linking on Windows where `PROTOBUF_USE_DLLS` is not defined.
Supporting a separately compiled `time_util` in a DLL build upstream would
require introducing a dedicated `PROTOBUF_UTIL_EXPORT` macro gated on a separate
`LIBPROTOBUF_UTIL_EXPORTS` define in `port_def.inc` for a target topology that
no upstream build system uses.

## Why local alternatives are worse

1. **Define `LIBPROTOBUF_EXPORTS` on `source_set("time_util")`:**
   Defining `LIBPROTOBUF_EXPORTS` when compiling `time_util.cc` would flip
   *every* `PROTOBUF_EXPORT` declaration seen by that translation unit
   (including `message_lite.h`, `arena.h`, etc.) from `__declspec(dllimport)` to
   `__declspec(dllexport)`, causing downstream DLLs to re-export core protobuf
   symbols and breaking imports of exported protobuf data symbols.
2. **Suppress `-Wno-inconsistent-dllimport` on `source_set("time_util")`:**
   When `-Winconsistent-dllimport` is suppressed, `clang-cl` implicitly converts
   the out-of-line definitions to `dllexport` while callers in other translation
   units still reference `__imp_...` symbols, relying on fragile linker
   fallbacks.
3. **Predefine `PROTOBUF_EXPORT` via GN `defines`:**
   `src/google/protobuf/port_def.inc` explicitly checks `#ifdef PROTOBUF_EXPORT`
   and triggers `#error PROTOBUF_EXPORT was previously defined`.
