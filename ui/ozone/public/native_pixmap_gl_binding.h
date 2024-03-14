// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_

#include "base/component_export.h"
#include "ui/gfx/buffer_types.h"

typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace ui {

// A binding maintained between NativePixmap and GL Texture in Ozone.
class COMPONENT_EXPORT(OZONE_BASE) NativePixmapGLBinding {
 public:
  NativePixmapGLBinding();
  virtual ~NativePixmapGLBinding();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
