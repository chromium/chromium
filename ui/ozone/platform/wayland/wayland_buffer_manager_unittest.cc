// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <drm_fourcc.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::_;

namespace ui {

namespace {

using MockTerminateGpuCallback =
    base::MockCallback<base::OnceCallback<void(std::string)>>;

constexpr gfx::Size kDefaultSize(1024, 768);

struct InputData {
  bool has_file = false;
  gfx::Size size;
  uint32_t planes_count = 0;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;
  uint32_t format = 0;
  uint32_t buffer_id = 0;
};

class MockSurfaceGpu : public WaylandSurfaceGpu {
 public:
  MockSurfaceGpu(WaylandBufferManagerGpu* buffer_manager,
                 gfx::AcceleratedWidget widget)
      : buffer_manager_(buffer_manager), widget_(widget) {
    buffer_manager_->RegisterSurface(widget_, this);
  }
  ~MockSurfaceGpu() { buffer_manager_->UnregisterSurface(widget_); }

  MOCK_METHOD2(OnSubmission,
               void(uint32_t buffer_id, const gfx::SwapResult& swap_result));
  MOCK_METHOD2(OnPresentation,
               void(uint32_t buffer_id,
                    const gfx::PresentationFeedback& feedback));

 private:
  WaylandBufferManagerGpu* const buffer_manager_;
  const gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(MockSurfaceGpu);
};

}  // namespace

class WaylandBufferManagerTest : public WaylandTest {
 public:
  WaylandBufferManagerTest() = default;
  ~WaylandBufferManagerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();

    manager_host_ = connection_->buffer_manager_host();
    EXPECT_TRUE(manager_host_);

    // Use the helper methods below, which automatically set the termination
    // callback and bind the interface again if the manager failed.
    manager_host_->SetTerminateGpuCallback(callback_.Get());
    auto interface_ptr = manager_host_->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(interface_ptr), {}, false, false);
  }

 protected:
  base::ScopedFD MakeFD() {
    base::FilePath temp_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
    auto file =
        base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                  base::File::FLAG_CREATE_ALWAYS);
    return base::ScopedFD(file.TakePlatformFile());
  }

  // Sets the terminate gpu callback expectation, calls OnChannelDestroyed,
  // sets the same callback again and re-establishes mojo connection again
  // for convenience.
  void SetTerminateCallbackExpectationAndDestroyChannel(
      MockTerminateGpuCallback* callback,
      bool fail) {
    if (!fail) {
      // To avoid warning messages as "Expected to be never called, but has 0
      // WillOnce()s", split the expecations based on the expected call times.
      EXPECT_CALL(*callback, Run(_)).Times(0);
    } else {
      EXPECT_CALL(*callback, Run(_))
          .Times(1)
          .WillRepeatedly(::testing::Invoke([this, callback](std::string) {
            manager_host_->OnChannelDestroyed();

            manager_host_->SetTerminateGpuCallback(callback->Get());

            auto interface_ptr = manager_host_->BindInterface();
            // Recreate the gpu side manager (the production code does the
            // same).
            buffer_manager_gpu_ = std::make_unique<WaylandBufferManagerGpu>();
            buffer_manager_gpu_->Initialize(std::move(interface_ptr), {}, false,
                                            false);
          }));
    }
  }

  void CreateDmabufBasedBufferAndSetTerminateExpectation(
      bool fail,
      uint32_t buffer_id,
      base::ScopedFD fd = base::ScopedFD(),
      const gfx::Size& size = kDefaultSize,
      const std::vector<uint32_t>& strides = {1},
      const std::vector<uint32_t>& offsets = {2},
      const std::vector<uint64_t>& modifiers = {3},
      uint32_t format = DRM_FORMAT_R8,
      uint32_t planes_count = 1) {
    if (!fd.is_valid())
      fd = MakeFD();

    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);
    buffer_manager_gpu_->CreateDmabufBasedBuffer(
        std::move(fd), kDefaultSize, strides, offsets, modifiers, format,
        planes_count, buffer_id);

    Sync();
  }

  void CreateShmBasedBufferAndSetTerminateExpecation(
      bool fail,
      uint32_t buffer_id,
      const gfx::Size& size = kDefaultSize,
      size_t length = 0) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    if (!length)
      length = size.width() * size.height() * 4;
    buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, size,
                                              buffer_id);

    Sync();
  }

  void DestroyBufferAndSetTerminateExpectation(gfx::AcceleratedWidget widget,
                                               uint32_t buffer_id,
                                               bool fail) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    buffer_manager_gpu_->DestroyBuffer(widget, buffer_id);

    Sync();
  }

  void ProcessCreatedBufferResourcesWithExpectation(size_t expected_size,
                                                    bool fail) {
    auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
    // To ensure, no other buffers are created, test the size of the vector.
    EXPECT_EQ(params_vector.size(), expected_size);

    for (auto* mock_params : params_vector) {
      if (!fail) {
        zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                                mock_params->buffer_resource());
      } else {
        zwp_linux_buffer_params_v1_send_failed(mock_params->resource());
      }
    }
  }

  std::unique_ptr<WaylandWindow> CreateWindow(
      PlatformWindowType type = PlatformWindowType::kWindow,
      gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget) {
    testing::Mock::VerifyAndClearExpectations(&delegate_);
    PlatformWindowInitProperties properties;
    properties.bounds = gfx::Rect(0, 0, 800, 600);
    properties.type = type;
    properties.parent_widget = parent_widget;
    auto new_window = WaylandWindow::Create(&delegate_, connection_.get(),
                                            std::move(properties));
    EXPECT_TRUE(new_window);

    Sync();

    EXPECT_NE(new_window->GetWidget(), gfx::kNullAcceleratedWidget);
    return new_window;
  }

  MockTerminateGpuCallback callback_;
  WaylandBufferManagerHost* manager_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerTest);
};

TEST_P(WaylandBufferManagerTest, CreateDmabufBasedBuffers) {
  constexpr uint32_t kDmabufBufferId = 1;

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);
  DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                          kDmabufBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, VerifyModifiers) {
  constexpr uint32_t kDmabufBufferId = 1;
  constexpr uint32_t kFourccFormatR8 = DRM_FORMAT_R8;
  constexpr uint64_t kFormatModiferLinear = DRM_FORMAT_MOD_LINEAR;

  const std::vector<uint64_t> kFormatModifiers{DRM_FORMAT_MOD_INVALID,
                                               kFormatModiferLinear};

  // Tests that fourcc format is added, but invalid modifier is ignored first.
  // Then, when valid modifier comes, it is stored.
  for (const auto& modifier : kFormatModifiers) {
    uint32_t modifier_hi = modifier >> 32;
    uint32_t modifier_lo = modifier & UINT32_MAX;
    zwp_linux_dmabuf_v1_send_modifier(server_.zwp_linux_dmabuf_v1()->resource(),
                                      kFourccFormatR8, modifier_hi,
                                      modifier_lo);

    Sync();

    auto buffer_formats = connection_->zwp_dmabuf()->supported_buffer_formats();
    DCHECK_EQ(buffer_formats.size(), 1u);
    DCHECK_EQ(buffer_formats.begin()->first,
              GetBufferFormatFromFourCCFormat(kFourccFormatR8));

    auto modifiers = buffer_formats.begin()->second;
    if (modifier == DRM_FORMAT_MOD_INVALID) {
      DCHECK_EQ(modifiers.size(), 0u);
    } else {
      DCHECK_EQ(modifiers.size(), 1u);
      DCHECK_EQ(modifiers[0], modifier);
    }
  }

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);

  CreateDmabufBasedBufferAndSetTerminateExpectation(
      false /*fail*/, kDmabufBufferId, base::ScopedFD(), kDefaultSize, {1}, {2},
      {kFormatModiferLinear}, kFourccFormatR8, 1);

  Sync();

  auto params_vector = server_.zwp_linux_dmabuf_v1()->buffer_params();
  EXPECT_EQ(params_vector.size(), 1u);
  EXPECT_EQ(params_vector[0]->modifier_hi_, kFormatModiferLinear >> 32);
  EXPECT_EQ(params_vector[0]->modifier_lo_, kFormatModiferLinear & UINT32_MAX);

  // Clean up.
  DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                          kDmabufBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateShmBasedBuffers) {
  constexpr uint32_t kShmBufferId = 1;

  CreateShmBasedBufferAndSetTerminateExpecation(false /*fail*/, kShmBufferId);

  DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                          kShmBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, ValidateDataFromGpu) {
  const InputData kBadInputs[] = {
      // All zeros.
      {},
      // Valid file but zeros everywhereelse.
      {true},
      // Valid file, invalid size, zeros elsewhere.
      {true, {kDefaultSize.width(), 0}},
      {true, {0, kDefaultSize.height()}},
      // Valid file and size but zeros in other fields.
      {true, kDefaultSize},
      // Vectors have different lengths.
      {true, kDefaultSize, 1, {1}, {2, 3}, {4, 5, 6}},
      // Vectors have same lengths but strides have a zero.
      {true, kDefaultSize, 1, {0}, {2}, {6}},
      // Vectors are valid but buffer format is not.
      {true, kDefaultSize, 1, {1}, {2}, {6}},
      // Everything is correct but the buffer ID is zero.
      {true, kDefaultSize, 1, {1}, {2}, {6}, DRM_FORMAT_R8},
  };

  for (const auto& bad : kBadInputs) {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
    base::ScopedFD dummy;
    CreateDmabufBasedBufferAndSetTerminateExpectation(
        true /*fail*/, bad.buffer_id,
        bad.has_file ? MakeFD() : std::move(dummy), bad.size, bad.strides,
        bad.offsets, bad.modifiers, bad.format, bad.planes_count);
  }
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  // This section tests that it is impossible to create buffers with the same
  // id if they haven't been assigned to any surfaces yet.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);

    // Can't create buffer with existing id.
    CreateDmabufBasedBufferAndSetTerminateExpectation(true /*fail*/,
                                                      kBufferId2);
  }

  // ... impossible to create buffers with the same id if one of them
  // has already been attached to a surface.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);

    buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, window_->GetBounds(),
                                      window_->GetBounds());

    CreateDmabufBasedBufferAndSetTerminateExpectation(true /*fail*/,
                                                      kBufferId1);
  }

  // ... impossible to destroy non-existing buffer.
  {
    // Either it is attached...
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, true /*fail*/);

    // Or not attached.
    DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                            kBufferId1, true /*fail*/);
  }

  // ... impossible to try to destroy an attached buffer if the widget is not
  // specified.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);

    buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, window_->GetBounds(),
                                      window_->GetBounds());

    DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                            kBufferId1, true /*fail*/);
  }

  // Still can destroy the buffer even if it has not been attached to any
  // widgets.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  }

  // ... impossible to destroy buffers twice.
  {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    // Attach to a surface.
    buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, window_->GetBounds(),
                                      window_->GetBounds());

    // Created non-attached buffer as well.
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);

    DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
    // Can't destroy the buffer with non-existing id (the manager cleared the
    // state after the previous failure).
    DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, true /*fail*/);

    // Non-attached buffer must have been also destroyed (we can't destroy it
    // twice) if there was a failure.
    DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                            kBufferId2, true /*fail*/);

    // Create and destroy non-attached buffer twice.
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);
    DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                            kBufferId2, false /*fail*/);
    DestroyBufferAndSetTerminateExpectation(gfx::kNullAcceleratedWidget,
                                            kBufferId2, true /*fail*/);
  }
}

TEST_P(WaylandBufferManagerTest, CommitBufferNonExistingBufferId) {
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, 1u);

  // Can't commit for non-existing buffer id.
  SetTerminateCallbackExpectationAndDestroyChannel(&callback_, true /*fail*/);
  buffer_manager_gpu_->CommitBuffer(window_->GetWidget(), 5u,
                                    window_->GetBounds(), window_->GetBounds());

  Sync();
}

TEST_P(WaylandBufferManagerTest, CommitOverlaysNonExistingBufferId) {
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, 1u);

  // Can't commit for non-existing buffer id.
  SetTerminateCallbackExpectationAndDestroyChannel(&callback_, true /*fail*/);

  std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlay_configs;
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, 1u,
      window_->GetBounds(), gfx::RectF(), window_->GetBounds(), false,
      gfx::GpuFenceHandle()));

  // Non-existing buffer id
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, 2u,
      window_->GetBounds(), gfx::RectF(), window_->GetBounds(), false,
      gfx::GpuFenceHandle()));

  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(),
                                      std::move(overlay_configs));

  Sync();
}

TEST_P(WaylandBufferManagerTest, CommitBufferNullWidget) {
  constexpr uint32_t kBufferId = 1;
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  // Can't commit for non-existing widget.
  SetTerminateCallbackExpectationAndDestroyChannel(&callback_, true /*fail*/);
  buffer_manager_gpu_->CommitBuffer(gfx::kNullAcceleratedWidget, kBufferId,
                                    window_->GetBounds(), window_->GetBounds());

  Sync();
}

TEST_P(WaylandBufferManagerTest, EnsureCorrectOrderOfCallbacks) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBounds(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  constexpr uint32_t kNumberOfCommits = 3;
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  // wp_presentation must not exist now. This means that the buffer
  // manager must send synthetized presentation feedbacks.
  ASSERT_TRUE(!connection_->presentation());
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  // As long as there hasn't any previous buffer attached (nothing to release
  // yet), it must be enough to just send a frame callback back.
  mock_surface->SendFrameCallback();

  Sync();

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  mock_surface->SendFrameCallback();

  Sync();

  // wp_presentation is available now.
  auto* mock_wp_presentation = server_.EnsureWpPresentation();
  ASSERT_TRUE(mock_wp_presentation);

  Sync();

  // Now, the wp_presentation object exists and there must be a real feedback
  // sent. Ensure the order now.
  ASSERT_TRUE(connection_->presentation());

  EXPECT_CALL(*mock_wp_presentation,
              Feedback(_, _, mock_surface->resource(), _))
      .Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  // Even though, the server send the presentation feeedback, the host manager
  // must make sure the order of the submission and presentation callbacks is
  // correct. Thus, no callbacks must be received by the MockSurfaceGpu.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  mock_wp_presentation->SendPresentationCallback();

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  // Now, send the release callback. The host manager must send the submission
  // and presentation callbacks in correct order.
  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest,
       DestroyedBuffersGeneratePresentationFeedbackFailure) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBounds(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(3);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  auto* mock_wp_presentation = server_.EnsureWpPresentation();
  ASSERT_TRUE(mock_wp_presentation);

  constexpr uint32_t kNumberOfCommits = 3;
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_wp_presentation,
              Feedback(_, _, mock_surface->resource(), _))
      .Times(3);

  Sync();

  ::testing::InSequence s;

  // wp_presentation_feedback should work now.
  ASSERT_TRUE(connection_->presentation());

  // Commit the first buffer and expect OnSubmission immediately.
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  // Deliberately drop the presentation feedback for the first buffer,
  // since we will destroy it.
  mock_wp_presentation->set_presentation_callback(nullptr);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  // Destroy the first buffer, which should trigger submission for the second
  // buffer.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, /*fail=*/false);
  mock_surface->DestroyPrevAttachedBuffer();
  mock_surface->SendFrameCallback();
  Sync();

  // Deliberately drop the presentation feedback for the second buffer,
  // since we will destroy it.
  mock_wp_presentation->set_presentation_callback(nullptr);

  // Commit buffer 3 then send the presentation callback for it. This should
  // not call OnPresentation as OnSubmission hasn't been called yet.
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId3, bounds, bounds);
  mock_surface->SendFrameCallback();
  mock_wp_presentation->SendPresentationCallback();
  Sync();

  // Destroy buffer 2, which should trigger OnSubmission for buffer 3, and
  // OnPresentation for buffer 1, 2, and 3.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId1,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId2,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, /*fail=*/false);
  mock_surface->DestroyPrevAttachedBuffer();
  mock_surface->SendFrameCallback();
  mock_wp_presentation->SendPresentationCallback();
  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId3, false /*fail*/);
}

// This test ensures that a discarded presentation feedback sent prior receiving
// results for the previous presentation feedback does not make them
// automatically failed.
TEST_P(WaylandBufferManagerTest,
       EnsureDiscardedPresentationDoesNotMakePreviousFeedbacksFailed) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  // Enable wp_presentation support.
  auto* mock_wp_presentation = server_.EnsureWpPresentation();
  ASSERT_TRUE(mock_wp_presentation);

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBounds(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(3);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  constexpr uint32_t kNumberOfCommits = 3;
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit first buffer
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  // Will be sent later.
  auto* presentation_callback1 =
      mock_wp_presentation->ReleasePresentationCallback();

  mock_surface->SendFrameCallback();

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit second buffer
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  // Will be sent later.
  auto* presentation_callback2 =
      mock_wp_presentation->ReleasePresentationCallback();

  // Release previous buffer and commit third buffer.
  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  mock_surface->SendFrameCallback();

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit third buffer
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId3, bounds, bounds);

  Sync();

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());

  Sync();

  // Even though WaylandBufferManagerHost stores the previous stores
  // presentation feedbacks and waits for their value, the current last one
  // mustn't result in the previous marked as failed. Thus, no feedback must be
  // actually sent to the MockSurfaceGpu as it's required to send feedbacks in
  // order.
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  mock_wp_presentation->SendPresentationCallbackDiscarded();

  Sync();

  // Now, start to send all the previous callbacks.
  EXPECT_CALL(mock_surface_gpu,
              OnPresentation(
                  kBufferId1,
                  ::testing::Field(
                      &gfx::PresentationFeedback::flags,
                      ::testing::Eq(gfx::PresentationFeedback::Flags::kVSync))))
      .Times(1);

  mock_wp_presentation->set_presentation_callback(presentation_callback1);
  mock_wp_presentation->SendPresentationCallback();

  Sync();

  // Now, send the second presentation feedback. It will send both second and
  // third feedback that was discarded.
  EXPECT_CALL(mock_surface_gpu,
              OnPresentation(
                  kBufferId2,
                  ::testing::Field(
                      &gfx::PresentationFeedback::flags,
                      ::testing::Eq(gfx::PresentationFeedback::Flags::kVSync))))
      .Times(1);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId3,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);

  mock_wp_presentation->set_presentation_callback(presentation_callback2);
  mock_wp_presentation->SendPresentationCallback();

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId3, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, TestCommitBufferConditions) {
  constexpr uint32_t kDmabufBufferId = 1;
  constexpr uint32_t kDmabufBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);

  // Part 1: the surface mustn't have a buffer attached until
  // zwp_linux_buffer_params_v1_send_created is called. Instead, the buffer must
  // be set as pending buffer.

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kDmabufBufferId,
                                    window_->GetBounds(), window_->GetBounds());
  Sync();

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  // Once the client receives a "...send_created" call, it must destroy the
  // params resource.
  EXPECT_TRUE(linux_dmabuf->buffer_params().empty());

  // Part 2: the surface mustn't have a buffer attached until frame callback is
  // sent by the server.

  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kDmabufBufferId2,
                                    window_->GetBounds(), window_->GetBounds());

  Sync();

  // After the frame callback is sent, the pending buffer will be committed.
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  mock_surface->SendFrameCallback();

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kDmabufBufferId,
                                          false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kDmabufBufferId2,
                                          false /*fail*/);
}

// Tests the surface does not have buffers attached until it's configured at
// least once.
TEST_P(WaylandBufferManagerTest, TestCommitBufferConditionsAckConfigured) {
  constexpr uint32_t kDmabufBufferId = 1;

  // Exercise three window types that create different windows - toplevel, popup
  // and subsurface.
  std::vector<PlatformWindowType> window_types{PlatformWindowType::kWindow,
                                               PlatformWindowType::kPopup,
                                               PlatformWindowType::kTooltip};

  for (const auto& type : window_types) {
    // If the type is not kWindow, provide default created window as parent of
    // the newly created window.
    auto temp_window = CreateWindow(type, type != PlatformWindowType::kWindow
                                              ? widget_
                                              : gfx::kNullAcceleratedWidget);
    auto widget = temp_window->GetWidget();

    // Subsurface doesn't have an interface for sending configure events.
    // Thus, WaylandAuxiliaryWindow notifies the manager that the window is
    // activated upon creation of the subsurface. Skip calling Show() and call
    // later then.
    if (type != PlatformWindowType::kTooltip)
      temp_window->Show(false);

    Sync();

    auto* mock_surface = server_.GetObject<wl::MockSurface>(
        temp_window->root_surface()->GetSurfaceId());
    MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

    auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
    EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);

    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kDmabufBufferId);

    Sync();

    ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                                 false /* fail */);

    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);

    buffer_manager_gpu_->CommitBuffer(
        widget, kDmabufBufferId, window_->GetBounds(), window_->GetBounds());
    Sync();

    if (type != PlatformWindowType::kTooltip) {
      DCHECK(mock_surface->xdg_surface());
      ActivateSurface(mock_surface->xdg_surface());
    } else {
      // WaylandAuxiliaryWindow uses the focused window as a parent.
      window_->SetPointerFocus(true);
      // See the comment near Show() call above.
      temp_window->Show(false);
    }

    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);

    Sync();

    window_->SetPointerFocus(false);
    temp_window.reset();
    DestroyBufferAndSetTerminateExpectation(widget, kDmabufBufferId,
                                            false /*fail*/);

    Sync();
  }
}

// The buffer that is not originally attached to any of the surfaces,
// must be attached when a commit request comes. Also, it must setup a buffer
// release listener and OnSubmission must be called for that buffer if it is
// released.
TEST_P(WaylandBufferManagerTest, AnonymousBufferAttachedAndReleased) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBounds(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  constexpr uint32_t kNumberOfCommits = 3;
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
  EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  mock_surface->SendFrameCallback();

  Sync();

  // Now synchronously create a second buffer and commit it. The release
  // callback must be setup and OnSubmission must be called.
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());

  Sync();

  mock_surface->SendFrameCallback();

  // Now asynchronously create another buffer so that a commit request
  // comes earlier than it is created by the Wayland compositor, but it can
  // released once the buffer is committed and processed (that is, it must be
  // able to setup a buffer release callback).
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK))
      .Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId3, bounds, bounds);

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);

  // Now, create the buffer from the Wayland compositor side and let the buffer
  // manager complete the commit request.
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId3, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyBufferForDestroyedWindow) {
  constexpr uint32_t kBufferId = 1;

  auto temp_window = CreateWindow();
  auto widget = temp_window->GetWidget();

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  Sync();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId, temp_window->GetBounds(),
                                    temp_window->GetBounds());

  Sync();

  temp_window.reset();
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyedWindowNoSubmissionSingleBuffer) {
  constexpr uint32_t kBufferId = 1;

  auto temp_window = CreateWindow();
  auto widget = temp_window->GetWidget();
  auto bounds = temp_window->GetBounds();

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  temp_window.reset();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId, bounds, bounds);

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyedWindowNoSubmissionMultipleBuffers) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  auto temp_window = CreateWindow();
  temp_window->Show(false);

  Sync();

  auto widget = temp_window->GetWidget();
  auto bounds = temp_window->GetBounds();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      temp_window->root_surface()->GetSurfaceId());
  ASSERT_TRUE(mock_surface);

  ActivateSurface(mock_surface->xdg_surface());

  Sync();

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  mock_surface->SendFrameCallback();

  Sync();

  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  Sync();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());

  Sync();

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  temp_window.reset();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
}

// Tests that OnSubmission and OnPresentation are properly triggered if a buffer
// is committed twice in a row and those buffers are destroyed.
TEST_P(WaylandBufferManagerTest, DestroyBufferCommittedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Destroying buffer2 should do nothing yet.
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Destroying buffer1 should give us two acks for buffer2.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(2);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(2);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
}

// Tests that OnSubmission and OnPresentation are properly triggered if a buffer
// is committed twice in a row and then released.
TEST_P(WaylandBufferManagerTest, ReleaseBufferCommittedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  auto* wl_buffer1 = mock_surface->attached_buffer();

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer1 should trigger two acks for buffer2.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(2);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(2);
  mock_surface->ReleaseBuffer(wl_buffer1);
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
}

// Tests that OnSubmission and OnPresentation callbacks are properly called
// even if buffers are not released in the same order they were committed.
TEST_P(WaylandBufferManagerTest, ReleaseOrderDifferentToCommitOrder) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(3);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  auto* wl_buffer1 = mock_surface->attached_buffer();

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(2);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();
  auto* wl_buffer2 = mock_surface->attached_buffer();

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId3, bounds, bounds);
  mock_surface->SendFrameCallback();
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer2 can't trigger OnSubmission for buffer3, because
  // OnSubmission for buffer2 has not been sent yet.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  mock_surface->ReleaseBuffer(wl_buffer2);
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer1 should trigger acks for both.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);
  mock_surface->ReleaseBuffer(wl_buffer1);
  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId3, false /*fail*/);
}

// This test verifies that submitting the buffer more than once results in
// OnSubmission callback as Wayland compositor is not supposed to release the
// buffer committed twice.
TEST_P(WaylandBufferManagerTest,
       OnSubmissionCalledForBufferCommitedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  ASSERT_TRUE(!connection_->presentation());
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface,
              DamageBuffer(0, 0, bounds.width(), bounds.height()))
      .Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  mock_surface->SendFrameCallback();

  Sync();

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface,
              DamageBuffer(0, 0, bounds.width(), bounds.height()))
      .Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  mock_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  // Now, commit the buffer with the |kBufferId2| again and make sure the
  // manager manually sends the submission callback as long as the compositor is
  // not going to release a buffer as it was the same buffer submitted more than
  // once.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface,
              DamageBuffer(0, 0, bounds.width(), bounds.height()))
      .Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  // Commit second buffer now.
  buffer_manager_gpu_->CommitBuffer(widget, kBufferId2, bounds, bounds);

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, DamageBuffer(_, _, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  mock_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  // If we commit another buffer now, the manager host must not automatically
  // trigger OnSubmission and OnPresentation callbacks.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface,
              DamageBuffer(0, 0, bounds.width(), bounds.height()))
      .Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  // Now, they must be triggered once the buffer is released.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  mock_surface->SendFrameCallback();

  Sync();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(widget, kBufferId2, false /*fail*/);
}

// Tests that submitting a single buffer only receives an OnSubmission. This is
// required behaviour to make sure that submitting buffers in a quiescent state
// will be immediately acked.
TEST_P(WaylandBufferManagerTest, OnSubmissionCalledForSingleBuffer) {
  constexpr uint32_t kBufferId1 = 1;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);

  Sync();

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  buffer_manager_gpu_->CommitBuffer(widget, kBufferId1, bounds, bounds);
  Sync();

  DestroyBufferAndSetTerminateExpectation(widget, kBufferId1, false /*fail*/);
}

// Tests that when CommitOverlays(), root_surface can only be committed once all
// overlays in the frame are committed.
TEST_P(WaylandBufferManagerTest, RootSurfaceIsCommittedLast) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBounds();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  auto* linux_dmabuf = server_.zwp_linux_dmabuf_v1();
  EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(3);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  Sync();

  // Ack creation for only the first 2 wl_buffers.
  LOG(ERROR) << linux_dmabuf->buffer_params().size();
  zwp_linux_buffer_params_v1_send_created(
      linux_dmabuf->buffer_params()[0]->resource(),
      linux_dmabuf->buffer_params()[0]->buffer_resource());
  zwp_linux_buffer_params_v1_send_created(
      linux_dmabuf->buffer_params()[1]->resource(),
      linux_dmabuf->buffer_params()[1]->buffer_resource());

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  // root_surface shall not be committed as one of its subsurface is not
  // committed yet due to pending wl_buffer creation.
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
  EXPECT_CALL(*mock_surface, Commit()).Times(0);

  std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlay_configs;
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, kBufferId1,
      bounds, gfx::RectF(), bounds, false, gfx::GpuFenceHandle()));
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, kBufferId2, bounds,
      gfx::RectF(), bounds, false, gfx::GpuFenceHandle()));
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      1, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, kBufferId3, bounds,
      gfx::RectF(), bounds, false, gfx::GpuFenceHandle()));
  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(),
                                      std::move(overlay_configs));
  Sync();
  testing::Mock::VerifyAndClearExpectations(mock_surface);

  // Once wl_buffer is created, all subsurfaces are committed, hence
  // root_surface can be committed.
  zwp_linux_buffer_params_v1_send_created(
      linux_dmabuf->buffer_params()[0]->resource(),
      linux_dmabuf->buffer_params()[0]->buffer_resource());
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
  EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  Sync();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandBufferManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
