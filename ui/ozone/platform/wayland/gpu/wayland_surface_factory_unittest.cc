// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
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
#include "ui/gl/gl_image_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
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

// Fake GLImage that just schedules overlay plane. It must become busy when
// scheduled and be associated with the swap id to track correct order of swaps
// and releases of the image.
class FakeGLImageNativePixmap : public gl::GLImageEGL {
 public:
  FakeGLImageNativePixmap(scoped_refptr<gfx::NativePixmap> pixmap,
                          const gfx::Size& size)
      : gl::GLImageEGL(size), pixmap_(pixmap) {}

  // Associates swap id with this image.
  void AssociateWithSwapId(uint32_t swap_id) {
    DCHECK_NE(swap_id_, swap_id);
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

  // Overridden from GLImage:
  void Flush() override {}
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override {
    return pixmap_;
  }

 protected:
  ~FakeGLImageNativePixmap() override {}

 private:
  scoped_refptr<gfx::NativePixmap> pixmap_;

  // Indicated if the gl image is busy. If yes, it was scheduled as overlay
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
  // sets the associated gl_image as displayed and non-busy, which indicates
  // that 1) the image has been sent to be shown after being scheduled 2) the
  // image is displayed. This sort of mimics a buffer queue, but in a simpliear
  // way.
  void FinishSwapBuffersAsync(
      uint32_t local_swap_id,
      std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images,
      gfx::SwapCompletionResult result) {
    last_finish_swap_id_ = pending_local_swap_ids_.front();
    pending_local_swap_ids_.pop();

    for (auto& gl_image : gl_images) {
      EXPECT_EQ(gl_image->GetAssociateWithSwapId(), last_finish_swap_id_);
      EXPECT_TRUE(gl_image->busy() && !gl_image->displayed());
      gl_image->SetBusy(false);
      gl_image->SetDisplayed(true);
    }

    for (auto& displayed_image : displayed_images_)
      displayed_image->SetDisplayed(false);
    displayed_images_ = std::move(gl_images);
  }

  void BufferPresented(uint64_t local_swap_id,
                       const gfx::PresentationFeedback& feedback) {
    // Make sure the presentation doesn't come earlier than than swap
    // completion. We don't explicitly check if the buffer is presented as this
    // DCHECK is more that enough.
    DCHECK(pending_local_swap_ids_.empty() ||
           pending_local_swap_ids_.front() > local_swap_id);
  }

 private:
  uint32_t local_swap_id_ = 0;
  // Make sure that local_swap_id_ != last_finish_swap_id_.
  uint32_t last_finish_swap_id_ = std::numeric_limits<uint32_t>::max();
  base::queue<uint64_t> pending_local_swap_ids_;

  // Keeps track of a displayed image.
  std::vector<scoped_refptr<FakeGLImageNativePixmap>> displayed_images_;
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

    window_->set_update_visual_size_immediately_for_testing(false);
    window_->set_apply_pending_state_on_update_visual_size_for_testing(false);

    auto manager_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(
        std::move(manager_ptr), kSupportedFormatsWithModifiers,
        /*supports_dma_buf=*/false,
        /*supports_viewporter=*/true,
        /*supports_acquire_fence=*/false, kAugmentedSurfaceNotSupportedVersion);

    // Wait until initialization and mojo calls go through.
    base::RunLoop().RunUntilIdle();
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

  void ScheduleOverlayPlane(gl::GLSurface* gl_surface,
                            gl::GLImage* image,
                            int z_order) {
    gl_surface->ScheduleOverlayPlane(
        image, nullptr,
        gfx::OverlayPlaneData(z_order,
                              gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
                              gfx::RectF(window_->GetBoundsInPixels()), {},
                              false, gfx::Rect(window_->size_px()), 1.0f,
                              gfx::OverlayPriorityHint::kNone, gfx::RRectF(),
                              gfx::ColorSpace::CreateSRGB(), absl::nullopt));
  }
};

TEST_P(WaylandSurfaceFactoryTest,
       GbmSurfacelessWaylandCommitOverlaysCallbacksTest) {
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
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_TRUE(gl_surface);
  gl_surface->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(gl_surface.get())
      ->SetNoGLFlushForTests();

  // Expect to create 4 buffers.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(4);

  // Create buffers and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<FakeGLImageNativePixmap>> fake_gl_image;
  for (int i = 0; i < 4; ++i) {
    auto native_pixmap = surface_factory_->CreateNativePixmap(
        widget_, nullptr, window_->size_px(), gfx::BufferFormat::BGRA_8888,
        gfx::BufferUsage::SCANOUT);
    fake_gl_image.push_back(base::MakeRefCounted<FakeGLImageNativePixmap>(
        native_pixmap, window_->size_px()));
  }

  auto* root_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_primary_surface = server_.GetObject<wl::MockSurface>(
      window_->primary_subsurface()->wayland_surface()->GetSurfaceId());

  CallbacksHelper cbs_helper;
  // Submit a frame with an overlay and background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[0]->SetBusy(true);

    // Prepare background.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[0].get(),
                         /*z_order=*/INT32_MIN);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[1]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[1].get(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[0]);
    gl_images.push_back(fake_gl_image[1]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  Sync();

  // Let's sync so that 1) GbmSurfacelessWayland submits the buffer according to
  // internal queue and fake server processes the request.

  // Also, we expect no buffer committed on primary subsurface.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(0);
  // 1 buffer committed on root surface.
  EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // The wl_buffers are requested during ScheduleOverlays. Thus, we have pending
  // requests that we need to execute.
  auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 2u);
  for (auto* mock_params : params_vector) {
    zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                            mock_params->buffer_resource());
  }

  Sync();

  testing::Mock::VerifyAndClearExpectations(mock_primary_surface);
  testing::Mock::VerifyAndClearExpectations(root_surface);

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/DamageBuffer/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& gl_image : fake_gl_image) {
    // All the images except the first one, which was associated with swap
    // id=0u, must be busy and not displayed. The first one must be displayed.
    if (gl_image->GetAssociateWithSwapId() == 0u) {
      EXPECT_FALSE(gl_image->busy());
      EXPECT_TRUE(gl_image->displayed());
    } else {
      EXPECT_FALSE(gl_image->displayed());
    }
  }

  auto* mock_overlay_surface = server_.GetObject<wl::MockSurface>(
      (*window_->wayland_subsurfaces().begin())
          ->wayland_surface()
          ->GetSurfaceId());

  // Submit another frame with only an overlay.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[2]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[2]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[2].get(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[2]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Expect no buffer committed on primary subsurface.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(0);
  // Expect 1 buffer to be committed on overlay subsurface, with frame callback.
  EXPECT_CALL(*mock_overlay_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Commit()).Times(1);
  // Expect no buffer committed on root surface.
  EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  Sync();

  auto params_vector2 = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 2u);
  for (auto* mock_params : params_vector2) {
    zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                            mock_params->buffer_resource());
  }

  Sync();

  // Send the frame callback so that pending buffer for swap id=1u is processed
  // and swapped.
  mock_overlay_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(mock_primary_surface);
  testing::Mock::VerifyAndClearExpectations(mock_overlay_surface);
  testing::Mock::VerifyAndClearExpectations(root_surface);

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
    fake_gl_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[3]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[3].get(),
                         /*z_order=*/0);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[3]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Expect 1 buffer committed on primary subsurface, with frame callback.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
  // Expect no buffer to be committed on overlay subsurface.
  EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_overlay_surface, DamageBuffer(_, _, _, _)).Times(0);
  // Expect no buffer committed on root surface.
  EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  Sync();

  auto params_vector3 = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 2u);
  for (auto* mock_params : params_vector3) {
    zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                            mock_params->buffer_resource());
  }

  Sync();

  // Send the frame callback so that pending buffer for swap id=2u is processed
  // and swapped.
  mock_overlay_surface->SendFrameCallback();

  // Release overlay image with swap id=1u before swap id=2u.
  mock_overlay_surface->ReleaseBuffer(mock_overlay_surface->attached_buffer());

  Sync();

  testing::Mock::VerifyAndClearExpectations(mock_primary_surface);
  testing::Mock::VerifyAndClearExpectations(mock_overlay_surface);
  testing::Mock::VerifyAndClearExpectations(root_surface);

  // Even 2nd frame is released, we do not send OnSubmission() out of order.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // This will result in Wayland server releasing previously attached buffer for
  // swap id=1u and calling OnSubmission for buffer with swap id=1u.
  mock_overlay_surface->ReleaseBuffer(
      mock_overlay_surface->prev_attached_buffer());

  Sync();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We should expect next 2 SwapCompletionCallbacks for the first 2 swap ids
  // consecutively.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 2u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& gl_image : fake_gl_image) {
    if (gl_image->GetAssociateWithSwapId() == 2u) {
      EXPECT_TRUE(gl_image->displayed());
      EXPECT_FALSE(gl_image->busy());
    } else {
      EXPECT_FALSE(gl_image->displayed());
    }
  }
}

TEST_P(WaylandSurfaceFactoryTest,
       GbmSurfacelessWaylandGroupOnSubmissionCallbacksTest) {
  // This tests multiple buffers per-frame. GbmSurfacelessWayland receive 1
  // OnSubmission call per frame before running in submission order.
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->use_fake_gbm_device_for_test_ = true;
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();
  buffer_manager_gpu_->supports_dmabuf_ = true;

  auto* gl_ozone = surface_factory_->GetGLOzone(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_TRUE(gl_surface);
  gl_surface->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(gl_surface.get())
      ->SetNoGLFlushForTests();

  // Create buffers and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<FakeGLImageNativePixmap>> fake_gl_image;
  for (int i = 0; i < 5; ++i) {
    auto native_pixmap = surface_factory_->CreateNativePixmap(
        widget_, nullptr, window_->size_px(), gfx::BufferFormat::BGRA_8888,
        gfx::BufferUsage::SCANOUT);
    fake_gl_image.push_back(base::MakeRefCounted<FakeGLImageNativePixmap>(
        native_pixmap, window_->size_px()));
  }

  auto* root_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_primary_surface = server_.GetObject<wl::MockSurface>(
      window_->primary_subsurface()->wayland_surface()->GetSurfaceId());

  CallbacksHelper cbs_helper;
  // Submit a frame with 1 primary plane, 1 underlay, and 1 background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[0]->SetBusy(true);

    // Prepare background.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[0].get(),
                         /*z_order=*/INT32_MIN);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[1]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[1].get(),
                         /*z_order=*/0);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[2]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[2]->SetBusy(true);

    // Prepare underlay plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[2].get(),
                         /*z_order=*/-1);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[0]);
    gl_images.push_back(fake_gl_image[1]);
    gl_images.push_back(fake_gl_image[2]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Let's sync so that 1) GbmSurfacelessWayland submits the buffer according to
  // internal queue and fake server processes the request.

  // Also, we expect primary buffer to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // Expect to create 3 buffers.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);

  Sync();

  // If wl_buffers have never been created, they will be requested during the
  // first commit.
  auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 3u);
  for (auto* param : params_vector) {
    zwp_linux_buffer_params_v1_send_created(param->resource(),
                                            param->buffer_resource());
  }

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_primary_surface);
  auto* subsurface = window_->wayland_subsurfaces().begin()->get();
  auto* mock_overlay_surface = server_.GetObject<wl::MockSurface>(
      subsurface->wayland_surface()->GetSurfaceId());

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/DamageBuffer/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();

  for (const auto& gl_image : fake_gl_image) {
    // All the images except the first one, which was associated with swap
    // id=0u, must be busy and not displayed. The first one must be displayed.
    if (gl_image->GetAssociateWithSwapId() == 0u) {
      EXPECT_FALSE(gl_image->busy());
      EXPECT_TRUE(gl_image->displayed());
    } else {
      EXPECT_FALSE(gl_image->displayed());
    }
  }

  // Submit another frame with 1 primary plane and 1 overlay
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[3]->SetBusy(true);

    // Prepare primary plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[3].get(),
                         /*z_order=*/0);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[4]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[4]->SetBusy(true);

    // Prepare overlay plane.
    ScheduleOverlayPlane(gl_surface.get(), fake_gl_image[4].get(),
                         /*z_order=*/1);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[3]);
    gl_images.push_back(fake_gl_image[4]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Expect primary buffer to be committed, but since it is not the top-most
  // surface in the frame it does not setup frame callback.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);

  // Expect overlay buffer to be committed, and sets up frame callback.
  EXPECT_CALL(*mock_overlay_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Commit()).Times(1);

  // Expect root surface to be committed without buffer.
  EXPECT_CALL(*root_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // Expect to create 2 more buffers.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);

  Sync();

  // 2 more buffers are to be created.
  params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 2u);
  for (auto* param : params_vector) {
    zwp_linux_buffer_params_v1_send_created(param->resource(),
                                            param->buffer_resource());
  }

  Sync();

  // Send the frame callback so that pending buffer for swap id=1u is processed
  // and swapped.
  mock_primary_surface->SendFrameCallback();
  mock_overlay_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_primary_surface);

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // Even though the second frame was submitted, we mustn't receive
  // SwapCompletionCallback until the previous frame's buffers are released.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // This will result in Wayland server releasing one of the previously attached
  // buffers for swap id=0u and calling OnSubmission for a buffer with swap
  // id=1u attached to the primary surface.
  mock_primary_surface->ReleaseBuffer(
      mock_primary_surface->prev_attached_buffer());

  Sync();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // OnSubmission was only called for one of the buffers with swap id=1u, so
  // SwapCompletionCallback should not run.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // Release the another buffer.
  mock_overlay_surface->ReleaseBuffer(
      mock_overlay_surface->prev_attached_buffer());

  Sync();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // OnSubmission was called for both of the buffers with swap id=1u, so
  // SwapCompletionCallback should run.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 1u);

  for (const auto& gl_image : fake_gl_image) {
    if (gl_image->GetAssociateWithSwapId() == 1u) {
      EXPECT_TRUE(gl_image->displayed());
      EXPECT_FALSE(gl_image->busy());
    } else {
      EXPECT_FALSE(gl_image->displayed());
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
    DCHECK(sk_canvas);
    canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));

    // Wait until the mojo calls are done.
    base::RunLoop().RunUntilIdle();

    Expectation damage = EXPECT_CALL(*surface_, DamageBuffer(5, 10, 20, 15));
    wl_resource* buffer_resource = nullptr;
    Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                             .WillOnce(SaveArg<0>(&buffer_resource));
    EXPECT_CALL(*surface_, Commit()).After(damage, attach);

    Sync();

    ASSERT_TRUE(buffer_resource);
    wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
    ASSERT_TRUE(buffer);
    EXPECT_EQ(wl_shm_buffer_get_width(buffer), bounds_px.width());
    EXPECT_EQ(wl_shm_buffer_get_height(buffer), bounds_px.height());

    // Release the buffer immediately as the test always attaches the same
    // buffer.
    surface_->ReleaseBufferFenced(buffer_resource, {});

    surface_->SendFrameCallback();

    Sync();
  }

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_P(WaylandSurfaceFactoryTest, CanvasResize) {
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBoundsInDIP().size(), 1);
  auto* sk_canvas = canvas->GetCanvas();
  DCHECK(sk_canvas);
  canvas->ResizeCanvas(gfx::Size(100, 50), 1);
  sk_canvas = canvas->GetCanvas();
  DCHECK(sk_canvas);
  canvas->PresentCanvas(gfx::Rect(0, 0, 100, 50));

  base::RunLoop().RunUntilIdle();

  Expectation damage = EXPECT_CALL(*surface_, DamageBuffer(0, 0, 100, 50));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 100);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 50);
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
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_FALSE(gl_surface);

  // Now, set gbm.
  buffer_manager_gpu_->gbm_device_ = std::make_unique<MockGbmDevice>();

  // It's still impossible to create the device if supports_dmabuf is false.
  EXPECT_FALSE(buffer_manager_gpu_->GetGbmDevice());
  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                        widget_);
  EXPECT_FALSE(gl_surface);

  // Now set supports_dmabuf.
  buffer_manager_gpu_->supports_dmabuf_ = true;
  EXPECT_TRUE(buffer_manager_gpu_->GetGbmDevice());
  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                        widget_);
  EXPECT_TRUE(gl_surface);

  // Reset gbm now. WaylandConnectionProxy can reset it when zwp is not
  // available. And factory must behave the same way as previously.
  buffer_manager_gpu_->gbm_device_ = nullptr;
  EXPECT_FALSE(buffer_manager_gpu_->GetGbmDevice());
  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(gl::GetDefaultDisplay(),
                                                        widget_);
  EXPECT_FALSE(gl_surface);
}

class WaylandSurfaceFactoryCompositorV3 : public WaylandSurfaceFactoryTest {};

TEST_P(WaylandSurfaceFactoryCompositorV3, SurfaceDamageTest) {
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
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(
      gl::GetDefaultDisplay(), widget_);
  EXPECT_TRUE(gl_surface);
  gl_surface->SetRelyOnImplicitSync();
  static_cast<ui::GbmSurfacelessWayland*>(gl_surface.get())
      ->SetNoGLFlushForTests();

  // Expect to create 4 buffers.
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);

  gfx::Size test_buffer_size = {300, 100};
  gfx::RectF test_buffer_dmg_uv = {0.2f, 0.3f, 0.6, 0.32f};
  gfx::Rect test_buffer_dmg = gfx::ToEnclosingRect(gfx::ScaleRect(
      test_buffer_dmg_uv, test_buffer_size.width(), test_buffer_size.height()));
  gfx::RectF crop_uv = {0.1f, 0.2f, 0.5, 0.5f};
  gfx::RectF expected_combined_uv = {0.2, 0.2, 0.8, 0.64};
  gfx::Rect expected_surface_dmg = gfx::ToEnclosingRect(
      gfx::ScaleRect(expected_combined_uv, window_->size_px().width(),
                     window_->size_px().height()));

  // Create buffers and FakeGlImageNativePixmap.
  std::vector<scoped_refptr<FakeGLImageNativePixmap>> fake_gl_image;
  auto native_pixmap = surface_factory_->CreateNativePixmap(
      widget_, nullptr, test_buffer_size, gfx::BufferFormat::BGRA_8888,
      gfx::BufferUsage::SCANOUT);
  fake_gl_image.push_back(base::MakeRefCounted<FakeGLImageNativePixmap>(
      native_pixmap, test_buffer_size));

  auto* root_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_primary_surface = server_.GetObject<wl::MockSurface>(
      window_->primary_subsurface()->wayland_surface()->GetSurfaceId());

  CallbacksHelper cbs_helper;
  // Submit a frame with an overlay and background.
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[0]->SetBusy(true);

    // Prepare background.
    gl_surface->ScheduleOverlayPlane(
        fake_gl_image[0].get(), nullptr,
        gfx::OverlayPlaneData(
            INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
            gfx::RectF(window_->GetBoundsInPixels()), crop_uv, false,
            gfx::Rect(test_buffer_dmg), 1.0f, gfx::OverlayPriorityHint::kNone,
            gfx::RRectF(), gfx::ColorSpace::CreateSRGB(), absl::nullopt));

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[0]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Wait until the mojo calls are done.
  base::RunLoop().RunUntilIdle();

  Expectation damage =
      EXPECT_CALL(*surface_, Damage(expected_surface_dmg.origin().x(),
                                    expected_surface_dmg.origin().y(),
                                    expected_surface_dmg.width(),
                                    expected_surface_dmg.height()));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  // Let's sync so that 1) GbmSurfacelessWayland submits the buffer according to
  // internal queue and fake server processes the request.
  Sync();

  auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
  DCHECK_EQ(params_vector.size(), 1u);

  zwp_linux_buffer_params_v1_send_created(
      params_vector.front()->resource(),
      params_vector.front()->buffer_resource());

  // And create buffers.
  Sync();

  testing::Mock::VerifyAndClearExpectations(mock_primary_surface);
  testing::Mock::VerifyAndClearExpectations(root_surface);

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We have just received Attach/DamageBuffer/Commit for buffer with swap
  // id=0u. The SwapCompletionCallback must be executed automatically as long as
  // we didn't have any buffers attached to the surface before.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(), 0u);

  cbs_helper.ResetLastFinishedSwapId();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceFactoryTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandSurfaceFactoryTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

INSTANTIATE_TEST_SUITE_P(
    CompositorVersionV3Test,
    WaylandSurfaceFactoryCompositorV3,
    Values(wl::ServerConfig{.compositor_version = wl::CompositorVersion::kV3}));

}  // namespace ui
