// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/trace_util.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gl {

base::trace_event::MemoryAllocatorDumpGuid GetGLTextureClientGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t texture_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gl-texture-client-x-process/%" PRIx64 "/%d",
                         context_group_tracing_id, texture_id));
}

base::trace_event::MemoryAllocatorDumpGuid GetGLBufferGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t buffer_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gl-buffer-x-process/%" PRIx64 "/%d",
                         context_group_tracing_id, buffer_id));
}

base::trace_event::MemoryAllocatorDumpGuid GetGLRenderbufferGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t renderbuffer_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gl-renderbuffer-x-process/%" PRIx64 "/%d",
                         context_group_tracing_id, renderbuffer_id));
}

base::trace_event::MemoryAllocatorDumpGuid GetGLTextureRasterGUIDForTracing(
    uint32_t texture_id) {
  return base::trace_event::MemoryAllocatorDumpGuid(
      base::StringPrintf("gl-texture-raster-gpu-process/%d", texture_id));
}

}  // namespace gl
