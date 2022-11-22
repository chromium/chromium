// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DCOMP_SURFACE_PROXY_H_
#define UI_GL_DCOMP_SURFACE_PROXY_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/video_types.h"

namespace gl {

class DCOMPSurfaceProxy : public base::RefCounted<DCOMPSurfaceProxy> {
 public:
  virtual const gfx::Size& GetSize() const = 0;
  virtual HANDLE GetSurfaceHandle() = 0;
  virtual void SetRect(const gfx::Rect& window_relative_rect) = 0;
  virtual void SetParentWindow(HWND parent) = 0;
  virtual void SetProtectedVideoType(
      gfx::ProtectedVideoType protected_video_type) = 0;

 protected:
  friend class base::RefCounted<DCOMPSurfaceProxy>;
  virtual ~DCOMPSurfaceProxy() = default;
};

}  // namespace gl

#endif  // UI_GL_DCOMP_SURFACE_PROXY_H_
