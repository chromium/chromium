// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_stub.h"

#include <GL/gl.h>

#include "base/notreached.h"
#include "ui/gfx/gpu_fence.h"

namespace gl {

GLImageStub::GLImageStub() {}

GLImageStub::~GLImageStub() {}

gfx::Size GLImageStub::GetSize() {
  return gfx::Size(1, 1);
}

}  // namespace gl
