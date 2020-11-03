// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
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
class Rect;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandSurfaceGpu;
class WaylandWindow;
struct OverlayPlane;

// Forwards calls through an associated mojo connection to WaylandBufferManager
// on the browser process side.
//
// It's guaranteed that WaylandBufferManagerGpu makes mojo calls on the right
// sequence.
class WaylandBufferManagerGpu : public ozone::mojom::WaylandBufferManagerGpu {
 public:
  WaylandBufferManagerGpu();
  ~WaylandBufferManagerGpu() override;

  // WaylandBufferManagerGpu overrides:
  void Initialize(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
      const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
          buffer_formats_with_modifiers,
      bool supports_dma_buf) override;

  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new buffer for swap.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    gfx::SwapResult swap_result) override;
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
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
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

  // Adds a WaylandBufferManagerGpu binding.
  void AddBindingWaylandBufferManagerGpu(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver);

  // Returns supported modifiers for the supplied |buffer_format|.
  const std::vector<uint64_t>& GetModifiersForBufferFormat(
      gfx::BufferFormat buffer_format) const;

  // Allocates a unique buffer ID.
  uint32_t AllocateBufferID();

 private:
  void CreateDmabufBasedBufferInternal(base::ScopedFD dmabuf_fd,
                                       gfx::Size size,
                                       const std::vector<uint32_t>& strides,
                                       const std::vector<uint32_t>& offsets,
                                       const std::vector<uint64_t>& modifiers,
                                       uint32_t current_format,
                                       uint32_t planes_count,
                                       uint32_t buffer_id);
  void CreateShmBasedBufferInternal(base::ScopedFD shm_fd,
                                    size_t length,
                                    gfx::Size size,
                                    uint32_t buffer_id);
  void CommitBufferInternal(gfx::AcceleratedWidget widget,
                            uint32_t buffer_id,
                            const gfx::Rect& damage_region);
  void CommitOverlaysInternal(
      gfx::AcceleratedWidget widget,
      std::vector<ozone::mojom::WaylandOverlayConfigPtr> overlays);
  void DestroyBufferInternal(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  void BindHostInterface(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host);

  void SaveTaskRunnerForWidgetOnIOThread(
      gfx::AcceleratedWidget widget,
      scoped_refptr<base::SingleThreadTaskRunner> origin_runner);
  void ForgetTaskRunnerForWidgetOnIOThread(gfx::AcceleratedWidget widget);

  // Provides the WaylandSurfaceGpu, which backs the |widget|, with swap and
  // presentation results.
  void SubmitSwapResultOnOriginThread(gfx::AcceleratedWidget widget,
                                      uint32_t buffer_id,
                                      gfx::SwapResult swap_result);
  void SubmitPresentationOnOriginThread(
      gfx::AcceleratedWidget widget,
      uint32_t buffer_id,
      const gfx::PresentationFeedback& feedback);

#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif

  mojo::Receiver<ozone::mojom::WaylandBufferManagerGpu> receiver_{this};

  // A pointer to a WaylandBufferManagerHost object, which always lives on a
  // browser process side. It's used for a multi-process mode.
  mojo::Remote<ozone::mojom::WaylandBufferManagerHost> remote_host_;

  mojo::AssociatedReceiver<ozone::mojom::WaylandBufferManagerGpu>
      associated_receiver_{this};

  std::map<gfx::AcceleratedWidget, WaylandSurfaceGpu*>
      widget_to_surface_map_;  // Guarded by |lock_|.

  // Supported buffer formats and modifiers sent by the Wayland compositor to
  // the client. Corresponds to the map stored in WaylandZwpLinuxDmabuf and
  // passed from it during initialization of this gpu host.
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
      supported_buffer_formats_with_modifiers_;

  // These task runners can be used to pass messages back to the same thread,
  // where the commit buffer request came from. For example, swap requests can
  // come from the GpuMainThread, but are rerouted to the IOChildThread and then
  // mojo calls happen. However, when the manager receives mojo calls, it has to
  // reroute calls back to the same thread where the calls came from to ensure
  // correct sequence. Note that not all calls come from the GpuMainThread, e.g.
  // WaylandCanvasSurface calls from the VizCompositorThread.
  // This map must only be accessed from the IO thread.
  base::small_map<std::map<gfx::AcceleratedWidget,
                           scoped_refptr<base::SingleThreadTaskRunner>>>
      commit_thread_runners_;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on IOChildThread. This is
  // needed to ensure mojo calls happen on a right sequence.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;

  // Protects access to |widget_to_surface_map_| and |commit_thread_runners_|.
  base::Lock lock_;

  // Keeps track of the next unique buffer ID.
  uint32_t next_buffer_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerGpu);
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
