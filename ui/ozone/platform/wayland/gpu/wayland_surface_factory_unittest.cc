// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"

#include <drm_fourcc.h>
#include <wayland-util.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gl/gl_utils.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::SaveArg;
using ::testing::Values;

namespace ui {

namespace {

constexpr uint32_t kAugmentedSurfaceNotSupportedVersion = 0;

// Holds a NativePixmap used for scheduling overlay planes. It must become busy
// when scheduled and be associated with the swap id to track correct order of
// swaps and releases of the image.
// TODO(rjkroege): Consider putting extra state inside a test NativePixmap
// implementation instead of a wrapper class.
class OverlayImageHolder : public base::RefCounted<OverlayImageHolder> {
 public:
  OverlayImageHolder(scoped_refptr<gfx::NativePixmap> pixmap,
                     const gfx::Size& size)
      : pixmap_(pixmap) {}

  // Associates swap id with this image.
  void AssociateWithSwapId(uint32_t swap_id) {
    ASSERT_NE(swap_id_, swap_id);
    swap_id_ = swap_id;
  }

  // Returns associated swap id with this image.
  uint32_t GetAssociateWithSwapId() { return swap_id_; }

  // The image is set busy when scheduled as overlay plane for
  // GbmSurfacelessWayland
  void SetBusy(bool busy) { busy_ = busy; }
  bool busy() const { return busy_; }

  // Sets the image as displayed.
  void SetDisplayed(bool displayed) { displayed_ = displayed; }
  bool displayed() const { return displayed_; }

  scoped_refptr<gfx::NativePixmap> GetNativePixmap() { return pixmap_; }

 private:
  friend class base::RefCounted<OverlayImageHolder>;

  ~OverlayImageHolder() = default;

  scoped_refptr<gfx::NativePixmap> pixmap_;

  // Indicated if the overlay image is busy. If yes, it was scheduled as overlay
  // plane for further submission and can't be reused until it's freed.
  bool busy_ = false;

  bool displayed_ = false;

  uint32_t swap_id_ = std::numeric_limits<uint32_t>::max();
};

// Helper that helps to identify the last swap id. Also sets gl image associated
// with that swap as free.
class CallbacksHelper {
 public:
  CallbacksHelper() = default;
  ~CallbacksHelper() = default;

  // Returns last executed swap id that received SwapCompletionCallback.
  uint32_t GetLastFinishedSwapId() const { return last_finish_swap_id_; }

  // Returns next available swap id that must be used for the next submission of
  // the buffer.
  uint32_t GetNextLocalSwapId() {
    auto next_swap_id = local_swap_id_++;
    pending_local_swap_ids_.push(next_swap_id);
    return next_swap_id;
  }

  void ResetLastFinishedSwapId() {
    last_finish_swap_id_ = std::numeric_limits<uint32_t>::max();
  }

  // Finishes the submission by setting the swap id of completed buffer swap and
  // sets the associated overlay_image as displayed and non-busy, which
  // indicates that 1) the image has been sent to be shown after being scheduled
  // 2) the image is displayed. This sort of mimics a buffer queue, but in a
  // simpler way.
  void FinishPresent(
      uint32_t local_swap_id,
      std::vector<scoped_refptr<OverlayImageHolder>> overlay_images,
      gfx::SwapCompletionResult result) {
    last_finish_swap_id_ = pending_local_swap_ids_.front();
    pending_local_swap_ids_.pop();

    for (auto& overlay_image : overlay_images) {
      EXPECT_EQ(overlay_image->GetAssociateWithSwapId(), last_finish_swap_id_);
      EXPECT_TRUE(overlay_image->busy() && !overlay_image->displayed());
      overlay_image->SetBusy(false);
      overlay_image->SetDisplayed(true);
    }

    for (auto& displayed_image : displayed_images_)
      displayed_image->SetDisplayed(false);
    displayed_images_ = std::move(overlay_images);
  }

  void BufferPresented(uint64_t local_swap_id,
                       const gfx::PresentationFeedback& feedback) {
    // Make sure the presentation doesn't come earlier than than swap
    // completion. We don't explicitly check if the buffer is presented as this
    // assert is more than enough.
    ASSERT_TRUE(pending_local_swap_ids_.empty() ||
                pending_local_swap_ids_.front() > local_swap_id);
  }

  // Corresponds to SoftwareOutputDevice::SwapBuffersCallback so that it can be
  // used with canvas surfaces.
  void CanvasSwapBuffersCallback(uint64_t local_swap_id,
                                 const gfx::Size& pixel_size) {
    last_finish_swap_id_ = pending_local_swap_ids_.front();
    pending_local_swap_ids_.pop();
    DCHECK_EQ(local_swap_id, last_finish_swap_id_);
    last_canvas_swap_pixel_size_ = pixel_size;
  }

  gfx::Size GetLastCanvasSwapPixelSize() const {
    return std::move(last_canvas_swap_pixel_size_);
  }

 private:
  uint32_t local_swap_id_ = 0;
  // Make sure that local_swap_id_ != last_finish_swap_id_.
  uint32_t last_finish_swap_id_ = std::numeric_limits<uint32_t>::max();
  base::queue<uint64_t> pending_local_swap_ids_;

  // Keeps track of a displayed image.
  std::vector<scoped_refptr<OverlayImageHolder>> displayed_images_;

  // Keeps track of last swap pixel size. Used only for the path that uses
  // canvas.
  gfx::Size last_canvas_swap_pixel_size_;
};

}  // namespace

class WaylandSurfaceFactoryTest : public WaylandTest {
 public:
  WaylandSurfaceFactoryTest() = default;
  WaylandSurfaceFactoryTest(const WaylandSurfaceFactoryTest&) = delete;
  WaylandSurfaceFactoryTest& operator=(const WaylandSurfaceFactoryTest&) =
      delete;
  ~WaylandSurfaceFactoryTest() override = default;

  void SetUp() override {
    const base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
        kSupportedFormatsWithModifiers{
            {gfx::BufferFormat::BGRA_8888, {DRM_FORMAT_MOD_LINEAR}}};

    WaylandTest::SetUp();

    auto manager_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(
        std::move(manager_ptr), kSupportedFormatsWithModifiers,
        /*supports_dma_buf=*/false,
        /*supports_viewporter=*/true,
        /*supports_acquire_fence=*/false,
        /*supports_overlays=*/true, kAugmentedSurfaceNotSupportedVersion,
        /*supports_single_pixel_buffer=*/true,
        /*server_version=*/{});

    // Wait until initialization and mojo calls go through.
    base::RunLoop().RunUntilIdle();

    // Store the surface_id for convenience.
    surface_id_ = window_->root_surface()->get_surface_id();
  }

  void TearDown() override {
    // The mojo call to destroy shared buffer goes after surfaces are destroyed.
    // Wait until it's done.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvas(
      gfx::AcceleratedWidget widget) {
    auto canvas = surface_factory_->CreateCanvasForWidget(widget_);
    base::RunLoop().RunUntilIdle();

    return canvas;
  }

  void ScheduleOverlayPlane(gl::Presenter* presenter,
                            gl::OverlayImage image,
                            int z_order) {
    presenter->ScheduleOverlayPlane(
        image, nullptr,
        gfx::OverlayPlaneData(
            z_order, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
            gfx::RectF(window_->GetBoundsInPixels()), {}, false,
            gfx::Rect(window_->applied_state().size_px), 1.0f,
            gfx::OverlayPriorityHint::kNone, gfx::RRectF(),
            gfx::ColorSpace::CreateSRGB(), std::nullopt));
  }

  uint32_t surface_id_ = 0;
};

TEST_P(WaylandSurfaceFactoryTest,
       GbmSurfacelessWaylandCommitOverlaysCallbacksTest) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }
  // This tests multiple buffers per-frame and order of SwapCompletionCallbacks.
  // Even when all OnSubmission from later frames are called, their
  // SwapCompletionCallbacks should not run until previous frames'
  // SwapCompletionCallbacks run.
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->use_fake_gbm_device_for_test_ = true;
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();
  buffer_manager_gpu_->supports_dmabuf_ = true;

  auto* gl_ozone = surface_factory_->GetGLOzone(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  auto presenter = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_TRUE(presenter);
  presenter->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(presenter.get())
      ->SetNoGLFlushForTests();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Expect to create 4 buffers.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(4);
  });

  // Create buffers and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<OverlayImageHolder>> fake_overlay_image;
  for (int i = 0; i < 4; ++i) {
    auto size_px = window_->applied_state().size_px;
    auto native_pixmap = surface_factory_->CreateNativePixmap(
        widget_, nullptr, size_px, gfx::BufferFormat::BGRA_8888,
        gfx::BufferUsage::SCANOUT);
    fake_overlay_image.push_back(
        base::MakeRefCounted<OverlayImageHolder>(native_pixmap, size_px));
  }

  CallbacksHelper cbs_helper;
  // Submit a frame with an overlay and background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[0]->SetBusy(true);

    // Prepare background.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[0]->GetNativePixmap(),
                         /*z_order=*/INT32_MIN);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[1]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[1]->GetNativePixmap(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[0]);
    overlay_images.push_back(fake_overlay_image[1]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Wait until GbmSurfacelessWayland submits the buffer according to internal
  // queue.
  base::RunLoop().RunUntilIdle();

  // The fake server must have dmabuf params created. Set expectations and
  // notify the client about created buffers.
  const uint32_t primary_subsurface_id =
      window_->primary_subsurface()->wayland_surface()->get_surface_id();
  PostToServerAndWait([main_surface_id = surface_id_, primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    auto* mock_primary_surface =
        server->GetObject<wl::MockSurface>(primary_subsurface_id);

    // Also, we expect no buffer committed on primary subsurface.
    EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Damage(_, _, _, _)).Times(0);
    // 1 buffer committed on root surface.
    EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*root_surface, Frame(_)).Times(1);
    EXPECT_CALL(*root_surface, Commit()).Times(1);

    // The wl_buffers are requested during ScheduleOverlays. Thus, we have
    // pending requests that we need to execute.
    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    ASSERT_EQ(params_vector.size(), 2u);
    for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
      zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                              mock_params->buffer_resource());
    }
  });

  // Verify our server side expectations now.
  PostToServerAndWait([main_surface_id = surface_id_, primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(primary_subsurface_id));
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(main_surface_id));
  });

  // Give mojo the chance to pass the callbacks if any.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/Damage/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& overlay_image : fake_overlay_image) {
    // All the images except the first one, which was associated with swap
    // id=0u, must be busy and not displayed. The first one must be displayed.
    if (overlay_image->GetAssociateWithSwapId() == 0u) {
      EXPECT_FALSE(overlay_image->busy());
      EXPECT_TRUE(overlay_image->displayed());
    } else {
      EXPECT_FALSE(overlay_image->displayed());
    }
  }

  // Submit another frame with only an overlay.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[2]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[2]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[2]->GetNativePixmap(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[2]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Wait until GbmSurfacelessWayland submits the buffer according to internal
  // queue.
  base::RunLoop().RunUntilIdle();

  const uint32_t overlay_surface_id = (*window_->wayland_subsurfaces().begin())
                                          ->wayland_surface()
                                          ->get_surface_id();
  // The fake server must have dmabuf params created. Set expectations and
  // notify the client about created buffers.
  PostToServerAndWait([overlay_surface_id, main_surface_id = surface_id_,
                       primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    auto* mock_overlay_surface =
        server->GetObject<wl::MockSurface>(overlay_surface_id);
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    auto* mock_primary_surface =
        server->GetObject<wl::MockSurface>(primary_subsurface_id);

    // Expect no buffer committed on primary subsurface.
    EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Damage(_, _, _, _)).Times(0);
    // Expect 1 buffer to be committed on overlay subsurface, with frame
    // callback.
    EXPECT_CALL(*mock_overlay_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Damage(_, _, _, _)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Commit()).Times(1);
    // Expect no buffer committed on root surface.
    EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Frame(_)).Times(0);
    EXPECT_CALL(*root_surface, Damage(_, _, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Commit()).Times(1);

    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    ASSERT_EQ(params_vector.size(), 1u);
    for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
      zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                              mock_params->buffer_resource());
    }

    // Also send the frame callback so that pending buffer for swap id=1u is
    // processed and swapped.
    root_surface->SendFrameCallback();
  });

  // Give a client a chance to send requests and then verify our expectations on
  // the server thread.
  PostToServerAndWait(
      [overlay_surface_id, main_surface_id = surface_id_,
       primary_subsurface_id](wl::TestWaylandServerThread* server) {
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(primary_subsurface_id));
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(overlay_surface_id));
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(main_surface_id));
      });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // Even though the second buffer was submitted, we mustn't receive
  // SwapCompletionCallback until the previous buffer is released.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // Submit another frame with 0 overlays, 1 primary plane.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[3]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[3]->GetNativePixmap(),
                         /*z_order=*/0);

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[3]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Wait until GbmSurfacelessWayland submits the buffer according to internal
  // queue.
  base::RunLoop().RunUntilIdle();

  // Process requests and send events.
  PostToServerAndWait([overlay_surface_id, main_surface_id = surface_id_,
                       primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    auto* mock_overlay_surface =
        server->GetObject<wl::MockSurface>(overlay_surface_id);
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    auto* mock_primary_surface =
        server->GetObject<wl::MockSurface>(primary_subsurface_id);

    // Expect 1 buffer committed on primary subsurface, with frame callback.
    EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Damage(_, _, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
    // Expect no buffer to be committed on overlay subsurface.
    EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_overlay_surface, Damage(_, _, _, _)).Times(0);
    // Expect no buffer committed on root surface.
    EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Frame(_)).Times(0);
    EXPECT_CALL(*root_surface, Damage(_, _, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Commit()).Times(1);

    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    ASSERT_EQ(params_vector.size(), 1u);
    for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
      zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                              mock_params->buffer_resource());
    }
  });

  // Send the frame callback so that pending buffer for swap id=2u is processed
  // and swapped.
  PostToServerAndWait(
      [overlay_surface_id](wl::TestWaylandServerThread* server) {
        // Send the frame callback so that pending buffer for swap id=1u is
        // processed and swapped.
        auto* mock_overlay_surface =
            server->GetObject<wl::MockSurface>(overlay_surface_id);
        mock_overlay_surface->SendFrameCallback();
        // Release overlay image with swap id=1u before swap id=2u.
        mock_overlay_surface->ReleaseBuffer(
            mock_overlay_surface->attached_buffer());
      });

  // Even 2nd frame is released, we do not send OnSubmission() out of order.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  PostToServerAndWait(
      [overlay_surface_id, main_surface_id = surface_id_,
       primary_subsurface_id](wl::TestWaylandServerThread* server) {
        // Verify our expectations.
        auto* mock_overlay_surface =
            server->GetObject<wl::MockSurface>(overlay_surface_id);
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(primary_subsurface_id));
        testing::Mock::VerifyAndClearExpectations(mock_overlay_surface);
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(main_surface_id));

        // This will result in Wayland server releasing previously attached
        // buffer for swap id=1u and calling OnSubmission for buffer with swap
        // id=1u.
        mock_overlay_surface->ReleaseBuffer(
            mock_overlay_surface->prev_attached_buffer());
      });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We should expect next 2 SwapCompletionCallbacks for the first 2 swap ids
  // consecutively.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 2u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& overlay_image : fake_overlay_image) {
    if (overlay_image->GetAssociateWithSwapId() == 2u) {
      EXPECT_TRUE(overlay_image->displayed());
      EXPECT_FALSE(overlay_image->busy());
    } else {
      EXPECT_FALSE(overlay_image->displayed());
    }
  }
}

TEST_P(WaylandSurfaceFactoryTest,
       GbmSurfacelessWaylandGroupOnSubmissionCallbacksTest) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }
  // This tests multiple buffers per-frame. GbmSurfacelessWayland receive 1
  // OnSubmission call per frame before running in submission order.
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->use_fake_gbm_device_for_test_ = true;
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();
  buffer_manager_gpu_->supports_dmabuf_ = true;

  auto* gl_ozone = surface_factory_->GetGLOzone(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  auto presenter = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_TRUE(presenter);
  presenter->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(presenter.get())
      ->SetNoGLFlushForTests();

  // Create buffers and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<OverlayImageHolder>> fake_overlay_image;
  for (int i = 0; i < 5; ++i) {
    auto size_px = window_->applied_state().size_px;
    auto native_pixmap = surface_factory_->CreateNativePixmap(
        widget_, nullptr, size_px, gfx::BufferFormat::BGRA_8888,
        gfx::BufferUsage::SCANOUT);
    fake_overlay_image.push_back(
        base::MakeRefCounted<OverlayImageHolder>(native_pixmap, size_px));
  }

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Expect to create 3 buffers.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  CallbacksHelper cbs_helper;
  // Submit a frame with 1 primary plane, 1 underlay, and 1 background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[0]->SetBusy(true);

    // Prepare background.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[0]->GetNativePixmap(),
                         /*z_order=*/INT32_MIN);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[1]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[1]->GetNativePixmap(),
                         /*z_order=*/0);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[2]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[2]->SetBusy(true);

    // Prepare underlay plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[2]->GetNativePixmap(),
                         /*z_order=*/-1);

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[0]);
    overlay_images.push_back(fake_overlay_image[1]);
    overlay_images.push_back(fake_overlay_image[2]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Wait until GbmSurfacelessWayland submits the buffer according to internal
  // queue.
  base::RunLoop().RunUntilIdle();

  // Also, we expect primary buffer to be committed.
  const uint32_t primary_subsurface_id =
      window_->primary_subsurface()->wayland_surface()->get_surface_id();
  PostToServerAndWait([main_surface_id = surface_id_, primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    auto* mock_primary_surface =
        server->GetObject<wl::MockSurface>(primary_subsurface_id);

    EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Damage(_, _, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
    EXPECT_CALL(*root_surface, Frame(_)).Times(1);
    EXPECT_CALL(*root_surface, Commit()).Times(1);

    testing::Mock::VerifyAndClearExpectations(server->zwp_linux_dmabuf_v1());

    // If wl_buffers have never been created, they will be requested during the
    // first commit.
    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    ASSERT_EQ(params_vector.size(), 3u);
    for (wl::TestZwpLinuxBufferParamsV1* param : params_vector) {
      zwp_linux_buffer_params_v1_send_created(param->resource(),
                                              param->buffer_resource());
    }
  });

  PostToServerAndWait([main_surface_id = surface_id_, primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(primary_subsurface_id));
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(main_surface_id));
  });

  auto* subsurface = window_->wayland_subsurfaces().begin()->get();
  const uint32_t overlay_surface_id =
      subsurface->wayland_surface()->get_surface_id();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/Damage/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& overlay_image : fake_overlay_image) {
    // All the images except the first one, which was associated with swap
    // id=0u, must be busy and not displayed. The first one must be displayed.
    if (overlay_image->GetAssociateWithSwapId() == 0u) {
      EXPECT_FALSE(overlay_image->busy());
      EXPECT_TRUE(overlay_image->displayed());
    } else {
      EXPECT_FALSE(overlay_image->displayed());
    }
  }

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Expect to create 2 more buffers.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });

  // Submit another frame with 1 primary plane and 1 overlay
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[3]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[3]->GetNativePixmap(),
                         /*z_order=*/0);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[4]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[4]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(presenter.get(),
                         fake_overlay_image[4]->GetNativePixmap(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[3]);
    overlay_images.push_back(fake_overlay_image[4]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Give mojo messages chance to reach host.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([main_surface_id = surface_id_, overlay_surface_id,
                       primary_subsurface_id](
                          wl::TestWaylandServerThread* server) {
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    auto* mock_primary_surface =
        server->GetObject<wl::MockSurface>(primary_subsurface_id);
    auto* mock_overlay_surface =
        server->GetObject<wl::MockSurface>(overlay_surface_id);

    // Expect primary buffer to be committed, but since it is not the top-most
    // surface in the frame it does not setup frame callback.
    EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_primary_surface, Damage(_, _, _, _)).Times(1);
    EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);

    // Expect overlay buffer to be committed, and sets up frame callback.
    EXPECT_CALL(*mock_overlay_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Damage(_, _, _, _)).Times(1);
    EXPECT_CALL(*mock_overlay_surface, Commit()).Times(1);

    // Expect root surface to be committed without buffer.
    EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Frame(_)).Times(0);
    EXPECT_CALL(*root_surface, Damage(_, _, _, _)).Times(0);
    EXPECT_CALL(*root_surface, Commit()).Times(1);

    testing::Mock::VerifyAndClearExpectations(server->zwp_linux_dmabuf_v1());

    // 2 more buffers are to be created.
    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    ASSERT_EQ(params_vector.size(), 2u);
    for (wl::TestZwpLinuxBufferParamsV1* param : params_vector) {
      zwp_linux_buffer_params_v1_send_created(param->resource(),
                                              param->buffer_resource());
    }
  });

  PostToServerAndWait([main_surface_id =
                           surface_id_](wl::TestWaylandServerThread* server) {
    auto* root_surface = server->GetObject<wl::MockSurface>(main_surface_id);
    // Send the frame callback so that pending buffer for swap id=1u is
    // processed and swapped.
    root_surface->SendFrameCallback();
  });

  PostToServerAndWait(
      [primary_subsurface_id](wl::TestWaylandServerThread* server) {
        testing::Mock::VerifyAndClearExpectations(
            server->GetObject<wl::MockSurface>(primary_subsurface_id));
      });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // Even though the second frame was submitted, we mustn't receive
  // SwapCompletionCallback until the previous frame's buffers are released.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  PostToServerAndWait(
      [primary_subsurface_id](wl::TestWaylandServerThread* server) {
        auto* mock_primary_surface =
            server->GetObject<wl::MockSurface>(primary_subsurface_id);
        // This will result in Wayland server releasing one of the previously
        // attached buffers for swap id=0u and calling OnSubmission for a buffer
        // with swap id=1u attached to the primary surface.
        mock_primary_surface->ReleaseBuffer(
            mock_primary_surface->prev_attached_buffer());
      });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // OnSubmission was only called for one of the buffers with swap id=1u, so
  // SwapCompletionCallback should not run.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  PostToServerAndWait(
      [overlay_surface_id](wl::TestWaylandServerThread* server) {
        auto* mock_overlay_surface =
            server->GetObject<wl::MockSurface>(overlay_surface_id);
        // Release the another buffer.
        mock_overlay_surface->ReleaseBuffer(
            mock_overlay_surface->prev_attached_buffer());
      });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // OnSubmission was called for both of the buffers with swap id=1u, so
  // SwapCompletionCallback should run.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 1u);

  for (const auto& overlay_image : fake_overlay_image) {
    if (overlay_image->GetAssociateWithSwapId() == 1u) {
      EXPECT_TRUE(overlay_image->displayed());
      EXPECT_FALSE(overlay_image->busy());
    } else {
      EXPECT_FALSE(overlay_image->displayed());
    }
  }
}

TEST_P(WaylandSurfaceFactoryTest, Canvas) {
  const std::vector<float> scale_factors = {1, 1.2, 1.3, 1.5, 1.7, 2, 2.3, 2.8};
  for (auto scale_factor : scale_factors) {
    auto canvas = CreateCanvas(widget_);
    ASSERT_TRUE(canvas);

    auto bounds_px = window_->GetBoundsInDIP();
    bounds_px = gfx::ScaleToRoundedRect(bounds_px, scale_factor);

    canvas->ResizeCanvas(bounds_px.size(), scale_factor);
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    const gfx::Rect damage(5, 10, 20, 15);
    // Surface damage will be affected by the scale, which must be an integer.
    const gfx::Rect expected_damage = ScaleToEnclosingRect(
        gfx::Rect(5, 10, 20, 15), 1.f / std::ceil(scale_factor));

    const uint32_t surface_id = window_->root_surface()->get_surface_id();
    PostToServerAndWait([surface_id,
                         expected_damage](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      ASSERT_FALSE(mock_surface->attached_buffer());
      Expectation damage =
          EXPECT_CALL(*mock_surface,
                      Damage(expected_damage.x(), expected_damage.y(),
                             expected_damage.width(), expected_damage.height()))
              .Times(1);
      Expectation attach = EXPECT_CALL(*mock_surface, Attach(_, 0, 0)).Times(1);
      EXPECT_CALL(*mock_surface, Commit()).After(damage, attach);
    });

    canvas->PresentCanvas(damage);
    canvas->OnSwapBuffers(base::DoNothing(), gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait(
        [surface_id, bounds_px](wl::TestWaylandServerThread* server) {
          auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
          auto* buffer_resource = mock_surface->attached_buffer();
          ASSERT_TRUE(buffer_resource);
          wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
          ASSERT_TRUE(buffer);
          EXPECT_EQ(wl_shm_buffer_get_width(buffer), bounds_px.width());
          EXPECT_EQ(wl_shm_buffer_get_height(buffer), bounds_px.height());

          // Release the buffer immediately as the test always attaches the same
          // buffer.
          mock_surface->ReleaseBufferFenced(buffer_resource, {});

          mock_surface->SendFrameCallback();
        });
  }

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_P(WaylandSurfaceFactoryTest, CanvasResize) {
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBoundsInDIP().size(), 1);
  auto* sk_canvas = canvas->GetCanvas();
  ASSERT_TRUE(sk_canvas);
  canvas->ResizeCanvas(gfx::Size(100, 50), 1);
  sk_canvas = canvas->GetCanvas();
  ASSERT_TRUE(sk_canvas);

  const uint32_t surface_id = window_->root_surface()->get_surface_id();
  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    Expectation damage =
        EXPECT_CALL(*mock_surface, Damage(0, 0, 100, 50)).Times(1);
    Expectation attach = EXPECT_CALL(*mock_surface, Attach(_, 0, 0)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).After(damage, attach);
  });

  canvas->PresentCanvas(gfx::Rect(0, 0, 100, 50));
  canvas->OnSwapBuffers(base::DoNothing(), gfx::FrameData());

  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* buffer_resource = mock_surface->attached_buffer();
    ASSERT_TRUE(buffer_resource);

    ASSERT_TRUE(buffer_resource);
    wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
    ASSERT_TRUE(buffer);
    EXPECT_EQ(wl_shm_buffer_get_width(buffer), 100);
    EXPECT_EQ(wl_shm_buffer_get_height(buffer), 50);
  });
}

// Checks that buffer swap ack is called only after Wayland calls OnSubmission.
TEST_P(WaylandSurfaceFactoryTest, CanvasBufferSwapAck) {
  constexpr float kDefaultScaleFactor = 1u;
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  auto bounds_px = window_->GetBoundsInPixels();

  canvas->ResizeCanvas(bounds_px.size(), kDefaultScaleFactor);

  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Send the first buffer. OnSubmission must be received immediately.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));
    CallbacksHelper cbs_helper;
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper),
                       cbs_helper.GetNextLocalSwapId()),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      mock_surface->SendFrameCallback();
    });

    base::RunLoop().RunUntilIdle();

    // The first OnSubmission comes immediately regardless on buffer releases.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
  }

  // Now submit the second buffer. OnSubmission must come only after the buffer
  // is released.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(1, 1, 30, 55));
    CallbacksHelper cbs_helper;
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper),
                       cbs_helper.GetNextLocalSwapId()),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      mock_surface->SendFrameCallback();
    });

    base::RunLoop().RunUntilIdle();

    // The second OnSubmission will come only after a buffer is released.
    EXPECT_TRUE(cbs_helper.GetLastCanvasSwapPixelSize().IsEmpty());

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      auto* buffer_resource = mock_surface->prev_attached_buffer();
      ASSERT_TRUE(buffer_resource);
      mock_surface->ReleaseBufferFenced(buffer_resource, {});
    });

    base::RunLoop().RunUntilIdle();

    // The second OnSubmission will come only after a buffer is released.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
  }
}

// Checks that buffer swap ack for an invalid frame is called after the previous
// valid frames receive their OnSubmission calls.
TEST_P(WaylandSurfaceFactoryTest, CanvasBufferSwapAck2) {
  constexpr float kDefaultScaleFactor = 1u;
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  auto bounds_px = window_->GetBoundsInPixels();

  canvas->ResizeCanvas(bounds_px.size(), kDefaultScaleFactor);

  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Send the first buffer. OnSubmission must be received immediately.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));
    CallbacksHelper cbs_helper;
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper),
                       cbs_helper.GetNextLocalSwapId()),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      mock_surface->SendFrameCallback();
    });

    base::RunLoop().RunUntilIdle();

    // The first OnSubmission comes immediately regardless on buffer releases.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
  }

  // Now submit the second buffer/frame and the third buffer/frame. Two
  // OnSubmission must come in a correct order - one for the valid frame and
  // another for the invalid frame.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(1, 1, 30, 55));
    CallbacksHelper cbs_helper;
    uint32_t second_buffer_swap_id = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), second_buffer_swap_id),
        gfx::FrameData());

    // Submit an invalid frame. It must be acked after the previous buffer/frame
    // gets OnSubmission.
    const uint32_t invalid_frame_swap_id = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), invalid_frame_swap_id),
        gfx::FrameData());

    // Submit a second invalid frame. This is required to ensure the correct
    // order of swaps.
    const uint32_t invalid_frame_swap_id2 = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), invalid_frame_swap_id2),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      mock_surface->SendFrameCallback();
    });

    base::RunLoop().RunUntilIdle();

    // The second (and the third) OnSubmission will come only after a buffer is
    // released.
    EXPECT_TRUE(cbs_helper.GetLastCanvasSwapPixelSize().IsEmpty());
    EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
              std::numeric_limits<uint32_t>::max());

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      auto* buffer_resource = mock_surface->prev_attached_buffer();
      ASSERT_TRUE(buffer_resource);
      mock_surface->ReleaseBufferFenced(buffer_resource, {});
    });

    base::RunLoop().RunUntilIdle();

    // The second OnSubmission will come only after a buffer is released.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
    // It must be the very last frame's swap id.
    EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), invalid_frame_swap_id2);
  }
}

// Check OnSubmission is called on resize.
TEST_P(WaylandSurfaceFactoryTest, CanvasBufferSwapAck3) {
  constexpr float kDefaultScaleFactor = 1u;
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  auto bounds_px = window_->GetBoundsInPixels();

  canvas->ResizeCanvas(bounds_px.size(), kDefaultScaleFactor);

  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  // Send the first buffer. OnSubmission must be received immediately.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));
    CallbacksHelper cbs_helper;
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper),
                       cbs_helper.GetNextLocalSwapId()),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      mock_surface->SendFrameCallback();
    });

    base::RunLoop().RunUntilIdle();

    // The first OnSubmission comes immediately regardless on buffer releases.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
  }

  // Now submit the second buffer/frame and couple of empty frames. All of
  // them must call OnSubmission as soon as Resize is called.
  {
    auto* sk_canvas = canvas->GetCanvas();
    ASSERT_TRUE(sk_canvas);

    canvas->PresentCanvas(gfx::Rect(1, 1, 30, 55));
    CallbacksHelper cbs_helper;
    uint32_t second_buffer_swap_id = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), second_buffer_swap_id),
        gfx::FrameData());

    const uint32_t invalid_frame_swap_id = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), invalid_frame_swap_id),
        gfx::FrameData());

    const uint32_t invalid_frame_swap_id2 = cbs_helper.GetNextLocalSwapId();
    canvas->OnSwapBuffers(
        base::BindOnce(&CallbacksHelper::CanvasSwapBuffersCallback,
                       base::Unretained(&cbs_helper), invalid_frame_swap_id2),
        gfx::FrameData());

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    // Nothing must come yet.
    EXPECT_TRUE(cbs_helper.GetLastCanvasSwapPixelSize().IsEmpty());
    EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
              std::numeric_limits<uint32_t>::max());

    gfx::Size new_size = bounds_px.size() + gfx::Size(2, 2);
    canvas->ResizeCanvas(new_size, kDefaultScaleFactor);

    // |Resize| will reset pending frames, which will post tasks for pending
    // buffer ack swap callbacks.
    base::RunLoop().RunUntilIdle();

    // OnSubmission must be called. The last swap id corresponds to the very
    // last frame.
    EXPECT_EQ(cbs_helper.GetLastCanvasSwapPixelSize(), bounds_px.size());
    EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), invalid_frame_swap_id2);
  }
}

TEST_P(WaylandSurfaceFactoryTest, CreateSurfaceCheckGbm) {
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->use_fake_gbm_device_for_test_ = true;

  // When gbm is not available, only canvas can be created with viz process
  // used.
  EXPECT_FALSE(buffer_manager_gpu_->GetGbmDevice());

  auto* gl_ozone = surface_factory_->GetGLOzone(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  EXPECT_TRUE(gl_ozone);
  auto presenter = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_FALSE(presenter);

  // Now, set gbm.
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();

  // It's still impossible to create the device if supports_dmabuf is false.
  EXPECT_FALSE(buffer_manager_gpu_->GetGbmDevice());
  presenter = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                       widget_);
  EXPECT_FALSE(presenter);

  // Now set supports_dmabuf.
  buffer_manager_gpu_->supports_dmabuf_ = true;
  EXPECT_TRUE(buffer_manager_gpu_->GetGbmDevice());
  presenter = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                       widget_);
  EXPECT_TRUE(presenter);

  // Reset gbm now. WaylandConnectionProxy can reset it when zwp is not
  // available. And factory must behave the same way as previously.
  buffer_manager_gpu_->gbm_device_ = nullptr;
  EXPECT_FALSE(buffer_manager_gpu_->GetGbmDevice());
  presenter = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                       widget_);
  EXPECT_FALSE(presenter);
}

class WaylandSurfaceFactoryCompositorV3 : public WaylandSurfaceFactoryTest {};

TEST_P(WaylandSurfaceFactoryCompositorV3, SurfaceDamageTest) {
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->use_fake_gbm_device_for_test_ = true;
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();
  buffer_manager_gpu_->supports_dmabuf_ = true;

  auto* gl_ozone = surface_factory_->GetGLOzone(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  ASSERT_TRUE(gl_ozone);
  auto presenter = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  ASSERT_TRUE(presenter);
  presenter->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(presenter.get())
      ->SetNoGLFlushForTests();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // This test only needs 1 buffer.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  gfx::Size test_buffer_size = {300, 100};
  gfx::RectF crop_uv = {0.1f, 0.2f, 0.5, 0.5f};
  gfx::Rect expected_src = gfx::ToEnclosingRect(
      gfx::ScaleRect({0.2f, 0.4f, 0.5f, 0.5f}, test_buffer_size.height(),
                     test_buffer_size.width()));
  gfx::Rect surface_damage_rect = window_->GetBoundsInPixels();

  // Create buffer and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<OverlayImageHolder>> fake_overlay_image;
  auto native_pixmap = surface_factory_->CreateNativePixmap(
      widget_, nullptr, test_buffer_size, gfx::BufferFormat::BGRA_8888,
      gfx::BufferUsage::SCANOUT);
  ASSERT_TRUE(native_pixmap);
  fake_overlay_image.push_back(base::MakeRefCounted<OverlayImageHolder>(
      native_pixmap, test_buffer_size));

  const uint32_t surface_id = window_->root_surface()->get_surface_id();

  CallbacksHelper cbs_helper;
  // Submit a frame with an overlay and background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_overlay_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_overlay_image[0]->SetBusy(true);

    // Prepare background.
    presenter->ScheduleOverlayPlane(
        fake_overlay_image[0]->GetNativePixmap(), nullptr,
        gfx::OverlayPlaneData(
            INT32_MIN,
            gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
            gfx::RectF(window_->GetBoundsInPixels()), crop_uv, false,
            surface_damage_rect, 1.0f, gfx::OverlayPriorityHint::kNone,
            gfx::RRectF(), gfx::ColorSpace::CreateSRGB(), std::nullopt));

    std::vector<scoped_refptr<OverlayImageHolder>> overlay_images;
    overlay_images.push_back(fake_overlay_image[0]);

    // And submit each image. They will be executed in FIFO manner.
    presenter->Present(
        base::BindOnce(&CallbacksHelper::FinishPresent,
                       base::Unretained(&cbs_helper), swap_id, overlay_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id),
        gfx::FrameData());
  }

  // Wait until the mojo calls are done.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait(
      [surface_id, expected_src, bounds_dip = window_->GetBoundsInDIP(),
       surface_damage_rect](wl::TestWaylandServerThread* server) {
        auto* root_surface = server->GetObject<wl::MockSurface>(surface_id);
        auto* test_viewport = root_surface->viewport();
        ASSERT_TRUE(test_viewport);

        EXPECT_CALL(*test_viewport,
                    // TODO(crbug.com/359904707) Use this instead of below
                    // workaround for rounding errors.

                    // SetSource(expected_src.x(), expected_src.y(),
                    //           expected_src.width(), expected_src.height()))

                    SetSource(_, _, _, _))
            .Times(1)
            .WillOnce(
                [expected_src](float x, float y, float width, float height) {
                  auto matches_with_precision_loss = [](float expected,
                                                        float actual) {
                    // Allows for a precision loss of 1/256
                    bool match = std::abs(wl_fixed_from_double(expected) -
                                          wl_fixed_from_double(actual)) <= 1;
                    if (!match) {
                      LOG(ERROR)
                          << "Expected: " << expected << " Actual: " << actual;
                    }
                    return match;
                  };
                  EXPECT_TRUE(matches_with_precision_loss(expected_src.x(), x));
                  EXPECT_TRUE(matches_with_precision_loss(expected_src.y(), y));
                  EXPECT_TRUE(
                      matches_with_precision_loss(expected_src.width(), width));
                  EXPECT_TRUE(matches_with_precision_loss(expected_src.height(),
                                                          height));
                });
        EXPECT_CALL(*test_viewport,
                    SetDestination(bounds_dip.width(), bounds_dip.height()))
            .Times(1);
        EXPECT_CALL(*root_surface, SetBufferTransform(WL_OUTPUT_TRANSFORM_90))
            .Times(1);
        Expectation damage =
            EXPECT_CALL(*root_surface, Damage(surface_damage_rect.origin().x(),
                                              surface_damage_rect.origin().y(),
                                              surface_damage_rect.width(),
                                              surface_damage_rect.height()))
                .Times(1);
        Expectation attach =
            EXPECT_CALL(*root_surface, Attach(_, 0, 0)).Times(1);
        EXPECT_CALL(*root_surface, Commit()).After(damage, attach);

        auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
        ASSERT_EQ(params_vector.size(), 1u);

        zwp_linux_buffer_params_v1_send_created(
            params_vector.front()->resource(),
            params_vector.front()->buffer_resource());
      });

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(surface_id));
  });

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/Damage/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceFactoryTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    CompositorVersionV3Test,
    WaylandSurfaceFactoryCompositorV3,
    Values(wl::ServerConfig{
        .compositor_version = wl::TestCompositor::Version::kV3}));

}  // namespace ui
