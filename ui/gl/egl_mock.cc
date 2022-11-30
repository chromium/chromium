// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/egl_mock.h"

namespace gl {

MockEGLInterface::MockEGLInterface() {}

MockEGLInterface::~MockEGLInterface() {}

MockEGLInterface* MockEGLInterface::interface_;

void MockEGLInterface::SetEGLInterface(MockEGLInterface* egl_interface) {
  interface_ = egl_interface;
}

}  // namespace gl
