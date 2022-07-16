// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_SHARED_MEMORY_H_
#define UI_GL_GL_IMAGE_SHARED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_memory.h"

namespace gl {

class GL_EXPORT GLImageSharedMemory : public GLImageMemory {
 public:
  explicit GLImageSharedMemory(const gfx::Size& size);

  GLImageSharedMemory(const GLImageSharedMemory&) = delete;
  GLImageSharedMemory& operator=(const GLImageSharedMemory&) = delete;

  bool Initialize(const base::UnsafeSharedMemoryRegion& shared_memory_region,
                  gfx::GenericSharedMemoryId shared_memory_id,
                  gfx::BufferFormat format,
                  size_t offset,
                  size_t stride);

  // Overridden from GLImage:
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageSharedMemory() override;

 private:
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  gfx::GenericSharedMemoryId shared_memory_id_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_SHARED_MEMORY_H_
