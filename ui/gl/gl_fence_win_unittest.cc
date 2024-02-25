// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11_3.h>

#include "media/base/win/d3d11_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_fence_win.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gl {

class GLFenceWinTest : public ::testing::Test {
 public:
  void SetUp() override {
    // By default, the D3D11 device, context and fence object will allow
    // successful calls to QueryInterface, GetImmediateContext and GetDevice,
    // and return objects of the latest interfaces. Tests may override this
    // behavior to simulate interfaces which are not available.
    ON_CALL(d3d11_device_, QueryInterface(IID_ID3D11Device5, _))
        .WillByDefault(media::SetComPointeeAndReturnOk<1>(&d3d11_device_));
    ON_CALL(d3d11_device_, GetImmediateContext(_))
        .WillByDefault(media::SetComPointee<0>(&d3d11_device_context_));

    ON_CALL(d3d11_device_context_, QueryInterface(IID_ID3D11DeviceContext4, _))
        .WillByDefault(
            media::SetComPointeeAndReturnOk<1>(&d3d11_device_context_));

    ON_CALL(d3d11_fence_, GetDevice(_))
        .WillByDefault(media::SetComPointee<0>(&d3d11_device_));
  }

 protected:
  media::D3D11DeviceMock d3d11_device_;
  media::D3D11DeviceContextMock d3d11_device_context_;
  media::D3D11FenceMock d3d11_fence_;
};

// Ensure graceful failure when ID3D11Device5 is not available.
TEST_F(GLFenceWinTest, CreateForGpuFenceNoDevice5) {
  EXPECT_CALL(d3d11_device_, QueryInterface(IID_ID3D11Device5, _))
      .WillOnce(Return(E_NOINTERFACE));

  std::unique_ptr<GLFenceWin> gl_fence_win =
      GLFenceWin::CreateForGpuFence(&d3d11_device_);
  EXPECT_EQ(gl_fence_win.get(), nullptr);
}

TEST_F(GLFenceWinTest, CreateForGpuFence) {
  // Ensure created fences are made with the D3D11_FENCE_FLAG_SHARED flag so
  // they can be used across processes.
  EXPECT_CALL(d3d11_device_,
              CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_ID3D11Fence, _))
      .WillOnce(media::SetComPointeeAndReturnOk<3>(&d3d11_fence_));

  // GLFenceWin internally uses base::win::ScopedHandle, which calls global
  // functions like CloseHandle to do its job. To avoid mocking ScopedHandle,
  // have D3D11FenceMock use a real, closeable event.
  const HANDLE mock_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

  // Ensure the share handle is created with the correct read+write flags
  // to be shared across processes.
  EXPECT_CALL(d3d11_fence_, CreateSharedHandle(nullptr,
                                               DXGI_SHARED_RESOURCE_READ |
                                                   DXGI_SHARED_RESOURCE_WRITE,
                                               nullptr, _))
      .WillOnce(DoAll(SetArgPointee<3>(mock_handle), Return(S_OK)));

  // Ensure we signal the fence with 1 to match the wait in ServerWait.
  EXPECT_CALL(d3d11_device_context_, Signal(&d3d11_fence_, 1))
      .WillOnce(Return(S_OK));

  std::unique_ptr<GLFenceWin> gl_fence_win =
      GLFenceWin::CreateForGpuFence(&d3d11_device_);
  EXPECT_NE(gl_fence_win.get(), nullptr);

  std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence_win->GetGpuFence();
  EXPECT_NE(gpu_fence, nullptr);
  EXPECT_FALSE(gpu_fence->GetGpuFenceHandle().is_null());
}

TEST_F(GLFenceWinTest, CreateFromGpuFence) {
  // GLFenceWin internally uses base::win::ScopedHandle, which calls global
  // functions like CloseHandle to do its job. To avoid mocking ScopedHandle,
  // have gfx::GpuFenceHandle use a real, closeable event.
  const HANDLE mock_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

  gfx::GpuFenceHandle gpu_fence_handle;
  EXPECT_TRUE(gpu_fence_handle.is_null());
  gpu_fence_handle.Adopt(base::win::ScopedHandle(mock_handle));
  EXPECT_FALSE(gpu_fence_handle.is_null());

  gfx::GpuFence gpu_fence(std::move(gpu_fence_handle));
  EXPECT_TRUE(gpu_fence_handle.is_null());

  const gfx::GpuFenceHandle& fence_handle_ref = gpu_fence.GetGpuFenceHandle();
  EXPECT_EQ(fence_handle_ref.Peek(), mock_handle);
  EXPECT_FALSE(fence_handle_ref.is_null());

  // Ensure that CreateFromGpuFence opens the shared handle.
  EXPECT_CALL(d3d11_device_, OpenSharedFence(_, IID_ID3D11Fence, _))
      .WillOnce(media::SetComPointeeAndReturnOk<2>(&d3d11_fence_));

  std::unique_ptr<GLFenceWin> gl_fence_win =
      GLFenceWin::CreateFromGpuFence(&d3d11_device_, gpu_fence);
  EXPECT_NE(gl_fence_win.get(), nullptr);

  std::unique_ptr<gfx::GpuFence> gpu_fence_out = gl_fence_win->GetGpuFence();
  EXPECT_NE(gpu_fence_out, nullptr);
  EXPECT_FALSE(gpu_fence_out->GetGpuFenceHandle().is_null());

  // Verify that Wait is called with 1 to match the Signal in CreateForGpuFence.
  EXPECT_CALL(d3d11_device_context_, Wait(&d3d11_fence_, 1))
      .WillOnce(Return(S_OK));

  gl_fence_win->ServerWait();
}

}  // namespace gl
