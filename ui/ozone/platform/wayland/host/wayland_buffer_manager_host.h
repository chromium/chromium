// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/version.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandBufferBacking;
class WaylandBufferHandle;
class WaylandConnection;
class WaylandWindow;
class WaylandSurface;

// This is the buffer manager which creates wl_buffers based on dmabuf (hw
// accelerated compositing) or shared memory (software compositing) and uses
// internal representation of surfaces, which are used to store buffers
// associated with the WaylandWindow.
class WaylandBufferManagerHost : public ozone::mojom::WaylandBufferManagerHost {
 public:
  explicit WaylandBufferManagerHost(WaylandConnection* connection);

  WaylandBufferManagerHost(const WaylandBufferManagerHost&) = delete;
  WaylandBufferManagerHost& operator=(const WaylandBufferManagerHost&) = delete;

  ~WaylandBufferManagerHost() override;

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

  // Called by WaylandFrameManager if overlay data is invalid.
  void OnCommitOverlayError(const std::string& message);

  // Returns supported buffer formats either from zwp_linux_dmabuf or wl_drm.
  wl::BufferFormatsWithModifiersMap GetSupportedBufferFormats() const;

  base::Version GetServerVersion() const;
  bool SupportsDmabuf() const;
  bool SupportsAcquireFence() const;
  bool SupportsViewporter() const;
  bool SupportsOverlays() const;
  bool SupportsNonBackedSolidColorBuffers() const;
  bool SupportsSinglePixelBuffer() const;
  uint32_t GetSurfaceAugmentorVersion() const;

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
  // ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.
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
  // ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.
  void CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                            uint64_t length,
                            const gfx::Size& size,
                            uint32_t buffer_id) override;
  // Called by the GPU and asks to create a solid color wl_buffer. Check
  // comments in the
  // ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom. The
  // availability of this depends on existence of surface-augmenter protocol.
  void CreateSolidColorBuffer(const gfx::Size& size,
                              const SkColor4f& color,
                              uint32_t buffer_id) override;
  // Called by the GPU and asks to create a single pixel wl_buffer. Check
  // comments in the
  // ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom. The
  // availability of this depends on existence of single pixel buffer protocol.
  void CreateSinglePixelBuffer(const SkColor4f& color,
                               uint32_t buffer_id) override;

  // Called by the GPU to destroy the imported wl_buffer with a |buffer_id|.
  void DestroyBuffer(uint32_t buffer_id) override;
  // Called by the GPU and asks to configure the surface/subsurfaces and attach
  // wl_buffers to WaylandWindow with the specified |widget|. Calls OnSubmission
  // and OnPresentation on successful swap and pixels presented.
  void CommitOverlays(gfx::AcceleratedWidget widget,
                      uint32_t frame_id,
                      const gfx::FrameData& data,
                      std::vector<wl::WaylandOverlayConfig> overlays) override;

  // Ensures a WaylandBufferHandle of |buffer_id| is created for the
  // |requestor|, with its wl_buffer object requested via Wayland. Returns said
  // buffer handle.
  WaylandBufferHandle* EnsureBufferHandle(WaylandSurface* requestor,
                                          uint32_t buffer_id);

  // Gets the WaylandBufferHandle of |buffer_id| used for |requestor|.
  WaylandBufferHandle* GetBufferHandle(WaylandSurface* requestor,
                                       uint32_t buffer_id);

  // Gets the buffer format of |buffer_id| used for |requestor| if it is a
  // DMA based buffer.
  uint32_t GetBufferFormat(WaylandSurface* requestor, uint32_t buffer_id);

  // Tells the |buffer_manager_gpu_ptr_| the result of a swap call and provides
  // it with the presentation feedback.
  void OnSubmission(
      gfx::AcceleratedWidget widget,
      uint32_t frame_id,
      const gfx::SwapResult& swap_result,
      gfx::GpuFenceHandle release_fence,
      const std::vector<wl::WaylandPresentationInfo>& presentation_infos);
  void OnPresentation(
      gfx::AcceleratedWidget widget,
      const std::vector<wl::WaylandPresentationInfo>& presentation_infos);

  // Inserts a sync_file into the write fence list of the DMA-BUF. When the
  // compositor tries to read from this DMA-BUF via GL, the kernel will
  // automatically force its GPU context to wait on all write fences in the
  // DMA-BUF, including the fence we inserted. This is used to synchronize with
  // compositors that don't support the
  // linux-explicit-synchronization-unstable-v1 protocol. Requires Linux 6.0 or
  // higher.
  void InsertAcquireFence(uint32_t buffer_id, int sync_fd);

  // Extracts a sync_file that represents all pending fences inside the DMA-BUF
  // kernel object. When the compositor reads the DMA-BUF from GL, the kernel
  // automatically adds a completion fence to the read fences list of the
  // DMA-BUF that will be signalled once the read operation completes. This is
  // used to synchronize with compositors that don't support the
  // linux-explicit-synchronization-unstable-v1 protocol. Requires Linux 6.0 or
  // higher.
  base::ScopedFD ExtractReleaseFence(uint32_t buffer_id);

  static bool SupportsImplicitSyncInterop();

 private:
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
  bool ValidateDataFromGpu(const gfx::Size& size, uint32_t buffer_id);
  bool ValidateBufferExistence(uint32_t buffer_id);

  // Terminates the GPU process on invalid data received
  void TerminateGpuProcess();

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  const raw_ptr<WaylandConnection> connection_;

  mojo::AssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      buffer_manager_gpu_associated_;
  mojo::Receiver<ozone::mojom::WaylandBufferManagerHost> receiver_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;

  // Maps buffer_id's to corresponding WaylandBufferBacking objects.
  base::flat_map<uint32_t, std::unique_ptr<WaylandBufferBacking>>
      buffer_backings_;

  base::flat_map<uint32_t, base::ScopedFD> dma_buffers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
