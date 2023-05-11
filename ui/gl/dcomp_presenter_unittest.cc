// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_presenter.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/test/sk_color_eq.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_helper.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace gl {
namespace {

constexpr const char* kSkiaGoldPixelDiffCorpus = "chrome-gpu-gtest";

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

void DestroyPresenter(scoped_refptr<DCompPresenter> presenter) {
  scoped_refptr<base::TaskRunner> task_runner =
      presenter->GetWindowTaskRunnerForTesting();
  DCHECK(presenter->HasOneRef());

  presenter.reset();

  // Ensure that the ChildWindowWin posts the task to delete the thread to the
  // main loop before doing RunUntilIdle. Otherwise the child threads could
  // outlive the main thread.
  RunPendingTasks(task_runner);

  base::RunLoop().RunUntilIdle();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateNV12Texture(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size) {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;

  std::vector<char> image_data(size.width() * size.height() * 3 / 2);
  // Y, U, and V should all be 160. Output color should be pink.
  memset(&image_data[0], 160, size.width() * size.height() * 3 / 2);

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = (const void*)&image_data[0];
  data.SysMemPitch = size.width();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, &data, &texture);
  EXPECT_HRESULT_SUCCEEDED(hr);
  return texture;
}

// The precise colors may differ depending on the video processor, so allow a
// margin for error.
const int kMaxColorChannelDeviation = 10;

// Create an overlay image with an initial color and rectangles, drawn using the
// painter's algorithm.
DCLayerOverlayImage CreateDCompSurface(
    const gfx::Size& surface_size,
    SkColor4f initial_color,
    std::vector<std::pair<gfx::Rect, SkColor4f>> rectangles_back_to_front =
        {}) {
  HRESULT hr = S_OK;

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
      gl::GetDirectCompositionDevice();

  Microsoft::WRL::ComPtr<IDCompositionSurface> surface;
  hr = dcomp_device->CreateSurface(surface_size.width(), surface_size.height(),
                                   DXGI_FORMAT_B8G8R8A8_UNORM,
                                   DXGI_ALPHA_MODE_IGNORE, &surface);
  CHECK_EQ(S_OK, hr);

  // Add a rect that initializes the whole surface to |initial_color|.
  rectangles_back_to_front.insert(rectangles_back_to_front.begin(),
                                  {gfx::Rect(surface_size), initial_color});

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  d3d11_device->GetImmediateContext(&immediate_context);

  for (const auto& [draw_rect, color] : rectangles_back_to_front) {
    CHECK(gfx::Rect(surface_size).Contains(draw_rect));

    Microsoft::WRL::ComPtr<ID3D11Texture2D> update_texture;
    RECT rect = draw_rect.ToRECT();
    POINT update_offset;
    hr = surface->BeginDraw(&rect, IID_PPV_ARGS(&update_texture),
                            &update_offset);
    CHECK_EQ(S_OK, hr);

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    hr = d3d11_device->CreateRenderTargetView(update_texture.Get(), nullptr,
                                              &rtv);
    CHECK_EQ(S_OK, hr);

    immediate_context->ClearRenderTargetView(rtv.Get(), color.vec());

    hr = surface->EndDraw();
    CHECK_EQ(S_OK, hr);
  }

  return DCLayerOverlayImage(surface_size, surface);
}

}  // namespace

class DCompPresenterTest : public testing::Test {
 public:
  DCompPresenterTest() : parent_window_(ui::GetHiddenWindow()) {}

 protected:
  void SetUp() override {
    // These tests are assumed to run on battery.
    fake_power_monitor_source_.SetOnBatteryPower(true);

    // Without this, the following check always fails.
    display_ = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true, /*gpu_preference=*/gl::GpuPreference::kDefault);
    presenter_ = CreateDCompPresenter();

    // All bots run on non-blocklisted hardware that supports DComp (>Win7)
    ASSERT_TRUE(DirectCompositionSupported());

    gl_surface_ = init::CreateOffscreenGLSurface(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());
    context_ = CreateGLContext(gl_surface_);
    SetDirectCompositionScaledOverlaysSupportedForTesting(false);
    SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_NV12);
  }

  void TearDown() override {
    context_ = nullptr;
    if (presenter_) {
      DestroyPresenter(std::move(presenter_));
    }
    gl::init::ShutdownGL(display_, false);
  }

  scoped_refptr<DCompPresenter> CreateDCompPresenter() {
    DirectCompositionSurfaceWin::Settings settings;
    scoped_refptr<DCompPresenter> presenter =
        base::MakeRefCounted<DCompPresenter>(
            gl::GLSurfaceEGL::GetGLDisplayEGL(),
            DCompPresenter::VSyncCallback(), settings);
    EXPECT_TRUE(presenter->Initialize());

    // ImageTransportSurfaceDelegate::AddChildWindowToBrowser() is called in
    // production code here. However, to remove dependency from
    // gpu/ipc/service/image_transport_presenter_delegate.h, here we directly
    // executes the required minimum code.
    if (parent_window_)
      ::SetParent(presenter->window(), parent_window_);

    return presenter;
  }

  scoped_refptr<GLContext> CreateGLContext(scoped_refptr<GLSurface> surface) {
    scoped_refptr<GLContext> context =
        gl::init::CreateGLContext(nullptr, surface.get(), GLContextAttribs());
    EXPECT_TRUE(context->MakeCurrent(surface.get()));
    return context;
  }

  // Wait for |presenter_| to present asynchronously check the swap result.
  void PresentAndCheckSwapResult(gfx::SwapResult expected_swap_result) {
    base::RunLoop wait_for_present;
    presenter_->Present(
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               gfx::SwapResult expected_swap_result,
               gfx::SwapCompletionResult result) {
              EXPECT_EQ(expected_swap_result, result.swap_result);
              quit_closure.Run();
            },
            wait_for_present.QuitClosure(), expected_swap_result),
        base::DoNothing(), gfx::FrameData());
    wait_for_present.Run();
  }

  HWND parent_window_;
  scoped_refptr<DCompPresenter> presenter_;
  scoped_refptr<GLSurface> gl_surface_;
  scoped_refptr<GLContext> context_;
  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

// Ensure that the overlay image isn't presented again unless it changes.
TEST_F(DCompPresenterTest, NoPresentTwice) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_FALSE(swap_chain);

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  swap_chain = presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  UINT last_present_count = 0;
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->GetLastPresentCount(&last_present_count));

  // One present is normal, and a second present because it's the first frame
  // and the other buffer needs to be drawn to.
  EXPECT_EQ(2u, last_present_count);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_EQ(swap_chain2.Get(), swap_chain.Get());

  // It's the same image, so it should have the same swapchain.
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->GetLastPresentCount(&last_present_count));
  EXPECT_EQ(2u, last_present_count);

  // The image changed, we should get a new present.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(100, 100);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain3->GetLastPresentCount(&last_present_count));
  // the present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be set to the onscreen video size.
TEST_F(DCompPresenterTest, SwapchainSizeWithScaledOverlays) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(64, 64);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // HW supports scaled overlays.
  // The input texture size is maller than the window size.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  // Onscreen quad.
  gfx::Rect quad_rect = gfx::Rect(100, 100);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc(&desc));
  // Onscreen quad_rect.size is (100, 100).
  EXPECT_EQ(100u, desc.BufferDesc.Width);
  EXPECT_EQ(100u, desc.BufferDesc.Height);

  // Clear SwapChainPresenters
  // Must do Clear first because the swap chain won't resize immediately if
  // a new size is given unless this is the very first time after Clear.
  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  // The input texture size is bigger than the window size.
  quad_rect = gfx::Rect(32, 48);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc(&desc));
  // Onscreen quad_rect.size is (32, 48).
  EXPECT_EQ(32u, desc.BufferDesc.Width);
  EXPECT_EQ(48u, desc.BufferDesc.Height);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is not support - swapchain should be the onscreen video size.
TEST_F(DCompPresenterTest, SwapchainSizeWithoutScaledOverlays) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(80, 80);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  gfx::Rect quad_rect = gfx::Rect(42, 42);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc(&desc));
  // Onscreen quad_rect.size is (42, 42).
  EXPECT_EQ(42u, desc.BufferDesc.Width);
  EXPECT_EQ(42u, desc.BufferDesc.Height);

  // The input texture size is smaller than the window size.
  quad_rect = gfx::Rect(124, 136);

  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = quad_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);

  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc(&desc));
  // Onscreen quad_rect.size is (124, 136).
  EXPECT_EQ(124u, desc.BufferDesc.Width);
  EXPECT_EQ(136u, desc.BufferDesc.Height);
}

// Test protected video flags
TEST_F(DCompPresenterTest, ProtectedVideos) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(1280, 720);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  gfx::Size window_size(640, 360);

  // Clear video
  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->quad_rect = gfx::Rect(window_size);
    params->content_rect = gfx::Rect(texture_size);
    params->color_space = gfx::ColorSpace::CreateREC709();
    params->protected_video_type = gfx::ProtectedVideoType::kClear;

    presenter_->ScheduleDCLayer(std::move(params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        presenter_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC desc;
    EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc(&desc));
    auto display_only_flag = desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    auto hw_protected_flag = desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(0u, display_only_flag);
    EXPECT_EQ(0u, hw_protected_flag);
  }

  // Software protected video
  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->quad_rect = gfx::Rect(window_size);
    params->content_rect = gfx::Rect(texture_size);
    params->color_space = gfx::ColorSpace::CreateREC709();
    params->protected_video_type = gfx::ProtectedVideoType::kSoftwareProtected;

    presenter_->ScheduleDCLayer(std::move(params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        presenter_->GetLayerSwapChainForTesting(0);
    ASSERT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC Desc;
    EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc(&Desc));
    auto display_only_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    auto hw_protected_flag = Desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    EXPECT_EQ(DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY, display_only_flag);
    EXPECT_EQ(0u, hw_protected_flag);
  }

  // TODO(magchen): Add a hardware protected video test when hardware protected
  // video support is enabled by default in the Intel driver and Chrome
}

class DCompPresenterPixelTest : public DCompPresenterTest {
 public:
  DCompPresenterPixelTest()
      : window_(&platform_delegate_, gfx::Rect(100, 100)) {
    parent_window_ = window_.hwnd();
  }

 protected:
  void SetUp() override {
    static_cast<ui::PlatformWindow*>(&window_)->Show();
    DCompPresenterTest::SetUp();
  }

  void TearDown() override {
    // Test harness times out without DestroyWindow() here.
    if (IsWindow(parent_window_))
      DestroyWindow(parent_window_);
    DCompPresenterTest::TearDown();
  }

  // DCompPresenter is surfaceless--it's root surface is achieved
  // via an overlay the size of the window.
  void InitializeRootAndScheduleRootSurface(const gfx::Size& window_size,
                                            SkColor4f initial_color) {
    // Schedule the root surface as a normal overlay
    std::unique_ptr<DCLayerOverlayParams> params =
        std::make_unique<DCLayerOverlayParams>();
    params->z_order = 0;
    params->quad_rect = gfx::Rect(window_size);
    params->content_rect = params->quad_rect;
    params->overlay_image = CreateDCompSurface(window_size, initial_color);
    EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(params)));
  }

  void InitializeForPixelTest(const gfx::Size& window_size,
                              const gfx::Size& texture_size,
                              const gfx::Rect& content_rect,
                              const gfx::Rect& quad_rect) {
    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size);
    ASSERT_NE(texture, nullptr);

    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = content_rect;
    params->quad_rect = quad_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    Sleep(1000);
  }

  // If |scale_via_buffer| is true, use the content/quad rects to scale the
  // buffer. If it is false, use the overlay's transform to scale the visual.
  void RunNearestNeighborTest(bool scale_via_buffer) {
    const gfx::Size window_size(100, 100);

    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

    InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

    auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
    dc_layer_params->content_rect = gfx::Rect(2, 2);
    dc_layer_params->overlay_image = CreateDCompSurface(
        dc_layer_params->content_rect.size(), SkColors::kBlack,
        {{gfx::Rect(0, 0, 1, 1), SkColors::kRed},
         {gfx::Rect(1, 0, 1, 1), SkColors::kGreen},
         {gfx::Rect(0, 1, 1, 1), SkColors::kBlue},
         {gfx::Rect(1, 1, 1, 1), SkColors::kBlack}});
    dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();
    dc_layer_params->z_order = 1;
    dc_layer_params->nearest_neighbor_filter = true;

    if (scale_via_buffer) {
      // Pick a large quad rect so the buffer is scaled up
      dc_layer_params->quad_rect = gfx::Rect(window_size);
    } else {
      // Pick a small quad rect and assign a transform so the quad rect is
      // scaled up
      dc_layer_params->quad_rect = dc_layer_params->content_rect;
      dc_layer_params->transform = gfx::Transform::MakeScale(
          window_size.width() / dc_layer_params->quad_rect.width(),
          window_size.height() / dc_layer_params->quad_rect.height());
    }

    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    SkBitmap pixels = GLTestHelper::ReadBackWindow(window_.hwnd(), window_size);

    EXPECT_SKCOLOR_EQ(
        SK_ColorRED, GLTestHelper::GetColorAtPoint(pixels, gfx::Point(49, 49)));
    EXPECT_SKCOLOR_EQ(SK_ColorGREEN, GLTestHelper::GetColorAtPoint(
                                         pixels, gfx::Point(51, 49)));
    EXPECT_SKCOLOR_EQ(SK_ColorBLUE, GLTestHelper::GetColorAtPoint(
                                        pixels, gfx::Point(49, 51)));
    EXPECT_SKCOLOR_EQ(SK_ColorBLACK, GLTestHelper::GetColorAtPoint(
                                         pixels, gfx::Point(51, 51)));
  }

  // These colors are used for |CheckOverlayExactlyFillsHole|.
  // The initial root surface color
  const SkColor4f kRootSurfaceInitialColor = SkColors::kBlack;
  // The "hole" in the root surface that we expect the overlay to completely
  // cover.
  const SkColor4f kRootSurfaceHiddenColor = SkColors::kRed;
  // The color of the visible portion of the overlay image.
  const SkColor4f kOverlayExpectedColor = SkColors::kBlue;
  // The color of the portion of the overlay image hidden by the content rect.
  const SkColor4f kOverlayImageHiddenColor = SkColors::kGreen;

  const char* CheckOverlayExactlyFillsHoleColorToString(SkColor4f c) {
    if (c == kRootSurfaceInitialColor) {
      return "RootSurfaceInitialColor";
    } else if (c == kRootSurfaceHiddenColor) {
      return "RootSurfaceHiddenColor";
    } else if (c == kOverlayExpectedColor) {
      return "OverlayExpectedColor";
    } else if (c == kOverlayImageHiddenColor) {
      return "OverlayImageHiddenColor";
    }
    return "unexpected color";
  }

  // Check that |fit_in_hole_overlay| exactly covers |root_surface_hole|.
  // This test uses the colors defined above to test for coverage: the resulting
  // image should only contain |kOverlayExpectedColor| where the hole was and
  // |kRootSurfaceInitialColor| elsewhere.
  void CheckOverlayExactlyFillsHole(
      const gfx::Size& window_size,
      const gfx::Rect& root_surface_hole,
      std::unique_ptr<DCLayerOverlayParams> fit_in_hole_overlay) {
    EXPECT_TRUE(gfx::Rect(window_size).Contains(root_surface_hole));

    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

    auto root_surface = std::make_unique<DCLayerOverlayParams>();
    root_surface->quad_rect = gfx::Rect(window_size);
    root_surface->content_rect = gfx::Rect(window_size);
    root_surface->overlay_image =
        CreateDCompSurface(window_size, kRootSurfaceInitialColor,
                           {{root_surface_hole, kRootSurfaceHiddenColor}});
    root_surface->color_space = gfx::ColorSpace::CreateSRGB();
    root_surface->z_order = 0;
    presenter_->ScheduleDCLayer(std::move(root_surface));

    presenter_->ScheduleDCLayer(std::move(fit_in_hole_overlay));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    auto pixels = GLTestHelper::ReadBackWindow(window_.hwnd(), window_size);

    for (int y = 0; y < window_size.height(); y++) {
      for (int x = 0; x < window_size.width(); x++) {
        gfx::Point location(x, y);
        bool in_hole = root_surface_hole.Contains(location);
        SkColor actual_color = GLTestHelper::GetColorAtPoint(pixels, location);
        SkColor expected_color =
            (in_hole ? kOverlayExpectedColor : kRootSurfaceInitialColor)
                .toSkColor();
        if (actual_color != expected_color) {
          ADD_FAILURE() << "Unexpected pixel at " << location.ToString()
                        << " (in_hole=" << in_hole << ")\n"
                        << "Expected:\n  " << std::hex << "0x" << expected_color
                        << " ("
                        << CheckOverlayExactlyFillsHoleColorToString(
                               SkColor4f::FromColor(expected_color))
                        << ")\n"
                        << "But got:\n  "
                        << "0x" << actual_color << " ("
                        << CheckOverlayExactlyFillsHoleColorToString(
                               SkColor4f::FromColor(actual_color))
                        << ")";
          return;
        }
      }
    }
  }

  TestPlatformDelegate platform_delegate_;
  ui::WinWindow window_;
};

class DCompPresenterVideoPixelTest : public DCompPresenterPixelTest {
 protected:
  void TestVideo(const gfx::ColorSpace& color_space,
                 SkColor expected_color,
                 bool check_color) {
    if (!presenter_) {
      return;
    }

    gfx::Size window_size(100, 100);
    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();

    gfx::Size texture_size(50, 50);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size);
    ASSERT_NE(texture, nullptr);

    {
      auto params = std::make_unique<DCLayerOverlayParams>();
      params->overlay_image.emplace(texture_size, texture);
      params->content_rect = gfx::Rect(texture_size);
      params->quad_rect = gfx::Rect(texture_size);
      params->color_space = color_space;
      presenter_->ScheduleDCLayer(std::move(params));
    }

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    {
      auto params = std::make_unique<DCLayerOverlayParams>();
      params->overlay_image.emplace(texture_size, texture);
      params->content_rect = gfx::Rect(texture_size);
      params->quad_rect = gfx::Rect(window_size);
      params->color_space = color_space;
      presenter_->ScheduleDCLayer(std::move(params));
    }

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
    Sleep(1000);

    if (check_color) {
      EXPECT_SKCOLOR_CLOSE(
          expected_color,
          GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
          kMaxColorChannelDeviation);
    }
  }
};

TEST_F(DCompPresenterVideoPixelTest, BT601) {
  TestVideo(gfx::ColorSpace::CreateREC601(), SkColorSetRGB(0xdb, 0x81, 0xe8),
            true);
}

TEST_F(DCompPresenterVideoPixelTest, BT709) {
  TestVideo(gfx::ColorSpace::CreateREC709(), SkColorSetRGB(0xe1, 0x90, 0xeb),
            true);
}

TEST_F(DCompPresenterVideoPixelTest, SRGB) {
  // SRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGB(), SK_ColorTRANSPARENT, false);
}

TEST_F(DCompPresenterVideoPixelTest, SCRGBLinear) {
  // SCRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGBLinear(), SK_ColorTRANSPARENT, false);
}

TEST_F(DCompPresenterVideoPixelTest, InvalidColorSpace) {
  // Invalid color space should be treated as BT.709
  TestVideo(gfx::ColorSpace(), SkColorSetRGB(0xe1, 0x90, 0xeb), true);
}

TEST_F(DCompPresenterPixelTest, SoftwareVideoSwapchain) {
  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size y_size(50, 50);
  size_t stride = y_size.width();

  std::vector<uint8_t> nv12_pixmap(stride * 3 * y_size.height() / 2, 0xff);

  auto params = std::make_unique<DCLayerOverlayParams>();
  params->overlay_image =
      DCLayerOverlayImage(y_size, nv12_pixmap.data(), stride);
  params->content_rect = gfx::Rect(y_size);
  params->quad_rect = gfx::Rect(window_size);
  params->color_space = gfx::ColorSpace::CreateREC709();
  presenter_->ScheduleDCLayer(std::move(params));

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xff, 0xb7, 0xff);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, VideoHandleSwapchain) {
  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, SkipVideoLayerEmptyBoundsRect) {
  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect;  // Layer with empty bounds rect.
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, SkipVideoLayerEmptyContentsRect) {
  // Swap chain size is overridden to onscreen size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Layer with empty content rect.
  auto params = std::make_unique<DCLayerOverlayParams>();
  params->overlay_image.emplace(texture_size, texture);
  params->quad_rect = gfx::Rect(window_size);
  params->color_space = gfx::ColorSpace::CreateREC709();
  presenter_->ScheduleDCLayer(std::move(params));

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, NV12SwapChain) {
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
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_NV12, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, YUY2SwapChain) {
  if (context_ && context_->GetVersionInfo() &&
      context_->GetVersionInfo()->driver_vendor.find("AMD") !=
          std::string::npos) {
    GTEST_SKIP()
        << "CreateSwapChainForCompositionSurfaceHandle fails with YUY2 format "
           "on Win10/AMD bot (Radeon RX550). See https://crbug.com/967860.";
  }

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
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_YUY2, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_F(DCompPresenterPixelTest, NonZeroBoundsOffset) {
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
    EXPECT_SKCOLOR_CLOSE(expected_color,
                         GLTestHelper::GetColorAtPoint(pixels, point),
                         kMaxColorChannelDeviation)
        << " at " << point.ToString();
  }
}

TEST_F(DCompPresenterPixelTest, ResizeVideoLayer) {
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // (1) Test if swap chain is overridden to window size (100, 100).
  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(texture_size);
    params->quad_rect = gfx::Rect(window_size);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // (2) Test if swap chain is overridden to window size (100, 100).
  {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(30, 30);
    params->quad_rect = gfx::Rect(window_size);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }
  swap_chain = presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
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
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(50, 50);
    params->quad_rect = on_screen_rect;
    params->clip_rect = on_screen_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain is set to monitor/onscreen size.
  swap_chain = presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(static_cast<UINT>(monitor_size.width()), desc.Width);
  EXPECT_EQ(static_cast<UINT>(monitor_size.height()), desc.Height);

  gfx::Transform transform;
  gfx::Point offset;
  gfx::Rect clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(0, &transform, &offset,
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
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->overlay_image.emplace(texture_size, texture);
    params->content_rect = gfx::Rect(50, 50);
    params->quad_rect = on_screen_rect;
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain is set to monitor size (100, 100).
  swap_chain = presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  presenter_->GetSwapChainVisualInfoForTesting(0, &transform, &offset,
                                               &clip_rect);
  EXPECT_EQ(gfx::Rect(monitor_size), transform.MapRect(gfx::Rect(100, 100)));
}

TEST_F(DCompPresenterPixelTest, SwapChainImage) {
  if (context_ && context_->GetVersionInfo() &&
      context_->GetVersionInfo()->driver_vendor.find("AMD") !=
          std::string::npos) {
    GTEST_SKIP() << "Fails on AMD RX 5500 XT. https://crbug.com/1152565.";
  }

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

  ASSERT_HRESULT_SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(
      d3d11_device.Get(), &desc, nullptr, &swap_chain));
  ASSERT_TRUE(swap_chain);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
  ASSERT_HRESULT_SUCCEEDED(
      swap_chain->GetBuffer(1u, IID_PPV_ARGS(&front_buffer_texture)));
  ASSERT_TRUE(front_buffer_texture);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
  ASSERT_TRUE(
      SUCCEEDED(swap_chain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer_texture))));
  ASSERT_TRUE(back_buffer_texture);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  ASSERT_HRESULT_SUCCEEDED(d3d11_device->CreateRenderTargetView(
      back_buffer_texture.Get(), nullptr, &rtv));
  ASSERT_TRUE(rtv);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device->GetImmediateContext(&context);
  ASSERT_TRUE(context);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  DXGI_PRESENT_PARAMETERS present_params = {};
  present_params.DirtyRectsCount = 0;
  present_params.pDirtyRects = nullptr;

  // Clear to red and present.
  {
    float clear_color[] = {1.0, 0.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_HRESULT_SUCCEEDED(swap_chain->Present1(0, 0, &present_params));

    auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
    dc_layer_params->overlay_image =
        DCLayerOverlayImage(swap_chain_size, swap_chain);
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);
    dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();
    dc_layer_params->z_order = 1;

    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Clear to green and present.
  {
    float clear_color[] = {0.0, 1.0, 0.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    ASSERT_HRESULT_SUCCEEDED(swap_chain->Present1(0, 0, &present_params));

    auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
    dc_layer_params->overlay_image =
        DCLayerOverlayImage(swap_chain_size, swap_chain);
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);
    dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();

    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorGREEN;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Present without clearing.  This will flip front and back buffers so the
  // previous rendered contents (red) will become visible again.
  {
    ASSERT_HRESULT_SUCCEEDED(swap_chain->Present1(0, 0, &present_params));

    auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
    dc_layer_params->overlay_image =
        DCLayerOverlayImage(swap_chain_size, swap_chain);
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);
    dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();

    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Clear to blue without present.
  {
    float clear_color[] = {0.0, 0.0, 1.0, 1.0};
    context->ClearRenderTargetView(rtv.Get(), clear_color);

    auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
    dc_layer_params->overlay_image =
        DCLayerOverlayImage(swap_chain_size, swap_chain);
    dc_layer_params->content_rect = gfx::Rect(swap_chain_size);
    dc_layer_params->quad_rect = gfx::Rect(window_size);
    dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();

    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }
}

// Test that the overlay quad rect's offset is affected by its transform.
TEST_F(DCompPresenterPixelTest, QuadOffsetAppliedAfterTransform) {
  // Our overlay quad rect is at 0,50 50x50 and scaled down by 1/2. Since we
  // expect the transform to affect the quad rect offset, we expect the output
  // rect to be at 0,25 25x25.
  const gfx::Rect quad_rect(gfx::Point(0, 50), gfx::Size(50, 50));
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(0.5, gfx::Vector2dF()));

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  auto dc_layer_params = std::make_unique<DCLayerOverlayParams>();
  dc_layer_params->overlay_image =
      CreateDCompSurface(quad_rect.size(), SkColors::kRed);
  dc_layer_params->content_rect = gfx::Rect(quad_rect.size());
  dc_layer_params->quad_rect = quad_rect;
  dc_layer_params->transform = quad_to_root_transform;
  dc_layer_params->color_space = gfx::ColorSpace::CreateSRGB();
  dc_layer_params->z_order = 1;

  presenter_->ScheduleDCLayer(std::move(dc_layer_params));
  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  // We expect DComp to display the overlay with the same bounds as if viz were
  // to composite it.
  const gfx::Rect mapped_quad_rect = quad_to_root_transform.MapRect(quad_rect);

  SkBitmap pixels = GLTestHelper::ReadBackWindow(window_.hwnd(), window_size);

  // Check the top edge of the scaled overlay
  EXPECT_SKCOLOR_CLOSE(SK_ColorBLACK,
                       GLTestHelper::GetColorAtPoint(
                           pixels, gfx::Point(0, mapped_quad_rect.y() - 1)),
                       kMaxColorChannelDeviation);
  EXPECT_SKCOLOR_CLOSE(SK_ColorRED,
                       GLTestHelper::ReadBackWindowPixel(
                           window_.hwnd(), gfx::Point(0, mapped_quad_rect.y())),
                       kMaxColorChannelDeviation);

  // Check the bottom edge of the scaled overlay
  EXPECT_SKCOLOR_CLOSE(
      SK_ColorRED,
      GLTestHelper::GetColorAtPoint(
          pixels, gfx::Point(0, mapped_quad_rect.bottom() - 1)),
      kMaxColorChannelDeviation);
  EXPECT_SKCOLOR_CLOSE(

      SK_ColorBLACK,
      GLTestHelper::GetColorAtPoint(pixels,
                                    gfx::Point(0, mapped_quad_rect.bottom())),
      kMaxColorChannelDeviation);
}

// Test that scaling a (very) small texture up works with nearest neighbor
// filtering using the content rect and quad rects.
TEST_F(DCompPresenterPixelTest, NearestNeighborFilteringScaleViaBuffer) {
  RunNearestNeighborTest(true);
}

// Test that scaling a (very) small texture up works with nearest neighbor
// filtering using the overlay's transform.
TEST_F(DCompPresenterPixelTest, NearestNeighborFilteringScaleViaTransform) {
  RunNearestNeighborTest(false);
}

// Test that the |content_rect| of an overlay scales the buffer to fit the
// display rect, if needed.
TEST_F(DCompPresenterPixelTest, ContentRectScalesUpBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect root_surface_hole = gfx::Rect(5, 10, 50, 75);

  // Provide an overlay that's smaller than the hole it needs to fill
  auto overlay = std::make_unique<DCLayerOverlayParams>();
  overlay->content_rect = gfx::Rect(1, 1);
  overlay->quad_rect = root_surface_hole;
  overlay->overlay_image =
      CreateDCompSurface(overlay->content_rect.size(), kOverlayExpectedColor);
  overlay->color_space = gfx::ColorSpace::CreateSRGB();
  overlay->z_order = 1;
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay scales the buffer to fit the
// display rect, if needed.
TEST_F(DCompPresenterPixelTest, ContentRectScalesDownBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect root_surface_hole = gfx::Rect(5, 10, 50, 75);

  // Provide an overlay that's larger than the hole it needs to fill
  auto overlay = std::make_unique<DCLayerOverlayParams>();
  overlay->content_rect = gfx::Rect(75, 100);
  overlay->quad_rect = root_surface_hole;
  overlay->overlay_image =
      CreateDCompSurface(overlay->content_rect.size(), kOverlayExpectedColor);
  overlay->color_space = gfx::ColorSpace::CreateSRGB();
  overlay->z_order = 1;
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay clips portions of the buffer.
TEST_F(DCompPresenterPixelTest, ContentRectClipsBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect tex_coord = gfx::Rect(1, 2, 50, 60);
  const gfx::Rect root_surface_hole =
      gfx::Rect(gfx::Point(20, 25), tex_coord.size());

  // Ensure the overlay is not scaled.
  EXPECT_EQ(root_surface_hole.width(), tex_coord.width());
  EXPECT_EQ(root_surface_hole.height(), tex_coord.height());

  // Provide an overlay that is the right size, but has extra data that is
  // clipped via content rect
  auto overlay = std::make_unique<DCLayerOverlayParams>();
  overlay->content_rect = tex_coord;
  overlay->quad_rect = root_surface_hole;
  overlay->overlay_image =
      CreateDCompSurface(window_size, kOverlayImageHiddenColor,
                         {{tex_coord, kOverlayExpectedColor}});
  overlay->color_space = gfx::ColorSpace::CreateSRGB();
  overlay->z_order = 1;
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay can clip a buffer and scale it's
// contents.
TEST_F(DCompPresenterPixelTest, ContentRectClipsAndScalesBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect tex_coord = gfx::Rect(5, 10, 15, 20);
  const gfx::Rect root_surface_hole =
      gfx::Rect(gfx::Point(20, 25), gfx::Size(50, 60));

  // Ensure the overlay is scaled
  EXPECT_NE(root_surface_hole.width(), tex_coord.width());
  EXPECT_NE(root_surface_hole.height(), tex_coord.height());

  // Provide an overlay that needs to be scaled and has extra data that is
  // clipped via content rect
  auto overlay = std::make_unique<DCLayerOverlayParams>();
  overlay->content_rect = tex_coord;
  overlay->quad_rect = root_surface_hole;
  overlay->overlay_image =
      CreateDCompSurface(window_size, kOverlayImageHiddenColor,
                         {{tex_coord, kOverlayExpectedColor}});
  overlay->color_space = gfx::ColorSpace::CreateSRGB();
  overlay->z_order = 1;

  // Use nearest neighbor to avoid interpolation at the edges of the content
  // rect
  overlay->nearest_neighbor_filter = true;

  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

class DCompPresenterSkiaGoldTest : public DCompPresenterPixelTest {
 protected:
  void SetUp() override {
    DCompPresenterPixelTest::SetUp();

    // |pixel_diff_| only needs to be initialized once per suite, but depends on
    // | DCompPresenterTest::SetUp()| so is initialized here.
    if (!pixel_diff_.has_value()) {
      pixel_diff_.emplace();

      ASSERT_TRUE(context_);
      const ui::test::TestEnvironmentMap test_environment = {
          {ui::test::TestEnvironmentKey::kSystemVersion,
           base::win::OSInfo::GetInstance()->release_id()},
          {ui::test::TestEnvironmentKey::kGpuDriverVendor,
           context_->GetVersionInfo()->driver_vendor},
          {ui::test::TestEnvironmentKey::kGpuDriverVersion,
           context_->GetVersionInfo()->driver_version},
          {ui::test::TestEnvironmentKey::kGlRenderer,
           context_->GetGLRenderer()},

      };

      pixel_diff_->Init(::testing::UnitTest::GetInstance()
                            ->current_test_info()
                            ->test_suite_name(),
                        kSkiaGoldPixelDiffCorpus, test_environment);
    }
  }

  void TearDown() override {
    DCompPresenterPixelTest::TearDown();
    test_initialized_ = false;
  }

  void InitializeTest(const gfx::Size& window_size) {
    ASSERT_FALSE(test_initialized_)
        << "InitializeTest should only be called once per test";
    test_initialized_ = true;

    ResizeWindow(window_size);

    capture_names_in_test_.clear();
  }

  void ResizeWindow(const gfx::Size& window_size) {
    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));
    window_size_ = window_size;
  }

  // |capture_name| identifies this screenshot and is appended to the skia gold
  // remote test name. Empty string is allowed, e.g. for tests that only have
  // one screenshot.
  // Tests should consider passing meaningful capture names if it helps make
  // them easier to understand and debug.
  // Unique capture names are required if a test checks multiple screenshots.
  void PresentAndCheckScreenshot(
      std::string capture_name = std::string(),
      const base::Location& caller_location = FROM_HERE) {
    ASSERT_TRUE(test_initialized_) << "Must call InitializeTest first";

    if (capture_names_in_test_.contains(capture_name)) {
      ADD_FAILURE_AT(caller_location.file_name(), caller_location.line_number())
          << "Capture names must be unique in a test. Capture name \""
          << capture_name << "\" is already used.";
      return;
    }
    capture_names_in_test_.insert(capture_name);

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    std::string screenshot_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    if (!capture_name.empty()) {
      screenshot_name = base::StringPrintf("%s/%s", screenshot_name.c_str(),
                                           capture_name.c_str());
    }

    SkBitmap window_readback =
        GLTestHelper::ReadBackWindow(window_.hwnd(), window_size_);
    ASSERT_TRUE(pixel_diff_);
    if (!pixel_diff_->CompareScreenshot(screenshot_name, window_readback)) {
      ADD_FAILURE_AT(caller_location.file_name(), caller_location.line_number())
          << "Screenshot mismatch for "
          << (capture_name.empty() ? "(unnamed capture)" : capture_name);
    }
  }

  const gfx::Size& current_window_size() const { return window_size_; }

 private:
  absl::optional<ui::test::SkiaGoldPixelDiff> pixel_diff_;

  // |true|, if |InitializeTest| has been called.
  bool test_initialized_ = false;

  // The size of the window and screenshots, in pixels.
  gfx::Size window_size_;

  // The values of the |capture_name| parameter of |PresentAndCheckScreenshot|
  // seen in the test so far.
  base::flat_set<std::string> capture_names_in_test_;
};

TEST_F(DCompPresenterSkiaGoldTest, NonAxisPerservingTransform) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = std::make_unique<DCLayerOverlayParams>();
  overlay->content_rect = gfx::Rect(50, 50);
  overlay->quad_rect = gfx::Rect(50, 50);
  overlay->overlay_image =
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kRed);
  overlay->z_order = 1;

  // Center and partially rotate the overlay
  overlay->transform.Translate(50, 50);
  overlay->transform.Rotate(15);
  overlay->transform.Translate(-25, -25);

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

class DCompPresenterBufferCountTest : public DCompPresenterTest,
                                      public testing::WithParamInterface<bool> {
 public:
  static const char* GetParamName(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "DCompTripleBufferVideoSwapChain" : "default";
  }

 protected:
  void SetUp() override {
    if (GetParam()) {
      enabled_features_.InitWithFeatures(
          {features::kDCompTripleBufferVideoSwapChain}, {});
    } else {
      enabled_features_.InitWithFeatures(
          {}, {features::kDCompTripleBufferVideoSwapChain});
    }

    DCompPresenterTest::SetUp();
  }

  base::test::ScopedFeatureList enabled_features_;
};

TEST_P(DCompPresenterBufferCountTest, VideoSwapChainBufferCount) {
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  constexpr gfx::Size texture_size(50, 50);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  ASSERT_TRUE(d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  auto params = std::make_unique<DCLayerOverlayParams>();
  params->overlay_image.emplace(texture_size, texture);
  params->content_rect = gfx::Rect(texture_size);
  params->quad_rect = gfx::Rect(window_size);
  params->color_space = gfx::ColorSpace::CreateREC709();
  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(params)));

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  auto swap_chain = presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // The expected size is window_size(100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);
  if (GetParam()) {
    EXPECT_EQ(3u, desc.BufferCount);
  } else {
    EXPECT_EQ(2u, desc.BufferCount);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DCompPresenterBufferCountTest,
                         testing::Bool(),
                         &DCompPresenterBufferCountTest::GetParamName);

}  // namespace gl
