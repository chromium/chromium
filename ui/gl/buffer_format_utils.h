// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_BUFFER_FORMAT_UTILS_H_
#define UI_GL_BUFFER_FORMAT_UTILS_H_

#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Map buffer format to GL internalformat. Return GL_NONE if no sensible
// mapping.
GL_EXPORT unsigned BufferFormatToGLInternalFormat(gfx::BufferFormat format);

// Map buffer format to GL type. Return GL_NONE if no sensible mapping.
GL_EXPORT unsigned BufferFormatToGLDataType(gfx::BufferFormat format);

}  // namespace gl

#endif  // UI_GL_BUFFER_FORMAT_UTILS_H_
