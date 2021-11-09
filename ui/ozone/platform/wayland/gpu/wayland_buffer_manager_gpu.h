// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

#if defined(WAYLAND_GBM)
#include "ui/gfx/linux/gbm_device.h"  // nogncheck
#endif

namespace gfx {
enum class SwapResult;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandSurfaceGpu;
class WaylandWindow;
struct OverlayPlane;

// Forwards calls through an associated mojo connection to WaylandBufferManager
// on the browser process side. All calls to this object must be done on the
// gpu main thread.
class WaylandBufferManagerGpu : public ozone::mojom::WaylandBufferManagerGpu {
 public:
  WaylandBufferManagerGpu();

  WaylandBufferManagerGpu(const WaylandBufferManagerGpu&) = delete;
  WaylandBufferManagerGpu& operator=(const WaylandBufferManagerGpu&) = delete;

  ~WaylandBufferManagerGpu() override;

  // WaylandBufferManagerGpu overrides:
  void Initialize(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
      const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
          buffer_formats_with_modifiers,
      bool supports_dma_buf,
      bool supports_viewporter,
      bool supports_acquire_fence,
      bool supports_non_backed_solid_color_buffers) override;

  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new buffer for swap.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    gfx::SwapResult swap_result,
                    gfx::GpuFenceHandle release_fence_handle) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  // If the client, which uses this manager and implements WaylandSurfaceGpu,
  // wants to receive OnSubmission and OnPresentation callbacks and know the
  // result of the below operations, they must register themselves with the
  // below APIs.
  void RegisterSurface(gfx::AcceleratedWidget widget,
                       WaylandSurfaceGpu* surface);
  void UnregisterSurface(gfx::AcceleratedWidget widget);
  WaylandSurfaceGpu* GetSurface(gfx::AcceleratedWidget widget);

  // Methods, which can be used when in both in-process-gpu and out of process
  // modes. These calls are forwarded to the browser process through the
  // WaylandConnection mojo interface. See more in
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  //
  // Asks Wayland to create generic dmabuf-based wl_buffer.
  void CreateDmabufBasedBuffer(base::ScopedFD dmabuf_fd,
                               gfx::Size size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t current_format,
                               uint32_t planes_count,
                               uint32_t buffer_id);

  // Asks Wayland to create a shared memory based wl_buffer.
  void CreateShmBasedBuffer(base::ScopedFD shm_fd,
                            size_t length,
                            gfx::Size size,
                            uint32_t buffer_id);

  // Asks Wayland to create a solid color wl_buffer that is not backed by
  // anything on the gpu side. Requires surface-augmenter protocol.
  void CreateSolidColorBuffer(SkColor color,
                              const gfx::Size& size,
                              uint32_t buf_id);

  // Asks Wayland to find a wl_buffer with the |buffer_id| and attach the
  // buffer to the WaylandWindow's surface, which backs the following |widget|.
  // Once the buffer is submitted and presented, the OnSubmission and
  // OnPresentation are called. Note, it's not guaranteed the OnPresentation
  // will follow the OnSubmission immediately, but the OnPresentation must never
  // be called before the OnSubmission is called for that particular buffer.
  // This logic must be checked by the client, though the host ensures this
  // logic as well. This call must not be done twice for the same |widget| until
  // the OnSubmission is called (which actually means the client can continue
  // sending buffer swap requests).
  //
  // CommitBuffer() calls CommitOverlays() to commit only a primary plane
  // buffer.
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& bounds_rect,
                    float surface_scale_factor,
                    const gfx::Rect& damage_region);
  // Send overlay configurations for a frame to a WaylandWindow identified by
  // |widget|.
  void CommitOverlays(
      gfx::AcceleratedWidget widget,
      std::vector<ozone::mojom::WaylandOverlayConfigPtr> overlays);

  // Asks Wayland to destroy a wl_buffer.
  void DestroyBuffer(gfx::AcceleratedWidget widget, uint32_t buffer_id);

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* gbm_device() const { return gbm_device_.get(); }
  void set_gbm_device(std::unique_ptr<GbmDevice> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }
#endif

  bool supports_acquire_fence() const { return supports_acquire_fence_; }
  bool supports_viewporter() const { return supports_viewporter_; }
  bool supports_non_backed_solid_color_buffers() const {
    return supports_non_backed_solid_color_buffers_;
  }

  // Adds a WaylandBufferManagerGpu binding.
  void AddBindingWaylandBufferManagerGpu(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver);

  // Returns supported modifiers for the supplied |buffer_format|.
  const std::vector<uint64_t>& GetModifiersForBufferFormat(
      gfx::BufferFormat buffer_format) const;

  // Atomically allocates next buffer id. It is guaranteed that the next buffer
  // id is unique.
  uint32_t AllocateBufferID();

 private:
  void BindHostInterface(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host);

  // Called when |receiver| is disconnected.
  void OnReceiverDisconnected();
  // Called when |remote_host| is disconnected.
  void OnHostDisconnected();

#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif
  // Whether Wayland server allows buffer submission with acquire fence.
  bool supports_acquire_fence_ = false;

  // Whether Wayland server implements wp_viewporter extension to support
  // cropping and scaling buffers.
  bool supports_viewporter_ = false;

  // Determines whether solid color overlays can be delegated without a backing
  // image via a wayland protocol.
  bool supports_non_backed_solid_color_buffers_ = false;

  mojo::Receiver<ozone::mojom::WaylandBufferManagerGpu> receiver_{this};

  // A pointer to a WaylandBufferManagerHost object, which always lives on a
  // browser process side. It's used for a multi-process mode.
  mojo::Remote<ozone::mojom::WaylandBufferManagerHost> remote_host_;

  mojo::AssociatedReceiver<ozone::mojom::WaylandBufferManagerGpu>
      associated_receiver_{this};

  std::map<gfx::AcceleratedWidget, WaylandSurfaceGpu*> widget_to_surface_map_;

  // Supported buffer formats and modifiers sent by the Wayland compositor to
  // the client. Corresponds to the map stored in WaylandZwpLinuxDmabuf and
  // passed from it during initialization of this gpu host.
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
      supported_buffer_formats_with_modifiers_;

  // Keeps track of the next unique buffer ID. Made atomic to guarantee race
  // free increases.
  std::atomic<uint32_t> next_buffer_id_{0};

  // All calls must happen on the correct sequence. See comments in the
  // constructor and in the OnHostDisconnected for more details.
  SEQUENCE_CHECKER(gpu_sequence_checker_);
};

}  // namespace ui

// This is a specialization of mojo::TypeConverter and has to be in the mojo
// namespace.
namespace mojo {
template <>
struct TypeConverter<ui::ozone::mojom::WaylandOverlayConfigPtr,
                     ui::OverlayPlane> {
  static ui::ozone::mojom::WaylandOverlayConfigPtr Convert(
      const ui::OverlayPlane& input);
};
}  // namespace mojo

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
