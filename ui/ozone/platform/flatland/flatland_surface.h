// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_

#include <fuchsia/ui/composition/cpp/fidl.h>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/flatland/flatland_connection.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

class FlatlandSurfaceFactory;

// Holder for Flatland resources backing rendering surface.
//
// This object creates some simple Flatland resources for containing a window's
// texture, and attaches them to the parent View (by sending an IPC to the
// browser process).
//
// The texture is updated through an image pipe.
class FlatlandSurface : public ui::PlatformWindowSurface {
 public:
  FlatlandSurface(FlatlandSurfaceFactory* flatland_surface_factory,
                  gfx::AcceleratedWidget window);
  ~FlatlandSurface() override;
  FlatlandSurface(const FlatlandSurface&) = delete;
  FlatlandSurface& operator=(const FlatlandSurface&) = delete;

  // Sets the texture of the surface to a new image pipe.
  bool SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request)
      override;

  // Creates a View for this surface, and returns a ViewHolderToken handle
  // that can be used to attach it into a scene graph.
  mojo::PlatformHandle CreateView();

  void AssertBelongsToCurrentThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  FlatlandConnection* flatland_connection() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return &flatland_;
  }

 private:
  void OnGetLayout(fuchsia::ui::composition::LayoutInfo info);

  FlatlandConnection flatland_;

  // Flatland resources used for the primary plane, that is not an overlay.
  uint64_t next_id_ = 1;
  fuchsia::ui::composition::TransformId root_transform_id_;
  fuchsia::ui::composition::TransformId main_plane_transform_id_;

  // TODO(crbug.com/1230150): Add link primitives.
  fuchsia::ui::composition::LayoutInfo layout_info_;

  FlatlandSurfaceFactory* const flatland_surface_factory_;
  const gfx::AcceleratedWidget window_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SURFACE_H_
