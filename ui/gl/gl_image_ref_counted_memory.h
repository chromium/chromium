// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_REF_COUNTED_MEMORY_H_
#define UI_GL_GL_IMAGE_REF_COUNTED_MEMORY_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_memory.h"

namespace base {
class RefCountedMemory;
}

namespace gl {

class GL_EXPORT GLImageRefCountedMemory : public GLImageMemory {
 public:
  explicit GLImageRefCountedMemory(const gfx::Size& size);

  GLImageRefCountedMemory(const GLImageRefCountedMemory&) = delete;
  GLImageRefCountedMemory& operator=(const GLImageRefCountedMemory&) = delete;

  bool Initialize(base::RefCountedMemory* ref_counted_memory,
                  gfx::BufferFormat format);

  // Overridden from GLImage:
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageRefCountedMemory() override;

 private:
  scoped_refptr<base::RefCountedMemory> ref_counted_memory_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_REF_COUNTED_MEMORY_H_
