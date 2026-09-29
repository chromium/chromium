# `0065-gzip-stream-no-export.patch`

## Status: Keep as-is

This patch is intentionally kept as a permanent Chromium-local patch and will
not be upstreamed.

## What the patch does

Immediately after `#include "google/protobuf/port_def.inc"` in
`src/google/protobuf/io/gzip_stream.h`, this patch adds:

```cpp
#undef PROTOBUF_EXPORT
#define PROTOBUF_EXPORT
```

This strips the `PROTOBUF_EXPORT` attribute from `GzipInputStream` and
`GzipOutputStream` for the duration of `gzip_stream.h` (before `port_undef.inc`
undefines the macro at the end of the header).

## Why Chromium carries this patch

In upstream CMake builds, `src/google/protobuf/io/gzip_stream.cc` is compiled
directly into `libprotobuf` (`src/file_lists.cmake`).

In Chromium, `gzip_stream.cc` depends on `//third_party/zlib`. Rather than
forcing `protobuf_lite` (and every target that depends on it) to depend on
`zlib`, Chromium compiles `gzip_stream.cc` in a standalone
`source_set("io_gzip_stream")` target in `//third_party/protobuf/BUILD.gn`
(used by `//third_party/federated_compute:base`).

For the same reason documented in `patches/0064-time-util-no-export.md`,
targets outside the `protobuf_lite` component on **Windows component builds**
(`is_component_build = true`) inherit `PROTOBUF_USE_DLLS` without
`LIBPROTOBUF_EXPORTS`, causing `PROTOBUF_EXPORT` to expand to
`__declspec(dllimport)`. Without this patch, compiling `gzip_stream.cc` in
`source_set("io_gzip_stream")` on Windows component builds fails with:

```text
error: 'google::protobuf::io::GzipInputStream::...' redeclared without
'dllimport' attribute: 'dllexport' attribute added
[-Werror,-Winconsistent-dllimport]
```

## Why this patch is not upstreamed

Upstream CMake builds `gzip_stream.cc` inside `libprotobuf`, and upstream Bazel
does not use `PROTOBUF_USE_DLLS`. Upstreaming would require a separate
per-target export macro (`PROTOBUF_GZIP_EXPORT`) in `port_def.inc` that upstream
build configurations do not need.

## Why local alternatives are worse

1. **Fold `gzip_stream.cc` into the protobuf component in component builds:**
   Unlike `time_util` (which has a dependency cycle with generated WKT headers),
   `//third_party/zlib` is not a dependency cycle. However, folding
   `gzip_stream.cc` into `protobuf_lite` only when `is_component_build` is true
   would introduce build-mode asymmetry in `BUILD.gn` and add a `zlib`
   dependency to `protobuf_lite` in developer component builds, which is more
   complex than carrying a 3-line header patch.
2. **Define `LIBPROTOBUF_EXPORTS` or pass `-Wno-inconsistent-dllimport` on
   `source_set("io_gzip_stream")`:**
   Has the same symbol re-export and `__imp_` mismatch pitfalls described in
   `patches/0064-time-util-no-export.md`.
