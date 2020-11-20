// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/direct_composition_surface_win.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/transform.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace gl {
namespace {
class TestPlatformDelegate : public ui::PlatformWindowDelegate {
 public:
  // ui::PlatformWindowDelegate implementation.
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
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

class DirectCompositionSurfaceTest : public testing::Test {
 public:
  DirectCompositionSurfaceTest() : parent_window_(ui::GetHiddenWindow()) {}

 protected:
  void SetUp() override {
    // Without this, the following check always fails.
    gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings*/ true);
    if (!QueryDirectCompositionDevice(QueryD3D11DeviceObjectFromANGLE())) {
      LOG(WARNING)
          << "GL implementation not using DirectComposition, skipping test.";
      return;
    }
    surface_ = CreateDirectCompositionSurfaceWin();
    context_ = CreateGLContext(surface_);
    if (surface_)
      surface_->SetEnableDCLayers(true);
    DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(false);
    DirectCompositionSurfaceWin::SetOverlayFormatUsedForTesting(
        DXGI_FORMAT_NV12);
  }

  void TearDown() override {
    context_ = nullptr;
    if (surface_)
      DestroySurface(std::move(surface_));
    gl::init::ShutdownGL(false);
  }

  scoped_refptr<DirectCompositionSurfaceWin>
  CreateDirectCompositionSurfaceWin() {
    DirectCompositionSurfaceWin::Settings settings;
    scoped_refptr<DirectCompositionSurfaceWin> surface =
        base::MakeRefCounted<DirectCompositionSurfaceWin>(
            parent_window_, DirectCompositionSurfaceWin::VSyncCallback(),
            settings);
    EXPECT_TRUE(surface->Initialize(GLSurfaceFormat()));

    // ImageTransportSurfaceDelegate::DidCreateAcceleratedSurfaceChildWindow()
    // is called in production code here. However, to remove dependency from
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

  HWND parent_window_;
  scoped_refptr<DirectCompositionSurfaceWin> surface_;
  scoped_refptr<GLContext> context_;
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
            surface_->SwapBuffers(base::DoNothing()));

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
            surface_->SwapBuffers(base::DoNothing()));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  surface_->SetEnableDCLayers(true);

  // Surface switched to use IDCompositionSurface, so must draw to entire
  // surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_FALSE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
  EXPECT_TRUE(context_->IsCurrent(surface_.get()));

  surface_->SetEnableDCLayers(false);

  // Surface switched to use IDXGISwapChain, so must draw to entire surface.
  EXPECT_FALSE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(0, 0, 100, 100)));
  EXPECT_TRUE(surface_->GetBackbufferSwapChainForTesting());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
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
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  ui::DCRendererLayerParams params;
  params.images[0] = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(100, 100);
  surface_->ScheduleDCLayer(params);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_FALSE(swap_chain);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  UINT last_present_count = 0;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetLastPresentCount(&last_present_count)));

  // One present is normal, and a second present because it's the first frame
  // and the other buffer needs to be drawn to.
  EXPECT_EQ(2u, last_present_count);

  surface_->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

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
  image_dxgi2->SetColorSpace(gfx::ColorSpace::CreateREC709());

  params.images[0] = image_dxgi2;
  params.images[1] = image_dxgi2;
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain3->GetLastPresentCount(&last_present_count)));
  // the present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be the minimum of the decoded
// video buffer size and the onscreen video size
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
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  // HW supports scaled overlays
  // The input texture size is maller than the window size.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);

  ui::DCRendererLayerParams params;
  params.images[0] = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(100, 100);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC Desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
  EXPECT_EQ((int)Desc.BufferDesc.Width, texture_size.width());
  EXPECT_EQ((int)Desc.BufferDesc.Height, texture_size.height());

  // Clear SwapChainPresenters
  // Must do Clear first because the swap chain won't resize immediately if
  // a new size is given unless this is the very first time after Clear.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  // The input texture size is bigger than the window size.
  params.quad_rect = gfx::Rect(32, 48);

  surface_->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&Desc)));
  EXPECT_EQ((int)Desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)Desc.BufferDesc.Height, params.quad_rect.height());
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is not support - swapchain should be the onscreen video size
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
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());

  ui::DCRendererLayerParams params;
  params.images[0] = image_dxgi;
  params.content_rect = gfx::Rect(texture_size);
  params.quad_rect = gfx::Rect(42, 42);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&desc)));
  EXPECT_EQ((int)desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)desc.BufferDesc.Height, params.quad_rect.height());

  // The input texture size is smaller than the window size.
  params.quad_rect = gfx::Rect(124, 136);

  surface_->ScheduleDCLayer(params);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_TRUE(SUCCEEDED(swap_chain2->GetDesc(&desc)));
  EXPECT_EQ((int)desc.BufferDesc.Width, params.quad_rect.width());
  EXPECT_EQ((int)desc.BufferDesc.Height, params.quad_rect.height());
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
  image_dxgi->SetColorSpace(gfx::ColorSpace::CreateREC709());
  gfx::Size window_size(640, 360);

  // Clear video
  {
    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.quad_rect = gfx::Rect(window_size);
    params.content_rect = gfx::Rect(texture_size);
    params.protected_video_type = gfx::ProtectedVideoType::kClear;

    surface_->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
    unsigned display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    unsigned hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(display_only_flag, (unsigned)0);
    EXPECT_EQ(hw_protected_flag, (unsigned)0);
  }

  // Software protected video
  {
    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.quad_rect = gfx::Rect(window_size);
    params.content_rect = gfx::Rect(texture_size);
    params.protected_video_type = gfx::ProtectedVideoType::kSoftwareProtected;

    surface_->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        surface_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc(&Desc)));
    unsigned display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    unsigned hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(display_only_flag, (unsigned)DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY);
    EXPECT_EQ(hw_protected_flag, (unsigned)0);
  }

  // TODO(magchen): Add a hardware protected video test when hardware procted
  // video support is enabled by defaut in the Intel driver and Chrome
}

std::vector<SkColor> ReadBackWindow(HWND window, const gfx::Size& size) {
  base::win::ScopedCreateDC mem_hdc(::CreateCompatibleDC(nullptr));
  DCHECK(mem_hdc.IsValid());

  BITMAPV4HEADER hdr;
  gfx::CreateBitmapV4HeaderForARGB888(size.width(), size.height(), &hdr);

  void* bits = nullptr;
  base::win::ScopedBitmap bitmap(
      ::CreateDIBSection(mem_hdc.Get(), reinterpret_cast<BITMAPINFO*>(&hdr),
                         DIB_RGB_COLORS, &bits, nullptr, 0));
  DCHECK(bitmap.is_valid());

  base::win::ScopedSelectObject select_object(mem_hdc.Get(), bitmap.get());

  // Grab a copy of the window. Use PrintWindow because it works even when the
  // window's partially occluded. The PW_RENDERFULLCONTENT flag is undocumented,
  // but works starting in Windows 8.1. It allows for capturing the contents of
  // the window that are drawn using DirectComposition.
  UINT flags = PW_CLIENTONLY | PW_RENDERFULLCONTENT;

  BOOL result = PrintWindow(window, mem_hdc.Get(), flags);
  if (!result)
    PLOG(ERROR) << "Failed to print window";

  GdiFlush();

  std::vector<SkColor> pixels(size.width() * size.height());
  memcpy(pixels.data(), bits, pixels.size() * sizeof(SkColor));
  return pixels;
}

SkColor ReadBackWindowPixel(HWND window, const gfx::Point& point) {
  gfx::Size size(point.x() + 1, point.y() + 1);
  auto pixels = ReadBackWindow(window, size);
  return pixels[size.width() * point.y() + point.x()];
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
    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.content_rect = content_rect;
    params.quad_rect = quad_rect;
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

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
              surface_->SwapBuffers(base::DoNothing()));

    // Ensure DWM swap completed.
    Sleep(1000);

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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

bool AreColorsSimilar(int a, int b) {
  // The precise colors may differ depending on the video processor, so allow
  // a margin for error.
  const int kMargin = 10;
  return abs(SkColorGetA(a) - SkColorGetA(b)) < kMargin &&
         abs(SkColorGetR(a) - SkColorGetR(b)) < kMargin &&
         abs(SkColorGetG(a) - SkColorGetG(b)) < kMargin &&
         abs(SkColorGetB(a) - SkColorGetB(b)) < kMargin;
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
    image_dxgi->SetColorSpace(color_space);

    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.content_rect = gfx::Rect(texture_size);
    params.quad_rect = gfx::Rect(texture_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    params.quad_rect = gfx::Rect(window_size);

    surface_->ScheduleDCLayer(params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
    Sleep(1000);

    if (check_color) {
      SkColor actual_color =
          ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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
  TestVideo(gfx::ColorSpace::CreateSCRGBLinear(), SK_ColorTRANSPARENT, false);
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
  y_image->SetColorSpace(gfx::ColorSpace::CreateREC709());

  ui::DCRendererLayerParams params;
  params.images[0] = y_image;
  params.images[1] = uv_image;
  params.content_rect = gfx::Rect(y_size);
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));
  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xff, 0xb7, 0xff);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, SkipVideoLayerEmptyContentsRect) {
  if (!surface_)
    return;
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);

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
  ui::DCRendererLayerParams params;
  params.images[0] = image_dxgi;
  params.quad_rect = gfx::Rect(window_size);
  surface_->ScheduleDCLayer(params);

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
            surface_->SwapBuffers(base::DoNothing()));

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NV12SwapChain) {
  if (!surface_)
    return;
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);

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
  EXPECT_EQ(desc.Format, DXGI_FORMAT_NV12);
  EXPECT_EQ(desc.Width, 50u);
  EXPECT_EQ(desc.Height, 50u);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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
      context_->GetVersionInfo()->driver_vendor == "ANGLE (AMD)")
    return;

  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);
  // By default NV12 is used, so set it to YUY2 explicitly.
  DirectCompositionSurfaceWin::SetOverlayFormatUsedForTesting(DXGI_FORMAT_YUY2);

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
  EXPECT_EQ(desc.Format, DXGI_FORMAT_YUY2);
  EXPECT_EQ(desc.Width, 50u);
  EXPECT_EQ(desc.Height, 50u);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  SkColor actual_color =
      ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
  EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
      << std::hex << "Expected " << expected_color << " Actual "
      << actual_color;
}

TEST_F(DirectCompositionPixelTest, NonZeroBoundsOffset) {
  if (!surface_)
    return;
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);

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

  auto pixels = ReadBackWindow(window_.hwnd(), window_size);

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
  // Swap chain size is overridden to content rect size only if scaled overlays
  // are supported.
  DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(true);

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

  {
    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.content_rect = gfx::Rect(texture_size);
    params.quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      surface_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(desc.Width, 50u);
  EXPECT_EQ(desc.Height, 50u);

  {
    ui::DCRendererLayerParams params;
    params.images[0] = image_dxgi;

    params.content_rect = gfx::Rect(30, 30);
    params.quad_rect = gfx::Rect(window_size);
    surface_->ScheduleDCLayer(params);

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));
  }
  swap_chain = surface_->GetLayerSwapChainForTesting(0);
  EXPECT_TRUE(SUCCEEDED(swap_chain->GetDesc1(&desc)));
  EXPECT_EQ(desc.Width, 30u);
  EXPECT_EQ(desc.Height, 30u);
}

TEST_F(DirectCompositionPixelTest, SwapChainImage) {
  if (!surface_)
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
      swap_chain_size, GL_BGRA_EXT, GL_UNSIGNED_BYTE, front_buffer_texture,
      swap_chain);
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

  ui::DCRendererLayerParams dc_layer_params;
  dc_layer_params.images[0] = front_buffer_image;
  dc_layer_params.content_rect = gfx::Rect(swap_chain_size);
  dc_layer_params.quad_rect = gfx::Rect(window_size);

  DXGI_PRESENT_PARAMETERS present_params = {};
  present_params.DirtyRectsCount = 0;
  present_params.pDirtyRects = nullptr;

  // Clear to red and present.
  {
    float clear_color[] = {1.0, 0.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    surface_->ScheduleDCLayer(dc_layer_params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Clear to green and present.
  {
    float clear_color[] = {0.0, 1.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    surface_->ScheduleDCLayer(dc_layer_params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    SkColor expected_color = SK_ColorGREEN;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Present without clearing.  This will flip front and back buffers so the
  // previous rendered contents (red) will become visible again.
  {
    ASSERT_TRUE(SUCCEEDED(swap_chain->Present1(0, 0, &present_params)));

    surface_->ScheduleDCLayer(dc_layer_params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
    EXPECT_TRUE(AreColorsSimilar(expected_color, actual_color))
        << std::hex << "Expected " << expected_color << " Actual "
        << actual_color;
  }

  // Clear to blue without present.
  {
    float clear_color[] = {0.0, 0.0, 1.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    surface_->ScheduleDCLayer(dc_layer_params);
    EXPECT_EQ(gfx::SwapResult::SWAP_ACK,
              surface_->SwapBuffers(base::DoNothing()));

    SkColor expected_color = SK_ColorRED;
    SkColor actual_color =
        ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75));
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
            surface_->SwapBuffers(base::DoNothing()));

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
            surface_->SwapBuffers(base::DoNothing()));

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
        ReadBackWindowPixel(window_.hwnd(), test_case.position);
    EXPECT_TRUE(AreColorsSimilar(test_case.expected_color, actual_color))
        << std::hex << "Expected " << test_case.expected_color << " Actual "
        << actual_color;
  }
}

}  // namespace
}  // namespace gl
