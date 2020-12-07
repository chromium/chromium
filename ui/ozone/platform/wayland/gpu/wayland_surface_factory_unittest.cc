// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_image_egl.h"
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

namespace ui {

namespace {

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
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    // The GLImage must be set busy as it has been scheduled before when
    // GbmSurfacelessWayland::ScheduleOverlayPlane was called.
    DCHECK(busy_);
    std::vector<gfx::GpuFence> acquire_fences;
    if (gpu_fence)
      acquire_fences.push_back(std::move(*gpu_fence));
    return pixmap_->ScheduleOverlayPlane(widget, z_order, transform,
                                         bounds_rect, crop_rect, enable_blend,
                                         std::move(acquire_fences), {});
  }
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
  ~WaylandSurfaceFactoryTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    auto manager_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(manager_ptr), {}, false, false);

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

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactoryTest);
};

TEST_P(WaylandSurfaceFactoryTest,
       GbmSurfacelessWaylandCommitOverlaysCallbacksTest) {
  // This tests multiple buffers per-frame and order of SwapCompletionCallbacks.
  // Even when all OnSubmission from later frames are called, their
  // SwapCompletionCallbacks should not run until previous frames'
  // SwapCompletionCallbacks run.
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->set_gbm_device(std::make_unique<MockGbmDevice>());

  auto* gl_ozone = surface_factory_->GetGLOzone(gl::kGLImplementationEGLGLES2);
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
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
        widget_, nullptr, window_->GetBounds().size(),
        gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT);
    fake_gl_image.push_back(base::MakeRefCounted<FakeGLImageNativePixmap>(
        native_pixmap, window_->GetBounds().size()));

    Sync();

    // Create one buffer at a time.
    auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
    DCHECK_EQ(params_vector.size(), 1u);
    zwp_linux_buffer_params_v1_send_created(
        params_vector.front()->resource(),
        params_vector.front()->buffer_resource());

    Sync();
  }

  auto* root_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_primary_surface = server_.GetObject<wl::MockSurface>(
      window_->primary_subsurface()->wayland_surface()->GetSurfaceId());

  CallbacksHelper cbs_helper;
  // Submit a frame with only primary plane
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[0]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[0].get(), window_->GetBounds(), {}, false, nullptr);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[0]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Let's sync so that 1) GbmSurfacelessWayland submits the buffer according to
  // internal queue and fake server processes the request.

  // Also, we expect only one buffer to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_primary_surface);

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

  // Submit another frame with only primary plane
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[1]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[1].get(), window_->GetBounds(), {}, false, nullptr);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[1]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Expect one buffer to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // Send the frame callback so that pending buffer for swap id=1u is processed
  // and swapped.
  mock_primary_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_primary_surface);

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // Even though the second buffer was submitted, we mustn't receive
  // SwapCompletionCallback until the previous buffer is released.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // Submit another frame with 2 overlays, 0 primary plane.
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
    gl_surface->ScheduleOverlayPlane(
        -1, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[2].get(), window_->GetBounds(), {}, false, nullptr);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[3]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        1, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[3].get(), window_->GetBounds(), {}, false, nullptr);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[2]);
    gl_images.push_back(fake_gl_image[3]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Do not expect parent surface to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(0);
  // Expect root surface to be committed.
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // Send the frame callback so that pending buffer for swap id=2u is processed
  // and swapped.
  mock_primary_surface->SendFrameCallback();

  Sync();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // Even though OnSubmission can come back because 2 overlays are submitted to
  // new wl_surfaces so OnSubmission for third frame has already run,
  // GbmSurfacelessWayland should not run SwapCompletionCallback out of order.
  EXPECT_EQ(cbs_helper.GetLastFinishedSwapId(),
            std::numeric_limits<uint32_t>::max());

  // This will result in Wayland server releasing previously attached buffer for
  // swap id=1u and calling OnSubmission for buffer with swap id=1u.
  mock_primary_surface->ReleaseBuffer(
      mock_primary_surface->prev_attached_buffer());

  Sync();

  // Give mojo the chance to pass the callbacks.
  base::RunLoop().RunUntilIdle();

  // We should expect next 2 SwapCompletionCallbacks for the next 2 swap ids
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
  // This tests multiple buffers per-frame. GbmSurfacelessWayland should wait
  // for all OnSubmission calls targeting the same frame before running
  // SwapCompletionCallbacks.
  gl::SetGLImplementation(gl::kGLImplementationEGLGLES2);

  buffer_manager_gpu_->set_gbm_device(std::make_unique<MockGbmDevice>());

  auto* gl_ozone = surface_factory_->GetGLOzone(gl::kGLImplementationEGLGLES2);
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
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
        widget_, nullptr, window_->GetBounds().size(),
        gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::SCANOUT);
    fake_gl_image.push_back(base::MakeRefCounted<FakeGLImageNativePixmap>(
        native_pixmap, window_->GetBounds().size()));

    Sync();

    // Create one buffer at a time.
    auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
    DCHECK_EQ(params_vector.size(), 1u);
    zwp_linux_buffer_params_v1_send_created(
        params_vector.front()->resource(),
        params_vector.front()->buffer_resource());

    Sync();
  }

  auto* root_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_primary_surface = server_.GetObject<wl::MockSurface>(
      window_->primary_subsurface()->wayland_surface()->GetSurfaceId());

  CallbacksHelper cbs_helper;
  // Submit a frame with 1 primary plane and 1 overlay
  {
    // Associate each image with swap id so that we could track released
    // buffers.
    auto swap_id = cbs_helper.GetNextLocalSwapId();
    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[0]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[0]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[0].get(), window_->GetBounds(), {}, false, nullptr);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[1]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[1]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        1, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[1].get(), window_->GetBounds(), {}, false, nullptr);

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

  // Let's sync so that 1) GbmSurfacelessWayland submits the buffer according to
  // internal queue and fake server processes the request.

  // Also, we expect primary buffer to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

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
    fake_gl_image[2]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[2]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[2].get(), window_->GetBounds(), {}, false, nullptr);

    // Associate the image with the next swap id so that we can easily track if
    // it became free to reuse.
    fake_gl_image[3]->AssociateWithSwapId(swap_id);
    // And set it to be busy...
    fake_gl_image[3]->SetBusy(true);

    // Prepare overlay plane.
    gl_surface->ScheduleOverlayPlane(
        1, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        fake_gl_image[3].get(), window_->GetBounds(), {}, false, nullptr);

    std::vector<scoped_refptr<FakeGLImageNativePixmap>> gl_images;
    gl_images.push_back(fake_gl_image[2]);
    gl_images.push_back(fake_gl_image[3]);

    // And submit each image. They will be executed in FIFO manner.
    gl_surface->SwapBuffersAsync(
        base::BindOnce(&CallbacksHelper::FinishSwapBuffersAsync,
                       base::Unretained(&cbs_helper), swap_id, gl_images),
        base::BindOnce(&CallbacksHelper::BufferPresented,
                       base::Unretained(&cbs_helper), swap_id));
  }

  // Expect primary buffer to be committed.
  EXPECT_CALL(*mock_primary_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_primary_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_primary_surface, Commit()).Times(1);

  // Expect overlay buffer to be committed.
  EXPECT_CALL(*mock_overlay_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, DamageBuffer(_, _, _, _)).Times(1);
  EXPECT_CALL(*mock_overlay_surface, Commit()).Times(1);

  // Expect root surface to be committed without buffer.
  EXPECT_CALL(*root_surface, Frame(_)).Times(0);
  EXPECT_CALL(*root_surface, Commit()).Times(1);

  // Send the frame callback so that pending buffer for swap id=1u is processed
  // and swapped.
  mock_overlay_surface->SendFrameCallback();
  mock_primary_surface->SendFrameCallback();

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
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBounds().size());
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
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 800);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 600);

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_P(WaylandSurfaceFactoryTest, CanvasResize) {
  auto canvas = CreateCanvas(widget_);
  ASSERT_TRUE(canvas);

  canvas->ResizeCanvas(window_->GetBounds().size());
  auto* sk_canvas = canvas->GetCanvas();
  DCHECK(sk_canvas);
  canvas->ResizeCanvas(gfx::Size(100, 50));
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

  // When gbm is not available, only canvas can be created with viz process
  // used.
  EXPECT_FALSE(buffer_manager_gpu_->gbm_device());

  auto* gl_ozone = surface_factory_->GetGLOzone(gl::kGLImplementationEGLGLES2);
  EXPECT_TRUE(gl_ozone);
  auto gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_FALSE(gl_surface);

  // Now, set gbm.
  buffer_manager_gpu_->set_gbm_device(std::make_unique<MockGbmDevice>());

  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_TRUE(gl_surface);

  // Reset gbm now. WaylandConnectionProxy can reset it when zwp is not
  // available. And factory must behave the same way as previously.
  buffer_manager_gpu_->set_gbm_device(nullptr);
  gl_surface = gl_ozone->CreateSurfacelessViewGLSurface(widget_);
  EXPECT_FALSE(gl_surface);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
