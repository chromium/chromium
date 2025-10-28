// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_layer_tree.h"

#include <d3d11_1.h>

#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/overlay_layer_id.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/swap_chain_presenter.h"

namespace gl {
namespace {

constexpr size_t kVideoProcessorDimensionsWindowSize = 100;

bool NeedSwapChainPresenter(const DCLayerOverlayParams& overlay) {
  return overlay.overlay_image && overlay.overlay_image->type() !=
                                      DCLayerOverlayType::kDCompVisualContent;
}

// Unconditionally get a IDCompositionVisual2 as a IDCompositionVisual3.
//
// |IDCompositionVisual3| should be available since Windows 8.1, but we noticed
// crashes due to unconditionally casting to the interface on the earliest
// versions of Windows 10. This should only be used for features that are
// conditionally run above those versions of Windows.
//
// See: https://crbug.com/1455666
Microsoft::WRL::ComPtr<IDCompositionVisual3> CheckedCastToVisual3(
    const Microsoft::WRL::ComPtr<IDCompositionVisual2>& visual2) {
  Microsoft::WRL::ComPtr<IDCompositionVisual3> visual3;
  HRESULT hr = visual2.As(&visual3);
  CHECK_EQ(hr, S_OK);
  CHECK(visual3);
  return visual3;
}

D2D_MATRIX_3X2_F TransformToD2D_MATRIX_3X2_F(const gfx::Transform& transform) {
  DCHECK(transform.Is2dTransform());
  // See |TransformToD2D_MATRIX_4X4_F| for notes.
  return D2D1::Matrix3x2F(transform.rc(0, 0), transform.rc(1, 0),
                          transform.rc(0, 1), transform.rc(1, 1),
                          transform.rc(0, 3), transform.rc(1, 3));
}

D2D_MATRIX_4X4_F TransformToD2D_MATRIX_4X4_F(const gfx::Transform& transform) {
  // D2D matrices are stored with the translation portion in the last row,
  // whereas Skia matrices are stored with the translation in the last column.
  // We need to transpose the matrix during the conversion to account for this
  // difference.
  const gfx::Transform& t = transform;
  return D2D1::Matrix4x4F(t.rc(0, 0), t.rc(1, 0), t.rc(2, 0), t.rc(3, 0),
                          t.rc(0, 1), t.rc(1, 1), t.rc(2, 1), t.rc(3, 1),
                          t.rc(0, 2), t.rc(1, 2), t.rc(2, 2), t.rc(3, 2),
                          t.rc(0, 3), t.rc(1, 3), t.rc(2, 3), t.rc(3, 3));
}

// The size the surfaces in the pool. Used in |VisualSubtree::Update| to
// determine how to scale the background color visual. This can be any size
// since we need a non-empty surface to display the background fill, so 1x1
// is fine.
constexpr gfx::Size kSolidColorSurfaceSize = gfx::Size(1, 1);

#if DCHECK_IS_ON()
bool VisualTreeValid(
    std::vector<std::optional<size_t>>& subtree_index_to_overlay,
    const std::vector<bool>& prev_subtree_is_attached_to_root) {
  for (size_t i = 0; i < subtree_index_to_overlay.size(); i++) {
    // Unused subtrees must be removed from the root.
    if (!subtree_index_to_overlay[i] && prev_subtree_is_attached_to_root[i]) {
      return false;
    }
  }
  return true;
}
#endif  // DCHECK_IS_ON()
}  // namespace

VideoProcessorWrapper::VideoProcessorWrapper() = default;
VideoProcessorWrapper::~VideoProcessorWrapper() = default;

VideoProcessorWrapper::SizeSmoother::SizeSmoother()
    : width_(kVideoProcessorDimensionsWindowSize),
      height_(kVideoProcessorDimensionsWindowSize) {}
VideoProcessorWrapper::SizeSmoother::~SizeSmoother() = default;

void VideoProcessorWrapper::SizeSmoother::SizeSmoother::PutSize(
    gfx::Size size) {
  width_.AddSample(size.width());
  height_.AddSample(size.height());
}

gfx::Size VideoProcessorWrapper::SizeSmoother::GetSize() const {
  return gfx::Size(width_.Max(), height_.Max());
}

// Owns a |IDCompositionSurface| filled with a solid color.
class SolidColorSurface final {
 public:
  SolidColorSurface() = delete;
  SolidColorSurface(SolidColorSurface&&) = default;
  SolidColorSurface& operator=(SolidColorSurface&&) = default;
  ~SolidColorSurface() = default;

  IDCompositionSurface* surface() const { return surface_.Get(); }

 private:
  friend class SolidColorSurfacePool;

  explicit SolidColorSurface(
      Microsoft::WRL::ComPtr<IDCompositionSurface> surface)
      : surface_(std::move(surface)) {
    CHECK(surface_);
  }

  // Fill the surface with the opaque part of |color|.
  base::expected<void, CommitError> FillColor(ID3D11Device* d3d11_device,
                                              SkColor4f color) {
    HRESULT hr = S_OK;
    RECT update_rect = D2D1::Rect(0, 0, kSolidColorSurfaceSize.width(),
                                  kSolidColorSurfaceSize.height());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture;
    POINT update_offset;
    hr = surface_->BeginDraw(&update_rect, IID_PPV_ARGS(&draw_texture),
                             &update_offset);
    if (FAILED(hr)) {
      LOG(ERROR) << "BeginDraw failed: "
                 << logging::SystemErrorCodeToString(hr);
      return base::unexpected(
          CommitError{CommitError::Reason::kSolidColorSurfaceBeginDraw, hr});
    }

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    hr =
        d3d11_device->CreateRenderTargetView(draw_texture.Get(), nullptr, &rtv);
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateRenderTargetView failed: "
                 << logging::SystemErrorCodeToString(hr);
      return base::unexpected(CommitError{
          CommitError::Reason::kSolidColorSurfaceCreateRenderTargetView, hr});
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
    d3d11_device->GetImmediateContext(&immediate_context);
    immediate_context->ClearRenderTargetView(rtv.Get(),
                                             color.makeOpaque().vec());

    hr = surface_->EndDraw();
    if (FAILED(hr)) {
      LOG(ERROR) << "EndDraw failed: " << logging::SystemErrorCodeToString(hr);
      return base::unexpected(
          CommitError{CommitError::Reason::kSolidColorSurfaceEndDraw, hr});
    }

    color_ = color;

    return base::ok();
  }

  // A surface with |DXGI_ALPHA_MODE_IGNORE|, filled with the opaque parts of
  // |color_|.
  Microsoft::WRL::ComPtr<IDCompositionSurface> surface_;

  // Only set if |surface_| was successfully filled to this color.
  std::optional<SkColor4f> color_;
};

SolidColorSurfacePool::SolidColorSurfacePool(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device)
    : d3d11_device_(std::move(d3d11_device)),
      dcomp_device_(std::move(dcomp_device)) {
  CHECK(d3d11_device_);
  CHECK(dcomp_device_);
}
SolidColorSurfacePool::~SolidColorSurfacePool() = default;

base::expected<IDCompositionSurface*, CommitError>
SolidColorSurfacePool::GetSolidColorSurface(const SkColor4f& color) {
  stats_since_last_trim_.num_surfaces_requested += 1;

  HRESULT hr = S_OK;

  auto first_unused_surface_it =
      std::next(tracked_surfaces_.begin(), num_used_this_frame_);

  if (auto found_color_it = std::ranges::find(tracked_surfaces_, color,
                                              &SolidColorSurface::color_);
      found_color_it != tracked_surfaces_.end()) {
    // We found an existing surface in the pool that already has the requested
    // color.

    if (found_color_it >= first_unused_surface_it) {
      // If the surface is in the "unused" portion of |tracked_surfaces_|, make
      // it be tracked now.
      std::swap(*first_unused_surface_it, *found_color_it);
      found_color_it = first_unused_surface_it;
      num_used_this_frame_++;
    } else {
      // The surface is already used by another overlay in this frame, so we can
      // just share it with no extra work.
    }

    return found_color_it->surface();
  }

  // There is no surface that already contains the requested |color|, so we'll
  // need to fill one.
  auto surface_to_fill_it = first_unused_surface_it;
  if (surface_to_fill_it == tracked_surfaces_.end()) {
    // If there are no existing allocations, we'll need to create a new one.
    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
    hr = dcomp_device_->CreateSurface(
        kSolidColorSurfaceSize.width(), kSolidColorSurfaceSize.height(),
        gfx::ColorSpaceWin::GetDXGIFormat(gfx::ColorSpace::CreateSRGB()),
        DXGI_ALPHA_MODE_IGNORE, &dcomp_surface);
    if (FAILED(hr)) {
      LOG(ERROR) << "CreateSurface failed: "
                 << logging::SystemErrorCodeToString(hr);
      return base::unexpected(CommitError{
          CommitError::Reason::kSolidColorSurfacePoolCreateSurface, hr});
    }

    surface_to_fill_it = tracked_surfaces_.insert(
        first_unused_surface_it, SolidColorSurface(std::move(dcomp_surface)));
  }

  // The surface we want to use doesn't have the right color at this point.
  RETURN_IF_ERROR(surface_to_fill_it->FillColor(d3d11_device_.Get(), color));

  // Update the partitioning index after |FillColor| succeeds. In the case of
  // failure, |tracked_surfaces_[num_used_this_frame_]| will still have a valid
  // surface, just not filled to any color yet.
  num_used_this_frame_++;

  stats_since_last_trim_.num_surfaces_recolored += 1;

  return surface_to_fill_it->surface();
}

void SolidColorSurfacePool::TrimAfterCommit() {
  // The is the maximum number of solid color surfaces (both in use and not in
  // use) that we will retain between frames. If we are actively using more than
  // this, this value will be ignored.
  //
  // The value is copied from gbm_surfaceless_wayland.cc's
  // |kMaxSolidColorBuffers|, which picks this value based on observationally
  // seeing max 9 in-flight buffers + some margin. However, this can be any
  // value. If the value is smaller than the number of overlays commonly seen
  // in a frame, we may thrash on allocations. If the value is too large, we
  // will end up wasting space.
  static constexpr size_t kMaxSolidColorSurfacesToRetain = 12;

  // Preserve up to |kMaxSolidColorSurfacesToRetain| surfaces, even if they
  // aren't used this frame.
  size_t trim_target_size =
      std::max(num_used_this_frame_, kMaxSolidColorSurfacesToRetain);
  // Protect against the case where there are fewer tracked surfaces than
  // |kMaxSolidColorSurfacesToRetain|.
  trim_target_size = std::min(trim_target_size, tracked_surfaces_.size());

  DVLOG(3) << "SolidColorSurfacePool stats before trim: " << "requested="
           << stats_since_last_trim_.num_surfaces_requested << ", "
           << "recolored=" << stats_since_last_trim_.num_surfaces_recolored
           << ", " << "in-use/total=" << num_used_this_frame_ << "/"
           << tracked_surfaces_.size()
           << (num_used_this_frame_ > kMaxSolidColorSurfacesToRetain
                   ? " (in-use exceeds kMaxSolidColorSurfacesToRetain)"
                   : "")
           << ", will trim to " << trim_target_size;

  auto first_surface_to_remove =
      std::next(tracked_surfaces_.begin(), trim_target_size);
  tracked_surfaces_.erase(first_surface_to_remove, tracked_surfaces_.end());

  // Reset for the next frame.
  num_used_this_frame_ = 0;
  stats_since_last_trim_ = {};
}

size_t SolidColorSurfacePool::GetNumSurfacesInPoolForTesting() const {
  CHECK_IS_TEST();
  return tracked_surfaces_.size();
}

DCLayerTree::DCLayerTree(bool disable_nv12_dynamic_textures,
                         bool disable_vp_auto_hdr,
                         bool disable_vp_scaling,
                         bool disable_vp_super_resolution,
                         bool disable_dc_letterbox_video_optimization,
                         bool force_dcomp_triple_buffer_video_swap_chain,
                         bool no_downscaled_overlay_promotion)
    : disable_nv12_dynamic_textures_(disable_nv12_dynamic_textures),
      disable_vp_auto_hdr_(disable_vp_auto_hdr),
      disable_vp_scaling_(disable_vp_scaling),
      disable_vp_super_resolution_(disable_vp_super_resolution),
      disable_dc_letterbox_video_optimization_(
          disable_dc_letterbox_video_optimization),
      force_dcomp_triple_buffer_video_swap_chain_(
          force_dcomp_triple_buffer_video_swap_chain),
      no_downscaled_overlay_promotion_(no_downscaled_overlay_promotion),
      tint_video_layer_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTintDcLayer)),
      ink_renderer_(std::make_unique<DelegatedInkRenderer>()) {}

DCLayerTree::~DCLayerTree() = default;

void DCLayerTree::Initialize(
    HWND window,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  window_ = window;
  DCHECK(window_);

  d3d11_device_ = std::move(d3d11_device);
  DCHECK(d3d11_device_);

  dcomp_device_ = GetDirectCompositionDevice();
  DCHECK(dcomp_device_);

  solid_color_surface_pool_ =
      std::make_unique<SolidColorSurfacePool>(d3d11_device_, dcomp_device_);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.As(&desktop_device);
  DCHECK(desktop_device);

  HRESULT hr =
      desktop_device->CreateTargetForHwnd(window_, TRUE, &dcomp_target_);
  // |CreateTargetForHwnd| can fail if |window_| belongs to a different process
  // (DCOMPOSITION_ERROR_ACCESS_DENIED) or we have already called
  // |CreateTargetForHwnd| for this window
  // (DCOMPOSITION_ERROR_WINDOW_ALREADY_COMPOSED). We don't expect either to be
  // the case here.
  CHECK_EQ(hr, S_OK);

  hr = dcomp_device_->CreateVisual(&dcomp_root_visual_);
  CHECK_EQ(hr, S_OK);

  if (base::FeatureList::IsEnabled(features::kDCompDebugVisualization)) {
    Microsoft::WRL::ComPtr<IDCompositionDeviceDebug> debug_device;
    hr = dcomp_device_.As(&debug_device);
    CHECK_EQ(hr, S_OK);
    CHECK(debug_device);
    DLOG(WARNING) << "DComp debug counters enabled, visible in the top right.";
    DLOG(WARNING) << "  - left: The composition engine FPS, averaged over the "
                     "last 60 composition frames";
    DLOG(WARNING) << "  - right: The overall CPU usage of the composition "
                     "thread, in milliseconds";
    hr = debug_device->EnableDebugCounters();
    CHECK_EQ(hr, S_OK);

    Microsoft::WRL::ComPtr<IDCompositionVisualDebug> debug_visual;
    hr = dcomp_root_visual_.As(&debug_visual);
    CHECK_EQ(hr, S_OK);
    CHECK(debug_visual);
    hr = debug_visual->EnableRedrawRegions();
    CHECK_EQ(hr, S_OK);
  }

  dcomp_target_->SetRoot(dcomp_root_visual_.Get());
  // A visual inherits the interpolation mode of the parent visual by default.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  // Set the interpolation mode to Linear to get a better upscaling quality.
  dcomp_root_visual_->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

  hdr_metadata_helper_ = std::make_unique<HDRMetadataHelperWin>(d3d11_device_);

  if (Microsoft::WRL::ComPtr<IDCompositionDevice5> dcomp_device5;
      SUCCEEDED(dcomp_device_.As(&dcomp_device5))) {
    hr = dcomp_device5->CreateDynamicTexture(&primary_plane_surface_);
    if (FAILED(hr)) {
      LOG(WARNING) << "Failed to create IDCompositionDynamicTexture: "
                   << logging::SystemErrorCodeToString(hr);
    }
  }
}

VideoProcessorWrapper* DCLayerTree::InitializeVideoProcessor(
    const gfx::Size& input_size,
    const gfx::Size& output_size,
    bool is_hdr_output,
    bool& video_processor_recreated) {
  video_processor_recreated = false;
  auto& video_processor_wrapper = is_hdr_output ? video_processor_wrapper_hdr_
                                                : video_processor_wrapper_sdr_;
  if (!video_processor_wrapper.video_device) {
    // This can fail if the D3D device is "Microsoft Basic Display Adapter".
    if (FAILED(d3d11_device_.As(&video_processor_wrapper.video_device))) {
      DLOG(ERROR) << "Failed to retrieve video device from D3D11 device";
      DCHECK(false);
      DisableDirectCompositionOverlays();
      return nullptr;
    }
    DCHECK(video_processor_wrapper.video_device);

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device_->GetImmediateContext(&context);
    DCHECK(context);
    context.As(&video_processor_wrapper.video_context);
    DCHECK(video_processor_wrapper.video_context);
  }

  // Calculate input and output size to be maximum in a sliding window.
  video_processor_wrapper.input_size_smoother.PutSize(input_size);
  video_processor_wrapper.output_size_smoother.PutSize(output_size);

  gfx::Size effective_input_size =
      video_processor_wrapper.input_size_smoother.GetSize();
  gfx::Size effective_output_size =
      video_processor_wrapper.output_size_smoother.GetSize();

  // Reuse existing video processor only if it has exactly the computed size.
  // Even if it may have bigger dimensions and may be reusable for requested
  // sizes we will recreate it to reduce resource usage. Sliding window max
  // above guarantees that this reduction will only happen after prolonged usage
  // with smaller texture sizes.
  if (video_processor_wrapper.video_processor &&
      video_processor_wrapper.video_input_size == effective_input_size &&
      video_processor_wrapper.video_output_size == effective_output_size) {
    return &video_processor_wrapper;
  }

  TRACE_EVENT2("gpu", "DCLayerTree::InitializeVideoProcessor", "input_size",
               input_size.ToString(), "output_size", output_size.ToString());

  video_processor_wrapper.video_input_size = effective_input_size;
  video_processor_wrapper.video_output_size = effective_output_size;
  video_processor_wrapper.video_processor.Reset();
  video_processor_wrapper.video_processor_enumerator.Reset();
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = input_size.width();
  desc.InputHeight = input_size.height();
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = output_size.width();
  desc.OutputHeight = output_size.height();
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
  HRESULT hr =
      video_processor_wrapper.video_device->CreateVideoProcessorEnumerator(
          &desc, &video_processor_wrapper.video_processor_enumerator);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessorEnumerator failed with error 0x"
                << std::hex << hr;
    // It might fail again next time. Disable overlay support so
    // overlay processor will stop sending down overlay frames.
    DisableDirectCompositionOverlays();
    return nullptr;
  }
  hr = video_processor_wrapper.video_device->CreateVideoProcessor(
      video_processor_wrapper.video_processor_enumerator.Get(), 0,
      &video_processor_wrapper.video_processor);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessor failed with error 0x" << std::hex
                << hr;
    // It might fail again next time. Disable overlay support so
    // overlay processor will stop sending down overlay frames.
    DisableDirectCompositionOverlays();
    return nullptr;
  }
  // Auto stream processing (the default) can hurt power consumption.
  video_processor_wrapper.video_context
      ->VideoProcessorSetStreamAutoProcessingMode(
          video_processor_wrapper.video_processor.Get(), 0, FALSE);

  video_processor_recreated = true;
  return &video_processor_wrapper;
}

IDXGISwapChain1* DCLayerTree::GetLayerSwapChainForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  if (video_swap_chains_.contains(layer_id)) {
    return video_swap_chains_.at(layer_id)->swap_chain().Get();
  }
  return nullptr;
}

DCLayerTree::VisualTree::VisualSubtree*
DCLayerTree::GetFrontMostVideoVisualSubtreeForTesting() const {
  CHECK_IS_TEST();
  VisualTree::VisualSubtree* front_sub_tree =
      visual_tree_->GetFrontMostVisualSubtreeForTesting();  // IN-TEST
  // `dcomp_visual_content` on front-most subtree should match
  // SwapChainPresenter::content() in `video_swap_chains`
  for (const auto& video_swap_chain : video_swap_chains_) {
    const auto& swap_chain_presenter = video_swap_chain.second;
    if (swap_chain_presenter->content_for_testing().Get() ==  // IN-TEST
        front_sub_tree->dcomp_visual_content()) {
      return front_sub_tree;
    }
  }

  return nullptr;
}

DCLayerTree::VisualTree::VisualSubtree::VisualSubtree() = default;
DCLayerTree::VisualTree::VisualSubtree::~VisualSubtree() {
  if (content_visual_) {
    // Explicitly null out the `content_visual_`'s content to ensure there are
    // no unexpected references to e.g. `IDCompositionTexture`, in case there
    // are lingering references to `content_visual_`.
    HRESULT hr = content_visual_->SetContent(nullptr);
    CHECK_EQ(S_OK, hr);
  }
}

bool DCLayerTree::VisualTree::VisualSubtree::Update(
    IDCompositionDevice3* dcomp_device,
    Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
    uint64_t dcomp_surface_serial,
    const gfx::Size& image_size,
    const gfx::RectF& content_rect,
    Microsoft::WRL::ComPtr<IDCompositionSurface> background_color_surface,
    const SkColor4f& background_color,
    const gfx::Rect& quad_rect,
    bool nearest_neighbor_filter,
    const gfx::Transform& quad_to_root_transform,
    const gfx::RRectF& rounded_corner_bounds,
    float opacity,
    const std::optional<gfx::Rect>& clip_rect_in_root,
    bool allow_antialiasing) {
  bool needs_commit = false;

  // Helper function to set |field| to |parameter| and return whether it
  // changed.
  auto SetField = [&needs_commit](auto& field, auto& parameter) -> bool {
    const bool changed = field != parameter;
    if (changed) {
      field = std::move(parameter);

      // We assume that any change to the input of |Update| will result in some
      // visual property change that requires a commit. If this is not true, an
      // input is not needed.
      needs_commit = true;
    }
    return changed;
  };

  // Fields on |VisualSubtree| should map 1:1 with parameters to |Update| (with
  // the exception of the DComp device pointer, DComp visuals, and Z-order). To
  // avoid issues with incremental computation, set fields to input parameters
  // here with the helper function and read the member fields below only if
  // guarded by the corresponding |*_changed| variable.
  const bool dcomp_visual_content_changed =
      SetField(dcomp_visual_content_, dcomp_visual_content);
  const bool dcomp_surface_serial_changed =
      SetField(dcomp_surface_serial_, dcomp_surface_serial);
  const bool image_size_changed = SetField(image_size_, image_size);
  const bool content_rect_changed = SetField(content_rect_, content_rect);
  const bool background_color_surface_changed =
      SetField(background_color_surface_, background_color_surface);
  const bool background_color_changed =
      SetField(background_color_, background_color);
  const bool quad_rect_changed = SetField(quad_rect_, quad_rect);
  const bool nearest_neighbor_filter_changed =
      SetField(nearest_neighbor_filter_, nearest_neighbor_filter);
  const bool quad_to_root_transform_changed =
      SetField(quad_to_root_transform_, quad_to_root_transform);
  const bool rounded_corner_bounds_changed =
      SetField(rounded_corner_bounds_, rounded_corner_bounds);
  const bool opacity_changed = SetField(opacity_, opacity);
  const bool clip_rect_in_root_changed =
      SetField(clip_rect_in_root_, clip_rect_in_root);
  const bool allow_antialiasing_changed =
      SetField(allow_antialiasing_, allow_antialiasing);

  // Methods that update the visual tree can only fail with OOM. We'll assert
  // success in this function to aid in debugging.
  HRESULT hr = S_OK;

  // All the visual are created together on the first |Update|.
  if (!clip_visual_) {
    needs_commit = true;

    CHECK(!rounded_corners_visual_);
    CHECK(!transform_visual_);
    CHECK(!background_color_visual_);
    CHECK(!content_visual_);

    hr = dcomp_device->CreateVisual(&clip_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&rounded_corners_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&transform_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&background_color_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&content_visual_);
    CHECK_EQ(hr, S_OK);

    hr = clip_visual_->AddVisual(rounded_corners_visual_.Get(), FALSE, nullptr);
    CHECK_EQ(hr, S_OK);
    hr = rounded_corners_visual_->AddVisual(transform_visual_.Get(), FALSE,
                                            nullptr);
    CHECK_EQ(hr, S_OK);
    hr = transform_visual_->AddVisual(background_color_visual_.Get(), FALSE,
                                      nullptr);
    CHECK_EQ(hr, S_OK);
    hr = transform_visual_->AddVisual(content_visual_.Get(), FALSE, nullptr);
    CHECK_EQ(hr, S_OK);

    // The default state for the border mode is INHERIT, so we need to force it
    // to HARD.
    hr = transform_visual_->SetBorderMode(DCOMPOSITION_BORDER_MODE_HARD);
    CHECK_EQ(hr, S_OK);
  }

  if (clip_rect_in_root_changed) {
    if (clip_rect_in_root_.has_value()) {
      // DirectComposition clips happen in the pre-transform visual space, while
      // cc/ clips happen post-transform. So the clip needs to go on a separate
      // parent visual that's untransformed.
      const gfx::Rect& clip_rect = clip_rect_in_root_.value();
      hr = clip_visual_->SetClip(D2D1::RectF(
          clip_rect.x(), clip_rect.y(), clip_rect.right(), clip_rect.bottom()));
      CHECK_EQ(hr, S_OK);
    } else {
      hr = clip_visual_->SetClip(nullptr);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (opacity_changed) {
    if (opacity_ != 1) {
      hr = CheckedCastToVisual3(clip_visual_)->SetOpacity(opacity_);
      CHECK_EQ(hr, S_OK);

      // Let all of this subtree's visuals blend as one, instead of
      // individually
      hr = clip_visual_->SetOpacityMode(DCOMPOSITION_OPACITY_MODE_LAYER);
      CHECK_EQ(hr, S_OK);
    } else {
      hr = CheckedCastToVisual3(clip_visual_)->SetOpacity(1.0);
      CHECK_EQ(hr, S_OK);
      hr = clip_visual_->SetOpacityMode(DCOMPOSITION_OPACITY_MODE_MULTIPLY);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (rounded_corner_bounds_changed) {
    if (!rounded_corner_bounds_.IsEmpty()) {
      Microsoft::WRL::ComPtr<IDCompositionRectangleClip> clip;
      hr = dcomp_device->CreateRectangleClip(&clip);
      CHECK_EQ(hr, S_OK);
      CHECK(clip);

      const gfx::RectF rect = rounded_corner_bounds_.rect();
      hr = clip->SetLeft(rect.x());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetRight(rect.right());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetBottom(rect.bottom());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetTop(rect.y());
      CHECK_EQ(hr, S_OK);

      const gfx::Vector2dF top_left = rounded_corner_bounds_.GetCornerRadii(
          gfx::RRectF::Corner::kUpperLeft);
      hr = clip->SetTopLeftRadiusX(top_left.x());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetTopLeftRadiusY(top_left.y());
      CHECK_EQ(hr, S_OK);

      const gfx::Vector2dF top_right = rounded_corner_bounds_.GetCornerRadii(
          gfx::RRectF::Corner::kUpperRight);
      hr = clip->SetTopRightRadiusX(top_right.x());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetTopRightRadiusY(top_right.y());
      CHECK_EQ(hr, S_OK);

      const gfx::Vector2dF bottom_left = rounded_corner_bounds_.GetCornerRadii(
          gfx::RRectF::Corner::kLowerLeft);
      hr = clip->SetBottomLeftRadiusX(bottom_left.x());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetBottomLeftRadiusY(bottom_left.y());
      CHECK_EQ(hr, S_OK);

      const gfx::Vector2dF bottom_right = rounded_corner_bounds_.GetCornerRadii(
          gfx::RRectF::Corner::kLowerRight);
      hr = clip->SetBottomRightRadiusX(bottom_right.x());
      CHECK_EQ(hr, S_OK);
      hr = clip->SetBottomRightRadiusY(bottom_right.y());
      CHECK_EQ(hr, S_OK);

      hr = rounded_corners_visual_->SetClip(clip.Get());
      CHECK_EQ(hr, S_OK);

      // Enable anti-aliasing of the rounded corners.
      hr =
          rounded_corners_visual_->SetBorderMode(DCOMPOSITION_BORDER_MODE_SOFT);
      CHECK_EQ(hr, S_OK);
    } else {
      hr = rounded_corners_visual_->SetClip(nullptr);
      CHECK_EQ(hr, S_OK);
      hr = rounded_corners_visual_->SetBorderMode(
          DCOMPOSITION_BORDER_MODE_INHERIT);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (quad_to_root_transform_changed) {
    if (quad_to_root_transform_.Is2dTransform()) {
      const D2D_MATRIX_3X2_F matrix =
          TransformToD2D_MATRIX_3X2_F(quad_to_root_transform_);
      hr = Microsoft::WRL::ComPtr<IDCompositionVisual>(transform_visual_)
               ->SetTransform(matrix);
      CHECK_EQ(hr, S_OK);
    } else {
      const D2D_MATRIX_4X4_F matrix =
          TransformToD2D_MATRIX_4X4_F(quad_to_root_transform_);
      hr = CheckedCastToVisual3(transform_visual_)->SetTransform(matrix);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (nearest_neighbor_filter_changed) {
    hr = transform_visual_->SetBitmapInterpolationMode(
        nearest_neighbor_filter_
            ? DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
            : DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
    CHECK_EQ(hr, S_OK);
  }

  if (image_size_changed || content_rect_changed || quad_rect_changed) {
    if (content_rect_.Contains(gfx::RectF(image_size_))) {
      // No need to set clip to content if the whole image is inside the content
      // rect region.
      hr = content_visual_->SetClip(nullptr);
      CHECK_EQ(hr, S_OK);
    } else {
      // Exclude content outside the content rect region.
      const auto content_clip =
          D2D1::RectF(content_rect_.x(), content_rect_.y(),
                      content_rect_.right(), content_rect_.bottom());
      hr = content_visual_->SetClip(content_clip);
      CHECK_EQ(hr, S_OK);
    }

    // Transform the (clipped) content so that it fills |quad_rect_|'s bounds.
    // |quad_rect_|'s offset is handled below, so we exclude it from the matrix.
    const bool needs_offset = !content_rect_.OffsetFromOrigin().IsZero();
    const bool needs_scale =
        static_cast<float>(quad_rect_.width()) != content_rect_.width() ||
        static_cast<float>(quad_rect_.height()) != content_rect_.height();
    if (needs_offset || needs_scale) {
      const float scale_x =
          static_cast<float>(quad_rect_.width()) / content_rect_.width();
      const float scale_y =
          static_cast<float>(quad_rect_.height()) / content_rect_.height();
      const D2D_MATRIX_3X2_F matrix =
          D2D1::Matrix3x2F::Translation(-content_rect_.x(),
                                        -content_rect_.y()) *
          D2D1::Matrix3x2F::Scale(scale_x, scale_y);
      hr = Microsoft::WRL::ComPtr<IDCompositionVisual>(content_visual_)
               ->SetTransform(matrix);
      CHECK_EQ(hr, S_OK);
    } else {
      hr = content_visual_->SetTransform(nullptr);
      CHECK_EQ(hr, S_OK);
    }

    // Visual offset is applied after transform so it is affected by the
    // transform, which is consistent with how the compositor maps quad rects to
    // their target space.
    hr = content_visual_->SetOffsetX(quad_rect_.x());
    CHECK_EQ(hr, S_OK);
    hr = content_visual_->SetOffsetY(quad_rect_.y());
    CHECK_EQ(hr, S_OK);
  }

  if (dcomp_visual_content_changed) {
    hr = content_visual_->SetContent(dcomp_visual_content_.Get());
    CHECK_EQ(hr, S_OK);
  }
#if DCHECK_IS_ON()
  dcomp_visual_content_changed_from_previous_frame_ =
      dcomp_visual_content_changed;
#endif

  if (dcomp_surface_serial_changed) {
    // The DComp surface has been drawn to and needs a commit to show its
    // update. No visual changes are needed in this case.
  }

  if (quad_rect_changed || background_color_surface_changed ||
      background_color_changed) {
    if (!background_color_surface_ || background_color.fA == 0.0) {
      // A fully transparent color is the same as no background fill.
      hr = background_color_visual_->SetContent(nullptr);
      CHECK_EQ(hr, S_OK);
    } else {
      const D2D_MATRIX_3X2_F matrix =
          TransformToD2D_MATRIX_3X2_F(gfx::TransformBetweenRects(
              gfx::RectF(kSolidColorSurfaceSize), gfx::RectF(quad_rect_)));
      hr = Microsoft::WRL::ComPtr<IDCompositionVisual>(background_color_visual_)
               ->SetTransform(matrix);
      CHECK_EQ(hr, S_OK);

      hr =
          background_color_visual_->SetContent(background_color_surface_.Get());
      CHECK_EQ(hr, S_OK);

      hr = CheckedCastToVisual3(background_color_visual_)
               ->SetOpacity(background_color.fA);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (quad_to_root_transform_changed || quad_rect_changed ||
      allow_antialiasing_changed) {
    const float kNeedsSoftBorderTolerance = 0.001;
    const bool content_soft_borders =
        allow_antialiasing_ &&
        (!quad_to_root_transform_.Preserves2dAxisAlignment() ||
         !gfx::IsNearestRectWithinDistance(
             quad_to_root_transform_.MapRect(gfx::RectF(quad_rect_)),
             kNeedsSoftBorderTolerance));
    // The border mode of the transform visual is set (instead of the content
    // visual), so this setting can affect both the content and the background
    // color, since both are are children of the transform visual.
    hr = transform_visual_->SetBorderMode(content_soft_borders
                                              ? DCOMPOSITION_BORDER_MODE_SOFT
                                              : DCOMPOSITION_BORDER_MODE_HARD);
    CHECK_EQ(hr, S_OK);
  }

  return needs_commit;
}

void DCLayerTree::VisualTree::VisualSubtree::GetSwapChainVisualInfoForTesting(
    gfx::Transform* out_transform,
    gfx::Point* out_offset,
    gfx::Rect* out_clip_rect) const {
  CHECK_IS_TEST();
  *out_transform = quad_to_root_transform_;
  *out_offset = quad_rect_.origin();
  *out_clip_rect = clip_rect_in_root_.value_or(gfx::Rect());
}

DCLayerTree::VisualTree::VisualTree(DCLayerTree* dc_layer_tree)
    : dc_layer_tree_(dc_layer_tree) {}

DCLayerTree::VisualTree::~VisualTree() = default;

base::expected<void, CommitError> DCLayerTree::VisualTree::BuildTree(
    const std::vector<DCLayerOverlayParams>& overlays) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  CHECK(std::ranges::is_sorted(overlays, {}, &DCLayerOverlayParams::z_order));
#endif

  // Index into the subtree from the previous frame that is being reused in the
  // current frame for the given overlay index.
  // |overlay_index_to_reused_subtree| has an entry for every overlay in the
  // current frame. Each entry indexes into |visual_subtrees_|, which are the
  // subtrees for the previous frame. Initialized with std::nullopt,
  // meaning not reused.
  std::vector<std::optional<size_t>> overlay_index_to_reused_subtree(
      overlays.size(), std::nullopt);

  // Index into the current frame overlay that uses the subtree of the previous
  // frame for the given subtree index. |subtree_index_to_overlay| has an entry
  // for every subtree in the previous frame. Each entry indexes into |overlays|
  // of the current frame. Initialized with std::nullopt, meaning the subtree
  // is not being reused in the current frame.
  std::vector<std::optional<size_t>> subtree_index_to_overlay(
      visual_subtrees_.size(), std::nullopt);

  // |visual_subtrees| will become |visual_subtrees_| of the current frame;
  std::vector<std::unique_ptr<VisualSubtree>> visual_subtrees;
  visual_subtrees.resize(overlays.size());

  decltype(layer_ids_for_testing_) layer_ids_for_testing;
  layer_ids_for_testing.reserve(overlays.size());

  // Populate the map with visual content and assign matching subtrees to the
  // overlays.
  VisualSubtreeMap subtree_map = BuildMapAndAssignMatchingSubtrees(
      overlays, visual_subtrees, overlay_index_to_reused_subtree,
      subtree_index_to_overlay);

  // Assign unused subtrees to the overlays that don't have a match.
  const size_t first_prev_frame_subtree_unused_index =
      ReuseUnmatchedSubtrees(visual_subtrees, overlay_index_to_reused_subtree,
                             subtree_index_to_overlay);

  // Status for each subtree of the previous frame if it's attached to the root.
  // Initialized with true, meaning attached.
  std::vector<bool> prev_subtree_is_attached_to_root(visual_subtrees_.size(),
                                                     true);

  bool needs_commit = DetachUnusedSubtreesFromRoot(
      first_prev_frame_subtree_unused_index, prev_subtree_is_attached_to_root);

  // Remove unused subtrees from the root that need repositioning.
  needs_commit |= DetachReusedSubtreesThatNeedRepositioningFromRoot(
      visual_subtrees, overlay_index_to_reused_subtree,
      subtree_index_to_overlay, prev_subtree_is_attached_to_root);

#if DCHECK_IS_ON()
  VisualTreeValid(subtree_index_to_overlay, prev_subtree_is_attached_to_root);
#endif  // DCHECK_IS_ON()

  IDCompositionVisual2* left_sibling_visual = nullptr;

  base::flat_set<std::optional<gfx::OverlayLayerId::SharedQuadStateLayerId>>
      layers_with_multiple_overlays;
  for (size_t i = 1; i < overlays.size(); i++) {
    const decltype(layers_with_multiple_overlays)::key_type sqs_layer_id =
        overlays[i].layer_id.shared_quad_state_layer_id();
    if (sqs_layer_id == decltype(layers_with_multiple_overlays)::key_type()) {
      // A default layer ID implies no explicit layer, which should be treated
      // as different from every other layer ID, including itself.
      continue;
    }

    if (overlays[i].layer_id == overlays[i - 1].layer_id) {
      // There were at least two contiguous quads in the same layer.
      layers_with_multiple_overlays.emplace(sqs_layer_id);
    }
  }

  size_t num_layers_modified = 0;

  // This loop walks the overlays and builds or updates the visual subtree for
  // each overlay. |left_sibling_visual| is required to properly stack visual
  // subtrees that are detached from the root visual.
  for (unsigned int i = 0; i < overlays.size(); i++) {
    bool subtree_attached_to_root = false;
    if (visual_subtrees[i]) {
      DCHECK(overlay_index_to_reused_subtree[i]);
      subtree_attached_to_root =
          prev_subtree_is_attached_to_root[overlay_index_to_reused_subtree[i]
                                               .value()];
    } else {
      // This overlay does not reuse a subtree from the previous frame.
      // Instantiate a new one.
      visual_subtrees[i] = std::make_unique<VisualSubtree>();
    }

    const uint64_t dcomp_surface_serial =
        overlays[i].overlay_image.has_value()
            ? overlays[i].overlay_image->dcomp_surface_serial()
            : 0;
    const gfx::Size image_size = overlays[i].overlay_image.has_value()
                                     ? overlays[i].overlay_image->size()
                                     : gfx::Size();

    // Only get a background color surface if we have a non-transparent
    // background color.
    IDCompositionSurface* background_color_surface = nullptr;
    if (overlays[i].background_color &&
        overlays[i].background_color->fA != 0.0) {
      // TODO(http://crbug.com/1380822): Refactor to remove early exits. They
      // may leave visual_subtrees_ corrupted.
      ASSIGN_OR_RETURN(
          background_color_surface,
          dc_layer_tree_->solid_color_surface_pool_->GetSolidColorSurface(
              overlays[i].background_color.value()));
    }

    VisualSubtree* visual_subtree = visual_subtrees[i].get();
    visual_subtree->set_z_order(overlays[i].z_order);
    IUnknown* dcomp_visual_content =
        overlays[i].overlay_image
            ? overlays[i].overlay_image->dcomp_visual_content()
            : nullptr;

    // TODO(crbug.com/324460866): We turn off overlay edge antialiasing when
    // there are multiple overlays in the same layer. This is a workaround to
    // avoid seams when there is e.g. a complex transform applied to the layer.
    // This works for partial delegation because we only expect non-trivial
    // transforms in ephemeral (i.e. animation) states. To support arbitrary
    // content in full delegation, we'll need to parent overlays in the same
    // layer under the same transform visual.
    const bool allow_antialiasing = !layers_with_multiple_overlays.contains(
        overlays[i].layer_id.shared_quad_state_layer_id());

    const bool visual_needs_commit = visual_subtrees[i]->Update(
        dc_layer_tree_->dcomp_device_.Get(), dcomp_visual_content,
        dcomp_surface_serial, image_size, overlays[i].content_rect,
        background_color_surface,
        overlays[i].background_color.value_or(SkColors::kTransparent),
        overlays[i].quad_rect, overlays[i].nearest_neighbor_filter,
        overlays[i].transform, overlays[i].rounded_corner_bounds,
        overlays[i].opacity, overlays[i].clip_rect, allow_antialiasing);

    if (!subtree_attached_to_root) {
      HRESULT hr = dc_layer_tree_->dcomp_root_visual_.Get()->AddVisual(
          visual_subtree->container_visual(), TRUE, left_sibling_visual);
      CHECK_EQ(hr, S_OK);
    }
    left_sibling_visual = visual_subtree->container_visual();

    if (visual_needs_commit || !subtree_attached_to_root) {
      num_layers_modified++;
      needs_commit = true;
    }

    layer_ids_for_testing.push_back(overlays[i].layer_id);
  }

  // Update subtree_map_ and visual_subtrees_ with new values.
  subtree_map_ = std::move(subtree_map);
  visual_subtrees_ = std::move(visual_subtrees);
  layer_ids_for_testing_ = std::move(layer_ids_for_testing);

  UMA_HISTOGRAM_COUNTS("GPU.OsCompositor.NumLayersModified",
                       num_layers_modified);

  if (needs_commit) {
    TRACE_EVENT0("gpu", "DCLayerTree::CommitAndClearPendingOverlays::Commit");
    HRESULT hr = dc_layer_tree_->dcomp_device_->Commit();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Commit failed with error 0x" << std::hex << hr;
      return base::unexpected(
          CommitError{CommitError::Reason::kIDCompositionDeviceCommit, hr});
    }
  }
  return base::ok();
}

DCLayerTree::VisualTree::VisualSubtreeMap
DCLayerTree::VisualTree::BuildMapAndAssignMatchingSubtrees(
    const std::vector<DCLayerOverlayParams>& overlays,
    std::vector<std::unique_ptr<VisualSubtree>>& new_visual_subtrees,
    std::vector<std::optional<size_t>>& overlay_index_to_reused_subtree,
    std::vector<std::optional<size_t>>& subtree_index_to_overlay) {
  CHECK_EQ(overlay_index_to_reused_subtree.size(), overlays.size());
  CHECK_EQ(new_visual_subtrees.size(), overlays.size());
  CHECK_EQ(subtree_index_to_overlay.size(), visual_subtrees_.size());

  // Contains {visual content, overlay index} pairs for this frame overlays.
  // This structure has entries for overlays that have visual content.
  // No entry is inserted for the overlays with no visual content.
  std::vector<std::pair<raw_ptr<IUnknown>, size_t>> map_results;
  // For each overlay populate |map_results| with visual content and indices
  // of overlays from this frame and find the matching subtree from the
  // previous frame.
  for (size_t i = 0; i < overlays.size(); i++) {
    if (!overlays[i].overlay_image) {
      continue;
    }
    IUnknown* dcomp_visual_content =
        overlays[i].overlay_image->dcomp_visual_content();
    if (!dcomp_visual_content) {
      continue;
    }
    map_results.emplace_back(dcomp_visual_content, i);

    // Find matching visual content from the previous frame.
    auto it = subtree_map_.find(dcomp_visual_content);
    if (it == subtree_map_.end()) {
      continue;
    }
    size_t matched_index = it->second;
    if (visual_subtrees_[matched_index]) {
      // Assign the matched index to the corresponding overlay.
      overlay_index_to_reused_subtree[i] = matched_index;
      // Assign overlay index to the matched subtree.
      subtree_index_to_overlay[matched_index] = i;
      // Move visual subtree from the old subtrees to new subtrees.
      new_visual_subtrees[i] = std::move(visual_subtrees_[matched_index]);
    }
  }
  // This converts to a flat_map on returning. We're doing this on purpose to
  // go from O(N^2) to O(N*logN) for building the map.
  return map_results;
}

size_t DCLayerTree::VisualTree::ReuseUnmatchedSubtrees(
    std::vector<std::unique_ptr<VisualSubtree>>& new_visual_subtrees,
    std::vector<std::optional<size_t>>& overlay_index_to_reused_subtree,
    std::vector<std::optional<size_t>>& subtree_index_to_overlay) {
  CHECK_EQ(new_visual_subtrees.size(), overlay_index_to_reused_subtree.size());
  CHECK_EQ(subtree_index_to_overlay.size(), visual_subtrees_.size());

  // No further actions are needed if the previous frame is empty.
  if (visual_subtrees_.empty()) {
    return 0;
  }
  // Index into |visual_subtrees_|.
  size_t prev_frame_subtree_index = 0;
  // Assign unused subtrees from previous frames to overlays that don't have
  // a match.
  for (size_t i = 0; i < new_visual_subtrees.size() &&
                     prev_frame_subtree_index < visual_subtrees_.size();
       i++) {
    if (new_visual_subtrees[i]) {
      // Skip overlay that has a match.
      continue;
    }
    // Find next unused subtree and assign it to the overlay at index |i|.
    for (; prev_frame_subtree_index < visual_subtrees_.size();
         prev_frame_subtree_index++) {
      if (!visual_subtrees_[prev_frame_subtree_index]) {
        continue;
      }
      // Assign the found index to the corresponding overlay.
      overlay_index_to_reused_subtree[i] = prev_frame_subtree_index;
      // Assign the overlay index to the found subtree.
      subtree_index_to_overlay[prev_frame_subtree_index] = i;
      // Move visual subtree from the old subtrees to new subtrees.
      new_visual_subtrees[i] =
          std::move(visual_subtrees_[prev_frame_subtree_index]);
      prev_frame_subtree_index++;
      break;
    }
  }
  return prev_frame_subtree_index;
}

bool DCLayerTree::VisualTree::DetachUnusedSubtreesFromRoot(
    size_t first_prev_frame_subtree_unused_index,
    std::vector<bool>& prev_subtree_is_attached_to_root) {
  CHECK_EQ(prev_subtree_is_attached_to_root.size(), visual_subtrees_.size());
  bool needs_commit = false;
  // Detach the remaining unused subtrees from the root.
  for (size_t i = first_prev_frame_subtree_unused_index;
       i < visual_subtrees_.size(); i++) {
    if (!visual_subtrees_[i]) {
      continue;
    }
    DetachSubtreeFromRoot(visual_subtrees_[i].get());
    prev_subtree_is_attached_to_root[i] = false;
    needs_commit = true;
  }
  return needs_commit;
}

bool DCLayerTree::VisualTree::DetachReusedSubtreesThatNeedRepositioningFromRoot(
    const std::vector<std::unique_ptr<VisualSubtree>>& new_visual_subtrees,
    const std::vector<std::optional<size_t>>& overlay_index_to_reused_subtree,
    const std::vector<std::optional<size_t>>& subtree_index_to_overlay,
    std::vector<bool>& prev_subtree_is_attached_to_root) {
  CHECK_EQ(new_visual_subtrees.size(), overlay_index_to_reused_subtree.size());
  CHECK_EQ(subtree_index_to_overlay.size(), visual_subtrees_.size());
  CHECK_EQ(prev_subtree_is_attached_to_root.size(), visual_subtrees_.size());

  // No further actions are needed if the previous frame is empty.
  if (visual_subtrees_.empty()) {
    return false;
  }
  bool needs_commit = false;
  // Index into |visual_subtrees_|.
  size_t prev_frame_subtree_index = 0;
  // This loop walks the overlay indices and detaches from the root any
  // subtrees that need repositioning in the current frame.
  for (size_t i = 0; i < overlay_index_to_reused_subtree.size(); i++) {
    if (!overlay_index_to_reused_subtree[i]) {
      continue;
    }
    size_t reused_subtree_index = overlay_index_to_reused_subtree[i].value();
    DCHECK_EQ(i, subtree_index_to_overlay[reused_subtree_index].value());
    // If the overlay at index |i| has a match, detach from the root any
    // subtrees that appear before the matching subtree and the previous match.
    for (; prev_frame_subtree_index < reused_subtree_index;
         prev_frame_subtree_index++) {
      if (!prev_subtree_is_attached_to_root[prev_frame_subtree_index]) {
        continue;
      }
      VisualSubtree* subtree =
          new_visual_subtrees[subtree_index_to_overlay[prev_frame_subtree_index]
                                  .value()]
              .get();
      DetachSubtreeFromRoot(subtree);
      prev_subtree_is_attached_to_root[prev_frame_subtree_index] = false;
      needs_commit = true;
    }
    if (reused_subtree_index == prev_frame_subtree_index) {
      ++prev_frame_subtree_index;
    }
  }
  return needs_commit;
}

void DCLayerTree::VisualTree::DetachSubtreeFromRoot(VisualSubtree* subtree) {
  HRESULT hr = dc_layer_tree_->dcomp_root_visual_.Get()->RemoveVisual(
      subtree->container_visual());
  CHECK_EQ(hr, S_OK);
}

DCLayerTree::VisualTree::VisualSubtree*
DCLayerTree::VisualTree::GetFrontMostVisualSubtreeForTesting() const {
  CHECK_IS_TEST();
  return visual_subtrees_.back().get();
}

base::expected<void, CommitError> DCLayerTree::CommitAndClearPendingOverlays(
    std::vector<DCLayerOverlayParams> overlays) {
  TRACE_EVENT1("gpu", "DCLayerTree::CommitAndClearPendingOverlays",
               "num_overlays", overlays.size());

  base::ScopedUmaHistogramTimer scoped_timer(
      "GPU.DirectComposition.CommitAndClearPendingOverlaysDuration",
      base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMicrosecondTimes);

  // If delegated ink metadata exists for this frame, attempt to make an overlay
  // so that a visual subtree can be created for a delegated ink visual.
  // TODO(crbug.com/335553727) Consider clearing ink_renderer_ when there's no
  // metadata.
  if (pending_delegated_ink_metadata_) {
    Microsoft::WRL::ComPtr<IDXGISwapChain1> root_swap_chain;
    auto it = std::ranges::find(overlays, 0, &DCLayerOverlayParams::z_order);
    if (it != overlays.end() && (*it).overlay_image) {
      Microsoft::WRL::ComPtr<IUnknown> root_visual_content =
          (*it).overlay_image->dcomp_visual_content();
      CHECK(root_visual_content);
      HRESULT hr = root_visual_content.As(&root_swap_chain);
      if (hr == E_NOINTERFACE) {
        DCHECK_EQ(root_swap_chain, nullptr);
      } else {
        CHECK_EQ(S_OK, hr);
        CHECK_NE(root_swap_chain, nullptr);
      }
    }

    if (auto ink_layer = ink_renderer_->MakeDelegatedInkOverlay(
            dcomp_device_.Get(), root_swap_chain.Get(),
            std::move(pending_delegated_ink_metadata_))) {
      overlays.push_back(std::move(*ink_layer));
    }
  }

  // Move unused video swap chains to `unused_video_swap_chains` for potential
  // reuse (when adjacent frames have a videos that have different layer IDs
  // which can sometimes happen when a video's src changes), then cleanup.
  decltype(video_swap_chains_) unused_video_swap_chains;
  {
    // Move all video swap chains to `unused_video_swap_chains` and the move
    // ones that are in the current frame back into `video_swap_chains_`.
    unused_video_swap_chains = std::move(video_swap_chains_);

    // We may be moving up to all of the swap chains back.
    video_swap_chains_.reserve(unused_video_swap_chains.size());

    size_t num_swap_chain_presenters = 0;
    for (auto& overlay : overlays) {
      if (NeedSwapChainPresenter(overlay)) {
        auto reused_video_swap_chain_it =
            unused_video_swap_chains.find(overlay.layer_id);
        if (reused_video_swap_chain_it != unused_video_swap_chains.end()) {
          video_swap_chains_.insert(std::move(*reused_video_swap_chain_it));
          unused_video_swap_chains.erase(reused_video_swap_chain_it);
        }
        num_swap_chain_presenters++;
      }
    }

    // If there are more videos this frame, reserve enough space for them.
    video_swap_chains_.reserve(num_swap_chain_presenters);
  }

  bool did_update_primary_plane_damage = false;
  bool need_background_layer = false;

  // Populate |overlays| with information required to build dcomp visual tree.
  for (auto it = overlays.begin(); it != overlays.end(); it++) {
    auto& overlay = *it;
    if (NeedSwapChainPresenter(overlay)) {
      // Present to swap chain and update the overlay with transform, clip
      // and content.
      auto& video_swap_chain = video_swap_chains_[overlay.layer_id];
      if (!video_swap_chain) {
        // TODO(sunnyps): Try to find a matching swap chain based on size, type
        // of swap chain, gl image, etc.
        auto unused_video_swap_chain_it = unused_video_swap_chains.begin();
        if (unused_video_swap_chain_it != unused_video_swap_chains.end()) {
          video_swap_chain = std::move(unused_video_swap_chain_it->second);
          unused_video_swap_chains.erase(unused_video_swap_chain_it);
        } else {
          video_swap_chain = std::make_unique<SwapChainPresenter>(
              this, d3d11_device_, dcomp_device_);
        }
      }

      std::optional<SwapChainPresenter::OverlayPositionAdjustment>
          overlay_position_adjustment;
      if (std::optional<DCLayerOverlayImage> video_image =
              video_swap_chain->PresentToSwapChain(
                  overlay, overlay_position_adjustment)) {
        overlay.overlay_image = std::move(video_image);
        overlay.content_rect = gfx::RectF(overlay.overlay_image->size());

        if (overlay_position_adjustment) {
          overlay.transform = overlay_position_adjustment->transform;
          overlay.quad_rect = overlay_position_adjustment->quad_rect;
          if (overlay.clip_rect) {
            overlay.clip_rect = overlay_position_adjustment->clip_rect;
          }
        }

        if (overlay.video_params.is_full_screen_video &&
            !overlay_position_adjustment &&
            base::FeatureList::IsEnabled(
                features::kEarlyFullScreenVideoOptimization)) {
          // If we failed to disable the desktop plane, we need to manually add
          // a solid color layer to act as the video background mat.
          need_background_layer = true;
        }
      } else {
        DLOG(ERROR) << "PresentToSwapChain failed";
        return base::unexpected(
            CommitError{CommitError::Reason::kPresentToSwapChain});
      }

      if (tint_video_layer_) {
        SkColor4f tint_color;
        switch (video_swap_chain->GetLastPresentationMode()) {
          case SwapChainPresenter::PresentationMode::kDecodeSwapChain:
            tint_color = SkColors::kBlue;
            break;
          case SwapChainPresenter::PresentationMode::kVpBlt:
            tint_color = SkColors::kMagenta;
            break;
          case SwapChainPresenter::PresentationMode::kVpBltWithStagingTexture:
            tint_color = SkColor4f(1.0, 0.5, 0.0, 1.0);
            break;
          case SwapChainPresenter::PresentationMode::kMfSurfaceProxy:
            tint_color = SkColors::kGreen;
            break;
        }

        DCLayerOverlayParams tint_overlay;
        tint_overlay.quad_rect = it->quad_rect;
        tint_overlay.transform = it->transform;
        tint_overlay.clip_rect = it->clip_rect;
        tint_overlay.rounded_corner_bounds = it->rounded_corner_bounds;
        tint_overlay.z_order = it->z_order;
        tint_overlay.opacity = 0.25;
        tint_overlay.background_color = tint_color;
        tint_overlay.layer_id =
            it->layer_id.MakeForChildOfSharedQuadStateLayer(1);
        it = overlays.insert(std::next(it), std::move(tint_overlay));
        // Do not access `overlay` after this point since it is invalidated.
      }
    } else if (primary_plane_surface_) {
      // If supported, "present" the primary plane buffer to a surface with
      // incremental damage.
      if (overlay.z_order == 0 && overlay.overlay_image) {
        if (Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture;
            SUCCEEDED(Microsoft::WRL::ComPtr<IUnknown>(
                          overlay.overlay_image->dcomp_visual_content())
                          .As(&dcomp_texture))) {
          DVLOG(1) << "Set primary_plane_surface_ damage: "
                   << overlay.damage_rect.ToString();

          const RECT damage_rect =
              gfx::ToEnclosingRect(overlay.damage_rect).ToRECT();
          HRESULT hr = primary_plane_surface_->SetTexture(dcomp_texture.Get(),
                                                          &damage_rect, 1);
          CHECK_EQ(hr, S_OK);

          overlay.overlay_image = DCLayerOverlayImage(
              overlay.overlay_image->size(), primary_plane_surface_,
              primary_plane_surface_serial_++);
          did_update_primary_plane_damage = true;
        } else {
          // Primary plane is not backed by `BufferQueue`.
        }
      } else {
        // Overlay is not the primary plane.
      }
    }
  }

  if (primary_plane_surface_ && primary_plane_surface_serial_ &&
      !did_update_primary_plane_damage) {
    // We need to commit the visual tree after `SetTexture`. We expect the
    // primary plane overlay to be removed from the visual tree this frame,
    // which will cause commit to happen.
    DVLOG(1) << "Reset primary_plane_surface_ damage.";
    primary_plane_surface_->SetTexture(nullptr);
    primary_plane_surface_serial_ = 0;
  }

  if (need_background_layer) {
    DCLayerOverlayParams background_mat;
    background_mat.quad_rect = gfx::Rect(GetMonitorSizeForWindow(window()));
    background_mat.z_order = INT_MIN;
    background_mat.background_color = SkColors::kBlack;
    background_mat.layer_id = gfx::OverlayLayerId::MakeVizInternal(
        gfx::OverlayLayerId::VizInternalId::kBackgroundColorLayer);
    overlays.insert(overlays.begin(), std::move(background_mat));
  }

  if (!visual_tree_) {
    visual_tree_ = std::make_unique<VisualTree>(this);
  }

  const base::expected<void, CommitError> status =
      visual_tree_->BuildTree(overlays);

  ink_renderer_->ReportPointsDrawn();

  // Clean up excess surfaces so the pool will not grow unbounded.
  solid_color_surface_pool_->TrimAfterCommit();

  return status;
}

bool DCLayerTree::SupportsDelegatedInk() {
  return ink_renderer_->DelegatedInkIsSupported(dcomp_device_);
}

void DCLayerTree::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  DCHECK(SupportsDelegatedInk());
  pending_delegated_ink_metadata_ = std::move(metadata);
}

void DCLayerTree::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  DCHECK(SupportsDelegatedInk());

  ink_renderer_->InitMessagePipeline(std::move(pending_receiver));
}

// Return properties of non root swap chain at given index.
void DCLayerTree::GetSwapChainVisualInfoForTesting(
    const gfx::OverlayLayerId& layer_id,
    gfx::Transform* out_transform,
    gfx::Point* out_offset,
    gfx::Rect* out_clip_rect) const {
  CHECK_IS_TEST();
  visual_tree_->GetSwapChainVisualInfoForTesting(  // IN-TEST
      layer_id, out_transform, out_offset, out_clip_rect);
}

size_t DCLayerTree::GetSwapChainPresenterCountForTesting() const {
  CHECK_IS_TEST();
  return video_swap_chains_.size();
}

size_t DCLayerTree::GetDcompLayerCountForTesting() const {
  CHECK_IS_TEST();
  return visual_tree_->GetDcompLayerCountForTesting();  // IN-TEST
}

IDCompositionVisual2* DCLayerTree::GetContentVisualForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return visual_tree_->GetContentVisualForTesting(layer_id);  // IN-TEST
}

IDCompositionSurface* DCLayerTree::GetBackgroundColorSurfaceForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return visual_tree_->GetBackgroundColorSurfaceForTesting(  // IN-TEST
      layer_id);
}

size_t DCLayerTree::GetNumSurfacesInPoolForTesting() const {
  CHECK_IS_TEST();
  return solid_color_surface_pool_
      ->GetNumSurfacesInPoolForTesting();  // IN-TEST
}

#if DCHECK_IS_ON()
bool DCLayerTree::DcompVisualContentChangedFromPreviousFrameForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return visual_tree_
      ->DcompVisualContentChangedFromPreviousFrameForTesting(  // IN-TEST
          layer_id);
}
#endif  // DCHECK_IS_ON()

const DCLayerTree::VisualTree::VisualSubtree*
DCLayerTree::VisualTree::GetSubtreeFromLayerIdForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  const auto it = std::ranges::find(layer_ids_for_testing_, layer_id);
  CHECK(it != layer_ids_for_testing_.end());
  const size_t index =
      std::ranges::distance(layer_ids_for_testing_.begin(), it);
  return visual_subtrees_.at(index).get();
}

// Return properties of non root swap chain at given index.
void DCLayerTree::VisualTree::GetSwapChainVisualInfoForTesting(
    const gfx::OverlayLayerId& layer_id,
    gfx::Transform* out_transform,
    gfx::Point* out_offset,
    gfx::Rect* out_clip_rect) const {
  CHECK_IS_TEST();
  return GetSubtreeFromLayerIdForTesting(layer_id)                   // IN-TEST
      ->GetSwapChainVisualInfoForTesting(out_transform, out_offset,  // IN-TEST
                                         out_clip_rect);
}

size_t DCLayerTree::VisualTree::GetDcompLayerCountForTesting() const {
  CHECK_IS_TEST();
  return visual_subtrees_.size();
}

IDCompositionVisual2* DCLayerTree::VisualTree::GetContentVisualForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return GetSubtreeFromLayerIdForTesting(layer_id)  // IN-TEST
      ->container_visual();
}

IDCompositionSurface*
DCLayerTree::VisualTree::GetBackgroundColorSurfaceForTesting(
    const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return GetSubtreeFromLayerIdForTesting(layer_id)  // IN-TEST
      ->background_color_surface_for_testing();     // IN-TEST
}

#if DCHECK_IS_ON()
bool DCLayerTree::VisualTree::
    DcompVisualContentChangedFromPreviousFrameForTesting(
        const gfx::OverlayLayerId& layer_id) const {
  CHECK_IS_TEST();
  return GetSubtreeFromLayerIdForTesting(layer_id)               // IN-TEST
      ->DcompVisualContentChangedFromPreviousFrameForTesting();  // IN-TEST
}
#endif  // DCHECK_IS_ON()

}  // namespace gl
