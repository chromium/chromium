// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_presenter.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/overlay_layer_id.h"
#include "ui/gfx/test/sk_color_eq.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/dcomp_surface_proxy.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/swap_chain_presenter.h"
#include "ui/gl/test/gl_test_helper.h"
#include "ui/gl/test/gl_test_support.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/win/win_window.h"

namespace gfx {
// Used by gtest to print expectation values.
void PrintTo(const SwapResult& swap_result, ::std::ostream* os) {
  switch (swap_result) {
    case SwapResult::SWAP_ACK:
      *os << "SWAP_ACK";
      return;
    case SwapResult::SWAP_FAILED:
      *os << "SWAP_FAILED";
      return;
    case SwapResult::SWAP_SKIPPED:
      *os << "SWAP_SKIPPED";
      return;
    case SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      *os << "SWAP_NAK_RECREATE_BUFFERS";
      return;
    case SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED:
      *os << "SWAP_NON_SIMPLE_OVERLAYS_FAILED";
      return;
  }
  NOTREACHED();
}
}  // namespace gfx

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
  void OnCursorUpdate() override {}
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
  UNSAFE_TODO(
      memset(&image_data[0], 160, size.width() * size.height() * 3 / 2));

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = (const void*)&image_data[0];
  data.SysMemPitch = size.width();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, &data, &texture);
  EXPECT_HRESULT_SUCCEEDED(hr);
  return texture;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateP010Texture(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size) {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_P010;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1;
  desc.BindFlags = 0;
  desc.MiscFlags = 0;

  // Y, U, and V should all be 160. Output color should be pink.
  std::vector<uint16_t> image_data(size.width() * size.height() * 3 / 2,
                                   0xA000);
  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = (const void*)&image_data[0];
  data.SysMemPitch = size.width() * 2;

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
      GetDirectCompositionDevice();

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
DCLayerOverlayParams CreateParamsFromImage(
    DCLayerOverlayImage image,
    std::optional<gfx::RectF> content_rect_override = {}) {
  DCLayerOverlayParams params;
  params.content_rect =
      content_rect_override.value_or(gfx::RectF(image.size()));
  params.overlay_image = std::move(image);
  return params;
}

}  // namespace

// Test parameters that affect all DCompPresenter tests.
struct GlobalParam {
  bool use_gpu_vsync = false;
};

void PrintTo(const GlobalParam& param, std::ostream* os) {
  if (param.use_gpu_vsync) {
    *os << "GpuVsyncOn";
  } else {
    *os << "GpuVsyncOff";
  }
}

// Base class that provides test parameterization intended for all
// DCompPresenter tests.
//
// If a test suite does not need its own parameterization, it should extend
// `DCompPresenterTestBase<>`. Otherwise, it should provide a `Param` type and a
// way for gtest to print it.
//
// Instantiations of derived test suites should look like:
//
//   INSTANTIATE_TEST_SUITE_P(,
//                            DCompPresenterTest,
//                            DCompPresenterTest::GetValues(),
//                            &DCompPresenterTest::GetParamName);
template <class Param = std::monostate>
class DCompPresenterTestBase
    : public testing::TestWithParam<std::tuple<GlobalParam, Param>> {
 public:
  // Combine the values generator for `Param` (if present) with the generator
  // for the global parameters.
  template <typename... Generator>
  static auto GetValues(const Generator&... g) {
    if constexpr (std::is_same_v<Param, std::monostate>) {
      static_assert(sizeof...(g) == 0,
                    "GetValues should take no parameters because the test "
                    "suite is not parameterized.");
      return testing::Combine(testing::ConvertGenerator(testing::Bool()),
                              testing::Values(std::monostate()));
    } else {
      static_assert(sizeof...(g) == 1,
                    "`GetValues` requires a values generator for `Param`.");
      return testing::Combine(testing::ConvertGenerator(testing::Bool()), g...);
    }
  }

  static const Param& GetTestParam() {
    return std::get<1>(DCompPresenterTestBase<Param>::GetParam());
  }

  // Helper to generate human-friendly test parametrization names. This expects
  // `Param` to be a type that is gtest-printable to a valid param string.
  static std::string GetParamName(
      const testing::TestParamInfo<
          typename DCompPresenterTestBase<Param>::ParamType>& info) {
    const std::string shared_param_name =
        testing::PrintToString(std::get<0>(info.param));
    if constexpr (std::is_same_v<Param, std::monostate>) {
      return shared_param_name;
    } else {
      return base::JoinString(
          {
              shared_param_name,
              testing::PrintToString(std::get<1>(info.param)),
          },
          "_");
    }
  }

  DCompPresenterTestBase() : parent_window_(ui::GetHiddenWindow()) {}

 protected:
  void SetUp() override {
    if (std::get<0>(this->GetParam()).use_gpu_vsync) {
      EnableFeature(features::kGpuVsync);
    } else {
      DisableFeature(features::kGpuVsync);
    }

    enabled_features_.InitWithFeatures(enabled_features_list_,
                                       disabled_features_list_);
    display_ = GLTestSupport::InitializeGL(std::nullopt);
    std::tie(gl_surface_, context_) =
        GLTestHelper::CreateOffscreenGLSurfaceAndContext();

    // These tests are assumed to run on battery.
    fake_power_monitor_source_.SetBatteryPowerStatus(
        base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);

    // All bots run on non-blocklisted hardware that supports DComp (>Win7)
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        QueryD3D11DeviceObjectFromANGLE();
    InitializeDirectComposition(d3d11_device.Get());
    ASSERT_TRUE(DirectCompositionSupported());

    presenter_ = CreateDCompPresenter();

    SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_NV12);
    SetDirectCompositionScaledOverlaysSupportedForTesting(false);
  }

  void TearDown() override {
    if (presenter_) {
      DestroyPresenter(std::move(presenter_));
    }

    context_.reset();
    gl_surface_.reset();
    init::ShutdownGL(display_, false);
    display_ = nullptr;
  }

  scoped_refptr<DCompPresenter> CreateDCompPresenter() {
    DCompPresenter::Settings settings;
    scoped_refptr<DCompPresenter> presenter =
        base::MakeRefCounted<DCompPresenter>(settings);

    // ImageTransportSurfaceDelegate::AddChildWindowToBrowser() is called in
    // production code here. However, to remove dependency from
    // gpu/ipc/service/image_transport_presenter_delegate.h, here we directly
    // executes the required minimum code.
    if (parent_window_) {
      ::SetParent(presenter->GetWindow(), parent_window_);
    }

    return presenter;
  }

  void ScheduleOverlay(DCLayerOverlayParams overlay) {
    EXPECT_NE(overlay.layer_id, gfx::OverlayLayerId());
    pending_overlays_.push_back(std::move(overlay));
  }

  void ScheduleOverlays(std::vector<DCLayerOverlayParams> overlays) {
    EXPECT_THAT(overlays, testing::Each(testing::Field(
                              "layer_id", &DCLayerOverlayParams::layer_id,
                              testing::Ne(gfx::OverlayLayerId()))));
    std::ranges::move(overlays, std::back_inserter(pending_overlays_));
  }

  static gfx::OverlayLayerId GetRootSurfaceId() {
    // Use an arbitrary render pass ID. The render passes themselves have been
    // forgotten by the time we reach DCompPresenter, so we just need any unique
    // identifier to represent the root surface.
    return gfx::OverlayLayerId::MakeVizInternalRenderPass(
        gfx::OverlayLayerId::RenderPassId(1));
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
    params.z_order = 0;
    params.quad_rect = gfx::Rect(window_size);
    params.overlay_image = CreateDCompSurface(window_size, initial_color);
    params.layer_id = GetRootSurfaceId();
    ScheduleOverlay(std::move(params));
  }

  // Wait for `presenter_` to present asynchronously and return the swap result.
  gfx::SwapResult PresentAndGetSwapResult() {
    presenter_->ScheduleDCLayers(std::move(pending_overlays_));

    std::optional<gfx::SwapResult> result;

    base::RunLoop wait_for_present;
    presenter_->Present(
        base::BindOnce(
            [](base::RepeatingClosure quit_closure,
               std::optional<gfx::SwapResult>* out_result,
               gfx::SwapCompletionResult result) {
              *out_result = result.swap_result;
              quit_closure.Run();
            },
            wait_for_present.QuitClosure(), base::Unretained(&result)),
        base::DoNothing(), gfx::FrameData());
    wait_for_present.Run();

    return result.value();
  }

  void EnableFeature(const base::test::FeatureRef& feature) {
    enabled_features_list_.push_back(feature);
  }

  void DisableFeature(const base::test::FeatureRef& feature) {
    disabled_features_list_.push_back(feature);
  }

  raw_ptr<GLDisplay> display_ = nullptr;
  scoped_refptr<GLSurface> gl_surface_;
  scoped_refptr<GLContext> context_;

  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
  HWND parent_window_;
  scoped_refptr<DCompPresenter> presenter_;
  base::test::ScopedFeatureList enabled_features_;
  std::vector<base::test::FeatureRef> enabled_features_list_;
  std::vector<base::test::FeatureRef> disabled_features_list_;

  std::vector<DCLayerOverlayParams> pending_overlays_;
};

class DCompPresenterTest : public DCompPresenterTestBase<> {};

// Ensure that the overlay image isn't presented again unless it changes.
TEST_P(DCompPresenterTest, NoPresentTwice) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params.quad_rect = gfx::Rect(100, 100);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_FALSE(swap_chain);

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  swap_chain = presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0));
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
    params.quad_rect = gfx::Rect(100, 100);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
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
    params.quad_rect = gfx::Rect(100, 100);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain3->GetLastPresentCount(&last_present_count));
  // The present count should increase with the new present
  EXPECT_EQ(3u, last_present_count);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is support - swapchain should be set to the onscreen video size.
TEST_P(DCompPresenterTest, SwapchainSizeWithScaledOverlays) {
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
    params.quad_rect = quad_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc(&desc));
  // Onscreen quad_rect.size is (100, 100).
  EXPECT_EQ(100u, desc.BufferDesc.Width);
  EXPECT_EQ(100u, desc.BufferDesc.Height);

  // Clear SwapChainPresenters
  // Must do Clear first because the swap chain won't resize immediately if
  // a new size is given unless this is the very first time after Clear.
  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  // The input texture size is bigger than the window size.
  quad_rect = gfx::Rect(32, 48);

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params.quad_rect = quad_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain2);

  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc(&desc));
  // Onscreen quad_rect.size is (32, 48).
  EXPECT_EQ(32u, desc.BufferDesc.Width);
  EXPECT_EQ(48u, desc.BufferDesc.Height);
}

// Ensure the swapchain size is set to the correct size if HW overlay scaling
// is not support - swapchain should be the onscreen video size.
TEST_P(DCompPresenterTest, SwapchainSizeWithoutScaledOverlays) {
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
    params.quad_rect = quad_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
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
    params.quad_rect = quad_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain2);

  EXPECT_HRESULT_SUCCEEDED(swap_chain2->GetDesc(&desc));
  // Onscreen quad_rect.size is (124, 136).
  EXPECT_EQ(124u, desc.BufferDesc.Width);
  EXPECT_EQ(136u, desc.BufferDesc.Height);
}

// Test protected video flags
TEST_P(DCompPresenterTest, ProtectedVideos) {
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
    params.quad_rect = gfx::Rect(window_size);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    params.video_params.protected_video_type = gfx::ProtectedVideoType::kClear;

    ScheduleOverlay(std::move(params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        presenter_->GetLayerSwapChainForTesting(
            gfx::OverlayLayerId::MakeForTesting(0));
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
    params.quad_rect = gfx::Rect(window_size);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    params.video_params.protected_video_type =
        gfx::ProtectedVideoType::kSoftwareProtected;

    ScheduleOverlay(std::move(params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        presenter_->GetLayerSwapChainForTesting(
            gfx::OverlayLayerId::MakeForTesting(0));
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

TEST_P(DCompPresenterTest, NoBackgroundColorSurfaceForNonColorOverlays) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  auto root_surface =
      CreateParamsFromImage(CreateDCompSurface(window_size, SkColors::kBlack));
  root_surface.quad_rect = gfx::Rect(window_size);
  root_surface.z_order = 1;
  root_surface.layer_id = gfx::OverlayLayerId::MakeForTesting(1);
  ScheduleOverlay(std::move(root_surface));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();
  EXPECT_EQ(1u, layer_tree->GetDcompLayerCountForTesting());
  EXPECT_EQ(0u, layer_tree->GetNumSurfacesInPoolForTesting());
}

TEST_P(DCompPresenterTest, BackgroundColorSurfaceTrim) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

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
        DCLayerOverlayParams params;
        params.quad_rect = gfx::Rect(window_size);
        params.background_color = SkColor4f::FromColor(SkColorSetRGB(i, 0, 0));
        params.z_order = i + 1;
        params.layer_id = gfx::OverlayLayerId::MakeForTesting(i);
        ScheduleOverlay(std::move(params));
      }
      ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
      EXPECT_EQ(num_buffers, layer_tree->GetNumSurfacesInPoolForTesting());
    }

    // We expect retained surfaces even after we present a frame with no solid
    // color overlays.
    {
      ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
      EXPECT_EQ(std::min(num_buffers, kMaxSolidColorBuffers),
                layer_tree->GetNumSurfacesInPoolForTesting());
    }
  }
}

// Check that when there's multiple background color surfaces, the correct one
// is reused even if the order they're requested in changes.
TEST_P(DCompPresenterTest, BackgroundColorSurfaceMultipleReused) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  std::vector<SkColor4f> colors = {SkColors::kRed, SkColors::kGreen};
  std::vector<IDCompositionSurface*> surfaces_frame1(2, nullptr);
  std::vector<IDCompositionSurface*> surfaces_frame2(2, nullptr);

  const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();

  {
    for (size_t i = 0; i < colors.size(); i++) {
      DCLayerOverlayParams params;
      params.quad_rect = gfx::Rect(window_size);
      params.background_color = colors[i];
      params.z_order = i + 1;
      params.layer_id = gfx::OverlayLayerId::MakeForTesting(i);
      ScheduleOverlay(std::move(params));
    }

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
    EXPECT_EQ(2u, layer_tree->GetNumSurfacesInPoolForTesting());

    surfaces_frame1[0] = layer_tree->GetBackgroundColorSurfaceForTesting(
        gfx::OverlayLayerId::MakeForTesting(0));
    surfaces_frame1[1] = layer_tree->GetBackgroundColorSurfaceForTesting(
        gfx::OverlayLayerId::MakeForTesting(1));
    // The overlays should have different background color surfaces since they
    // have different background colors.
    EXPECT_NE(surfaces_frame1[0], surfaces_frame1[1]);
  }

  {
    // Swap the colors so they appear as overlays in a different order for the
    // next frame.
    std::swap(colors[0], colors[1]);

    for (size_t i = 0; i < colors.size(); i++) {
      DCLayerOverlayParams params;
      params.quad_rect = gfx::Rect(window_size);
      params.background_color = colors[i];
      params.z_order = i + 1;
      params.layer_id = gfx::OverlayLayerId::MakeForTesting(i);
      ScheduleOverlay(std::move(params));
    }

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
    EXPECT_EQ(2u, layer_tree->GetNumSurfacesInPoolForTesting());

    surfaces_frame2[0] = layer_tree->GetBackgroundColorSurfaceForTesting(
        gfx::OverlayLayerId::MakeForTesting(0));
    surfaces_frame2[1] = layer_tree->GetBackgroundColorSurfaceForTesting(
        gfx::OverlayLayerId::MakeForTesting(1));
    EXPECT_NE(surfaces_frame2[0], surfaces_frame2[1]);

    // We reversed the order of the color overlays. We expect the background
    // color surfaces to be reused, but reversed.
    EXPECT_EQ(surfaces_frame1[0], surfaces_frame2[1]);
    EXPECT_EQ(surfaces_frame1[1], surfaces_frame2[0]);
  }
}

// Check that the delegated ink visual gets added to the DC Layer tree
// if there is no root surface.
TEST_P(DCompPresenterTest, DelegatedInkVisualAddedWithRootSurfaceVisualNull) {
  std::unique_ptr<gfx::DelegatedInkMetadata> metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(
          gfx::PointF(12, 12), /*diameter=*/3, SK_ColorBLACK,
          base::TimeTicks::Now(), gfx::RectF(10, 10, 90, 90),
          /*hovering=*/false);

  // Set start point to initialize ink renderer.
  DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();
  if (!layer_tree->SupportsDelegatedInk()) {
    return;
  }
  layer_tree->SetDelegatedInkTrailStartPoint(std::move(metadata));

  EXPECT_TRUE(layer_tree->CommitAndClearPendingOverlays({}).has_value());

  // Despite no overlays, there should be one visual subtree for the delegated
  // ink trail.
  EXPECT_EQ(1u, layer_tree->GetDcompLayerCountForTesting());
}

// Ensure that swap chains stay attached to the same visual between subsequent
// frames.
// Please ensure this test is not broken. Re-attaching swapchains between
// subsequent frames may cause flickering under certain conditions that include
// specific Intel drivers, custom present duration etc.
// See https://bugs.chromium.org/p/chromium/issues/detail?id=1421175.
TEST_P(DCompPresenterTest, VisualsReused) {
  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  EXPECT_NE(texture, nullptr);

  // Frame 1:
  // overlay 0: root dcomp surface
  // overlay 1: swapchain z-order = 1 (overlay)
  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlue);
  {
    DCLayerOverlayParams params;
    params.overlay_image.emplace(texture_size, texture);
    params.content_rect = gfx::RectF(texture_size);
    params.quad_rect = gfx::Rect(100, 100);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    // Overlay
    params.z_order = 1;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(params));
  }

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  DCLayerTree* dcLayerTree = presenter_->GetLayerTreeForTesting();

#if DCHECK_IS_ON()
  EXPECT_TRUE(dcLayerTree->DcompVisualContentChangedFromPreviousFrameForTesting(
      gfx::OverlayLayerId::MakeForTesting(0)));
#endif  // DCHECK_IS_ON()

  EXPECT_EQ(2u, dcLayerTree->GetDcompLayerCountForTesting());
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visual0 =
      dcLayerTree->GetContentVisualForTesting(GetRootSurfaceId());
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visual1 =
      dcLayerTree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));

  // Frame 2:
  // overlay 0: swapchain z-order = -1 (underlay)
  // overlay 1: root dcomp surface
  {
    DCLayerOverlayParams params;
    params.overlay_image.emplace(texture_size, texture);
    params.content_rect = gfx::RectF(texture_size);
    params.quad_rect = gfx::Rect(100, 100);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    // Underlay
    params.z_order = -1;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(params));
  }
  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlue);

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  EXPECT_EQ(2u, dcLayerTree->GetDcompLayerCountForTesting());
  // Verify that the visuals are reused from the previous frame but attached
  // to the root visual in a reversed order.
  EXPECT_EQ(visual0.Get(),
            dcLayerTree->GetContentVisualForTesting(GetRootSurfaceId()));
  EXPECT_EQ(visual1.Get(), dcLayerTree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(0)));
#if DCHECK_IS_ON()
  EXPECT_FALSE(
      dcLayerTree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(0)));
#endif  // DCHECK_IS_ON()
}

DCLayerOverlayParams CreateOverlayWithSwapChain(
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    const gfx::Size& swap_chain_size,
    int z_order) {
  DCLayerOverlayParams params;
  params.overlay_image = DCLayerOverlayImage(swap_chain_size, swap_chain);
  params.content_rect = gfx::RectF(swap_chain_size);
  params.quad_rect = gfx::Rect(100, 100);
  params.video_params.color_space = gfx::ColorSpace::CreateSRGB();
  params.z_order = z_order;
  params.layer_id = gfx::OverlayLayerId::MakeForTesting(z_order);
  return params;
}

void CreateSwapChain(IDXGIFactory2* dxgi_factory,
                     ID3D11Device* d3d11_device,
                     const DXGI_SWAP_CHAIN_DESC1& desc,
                     Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain) {
  ASSERT_HRESULT_SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(
      d3d11_device, &desc, nullptr, &swap_chain));
  ASSERT_TRUE(swap_chain);
}

TEST_P(DCompPresenterTest, MatchedAndUnmatchedVisualsReused) {
  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlue);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  ASSERT_TRUE(d3d11_device);
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  ASSERT_HRESULT_SUCCEEDED(d3d11_device.As(&dxgi_device));
  ASSERT_TRUE(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  ASSERT_HRESULT_SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter));
  ASSERT_TRUE(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  ASSERT_HRESULT_SUCCEEDED(
      dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));
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

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainA;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainA);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainB;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainB);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainC;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainC);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainD;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainD);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainE;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainE);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainF;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainF);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainL;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainL);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chainM;
  CreateSwapChain(dxgi_factory.Get(), d3d11_device.Get(), desc, swap_chainM);

  // Frame 1: RootSurface, A B C D E F
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainA, swap_chain_size, 1));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainB, swap_chain_size, 2));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainC, swap_chain_size, 3));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainD, swap_chain_size, 4));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainE, swap_chain_size, 5));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainF, swap_chain_size, 6));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  DCLayerTree* dc_layer_tree = presenter_->GetLayerTreeForTesting();

#if DCHECK_IS_ON()
  for (int i = 1; i <= 6; i++) {
    EXPECT_TRUE(
        dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
            gfx::OverlayLayerId::MakeForTesting(i)));
  }
#endif  // DCHECK_IS_ON()

  EXPECT_EQ(7u, dc_layer_tree->GetDcompLayerCountForTesting());
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualRS =
      dc_layer_tree->GetContentVisualForTesting(GetRootSurfaceId());
  EXPECT_NE(visualRS, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualA =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(1));
  EXPECT_NE(visualA, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualB =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(2));
  EXPECT_NE(visualB, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualC =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(3));
  EXPECT_NE(visualC, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualD =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(4));
  EXPECT_NE(visualD, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualE =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(5));
  EXPECT_NE(visualE, nullptr);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visualF =
      dc_layer_tree->GetContentVisualForTesting(
          gfx::OverlayLayerId::MakeForTesting(6));
  EXPECT_NE(visualF, nullptr);

  // Frame 2: RootSurface, A L D C M
  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlue);
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainA, swap_chain_size, 1));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainL, swap_chain_size, 2));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainD, swap_chain_size, 3));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainC, swap_chain_size, 4));
  ScheduleOverlay(CreateOverlayWithSwapChain(swap_chainM, swap_chain_size, 5));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  EXPECT_EQ(6u, dc_layer_tree->GetDcompLayerCountForTesting());

  // Verify:
  // RootSurface is matched to RootSurface and kept attached to the root.
  // A is matched to A and kept attached to the root.
  // L is reused from B and kept attached to the root.
  // D is matched to D and kept attached to the root.
  // C is matched to C and reattached to the root.
  // M is reused from E and kept attached to the root.
  EXPECT_EQ(visualRS.Get(), dc_layer_tree->GetContentVisualForTesting(
                                GetRootSurfaceId()) /*RS*/);
  EXPECT_EQ(visualA.Get(), dc_layer_tree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(1)) /*A*/);
  EXPECT_EQ(visualB.Get(), dc_layer_tree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(2)) /*L*/);
  EXPECT_EQ(visualD.Get(), dc_layer_tree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(3)) /*D*/);
  EXPECT_EQ(visualC.Get(), dc_layer_tree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(4)) /*C*/);
  EXPECT_EQ(visualE.Get(), dc_layer_tree->GetContentVisualForTesting(
                               gfx::OverlayLayerId::MakeForTesting(5)) /*M*/);
#if DCHECK_IS_ON()
  EXPECT_FALSE(
      dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(1)));
  EXPECT_TRUE(
      dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(2)));
  EXPECT_FALSE(
      dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(3)));
  EXPECT_FALSE(
      dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(4)));
  EXPECT_TRUE(
      dc_layer_tree->DcompVisualContentChangedFromPreviousFrameForTesting(
          gfx::OverlayLayerId::MakeForTesting(5)));
#endif  // DCHECK_IS_ON()
}

// `SwapChainPresenter` internally tries to allocates a swap chain that is the
// size of the onscreen size. This is because some Intel devices do not support
// scaling overlays in HW. We do not account for the clip rect in the onscreen
// size calculation, so a clipped video can end up with a very large "onscreen"
// size even though most of it is not visible.
TEST_P(DCompPresenterTest, VeryLargeOnscreenSize) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

  gfx::Size texture_size(50, 50);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params.quad_rect = gfx::Rect(1, 1);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    // This transform will make us have an onscreen size with a dimension larger
    // than the D3D11 max texture size.
    params.transform =
        gfx::Transform::MakeScale(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 10);

    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  {
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params.quad_rect = gfx::Rect(1, 1);

    // This transform will make us have an onscreen size with a dimension larger
    // than the D3D11 max texture size.
    params.transform =
        gfx::Transform::MakeScale(10, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1);

    params.layer_id = gfx::OverlayLayerId::MakeForTesting(1);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));
  }

  EXPECT_FALSE(presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0)));
  EXPECT_FALSE(presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(1)));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  auto ExpectSwapChainAndMaintainsAspectRatio = [&](gfx::OverlayLayerId id,
                                                    gfx::SizeF expected_size) {
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
        presenter_->GetLayerSwapChainForTesting(id);
    EXPECT_TRUE(swap_chain);

    DXGI_SWAP_CHAIN_DESC1 desc;
    EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));

    const gfx::SizeF swap_chain_size = gfx::SizeF(desc.Width, desc.Height);

    // Calculate the closeness based on the magnitude of the aspect ratios.
    // `SwapChainPresenter` can slightly adjust the swap chain size, so the
    // aspect ratios will never be exact. In this test, it can also be very
    // large or very small.
    EXPECT_NEAR(std::log10(expected_size.AspectRatio()),
                std::log10(swap_chain_size.AspectRatio()), 0.00003);
  };

  // Maintaining the aspect ratio is not a requirement for the video to appear,
  // but doing so will improve the quality of the resulting image.
  ExpectSwapChainAndMaintainsAspectRatio(
      gfx::OverlayLayerId::MakeForTesting(0),
      gfx::SizeF(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1, 10));
  ExpectSwapChainAndMaintainsAspectRatio(
      gfx::OverlayLayerId::MakeForTesting(1),
      gfx::SizeF(10, D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION + 1));
}

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterTest,
                         DCompPresenterTest::GetValues(),
                         &DCompPresenterTest::GetParamName);

template <class Param = std::monostate>
class DCompPresenterPixelTestBase : public DCompPresenterTestBase<Param> {
 public:
  DCompPresenterPixelTestBase()
      : window_(&platform_delegate_, gfx::Rect(100, 100)) {
    this->parent_window_ = window_.hwnd();
  }

 protected:
  void SetUp() override {
    static_cast<ui::PlatformWindow*>(&window_)->Show();
    DCompPresenterTestBase<Param>::SetUp();
  }

  void TearDown() override {
    // Test harness times out without DestroyWindow() here.
    if (IsWindow(this->parent_window_)) {
      DestroyWindow(this->parent_window_);
    }
    DCompPresenterTestBase<Param>::TearDown();
  }

  void InitializeForPixelTest(const gfx::Size& window_size,
                              const gfx::Size& texture_size,
                              const gfx::Rect& content_rect,
                              const gfx::Rect& quad_rect,
                              const bool is_p010) {
    EXPECT_TRUE(
        this->presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    this->InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        GetDirectCompositionD3D11Device();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
        is_p010 ? CreateP010Texture(d3d11_device, texture_size)
                : CreateNV12Texture(d3d11_device, texture_size);
    ASSERT_NE(texture, nullptr);

    auto params = CreateParamsFromImage(
        DCLayerOverlayImage(texture_size, texture),
        /*content_rect_override=*/gfx::RectF(content_rect));
    params.quad_rect = quad_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    params.video_params.is_p010_content = is_p010;
    this->ScheduleOverlay(std::move(params));

    ASSERT_EQ(this->PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    Sleep(1000);
  }

  // If |scale_via_buffer| is true, use the content/quad rects to scale the
  // buffer. If it is false, use the overlay's transform to scale the visual.
  void RunNearestNeighborTest(bool scale_via_buffer) {
    const gfx::Size window_size(100, 100);

    EXPECT_TRUE(
        this->presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    this->InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

    auto dc_layer_params = CreateParamsFromImage(
        CreateDCompSurface(gfx::Size(2, 2), SkColors::kBlack,
                           {{gfx::Rect(0, 0, 1, 1), SkColors::kRed},
                            {gfx::Rect(1, 0, 1, 1), SkColors::kGreen},
                            {gfx::Rect(0, 1, 1, 1), SkColors::kBlue},
                            {gfx::Rect(1, 1, 1, 1), SkColors::kBlack}}));
    dc_layer_params.z_order = 1;
    dc_layer_params.nearest_neighbor_filter = true;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    if (scale_via_buffer) {
      // Pick a large quad rect so the buffer is scaled up
      dc_layer_params.quad_rect = gfx::Rect(window_size);
    } else {
      // Pick a small quad rect and assign a transform so the quad rect is
      // scaled up
      dc_layer_params.quad_rect =
          gfx::ToNearestRect(dc_layer_params.content_rect);
      dc_layer_params.transform = gfx::Transform::MakeScale(
          window_size.width() / dc_layer_params.quad_rect.width(),
          window_size.height() / dc_layer_params.quad_rect.height());
    }

    this->ScheduleOverlay(std::move(dc_layer_params));
    ASSERT_EQ(this->PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

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
  void CheckOverlayExactlyFillsHole(const gfx::Size& window_size,
                                    const gfx::Rect& root_surface_hole,
                                    DCLayerOverlayParams fit_in_hole_overlay) {
    EXPECT_TRUE(gfx::Rect(window_size).Contains(root_surface_hole));

    EXPECT_TRUE(
        this->presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    auto root_surface = CreateParamsFromImage(
        CreateDCompSurface(window_size, kRootSurfaceInitialColor,
                           {{root_surface_hole, kRootSurfaceHiddenColor}}));
    root_surface.quad_rect = gfx::Rect(window_size);
    root_surface.z_order = 0;
    root_surface.layer_id = this->GetRootSurfaceId();
    this->ScheduleOverlay(std::move(root_surface));

    this->ScheduleOverlay(std::move(fit_in_hole_overlay));

    ASSERT_EQ(this->PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

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

  void InitializeSwapChainForTest(
      gfx::Size swap_chain_size,
      Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain,
      Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv) {
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        GetDirectCompositionD3D11Device();
    ASSERT_TRUE(d3d11_device);
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    ASSERT_HRESULT_SUCCEEDED(d3d11_device.As(&dxgi_device));
    ASSERT_TRUE(dxgi_device);
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    ASSERT_HRESULT_SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter));
    ASSERT_TRUE(dxgi_adapter);
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    ASSERT_HRESULT_SUCCEEDED(
        dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)));
    ASSERT_TRUE(dxgi_factory);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = swap_chain_size.width();
    desc.Height = swap_chain_size.height();
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 2;
    desc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Flags = 0;

    ASSERT_HRESULT_SUCCEEDED(dxgi_factory->CreateSwapChainForComposition(
        d3d11_device.Get(), &desc, nullptr, &swap_chain));
    ASSERT_TRUE(swap_chain);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> front_buffer_texture;
    ASSERT_HRESULT_SUCCEEDED(
        swap_chain->GetBuffer(1u, IID_PPV_ARGS(&front_buffer_texture)));
    ASSERT_TRUE(front_buffer_texture);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
    ASSERT_TRUE(SUCCEEDED(
        swap_chain->GetBuffer(0u, IID_PPV_ARGS(&back_buffer_texture))));
    ASSERT_TRUE(back_buffer_texture);

    d3d11_device->CreateRenderTargetView(back_buffer_texture.Get(), nullptr,
                                         &rtv);
  }

  [[nodiscard]] HRESULT ClearRenderTargetViewAndPresent(
      const SkColor4f& clear_color,
      IDXGISwapChain1* swap_chain,
      ID3D11RenderTargetView* rtv) {
    GetImmediateDeviceContext()->ClearRenderTargetView(rtv, clear_color.vec());
    DXGI_PRESENT_PARAMETERS present_params = {};
    present_params.DirtyRectsCount = 0;
    present_params.pDirtyRects = nullptr;
    return swap_chain->Present1(0, 0, &present_params);
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> GetImmediateDeviceContext() {
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    GetDirectCompositionD3D11Device()->GetImmediateContext(&device_context);
    EXPECT_TRUE(device_context);
    return device_context;
  }

  TestPlatformDelegate platform_delegate_;
  ui::WinWindow window_;
};

class DCompPresenterVideoPixelTest : public DCompPresenterPixelTestBase<> {
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
      params.quad_rect = gfx::Rect(texture_size);
      params.video_params.color_space = color_space;
      params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
      ScheduleOverlay(std::move(params));
    }

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    // Scaling up the swapchain with the same image should cause it to be
    // transformed again, but not presented again.
    {
      auto params =
          CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
      params.quad_rect = gfx::Rect(window_size);
      params.video_params.color_space = color_space;
      params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
      ScheduleOverlay(std::move(params));
    }

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
    Sleep(1000);

    if (check_color) {
      EXPECT_SKCOLOR_CLOSE(
          expected_color,
          GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
          kMaxColorChannelDeviation);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterVideoPixelTest,
                         DCompPresenterVideoPixelTest::GetValues(),
                         &DCompPresenterVideoPixelTest::GetParamName);

TEST_P(DCompPresenterVideoPixelTest, BT601) {
  TestVideo(gfx::ColorSpace::CreateREC601(), SkColorSetRGB(0xdb, 0x81, 0xe8),
            true);
}

TEST_P(DCompPresenterVideoPixelTest, BT709) {
  TestVideo(gfx::ColorSpace::CreateREC709(), SkColorSetRGB(0xe1, 0x90, 0xeb),
            true);
}

TEST_P(DCompPresenterVideoPixelTest, SRGB) {
  // SRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGB(), SK_ColorTRANSPARENT, false);
}

TEST_P(DCompPresenterVideoPixelTest, SCRGBLinear) {
  // SCRGB doesn't make sense on an NV12 input, but don't crash.
  TestVideo(gfx::ColorSpace::CreateSRGBLinear(), SK_ColorTRANSPARENT, false);
}

TEST_P(DCompPresenterVideoPixelTest, InvalidColorSpace) {
  // Invalid color space should be treated as BT.709
  TestVideo(gfx::ColorSpace(), SkColorSetRGB(0xe1, 0x90, 0xeb), true);
}

class DCompPresenterPixelTest : public DCompPresenterPixelTestBase<> {};

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterPixelTest,
                         DCompPresenterPixelTest::GetValues(),
                         &DCompPresenterPixelTest::GetParamName);

TEST_P(DCompPresenterPixelTest, SoftwareVideoSwapchain) {
  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();

  gfx::Size y_size(50, 50);
  size_t stride = y_size.width();

  std::vector<uint8_t> nv12_pixmap(stride * 3 * y_size.height() / 2, 0xff);

  auto params = CreateParamsFromImage(
      DCLayerOverlayImage(y_size, base::span(nv12_pixmap), stride));
  params.quad_rect = gfx::Rect(window_size);
  params.video_params.color_space = gfx::ColorSpace::CreateREC709();
  params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(params));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  Sleep(1000);

  SkColor expected_color = SkColorSetRGB(0xff, 0xb7, 0xff);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, VideoHandleSwapchain) {
  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, SkipVideoLayerEmptyBoundsRect) {
  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect;  // Layer with empty bounds rect.
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, SkipVideoLayerEmptyContentsRect) {
  // Swap chain size is overridden to onscreen size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

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
  params.quad_rect = gfx::Rect(window_size);
  params.video_params.color_space = gfx::ColorSpace::CreateREC709();
  params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(params));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  Sleep(1000);

  // No color is written since the visual committed to DirectComposition has no
  // content.
  SkColor expected_color = SK_ColorBLACK;
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, BGRASwapChain) {
  // By default NV12 is used, so set it to BGRA explicitly.
  SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_B8G8R8A8_UNORM);
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height.  Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_B8G8R8A8_UNORM, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, NV12SwapChain) {
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height.  Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
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

TEST_P(DCompPresenterPixelTest, YUY2SwapChain) {
  // By default NV12 is used, so set it to YUY2 explicitly.
  SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_YUY2);
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height.  Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
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

TEST_P(DCompPresenterPixelTest, P010SwapChain) {
  if (GetDirectCompositionOverlaySupportFlagsForTesting(DXGI_FORMAT_P010) ==
      0) {
    GTEST_SKIP() << "P010 overlay is not supported on this test system.";
  }

  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  // By default NV12 is used, so set it to P010 explicitly.
  SetDirectCompositionOverlayFormatUsedForTesting(DXGI_FORMAT_P010);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  // Pass content rect with odd with and height. Surface should round up
  // width and height when creating swap chain.
  gfx::Rect content_rect(0, 0, 49, 49);
  gfx::Rect quad_rect(window_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         true);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(DXGI_FORMAT_P010, desc.Format);
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  SkColor expected_color = SkColorSetRGB(0xe1, 0x90, 0xeb);
  EXPECT_SKCOLOR_CLOSE(
      expected_color,
      GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
      kMaxColorChannelDeviation);
}

TEST_P(DCompPresenterPixelTest, NonZeroBoundsOffset) {
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  gfx::Size texture_size(50, 50);
  gfx::Rect content_rect(texture_size);
  gfx::Rect quad_rect(gfx::Point(25, 25), texture_size);
  InitializeForPixelTest(window_size, texture_size, content_rect, quad_rect,
                         false);

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

TEST_P(DCompPresenterPixelTest, ResizeVideoLayer) {
  // Swap chain size is overridden to onscreen rect size only if scaled overlays
  // are supported.
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

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
    params.quad_rect = gfx::Rect(window_size);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
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
    params.quad_rect = gfx::Rect(window_size);
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }
  swap_chain = presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0));
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // Onscreen window_size is (100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  if (base::FeatureList::IsEnabled(
          features::kEarlyFullScreenVideoOptimization)) {
    // The rest of this test checks that video overlays are adjusted for full
    // screen. EarlyFullScreenVideoOptimization handles adjustment of overlay
    // position during overlay processing.
    return;
  }

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
    params.quad_rect = on_screen_rect;
    params.clip_rect = on_screen_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain is set to monitor/onscreen size.
  swap_chain = presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0));
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(static_cast<UINT>(monitor_size.width()), desc.Width);
  EXPECT_EQ(static_cast<UINT>(monitor_size.height()), desc.Height);

  gfx::Transform transform;
  gfx::Point offset;
  gfx::Rect clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &transform, &offset, &clip_rect);
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
    params.quad_rect = on_screen_rect;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain is set to monitor size (100, 100).
  swap_chain = presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0));
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);

  // Make sure the new transform matrix is adjusted, so it transforms the swap
  // chain to |new_on_screen_rect| which fits the monitor.
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &transform, &offset, &clip_rect);
  EXPECT_EQ(gfx::Rect(monitor_size), transform.MapRect(gfx::Rect(100, 100)));
}

TEST_P(DCompPresenterPixelTest, SwapChainImage) {
  gfx::Size swap_chain_size(50, 50);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  InitializeSwapChainForTest(swap_chain_size, swap_chain, rtv);
  ASSERT_TRUE(swap_chain);
  ASSERT_TRUE(rtv);

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  // Clear to red and present.
  {
    ASSERT_HRESULT_SUCCEEDED(ClearRenderTargetViewAndPresent(
        SkColors::kRed, swap_chain.Get(), rtv.Get()));

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
    dc_layer_params.quad_rect = gfx::Rect(window_size);
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    ScheduleOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Clear to green and present.
  {
    ASSERT_HRESULT_SUCCEEDED(ClearRenderTargetViewAndPresent(
        SkColors::kGreen, swap_chain.Get(), rtv.Get()));

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
    dc_layer_params.quad_rect = gfx::Rect(window_size);
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    ScheduleOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorGREEN;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Present without clearing.  This will flip front and back buffers so the
  // previous rendered contents (red) will become visible again.
  {
    DXGI_PRESENT_PARAMETERS present_params = {};
    present_params.DirtyRectsCount = 0;
    present_params.pDirtyRects = nullptr;
    ASSERT_HRESULT_SUCCEEDED(swap_chain->Present1(0, 0, &present_params));

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
    dc_layer_params.quad_rect = gfx::Rect(window_size);
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    ScheduleOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }

  // Clear to blue without present.
  {
    float clear_color[] = {0.0, 0.0, 1.0, 1.0};
    GetImmediateDeviceContext()->ClearRenderTargetView(rtv.Get(), clear_color);

    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
    dc_layer_params.quad_rect = gfx::Rect(window_size);
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

    ScheduleOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    SkColor expected_color = SK_ColorRED;
    EXPECT_SKCOLOR_CLOSE(
        expected_color,
        GLTestHelper::ReadBackWindowPixel(window_.hwnd(), gfx::Point(75, 75)),
        kMaxColorChannelDeviation);
  }
}

// Test that the overlay quad rect's offset is affected by its transform.
TEST_P(DCompPresenterPixelTest, QuadOffsetAppliedAfterTransform) {
  // Our overlay quad rect is at 0,50 50x50 and scaled down by 1/2. Since we
  // expect the transform to affect the quad rect offset, we expect the output
  // rect to be at 0,25 25x25.
  const gfx::Rect quad_rect(gfx::Point(0, 50), gfx::Size(50, 50));
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(0.5, gfx::Vector2dF()));

  gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kBlack);

  auto dc_layer_params = CreateParamsFromImage(
      CreateDCompSurface(quad_rect.size(), SkColors::kRed));
  dc_layer_params.quad_rect = quad_rect;
  dc_layer_params.transform = quad_to_root_transform;
  dc_layer_params.z_order = 1;
  dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  ScheduleOverlay(std::move(dc_layer_params));
  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

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
TEST_P(DCompPresenterPixelTest, NearestNeighborFilteringScaleViaBuffer) {
  RunNearestNeighborTest(true);
}

// Test that scaling a (very) small texture up works with nearest neighbor
// filtering using the overlay's transform.
TEST_P(DCompPresenterPixelTest, NearestNeighborFilteringScaleViaTransform) {
  RunNearestNeighborTest(false);
}

// Test that the |content_rect| of an overlay scales the buffer to fit the
// display rect, if needed.
TEST_P(DCompPresenterPixelTest, ContentRectScalesUpBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect root_surface_hole = gfx::Rect(5, 10, 50, 75);

  // Provide an overlay that's smaller than the hole it needs to fill
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(1, 1), kOverlayExpectedColor));
  overlay.quad_rect = root_surface_hole;
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay scales the buffer to fit the
// display rect, if needed.
TEST_P(DCompPresenterPixelTest, ContentRectScalesDownBuffer) {
  const gfx::Size window_size(100, 100);
  const gfx::Rect root_surface_hole = gfx::Rect(5, 10, 50, 75);

  // Provide an overlay that's larger than the hole it needs to fill
  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(75, 100), kOverlayExpectedColor));
  overlay.quad_rect = root_surface_hole;
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay clips portions of the buffer.
TEST_P(DCompPresenterPixelTest, ContentRectClipsBuffer) {
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
  overlay.quad_rect = root_surface_hole;
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Test that the |content_rect| of an overlay can clip a buffer and scale it's
// contents.
TEST_P(DCompPresenterPixelTest, ContentRectClipsAndScalesBuffer) {
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
  overlay.quad_rect = root_surface_hole;
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  // Use nearest neighbor to avoid interpolation at the edges of the content
  // rect
  overlay.nearest_neighbor_filter = true;

  CheckOverlayExactlyFillsHole(window_size, root_surface_hole,
                               std::move(overlay));
}

// Check that the surface backing solid color overlays is reused across frames.
// This can happen e.g. with a solid color draw quad animating its color.
TEST_P(DCompPresenterPixelTest, BackgroundColorSurfaceReuse) {
  const gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  SkColor4f colors[] = {
      SkColors::kRed,    SkColors::kGreen, SkColors::kBlue,
      SkColors::kYellow, SkColors::kCyan,  SkColors::kMagenta,
  };

  IDCompositionSurface* background_color_surface = nullptr;

  for (const SkColor4f& color : colors) {
    DCLayerOverlayParams params;
    params.quad_rect = gfx::Rect(window_size);
    params.background_color = color;
    params.z_order = 1;
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    EXPECT_SKCOLOR_EQ(color.toSkColor(), GLTestHelper::ReadBackWindowPixel(
                                             window_.hwnd(), gfx::Point(0, 0)));

    const DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();

    EXPECT_EQ(1u, layer_tree->GetDcompLayerCountForTesting());
    EXPECT_EQ(1u, layer_tree->GetNumSurfacesInPoolForTesting());

    if (background_color_surface == nullptr) {
      background_color_surface =
          layer_tree->GetBackgroundColorSurfaceForTesting(
              gfx::OverlayLayerId::MakeForTesting(0));
    }
    EXPECT_NE(background_color_surface, nullptr);
    EXPECT_EQ(background_color_surface,
              layer_tree->GetBackgroundColorSurfaceForTesting(
                  gfx::OverlayLayerId::MakeForTesting(0)))
        << "DComp content for solid color overlay expected to be reused across "
           "frames";
  }
}

struct EarlyFullScreenVideoOptimizationParam {
  bool use_early_full_screen_video_optimization = false;
};

void PrintTo(const EarlyFullScreenVideoOptimizationParam& param,
             std::ostream* os) {
  if (param.use_early_full_screen_video_optimization) {
    *os << "EarlyFullScreenOptOn";
  } else {
    *os << "EarlyFullScreenOptOff";
  }
}

class DCompPresenterFullScreenOptimizationPixelTest
    : public DCompPresenterPixelTestBase<
          EarlyFullScreenVideoOptimizationParam> {
 public:
  void SetUp() override {
    static_cast<ui::PlatformWindow*>(&window_)->Show();
    EnableFeature(features::kDirectCompositionLetterboxVideoOptimization);
    if (GetTestParam().use_early_full_screen_video_optimization) {
      EnableFeature(features::kEarlyFullScreenVideoOptimization);
    } else {
      DisableFeature(features::kEarlyFullScreenVideoOptimization);
    }
    DCompPresenterPixelTestBase::SetUp();
  }

  void SetMonitorSize(const gfx::Size& monitor_size) {
    SetDirectCompositionScaledOverlaysSupportedForTesting(true);
    SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
    EXPECT_TRUE(
        this->presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));
    static_cast<ui::PlatformWindow*>(&window_)->SetBoundsInPixels(
        gfx::Rect(monitor_size));
  }

  void DoOneFrame(const gfx::Size& monitor_size,
                  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                  bool is_full_screen) {
    ASSERT_NE(texture, nullptr);

    // Use red for full screen because we expect the root surface to be fully
    // occluded by the "full screen"-ed swap chain. We can use the presence of
    // red as an indication that the full screening failed.
    //
    // We don't expect the video to be either of these background colors.
    const SkColor4f expected_background_color = SkColors::kBlack;
    const SkColor4f unexpected_background_color = SkColors::kRed;
    const SkColor4f root_surface_color = is_full_screen
                                             ? unexpected_background_color
                                             : expected_background_color;
    InitializeRootAndScheduleRootSurface(monitor_size, root_surface_color);

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    const gfx::Size texture_size = gfx::Size(desc.Width, desc.Height);
    auto params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    if (is_full_screen) {
      const float scale_factor =
          std::min(static_cast<float>(monitor_size.width()) /
                       static_cast<float>(texture_size.width()),
                   static_cast<float>(monitor_size.height()) /
                       static_cast<float>(texture_size.height()));
      params.quad_rect =
          gfx::ScaleToEnclosingRect(gfx::Rect(texture_size), scale_factor);
      params.transform = gfx::Transform::MakeTranslation(
          (monitor_size.width() - params.quad_rect.width()) / 2.0,
          (monitor_size.height() - params.quad_rect.height()) / 2.0);
      if (GetTestParam().use_early_full_screen_video_optimization) {
        params.video_params.is_full_screen_video = true;
      } else {
        params.video_params.possible_video_fullscreen_letterboxing = true;
      }
    } else {
      // An arbitrary rect that is not the full screen.
      params.quad_rect = gfx::Rect(gfx::ScaleToCeiledSize(monitor_size, 0.5));
    }
    const gfx::Rect expected_target_rect =
        params.transform.MapRect(params.quad_rect);
    params.clip_rect = expected_target_rect;
    params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    ScheduleOverlay(std::move(params));

    ASSERT_EQ(this->PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    const SkBitmap actual_pixels =
        GLTestHelper::ReadBackWindow(window_.hwnd(), monitor_size);
    gfx::Rect actual_video_bounds;
    bool already_asserted_wrong_background_color = false;
    for (int y = 0; y < actual_pixels.height(); y++) {
      for (int x = 0; x < actual_pixels.width(); x++) {
        const SkColor4f actual_color = actual_pixels.getColor4f(x, y);
        if (actual_color == expected_background_color) {
          // If the color matches the background mat color, no action.
        } else if (actual_color == unexpected_background_color) {
          // If `actual_color` is `root_surface_color`, then we are missing
          // the video background mat.
          if (!already_asserted_wrong_background_color) {
            EXPECT_SKCOLOR_EQ(actual_color.toSkColor(),
                              expected_background_color.toSkColor())
                << "background mat is the wrong color!";
            // Avoid spamming the log output for every pixel.
            already_asserted_wrong_background_color = true;
          }
        } else {
          // Any other color will be conservatively called "part of the video".
          actual_video_bounds.Union(gfx::Rect(x, y, 1, 1));
        }
      }
    }

    // Check that the video appears in the center of the window. If something
    // went wrong, the video may be stretched across the whole window or may
    // appear un-centered at the top left.
    EXPECT_EQ(actual_video_bounds, expected_target_rect);
  }
};

// Check that the first frame of a full screen optimized swap chain appears in
// the correct place.
TEST_P(DCompPresenterFullScreenOptimizationPixelTest, FirstFrame) {
  // Test a fraction of a 1920x1200 monitor to speed up the pixel readback.
  const float scale_factor = 0.25;
  const gfx::Size monitor_size(1920 * scale_factor, 1200 * scale_factor);
  const gfx::Size texture_size(2, 2);

  SetMonitorSize(monitor_size);

  auto texture =
      CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size);

  DoOneFrame(monitor_size, texture, /*is_full_screen=*/true);
}

// Check that two contiguous frames that share a `SwapChainPresenter` but do not
// update the video texture will maintain the full screen optimization.
TEST_P(DCompPresenterFullScreenOptimizationPixelTest, KeepFullScreenSameImage) {
  // Test a fraction of a 1920x1200 monitor to speed up the pixel readback.
  const float scale_factor = 0.25;
  const gfx::Size monitor_size(1920 * scale_factor, 1200 * scale_factor);
  const gfx::Size texture_size(2, 2);

  SetMonitorSize(monitor_size);

  auto texture =
      CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size);

  DoOneFrame(monitor_size, texture, /*is_full_screen=*/true);
  DoOneFrame(monitor_size, texture, /*is_full_screen=*/true);
}

// Check that two contiguous frames that share a `SwapChainPresenter` that
// changes the video texture will maintain the full screen optimization.
TEST_P(DCompPresenterFullScreenOptimizationPixelTest,
       KeepFullScreenDifferentImage) {
  // Test a fraction of a 1920x1200 monitor to speed up the pixel readback.
  const float scale_factor = 0.25;
  const gfx::Size monitor_size(1920 * scale_factor, 1200 * scale_factor);
  const gfx::Size texture_size(2, 2);

  SetMonitorSize(monitor_size);

  DoOneFrame(monitor_size,
             CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size),
             /*is_full_screen=*/true);

  DoOneFrame(monitor_size,
             CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size),
             /*is_full_screen=*/true);
}

// Check entering full screen mode succeeds when we do not change the video
// texture.
TEST_P(DCompPresenterFullScreenOptimizationPixelTest,
       EnterFullScreenSameImage) {
  // Test a fraction of a 1920x1200 monitor to speed up the pixel readback.
  const float scale_factor = 0.25;
  const gfx::Size monitor_size(1920 * scale_factor, 1200 * scale_factor);
  const gfx::Size texture_size(2, 2);

  SetMonitorSize(monitor_size);

  auto texture =
      CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size);

  DoOneFrame(monitor_size, texture, /*is_full_screen=*/false);
  DoOneFrame(monitor_size, texture, /*is_full_screen=*/true);
}

// Check exiting full screen mode succeeds when we do not change the video
// texture.
TEST_P(DCompPresenterFullScreenOptimizationPixelTest, ExitFullScreenSameImage) {
  // Test a fraction of a 1920x1200 monitor to speed up the pixel readback.
  const float scale_factor = 0.25;
  const gfx::Size monitor_size(1920 * scale_factor, 1200 * scale_factor);
  const gfx::Size texture_size(2, 2);

  SetMonitorSize(monitor_size);

  auto texture =
      CreateNV12Texture(GetDirectCompositionD3D11Device(), texture_size);

  DoOneFrame(monitor_size, texture, /*is_full_screen=*/true);
  DoOneFrame(monitor_size, texture, /*is_full_screen=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DCompPresenterFullScreenOptimizationPixelTest,
    DCompPresenterFullScreenOptimizationPixelTest::GetValues(
        testing::ConvertGenerator(testing::Bool())),
    &DCompPresenterFullScreenOptimizationPixelTest::GetParamName);

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
    window_size_ = window_size;
  }

  // Strips out the parameterization parts of the test case. This is to ensure
  // that the golden test names match.
  std::string GetGoldenTestName(const std::string& test_name) {
    std::string golden_test_name = test_name;

    // Find the position of the first '/'
    size_t first_slash_pos = golden_test_name.find('/');

    // If '/' exists, remove everything before it
    if (first_slash_pos != std::string::npos) {
      golden_test_name = golden_test_name.substr(first_slash_pos + 1);
    }

    return golden_test_name;
  }

  // Strips out the parameterization parts of the test case. This is to ensure
  // that the golden test case names match.
  std::string GetGoldenTestCaseName(const std::string& test_case_name) {
    std::string golden_test_case_name = test_case_name;

    // Find the position of the last '/'
    size_t last_slash_pos = golden_test_case_name.find_last_of('/');

    // If '/' exists, remove everything after it
    if (last_slash_pos != std::string::npos) {
      golden_test_case_name = golden_test_case_name.substr(0, last_slash_pos);
    }

    return golden_test_case_name;
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

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

    SkBitmap window_readback =
        GLTestHelper::ReadBackWindow(window_.hwnd(), window_size_);
    CHECK(pixel_diff_);

    std::string test_name = GetGoldenTestName(::testing::UnitTest::GetInstance()
                                                  ->current_test_info()
                                                  ->test_suite_name());
    std::string test_case_name = GetGoldenTestCaseName(
        ::testing::UnitTest::GetInstance()->current_test_info()->name());

    if (!pixel_diff_->CompareScreenshot(
            ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
                test_name, test_case_name,
                capture_name.empty() ? std::nullopt
                                     : std::make_optional(capture_name)),
            window_readback, matching_algorithm_.get())) {
      ADD_FAILURE_AT(caller_location.file_name(), caller_location.line_number())
          << "Screenshot mismatch for "
          << (capture_name.empty() ? "(unnamed capture)" : capture_name);
    }
  }

  const gfx::Size& current_window_size() const { return window_size_; }

  void AddOverlaysForOpacityTest(
      base::RepeatingCallback<DCLayerOverlayParams(const gfx::Rect&, float)>
          get_overlay_for_opacity) {
    const int kOverlayCount = 10;
    for (int i = 0; i < kOverlayCount; i++) {
      const int width = current_window_size().width() / kOverlayCount;
      const gfx::Rect quad_rect =
          gfx::Rect(i * width, 0, width, current_window_size().height());
      const float opacity =
          static_cast<float>(i) / static_cast<float>(kOverlayCount);

      auto overlay = get_overlay_for_opacity.Run(quad_rect, opacity);
      overlay.z_order = i + 1;
      overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(i);

      ScheduleOverlay(std::move(overlay));
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

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterSkiaGoldTest,
                         DCompPresenterSkiaGoldTest::GetValues(),
                         &DCompPresenterSkiaGoldTest::GetParamName);

// Check that a translation transform works.
TEST_P(DCompPresenterSkiaGoldTest, TransformTranslate) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(50, 50);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  overlay.transform.Translate(25, 25);

  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that a scaling transform works.
TEST_P(DCompPresenterSkiaGoldTest, TransformScale) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(50, 50);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  overlay.transform.Translate(50, 50);
  overlay.transform.Scale(1.2);
  overlay.transform.Translate(-25, -25);

  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that a rotation transform works.
TEST_P(DCompPresenterSkiaGoldTest, TransformRotation) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(50, 50);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  // Center and partially rotate the overlay
  overlay.transform.Translate(50, 50);
  overlay.transform.Rotate(15);
  overlay.transform.Translate(-25, -25);

  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that a complex transform (i.e. non-flat) works.
TEST_P(DCompPresenterSkiaGoldTest, Transform3D) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  DCLayerOverlayParams overlay;

  overlay.quad_rect = gfx::Rect(120, 75);

  overlay.background_color = SkColors::kGreen;

  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  overlay.transform.Translate(50, 50);
  overlay.transform.ApplyPerspectiveDepth(100);
  overlay.transform.RotateAboutYAxis(45);
  overlay.transform.RotateAboutXAxis(30);
  overlay.transform.Translate(-25, -25);

  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// This kind of transform is uncommon, but should be supported when rotations
// are supported.
TEST_P(DCompPresenterSkiaGoldTest, TransformShear) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(50, 50), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(50, 50);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  overlay.transform.Translate(50, 50);
  overlay.transform.Skew(15, 30);
  overlay.transform.Translate(-25, -25);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Test that solid color overlays completely fill their display rect.
TEST_P(DCompPresenterSkiaGoldTest, SolidColorSimpleOpaque) {
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
    DCLayerOverlayParams overlay;
    overlay.quad_rect = bounds;
    overlay.background_color = std::optional<SkColor4f>(color);
    overlay.z_order = i + 1;
    overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(i);
    ScheduleOverlay(std::move(overlay));
  }

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from DComp tree parameter.
TEST_P(DCompPresenterSkiaGoldTest, OpacityFromOverlay) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        auto overlay = CreateParamsFromImage(
            CreateDCompSurface(quad_rect.size(), SkColors::kWhite));
        overlay.quad_rect = quad_rect;
        overlay.opacity = opacity;
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from the overlay image itself.
TEST_P(DCompPresenterSkiaGoldTest, OpacityFromImage) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        SkColor4f overlay_color = SkColors::kWhite;
        overlay_color.fA = opacity;

        auto overlay = CreateParamsFromImage(
            CreateDCompSurface(quad_rect.size(), overlay_color));
        overlay.quad_rect = quad_rect;
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Test that opacity works when originating from a solid color overlay.
TEST_P(DCompPresenterSkiaGoldTest, OpacityFromSolidColor) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  AddOverlaysForOpacityTest(
      base::BindRepeating([](const gfx::Rect& quad_rect, float opacity) {
        SkColor4f overlay_color = SkColors::kWhite;
        overlay_color.fA = opacity;

        DCLayerOverlayParams overlay;
        overlay.quad_rect = quad_rect;
        overlay.background_color = std::optional<SkColor4f>(overlay_color);
        return overlay;
      }));

  PresentAndCheckScreenshot();
}

// Check that an overlay with a DComp surface will visually reflect draws to the
// surface if its dcomp_surface_serial changes. This requires DCLayerTree to
// call Commit, even if no other tree properties change.
TEST_P(DCompPresenterSkiaGoldTest, SurfaceSerialForcesCommit) {
  InitializeTest(gfx::Size(100, 100));

  const std::vector<SkColor4f> colors = {SkColors::kRed, SkColors::kGreen,
                                         SkColors::kBlue, SkColors::kWhite};

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
      GetDirectCompositionDevice();

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
    overlay.quad_rect = gfx::Rect(current_window_size());
    overlay.z_order = 0;
    overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(overlay));

    PresentAndCheckScreenshot(base::NumberToString(i));
  }
}

// Check that we support simple rounded corners.
TEST_P(DCompPresenterSkiaGoldTest, RoundedCornerSimple) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(current_window_size(), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(current_window_size());
  overlay.quad_rect.Inset(kPaddingFromEdgeForAntiAliasedOutput);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  overlay.rounded_corner_bounds =
      gfx::RRectF(gfx::RectF(overlay.quad_rect), 25.f);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we support rounded corners with complex radii.
TEST_P(DCompPresenterSkiaGoldTest, RoundedCornerNonUniformRadii) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(current_window_size(), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(current_window_size());
  overlay.quad_rect.Inset(kPaddingFromEdgeForAntiAliasedOutput);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  gfx::RRectF bounds = gfx::RRectF(gfx::RectF(overlay.quad_rect));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kUpperLeft, gfx::Vector2dF(5, 40));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kUpperRight,
                        gfx::Vector2dF(15, 30));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kLowerRight,
                        gfx::Vector2dF(25, 20));
  bounds.SetCornerRadii(gfx::RRectF::Corner::kLowerLeft,
                        gfx::Vector2dF(35, 10));
  overlay.rounded_corner_bounds = bounds;
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that there are no seams between solid color quads when there is a
// rounded corner clip present. Seams can appear since the solid color visual
// uses a shared image that is scaled to fit the overlay. The combination of
// scaling and soft borders implied by rounded corners can cause seams.
// This is a common case in e.g. the omnibox.
TEST_P(DCompPresenterSkiaGoldTest,
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
    DCLayerOverlayParams overlay;
    overlay.quad_rect = quad;
    overlay.background_color = std::optional<SkColor4f>(SkColors::kWhite);
    overlay.z_order = overlay_z_order;
    overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(overlay_z_order);
    overlay.rounded_corner_bounds = bounds;
    ScheduleOverlay(std::move(overlay));

    overlay_z_order++;
  }

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we translate the overlay so that both
// the right and left edges cover half a pixel.
TEST_P(DCompPresenterSkiaGoldTest, SoftBordersFromNonIntegralTranslation) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(20, 20), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(overlay.overlay_image->size());
  overlay.transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                              kPaddingFromEdgeForAntiAliasedOutput);
  overlay.transform.Translate(0.5, 0);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we scale the overlay so the right edge
// covers half a pixel.
TEST_P(DCompPresenterSkiaGoldTest, SoftBordersFromNonIntegralScaling) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(20, 20), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(overlay.overlay_image->size());
  overlay.transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                              kPaddingFromEdgeForAntiAliasedOutput);
  overlay.transform.Scale(
      (static_cast<float>(overlay.quad_rect.width()) + 0.5) /
          static_cast<float>(overlay.quad_rect.width()),
      1);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that we get a soft border when we create a non-integral rounded corner
// bounds so the right edge covers half a pixel.
TEST_P(DCompPresenterSkiaGoldTest,
       SoftBordersFromNonIntegralRoundedCornerBounds) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  auto overlay = CreateParamsFromImage(
      CreateDCompSurface(gfx::Size(21, 20), SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(overlay.overlay_image->size());

  // DComp seems to not actually use soft borders unless there's a non-zero
  // radius.
  const double kForceDCompRoundedCornerSoftBorder =
      std::numeric_limits<float>::epsilon();

  overlay.rounded_corner_bounds = gfx::RRectF(
      gfx::RectF(0, 0, 20.5, 20), kForceDCompRoundedCornerSoftBorder);
  overlay.rounded_corner_bounds.Offset(kPaddingFromEdgeForAntiAliasedOutput,
                                       kPaddingFromEdgeForAntiAliasedOutput);
  overlay.transform.Translate(kPaddingFromEdgeForAntiAliasedOutput,
                              kPaddingFromEdgeForAntiAliasedOutput);
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Check that an overlay with a non-opaque image can show a background color.
TEST_P(DCompPresenterSkiaGoldTest, ImageWithBackgroundColor) {
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
  overlay.quad_rect = gfx::Rect(100, 50);
  overlay.background_color = SkColors::kGreen;
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);

  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

// Test that we support sampling from overlay images with non-integral content
// rects. This test should output a blue square with a faint green outline.
TEST_P(DCompPresenterSkiaGoldTest, NonIntegralContentRectHalfCoverage) {
  InitializeTest(gfx::Size(100, 100));

  InitializeRootAndScheduleRootSurface(current_window_size(), SkColors::kBlack);

  gfx::Size image_size = gfx::Size(50, 50);
  gfx::Rect image_inner_rect = gfx::Rect(image_size);
  image_inner_rect.Inset(1);
  auto overlay = CreateParamsFromImage(CreateDCompSurface(
      image_size, SkColors::kGreen, {{image_inner_rect, SkColors::kBlue}}));
  overlay.content_rect.Inset(0.5);
  overlay.quad_rect = gfx::Rect(
      gfx::Point(20, 20),
      gfx::Size(overlay.content_rect.width(), overlay.content_rect.height()));
  overlay.z_order = 1;
  overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  ScheduleOverlay(std::move(overlay));

  PresentAndCheckScreenshot();
}

std::vector<DCLayerOverlayParams> GetOverlaysForSeamsWithComplexTransformTest(
    base::RepeatingCallback<void(int x, int y, DCLayerOverlayParams&)>
        update_overlay) {
  gfx::Transform non_integral_transform;
  non_integral_transform.Scale(0.7301, 0.773);
  non_integral_transform.Skew(3, 5);
  non_integral_transform.Translate(10.25, 5.15);

  std::vector<DCLayerOverlayParams> overlays;

  const gfx::Size tile_size = gfx::Size(25, 25);
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      DCLayerOverlayParams& overlay = overlays.emplace_back();
      overlay.quad_rect =
          gfx::Rect(x * tile_size.width(), y * tile_size.height(),
                    tile_size.width(), tile_size.height());
      overlay.content_rect = gfx::RectF(tile_size);
      overlay.overlay_image = CreateDCompSurface(tile_size, SkColors::kWhite);
      overlay.transform = non_integral_transform;
      overlay.z_order = x + y * 4 + 1;

      update_overlay.Run(x, y, overlay);
    }
  }

  return overlays;
}

// Check that DCLayerTree does not introduce seams from edge AA on adjacent
// overlays that are transformed. The result should be a solid quadrilateral.
TEST_P(DCompPresenterSkiaGoldTest, EdgeAANoSeamsOnSameLayerComplexTransform) {
  InitializeTest(gfx::Size(100, 100));

  ScheduleOverlays(GetOverlaysForSeamsWithComplexTransformTest(
      base::BindRepeating([](int x, int y, DCLayerOverlayParams& overlay) {
        // All on the same layer.
        overlay.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
      })));

  PresentAndCheckScreenshot();
}

// Check that we always have edge AA turned on for overlay transforms when there
// is only one overlay per contiguous layer ID in the overlay list. This should
// have the same output as |NoSeamsOnNonIntegralTransformSameLayer| but may
// include seams between the overlays.
TEST_P(DCompPresenterSkiaGoldTest, EdgeAASeamsOnNotSameLayerComplexTransform) {
  InitializeTest(gfx::Size(100, 100));

  ScheduleOverlays(GetOverlaysForSeamsWithComplexTransformTest(
      base::BindRepeating([](int x, int y, DCLayerOverlayParams& overlay) {
        // Reuse layer IDs but have no two adjacent overlays have the same ID.
        overlay.layer_id = gfx::OverlayLayerId::MakeForTesting((x + y * 4) % 2);
      })));

  PresentAndCheckScreenshot();
}

class DCompPresenterDelegatedInkSkiaGoldTest
    : public DCompPresenterSkiaGoldTest {
 protected:
  void SetUp() override {
    DCompPresenterSkiaGoldTest::SetUp();
    if (!presenter_->GetLayerTreeForTesting()->SupportsDelegatedInk()) {
      GTEST_SKIP() << "Delegated ink is not supported due to lack of gpu "
                      "support or availability of api on OS.";
    }
  }
  void ScheduleInkTrail(const gfx::RectF& presentation_area,
                        const SkColor& color,
                        float diameter,
                        base::span<const gfx::DelegatedInkPoint> points) {
    DCLayerTree* layer_tree = presenter_->GetLayerTreeForTesting();
    // Metadata timestamp and point should match the first DelegatedInkPoint.
    gfx::DelegatedInkMetadata metadata(points[0].point(), diameter, color,
                                       points[0].timestamp(), presentation_area,
                                       /*hovering=*/false);
    for (auto point : points) {
      layer_tree->GetInkRendererForTesting()->StoreDelegatedInkPoint(point);
    }
    layer_tree->SetDelegatedInkTrailStartPoint(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }
};

constexpr base::TimeTicks kEarliestTimestamp = base::TimeTicks();
constexpr base::TimeDelta kMicrosecondsBetweenEachPoint =
    base::Microseconds(10);

INSTANTIATE_TEST_SUITE_P(All,
                         DCompPresenterDelegatedInkSkiaGoldTest,
                         DCompPresenterDelegatedInkSkiaGoldTest::GetValues(),
                         &DCompPresenterDelegatedInkSkiaGoldTest::GetParamName);

// This test validates the following:
// The presentation area with a non-zero and non-integer origin is
// rendered correctly. An ink trail is drawn at the edge of the
// presentation area, just within the bounds. It should be clipped to the
// presentation area and placed correctly within the root frame - window
// coordinates. Note that although the presentation area in the metadata
// is a gfx::RectF, an integral enclosed rect size is used to build
// the content rect and the quad rect of the overlay.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, NonIntegerPresentationArea) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);
  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);

  // Create and send some delegated ink points + metadata.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(10.7, 10.7), kEarliestTimestamp, kPointerId},
      {gfx::PointF(10.7, 30),
       kEarliestTimestamp + kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(10.4, 10.4, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);
  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual.
  PresentAndCheckScreenshot("ink-trail-present");
}

// This test checks whether the delegated ink trail is synchronized with the
// root swap chain. The ink trail should be absent if the swap chain is
// presented and no delegated ink visual subtree is added to the tree.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, TrailSyncedToSwapChainPresent) {
  // Initialize the swap chain that will be used as the root overlay
  // image.
  gfx::Size swap_chain_size(200, 200);
  InitializeTest(swap_chain_size);
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
  Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
  InitializeSwapChainForTest(swap_chain_size, swap_chain, rtv);
  ASSERT_TRUE(swap_chain);
  ASSERT_TRUE(rtv);

  // Define 200x200 monitor size.
  const gfx::Size monitor_size(200, 200);

  // Create and send some delegated ink points slightly outside the bounds.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(12, 12), kEarliestTimestamp, kPointerId},
      {gfx::PointF(12, 30), kEarliestTimestamp + kMicrosecondsBetweenEachPoint,
       kPointerId}};
  gfx::RectF presentation_area(10, 10, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);

  // Make root overlay.
  auto dc_layer_params =
      CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
  dc_layer_params.quad_rect = gfx::Rect(monitor_size);
  dc_layer_params.z_order = 0;
  dc_layer_params.layer_id = GetRootSurfaceId();
  ScheduleOverlay(std::move(dc_layer_params));

  ASSERT_HRESULT_SUCCEEDED(ClearRenderTargetViewAndPresent(
      SkColors::kGreen, swap_chain.Get(), rtv.Get()));
  // Verify trail present.
  PresentAndCheckScreenshot("ink-trail-present");

  // Clear swap chain to blue and make sure the delegated ink trail is not
  // present for this next frame.
  ASSERT_HRESULT_SUCCEEDED(ClearRenderTargetViewAndPresent(
      SkColors::kBlue, swap_chain.Get(), rtv.Get()));
  dc_layer_params =
      CreateParamsFromImage(DCLayerOverlayImage(swap_chain_size, swap_chain));
  dc_layer_params.quad_rect = gfx::Rect(monitor_size);
  dc_layer_params.layer_id = GetRootSurfaceId();
  ScheduleOverlay(std::move(dc_layer_params));
  PresentAndCheckScreenshot("cleared-swapchain");
}

// This test checks whether a delegated ink trail renders correctly
// for a root surface with a dcomp surface image, and is correctly removed
// when no metadata is present for a frame.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, RootSurfaceIsDCompSurface) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);

  // Create and send some delegated ink points + metadata.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(11, 11), kEarliestTimestamp, kPointerId},
      {gfx::PointF(30, 30), kEarliestTimestamp + kMicrosecondsBetweenEachPoint,
       kPointerId},
      {gfx::PointF(49, 11),
       kEarliestTimestamp + 2 * kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(10, 10, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);

  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual.
  PresentAndCheckScreenshot("ink-trail-present");

  // Create another surface and make sure there's no ink trail in the next
  // frame.
  auto overlay =
      CreateParamsFromImage(CreateDCompSurface(window_size, SkColors::kWhite));
  overlay.quad_rect = gfx::Rect(200, 200);
  overlay.z_order = 0;
  overlay.layer_id = GetRootSurfaceId();
  ScheduleOverlay(std::move(overlay));
  PresentAndCheckScreenshot("no-ink-trail");
}

// This test verifies that the ink trail renders correctly when the delegated
// ink metadata changes properties such as color and diameter.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, MetadataChangesProperties) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);

  // Create and send some delegated ink points + metadata.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(11, 11), kEarliestTimestamp, kPointerId},
      {gfx::PointF(30, 30), kEarliestTimestamp + kMicrosecondsBetweenEachPoint,
       kPointerId},
      {gfx::PointF(49, 11),
       kEarliestTimestamp + 2 * kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(10, 10, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);

  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual.
  PresentAndCheckScreenshot("red-ink-trail");

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);
  // Create and send some delegated ink points + metadata with different
  // properties.
  const int kPointerId2 = 3;
  gfx::DelegatedInkPoint blue_points[] = {
      {gfx::PointF(11, 11),
       kEarliestTimestamp + 3 * kMicrosecondsBetweenEachPoint, kPointerId2},
      {gfx::PointF(30, 30),
       kEarliestTimestamp + 4 * kMicrosecondsBetweenEachPoint, kPointerId2},
      {gfx::PointF(49, 11),
       kEarliestTimestamp + 5 * kMicrosecondsBetweenEachPoint, kPointerId2}};
  ScheduleInkTrail(presentation_area, SK_ColorBLUE, /* diameter= */ 5.0,
                   blue_points);

  PresentAndCheckScreenshot("thin-blue-ink-trail");
}

// This test validates that the Ink trail does not render when metadata is
// outside presentation area by a fraction. The metadata corresponds to
// the first ink point in this case.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest,
       MetadataOutsidePresentationArea) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);

  // Create and send the first ink point out of bounds. The first ink point is
  // transmitted as the metadata. The rest of the points are within bounds.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(10.1, 10.1), kEarliestTimestamp, kPointerId},
      {gfx::PointF(10.6, 30),
       kEarliestTimestamp + kMicrosecondsBetweenEachPoint, kPointerId},
      {gfx::PointF(10.6, 60),
       kEarliestTimestamp + 2 * kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(10.4, 10.4, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);

  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual.
  PresentAndCheckScreenshot("no-ink-trail");
}

// The test verifies that the ink trail is correctly clipped when part of it is
// outside the presentation area.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, InkTrailPortionClipped) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);

  // Draw a red trail. The trail has some points outside of the presentation
  // area which should be clipped.
  const int kPointerId = 2;
  gfx::DelegatedInkPoint points[] = {
      // Inside presentation area.
      {gfx::PointF(55, 55), kEarliestTimestamp, kPointerId},
      // Outside presentation area.
      {gfx::PointF(75, 120), kEarliestTimestamp + kMicrosecondsBetweenEachPoint,
       kPointerId},
      // Inside presentation area.
      {gfx::PointF(90, 55),
       kEarliestTimestamp + 2 * kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(50, 50, 100, 100);
  ScheduleInkTrail(presentation_area, SK_ColorRED, /* diameter= */ 10.0,
                   points);

  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual. Trail should resemble a "V" cut
  // from the bottom.
  PresentAndCheckScreenshot("red-ink-trail-10");
}

// The test verifies that the ink trail is correctly clipped when the points are
// within the presentation area but its thickness causes a portion of it to be
// outside the bounds.
TEST_P(DCompPresenterDelegatedInkSkiaGoldTest, InkTrailClippedDueToThickness) {
  gfx::Size window_size(200, 200);
  InitializeTest(window_size);

  InitializeRootAndScheduleRootSurface(window_size, SkColors::kWhite);
  const int kPointerId = 2;

  // Draw a thicker blue trail in the next frame. This trail should also be
  // clipped at the bottom; while the point is within the bounds, the diameter
  // will cause the ink trail to extend beyond.
  gfx::DelegatedInkPoint points[] = {
      {gfx::PointF(55, 55), kEarliestTimestamp, kPointerId},
      {gfx::PointF(55, 85), kEarliestTimestamp + kMicrosecondsBetweenEachPoint,
       kPointerId},
      {gfx::PointF(55, 100),
       kEarliestTimestamp + 2 * kMicrosecondsBetweenEachPoint, kPointerId}};
  gfx::RectF presentation_area(50, 50, 100, 100);

  ScheduleInkTrail(presentation_area, SK_ColorBLUE, /* diameter= */ 40.0,
                   points);

  // Commit. Delegated ink overlay will be created here and its visual subtree
  // will be added to the dcomp root visual.
  PresentAndCheckScreenshot("blue-ink-trail-40");
}

struct TripleBufferParam {
  bool use_triple_buffer_video_swap_chain = false;
};

void PrintTo(const TripleBufferParam& param, std::ostream* os) {
  if (param.use_triple_buffer_video_swap_chain) {
    *os << "DCompTripleBufferVideoSwapChain";
  } else {
    *os << "DcompTripleBufferVideoSwapChain_default";
  }
}

class DCompPresenterBufferCountTest
    : public DCompPresenterTestBase<TripleBufferParam> {
 protected:
  void SetUp() override {
    if (GetTestParam().use_triple_buffer_video_swap_chain) {
      DCompPresenterTestBase::EnableFeature(
          features::kDCompTripleBufferVideoSwapChain);
    } else {
      DCompPresenterTestBase::DisableFeature(
          features::kDCompTripleBufferVideoSwapChain);
    }
    DCompPresenterTestBase::SetUp();
  }
};

TEST_P(DCompPresenterBufferCountTest, VideoSwapChainBufferCount) {
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  constexpr gfx::Size window_size(100, 100);
  EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

  constexpr gfx::Size texture_size(50, 50);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  ASSERT_TRUE(d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  auto params =
      CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
  params.quad_rect = gfx::Rect(window_size);
  params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
  params.video_params.color_space = gfx::ColorSpace::CreateREC709();
  ScheduleOverlay(std::move(params));

  ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);

  auto swap_chain = presenter_->GetLayerSwapChainForTesting(
      gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);

  DXGI_SWAP_CHAIN_DESC1 desc;
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  // The expected size is window_size(100, 100).
  EXPECT_EQ(100u, desc.Width);
  EXPECT_EQ(100u, desc.Height);
  if (GetTestParam().use_triple_buffer_video_swap_chain) {
    EXPECT_EQ(3u, desc.BufferCount);
  } else {
    EXPECT_EQ(2u, desc.BufferCount);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterBufferCountTest,
                         DCompPresenterBufferCountTest::GetValues(
                             testing::ConvertGenerator(testing::Bool())),
                         &DCompPresenterBufferCountTest::GetParamName);

enum class SwapChainPresentationMode {
  kDecodeSwapChain,
  kMFDCompSurface,
};

struct LetterboxingTestParams {
  bool use_letterbox_video_optimization = false;
  SwapChainPresentationMode presentation_mode =
      SwapChainPresentationMode::kDecodeSwapChain;
  bool early_full_screen_video_optimization = false;
};

void PrintTo(const LetterboxingTestParams& param, std::ostream* os) {
  if (param.use_letterbox_video_optimization) {
    *os << "LetterboxOptOn";
  } else {
    *os << "LetterboxOptOff";
  }

  *os << "_";

  switch (param.presentation_mode) {
    case SwapChainPresentationMode::kDecodeSwapChain:
      *os << "DecodeSwapChain";
      break;
    case SwapChainPresentationMode::kMFDCompSurface:
      *os << "MFDCompSurface";
      break;
  }

  *os << "_";

  if (param.early_full_screen_video_optimization) {
    *os << "EarlyFullScreenOptOn";
  } else {
    *os << "EarlyFullScreenOptOff";
  }
}

class DCompPresenterLetterboxingTest
    : public DCompPresenterTestBase<LetterboxingTestParams> {
 protected:
  void SetUp() override {
    SetupScopedFeatureList();
    DCompPresenterTestBase::SetUp();
  }

  virtual void SetupScopedFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    // TODO(crbug.com/428158600): For now set up
    // kDesktopPlaneRemovalForMFFullScreenLetterbox flag by
    // following kDirectCompositionLetterboxVideoOptimization flag.
    if (GetTestParam().use_letterbox_video_optimization) {
      DCompPresenterTestBase::EnableFeature(
          features::kDirectCompositionLetterboxVideoOptimization);
      DCompPresenterTestBase::EnableFeature(
          features::kDesktopPlaneRemovalForMFFullScreenLetterbox);
    } else {
      DCompPresenterTestBase::DisableFeature(
          features::kDirectCompositionLetterboxVideoOptimization);
      DCompPresenterTestBase::DisableFeature(
          features::kDesktopPlaneRemovalForMFFullScreenLetterbox);
    }

    if (GetTestParam().early_full_screen_video_optimization) {
      DCompPresenterTestBase::EnableFeature(
          features::kEarlyFullScreenVideoOptimization);
    } else {
      DCompPresenterTestBase::DisableFeature(
          features::kEarlyFullScreenVideoOptimization);
    }
  }

  class MockDCOMPSurfaceProxy : public gl::DCOMPSurfaceProxy {
   public:
    MockDCOMPSurfaceProxy()
        : scoped_handle_(
              gl::SwapChainPresenter::CreateDCompSurfaceHandleForTesting()) {}
    MOCK_METHOD(gfx::Size&, GetSize, (), (const, override));
    MOCK_METHOD(void,
                SetRect,
                (const gfx::Rect& window_relative_rect),
                (override));
    MOCK_METHOD(void, SetParentWindow, (HWND parent), (override));

    HANDLE GetSurfaceHandle() override { return scoped_handle_.Get(); }

   private:
    ~MockDCOMPSurfaceProxy() override = default;

    const base::win::ScopedHandle scoped_handle_;
  };

  void ScheduleFullScreenOverlay(DCLayerOverlayParams overlay) {
    if (GetTestParam().use_letterbox_video_optimization &&
        base::FeatureList::IsEnabled(
            features::kEarlyFullScreenVideoOptimization)) {
      overlay.video_params.is_full_screen_video = true;
    } else {
      overlay.video_params.possible_video_fullscreen_letterboxing = true;
    }

    DCompPresenterTestBase<LetterboxingTestParams>::ScheduleOverlay(
        std::move(overlay));
  }

  DCLayerOverlayImage CreateOverlayImage(
      const gfx::Size& resource_size,
      const gfx::Size& monitor_size,
      const gfx::Rect& clip_rect,
      std::optional<gfx::Rect> dcomp_surface_set_rect_override = std::nullopt) {
    switch (GetTestParam().presentation_mode) {
      case SwapChainPresentationMode::kDecodeSwapChain: {
        Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
            GetDirectCompositionD3D11Device();
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
            CreateNV12Texture(d3d11_device, resource_size);
        return DCLayerOverlayImage(resource_size, texture);
      }
      case SwapChainPresentationMode::kMFDCompSurface: {
        auto dcomp_surface_proxy =
            CreateMFDCompSurfaceProxy(resource_size, monitor_size, clip_rect,
                                      dcomp_surface_set_rect_override);
        return DCLayerOverlayImage(resource_size, dcomp_surface_proxy);
      }
    }
    NOTREACHED() << "Unexpected presentation_mode value: "
                 << static_cast<int>(GetTestParam().presentation_mode);
  }

  // `dcomp_surface_set_rect_override` can be optionally passed in to set an
  // explicit Rect we expect `DCOMPSurfaceProxy->SetRect` to be called with,
  // such as if we have a non-uniform transform and do not expect letterboxing
  // optimizations to trigger. If this parameter is not passed in, the default
  // behavior is to expect SetRect is called with the monitor size due to
  // letterboxing optimizations, or just the clip rect if letterboxing
  // optimizations are not used.
  scoped_refptr<MockDCOMPSurfaceProxy> CreateMFDCompSurfaceProxy(
      const gfx::Size& resource_size,
      const gfx::Size& monitor_size,
      const gfx::Rect& clip_rect,
      std::optional<gfx::Rect> dcomp_surface_set_rect_override = std::nullopt) {
    auto dcomp_surface_proxy = base::MakeRefCounted<MockDCOMPSurfaceProxy>();
    EXPECT_CALL(*dcomp_surface_proxy, SetParentWindow(testing::_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*dcomp_surface_proxy, GetSize())
        .WillRepeatedly(::testing::ReturnRefOfCopy(resource_size));
    gfx::Rect expected_rect;
    if (dcomp_surface_set_rect_override.has_value()) {
      expected_rect = dcomp_surface_set_rect_override.value();
    } else {
      expected_rect = GetTestParam().use_letterbox_video_optimization
                          ? gfx::Rect(monitor_size)
                          : clip_rect;
    }
    EXPECT_CALL(*dcomp_surface_proxy, SetRect(testing::Eq(expected_rect)))
        .Times(testing::AnyNumber());
    return dcomp_surface_proxy;
  }

  bool CheckVideoDisablesDesktopPlane(const gfx::Size& monitor_size) {
    // To disable the desktop plane, the swap chain or dcomp surface must be
    // unoccluded and covering the whole screen. Retrieve the front-most overlay
    // in the visual tree, ensure it is a decode swap chain or dcomp surface,
    // and make sure it is unoccluded.
    gl::DCLayerTree::VisualTree::VisualSubtree* front_sub_tree =
        presenter_->GetLayerTreeForTesting()
            ->GetFrontMostVideoVisualSubtreeForTesting();
    EXPECT_NE(front_sub_tree, nullptr);

    gfx::Transform visual_transform =
        front_sub_tree->GetQuadToRootTransformForTesting();
    // Rotated videos cannot cover the whole screen.
    EXPECT_TRUE(visual_transform.Preserves2dAxisAlignment());
    gfx::Rect monitor_rect(monitor_size);
    gfx::Rect onscreen_rect;
    std::optional<gfx::Rect> clip_rect =
        front_sub_tree->GetClipRectInRootForTesting();

    if (GetTestParam().presentation_mode ==
        SwapChainPresentationMode::kDecodeSwapChain) {
      Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
      HRESULT hr = front_sub_tree->dcomp_visual_content()->QueryInterface(
          IID_PPV_ARGS(&decode_swap_chain));
      if (FAILED(hr) || !decode_swap_chain) {
        // This is reachable but current tests only test decode swap chain
        // presentation. Adjust if supporting regular swap chain testing.
        NOTREACHED();
      }

      // TODO(crbug.com/414842426): `SwapChainPresenter` doesn't tell
      // `DCLayerTree` how large the video content is, so must use DestSize to
      // find out instead of with a property on the overlay.

      uint32_t dest_width, dest_height;
      EXPECT_HRESULT_SUCCEEDED(
          decode_swap_chain->GetDestSize(&dest_width, &dest_height));

      // This is the actual onscreen rect of the video.
      onscreen_rect =
          visual_transform.MapRect(gfx::Rect(dest_width, dest_height));

      // If there is a `clip_rect` present, clip the actual onscreen rect to a
      // clipped onscreen rect.
      if (clip_rect.has_value()) {
        onscreen_rect.Intersect(clip_rect.value());
      }
    } else if (GetTestParam().presentation_mode ==
               SwapChainPresentationMode::kMFDCompSurface) {
      // TODO(crbug.com/414842426): The clip rect is the only rect information
      // we have about the dcomp surface. Add support for `SwapChainPresenter`
      // to track size of the dcomp surface and track a target rect of where the
      // content is being mapped inside its actual size.

      // We are not able to get the rect used to originally call
      // `DCOMPSurfaceProxy::SetRect`, but we can use the visual_clip_rect.
      // `SwapChainPresenter` will use the monitor size to call SetRect and will
      // set its visual_clip_rect as the monitor size. As further validation, on
      // creation of the `MockDCOMPSurfaceProxy` in `CreateOverlayImage`, we
      // check that the monitor size is used to call SetRect.
      if (!clip_rect.has_value()) {
        // Current implementation of tests expect that we have a clip rect.
        NOTREACHED();
      }
      onscreen_rect = clip_rect.value();
    }

    // If the video layer's onscreen rect covers the monitor,
    // that allows for DWM to disable the desktop plane.
    if (!onscreen_rect.Contains(monitor_rect)) {
      return false;
    }

    return true;
  }

  // The video is correctly letterboxed if the logical onscreen rect touches
  // two sides of the monitor and is centered.
  // TODO(crbug.com/416206298): Add full DWM optimization validations such as
  // DMRRS enablement checks.
  bool CheckVideoIsLetterboxedCorrectly(const gfx::Size& monitor_size) {
    gl::DCLayerTree::VisualTree::VisualSubtree* front_sub_tree =
        presenter_->GetLayerTreeForTesting()
            ->GetFrontMostVideoVisualSubtreeForTesting();
    EXPECT_NE(front_sub_tree, nullptr);

    gfx::Transform visual_transform =
        front_sub_tree->GetQuadToRootTransformForTesting();
    gfx::Rect monitor_rect(monitor_size);
    gfx::Rect onscreen_rect;

    if (GetTestParam().presentation_mode ==
        SwapChainPresentationMode::kDecodeSwapChain) {
      Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
      HRESULT hr = front_sub_tree->dcomp_visual_content()->QueryInterface(
          IID_PPV_ARGS(&decode_swap_chain));
      if (FAILED(hr) || !decode_swap_chain) {
        // This is reachable but current tests only test decode swap chain
        // presentation. Adjust if supporting regular swap chain testing.
        NOTREACHED();
      }

      RECT target_rect;
      EXPECT_HRESULT_SUCCEEDED(decode_swap_chain->GetTargetRect(&target_rect));
      // Note: We want the position of the actual video contents (i.e.
      // `target_rect`) in window space. We're assuming here that, when
      // full-screened, the swap chain (i.e. `dest_size`) is the size of the
      // monitor and the overlay layer (i.e. `quad_rect` + `transform`) also
      // fills the monitor.
      //
      // We make this assumption because running with
      // `!EarlyFullScreenVideoOptimization`, `DCLayerTree` does not correctly
      // place the `quad_rect` in the full screen case and relies on letting the
      // content visual overflow its intended bounds.
      onscreen_rect = gfx::Rect(target_rect);
      onscreen_rect.Offset(
          gfx::ToRoundedVector2d(visual_transform.To2dTranslation()));
    } else if (GetTestParam().presentation_mode ==
               SwapChainPresentationMode::kMFDCompSurface) {
      // TODO(crbug.com/414842426): The clip rect is the only rect information
      // we have about the dcomp surface. Add support for `SwapChainPresenter`
      // to track size of the dcomp surface and track a target rect of where the
      // content is being mapped inside its actual size.

      // We are not able to get the rect used to originally call
      // `DCOMPSurfaceProxy::SetRect`, but we can use the visual_clip_rect.
      // `SwapChainPresenter` will use the monitor size to call SetRect and will
      // set its visual_clip_rect as the monitor size. As further validation, on
      // creation of the `MockDCOMPSurfaceProxy` in `CreateOverlayImage`, we
      // check that the monitor size is used to call SetRect.
      std::optional<gfx::Rect> clip_rect =
          front_sub_tree->GetClipRectInRootForTesting();
      if (!clip_rect.has_value()) {
        // Current implementation of tests expect that we have a clip rect.
        NOTREACHED();
      }
      onscreen_rect = clip_rect.value();
    }

    bool is_letterboxing =
        onscreen_rect.x() == 0 && onscreen_rect.width() == monitor_rect.width();
    bool is_pillarboxing = onscreen_rect.y() == 0 &&
                           onscreen_rect.height() == monitor_rect.height();

    if (is_letterboxing && is_pillarboxing) {
      return true;
    } else if (is_letterboxing) {
      float monitor_center_y = monitor_rect.height() / 2;
      float onscreen_rect_center_y =
          onscreen_rect.y() + (onscreen_rect.height() / 2);
      if (std::abs(monitor_center_y - onscreen_rect_center_y) <= 1) {
        return true;
      }
    } else if (is_pillarboxing) {
      float monitor_center_x = monitor_rect.width() / 2;
      float onscreen_rect_center_x =
          onscreen_rect.x() + (onscreen_rect.width() / 2);
      if (std::abs(monitor_center_x - onscreen_rect_center_x) <= 1) {
        return true;
      }
    }

    // Neither letterboxing nor pillarboxing.
    return false;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DCompPresenterLetterboxingTest,
    DCompPresenterLetterboxingTest::GetValues(testing::ConvertGenerator(
        testing::Combine(
            testing::Bool(),
            testing::Values(SwapChainPresentationMode::kDecodeSwapChain,
                            SwapChainPresentationMode::kMFDCompSurface),
            testing::Bool()),
        [](std::tuple<bool, SwapChainPresentationMode, bool> t) {
          return LetterboxingTestParams{
              .use_letterbox_video_optimization = std::get<0>(t),
              .presentation_mode = std::get<1>(t),
              .early_full_screen_video_optimization = std::get<2>(t),
          };
        })),
    &DCompPresenterLetterboxingTest::GetParamName);

TEST_P(DCompPresenterLetterboxingTest, FullScreenLetterboxingResizeVideoLayer) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Use 1080p content size for texture or dcomp surface.
  const gfx::Size resource_size(1920, 1080);
  const int letterboxing_height =
      (monitor_size.height() - resource_size.height()) / 2;
  const gfx::Rect quad_rect(0, 0, resource_size.width(),
                            resource_size.height());

  // First test if swap chain and its visual info is adjusted to fit the
  // monitor when letterboxing is generated for full screen presentation.
  gfx::Transform quad_to_root_transform =
      gfx::Transform::MakeTranslation(0, letterboxing_height);
  gfx::Rect clip_rect = quad_to_root_transform.MapRect(quad_rect);

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  if (base::FeatureList::IsEnabled(
          features::kEarlyFullScreenVideoOptimization)) {
    // The rest of this test checks that video overlays are adjusted for full
    // screen. EarlyFullScreenVideoOptimization handles adjustment of overlay
    // position during overlay processing.
    return;
  }

  // Second test if swap chain visual info is adjusted to fit the monitor when
  // some negative offset from typical letterboxing positioning.
  quad_to_root_transform =
      gfx::Transform::MakeTranslation(0, letterboxing_height - 2);
  clip_rect = quad_to_root_transform.MapRect(quad_rect);

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_FALSE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  // Third test if swap chain visual info is adjusted to fit the monitor when
  // some positive offset from typical letterboxing positioning.
  quad_to_root_transform =
      gfx::Transform::MakeTranslation(0, letterboxing_height + 2);
  clip_rect = quad_to_root_transform.MapRect(quad_rect);

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_FALSE(CheckVideoIsLetterboxedCorrectly(monitor_size));
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

  // Use 1080p content size for texture or dcomp surface.
  const gfx::Size resource_size(1920, 1080);
  const int letterboxing_height =
      (monitor_size.height() - resource_size.height()) / 2;
  const gfx::Rect quad_rect(0, 0, resource_size.width(),
                            resource_size.height());
  const gfx::Rect clip_rect(0, letterboxing_height, resource_size.width(),
                            resource_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height)));

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
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

  // Make a 1080p texture or dcomp surface as display input.
  const gfx::Size resource_size(1920, 1080);

  // First full screen presentation with letterboxing.
  const int letterboxing_height =
      (monitor_size.height() - resource_size.height()) / 2;
  const gfx::Rect quad_rect =
      gfx::Rect(0, 0, resource_size.width(), resource_size.height());
  const gfx::Rect clip_rect = gfx::Rect(
      0, letterboxing_height, resource_size.width(), resource_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0, letterboxing_height)));

  DCLayerOverlayImage overlay_image =
      CreateOverlayImage(resource_size, monitor_size, clip_rect);
  {
    auto dc_layer_params =
        CreateParamsFromImage(overlay_image.CloneForTesting());

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
  UINT last_present_count;
  if (GetTestParam().presentation_mode ==
      SwapChainPresentationMode::kDecodeSwapChain) {
    // Make sure it's a valid swap chain presentation
    swap_chain = presenter_->GetLayerSwapChainForTesting(
        gfx::OverlayLayerId::MakeForTesting(0));
    ASSERT_TRUE(swap_chain);

    // One present is normal, and a second present because it's the first frame
    // and the other buffer needs to be drawn to.
    last_present_count = 0;
    EXPECT_HRESULT_SUCCEEDED(
        swap_chain->GetLastPresentCount(&last_present_count));
    EXPECT_EQ(2u, last_present_count);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  // Swap chain visual info is collected for the first presentation.
  gfx::Transform visual_transform1;
  gfx::Point visual_offset1;
  gfx::Rect visual_clip_rect1;
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &visual_transform1,
      &visual_offset1, &visual_clip_rect1);

  // Followed by second presentation with the same image.
  {
    auto dc_layer_params =
        CreateParamsFromImage(overlay_image.CloneForTesting());

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().presentation_mode ==
      SwapChainPresentationMode::kDecodeSwapChain) {
    // It's the same image, so it should have the same swapchain.
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain2 =
        presenter_->GetLayerSwapChainForTesting(
            gfx::OverlayLayerId::MakeForTesting(0));
    EXPECT_EQ(swap_chain2.Get(), swap_chain.Get());

    // No new presentation happened and no present count increase since it's
    // with the same image.
    EXPECT_HRESULT_SUCCEEDED(
        swap_chain->GetLastPresentCount(&last_present_count));
    EXPECT_EQ(2u, last_present_count);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  // Swap chain visual info should be kept same as the previous presentation.
  gfx::Transform visual_transform2;
  gfx::Point visual_offset2;
  gfx::Rect visual_clip_rect2;
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &visual_transform2,
      &visual_offset2, &visual_clip_rect2);
  EXPECT_EQ(visual_transform1, visual_transform2);
  EXPECT_EQ(visual_offset1, visual_offset2);
  EXPECT_EQ(visual_clip_rect1, visual_clip_rect2);

  {
    auto dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }
  if (GetTestParam().presentation_mode ==
      SwapChainPresentationMode::kDecodeSwapChain) {
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain3 =
        presenter_->GetLayerSwapChainForTesting(
            gfx::OverlayLayerId::MakeForTesting(0));
    EXPECT_HRESULT_SUCCEEDED(
        swap_chain3->GetLastPresentCount(&last_present_count));
    // The present count should increase with the new image presentation.
    EXPECT_EQ(3u, last_present_count);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }
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

  // Use 1800x1200 content size for texture or dcomp surface.
  const gfx::Size resource_size(1800, 1200);
  const int letterboxing_width =
      (monitor_size.width() - resource_size.width()) / 2;
  const gfx::Rect quad_rect(0, 0, resource_size.width(),
                            resource_size.height());

  // First test if swap chain and its visual info is adjusted to fit the
  // monitor when letterboxing is generated for full screen presentation.
  int offset = 0;
  gfx::Transform quad_to_root_transform =
      gfx::Transform::MakeTranslation(letterboxing_width + offset, 0);
  gfx::Rect clip_rect = quad_to_root_transform.MapRect(quad_rect);
  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  if (base::FeatureList::IsEnabled(
          features::kEarlyFullScreenVideoOptimization)) {
    // The rest of this test checks that video overlays are adjusted for full
    // screen. EarlyFullScreenVideoOptimization handles adjustment of overlay
    // position during overlay processing.
    return;
  }

  // Second test if swap chain visual info is adjusted to fit the monitor when
  // some negative offset from typical letterboxing positioning.
  offset = -2;
  quad_to_root_transform =
      gfx::Transform::MakeTranslation(letterboxing_width + offset, 0);
  clip_rect = quad_to_root_transform.MapRect(quad_rect);

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_FALSE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }

  // Third test if swap chain visual info is adjusted to fit the monitor when
  // some positive offset from typical letterboxing positioning.
  offset = 2;
  quad_to_root_transform =
      gfx::Transform::MakeTranslation(letterboxing_width + offset, 0);
  clip_rect = quad_to_root_transform.MapRect(quad_rect);

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_FALSE(CheckVideoIsLetterboxedCorrectly(monitor_size));
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

  // Use 1800x1200 content size for texture or dcomp surface.
  const gfx::Size resource_size(1800, 1200);
  const int letterboxing_width =
      (monitor_size.width() - resource_size.width()) / 2;
  const gfx::Rect quad_rect(0, 0, resource_size.width(),
                            resource_size.height());
  const gfx::Rect clip_rect(letterboxing_width, 0, resource_size.width(),
                            resource_size.height());
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(letterboxing_width, 0)));

  {
    DCLayerOverlayParams dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect));

    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));
    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }
}

// MF presentation via `DCOMPSurfaceProxy->SetRect` only supports uniform
// scaling. MF will not correctly size letterboxed videos with non-uniform
// scaling, so they are not subject to DWM fullscreen optimizations.
TEST_P(DCompPresenterLetterboxingTest,
       FullScreenLetterboxingNonUniformScaling) {
  // Define 1920x1200 monitor size.
  const gfx::Size monitor_size(1920, 1200);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Use a 1000x1000 content size for texture or dcomp surface proxy.
  scoped_refptr<MockDCOMPSurfaceProxy> dcomp_surface_proxy =
      base::MakeRefCounted<MockDCOMPSurfaceProxy>();
  const gfx::Size resource_size(1000, 1000);
  // Target letterboxed rect after non-uniform scaling is 1920x1080 and centered
  // in monitor.
  const int letterboxing_height = 60;
  const gfx::Rect target_letterboxed_rect(0, letterboxing_height, 1920, 1080);

  // Test if dcomp surface and its visual info remains the clip size
  // when letterboxing is generated for full screen presentation.
  const gfx::Rect quad_rect = gfx::Rect(resource_size);
  const gfx::Rect clip_rect = target_letterboxed_rect;
  gfx::Transform quad_to_root_transform = gfx::TransformBetweenRects(
      gfx::RectF(quad_rect), gfx::RectF(target_letterboxed_rect));

  {
    // Override expected value for `DCOMPSurfaceProxy->SetRect` because
    // letterboxing optimizations will not be used due to non-uniform scaling.
    std::optional<gfx::Rect> dcomp_surface_set_rect_override;
    if (GetTestParam().presentation_mode ==
        SwapChainPresentationMode::kMFDCompSurface) {
      dcomp_surface_set_rect_override = target_letterboxed_rect;
    }
    auto dc_layer_params = CreateParamsFromImage(
        CreateOverlayImage(resource_size, monitor_size, clip_rect,
                           dcomp_surface_set_rect_override));
    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleFullScreenOverlay(std::move(dc_layer_params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  if (GetTestParam().use_letterbox_video_optimization) {
    if (GetTestParam().presentation_mode ==
        SwapChainPresentationMode::kDecodeSwapChain) {
      EXPECT_TRUE(CheckVideoDisablesDesktopPlane(monitor_size));
    } else if (GetTestParam().presentation_mode ==
               SwapChainPresentationMode::kMFDCompSurface) {
      // In dcomp surface case, letterboxing optimizations won't be used for
      // non-uniform transform, so desktop plane won't be able to be disabled.
      EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    }
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  } else {
    EXPECT_FALSE(CheckVideoDisablesDesktopPlane(monitor_size));
    EXPECT_TRUE(CheckVideoIsLetterboxedCorrectly(monitor_size));
  }
}

class DCompPresenterFullscreenRoundingTest : public DCompPresenterTestBase<> {};

TEST_P(DCompPresenterFullscreenRoundingTest,
       FullScreenRoundingWithHalfPixelTranslation) {
  if (base::FeatureList::IsEnabled(
          features::kEarlyFullScreenVideoOptimization)) {
    // This test case is implemented in
    // `OverlayProcessorWinFullScreenWithAdjustmentTest`.
    GTEST_SKIP() << "EarlyFullScreenVideoOptimization handles adjustment of "
                    "overlay position during overlay processing.";
  }

  // Define 1920x1080 monitor size.
  const gfx::Size monitor_size(1920, 1080);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 1920*1080 texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(1920, 1080);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Simulate a half pixel translation in the DCLayerParams
  const gfx::Rect quad_rect = gfx::Rect(0, 0, 1920, 1080);
  const gfx::Rect clip_rect = quad_rect;
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1, gfx::Vector2dF(0.5, 0.5)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(dc_layer_params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));

  EXPECT_EQ(1920u, desc.Width);
  EXPECT_EQ(1080u, desc.Height);

  Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->QueryInterface(IID_PPV_ARGS(&decode_swap_chain)));
  // The dest size has been set to monitor size.
  uint32_t dest_width, dest_height;
  EXPECT_HRESULT_SUCCEEDED(
      decode_swap_chain->GetDestSize(&dest_width, &dest_height));

  EXPECT_EQ(1920u, dest_width);
  EXPECT_EQ(1080u, dest_height);

  // The target rect has been set to the onscreen content rect.
  RECT target_rect;
  EXPECT_HRESULT_SUCCEEDED(decode_swap_chain->GetTargetRect(&target_rect));

  EXPECT_EQ(clip_rect, gfx::Rect(target_rect));

  // Ensure translation was removed.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &visual_transform, &visual_offset,
      &visual_clip_rect);
  DVLOG(1) << "visual_transform" << visual_transform.ToString();

  EXPECT_TRUE(visual_transform.IsIdentity());
}

// This test attempts to emulate the behavior of
// https://codepen.io/OpherV/pen/vYxxbMQ The test site has a 2560x1440 video
// which is scaled to 200% width & 200% height, which should result in just the
// upper left portion of the frame being shown. When in full screen on a
// 1920x1080 monitor the video at 200% scaling should have a swap chain size of
// 3840 x 2160 but the clipping rect should match the monitor size of 1920x1080.
TEST_P(DCompPresenterFullscreenRoundingTest, FullScreenContentWithClipping) {
  // Define 1920x1080 monitor size.
  const gfx::Size monitor_size(1920, 1080);
  SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  SetDirectCompositionMonitorInfoForTesting(1, monitor_size);
  EXPECT_TRUE(presenter_->Resize(monitor_size, 1.0, gfx::ColorSpace(), true));

  // Schedule the overlay for root surface.
  InitializeRootAndScheduleRootSurface(monitor_size, SkColors::kBlack);

  // Make a 2560*1440 texture as display input.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      GetDirectCompositionD3D11Device();
  const gfx::Size texture_size(2560, 1440);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture =
      CreateNV12Texture(d3d11_device, texture_size);
  ASSERT_NE(texture, nullptr);

  // Simulate a scaled up visual that will be clipped in half
  const gfx::Rect quad_rect = gfx::Rect(0, 0, 2560, 1440);
  const gfx::Rect clip_rect = gfx::Rect(0, 0, 1920, 1080);
  const gfx::Transform quad_to_root_transform(
      gfx::AxisTransform2d(1.5, gfx::Vector2dF(0, 0)));
  {
    auto dc_layer_params =
        CreateParamsFromImage(DCLayerOverlayImage(texture_size, texture));
    dc_layer_params.quad_rect = quad_rect;
    dc_layer_params.transform = quad_to_root_transform;
    dc_layer_params.clip_rect = clip_rect;
    dc_layer_params.video_params.color_space = gfx::ColorSpace::CreateREC709();
    dc_layer_params.z_order = 1;
    dc_layer_params.layer_id = gfx::OverlayLayerId::MakeForTesting(0);
    ScheduleOverlay(std::move(dc_layer_params));

    ASSERT_EQ(PresentAndGetSwapResult(), gfx::SwapResult::SWAP_ACK);
  }

  // Swap chain size is set to onscreen content size.
  DXGI_SWAP_CHAIN_DESC1 desc;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain =
      presenter_->GetLayerSwapChainForTesting(
          gfx::OverlayLayerId::MakeForTesting(0));
  ASSERT_TRUE(swap_chain);
  EXPECT_HRESULT_SUCCEEDED(swap_chain->GetDesc1(&desc));
  EXPECT_EQ(3840u, desc.Width);
  EXPECT_EQ(2160u, desc.Height);

  Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain;
  EXPECT_HRESULT_SUCCEEDED(
      swap_chain->QueryInterface(IID_PPV_ARGS(&decode_swap_chain)));
  // The dest size has been set to monitor size.
  uint32_t dest_width, dest_height;
  EXPECT_HRESULT_SUCCEEDED(
      decode_swap_chain->GetDestSize(&dest_width, &dest_height));
  EXPECT_EQ(3840u, dest_width);
  EXPECT_EQ(2160u, dest_height);

  // The target rect has been set to the onscreen content rect.
  RECT target_rect;
  EXPECT_HRESULT_SUCCEEDED(decode_swap_chain->GetTargetRect(&target_rect));
  EXPECT_EQ(gfx::Rect(target_rect), gfx::Rect(3840, 2160));

  // Ensure translation was removed.
  gfx::Transform visual_transform;
  gfx::Point visual_offset;
  gfx::Rect visual_clip_rect;
  presenter_->GetSwapChainVisualInfoForTesting(
      gfx::OverlayLayerId::MakeForTesting(0), &visual_transform, &visual_offset,
      &visual_clip_rect);
  DVLOG(1) << "visual_transform" << visual_transform.ToString();
  EXPECT_EQ(visual_transform.To2dTranslation(), gfx::Vector2dF());
  EXPECT_EQ(clip_rect, visual_clip_rect);
}

INSTANTIATE_TEST_SUITE_P(,
                         DCompPresenterFullscreenRoundingTest,
                         DCompPresenterFullscreenRoundingTest::GetValues(),
                         &DCompPresenterFullscreenRoundingTest::GetParamName);

}  // namespace gl
