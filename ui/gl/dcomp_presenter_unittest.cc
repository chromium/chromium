// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_presenter.h"

#include <limits>
#include <memory>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/test/sk_color_eq.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/dc_layer_tree.h"
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

void ClearRect(IDCompositionSurface* surface,
               const gfx::Rect& update_rect,
               SkColor4f update_color) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
  d3d11_device->GetImmediateContext(&immediate_context);

  HRESULT hr = S_OK;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> update_texture;
  RECT rect = update_rect.ToRECT();
  POINT update_offset;
  hr = surface->BeginDraw(&rect, IID_PPV_ARGS(&update_texture), &update_offset);
  CHECK_EQ(S_OK, hr);

  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  hr =
      d3d11_device->CreateRenderTargetView(update_texture.Get(), nullptr, &rtv);
  CHECK_EQ(S_OK, hr);

  immediate_context->ClearRenderTargetView(rtv.Get(),
                                           update_color.premul().vec());

  hr = surface->EndDraw();
  CHECK_EQ(S_OK, hr);
}

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
  hr = dcomp_device->CreateSurface(
      surface_size.width(), surface_size.height(), DXGI_FORMAT_B8G8R8A8_UNORM,
      initial_color.isOpaque() ? DXGI_ALPHA_MODE_IGNORE
                               : DXGI_ALPHA_MODE_PREMULTIPLIED,
      &surface);
  CHECK_EQ(S_OK, hr);

  // Add a rect that initializes the whole surface to |initial_color|.
  rectangles_back_to_front.insert(rectangles_back_to_front.begin(),
                                  {gfx::Rect(surface_size), initial_color});

  for (const auto& [draw_rect, color] : rectangles_back_to_front) {
    CHECK(gfx::Rect(surface_size).Contains(draw_rect));
    ClearRect(surface.Get(), draw_rect, color);
  }

  return DCLayerOverlayImage(surface_size, surface);
}

// Create a |DCLayerOverlayParams| from an |image| and set the |content_rect| to
// the bounds of |image|, or |content_rect_override|, if set.
std::unique_ptr<DCLayerOverlayParams> CreateParamsFromImage(
    DCLayerOverlayImage image,
    absl::optional<gfx::RectF> content_rect_override = {}) {
  auto params = std::make_unique<DCLayerOverlayParams>();
  params->content_rect =
      content_rect_override.value_or(gfx::RectF(image.size()));
  params->overlay_image = std::move(image);
  return params;
}

}  // namespace

class DCompPresenterTest : public testing::Test {
 public:
  DCompPresenterTest() : parent_window_(ui::GetHiddenWindow()) {}

 protected:
  void SetUp() override {
    // Without this, the following check always fails.
    display_ = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true, /*gpu_preference=*/gl::GpuPreference::kDefault);

    gl_surface_ = init::CreateOffscreenGLSurface(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());

    scoped_refptr<GLContext> context = gl::init::CreateGLContext(
        nullptr, gl_surface_.get(), GLContextAttribs());
    EXPECT_TRUE(context->MakeCurrent(gl_surface_.get()));
    context_ = std::move(context);

    // These tests are assumed to run on battery.
    fake_power_monitor_source_.SetOnBatteryPower(true);

    presenter_ = CreateDCompPresenter();

    // All bots run on non-blocklisted hardware that supports DComp (>Win7)
    ASSERT_TRUE(DirectCompositionSupported());

    SetDirectCompositionScaledOverlaysSupportedForTesting(false);
    SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_NV12);
  }

  void TearDown() override {
    if (presenter_) {
      DestroyPresenter(std::move(presenter_));
    }

    context_.reset();
    gl_surface_.reset();
    gl::init::ShutdownGL(display_, false);
    display_ = nullptr;
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

  // DCompPresenter is surfaceless--it's root surface is achieved via an
  // overlay the size of the window.
  // We can also present a manual initialized root surface with specific size
  // and color.
  void InitializeRootAndScheduleRootSurface(const gfx::Size& window_size,
                                            SkColor4f initial_color) {
    // Schedule the root surface as a normal overlay
    auto params =
        CreateParamsFromImage(CreateDCompSurface(window_size, initial_color));
    params->z_order = 0;
    params->quad_rect = gfx::Rect(window_size);
    params->overlay_image = CreateDCompSurface(window_size, initial_color);
    EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(params)));
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

  raw_ptr<GLDisplay> display_ = nullptr;
  scoped_refptr<GLSurface> gl_surface_;
  scoped_refptr<GLContext> context_;

  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
  HWND parent_window_;
  scoped_refptr<DCompPresenter> presenter_;
};

// Ensure that the overlay image isn't presented again unless it changes.
TEST_F(DCompPresenterTest, NoPresentTwice) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params->quad_rect = gfx::Rect(100, 100);
    params->color_space = gfx::ColorSpace::CreateREC709();
    presenter_->ScheduleDCLayer(std::move(params));
  }

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain3->GetLastPresentCount(&last_present_count));
  // The present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be set to the onscreen video size.
TEST_F(DCompPresenterTest, SwapchainSizeWithScaledOverlays) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(80, 80);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  gfx::Rect quad_rect = gfx::Rect(42, 42);

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(1280, 720);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  gfx::Size window_size(640, 360);

  // Clear video
  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params->quad_rect = gfx::Rect(window_size);
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params->quad_rect = gfx::Rect(window_size);
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

TEST_F(DCompPresenterTest, NoBackgroundColorSurfaceForNonColorOverlays) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  auto root_surface =
      CreateParamsFromImage(CreateDCompSurface(window_size, SkColors::kBlack));
  root_surface->quad_rect = gfx::Rect(window_size);
  root_surface->z_order = 1;
  presenter_->ScheduleDCLayer(std::move(root_surface));

  PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

  const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();
  EXPECT_EQ(1u, layer_tree->GetDcompLayerCountForTesting());
  EXPECT_EQ(0u, layer_tree->GetNumSurfacesInPoolForTesting());
}

TEST_F(DCompPresenterTest, BackgroundColorSurfaceTrim) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();

  // See |TrimAfterCommit|.
  static constexpr size_t kMaxSolidColorBuffers = 12;

  // From an empty state, the surface pool will allocate surfaces on demand and
  // retain as many that are in use (and unused surfaces, up to
  // |kMaxSolidColorBuffers| total). We iterate to |kMaxSolidColorBuffers + 1|
  // to exceed this threshold.
  for (size_t num_buffers = 1; num_buffers <= kMaxSolidColorBuffers + 1;
       num_buffers++) {
    // We expect as many retained surfaces as there are unique solid color
    // overlays in the frame.
    {
      for (size_t i = 0; i < num_buffers; i++) {
        auto params = std::make_unique<DCLayerOverlayParams>();
        params->quad_rect = gfx::Rect(window_size);
        params->background_color = SkColor4f::FromColor(SkColorSetRGB(i, 0, 0));
        params->z_order = i + 1;
        presenter_->ScheduleDCLayer(std::move(params));
      }
      PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
      EXPECT_EQ(num_buffers, layer_tree->GetNumSurfacesInPoolForTesting());
    }

    // We expect retained surfaces even after we present a frame with no solid
    // color overlays.
    {
      PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
      EXPECT_EQ(std::min(num_buffers, kMaxSolidColorBuffers),
                layer_tree->GetNumSurfacesInPoolForTesting());
    }
  }
}

// Check that when there's multiple background color surfaces, the correct one
// is reused even if the order they're requested in changes.
TEST_F(DCompPresenterTest, BackgroundColorSurfaceMultipleReused) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  std::vector<SkColor4f> colors = {SkColors::kRed, SkColors::kGreen};
  std::vector<IDCompositionSurface*> surfaces_frame1(2, nullptr);
  std::vector<IDCompositionSurface*> surfaces_frame2(2, nullptr);

  const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();

  {
    for (size_t i = 0; i < colors.size(); i++) {
      auto params = std::make_unique<DCLayerOverlayParams>();
      params->quad_rect = gfx::Rect(window_size);
      params->background_color = colors[i];
      params->z_order = i + 1;
      presenter_->ScheduleDCLayer(std::move(params));
    }

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
    EXPECT_EQ(2u, layer_tree->GetNumSurfacesInPoolForTesting());

    surfaces_frame1[0] = layer_tree->GetBackgroundColorSurfaceForTesting(0);
    surfaces_frame1[1] = layer_tree->GetBackgroundColorSurfaceForTesting(1);
    // The overlays should have different background color surfaces since they
    // have different background colors.
    EXPECT_NE(surfaces_frame1[0], surfaces_frame1[1]);
  }

  {
    // Swap the colors so they appear as overlays in a different order for the
    // next frame.
    std::swap(colors[0], colors[1]);

    for (size_t i = 0; i < colors.size(); i++) {
      auto params = std::make_unique<DCLayerOverlayParams>();
      params->quad_rect = gfx::Rect(window_size);
      params->background_color = colors[i];
      params->z_order = i + 1;
      presenter_->ScheduleDCLayer(std::move(params));
    }

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
    EXPECT_EQ(2u, layer_tree->GetNumSurfacesInPoolForTesting());

    surfaces_frame2[0] = layer_tree->GetBackgroundColorSurfaceForTesting(0);
    surfaces_frame2[1] = layer_tree->GetBackgroundColorSurfaceForTesting(1);
    EXPECT_NE(surfaces_frame2[0], surfaces_frame2[1]);

    // We reversed the order of the color overlays. We expect the background
    // color surfaces to be reused, but reversed.
    EXPECT_EQ(surfaces_frame1[0], surfaces_frame2[1]);
    EXPECT_EQ(surfaces_frame1[1], surfaces_frame2[0]);
  }
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

  void InitializeForPixelTest(const gfx::Size& window_size,
                              const gfx::Size& texture_size,
                              const gfx::Rect& content_rect,
                              const gfx::Rect& quad_rect) {
    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        GetDirectCompositionD3D11Device();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size);
    ASSERT_NE(texture, nullptr);

    auto params = CreateParamsFromImage(
        DCLayerOverlayImage(texture_size, texture),
        /*content_rect_override=*/gfx::RectF(content_rect));
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

    auto dc_layer_params = CreateParamsFromImage(
        CreateDCompSurface(gfx::Size(2, 2), SkColors::kBlack,
                           {{gfx::Rect(0, 0, 1, 1), SkColors::kRed},
                            {gfx::Rect(1, 0, 1, 1), SkColors::kGreen},
                            {gfx::Rect(0, 1, 1, 1), SkColors::kBlue},
                            {gfx::Rect(1, 1, 1, 1), SkColors::kBlack}}));
    dc_layer_params->z_order = 1;
    dc_layer_params->nearest_neighbor_filter = true;

    if (scale_via_buffer) {
      // Pick a large quad rect so the buffer is scaled up
      dc_layer_params->quad_rect = gfx::Rect(window_size);
    } else {
      // Pick a small quad rect and assign a transform so the quad rect is
      // scaled up
      dc_layer_params->quad_rect =
          gfx::ToNearestRect(dc_layer_params->content_rect);
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

    auto root_surface = CreateParamsFromImage(
        CreateDCompSurface(window_size, kRootSurfaceInitialColor,
                           {{root_surface_hole, kRootSurfaceHiddenColor}}));
    root_surface->quad_rect = gfx::Rect(window_size);
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
        GetDirectCompositionD3D11Device();

    gfx::Size texture_size(50, 50);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        CreateNV12Texture(d3d11_device, texture_size);
    ASSERT_NE(texture, nullptr);

    {
      auto params =
          CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
      params->quad_rect = gfx::Rect(texture_size);
      params->color_space = color_space;
      presenter_->ScheduleDCLayer(std::move(params));
    }

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    {
      auto params =
          CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
      GetDirectCompositionD3D11Device();

  gfx::Size y_size(50, 50);
  size_t stride = y_size.width();

  std::vector<uint8_t> nv12_pixmap(stride * 3 * y_size.height() / 2, 0xff);

  auto params = CreateParamsFromImage(
      DCLayerOverlayImage(y_size, nv12_pixmap.data(), stride));
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
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Layer with empty content rect.
  auto params =
      CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture),
                            /*content_rect_override=*/gfx::RectF());
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
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // (1) Test if swap chain is overridden to window size (100, 100).
  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture),
                              /*content_rect_override=*/gfx::RectF(30, 30));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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
      GetDirectCompositionD3D11Device();
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

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
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

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
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

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
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

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
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

  auto dc_layer_params = CreateParamsFromImage(
      CreateDCompSurface(quad_rect.size(), SkColors::kRed));
  dc_layer_params->quad_rect = quad_rect;
  dc_layer_params->transform = quad_to_root_transform;
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
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(1, 1), kOverlayExpectedColor));
  overlay->quad_rect = root_surface_hole;
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
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(75, 100), kOverlayExpectedColor));
  overlay->quad_rect = root_surface_hole;
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
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(window_size, kOverlayImageHiddenColor,
                         {{tex_coord, kOverlayExpectedColor}}),
      /*content_rect_override=*/gfx::RectF(tex_coord));
  overlay->quad_rect = root_surface_hole;
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
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(window_size, kOverlayImageHiddenColor,
                         {{tex_coord, kOverlayExpectedColor}}),
      /*content_rect_override=*/gfx::RectF(tex_coord));
  overlay->quad_rect = root_surface_hole;
  overlay->z_order = 1;

  // Use nearest neighbor to avoid interpolation at the edges of the content
  // rect
  overlay->nearest_neighbor_filter = true;

  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Check that the surface backing solid color overlays is reused across frames.
// This can happen e.g. with a solid color draw quad animating its color.
TEST_F(DCompPresenterPixelTest, BackgroundColorSurfaceReuse) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
  EXPECT_TRUE(presenter_->SetDrawRectangle(gfx::Rect(window_size)));

  SkColor4f colors[] = {
      SkColors::kRed,    SkColors::kGreen, SkColors::kBlue,
      SkColors::kYellow, SkColors::kCyan,  SkColors::kMagenta,
  };

  IDCompositionSurface* background_color_surface = nullptr;

  for (const SkColor4f& color : colors) {
    auto params = std::make_unique<DCLayerOverlayParams>();
    params->quad_rect = gfx::Rect(window_size);
    params->background_color = color;
    params->z_order = 1;
    presenter_->ScheduleDCLayer(std::move(params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);

    EXPECT_SKCOLOR_EQ(color.toSkColor(), GLTestHelper::ReadBackWindowPixel(
                                             window_.hwnd(), gfx::Point(0, 0)));

    const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();

    EXPECT_EQ(1u, layer_tree->GetDcompLayerCountForTesting());
    EXPECT_EQ(1u, layer_tree->GetNumSurfacesInPoolForTesting());

    if (background_color_surface == nullptr) {
      background_color_surface =
          layer_tree->GetBackgroundColorSurfaceForTesting(0);
    }
    EXPECT_NE(background_color_surface, nullptr);
    EXPECT_EQ(background_color_surface,
              layer_tree->GetBackgroundColorSurfaceForTesting(0))
        << "DComp content for solid color overlay expected to be reused across "
           "frames";
  }
}

class DCompPresenterSkiaGoldTest : public DCompPresenterPixelTest {
 protected:
  void SetUp() override {
    DCompPresenterPixelTest::SetUp();
    ASSERT_TRUE(context_);
    const ui::test::TestEnvironmentMap test_environment = {
        {ui::test::TestEnvironmentKey::kSystemVersion,
         base::win::OSInfo::GetInstance()->release_id()},
        {ui::test::TestEnvironmentKey::kGpuDriverVendor,
         context_->GetVersionInfo()->driver_vendor},
        {ui::test::TestEnvironmentKey::kGpuDriverVersion,
         context_->GetVersionInfo()->driver_version},
        {ui::test::TestEnvironmentKey::kGlRenderer, context_->GetGLRenderer()},

    };

    pixel_diff_ = ui::test::SkiaGoldPixelDiff::GetSession(
        kSkiaGoldPixelDiffCorpus, test_environment);
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

  // An offset to move the test output off the top-left edges so that we don't
  // need to dilate the edges of |SobelSkiaGoldMatchingAlgorithm|.
  static const int kPaddingFromEdgeForAntiAliasedOutput = 5;

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

    SkBitmap window_readback =
        GLTestHelper::ReadBackWindow(window_.hwnd(), window_size_);
    CHECK(pixel_diff_);
    if (!pixel_diff_->CompareScreenshot(
            ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
                ::testing::UnitTest::GetInstance()->current_test_info(),
                capture_name.empty() ? absl::nullopt
                                     : absl::make_optional(capture_name)),
            window_readback, matching_algorithm_.get())) {
      ADD_FAILURE_AT(caller_location.file_name(), caller_location.line_number())
          << "Screenshot mismatch for "
          << (capture_name.empty() ? "(unnamed capture)" : capture_name);
    }
  }

  const gfx::Size& current_window_size() const { return window_size_; }

  void AddOverlaysForOpacityTest(
      base::RepeatingCallback<
          std::unique_ptr<DCLayerOverlayParams>(const gfx::Rect&, float)>
          get_overlay_for_opacity) {
    const int kOverlayCount = 10;
    for (int i = 0; i < kOverlayCount; i++) {
      const int width = current_window_size().width() / kOverlayCount;
      const gfx::Rect quad_rect =
          gfx::Rect(i * width, 0, width, current_window_size().height());
      const float opacity =
          static_cast<float>(i) / static_cast<float>(kOverlayCount);

      auto overlay = get_overlay_for_opacity.Run(quad_rect, opacity);
      overlay->z_order = i + 1;

      presenter_->ScheduleDCLayer(std::move(overlay));
    }
  }

 private:
  raw_ptr<ui::test::SkiaGoldPixelDiff> pixel_diff_ = nullptr;

  // The matching algorithm for goldctl to use.
  std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> matching_algorithm_;

  // |true|, if |InitializeTest| has been called.
  bool test_initialized_ = false;

  // The size of the window and screenshots, in pixels.
  gfx::Size window_size_;

  // The values of the |capture_name| parameter of |PresentAndCheckScreenshot|
  // seen in the test so far.
  base::flat_set<std::string> capture_names_in_test_;
};

// Check that a translation transform works.
TEST_F(DCompPresenterSkiaGoldTest, TransformTranslate) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(50, 50);
  overlay->z_order = 1;

  overlay->transform.Translate(25, 25);

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// Check that a scaling transform works.
TEST_F(DCompPresenterSkiaGoldTest, TransformScale) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(50, 50);
  overlay->z_order = 1;

  overlay->transform.Translate(50, 50);
  overlay->transform.Scale(1.2);
  overlay->transform.Translate(-25, -25);

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// Check that a rotation transform works.
TEST_F(DCompPresenterSkiaGoldTest, TransformRotation) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(50, 50);
  overlay->z_order = 1;

  // Center and partially rotate the overlay
  overlay->transform.Translate(50, 50);
  overlay->transform.Rotate(15);
  overlay->transform.Translate(-25, -25);

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// Check that a complex transform (i.e. non-flat) works.
TEST_F(DCompPresenterSkiaGoldTest, Transform3D) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = std::make_unique<DCLayerOverlayParams>();

  overlay->quad_rect = gfx::Rect(120, 75);

  overlay->background_color = SkColors::kGreen;

  overlay->z_order = 1;

  overlay->transform.Translate(50, 50);
  overlay->transform.ApplyPerspectiveDepth(100);
  overlay->transform.RotateAboutYAxis(45);
  overlay->transform.RotateAboutXAxis(30);
  overlay->transform.Translate(-25, -25);

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// This kind of transform is uncommon, but should be supported when rotations
// are supported.
TEST_F(DCompPresenterSkiaGoldTest, TransformShear) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(50, 50);
  overlay->z_order = 1;
  overlay->transform.Translate(50, 50);
  overlay->transform.Skew(15, 30);
  overlay->transform.Translate(-25, -25);
  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// Test that solid color overlays completely fill their display rect.
TEST_F(DCompPresenterSkiaGoldTest, SolidColorSimpleOpaque) {
  InitializeTest(gfx::Size(100, 100));

  const SkColor4f root_surface_color = SkColors::kBlack;

  InitializeRootAndScheduleRootSurface(current_window_size(),
                                       root_surface_color);

  const std::vector<std::pair<SkColor4f, gfx::Rect>> colors = {
      {SkColors::kRed, gfx::Rect(5, 10, 15, 20)},
      {SkColors::kGreen, gfx::Rect(15, 12, 15, 20)},
      {SkColors::kBlue, gfx::Rect(25, 14, 15, 20)},
      {SkColors::kWhite, gfx::Rect(35, 16, 15, 20)},
  };

  for (size_t i = 0; i < colors.size(); i++) {
    auto& [color, bounds] = colors[i];
    auto overlay = std::make_unique<DCLayerOverlayParams>();
    overlay->quad_rect = bounds;
    overlay->background_color = absl::optional<SkColor4f>(color);
    overlay->z_order = i + 1;
    presenter_->ScheduleDCLayer(std::move(overlay));
  }

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from DComp tree parameter.
TEST_F(DCompPresenterSkiaGoldTest, OpacityFromOverlay) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        auto overlay = CreateParamsFromImage(
            CreateDCompSurface(quad_rect.size(), SkColors::kWhite));
        overlay->quad_rect = quad_rect;
        overlay->opacity = opacity;
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from the overlay image itself.
TEST_F(DCompPresenterSkiaGoldTest, OpacityFromImage) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        SkColor4f overlay_color = SkColors::kWhite;
        overlay_color.fA = opacity;

        auto overlay = CreateParamsFromImage(
            CreateDCompSurface(quad_rect.size(), overlay_color));
        overlay->quad_rect = quad_rect;
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from a solid color overlay.
TEST_F(DCompPresenterSkiaGoldTest, OpacityFromSolidColor) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        SkColor4f overlay_color = SkColors::kWhite;
        overlay_color.fA = opacity;

        auto overlay = std::make_unique<DCLayerOverlayParams>();
        overlay->quad_rect = quad_rect;
        overlay->background_color = absl::optional<SkColor4f>(overlay_color);
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Check that an overlay with a DComp surface will visually reflect draws to the
// surface if its dcomp_surface_serial changes. This requires DCLayerTree to
// call Commit, even if no other tree properties change.
TEST_F(DCompPresenterSkiaGoldTest, SurfaceSerialForcesCommit) {
  InitializeTest(gfx::Size(100, 100));

  const std::vector<SkColor4f> colors = {SkColors::kRed, SkColors::kGreen,
                                         SkColors::kBlue, SkColors::kWhite};

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
      gl::GetDirectCompositionDevice();

  Microsoft::WRL::ComPtr<IDCompositionSurface> surface;
  ASSERT_HRESULT_SUCCEEDED(dcomp_device->CreateSurface(
      current_window_size().width(), current_window_size().height(),
      DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_IGNORE, &surface));
  uint64_t surface_serial = 0;

  ClearRect(surface.Get(), gfx::Rect(current_window_size()), SkColors::kBlack);

  for (size_t i = 0; i < colors.size(); i++) {
    const auto color = colors[i];

    ClearRect(surface.Get(), gfx::Rect(i * 10, i * 5, 15, 15), color);
    surface_serial++;

    auto overlay = CreateParamsFromImage(
        DCLayerOverlayImage(current_window_size(), surface, surface_serial));
    overlay->quad_rect = gfx::Rect(current_window_size());
    overlay->z_order = 0;
    presenter_->ScheduleDCLayer(std::move(overlay));

    PresentAndCheckScreenshot(base::NumberToString(i));
  }
}

// Check that we support simple rounded corners.
TEST_F(DCompPresenterSkiaGoldTest, RoundedCornerSimple) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(current_window_size(), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(current_window_size());
  overlay->quad_rect.Inset(kPaddingFromEdgeForAntiAliasedOutput);
  overlay->z_order = 1;
  overlay->rounded_corner_bounds =
      gfx::RRectF(gfx::RectF(overlay->quad_rect), 25.f);
  presenter_->ScheduleDCLayer(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we support rounded corners with complex radii.
TEST_F(DCompPresenterSkiaGoldTest, RoundedCornerNonUniformRadii) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(current_window_size(), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(current_window_size());
  overlay->quad_rect.Inset(kPaddingFromEdgeForAntiAliasedOutput);
  overlay->z_order = 1;

  gfx::RRectF bounds = gfx::RRectF(gfx::RectF(overlay->quad_rect));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kUpperLeft, gfx::Vector2dF(5, 40));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kUpperRight,
                        gfx::Vector2dF(15, 30));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kLowerRight,
                        gfx::Vector2dF(25, 20));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kLowerLeft,
                        gfx::Vector2dF(35, 10));
  overlay->rounded_corner_bounds = bounds;
  presenter_->ScheduleDCLayer(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that there are no seams between solid color quads when there is a
// rounded corner clip present. Seams can appear since the solid color visual
// uses a shared image that is scaled to fit the overlay. The combination of
// scaling and soft borders implied by rounded corners can cause seams.
// This is a common case in e.g. the omnibox.
TEST_F(DCompPresenterSkiaGoldTest,
       NoSeamsBetweenAdjacentSolidColorsWithSharedRoundedCorner) {
  // We specifically don't want to ignore anti-aliasing in this test
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  gfx::RRectF bounds = gfx::RRectF(gfx::RectF(current_window_size()), 0);
  // Give the rounded rect a radius, but ensure that it is not visible so AA
  // doesn't affect this test.
  bounds.Outset(5);

  std::vector<gfx::Rect> quads = {
      gfx::Rect(55, 45, 45, 55),
      gfx::Rect(0, 45, 55, 55),

      gfx::Rect(45, 0, 55, 45),
      gfx::Rect(0, 0, 45, 45),
  };

  int overlay_z_order = 1;
  for (auto& quad : quads) {
    auto overlay = std::make_unique<DCLayerOverlayParams>();
    overlay->quad_rect = quad;
    overlay->background_color = absl::optional<SkColor4f>(SkColors::kWhite);
    overlay->z_order = overlay_z_order;
    overlay->rounded_corner_bounds = bounds;
    presenter_->ScheduleDCLayer(std::move(overlay));

    overlay_z_order++;
  }

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we translate the overlay so that both
// the right and left edges cover half a pixel.
TEST_F(DCompPresenterSkiaGoldTest, SoftBordersFromNonIntegralTranslation) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(20, 20), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(overlay->overlay_image->size());
  overlay->transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                               kPaddingFromEdgeForAntiAliasedOutput);
  overlay->transform.Translate(0.5, 0);
  overlay->z_order = 1;
  presenter_->ScheduleDCLayer(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we scale the overlay so the right edge
// covers half a pixel.
TEST_F(DCompPresenterSkiaGoldTest, SoftBordersFromNonIntegralScaling) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(20, 20), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(overlay->overlay_image->size());
  overlay->transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                               kPaddingFromEdgeForAntiAliasedOutput);
  overlay->transform.Scale(
      (static_cast<float>(overlay->quad_rect.width()) + 0.5) /
          static_cast<float>(overlay->quad_rect.width()),
      1);
  overlay->z_order = 1;
  presenter_->ScheduleDCLayer(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we create a non-integral rounded corner
// bounds so the right edge covers half a pixel.
TEST_F(DCompPresenterSkiaGoldTest,
       SoftBordersFromNonIntegralRoundedCornerBounds) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(21, 20), SkColors::kWhite));
  overlay->quad_rect = gfx::Rect(overlay->overlay_image->size());

  // DComp seems to not actually use soft borders unless there's a non-zero
  // radius.
  const double kForceDCompRoundedCornerSoftBorder =
      std::numeric_limits<float>::epsilon();

  overlay->rounded_corner_bounds = gfx::RRectF(
      gfx::RectF(0, 0, 20.5, 20), kForceDCompRoundedCornerSoftBorder);
  overlay->rounded_corner_bounds.Offset(kPaddingFromEdgeForAntiAliasedOutput,
                                        kPaddingFromEdgeForAntiAliasedOutput);
  overlay->transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                               kPaddingFromEdgeForAntiAliasedOutput);
  overlay->z_order = 1;
  presenter_->ScheduleDCLayer(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that DCLayerTree sorts overlays by their z-order instead of using the
// schedule order.
TEST_F(DCompPresenterSkiaGoldTest, OverlaysAreSortedByZOrder) {
  InitializeTest(gfx::Size(100, 100));

  // Insert overlays out of order with respect to z-ordering
  std::vector<std::pair<SkColor4f, int>> color_and_z_order = {
      {SkColors::kGreen, 2},
      {SkColors::kGreen, -1},
      {SkColors::kRed, -2},
      {SkColors::kRed, 1},
  };

  for (const auto& [color, z_order] : color_and_z_order) {
    gfx::Rect quad_rect = gfx::Rect(15 + z_order * 5, 15 + z_order * 5, 30, 30);
    auto overlay =
        CreateParamsFromImage(CreateDCompSurface(quad_rect.size(), color));
    overlay->quad_rect = quad_rect;
    overlay->z_order = z_order;

    presenter_->ScheduleDCLayer(std::move(overlay));
  }

  // Insert a translucent root plane so that we can easily see underlays
  SkColor4f translucent_blue = SkColors::kBlue;
  translucent_blue.fA = 0.5;
  InitializeRootAndScheduleRootSurface(current_window_size(), translucent_blue);

  {
    // Insert a black backdrop since our root surface is not opaque. This is not
    // strictly required, but it ensures that we explicitly make all pixels in
    // our output opaque.
    auto overlay = CreateParamsFromImage(
        CreateDCompSurface(current_window_size(), SkColors::kBlack));
    overlay->quad_rect = gfx::Rect(current_window_size());
    overlay->z_order = INT_MIN;
    presenter_->ScheduleDCLayer(std::move(overlay));
  }

  PresentAndCheckScreenshot();
}

// Check that an overlay with a non-opaque image can show a background color.
TEST_F(DCompPresenterSkiaGoldTest, ImageWithBackgroundColor) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(CreateDCompSurface(
      gfx::Size(100, 50), SkColors::kTransparent,
      {
          {gfx::Rect(5, 5, 20, 20),
           SkColor4f::FromColor(SkColorSetA(SK_ColorRED, 0x80))},
          {gfx::Rect(15, 15, 20, 20),
           SkColor4f::FromColor(SkColorSetA(SK_ColorBLUE, 0x80))},
      }));
  overlay->quad_rect = gfx::Rect(100, 50);
  overlay->background_color = SkColors::kGreen;
  overlay->z_order = 1;

  EXPECT_TRUE(presenter_->ScheduleDCLayer(std::move(overlay)));

  PresentAndCheckScreenshot();
}

// Test that we support sampling from overlay images with non-integral content
// rects. This test should output a blue square with a faint green outline.
TEST_F(DCompPresenterSkiaGoldTest, NonIntegralContentRectHalfCoverage) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  gfx::Size image_size = gfx::Size(50, 50);
  gfx::Rect image_inner_rect = gfx::Rect(image_size);
  image_inner_rect.Inset(1);
  auto overlay = CreateParamsFromImage(CreateDCompSurface(
      image_size, SkColors::kGreen, {{image_inner_rect, SkColors::kBlue}}));
  overlay->content_rect.Inset(0.5);
  overlay->quad_rect = gfx::Rect(
      gfx::Point(20, 20),
      gfx::Size(overlay->content_rect.width(), overlay->content_rect.height()));
  overlay->z_order = 1;
  presenter_->ScheduleDCLayer(std::move(overlay));

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
      GetDirectCompositionD3D11Device();
  ASSERT_TRUE(d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  auto params =
      CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
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

class DCompPresenterLetterboxingTest
    : public DCompPresenterTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    SetupScopedFeatureList();

    DCompPresenterTest::SetUp();
  }

  virtual void SetupScopedFeatureList() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDirectCompositionLetterboxVideoOptimization);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDirectCompositionLetterboxVideoOptimization);
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, DCompPresenterLetterboxingTest, testing::Bool());

TEST_P(DCompPresenterLetterboxingTest, FullScreenLetterboxingResizeVideoLayer) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1080p texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1920, 1080);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // First test if swap chain and its visual info is adjusted to fit the
  // monitor when letterboxing is generated for full screen presentation.
  const int letterboxing_height =
      (monitor_size.height() - texture_size.height()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, texture_size.width(), texture_size.height());
  gfx::Rect clip_rect = gfx::Rect(0, letterboxing_height, texture_size.width(),
                                  texture_size.height());
  gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(1920u, desc.Width);
  EXPECT_EQ(1080u, desc.Height);

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform, &visual_offset, &visual_clip_rect);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform);
    EXPECT_EQ(clip_rect, visual_clip_rect);
  }

  // Second test if swap chain visual info is adjusted to fit the monitor when
  // some negative offset from typical letterboxing positioning.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  clip_rect = gfx::Rect(0, letterboxing_height - 2, texture_size.width(),
                        texture_size.height());
  quad_to_root_transform = gfx::Transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height - 2)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc2;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);
  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc1(&desc2));

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // there would be four pixels more to cover extra blank bar since the
    // adjustment is basically a padding without movedown.
    EXPECT_EQ(1920u, desc2.Width);
    EXPECT_EQ(1084u, desc2.Height);
  } else {
    EXPECT_EQ(1920u, desc2.Width);
    EXPECT_EQ(1080u, desc2.Height);
  }

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform2;
  gfx::Point visual_offset2;
  gfx::Rect visual_clip_rect2;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform2, &visual_offset2, &visual_clip_rect2);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect2);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform2);
    EXPECT_EQ(clip_rect, visual_clip_rect2);
  }

  // Third test if swap chain visual info is adjusted to fit the monitor when
  // some positive offset from typical letterboxing positioning.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  clip_rect = gfx::Rect(0, letterboxing_height + 2, texture_size.width(),
                        texture_size.height());
  quad_to_root_transform = gfx::Transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height + 2)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size
  DXGI_SWAP_CHAIN_DESC1 desc3;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain3);
  EXPECT_HRESULT_SUCCEEDED(swap_chain3->GetDesc1(&desc3));
  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // there would be two pixels more to cover extra blank bar since the
    // adjustment is basically a moveup.
    EXPECT_EQ(1920u, desc3.Width);
    EXPECT_EQ(1082u, desc3.Height);
  } else {
    EXPECT_EQ(1920u, desc3.Width);
    EXPECT_EQ(1080u, desc3.Height);
  }

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform3;
  gfx::Point visual_offset3;
  gfx::Rect visual_clip_rect3;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform3, &visual_offset3, &visual_clip_rect3);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect3);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform3);
    EXPECT_EQ(clip_rect, visual_clip_rect3);
  }
}

TEST_P(DCompPresenterLetterboxingTest,
       FullScreenLetterboxingWithDesktopPlaneRemoval) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1080p texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1920, 1080);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Test if swap chain and its visual info is adjusted to fit the monitor when
  // letterboxing is generated for full screen presentation.
  const int letterboxing_height =
      (monitor_size.height() - texture_size.height()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, texture_size.width(), texture_size.height());
  const gfx::Rect clip_rect = gfx::Rect(
      0, letterboxing_height, texture_size.width(), texture_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(1920u, desc.Width);
  EXPECT_EQ(1080u, desc.Height);

  if (GetParam()) {
    // Check desktop plane removal part 1.
    Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
    EXPECT_HRESULT_SUCCEEDED(
        swap_chain->QueryInterface(IID_PPV_ARGS(&decode_swap_chain)));
    // The dest size has been set to monitor size.
    uint32_t dest_width, dest_height;
    EXPECT_HRESULT_SUCCEEDED(
        decode_swap_chain->GetDestSize(&dest_width, &dest_height));
    EXPECT_EQ(1920u, dest_width);
    EXPECT_EQ(1200u, dest_height);

    // The target rect has been set to the onscreen content rect.
    RECT target_rect;
    EXPECT_HRESULT_SUCCEEDED(decode_swap_chain->GetTargetRect(&target_rect));
    EXPECT_EQ(clip_rect, gfx::Rect(target_rect));
  }

  // Swap chain visual is clipped to the whole monitor size.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform, &visual_offset, &visual_clip_rect);
  if (GetParam()) {
    // Check desktop plane removal part 2.
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay.
    EXPECT_TRUE(visual_transform.IsIdentity());
    // Visual clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin transform and clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform);
    EXPECT_EQ(clip_rect, visual_clip_rect);
  }
}

TEST_P(DCompPresenterLetterboxingTest, FullScreenLetterboxingKeepVisualInfo) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1080p texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1920, 1080);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // First full screen presentation with letterboxing.
  const int letterboxing_height =
      (monitor_size.height() - texture_size.height()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, texture_size.width(), texture_size.height());
  const gfx::Rect clip_rect = gfx::Rect(
      0, letterboxing_height, texture_size.width(), texture_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Make sure it's a valid swap chain presentation
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);

  // One present is normal, and a second present because it's the first frame
  // and the other buffer needs to be drawn to.
  UINT last_present_count = 0;
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->GetLastPresentCount(&last_present_count));
  EXPECT_EQ(2u, last_present_count);

  // Swap chain visual info is collected for the first presentation.
  gfx::Transform visual_transform1;
  gfx::Point visual_offset1;
  gfx::Rect visual_clip_rect1;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform1, &visual_offset1, &visual_clip_rect1);

  // Followed by second presentation with the same image.
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // It's the same image, so it should have the same swapchain.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_EQ(swap_chain2.Get(), swap_chain.Get());

  // No new presentation happened and no present count increase since it's with
  // the same image.
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->GetLastPresentCount(&last_present_count));
  EXPECT_EQ(2u, last_present_count);

  // Swap chain visual info should be kept same as the previous presentation.
  gfx::Transform visual_transform2;
  gfx::Point visual_offset2;
  gfx::Rect visual_clip_rect2;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform2, &visual_offset2, &visual_clip_rect2);
  EXPECT_EQ(visual_transform1, visual_transform2);
  EXPECT_EQ(visual_offset1, visual_offset2);
  EXPECT_EQ(visual_clip_rect1, visual_clip_rect2);

  // More checks followed by third presentation with a new image.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(0);
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain3->GetLastPresentCount(&last_present_count));
  // The present count should increase with the new image presentation.
  EXPECT_EQ(3u, last_present_count);
}

// Pillarboxing is generally considered as a special letterboxing.
TEST_P(DCompPresenterLetterboxingTest, FullScreenPillarboxingResizeVideoLayer) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1800*1200 texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1800, 1200);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // First test if swap chain and its visual info is adjusted to fit the
  // monitor when letterboxing is generated for full screen presentation.
  const int letterboxing_width =
      (monitor_size.width() - texture_size.width()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, texture_size.width(), texture_size.height());
  gfx::Rect clip_rect = gfx::Rect(letterboxing_width, 0, texture_size.width(),
                                  texture_size.height());
  gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(letterboxing_width, 0)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(1800u, desc.Width);
  EXPECT_EQ(1200u, desc.Height);

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform, &visual_offset, &visual_clip_rect);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform);
    EXPECT_EQ(clip_rect, visual_clip_rect);
  }

  // Second test if swap chain visual info is adjusted to fit the monitor when
  // some negative offset from typical letterboxing positioning.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  clip_rect = gfx::Rect(letterboxing_width - 2, 0, texture_size.width(),
                        texture_size.height());
  quad_to_root_transform = gfx::Transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(letterboxing_width - 2, 0)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc2;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain2);
  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc1(&desc2));

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // there would be four pixels more to cover extra blank bar since the
    // adjustment is basically a padding without move-right.
    EXPECT_EQ(1804u, desc2.Width);
    EXPECT_EQ(1200u, desc2.Height);
  } else {
    EXPECT_EQ(1800u, desc2.Width);
    EXPECT_EQ(1200u, desc2.Height);
  }

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform2;
  gfx::Point visual_offset2;
  gfx::Rect visual_clip_rect2;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform2, &visual_offset2, &visual_clip_rect2);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect2);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform2);
    EXPECT_EQ(clip_rect, visual_clip_rect2);
  }

  // Third test if swap chain visual info is adjusted to fit the monitor when
  // some positive offset from typical letterboxing positioning.
  texture = CreateNV12Texture(d3d11_device, texture_size);
  clip_rect = gfx::Rect(letterboxing_width + 2, 0, texture_size.width(),
                        texture_size.height());
  quad_to_root_transform = gfx::Transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(letterboxing_width + 2, 0)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));
    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size
  DXGI_SWAP_CHAIN_DESC1 desc3;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain3);
  EXPECT_HRESULT_SUCCEEDED(swap_chain3->GetDesc1(&desc3));
  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // there would be two pixels more to cover extra blank bar since the
    // adjustment is basically a move-left.
    EXPECT_EQ(1802u, desc3.Width);
    EXPECT_EQ(1200u, desc3.Height);
  } else {
    EXPECT_EQ(1800u, desc3.Width);
    EXPECT_EQ(1200u, desc3.Height);
  }

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  gfx::Transform visual_transform3;
  gfx::Point visual_offset3;
  gfx::Rect visual_clip_rect3;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform3, &visual_offset3, &visual_clip_rect3);

  if (GetParam()) {
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay. And visual
    // clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect3);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform3);
    EXPECT_EQ(clip_rect, visual_clip_rect3);
  }
}

TEST_P(DCompPresenterLetterboxingTest,
       FullScreenPillarboxingWithDesktopPlaneRemoval) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1800*1200 texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1800, 1200);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Test if swap chain and its visual info is adjusted to fit the monitor when
  // letterboxing is generated for full screen presentation.
  const int letterboxing_width =
      (monitor_size.width() - texture_size.width()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, texture_size.width(), texture_size.height());
  const gfx::Rect clip_rect = gfx::Rect(
      letterboxing_width, 0, texture_size.width(), texture_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(letterboxing_width, 0)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params->quad_rect = quad_rect;
    dc_layer_params->transform = quad_to_root_transform;
    dc_layer_params->clip_rect = clip_rect;
    dc_layer_params->color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params->z_order = 1;
    dc_layer_params->possible_video_fullscreen_letterboxing = true;
    presenter_->ScheduleDCLayer(std::move(dc_layer_params));

    PresentAndCheckSwapResult(gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(0);
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(1800u, desc.Width);
  EXPECT_EQ(1200u, desc.Height);

  if (GetParam()) {
    // Check desktop plane removal part 1.
    Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
    EXPECT_HRESULT_SUCCEEDED(
        swap_chain->QueryInterface(IID_PPV_ARGS(&decode_swap_chain)));
    // The dest size has been set to monitor size.
    uint32_t dest_width, dest_height;
    EXPECT_HRESULT_SUCCEEDED(
        decode_swap_chain->GetDestSize(&dest_width, &dest_height));
    EXPECT_EQ(1920u, dest_width);
    EXPECT_EQ(1200u, dest_height);

    // The target rect has been set to the onscreen content rect.
    RECT target_rect;
    EXPECT_HRESULT_SUCCEEDED(decode_swap_chain->GetTargetRect(&target_rect));
    EXPECT_EQ(clip_rect, gfx::Rect(target_rect));
  }

  // Swap chain visual is clipped to the whole monitor size.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      0, &visual_transform, &visual_offset, &visual_clip_rect);
  if (GetParam()) {
    // Check desktop plane removal part 2.
    // In case DirectCompositionLetterboxVideoOptimization feature is enabled,
    // DWM will do the swap chain positioning in case of overlay.
    EXPECT_TRUE(visual_transform.IsIdentity());
    // Visual clip rect has been set to monitor rect.
    EXPECT_EQ(gfx::Rect(monitor_size), visual_clip_rect);
  } else {
    // In case DirectCompositionLetterboxVideoOptimization feature is disabled,
    // keep the origin transform and clip rect from DCLayerOverlayParams.
    EXPECT_EQ(quad_to_root_transform, visual_transform);
    EXPECT_EQ(clip_rect, visual_clip_rect);
  }
}

}  // namespace gl
