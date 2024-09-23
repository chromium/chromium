// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_MAKE_CURRENT_UNSAFE_H_
#define UI_GL_SCOPED_MAKE_CURRENT_UNSAFE_H_

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLContext;
class GLSurface;
}  // namespace gl

namespace ui {

// Same as ScopedMakeCurrent, but unsafe. Meaning that it doesn't store either
// |context| or |surface| as refptr. The client must ensure those outlive
// |this|.
//
// TODO(msisov): make ScopedMakeCurrent base and add safe and unsafe
// implementations instead of two independent ones.
class GL_EXPORT ScopedMakeCurrentUnsafe {
 public:
  ScopedMakeCurrentUnsafe(gl::GLContext* context, gl::GLSurface* surface);

  ScopedMakeCurrentUnsafe(const ScopedMakeCurrentUnsafe&) = delete;
  ScopedMakeCurrentUnsafe& operator=(const ScopedMakeCurrentUnsafe&) = delete;

  ~ScopedMakeCurrentUnsafe();

  // Returns whether the |context_| is current.
  bool IsContextCurrent() { return is_context_current_; }

 private:
  const raw_ptr<gl::GLContext> previous_context_;
  const raw_ptr<gl::GLSurface> previous_surface_;
  const raw_ptr<gl::GLContext> context_;
  const raw_ptr<gl::GLSurface> surface_;
  bool is_context_current_ = false;
};

}  // namespace ui

#endif  // UI_GL_SCOPED_MAKE_CURRENT_UNSAFE_H_
