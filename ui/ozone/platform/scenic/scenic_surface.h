// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

class ScenicSurfaceFactory;

// Holder for Scenic resources backing rendering surface.
//
// This object creates some simple Scenic resources for containing a window's
// texture, and attaches them to the parent View (by sending an IPC to the
// browser process).
//
// The texture is updated through an image pipe.
class ScenicSurface : public ui::PlatformWindowSurface {
 public:
  ScenicSurface(
      ScenicSurfaceFactory* scenic_surface_factory,
      gfx::AcceleratedWidget window,
      scenic::SessionPtrAndListenerRequest sesion_and_listener_request);
  ~ScenicSurface() override;

  // Sets the texture of the surface to a new image pipe.
  bool SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request)
      override;

  // Sets the texture of the surface to an image resource.
  void SetTextureToImage(const scenic::Image& image);

  // Presents a ViewHolder that is corresponding to the overlay content coming
  // from BufferCollection specified by |id|.
  bool PresentOverlayView(
      gfx::SysmemBufferCollectionId id,
      fuchsia::ui::views::ViewHolderToken view_holder_token);

  // Updates positioning of ViewHolder specified by |id|. Note that it requires
  // |id| to be added by PresentOverlayView() before.
  bool UpdateOverlayViewPosition(gfx::SysmemBufferCollectionId id,
                                 int plane_z_order,
                                 const gfx::Rect& display_bounds,
                                 const gfx::RectF& crop_rect,
                                 gfx::OverlayTransform plane_transform,
                                 std::vector<zx::event> acquire_fences);

  // Remove ViewHolder specified by |id|.
  bool RemoveOverlayView(gfx::SysmemBufferCollectionId id);

  // Creates a View for this surface, and returns a ViewHolderToken handle
  // that can be used to attach it into a scene graph.
  mojo::PlatformHandle CreateView();

  void OnScenicEvents(std::vector<fuchsia::ui::scenic::Event> events);

  void AssertBelongsToCurrentThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  scenic::Session* scenic_session() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return &scenic_session_;
  }

 private:
  void UpdateViewHolderScene();

  scenic::Session scenic_session_;
  std::unique_ptr<scenic::View> parent_;

  // Scenic resources used for the primary plane, that is not an overlay.
  scenic::ShapeNode main_shape_;
  scenic::Material main_material_;
  gfx::SizeF main_shape_size_;

  ScenicSurfaceFactory* const scenic_surface_factory_;
  const gfx::AcceleratedWidget window_;

  struct OverlayViewInfo {
    OverlayViewInfo(scenic::ViewHolder holder, scenic::EntityNode node);

    scenic::ViewHolder view_holder;
    scenic::EntityNode entity_node;
    int plane_z_order = 0;
    gfx::Rect display_bounds;
    gfx::RectF crop_rect;
    gfx::OverlayTransform plane_transform;
  };
  std::unordered_map<gfx::SysmemBufferCollectionId,
                     OverlayViewInfo,
                     base::UnguessableTokenHash>
      overlays_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ScenicSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
