// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_helper.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace gl {
namespace {

class GLImageRefCountedMemory : public GLImageMemoryForTesting {
 public:
  explicit GLImageRefCountedMemory(const gfx::Size& size)
      : GLImageMemoryForTesting(size) {}

  GLImageRefCountedMemory(const GLImageRefCountedMemory&) = delete;
  GLImageRefCountedMemory& operator=(const GLImageRefCountedMemory&) = delete;

  bool Initialize(base::RefCountedMemory* ref_counted_memory,
                  gfx::BufferFormat format) {
    if (!GLImageMemory::Initialize(
            ref_counted_memory->front(), format,
            gfx::RowSizeForBufferFormat(GetSize().width(), format, 0))) {
      return false;
    }

    DCHECK(!ref_counted_memory_.get());
    ref_counted_memory_ = ref_counted_memory;
    return true;
  }

 private:
  ~GLImageRefCountedMemory() override = default;
  scoped_refptr<base::RefCountedMemory> ref_counted_memory_;
};

class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {}
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
};

void RunPendingTasks(scoped_refptr<base::TaskRunner> task_runner) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE, BindOnce(&base::WaitableEvent::Signal, Unretained(&done)));
  done.Wait();
}

void DestroySurface(scoped_refptr<DirectCompositionSurfaceWin> surface) {
  scoped_refptr<base::TaskRunner> task_runner =
      surface->GetWindowTaskRunnerForTesting();
  DCHECK(surface->HasOneRef());

  surface = nullptr;

  // Ensure that the ChildWindowWin posts the task to delete the thread to the
  // main loop before doing RunUntilIdle. Otherwise the child threads could
  // outlive the main thread.
  RunPendingTasks(task_runner);

  base::RunLoop().RunUntilIdle();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateNV12Texture(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size,
    bool shared) {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1;
  desc.BindFlags = 0;
  if (shared) {
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX |
                     D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
  }

  std::vector<char> image_data(size.width() * size.height() * 3 / 2);
  // Y, U, and V should all be 160. Output color should be pink.
  memset(&image_data[0], 160, size.width() * size.height() * 3 / 2);

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = (const void*)&image_data[0];
  data.SysMemPitch = size.width();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, &data, &texture);
  CHECK(SUCCEEDED(hr));
  return texture;
}

bool AreColorsSimilar(int a, int b) {
  // The precise colors may differ depending on the video processor, so allow
  // a margin for error.
  const int kMargin = 10;
  return abs(SkColorGetA(a) - SkColorGetA(b)) < kMargin &&
         abs(SkColorGetR(a) - SkColorGetR(b)) < kMargin &&
         abs(SkColorGetG(a) - SkColorGetG(b)) < kMargin &&
         abs(SkColorGetB(a) - SkColorGetB(b)) < kMargin;
}

}  // namespace

class DirectCompositionSurfaceTest : public testing::Test {
 public:
  DirectCompositionSurfaceTest() : parent_window_(ui::GetHiddenWindow()) {}

 protected:
  void SetUp() override {
    // These tests are assumed to run on battery.
    fake_power_monitor_source_.SetOnBatteryPower(true);

    // Without this, the following check always fails.
    display_ = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true, /*system_device_id=*/0);
    if (!DirectCompositionSupported()) {
      LOG(WARNING) << "DirectComposition not supported, skipping test.";
      return;
    }
    surface_ = CreateDirectCompositionSurfaceWin();
    context_ = CreateGLContext(surface_);
    if (surface_)
      surface_->SetEnableDCLayers(true);
    SetDirectCompositionScaledOverlaysSupportedForTesting(false);
    SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_NV12);
  }

  void TearDown() override {
    context_ = nullptr;
    if (surface_)
      DestroySurface(std::move(surface_));
    gl::init::ShutdownGL(display_, false);
  }

  scoped_refptr<DirectCompositionSurfaceWin>
  CreateDirectCompositionSurfaceWin() {
    DirectCompositionSurfaceWin::Settings settings;
    scoped_refptr<DirectCompositionSurfaceWin> surface =
        base::MakeRefCounted<DirectCompositionSurfaceWin>(
            gl::GLSurfaceEGL::GetGLDisplayEGL(), parent_window_,
            DirectCompositionSurfaceWin::VSyncCallback(), settings);
    EXPECT_TRUE(surface->Initialize(GLSurfaceFormat()));

    // ImageTransportSurfaceDelegate::AddChildWindowToBrowser() is called in
    // production code here. However, to remove dependency from
    // gpu/ipc/service/image_transport_surface_delegate.h, here we directly
    // executes the required minimum code.
    if (parent_window_)
      ::SetParent(surface->window(), parent_window_);

    return surface;
  }

  scoped_refptr<GLContext> CreateGLContext(
      scoped_refptr<DirectCompositionSurfaceWin> surface) {
    scoped_refptr<GLContext> context =
        gl::init::CreateGLContext(nullptr, surface.get(), GLContextAttribs());
    EXPECT_TRUE(context->MakeCurrent(surface.get()));
    return context;
  }

  // Helper to allow for easy friending of the below restricted function.
  void SetColorSpaceOnGLImage(gl::GLImage* gl_image,
                              const gfx::ColorSpace& color_space) {
    gl_image->SetColorSpace(color_space);
  }

  HWND parent_window_;
  scoped_refptr<DirectCompositionSurfaceWin> surface_;
  scoped_refptr<GLContext> context_;
  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(DirectCompositionSurfaceTest, TestMakeCurrent) {
  if (!surface_)
    return;
  EXPECT_TRUE(
      surface_->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), true));

  // First SetDrawRectangle must be full size of surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  // SetDrawRectangle can't be called again until swap.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  // SetDrawRectangle must be contained within surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 101, 101)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  EXPECT_TRUE(
      surface_->Resize(gfx::Size(50, 50), 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  scoped_refptr<DirectCompositionSurfaceWin> surface2 =
      CreateDirectCompositionSurfaceWin();
  scoped_refptr<GLContext> context2 = CreateGLContext(surface2.get());

  surface2->SetEnableDCLayers(true);
  EXPECT_TRUE(
      surface2->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), true));
  // The previous IDCompositionSurface should be suspended when another
  // surface is being drawn to.
  EXPECT_TRUE(surface2->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(context2->IsCurrent(surface2.get()));

  // It should be possible to switch back to the previous surface and
  // unsuspend it.
  EXPECT_TRUE(context_->MakeCurrent(surface_.get()));
  context2 = nullptr;
  DestroySurface(std::move(surface2));
}

// Tests that switching using EnableDCLayers works.
TEST_F(DirectCompositionSurfaceTest, DXGIDCLayerSwitch) {
  if (!surface_)
    return;
  surface_->SetEnableDCLayers(false);
  EXPECT_TRUE(
      surface_->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), true));
  EXPECT_FALSE(surface_->GetBackbufferSwapChainForTesting());

  // First SetDrawRectangle must be full size of surface for DXGI swapchain.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface_->GetBackbufferSwapChainForTesting());

  // SetDrawRectangle and SetEnableDCLayers can't be called again until swap.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  surface_->SetEnableDCLayers(true);

  // Surface switched to use IDCompositionSurface, so must draw to entire
  // surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_FALSE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  surface_->SetEnableDCLayers(false);

  // Surface switched to use IDXGISwapChain, so must draw to entire surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));
}

// Ensure that the swapchain's alpha is correct.
TEST_F(DirectCompositionSurfaceTest, SwitchAlpha) {
  if (!surface_)
    return;
  surface_->SetEnableDCLayers(false);
  EXPECT_TRUE(
      surface_->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), true));
  EXPECT_FALSE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetBackbufferSwapChainForTesting();
  ASSERT_TRUE(swap_chain);
  DXGI_SWAP_CHAIN_DESC1 desc;
  swap_chain->GetDesc1(&desc);
  EXPECT_EQ(DXGI_ALPHA_MODE_PREMULTIPLIED, desc.AlphaMode);

  // Resize to the same parameters should have no effect.
  EXPECT_TRUE(
      surface_->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(
      surface_->Resize(gfx::Size(100, 100), 1.0, gfx::ColorSpace(), false));
  EXPECT_FALSE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));

  swap_chain = surface_->GetBackbufferSwapChainForTesting();
  ASSERT_TRUE(swap_chain);
  swap_chain->GetDesc1(&desc);
  EXPECT_EQ(DXGI_ALPHA_MODE_IGNORE, desc.AlphaMode);
}

// Ensure that the GLImage isn't presented again unless it changes.
TEST_F(DirectCompositionSurfaceTest, NoPresentTwice) {
  if (!surface_)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  SetColorSpaceOnGLImage(image_dxgi.get(), gfx::ColorSpace::CreateREC709());

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    surface_->ScheduleDCLayer(std::move(params));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_FALSE(swap_chain);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  UINT last_present_count = 0;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetLastPresentCount(&last_present_count)));

  // One present is normal, and a second present because it's the first frame
  // and the other buffer needs to be drawn to.
  EXPECT_EQ(2u, last_present_count);

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface_->GetLayerSwapChainForTesting(0);
  EXPECT_EQ(swap_chain2.Get(), swap_chain.Get());

  // It's the same image, so it should have the same swapchain.
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetLastPresentCount(&last_present_count)));
  EXPECT_EQ(2u, last_present_count);

  // The image changed, we should get a new present
  scoped_refptr<GLImageDXGI> image_dxgi2(
      new GLImageDXGI(texture_size, nullptr));
  image_dxgi2->SetTexture(texture, 0);
  SetColorSpaceOnGLImage(image_dxgi2.get(), gfx::ColorSpace::CreateREC709());

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    params->images[0] = image_dxgi2;
    params->images[1] = image_dxgi2;
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain3->GetLastPresentCount(&last_present_count)));
  // the present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be set to the onscreen video size.
TEST_F(DirectCompositionSurfaceTest, SwapchainSizeWithScaledOverlays) {
  if (!surface_)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(64, 64);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  SetColorSpaceOnGLImage(image_dxgi.get(), gfx::ColorSpace::CreateREC709());

  // HW supports scaled overlays.
  // The input texture size is maller than the window size.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  // Onscreen quad.
  gfx::Rect quad_rect = gfx::Rect(100, 100);

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&desc)));
  // Onscreen quad_rect.size is (100, 100).
  EXPECT_EQ(100u, desc.BufferDesc.Width);
  EXPECT_EQ(100u, desc.BufferDesc.Height);

  // Clear SwapChainPresenters
  // Must do Clear first because the swap chain won't resize immediately if
  // a new size is given unless this is the very first time after Clear.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  // The input texture size is bigger than the window size.
  quad_rect = gfx::Rect(32, 48);

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&desc)));
  // Onscreen quad_rect.size is (32, 48).
  EXPECT_EQ(32u, desc.BufferDesc.Width);
  EXPECT_EQ(48u, desc.BufferDesc.Height);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is not support - swapchain should be the onscreen video size.
TEST_F(DirectCompositionSurfaceTest, SwapchainSizeWithoutScaledOverlays) {
  if (!surface_)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(80, 80);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  SetColorSpaceOnGLImage(image_dxgi.get(), gfx::ColorSpace::CreateREC709());

  gfx::Rect quad_rect = gfx::Rect(42, 42);

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&desc)));
  // Onscreen quad_rect.size is (42, 42).
  EXPECT_EQ(42u, desc.BufferDesc.Width);
  EXPECT_EQ(42u, desc.BufferDesc.Height);

  // The input texture size is smaller than the window size.
  quad_rect = gfx::Rect(124, 136);

  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    surface_->ScheduleDCLayer(std::move(params));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&desc)));
  // Onscreen quad_rect.size is (124, 136).
  EXPECT_EQ(124u, desc.BufferDesc.Width);
  EXPECT_EQ(136u, desc.BufferDesc.Height);
}

// Test protected video flags
TEST_F(DirectCompositionSurfaceTest, ProtectedVideos) {
  if (!surface_)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(1280, 720);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, false);

  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  image_dxgi->SetTexture(texture, 0);
  SetColorSpaceOnGLImage(image_dxgi.get(), gfx::ColorSpace::CreateREC709());
  gfx::Size window_size(640, 360);

  // Clear video
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->quad_rect = gfx::Rect(window_size);
    params->content_rect = gfx::Rect(texture_size);
    params->protected_video_type = gfx::ProtectedVideoType::kClear;

    surface_->ScheduleDCLayer(std::move(params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&desc)));
    auto display_only_flag = desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    auto hw_protected_flag = desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(0u, display_only_flag);
    EXPECT_EQ(0u, hw_protected_flag);
  }

  // Software protected video
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->quad_rect = gfx::Rect(window_size);
    params->content_rect = gfx::Rect(texture_size);
    params->protected_video_type = gfx::ProtectedVideoType::kSoftwareProtected;

    surface_->ScheduleDCLayer(std::move(params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
    auto display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    auto hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY, display_only_flag);
    EXPECT_EQ(0u, hw_protected_flag);
  }

  // TODO(magchen): Add a hardware protected video test when hardware procted
  // video support is enabled by defaut in the Intel driver and Chrome
}

class DirectCompositionPixelTest : public DirectCompositionSurfaceTest {
 public:
  DirectCompositionPixelTest()
      : window_(&platform_delegate_, gfx::Rect(100, 100)) {
    parent_window_ = window_.hwnd();
  }

 protected:
  void SetUp() override {
    static_cast<ui::PlatformWindow*>(&window_)->Show();
    DirectCompositionSurfaceTest::SetUp();
  }

  void TearDown() override {
    // Test harness times out without DestroyWindow() here.
    if (IsWindow(parent_window_))
      DestroyWindow(parent_window_);
    DirectCompositionSurfaceTest::TearDown();
  }

  void InitializeForPixelTest(const gfx::Size& window_size,
                              const gfx::Size& texture_size,
                              const gfx::Rect& content_rect,
                              const gfx::Rect& quad_rect) {
    EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size, true);
    Microsoft::WRL::ComPtr<IDXGIResource1> resource;
    texture.As(&resource);
    HANDLE handle = 0;
    resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                                 &handle);
    // The format doesn't matter, since we aren't binding.
    scoped_refptr<GLImageDXGI> image_dxgi(
        new GLImageDXGI(texture_size, nullptr));
    ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                             gfx::BufferFormat::RGBA_8888));

    // Pass content rect with odd with and height.  Surface should round up
    // width and height when creating swap chain.
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->content_rect = content_rect;
    params->quad_rect = quad_rect;
    surface_->ScheduleDCLayer(std::move(params));

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    Sleep(1000);
  }

  void PixelTestSwapChain(bool layers_enabled) {
    if (!surface_)
      return;
    if (!layers_enabled)
      surface_->SetEnableDCLayers(false);

    gfx::Size window_size(100, 100);
    EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    // Ensure DWM swap completed.
    Sleep(1000);

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_EQ(expected_color, actual_color)
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;

    EXPECT_TRUE(context_->IsCurrent(surface_.get()));
  }

  TestPlatformDelegate platform_delegate_;
  ui::WinWindow window_;
};

TEST_F(DirectCompositionPixelTest, DCLayersEnabled) {
  PixelTestSwapChain(true);
}

TEST_F(DirectCompositionPixelTest, DCLayersDisabled) {
  PixelTestSwapChain(false);
}

class DirectCompositionVideoPixelTest : public DirectCompositionPixelTest {
 protected:
  void TestVideo(const gfx::ColorSpace& color_space,
                 SkColor expected_color,
                 bool check_color) {
    if (!surface_)
      return;

    gfx::Size window_size(100, 100);
    EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();

    gfx::Size texture_size(50, 50);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size, false);

    scoped_refptr<GLImageDXGI> image_dxgi(
        new GLImageDXGI(texture_size, nullptr));
    image_dxgi->SetTexture(texture, 0);
    SetColorSpaceOnGLImage(image_dxgi.get(), color_space);

    {
      std::unique_ptr<ui::DCRendererLayerParams> params =
          std::make_unique<ui::DCRendererLayerParams>();
      params->images[0] = image_dxgi;
      params->content_rect = gfx::Rect(texture_size);
      params->quad_rect = gfx::Rect(texture_size);
      surface_->ScheduleDCLayer(std::move(params));
    }

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    {
      std::unique_ptr<ui::DCRendererLayerParams> params =
          std::make_unique<ui::DCRendererLayerParams>();
      params->images[0] = image_dxgi;
      params->content_rect = gfx::Rect(texture_size);
      params->quad_rect = gfx::Rect(window_size);
      surface_->ScheduleDCLayer(std::move(params));
    }

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
    Sleep(1000);

    if (check_color) {
      SkColor actual_color =
          GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
      EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
          << std::hex << "Expected " << expected_color << " Actual "
          << actual_color;
    }
  }
};

TEST_F(DirectCompositionVideoPixelTest, BT601) {
  TestVideo(gfx::ColorSpace::CreateREC601(), SkColorSetRGB(0xdb, 0x81, 0xe8),
            true);
}

TEST_F(DirectCompositionVideoPixelTest, BT709) {
  TestVideo(gfx::ColorSpace::CreateREC709(), SkColorSetRGB(0xe1, 0x90, 0xeb),
            true);
}

TEST_F(DirectCompositionVideoPixelTest, SRGB) {
  // SRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGB(), SK_ColorTRANSPARENT, false);
}

TEST_F(DirectCompositionVideoPixelTest, SCRGBLinear) {
  // SCRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGBLinear(), SK_ColorTRANSPARENT, false);
}

TEST_F(DirectCompositionVideoPixelTest, InvalidColorSpace) {
  // Invalid color space should be treated as BT.709
  TestVideo(gfx::ColorSpace(), SkColorSetRGB(0xe1, 0x90, 0xeb), true);
}

TEST_F(DirectCompositionPixelTest, SoftwareVideoSwapchain) {
  if (!surface_)
    return;

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size y_size(50, 50);
  gfx::Size uv_size(25, 25);
  size_t y_stride =
      gfx::RowSizeForBufferFormat(y_size.width(), gfx::BufferFormat::R_8, 0);
  size_t uv_stride =
      gfx::RowSizeForBufferFormat(uv_size.width(), gfx::BufferFormat::RG_88, 0);
  std::vector<uint8_t> y_data(y_stride * y_size.height(), 0xff);
  std::vector<uint8_t> uv_data(uv_stride * uv_size.height(), 0xff);
  auto y_image = base::MakeRefCounted<GLImageRefCountedMemory>(y_size);
  y_image->Initialize(new base::RefCountedBytes(y_data),
                      gfx::BufferFormat::R_8);
  auto uv_image = base::MakeRefCounted<GLImageRefCountedMemory>(uv_size);
  uv_image->Initialize(new base::RefCountedBytes(uv_data),
                       gfx::BufferFormat::RG_88);
  SetColorSpaceOnGLImage(y_image.get(), gfx::ColorSpace::CreateREC709());

  std::unique_ptr<ui::DCRendererLayerParams> params =
      std::make_unique<ui::DCRendererLayerParams>();
  params->images[0] = y_image;
  params->images[1] = uv_image;
  params->content_rect = gfx::Rect(y_size);
  params->quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(std::move(params));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));
  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xff, 0xb7, 0xff);
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, VideoHandleSwapchain) {
  if (!surface_)
    return;

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, SkipVideoLayerEmptyBoundsRect) {
  if (!surface_)
    return;

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect;  // Layer with empty bounds rect.
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, SkipVideoLayerEmptyContentsRect) {
  if (!surface_)
    return;
  // Swap chain size is overridden to onscreen size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.As(&resource);
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  // Layer with empty content rect.
  std::unique_ptr<ui::DCRendererLayerParams> params =
      std::make_unique<ui::DCRendererLayerParams>();
  params->images[0] = image_dxgi;
  params->quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(std::move(params));

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NV12SwapChain) {
  if (!surface_)
    return;
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height.  Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_NV12, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, YUY2SwapChain) {
  if (!surface_)
    return;
  // CreateSwapChainForCompositionSurfaceHandle fails with YUY2 format on
  // Win10/AMD bot (Radeon RX550). See https://crbug.com/967860.
  if (context_ && context_->GetVersionInfo() &&
      context_->GetVersionInfo()->driver_vendor.find("AMD") !=
          std::string::npos)
    return;

  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  // By default NV12 is used, so set it to YUY2 explicitly.
  SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_YUY2);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height.  Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_YUY2, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NonZeroBoundsOffset) {
  if (!surface_)
    return;
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect(gfx::Point(25, 25), texture_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  SkColor video_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  struct {
    gfx::Point point;
    SkColor expected_color;
  } test_cases[] = {
      // Outside bounds
      {{24, 24}, SK_ColorBLACK},
      {{75, 75}, SK_ColorBLACK},
      // Inside bounds
      {{25, 25}, video_color},
      {{74, 74}, video_color},
  };

  auto pixels = GLTestHelper::ReadBackWindow(window_.hwnd(), window_size);

  for (const auto& test_case : test_cases) {
    const auto& point = test_case.point;
    const auto& expected_color = test_case.expected_color;
    SkColor actual_color = pixels[window_size.width() * point.y() + point.x()];
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color << " at " << point.ToString();
  }
}

TEST_F(DirectCompositionPixelTest, ResizeVideoLayer) {
  if (!surface_)
    return;
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size, true);
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  texture.As(&resource);
  HANDLE handle = 0;
  resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                               &handle);
  // The format doesn't matter, since we aren't binding.
  scoped_refptr<GLImageDXGI> image_dxgi(new GLImageDXGI(texture_size, nullptr));
  ASSERT_TRUE(image_dxgi->InitializeHandle(base::win::ScopedHandle(handle), 0,
                                           gfx::BufferFormat::RGBA_8888));

  // (1) Test if swap chain is overridden to window size (100, 100).
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(std::move(params));

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // (2) Test if swap chain is overridden to window size (100, 100).
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->content_rect = gfx::Rect(30, 30);
    params->quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(std::move(params));

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
  }
  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // (3) Test if swap chain is adjusted to fit the monitor when overlay scaling
  // is not supported and video on-screen size is slightly smaller than the
  // monitor. Clipping is on.
  SetDirectCompositionScaledOverlaysSupportedForTesting(false);
  gfx::Size monitor_size = window_size;
  SetDirectCompositionMonitorInfoForTesting(1, window_size);
  gfx::Rect on_screen_rect =
      gfx::Rect(0, 0, monitor_size.width() - 2, monitor_size.height() - 2);
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->content_rect = gfx::Rect(50, 50);
    params->quad_rect = on_screen_rect;
    params->clip_rect = on_screen_rect;
    surface_->ScheduleDCLayer(std::move(params));

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
  }

  // Swap chain is set to monitor/onscreen size.
  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(static_cast<UINT>(monitor_size.width()), desc.Width);
  EXPECT_EQ(static_cast<UINT>(monitor_size.height()), desc.Height);

  gfx::Transform transform;
  gfx::Point offset;
  gfx::Rect clip_rect;
  surface_->GetSwapChainVisualInfoForTesting(0, &transform, &offset,
                                             &clip_rect);
  EXPECT_TRUE(transform.IsIdentity());
  EXPECT_EQ(gfx::Rect(monitor_size), clip_rect);

  // (4) Test if the final on-screen size is adjusted to fit the monitor when
  // overlay scaling is supported and video on-screen size is slightly bigger
  // than the monitor. Clipping is off.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  on_screen_rect =
      gfx::Rect(0, 0, monitor_size.width() + 2, monitor_size.height() + 2);
  {
    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;

    params->content_rect = gfx::Rect(50, 50);
    params->quad_rect = on_screen_rect;
    surface_->ScheduleDCLayer(std::move(params));

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));
  }

  // Swap chain is set to monitor size (100, 100).
  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  surface_->GetSwapChainVisualInfoForTesting(0, &transform, &offset,
                                             &clip_rect);
  EXPECT_EQ(gfx::Rect(monitor_size), transform.MapRect(gfx::Rect(100, 100)));
}

TEST_F(DirectCompositionPixelTest, SwapChainImage) {
  if (!surface_)
    return;
  // Fails on AMD RX 5500 XT. https://crbug.com/1152565.
  if (context_ && context_->GetVersionInfo() &&
      context_->GetVersionInfo()->driver_vendor.find("AMD") !=
          std::string::npos)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  ASSERT_TRUE(d3d11_device);
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  ASSERT_TRUE(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  ASSERT_TRUE(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
  ASSERT_TRUE(dxgi_factory);

  gfx::Size swap_chain_size(50, 50);
  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = swap_chain_size.width();
  desc.Height = swap_chain_size.height();
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = 0;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

  ASSERT_TRUE(SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(
      d3d11_device.Get(), &desc, nullptr, &swap_chain)));
  ASSERT_TRUE(swap_chain);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
  ASSERT_TRUE(SUCCEEDED(
      swap_chain->GetBuffer(1u, IID_PPV_ARGS(&front_buffer_texture))));
  ASSERT_TRUE(front_buffer_texture);

  auto front_buffer_image = base::MakeRefCounted<GLImageD3D>(
      swap_chain_size, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
      gfx::ColorSpace::CreateSRGB(), front_buffer_texture,
      /*array_slice=*/0, /*plane_index=*/0, swap_chain);
  ASSERT_TRUE(front_buffer_image->Initialize());

  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
  ASSERT_TRUE(
      SUCCEEDED(swap_chain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer_texture))));
  ASSERT_TRUE(back_buffer_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  ASSERT_TRUE(SUCCEEDED(d3d11_device->CreateRenderTargetView(
      back_buffer_texture.Get(), nullptr, &rtv)));
  ASSERT_TRUE(rtv);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device->GetImmediateContext(&context);
  ASSERT_TRUE(context);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  DXGI_PRESENT_PARAMETERS present_params = {};
  present_params.DirtyRectsCount = 0;
  present_params.pDirtyRects = nullptr;

  // Clear to red and present.
  {
    float clear_color[] = {1.0, 0.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    std::unique_ptr<ui::DCRendererLayerParams> dc_layer_params =
        std::make_unique<ui::DCRendererLayerParams>();
    dc_layer_params->images[0] = front_buffer_image;
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(std::move(dc_layer_params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Clear to green and present.
  {
    float clear_color[] = {0.0, 1.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    std::unique_ptr<ui::DCRendererLayerParams> dc_layer_params =
        std::make_unique<ui::DCRendererLayerParams>();
    dc_layer_params->images[0] = front_buffer_image;
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(std::move(dc_layer_params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    SkColor expected_color = SK_ColorGREEN;
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Present without clearing.  This will flip front and back buffers so the
  // previous rendered contents (red) will become visible again.
  {
    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    std::unique_ptr<ui::DCRendererLayerParams> dc_layer_params =
        std::make_unique<ui::DCRendererLayerParams>();
    dc_layer_params->images[0] = front_buffer_image;
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(std::move(dc_layer_params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Clear to blue without present.
  {
    float clear_color[] = {0.0, 0.0, 1.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    std::unique_ptr<ui::DCRendererLayerParams> dc_layer_params =
        std::make_unique<ui::DCRendererLayerParams>();
    dc_layer_params->images[0] = front_buffer_image;
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(std::move(dc_layer_params));
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing(), FrameData()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }
}

// Offsets BeginDraw update rect so that the returned update offset is also
// offset by at least the same amount from the original update rect.
class DrawOffsetOverridingDCompositionSurface
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDCompositionSurface> {
 public:
  DrawOffsetOverridingDCompositionSurface(
      Microsoft::WRL::ComPtr<IDCompositionSurface> surface,
      const gfx::Point& draw_offset)
      : surface_(std::move(surface)), draw_offset_(draw_offset) {}

  IFACEMETHODIMP BeginDraw(const RECT* updateRect,
                           REFIID iid,
                           void** updateObject,
                           POINT* updateOffset) override {
    RECT offsetRect = *updateRect;
    offsetRect.left += draw_offset_.x();
    offsetRect.right += draw_offset_.x();
    offsetRect.top += draw_offset_.y();
    offsetRect.bottom += draw_offset_.y();
    return surface_->BeginDraw(&offsetRect, iid, updateObject, updateOffset);
  }

  IFACEMETHODIMP EndDraw() override { return surface_->EndDraw(); }

  IFACEMETHODIMP ResumeDraw() override { return surface_->ResumeDraw(); }

  IFACEMETHODIMP SuspendDraw() override { return surface_->SuspendDraw(); }

  IFACEMETHODIMP Scroll(const RECT* scrollRect,
                        const RECT* clipRect,
                        int offsetX,
                        int offsetY) override {
    return surface_->Scroll(scrollRect, clipRect, offsetX, offsetY);
  }

 private:
  Microsoft::WRL::ComPtr<IDCompositionSurface> surface_;
  const gfx::Point draw_offset_;
};

TEST_F(DirectCompositionPixelTest, RootSurfaceDrawOffset) {
  if (!surface_)
    return;

  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  constexpr gfx::Point draw_offset(50, 50);
  auto root_surface = surface_->GetRootSurfaceForTesting();
  auto dcomp_surface =
      Microsoft::WRL::Make<DrawOffsetOverridingDCompositionSurface>(
          root_surface->dcomp_surface(), draw_offset);
  root_surface->SetDCompSurfaceForTesting(std::move(dcomp_surface));

  // Even though draw_rect is the first quadrant, the rendering will be limited
  // to the third quadrant because the dcomp surface will return that offset.
  constexpr gfx::Rect draw_rect(0, 0, 50, 50);
  EXPECT_TRUE(surface_->SetDrawRectangle(draw_rect));

  glClearColor(1.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing(), FrameData()));

  Sleep(1000);

  // Note: The colors read back are BGRA so the expected colors are inverted
  // with respect to the clear color.
  struct {
    gfx::Point position;
    SkColor expected_color;
  } test_cases[] = {{gfx::Point(75, 75), SkColorSetRGB(255, 0, 0)},
                    {gfx::Point(25, 25), SkColorSetRGB(0, 0, 255)}};

  for (const auto& test_case : test_cases) {
    SkColor actual_color =
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), test_case.position);
    EXPECT_TRUE(AreColorsSimilar(test_case.expected_color, actual_color))
        << std::hex << "Expected " << test_case.expected_color << " Actual "
        << actual_color;
  }
}

void RunBufferCountTest(scoped_refptr<DirectCompositionSurfaceWin> surface,
                        UINT buffer_count,
                        bool for_video) {
  if (!surface)
    return;

  if (for_video) {
    SetDirectCompositionScaledOverlaysSupportedForTesting(true);
    EXPECT_TRUE(surface->SetEnableDCLayers(true));
  } else {
    EXPECT_TRUE(surface->SetEnableDCLayers(false));
  }

  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(surface->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(surface->SetDrawRectangle(gfx::Rect(window_size)));

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  constexpr gfx::Size texture_size(50, 50);
  if (for_video) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();
    ASSERT_TRUE(d3d11_device);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size, /*shared=*/false);
    // The format doesn't matter, since we aren't binding.
    scoped_refptr<GLImageDXGI> image_dxgi(
        new GLImageDXGI(texture_size, nullptr));
    image_dxgi->SetTexture(texture, /*level=*/0);

    std::unique_ptr<ui::DCRendererLayerParams> params =
        std::make_unique<ui::DCRendererLayerParams>();
    params->images[0] = image_dxgi;
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(window_size);
    EXPECT_TRUE(surface->ScheduleDCLayer(std::move(params)));
  }

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface->SwapBuffers(base::DoNothing(), FrameData()));

  auto swap_chain = for_video ? surface->GetLayerSwapChainForTesting(0)
                              : surface->GetBackbufferSwapChainForTesting();
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  // The expected size is window_size(100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);
  EXPECT_EQ(buffer_count, desc.BufferCount);
}

TEST_F(DirectCompositionSurfaceTest, RootSwapChainBufferCount) {
  RunBufferCountTest(surface_, /*buffer_count=*/2u, /*for_video=*/false);
}

TEST_F(DirectCompositionSurfaceTest, VideoSwapChainBufferCount) {
  RunBufferCountTest(surface_, /*buffer_count=*/2u, /*for_video=*/true);
}

class DirectCompositionTripleBufferingTest
    : public DirectCompositionSurfaceTest {
 public:
  DirectCompositionTripleBufferingTest() = default;
  ~DirectCompositionTripleBufferingTest() override = default;

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kDCompTripleBufferRootSwapChain,
                                    features::kDCompTripleBufferVideoSwapChain},
                                   {});
    DirectCompositionSurfaceTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DirectCompositionTripleBufferingTest, MainSwapChainBufferCount) {
  RunBufferCountTest(surface_, /*buffer_count=*/3u, /*for_video=*/false);
}

TEST_F(DirectCompositionTripleBufferingTest, VideoSwapChainBufferCount) {
  RunBufferCountTest(surface_, /*buffer_count=*/3u, /*for_video=*/true);
}

}  // namespace gl
