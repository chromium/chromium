// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_VISUAL_PICKER_GLX_H_
#define UI_GL_GL_VISUAL_PICKER_GLX_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace gl {

// Picks the best X11 visuals to use for GL.  This class is adapted from GTK's
// pick_better_visual_for_gl.  Tries to find visuals that
// 1. Support GL
// 2. Support double buffer
// 3. Have an alpha channel only if we want one
class GL_EXPORT GLVisualPickerGLX {
 public:
  static GLVisualPickerGLX* GetInstance();

  ~GLVisualPickerGLX();

  x11::VisualId system_visual() const { return system_visual_; }

  x11::VisualId rgba_visual() const { return rgba_visual_; }

  x11::Glx::FbConfig GetFbConfigForFormat(gfx::BufferFormat format) const;

 private:
  friend struct base::DefaultSingletonTraits<GLVisualPickerGLX>;

  x11::VisualId PickBestGlVisual(
      const x11::Glx::GetVisualConfigsReply& configs,
      base::RepeatingCallback<bool(const x11::Connection::VisualInfo&)> pred,
      bool want_alpha) const;

  x11::VisualId PickBestSystemVisual(
      const x11::Glx::GetVisualConfigsReply& configs) const;

  x11::VisualId PickBestRgbaVisual(
      const x11::Glx::GetVisualConfigsReply& configs) const;

  void FillConfigMap();

  x11::Connection* const connection_;

  x11::VisualId system_visual_;
  x11::VisualId rgba_visual_;

  base::flat_map<gfx::BufferFormat, x11::Glx::FbConfig> config_map_;

  GLVisualPickerGLX();

  DISALLOW_COPY_AND_ASSIGN(GLVisualPickerGLX);
};

}  // namespace gl

#endif  // UI_GL_GL_VISUAL_PICKER_GLX_H_
