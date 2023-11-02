// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a stub GL Interface for unit testing. The interface
// corresponds to the set of functionally distinct GL functions defined in
// generate_bindings.py, which may originate from either desktop GL or GLES.

#ifndef UI_GL_GL_STUB_API_BASE_H_
#define UI_GL_GL_STUB_API_BASE_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace gl {

class GL_EXPORT GLStubApiBase : public gl::GLApi {
 public:
  GLStubApiBase() {}
  ~GLStubApiBase() override {}

#include "ui/gl/gl_stub_autogen_gl.h"
};

}  // namespace gl

#endif  // UI_GL_GL_STUB_API_BASE_H_
