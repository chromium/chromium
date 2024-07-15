// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_timing.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

class GPUTimingTest : public testing::Test {
 public:
  void SetUp() override { SetupGLContext(); }

  void TearDown() override {
    context_ = nullptr;
    surface_ = nullptr;
    MockGLInterface::SetGLInterface(nullptr);
    GLSurfaceTestSupport::ShutdownGL(display_);
    cpu_time_bounded_ = false;
    gl_.reset();
    gpu_timing_fake_queries_.Reset();
  }

  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() {
    EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _))
        .WillOnce(SetArgPointee<1>(0))
        .RetiresOnSaturation();
    scoped_refptr<GPUTimingClient> client = context_->CreateGPUTimingClient();
    if (!cpu_time_bounded_) {
      client->SetCpuTimeForTesting(
          base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
      cpu_time_bounded_ = true;
    }
    return client;
  }

 protected:
  void SetupGLContext() {
    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    display_ = GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::StrictMock<MockGLInterface>>();
    MockGLInterface::SetGLInterface(gl_.get());

    context_ = new GLContextStub;
    context_->SetExtensionsString("GL_EXT_disjoint_timer_query");
    context_->SetGLVersionString("OpenGL ES 3.0");
    surface_ = new GLSurfaceStub;
    context_->MakeCurrent(surface_.get());
    gpu_timing_fake_queries_.Reset();
  }

  bool cpu_time_bounded_ = false;
  std::unique_ptr<::testing::StrictMock<MockGLInterface>> gl_;
  scoped_refptr<GLContextStub> context_;
  scoped_refptr<GLSurfaceStub> surface_;
  GPUTimingFake gpu_timing_fake_queries_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(GPUTimingTest, FakeTimerTest) {
  scoped_refptr<GPUTimingClient> gpu_timing_client = CreateGPUTimingClient();

  // Tests that we can properly set fake cpu times.
  gpu_timing_fake_queries_.SetCurrentCPUTime(123);
  EXPECT_EQ(123, gpu_timing_client->GetCurrentCPUTime());

  base::RepeatingCallback<int64_t(void)> empty;
  gpu_timing_client->SetCpuTimeForTesting(empty);
  EXPECT_NE(123, gpu_timing_client->GetCurrentCPUTime());
}

TEST_F(GPUTimingTest, ForceTimeElapsedQuery) {
  // Test that forcing time elapsed query affects all clients.
  scoped_refptr<GPUTimingClient> client1 = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client_force = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());
  client_force->ForceTimeElapsedQuery();
  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client2 = CreateGPUTimingClient();
  EXPECT_TRUE(client2->IsForceTimeElapsedQuery());
}

TEST_F(GPUTimingTest, QueryTimeStampTest) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  std::unique_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);

  const int64_t begin_cpu_time = 1230;
  const int64_t begin_gl_time = 10 * base::Time::kNanosecondsPerMicrosecond;
  const int64_t cpu_gl_offset =
      begin_gl_time / base::Time::kNanosecondsPerMicrosecond - begin_cpu_time;
  gpu_timing_fake_queries_.SetCPUGLOffset(cpu_gl_offset);
  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);
  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, false);

  gpu_timer->QueryTimeStamp();

  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time - 1);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);
  EXPECT_TRUE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time + 1);
  EXPECT_TRUE(gpu_timer->IsAvailable());

  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64_t start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  EXPECT_EQ(begin_cpu_time, start);
  EXPECT_EQ(begin_cpu_time, end);
}

TEST_F(GPUTimingTest, QueryTimestampUsingElapsedTest) {
  // Test timestamp queries on platforms with GL_EXT_disjoint_timer_query but
  // still lack support for timestamp queries
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  std::unique_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);

  const int64_t begin_cpu_time = 123;
  const int64_t begin_gl_time = 10 * base::Time::kNanosecondsPerMicrosecond;
  const int64_t cpu_gl_offset = begin_gl_time - begin_cpu_time;
  gpu_timing_fake_queries_.SetCPUGLOffset(cpu_gl_offset);
  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);

  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, true);

  // Custom mock override to ensure the timestamp bits are 0
  EXPECT_CALL(*gl_, GetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, NotNull()))
      .Times(Exactly(1))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));

  gpu_timer->QueryTimeStamp();

  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time - 1);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time + 1);
  EXPECT_TRUE(gpu_timer->IsAvailable());
  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64_t start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  // Force time elapsed won't be set until a query is actually attempted
  ASSERT_TRUE(client->IsForceTimeElapsedQuery());
  EXPECT_EQ(begin_cpu_time, start);
  EXPECT_EQ(begin_cpu_time, end);
}

}  // namespace gl
