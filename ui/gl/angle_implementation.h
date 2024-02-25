// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANGLE_IMPLEMENTATION_H_
#define UI_GL_ANGLE_IMPLEMENTATION_H_

namespace gl {

enum class ANGLEImplementation {
  kNone = 0,
  kD3D9 = 1,
  kD3D11 = 2,
  kOpenGL = 3,
  kOpenGLES = 4,
  kNull = 5,
  kVulkan = 6,
  kSwiftShader = 7,
  kMetal = 8,
  kDefault = 9,
  kMaxValue = kDefault,
};

}  // namespace gl

#endif  // UI_GL_ANGLE_IMPLEMENTATION_H_
