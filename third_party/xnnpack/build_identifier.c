// Copyright 2024 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

// Auto-generated file. Do not edit!
//   Generator: scripts/generate-build-identifier.py
//
// The following inputs were used to generate this file.
// - src/packing.c
// - src/enums/allocation-type.c
// - src/enums/datatype-strings.c
// - src/enums/microkernel-type.c
// - src/enums/node-type.c
// - src/enums/operator-type.c
// - src/log.c
// - src/configs/hardware-config.c
// - src/params.c
// - src/memory.c
// - src/mutex.c
// - src/tables/exp2-k-over-64.c
// - src/tables/exp2-k-over-2048.c
// - src/tables/exp2minus-k-over-4.c
// - src/tables/exp2minus-k-over-8.c
// - src/tables/exp2minus-k-over-16.c
// - src/tables/exp2minus-k-over-32.c
// - src/tables/exp2minus-k-over-64.c
// - src/tables/exp2minus-k-over-2048.c
// - src/tables/vlog.c
// - src/amalgam/gen/scalar.c
// - src/amalgam/gen/avxvnni.c
// - src/amalgam/gen/avx256skx.c
// - src/amalgam/gen/avx256vnni.c
// - src/amalgam/gen/avx256vnnigfni.c
// - src/amalgam/gen/avx512amx.c
// - src/amalgam/gen/avx512fp16.c
// - src/amalgam/gen/sse.c
// - src/amalgam/gen/sse2.c
// - src/amalgam/gen/ssse3.c
// - src/amalgam/gen/sse41.c
// - src/amalgam/gen/avx.c
// - src/amalgam/gen/f16c.c
// - src/amalgam/gen/fma3.c
// - src/amalgam/gen/avx2.c
// - src/amalgam/gen/avx512f.c
// - src/amalgam/gen/avx512skx.c
// - src/amalgam/gen/avx512vbmi.c
// - src/amalgam/gen/avx512vnni.c
// - src/amalgam/gen/avx512vnnigfni.c

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static const uint8_t xnn_build_identifier[] = {
   13, 147, 244, 144,   1,  49, 217, 184,
  141, 211, 194,  68,  60, 139, 106,  24,
  247,   4, 162,   8, 107, 114,   3, 206,
   82,   5, 194,  30, 143, 203,  79, 181
};

size_t xnn_experimental_get_build_identifier_size() {
  return sizeof(xnn_build_identifier);
}

const void* xnn_experimental_get_build_identifier_data() {
  return xnn_build_identifier;
}

bool xnn_experimental_check_build_identifier(const void* data, const size_t size) {
  if(size != xnn_experimental_get_build_identifier_size()) {
    return false;
  }
  return !memcmp(data, xnn_build_identifier, size);
}
