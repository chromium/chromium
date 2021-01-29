// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandConnection;
class WaylandSubsurface;
class WaylandWindow;
class WaylandSurface;

// This is an internal helper representation of a wayland buffer object, which
// the GPU process creates when CreateBuffer is called. It's used for
// asynchronous buffer creation and stores |params| parameter to find out,
// which Buffer the wl_buffer corresponds to when CreateSucceeded is called.
// What is more, the Buffer stores such information as a widget it is attached
// to, its buffer id for simpler buffer management and other members specific
// to this Buffer object on run-time.
struct WaylandBuffer {
  WaylandBuffer() = delete;
  WaylandBuffer(const gfx::Size& size, uint32_t buffer_id);
  ~WaylandBuffer();

  // Actual buffer size.
  const gfx::Size size;

  // Damage region this buffer describes. Must be emptied once buffer is
  // submitted.
  gfx::Rect damage_region;

  // The id of this buffer.
  const uint32_t buffer_id;

  // A wl_buffer backed by a dmabuf created on the GPU side.
  wl::Object<struct wl_buffer> wl_buffer;

  // Tells if the buffer has the wl_buffer attached. This can be used to
  // identify potential problems, when the Wayland compositor fails to create
  // wl_buffers.
  bool attached = false;

  // Tells if the buffer has already been released aka not busy, and the
  // surface can tell the gpu about successful swap.
  bool released = true;

  DISALLOW_COPY_AND_ASSIGN(WaylandBuffer);
};

// This is the buffer manager which creates wl_buffers based on dmabuf (hw
// accelerated compositing) or shared memory (software compositing) and uses
// internal representation of surfaces, which are used to store buffers
// associated with the WaylandWindow.
class WaylandBufferManagerHost : public ozone::mojom::WaylandBufferManagerHost,
                                 public WaylandWindowObserver {
 public:
  explicit WaylandBufferManagerHost(WaylandConnection* connection);
  ~WaylandBufferManagerHost() override;

  // WaylandWindowObserver implements:
  void OnWindowAdded(WaylandWindow* window) override;
  void OnWindowRemoved(WaylandWindow* window) override;
  void OnWindowConfigured(WaylandWindow* window) override;
  void OnSubsurfaceAdded(WaylandWindow* window,
                         WaylandSubsurface* subsurface) override;
  void OnSubsurfaceRemoved(WaylandWindow* window,
                           WaylandSubsurface* subsurface) override;

  // Start allowing attaching buffers to |surface|, same as
  // OnWindowConfigured(), but for WaylandSurface.
  void SetSurfaceConfigured(WaylandSurface* surface);
  void SetTerminateGpuCallback(
      base::OnceCallback<void(std::string)> terminate_gpu_cb);

  // Returns bound pointer to own mojo interface. If there were previous
  // interface bindings, it will be unbound and the state of the
  // |buffer_manager_| will be cleared.
  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> BindInterface();

  // Unbinds the interface and clears the state of the |buffer_manager_|. Used
  // only when the GPU channel, which uses the mojo pipe to this interface, is
  // destroyed.
  void OnChannelDestroyed();

  // Returns supported buffer formats either from zwp_linux_dmabuf or wl_drm.
  wl::BufferFormatsWithModifiersMap GetSupportedBufferFormats() const;

  bool SupportsDmabuf() const;
  bool SupportsAcquireFence() const;

  // ozone::mojom::WaylandBufferManagerHost overrides:
  //
  // These overridden methods below are invoked by the GPU when hardware
  // accelerated rendering is used.
  void SetWaylandBufferManagerGpu(
      mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
          buffer_manager_gpu_associated) override;
  //
  // Called by the GPU and asks to import a wl_buffer based on a gbm file
  // descriptor using zwp_linux_dmabuf protocol. Check comments in the
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  void CreateDmabufBasedBuffer(mojo::PlatformHandle dmabuf_fd,
                               const gfx::Size& size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t format,
                               uint32_t planes_count,
                               uint32_t buffer_id) override;
  // Called by the GPU and asks to import a wl_buffer based on a shared memory
  // file descriptor using wl_shm protocol. Check comments in the
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  void CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                            uint64_t length,
                            const gfx::Size& size,
                            uint32_t buffer_id) override;
  // Called by the GPU to destroy the imported wl_buffer with a |buffer_id|.
  void DestroyBuffer(gfx::AcceleratedWidget widget,
                     uint32_t buffer_id) override;
  // Called by the GPU and asks to configure the surface/subsurfaces and attach
  // wl_buffers to WaylandWindow with the specified |widget|. Calls OnSubmission
  // and OnPresentation on successful swap and pixels presented.
  void CommitOverlays(
      gfx::AcceleratedWidget widget,
      std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlays) override;

  // Called by WaylandWindow to start recording a frame. This helps record the
  // number of subsurface commits needed to finish for this frame before
  // |root_surface| can be committed.
  // This pairs with an EndCommitFrame(). Every CommitBufferInternal() in
  // between increases the number of needed pending commits by 1.
  void StartFrame(WaylandSurface* root_surface);
  void EndFrame(uint32_t buffer_id = 0u,
                const gfx::Rect& damage_region = gfx::Rect());

  // Called by the WaylandWindow and asks to attach a wl_buffer with a
  // |buffer_id| to a WaylandSurface.
  // Calls OnSubmission and OnPresentation on successful swap and pixels
  // presented.
  // |wait_for_frame_callback| instructs that a surface should wait for previous
  // wl_frame_callback. This is primarily used for sync wl_subsurfaces case
  // where buffer updates within a frame should be seen together. A root_surface
  // commit will move an entire wl_surface tree from pending state to ready
  // state. This root_surface commit must wait for wl_frame_callback, such that
  // in effect all other surface updates wait for this wl_frame_callback, too.
  // |access_fence_handle| specifies a gpu fence created by the gpu process.
  // It's to be waited on before content of the buffer is ready to be read by
  // Wayland host.
  bool CommitBufferInternal(
      WaylandSurface* wayland_surface,
      uint32_t buffer_id,
      const gfx::Rect& damage_region,
      bool wait_for_frame_callback = true,
      bool commit_synced_subsurface = false,
      gfx::GpuFenceHandle access_fence_handle = gfx::GpuFenceHandle());

  // When a surface is hidden, the client may want to detach the buffer attached
  // to the surface to ensure Wayland does not present those contents and do not
  // composite in a wrong way. Otherwise, users may see the contents of a hidden
  // surface on their screens.
  void ResetSurfaceContents(WaylandSurface* wayland_surface);

  // Returns the anonymously created WaylandBuffer.
  std::unique_ptr<WaylandBuffer> PassAnonymousWlBuffer(uint32_t buffer_id);

 private:
  // This is an internal representation of a real surface, which holds a pointer
  // to WaylandSurface. Also, this object holds buffers, frame callbacks and
  // presentation callbacks for that surface.
  class Surface;

  // This represents a frame that consists of state changes to multiple
  // synchronized wl_surfaces that are in the same hierarchy. It defers
  // committing the root surface until all child surfaces' states are ready.
  struct Frame;

  bool CreateBuffer(const gfx::Size& size, uint32_t buffer_id);

  Surface* GetSurface(WaylandSurface* wayland_surface) const;

  void RemovePendingFrames(WaylandSurface* root_surface, uint32_t buffer_id);

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const base::ScopedFD& file,
                           const gfx::Size& size,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t format,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateBufferIdFromGpu(uint32_t buffer_id);
  bool ValidateDataFromGpu(const base::ScopedFD& file,
                           size_t length,
                           const gfx::Size& size,
                           uint32_t buffer_id);

  // Callback method. Receives a result for the request to create a wl_buffer
  // backend by dmabuf file descriptor from ::CreateBuffer call.
  void OnCreateBufferComplete(uint32_t buffer_id,
                              wl::Object<struct wl_buffer> new_buffer);

  // Tells the |buffer_manager_gpu_ptr_| the result of a swap call and provides
  // it with the presentation feedback.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::SwapResult& swap_result);
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback);

  // Terminates the GPU process on invalid data received
  void TerminateGpuProcess();

  bool DestroyAnonymousBuffer(uint32_t buffer_id);

  base::flat_map<WaylandSurface*, std::unique_ptr<Surface>> surfaces_;

  // When StartCommitFrame() is called, a Frame is pushed to
  // |pending_frames_|. See StartCommitFrame().
  std::vector<std::unique_ptr<Frame>> pending_frames_;

  // When a WaylandWindow/WaylandSubsurface is removed, its corresponding
  // Surface may still have an un-released buffer and un-acked presentation.
  // Thus, we keep removed surfaces in the graveyard. It's safe to delete them
  // when all of the Surface's buffers are destroyed because buffer destruction
  // is deferred till after buffers are released and presentations are acked.
  std::list<std::unique_ptr<Surface>> surface_graveyard_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;

  mojo::AssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      buffer_manager_gpu_associated_;
  mojo::Receiver<ozone::mojom::WaylandBufferManagerHost> receiver_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;

  // Contains anonymous buffers aka buffers that are not attached to any of the
  // existing surfaces and that will be mapped to surfaces later.
  // Typically created when CreateAnonymousImage is called on the gpu process
  // side.
  // We assume that a buffer_id/wl_buffer will never be used on multiple
  // wl_surfaces so we never re-map buffers to surfaces. If we ever need to use
  // the same buffer for 2 surfaces at the same time, create multiple wl_buffers
  // referencing the same dmabuf or underlying storage.
  base::flat_map<uint32_t, std::unique_ptr<WaylandBuffer>> anonymous_buffers_;

  base::WeakPtrFactory<WaylandBufferManagerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
