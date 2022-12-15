// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_PRESENTER_H_
#define UI_GL_PRESENTER_H_

#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Class that is used for presentation on the surfaceless platforms. Temporarily
// is in ui/gl and subclasses SurfacelessEGL. Base class will be removed and
// class will be moved to ui/gfx
class GL_EXPORT Presenter : public SurfacelessEGL {
 public:
  Presenter(GLDisplayEGL* display, const gfx::Size& size);

 protected:
  ~Presenter() override;
};

}  // namespace gl

#endif  // UI_GL_PRESENTER_H_
