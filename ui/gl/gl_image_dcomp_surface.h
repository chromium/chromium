// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_DCOMP_SURFACE_H_
#define UI_GL_GL_IMAGE_DCOMP_SURFACE_H_

#include <dcomp.h>
#include <wrl/client.h>

#include "base/win/scoped_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageDCOMPSurface : public GLImage {
 public:
  GLImageDCOMPSurface(const gfx::Size& size, HANDLE surface_handle);

  // Safe downcast. Returns nullptr on failure.
  static GLImageDCOMPSurface* FromGLImage(GLImage* image);

  // GLImage implementation.
  gfx::Size GetSize() override;
  unsigned GetDataType() override;
  unsigned GetInternalFormat() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override;
  void Flush() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  Type GetType() const override;

  Microsoft::WRL::ComPtr<IDCompositionSurface> CreateSurfaceForDevice(
      IDCompositionDevice2* device);
  void SetSize(gfx::Size size);
  const gfx::ColorSpace& color_space() const { return color_space_; }
  HANDLE GetSurfaceHandle() { return surface_handle_.Get(); }
  virtual void SetParentWindow(HWND parent);
  HWND GetParentWindow() { return last_parent_; }
  virtual void SetRect(const gfx::Rect& window_relative_rect) {}
  virtual bool IsContextValid() const;

 protected:
  ~GLImageDCOMPSurface() override;

  gfx::Size size_;
  gfx::ColorSpace color_space_;
  base::win::ScopedHandle surface_handle_;
  HWND last_parent_ = NULL;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_DCOMP_SURFACE_H_
