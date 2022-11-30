// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_VISUAL_PICKER_GLX_H_
#define UI_BASE_X_VISUAL_PICKER_GLX_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/glx.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// Picks the best X11 visuals to use for GL.  This class is adapted from GTK's
// pick_better_visual_for_gl.  Tries to find visuals that
// 1. Support GL
// 2. Support double buffer
// 3. Have an alpha channel only if we want one
class COMPONENT_EXPORT(UI_BASE_X) VisualPickerGlx {
 public:
  static VisualPickerGlx* GetInstance();

  VisualPickerGlx(const VisualPickerGlx&) = delete;
  VisualPickerGlx& operator=(const VisualPickerGlx&) = delete;

  ~VisualPickerGlx();

  x11::VisualId system_visual() const { return system_visual_; }

  x11::VisualId rgba_visual() const { return rgba_visual_; }

  x11::Glx::FbConfig GetFbConfigForFormat(gfx::BufferFormat format);

 private:
  friend struct base::DefaultSingletonTraits<VisualPickerGlx>;

  x11::VisualId PickBestGlVisual(
      const x11::Glx::GetVisualConfigsReply& configs,
      base::RepeatingCallback<bool(const x11::Connection::VisualInfo&)> pred,
      bool want_alpha) const;

  x11::VisualId PickBestSystemVisual(
      const x11::Glx::GetVisualConfigsReply& configs) const;

  x11::VisualId PickBestRgbaVisual(
      const x11::Glx::GetVisualConfigsReply& configs) const;

  void FillConfigMap();

  const raw_ptr<x11::Connection> connection_;

  x11::VisualId system_visual_{};
  x11::VisualId rgba_visual_{};

  std::unique_ptr<base::flat_map<gfx::BufferFormat, x11::Glx::FbConfig>>
      config_map_;

  VisualPickerGlx();
};

}  // namespace ui

#endif  // UI_BASE_X_VISUAL_PICKER_GLX_H_
