// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_

#include <cstdint>
#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.h"
#include "ui/ozone/public/drm_modifiers_filter.h"

namespace gfx {
enum class SwapResult;
}  // namespace gfx

namespace ui {

class GbmDevice;
class WaylandConnection;
class WaylandSurfaceGpu;
class WaylandWindow;

// Forwards calls through an associated mojo connection to WaylandBufferManager
// on the browser process side.
//
// It's guaranteed that WaylandBufferManagerGpu makes mojo calls on the right
// sequence.
class WaylandBufferManagerGpu : public ozone::mojom::WaylandBufferManagerGpu {
 public:
  WaylandBufferManagerGpu();
  explicit WaylandBufferManagerGpu(const base::FilePath& drm_node_path);
  WaylandBufferManagerGpu(const WaylandBufferManagerGpu&) = delete;
  WaylandBufferManagerGpu& operator=(const WaylandBufferManagerGpu&) = delete;

  ~WaylandBufferManagerGpu() override;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_runner() const {
    return gpu_thread_runner_;
  }

  // WaylandBufferManagerGpu overrides:
  void Initialize(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
      const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
          buffer_formats_with_modifiers,
      bool supports_dma_buf,
      bool supports_viewporter,
      bool supports_acquire_fence,
      bool supports_overlays,
      uint32_t supported_surface_augmentor_version,
      bool supports_single_pixel_buffer,
      const base::Version& server_version) override;

  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new frame for swap.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t frame_id,
                    gfx::SwapResult swap_result,
                    gfx::GpuFenceHandle release_fence_handle,
                    const std::vector<wl::WaylandPresentationInfo>&
                        presentation_infos) override;

  void OnPresentation(gfx::AcceleratedWidget widget,
                      const std::vector<wl::WaylandPresentationInfo>&
                          presentation_infos) override;

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
  // ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.
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
  void CreateSolidColorBuffer(SkColor4f color,
                              const gfx::Size& size,
                              uint32_t buf_id);

  // Asks Wayland to create a single pixel wl_buffer that is not backed by
  // anything on the gpu side. Requires single pixel buffer protocol.
  void CreateSinglePixelBuffer(SkColor4f color, uint32_t buf_id);

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
                    uint32_t frame_id,
                    uint32_t buffer_id,
                    gfx::FrameData data,
                    const gfx::Rect& bounds_rect,
                    bool enable_blend,
                    const gfx::RoundedCornersF& corners,
                    float surface_scale_factor,
                    const gfx::Rect& damage_region);
  // Send overlay configurations for a frame to a WaylandWindow identified by
  // |widget|.
  void CommitOverlays(gfx::AcceleratedWidget widget,
                      uint32_t frame_id,
                      gfx::FrameData data,
                      std::vector<wl::WaylandOverlayConfig> overlays);

  // Asks Wayland to destroy a wl_buffer.
  void DestroyBuffer(uint32_t buffer_id);

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* GetGbmDevice();
#endif

  bool supports_acquire_fence() const { return supports_acquire_fence_; }
  bool supports_viewporter() const { return supports_viewporter_; }
  bool supports_overlays() const { return supports_overlays_; }
  bool supports_non_backed_solid_color_buffers() const {
    return supports_non_backed_solid_color_buffers_;
  }
  bool supports_single_pixel_buffer() const {
    return supports_single_pixel_buffer_;
  }
  bool supports_subpixel_accurate_position() const {
    return supports_subpixel_accurate_position_;
  }
  bool supports_surface_background_color() const {
    return supports_surface_background_color_;
  }
  bool supports_clip_rect() const { return supports_clip_rect_; }
  bool supports_affine_transform() const { return supports_affine_transform_; }
  bool supports_out_of_window_clip_rect() const {
    return supports_out_of_window_clip_rect_;
  }
  bool has_transformation_fix() const { return has_transformation_fix_; }

  void set_drm_modifiers_filter(
      std::unique_ptr<DrmModifiersFilter> drm_modifiers_filter) {
    drm_modifiers_filter_ = std::move(drm_modifiers_filter);
  }

  // Adds a WaylandBufferManagerGpu binding.
  void AddBindingWaylandBufferManagerGpu(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver);

  // Returns supported modifiers for the supplied |buffer_format|.
  const std::vector<uint64_t> GetModifiersForBufferFormat(
      gfx::BufferFormat buffer_format) const;

  // Allocates a unique buffer ID.
  uint32_t AllocateBufferID();

  // Returns if a format is supported by current Wayland implementation.
  bool SupportsFormat(gfx::BufferFormat buffer_format) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest, CreateSurfaceCheckGbm);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandCommitOverlaysCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandGroupOnSubmissionCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryCompositorV3,
                           SurfaceDamageTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest,
                           ExecutesTasksAfterInitialization);
  FRIEND_TEST_ALL_PREFIXES(WaylandOverlayManagerTest,
                           SupportsNonIntegerDisplayRect);

  void BindHostInterface(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host);

  void SaveTaskRunnerForWidgetOnIOThread(
      gfx::AcceleratedWidget widget,
      scoped_refptr<base::SingleThreadTaskRunner> origin_runner);
  void ForgetTaskRunnerForWidgetOnIOThread(gfx::AcceleratedWidget widget);

  // Provides the WaylandSurfaceGpu, which backs the |widget|, with swap and
  // presentation results.
  void HandleSubmissionOnOriginThread(
      gfx::AcceleratedWidget widget,
      uint32_t frame_id,
      gfx::SwapResult swap_result,
      gfx::GpuFenceHandle release_fence,
      const std::vector<wl::WaylandPresentationInfo>& presentation_infos);
  void HandlePresentationOnOriginThread(
      gfx::AcceleratedWidget widget,
      const std::vector<wl::WaylandPresentationInfo>& presentation_infos);

  void OnHostDisconnected();

  // Executes the |task| immediately if the pipe has been bound. Otherwise, the
  // tasks are stored and executed after the remote pipe becomes bound.
  void RunOrQueueTask(base::OnceClosure task);
  // Called when the manager is initialized and the remote host is bound. Runs
  // pending tasks.
  void ProcessPendingTasks();

  // Internal methods that do calls to the |remote_host|.
  void CreateDmabufBasedBufferTask(base::ScopedFD dmabuf_fd,
                                   gfx::Size size,
                                   const std::vector<uint32_t>& strides,
                                   const std::vector<uint32_t>& offsets,
                                   const std::vector<uint64_t>& modifiers,
                                   uint32_t current_format,
                                   uint32_t planes_count,
                                   uint32_t buffer_id);
  void CreateShmBasedBufferTask(base::ScopedFD shm_fd,
                                size_t length,
                                gfx::Size size,
                                uint32_t buffer_id);
  void CreateSolidColorBufferTask(SkColor4f color,
                                  const gfx::Size& size,
                                  uint32_t buf_id);
  void CreateSinglePixelBufferTask(SkColor4f color, uint32_t buf_id);
  void CommitOverlaysTask(gfx::AcceleratedWidget widget,
                          uint32_t frame_id,
                          gfx::FrameData data,
                          std::vector<wl::WaylandOverlayConfig> overlays);
  void DestroyBufferTask(uint32_t buffer_id);

#if defined(WAYLAND_GBM)
  // Uses |drm_node_path| to open the handle and store it into
  // |drm_render_node_fd|.
  void OpenAndStoreDrmRenderNodeFd(const base::FilePath& drm_node_path);
  // Used by the gbm_device for self creation.
  base::ScopedFD drm_render_node_fd_;
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
  // When set, avoids creating a real gbm_device. Instead, tests that set
  // this variable to true must set own instance of the GbmDevice. See the
  // CreateSurfaceCheckGbm for example.
  bool use_fake_gbm_device_for_test_ = false;
#endif
  // Whether Wayland server allows buffer submission with acquire fence.
  bool supports_acquire_fence_ = false;

  // Whether Wayland server implements wp_viewporter extension to support
  // cropping and scaling buffers.
  bool supports_viewporter_ = false;

  // Whether delegated overlays should be used for this Wayland server.
  bool supports_overlays_ = false;

  // Determines whether solid color overlays can be delegated without a backing
  // image via a wayland protocol.
  bool supports_non_backed_solid_color_buffers_ = false;

  // Determines whether single pixel buffer are supported via a wayland
  // protocol.
  bool supports_single_pixel_buffer_ = false;

  // Determines whether subpixel accurate position is supported.
  bool supports_subpixel_accurate_position_ = false;

  // Determines whether background information for surfaces is supported.
  bool supports_surface_background_color_ = false;

  // Determines whether Wayland server supports Wayland protocols that allow to
  // export wl_buffers backed by dmabuf.
  bool supports_dmabuf_ = true;

  bool supports_clip_rect_ = false;

  // Determines whether Wayland server supports delegating non axis-aligned 2d
  // transforms.
  bool supports_affine_transform_ = false;

  // Whether wayland server supports clip delegation for quads that are
  // partially or fully outside of the window.
  bool supports_out_of_window_clip_rect_ = false;

  // Whether wayland server has the fix that applies transformations in the
  // correct order.
  bool has_transformation_fix_ = false;

  // A DRM modifiers filter to ensure we don't allocate buffers with modifiers
  // not supported by Vulkan.
  std::unique_ptr<DrmModifiersFilter> drm_modifiers_filter_;

  mojo::ReceiverSet<ozone::mojom::WaylandBufferManagerGpu> receiver_set_;

  // A pointer to a WaylandBufferManagerHost object, which always lives on a
  // browser process side. It's used for a multi-process mode.
  mojo::Remote<ozone::mojom::WaylandBufferManagerHost> remote_host_;

  mojo::AssociatedReceiver<ozone::mojom::WaylandBufferManagerGpu>
      associated_receiver_{this};

  std::map<gfx::AcceleratedWidget, raw_ptr<WaylandSurfaceGpu, CtnExperimental>>
      widget_to_surface_map_;  // Guarded by |lock_|.

  // Supported buffer formats and modifiers sent by the Wayland compositor to
  // the client. Corresponds to the map stored in WaylandZwpLinuxDmabuf and
  // passed from it during initialization of this gpu host.
  base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
      supported_buffer_formats_with_modifiers_;

  // These task runners can be used to pass messages back to the same thread,
  // where the commit buffer request came from. For example, swap requests can
  // come from the Viz thread, but are rerouted to the GpuMainThread and then
  // mojo calls happen. However, when the manager receives mojo calls, it has to
  // reroute calls back to the same thread where the calls came from to ensure
  // correct sequence. Note that not all calls come from the Viz thread, e.g.
  // GbmPixmapWayland may call from either the GpuMainThread or IOChildThread.
  // This map must only be accessed from the GpuMainThread.
  base::small_map<std::map<gfx::AcceleratedWidget,
                           scoped_refptr<base::SingleThreadTaskRunner>>>
      commit_thread_runners_;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on GpuMainThread. This is
  // needed to ensure mojo calls happen on a right sequence.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_runner_;

  // Protects access to |widget_to_surface_map_| and |commit_thread_runners_|.
  base::Lock lock_;

  // Keeps track of the next unique buffer ID.
  uint32_t next_buffer_id_ = 0;

  // The tasks that are blocked on a remote_host pipe becoming bound.
  std::vector<base::OnceClosure> pending_tasks_;

  // All calls must happen on the correct sequence. See comments in the
  // constructor for more details.
  SEQUENCE_CHECKER(gpu_sequence_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_H_
