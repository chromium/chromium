// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_RESTORE_TEXTURE_H_
#define UI_GL_SCOPED_RESTORE_TEXTURE_H_

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Restores the texture binding that the passed-in target had at this object's
// creation when this object is destroyed. If a non-zero `new_binding` is passed
// in, binds `target` to it at construction time.
class GL_EXPORT ScopedRestoreTexture {
 public:
  // Clients can pass true for `restore_prev_even_if_invalid` to have the
  // construction-time binding restored at destruction even if that binding did
  // not refer to a valid texture at the time of construction, rather than the
  // default behavior of restoring to 0 at destruction in that case.
  // TODO(crbug.com/1367187): Eliminate this boolean once all clients are
  // converted to the default behavior.
  ScopedRestoreTexture(gl::GLApi* api,
                       GLenum target,
                       bool restore_prev_even_if_invalid = false,
                       GLuint new_binding = 0);

  ScopedRestoreTexture(const ScopedRestoreTexture&) = delete;
  ScopedRestoreTexture& operator=(const ScopedRestoreTexture&) = delete;

  ~ScopedRestoreTexture();

 private:
  const raw_ptr<gl::GLApi> api_;
  const GLenum target_;
  GLuint prev_binding_ = 0;
};

}  // namespace gl

#endif  // UI_GL_SCOPED_RESTORE_TEXTURE_H_
