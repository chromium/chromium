// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_STUB_H_
#define UI_GL_GL_IMAGE_STUB_H_

#include <stdint.h>

#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

// A GLImage that does nothing for unit tests.
class GL_EXPORT GLImageStub : public GLImage {
 public:
  GLImageStub();

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  bool BindTexImage(unsigned target) override;

 protected:
  ~GLImageStub() override;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_STUB_H_
