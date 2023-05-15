// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_layer_tree.h"

#include <d3d11_1.h>

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/swap_chain_presenter.h"

namespace gl {
namespace {
bool SizeContains(const gfx::Size& a, const gfx::Size& b) {
  return gfx::Rect(a).Contains(gfx::Rect(b));
}

bool NeedSwapChainPresenter(const DCLayerOverlayParams* overlay) {
  return overlay->overlay_image->type() !=
         DCLayerOverlayType::kDCompVisualContent;
}

// TODO(http://crbug.com/1380822): Implement dcomp visual tree optimization.
BASE_FEATURE(kDCVisualTreeOptimization,
             "DCVisualTreeOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

VideoProcessorWrapper::VideoProcessorWrapper() = default;
VideoProcessorWrapper::~VideoProcessorWrapper() = default;
VideoProcessorWrapper::VideoProcessorWrapper(VideoProcessorWrapper&& other) =
    default;
VideoProcessorWrapper& VideoProcessorWrapper::operator=(
    VideoProcessorWrapper&& other) = default;

DCLayerTree::DCLayerTree(bool disable_nv12_dynamic_textures,
                         bool disable_vp_scaling,
                         bool disable_vp_super_resolution,
                         bool no_downscaled_overlay_promotion)
    : disable_nv12_dynamic_textures_(disable_nv12_dynamic_textures),
      disable_vp_scaling_(disable_vp_scaling),
      disable_vp_super_resolution_(disable_vp_super_resolution),
      no_downscaled_overlay_promotion_(no_downscaled_overlay_promotion),
      ink_renderer_(std::make_unique<DelegatedInkRenderer>()) {}

DCLayerTree::~DCLayerTree() = default;

bool DCLayerTree::Initialize(HWND window) {
  window_ = window;
  DCHECK(window_);

  d3d11_device_ = QueryD3D11DeviceObjectFromANGLE();
  DCHECK(d3d11_device_);

  dcomp_device_ = GetDirectCompositionDevice();
  DCHECK(dcomp_device_);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.As(&desktop_device);
  DCHECK(desktop_device);

  HRESULT hr =
      desktop_device->CreateTargetForHwnd(window_, TRUE, &dcomp_target_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTargetForHwnd failed with error 0x" << std::hex << hr;
    return false;
  }

  dcomp_device_->CreateVisual(&dcomp_root_visual_);
  DCHECK(dcomp_root_visual_);
  dcomp_target_->SetRoot(dcomp_root_visual_.Get());
  // A visual inherits the interpolation mode of the parent visual by default.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  // Set the interpolation mode to Linear to get a better upscaling quality.
  dcomp_root_visual_->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);

  hdr_metadata_helper_ = std::make_unique<HDRMetadataHelperWin>(d3d11_device_);

  return true;
}

VideoProcessorWrapper* DCLayerTree::InitializeVideoProcessor(
    const gfx::Size& input_size,
    const gfx::Size& output_size,
    bool is_hdr_output) {
  VideoProcessorWrapper& video_processor_wrapper =
      GetOrCreateVideoProcessor(is_hdr_output);

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

  if (video_processor_wrapper.video_processor &&
      SizeContains(video_processor_wrapper.video_input_size, input_size) &&
      SizeContains(video_processor_wrapper.video_output_size, output_size))
    return &video_processor_wrapper;

  TRACE_EVENT2("gpu", "DCLayerTree::InitializeVideoProcessor", "input_size",
               input_size.ToString(), "output_size", output_size.ToString());
  video_processor_wrapper.video_input_size = input_size;
  video_processor_wrapper.video_output_size = output_size;

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
  return &video_processor_wrapper;
}

VideoProcessorWrapper& DCLayerTree::GetOrCreateVideoProcessor(bool is_hdr) {
  VideoProcessorType video_processor_type =
      is_hdr ? VideoProcessorType::kHDR : VideoProcessorType::kSDR;
  return video_processor_map_
      .try_emplace(video_processor_type, VideoProcessorWrapper())
      .first->second;
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DCLayerTree::GetLayerSwapChainForTesting(size_t index) const {
  if (index < video_swap_chains_.size())
    return video_swap_chains_[index]->swap_chain();
  return nullptr;
}

// Return properties of non root swap chain at given index.
void DCLayerTree::GetSwapChainVisualInfoForTesting(size_t index,
                                                   gfx::Transform* transform,
                                                   gfx::Point* offset,
                                                   gfx::Rect* clip_rect) const {
  if (visual_tree_) {
    visual_tree_->GetSwapChainVisualInfoForTesting(index, transform,  // IN-TEST
                                                   offset, clip_rect);
  }
}

DCLayerTree::VisualTree::VisualSubtree::VisualSubtree() = default;
DCLayerTree::VisualTree::VisualSubtree::~VisualSubtree() = default;

bool DCLayerTree::VisualTree::VisualSubtree::Update(
    IDCompositionDevice2* dcomp_device,
    Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
    uint64_t dcomp_surface_serial,
    const gfx::Size& image_size,
    const gfx::Rect& content_rect,
    const gfx::Rect& quad_rect,
    bool nearest_neighbor_filter,
    const gfx::Transform& quad_to_root_transform,
    const absl::optional<gfx::Rect>& clip_rect_in_root) {
  bool needs_commit = false;

  // Methods that update the visual tree can only fail with OOM. We'll assert
  // success in this function to aid in debugging.
  HRESULT hr = S_OK;

  if (!clip_visual_) {
    needs_commit = true;

    // All the visual are created together on the first |Update|.
    CHECK(!transform_visual_);
    CHECK(!content_visual_);

    hr = dcomp_device->CreateVisual(&clip_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&transform_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&content_visual_);
    CHECK_EQ(hr, S_OK);
    hr = clip_visual_->AddVisual(transform_visual_.Get(), FALSE, nullptr);
    CHECK_EQ(hr, S_OK);
    hr = transform_visual_->AddVisual(content_visual_.Get(), FALSE, nullptr);
    CHECK_EQ(hr, S_OK);
  }

  if (clip_rect_ != clip_rect_in_root) {
    clip_rect_ = clip_rect_in_root;
    needs_commit = true;

    if (clip_rect_.has_value()) {
      // DirectComposition clips happen in the pre-transform visual space, while
      // cc/ clips happen post-transform. So the clip needs to go on a separate
      // parent visual that's untransformed.
      gfx::Rect clip_rect = clip_rect_.value();
      hr = clip_visual_->SetClip(D2D1::RectF(
          clip_rect.x(), clip_rect.y(), clip_rect.right(), clip_rect.bottom()));
      CHECK_EQ(hr, S_OK);
    } else {
      hr = clip_visual_->SetClip(nullptr);
      CHECK_EQ(hr, S_OK);
    }
  }

  if (transform_ != quad_to_root_transform) {
    transform_ = quad_to_root_transform;
    needs_commit = true;

    DCHECK(transform_.IsFlat());
    D2D_MATRIX_3X2_F matrix =
        // D2D_MATRIX_3x2_F is row-major.
        D2D1::Matrix3x2F(transform_.rc(0, 0), transform_.rc(1, 0),  //
                         transform_.rc(0, 1), transform_.rc(1, 1),  //
                         transform_.rc(0, 3), transform_.rc(1, 3));
    hr = transform_visual_->SetTransform(matrix);
    CHECK_EQ(hr, S_OK);
  }

  if (nearest_neighbor_filter_ != nearest_neighbor_filter) {
    nearest_neighbor_filter_ = nearest_neighbor_filter;
    needs_commit = true;

    hr = transform_visual_->SetBitmapInterpolationMode(
        nearest_neighbor_filter_
            ? DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
            : DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
    CHECK_EQ(hr, S_OK);
  }

  if (image_size_ != image_size || content_rect_ != content_rect ||
      quad_rect_ != quad_rect) {
    image_size_ = image_size;
    content_rect_ = content_rect;
    quad_rect_ = quad_rect;
    needs_commit = true;

    if (content_rect_.Contains(gfx::Rect(image_size_))) {
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
    const bool needs_scale = quad_rect_.width() != content_rect_.width() ||
                             quad_rect_.height() != content_rect_.height();
    if (needs_offset || needs_scale) {
      const float scale_x = static_cast<float>(quad_rect_.width()) /
                            static_cast<float>(content_rect_.width());
      const float scale_y = static_cast<float>(quad_rect_.height()) /
                            static_cast<float>(content_rect_.height());
      const D2D_MATRIX_3X2_F matrix =
          D2D1::Matrix3x2F::Translation(-content_rect_.x(),
                                        -content_rect_.y()) *
          D2D1::Matrix3x2F::Scale(scale_x, scale_y);
      hr = content_visual_->SetTransform(matrix);
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

  if (dcomp_visual_content_ != dcomp_visual_content) {
    dcomp_visual_content_ = std::move(dcomp_visual_content);
    needs_commit = true;
    hr = content_visual_->SetContent(dcomp_visual_content_.Get());
    CHECK_EQ(hr, S_OK);
  }

  if (dcomp_surface_serial_ != dcomp_surface_serial) {
    // If dcomp_surface data is updated needs a commit.
    needs_commit = true;
    dcomp_surface_serial_ = dcomp_surface_serial;
  }
#if DCHECK_IS_ON()
  // dcomp_surface_serial_ is used for root surface only. For other surfaces
  // it's always zero.
  if (dcomp_surface_serial_ > 0)
    DCHECK_EQ(z_order_, 0);
#endif
  return needs_commit;
}

void DCLayerTree::VisualTree::VisualSubtree::GetSwapChainVisualInfoForTesting(
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  *transform = transform_;
  *offset = quad_rect_.origin();
  *clip_rect = clip_rect_.value_or(gfx::Rect());
}

DCLayerTree::VisualTree::VisualTree(DCLayerTree* dc_layer_tree)
    : dc_layer_tree_(dc_layer_tree) {}

DCLayerTree::VisualTree::~VisualTree() = default;

bool DCLayerTree::VisualTree::UpdateTree(
    const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays,
    bool needs_rebuild_visual_tree) {
  // Grow or shrink list of visual subtrees to match pending overlays.
  size_t old_visual_subtrees_size = visual_subtrees_.size();
  if (old_visual_subtrees_size != overlays.size()) {
    needs_rebuild_visual_tree = true;
  }

  // Visual for root surface. Cache it to add DelegatedInk visual if needed.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> root_surface_visual;
  bool needs_commit = false;
  std::vector<std::unique_ptr<VisualSubtree>> visual_subtrees;
  visual_subtrees.resize(overlays.size());
  // Build or update visual subtree for each overlay.
  for (size_t i = 0; i < overlays.size(); ++i) {
    const bool is_root_plane = overlays[i]->z_order == 0;
    if (!is_root_plane && overlays[i]->overlay_image) {
      TRACE_EVENT2(
          "gpu", "DCLayerTree::VisualTree::UpdateOverlay", "image_type",
          DCLayerOverlayTypeToString(overlays[i]->overlay_image->type()),
          "size", overlays[i]->content_rect.size().ToString());
    }

    IUnknown* dcomp_visual_content =
        overlays[i]->overlay_image->dcomp_visual_content();
    // Find matching subtree for each overlay. If subtree is found, move it
    // from visual subtrees of previous frame to visual subtrees of this frame.
    auto it = std::find_if(
        visual_subtrees_.begin(), visual_subtrees_.end(),
        [dcomp_visual_content](const std::unique_ptr<VisualSubtree>& subtree) {
          return subtree &&
                 subtree->dcomp_visual_content() == dcomp_visual_content;
        });
    if (it == visual_subtrees_.end()) {
      // This overlay's visual content does not present in the old visual tree.
      // Instantiate a new visual subtree.
      visual_subtrees[i] = std::make_unique<VisualSubtree>();
      visual_subtrees[i]->set_z_order(overlays[i]->z_order);
      needs_rebuild_visual_tree = true;
    } else {
      // Move visual subtree from the old subtrees to new subtrees.
      visual_subtrees[i] = std::move(*it);
      if (visual_subtrees[i]->z_order() != overlays[i]->z_order) {
        visual_subtrees[i]->set_z_order(overlays[i]->z_order);
        // Z-order is a property of the root visual's child list, not any
        // property on the subtree's nodes. If it changes, we need to rebuild
        // the tree.
        needs_rebuild_visual_tree = true;
      }
    }
    // We don't need to set |needs_rebuild_visual_tree| here since that is only
    // needed when the root visual's children need to be reordered. |Update|
    // only affects the subtree for each child, so only a commit is needed in
    // this case.
    needs_commit |= visual_subtrees[i]->Update(
        dc_layer_tree_->dcomp_device_.Get(),
        overlays[i]->overlay_image->dcomp_visual_content(),
        overlays[i]->overlay_image->dcomp_surface_serial(),
        overlays[i]->overlay_image->size(), overlays[i]->content_rect,
        overlays[i]->quad_rect, overlays[i]->nearest_neighbor_filter,
        overlays[i]->transform, overlays[i]->clip_rect);

    // Zero z_order represents root layer.
    if (overlays[i]->z_order == 0) {
      // Verify we have single root visual layer.
      DCHECK(!root_surface_visual);
      root_surface_visual = visual_subtrees[i]->content_visual();
    }
  }
  // Update visual_subtrees_ with new values.
  visual_subtrees_ = std::move(visual_subtrees);

  // Note: needs_rebuild_visual_tree might be set in this method,
  // |DCLayerTree::CommitAndClearPendingOverlays|, and can also be set in
  // |DCLayerTree::SetDelegatedInkTrailStartPoint| to add a delegated ink visual
  // into the root surface's visual.
  if (needs_rebuild_visual_tree) {
    TRACE_EVENT0(
        "gpu", "DCLayerTree::CommitAndClearPendingOverlays::ReBuildVisualTree");

    // Rebuild root visual's child list.
    dc_layer_tree_->dcomp_root_visual_->RemoveAllVisuals();

    for (size_t i = 0; i < visual_subtrees_.size(); ++i) {
      // We call AddVisual with insertAbove FALSE and referenceVisual nullptr
      // which is equivalent to saying that the visual should be below no
      // other visual, or in other words it should be above all other visuals.
      dc_layer_tree_->dcomp_root_visual_->AddVisual(
          visual_subtrees_[i]->container_visual(), FALSE, nullptr);
    }

    dc_layer_tree_->AddDelegatedInkVisualToTreeIfNeeded(
        root_surface_visual.Get());

    needs_commit = true;
  }

  if (needs_commit) {
    TRACE_EVENT0("gpu", "DCLayerTree::CommitAndClearPendingOverlays::Commit");
    HRESULT hr = dc_layer_tree_->dcomp_device_->Commit();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Commit failed with error 0x" << std::hex << hr;
      return false;
    }
  }
  return true;
}

void DCLayerTree::VisualTree::GetSwapChainVisualInfoForTesting(
    size_t index,
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  for (size_t i = 0, swapchain_i = 0; i < visual_subtrees_.size(); ++i) {
    // Skip root layer.
    if (visual_subtrees_[i]->z_order() == 0) {
      continue;
    }

    if (swapchain_i == index) {
      visual_subtrees_[i]->GetSwapChainVisualInfoForTesting(  // IN-TEST
          transform, offset, clip_rect);
      return;
    }
    swapchain_i++;
  }
}

bool DCLayerTree::CommitAndClearPendingOverlays(
    DirectCompositionChildSurfaceWin* root_surface) {
  TRACE_EVENT1("gpu", "DCLayerTree::CommitAndClearPendingOverlays",
               "num_pending_overlays", pending_overlays_.size());
  DCHECK(!needs_rebuild_visual_tree_ || ink_renderer_->HasBeenInitialized());

  {
    Microsoft::WRL::ComPtr<IDXGISwapChain1> root_swap_chain;
    Microsoft::WRL::ComPtr<IDCompositionSurface> root_dcomp_surface;
    if (root_surface) {
      root_swap_chain = root_surface->swap_chain();
      root_dcomp_surface = root_surface->dcomp_surface();

      Microsoft::WRL::ComPtr<IUnknown> root_visual_content;
      if (root_swap_chain) {
        root_visual_content = root_swap_chain;
      } else {
        root_visual_content = root_dcomp_surface;
      }

      // Add a placeholder overlay for the root surface, at a z-order of 0.
      auto root_params = std::make_unique<DCLayerOverlayParams>();
      root_params->z_order = 0;
      root_params->overlay_image = DCLayerOverlayImage(
          root_surface->GetSize(), std::move(root_visual_content),
          root_surface->dcomp_surface_serial());
      root_params->content_rect = gfx::Rect(root_params->overlay_image->size());
      root_params->quad_rect = gfx::Rect(root_params->overlay_image->size());
      ScheduleDCLayer(std::move(root_params));
    } else {
      auto it = std::find_if(
          pending_overlays_.begin(), pending_overlays_.end(),
          [](const std::unique_ptr<DCLayerOverlayParams>& overlay) {
            return overlay->z_order == 0;
          });
      if (it != pending_overlays_.end()) {
        Microsoft::WRL::ComPtr<IUnknown> root_visual_content =
            (*it)->overlay_image->dcomp_visual_content();
        HRESULT hr = root_visual_content.As(&root_swap_chain);
        if (hr == E_NOINTERFACE) {
          DCHECK_EQ(nullptr, root_swap_chain);
          hr = root_visual_content.As(&root_dcomp_surface);
        }
        CHECK_EQ(S_OK, hr);
      } else {
        // Note: this is allowed in tests, but not expected otherwise.
        DLOG(WARNING) << "No root surface in overlay list";
      }
    }

    if (root_swap_chain != root_swap_chain_ ||
        root_dcomp_surface != root_dcomp_surface_) {
      DCHECK(!(root_swap_chain && root_dcomp_surface));
      root_swap_chain_ = std::move(root_swap_chain);
      root_dcomp_surface_ = std::move(root_dcomp_surface);
      needs_rebuild_visual_tree_ = true;
    }
  }

  std::vector<std::unique_ptr<DCLayerOverlayParams>> overlays;
  std::swap(pending_overlays_, overlays);

  // Grow or shrink list of swap chain presenters to match pending overlays.
  const size_t num_swap_chain_presenters =
      std::count_if(overlays.begin(), overlays.end(), [](const auto& overlay) {
        return NeedSwapChainPresenter(overlay.get());
      });
  // Grow or shrink list of swap chain presenters to match pending overlays.
  if (video_swap_chains_.size() != num_swap_chain_presenters) {
    video_swap_chains_.resize(num_swap_chain_presenters);
    // If we need to grow or shrink swap chain presenters, we'll need to add or
    // remove visuals.
    needs_rebuild_visual_tree_ = true;
  }

  // Sort layers by z-order.
  std::sort(overlays.begin(), overlays.end(),
            [](const auto& a, const auto& b) -> bool {
              return a->z_order < b->z_order;
            });

  // |overlays| and |video_swap_chains_| do not have a 1:1 mapping because the
  // root surface placeholder overlay does not have SwapChainPresenter, so there
  // is one less element in |video_swap_chains_| than |overlays|.
  auto video_swap_iter = video_swap_chains_.begin();

  // Populate |overlays| with information required to build dcomp visual tree.
  for (size_t i = 0; i < overlays.size(); ++i) {
    if (!NeedSwapChainPresenter(overlays[i].get())) {
      continue;
    }
    // Present to swap chain and update the overlay with transform, clip
    // and content.
    auto& video_swap_chain = *(video_swap_iter++);
    if (!video_swap_chain) {
      // TODO(sunnyps): Try to find a matching swap chain based on size, type of
      // swap chain, gl image, etc.
      video_swap_chain = std::make_unique<SwapChainPresenter>(
          this, window_, d3d11_device_, dcomp_device_);
      if (frame_rate_ > 0)
        video_swap_chain->SetFrameRate(frame_rate_);
    }
    gfx::Transform transform;
    gfx::Rect clip_rect;
    if (!video_swap_chain->PresentToSwapChain(*overlays[i], &transform,
                                              &clip_rect)) {
      DLOG(ERROR) << "PresentToSwapChain failed";
      return false;
    }
    // |SwapChainPresenter| may have changed the size of the overlay's quad
    // rect, e.g. to present to a swap chain exactly the size of the display
    // rect when the source video is larger.
    overlays[i]->transform = transform;
    overlays[i]->content_rect = gfx::Rect(video_swap_chain->content_size());
    overlays[i]->quad_rect.set_size(video_swap_chain->content_size());
    if (overlays[i]->clip_rect.has_value())
      overlays[i]->clip_rect = clip_rect;
    overlays[i]->overlay_image = DCLayerOverlayImage(
        video_swap_chain->content_size(), video_swap_chain->content());
  }

  bool status = BuildVisualTreeHelper(overlays, needs_rebuild_visual_tree_);
  needs_rebuild_visual_tree_ = false;

  return status;
}

bool DCLayerTree::BuildVisualTreeHelper(
    const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays,
    bool needs_rebuild_visual_tree) {
  // TODO(http://crbug.com/1380822): Enable optimization when delegated ink
  // trails is active.
  bool use_visual_tree_optimization =
      base::FeatureList::IsEnabled(kDCVisualTreeOptimization) &&
      !ink_renderer_->HasBeenInitialized();

  // Optimized and not optimized trees are incompatible and cannot be reused
  // for incremental updates. Rebuild visual tree if switching between optimized
  // and not optimized trees or vice versa. It will be removed once delegated
  // ink trails work with optimized DCOMP trees.
  if (visual_tree_ &&
      use_visual_tree_optimization != visual_tree_->tree_optimized()) {
    visual_tree_ = nullptr;
  }

  // TODO(http://crbug.com/1380822): Implement tree optimization where the
  // tree is built incrementally and does not require full rebuild.
  if (use_visual_tree_optimization) {
    NOTREACHED();
    return false;
  }

  if (!visual_tree_) {
    visual_tree_ = std::make_unique<VisualTree>(this);
  }

  return visual_tree_->UpdateTree(overlays, needs_rebuild_visual_tree);
}

bool DCLayerTree::ScheduleDCLayer(
    std::unique_ptr<DCLayerOverlayParams> params) {
  pending_overlays_.push_back(std::move(params));
  return true;
}

void DCLayerTree::SetFrameRate(float frame_rate) {
  frame_rate_ = frame_rate;
  for (size_t ii = 0; ii < video_swap_chains_.size(); ++ii)
    video_swap_chains_[ii]->SetFrameRate(frame_rate);
}

bool DCLayerTree::SupportsDelegatedInk() {
  return ink_renderer_->DelegatedInkIsSupported(dcomp_device_);
}

bool DCLayerTree::InitializeInkRenderer() {
  return ink_renderer_->Initialize(dcomp_device_, root_swap_chain_);
}

void DCLayerTree::AddDelegatedInkVisualToTreeIfNeeded(
    IDCompositionVisual2* root_surface_visual) {
  // Only add the ink visual to the tree if it has already been initialized.
  // It will only have been initialized if delegated ink has been used, so
  // this ensures the visual is only added when it is needed. The ink renderer
  // must be updated so that if the root swap chain or dcomp device have
  // changed the ink visual and delegated ink object can be updated
  // accordingly.
  if (!ink_renderer_->HasBeenInitialized()) {
    return;
  }

  // Reinitialize the ink renderer in case the root swap chain or dcomp
  // device changed since initialization.
  if (!InitializeInkRenderer()) {
    return;
  }

  DCHECK(SupportsDelegatedInk());
  root_surface_visual->AddVisual(ink_renderer_->GetInkVisual(), FALSE, nullptr);
  // Adding the ink visual to a new visual tree invalidates all previously set
  // properties. Therefore, force update.
  ink_renderer_->SetNeedsDcompPropertiesUpdate();
}

void DCLayerTree::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  DCHECK(SupportsDelegatedInk());

  if (!ink_renderer_->HasBeenInitialized()) {
    if (!InitializeInkRenderer())
      return;
    // This ensures that the delegated ink visual is added to the tree after
    // the root visual is created, during
    // DCLayerTree::CommitAndClearPendingOverlays
    needs_rebuild_visual_tree_ = true;
  }

  ink_renderer_->SetDelegatedInkTrailStartPoint(std::move(metadata));
}

void DCLayerTree::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  DCHECK(SupportsDelegatedInk());

  ink_renderer_->InitMessagePipeline(std::move(pending_receiver));
}

}  // namespace gl
