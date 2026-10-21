// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gpu_timing.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

namespace {

int64_t ToMicroseconds(base::TimeTicks time) {
  return (time - base::TimeTicks()).InMicroseconds();
}

}  // namespace

class GLSurfacePresentationHelperTest : public testing::Test {
 public:
  void SetUp() override {
    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    display_ = GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::NiceMock<MockGLInterface>>();
    MockGLInterface::SetGLInterface(gl_.get());

    context_ = new GLContextStub;
    context_->SetExtensionsString("GL_EXT_disjoint_timer_query");
    context_->SetGLVersionString("OpenGL ES 3.0");
    surface_ = new GLSurfaceStub;
    context_->MakeCurrent(surface_.get());

    gpu_timing_fake_.Reset();
    gpu_timing_fake_.ExpectDisjointCalls(*gl_);
    gpu_timing_fake_.ExpectGPUTimeStampQuery(*gl_, /*elapsed_query=*/false);
    gpu_timing_fake_.SetCurrentCPUTime(ToMicroseconds(base::TimeTicks::Now()));

    // GPUTiming is shared by all clients of the context, so this makes the
    // helper's GPUTiming use the fake CPU clock too.
    gpu_timing_client_ = context_->CreateGPUTimingClient();
    gpu_timing_client_->SetCpuTimeForTesting(
        base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));

    helper_ = std::make_unique<GLSurfacePresentationHelper>(
        /*vsync_provider=*/nullptr);
    helper_->OnMakeCurrent(context_.get(), surface_.get());
  }

  void TearDown() override {
    helper_.reset();
    gpu_timing_client_ = nullptr;
    context_ = nullptr;
    surface_ = nullptr;
    MockGLInterface::SetGLInterface(nullptr);
    GLSurfaceTestSupport::ShutdownGL(display_);
    gl_.reset();
    gpu_timing_fake_.Reset();
  }

 protected:
  // Simulates a SwapBuffers(). The GPU executes the timestamp query at
  // `gpu_time` (in the CPU clock domain at the time of the swap).
  void Swap(base::TimeTicks gpu_time) {
    gpu_timing_fake_.SetCurrentCPUTime(ToMicroseconds(gpu_time));
    feedback_.reset();
    helper_->PreSwapBuffers(
        base::BindOnce(
            [](std::optional<gfx::PresentationFeedback>* out,
               const gfx::PresentationFeedback& feedback) { *out = feedback; },
            &feedback_),
        /*frame_id=*/-1);
    helper_->PostSwapBuffers(gfx::SwapResult::SWAP_ACK);
  }

  // Lets some time pass, then waits for the presentation callback.
  void WaitForFeedback() {
    base::PlatformThread::Sleep(base::Milliseconds(5));
    gpu_timing_fake_.SetCurrentCPUTime(ToMicroseconds(base::TimeTicks::Now()));
    ASSERT_TRUE(base::test::RunUntil([&] { return feedback_.has_value(); }));
  }

  std::unique_ptr<::testing::NiceMock<MockGLInterface>> gl_;
  scoped_refptr<GLContextStub> context_;
  scoped_refptr<GLSurfaceStub> surface_;
  scoped_refptr<GPUTimingClient> gpu_timing_client_;
  GPUTimingFake gpu_timing_fake_;
  raw_ptr<GLDisplay> display_ = nullptr;
  std::unique_ptr<GLSurfacePresentationHelper> helper_;
  std::optional<gfx::PresentationFeedback> feedback_;
};

// When the GPU and CPU clocks agree, the GPU timestamp is reported as-is.
TEST_F(GLSurfacePresentationHelperTest, UsesGPUTimestamp) {
  const base::TimeTicks gpu_time =
      base::TimeTicks::Now() + base::Milliseconds(1);
  Swap(gpu_time);
  WaitForFeedback();

  ASSERT_TRUE(feedback_.has_value());
  EXPECT_FALSE(feedback_->failed());
  EXPECT_EQ(gpu_time, feedback_->timestamp);
}

// Simulates a driver (e.g. nouveau) whose glGetInteger64v(GL_TIMESTAMP) is in
// a different clock domain than timestamp queries, and the two have drifted
// apart. The GPU timestamp converted to CPU time would be from before the
// frame was submitted, which viz would reject as a presentation failure. The
// helper must not report a timestamp outside of [swap, now].
TEST_F(GLSurfacePresentationHelperTest, GPUTimestampClockDrift) {
  const base::TimeTicks swap_time = base::TimeTicks::Now();
  Swap(swap_time + base::Milliseconds(1));

  // GL_TIMESTAMP now reads 100ms ahead of the clock used by the query.
  gpu_timing_fake_.SetCPUGLOffset(base::Milliseconds(100).InMicroseconds());
  WaitForFeedback();

  ASSERT_TRUE(feedback_.has_value());
  EXPECT_FALSE(feedback_->failed());
  EXPECT_GE(feedback_->timestamp, swap_time);
  EXPECT_LE(feedback_->timestamp, base::TimeTicks::Now());
}

}  // namespace gl
