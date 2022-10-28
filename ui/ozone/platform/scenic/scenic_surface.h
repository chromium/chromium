// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/scenic/safe_presenter.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

class ScenicSurfaceFactory;
class SysmemBufferManager;

// Holder for Scenic resources backing rendering surface.
//
// This object creates some simple Scenic resources for containing a window's
// texture, and attaches them to the parent View (by sending an IPC to the
// browser process).
//
// The texture is updated through an image pipe.
class ScenicSurface : public PlatformWindowSurface {
 public:
  ScenicSurface(
      ScenicSurfaceFactory* scenic_surface_factory,
      SysmemBufferManager* sysmem_buffer_manager,
      gfx::AcceleratedWidget window,
      scenic::SessionPtrAndListenerRequest sesion_and_listener_request);

  ScenicSurface(const ScenicSurface&) = delete;
  ScenicSurface& operator=(const ScenicSurface&) = delete;

  ~ScenicSurface() override;

  // PlatformWindowSurface overrides.
  void Present(scoped_refptr<gfx::NativePixmap> primary_plane_pixmap,
               std::vector<ui::OverlayPlane> overlays,
               std::vector<gfx::GpuFenceHandle> acquire_fences,
               std::vector<gfx::GpuFenceHandle> release_fences,
               SwapCompletionCallback completion_callback,
               BufferPresentedCallback presentation_callback) override;

  // Allocates a new NativePixmap for the primary plane. The first time is
  // invoked |image_pipe_| will be initialized.
  scoped_refptr<gfx::NativePixmap> AllocatePrimaryPlanePixmap(
      VkDevice vk_device,
      const gfx::Size& size,
      gfx::BufferFormat buffer_format);

  // Sets the texture of the surface to a new image pipe.
  void SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe2> image_pipe_request);

  // Sets the texture of the surface to an image resource.
  void SetTextureToImage(const scenic::Image& image);

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

  SafePresenter* safe_presenter() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return &safe_presenter_;
  }

 private:
  struct PresentationState {
    int presented_frame_ordinal;
    base::TimeTicks presentation_time;
    base::TimeDelta interval;
  };

  struct PresentedFrame {
    PresentedFrame(uint32_t ordinal,
                   uint32_t image_id,
                   scoped_refptr<gfx::NativePixmap> primary_plane,
                   SwapCompletionCallback completion_callback,
                   BufferPresentedCallback presentation_callback);
    PresentedFrame(PresentedFrame&& other);
    PresentedFrame& operator=(PresentedFrame&& other);
    ~PresentedFrame();

    uint32_t ordinal;
    uint32_t image_id;

    // Ensures the pixmap is not destroyed until after frame is presented.
    scoped_refptr<gfx::NativePixmap> primary_plane;

    SwapCompletionCallback completion_callback;
    BufferPresentedCallback presentation_callback;
  };

  void InitializeImagePipe();

  // Removes a buffer collection registered with |image_pipe_| when it's no
  // longer needed.
  void RemoveBufferCollection(zx_koid_t buffer_collection_id);
  void OnPresentComplete(fuchsia::images::PresentationInfo presentation_info);
  void UpdateViewHolderScene();

  void PresentEmptyImage();

  scenic::Session scenic_session_;
  std::unique_ptr<scenic::View> parent_;

  // Used for safely queueing Present() operations on |scenic_session_|.
  SafePresenter safe_presenter_;

  fuchsia::images::ImagePipe2Ptr image_pipe_;

  // Unique ids to be used as both image and uint32_t buffer collection ids.
  uint32_t next_unique_id_ = 0;

  // Mapping between the SysmemBufferCollectionId stored in NativePixmapHandles
  // and uint32_t id registered with image pipe.
  base::flat_map<zx_koid_t, uint32_t> buffer_collection_to_image_id_;

  // Ordinal that will be assigned to the next frame. Ordinals are used to
  // calculate frame position relative to the current frame stored in
  // |presentation_state_|. They will wrap around when reaching 2^32, but the
  // math used to calculate relative position will still work as expected.
  uint32_t next_frame_ordinal_ = 0;

  // Presentation information received from ImagePipe after rendering a frame.
  // Used to calculate target presentation time for the frames presented in the
  // future.
  absl::optional<PresentationState> presentation_state_;
  base::TimeTicks last_frame_present_time_;

  base::circular_deque<PresentedFrame> pending_frames_;

  std::vector<zx::event> release_fences_from_last_present_;

  // Scenic resources used for the primary plane, that is not an overlay.
  scenic::ShapeNode main_shape_;
  scenic::Material main_material_;
  gfx::SizeF main_shape_size_;

  ScenicSurfaceFactory* const scenic_surface_factory_;
  SysmemBufferManager* const sysmem_buffer_manager_;
  const gfx::AcceleratedWidget window_;

  struct OverlayViewInfo {
    OverlayViewInfo(scenic::Session* scenic_session,
                    fuchsia::ui::views::ViewHolderToken view_holder_token);

    scenic::ViewHolder view_holder;
    scenic::EntityNode entity_node;

    int plane_z_order = 0;
    gfx::Rect display_bounds;
    gfx::RectF crop_rect;
    gfx::OverlayTransform plane_transform;

    // Used only in `Present()` in order to update `visible`.
    bool should_be_visible = false;
  };

  // Current set of overlays. Identified by koid of the buffer collection
  // handle.
  std::unordered_map<zx_koid_t, OverlayViewInfo> overlay_views_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ScenicSurface> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
