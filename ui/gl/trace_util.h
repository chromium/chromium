// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_TRACE_UTIL_H_
#define UI_GL_TRACE_UTIL_H_

#include <stdint.h>

#include "base/trace_event/memory_allocator_dump.h"
#include "ui/gl/gl_export.h"

namespace gl {

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetGLTextureClientGUIDForTracing(uint64_t context_group_tracing_id,
                                 uint32_t texture_client_id);

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetGLRenderbufferGUIDForTracing(uint64_t context_group_tracing_id,
                                uint32_t renderbuffer_id);

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetGLTextureServiceGUIDForTracing(uint64_t texture_tracing_id);

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid GetGLBufferGUIDForTracing(
    uint64_t context_group_tracing_id,
    uint32_t buffer_id);

GL_EXPORT base::trace_event::MemoryAllocatorDumpGuid
GetGLTextureRasterGUIDForTracing(uint32_t texture_id);

}  // namespace gl

#endif  // UI_GL_TRACE_UTIL_H_
