// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11_1.h>

#include "ui/gl/dc_layer_tree.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/direct_composition_child_surface_win.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/swap_chain_presenter.h"

namespace gl {
namespace {
bool SizeContains(const gfx::Size& a, const gfx::Size& b) {
  return gfx::Rect(a).Contains(gfx::Rect(b));
}

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
  for (size_t i = 0, swapchain_i = 0; i < visual_subtrees_.size(); ++i) {
    // Skip root layer.
    if (visual_subtrees_[i]->z_order() == 0)
      continue;

    if (swapchain_i == index) {
      visual_subtrees_[i]->GetSwapChainVisualInfoForTesting(  // IN-TEST
          transform, offset, clip_rect);
      return;
    }
    swapchain_i++;
  }
}

DCLayerTree::VisualSubtree::VisualSubtree() = default;
DCLayerTree::VisualSubtree::~VisualSubtree() = default;

bool DCLayerTree::VisualSubtree::Update(
    IDCompositionDevice2* dcomp_device,
    Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
    uint64_t dcomp_surface_serial,
    const gfx::Vector2d& quad_rect_offset,
    const gfx::Transform& quad_to_root_transform,
    const absl::optional<gfx::Rect>& clip_rect_in_root) {
  bool needs_commit = false;

  // Methods that update the visual tree can only fail with OOM. We'll assert
  // success in this function to aid in debugging.
  HRESULT hr = S_OK;

  if (!clip_visual_) {
    needs_commit = true;

    // All the visual are created together on the first |Update|.
    DCHECK(!content_visual_);
    hr = dcomp_device->CreateVisual(&clip_visual_);
    CHECK_EQ(hr, S_OK);
    hr = dcomp_device->CreateVisual(&content_visual_);
    CHECK_EQ(hr, S_OK);
    hr = clip_visual_->AddVisual(content_visual_.Get(), FALSE, nullptr);
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

  if (offset_ != quad_rect_offset) {
    offset_ = quad_rect_offset;
    needs_commit = true;

    // Visual offset is applied before transform so it behaves similar to how
    // the compositor uses transform to map quad rect in layer space to target
    // space.
    hr = content_visual_->SetOffsetX(offset_.x());
    CHECK_EQ(hr, S_OK);
    hr = content_visual_->SetOffsetY(offset_.y());
    CHECK_EQ(hr, S_OK);
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
    hr = content_visual_->SetTransform(matrix);
    CHECK_EQ(hr, S_OK);
  }

  if (dcomp_visual_content_ != dcomp_visual_content) {
    dcomp_visual_content_ = std::move(dcomp_visual_content);
    needs_commit = true;
    hr = content_visual_->SetContent(dcomp_visual_content_.Get());
    if (FAILED(hr)) {
      // This can be changed back to a CHECK_EQ once
      // DirectCompositionPixelTest.RootSurfaceDrawOffset in
      // ui/gl/direct_composition_surface_win_unittest.cc is removed.
      DLOG(ERROR) << "SetContent failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
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

void DCLayerTree::VisualSubtree::GetSwapChainVisualInfoForTesting(
    gfx::Transform* transform,
    gfx::Point* offset,
    gfx::Rect* clip_rect) const {
  *transform = transform_;
  *offset = gfx::Point() + offset_;
  *clip_rect = clip_rect_.value_or(gfx::Rect());
}

bool DCLayerTree::CommitAndClearPendingOverlays(
    DirectCompositionChildSurfaceWin* root_surface) {
  TRACE_EVENT1("gpu", "DCLayerTree::CommitAndClearPendingOverlays",
               "num_pending_overlays", pending_overlays_.size());
  DCHECK(!needs_rebuild_visual_tree_ || ink_renderer_->HasBeenInitialized());

  if (root_surface) {
    if (root_surface->swap_chain() != root_swap_chain_ ||
        root_surface->dcomp_surface() != root_dcomp_surface_) {
      root_swap_chain_ = root_surface->swap_chain();
      root_dcomp_surface_ = root_surface->dcomp_surface();
      needs_rebuild_visual_tree_ = true;
    }
  }

  std::vector<std::unique_ptr<ui::DCRendererLayerParams>> overlays;
  std::swap(pending_overlays_, overlays);

  // Grow or shrink list of swap chain presenters to match pending overlays.
  if (video_swap_chains_.size() != overlays.size()) {
    video_swap_chains_.resize(overlays.size());
    // If we need to grow or shrink swap chain presenters, we'll need to add or
    // remove visuals.
    needs_rebuild_visual_tree_ = true;
  }

  // DCompSurfaceless also uses DCLayerTree and lets its caller schedule an
  // overlay for the root surface, instead of owning its own.
  if (root_surface) {
    // Add a placeholder overlay for the root surface, at a z-order of 0.
    auto root_params = std::make_unique<ui::DCRendererLayerParams>();
    root_params->z_order = 0;
    root_params->dcomp_visual_content =
        root_swap_chain_ ? static_cast<IUnknown*>(root_swap_chain_.Get())
                         : static_cast<IUnknown*>(root_dcomp_surface_.Get());
    root_params->dcomp_surface_serial = root_surface->dcomp_surface_serial();
    overlays.emplace_back(std::move(root_params));
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
    // Skip root surface overlay.
    if (overlays[i]->z_order == 0)
      continue;
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
    overlays[i]->transform = transform;
    if (overlays[i]->clip_rect.has_value())
      overlays[i]->clip_rect = clip_rect;
    overlays[i]->dcomp_visual_content = video_swap_chain->content();
  }

  bool status = BuildVisualTreeHelper(overlays, needs_rebuild_visual_tree_);
  needs_rebuild_visual_tree_ = false;

  return status;
}

bool DCLayerTree::BuildVisualTreeHelper(
    const std::vector<std::unique_ptr<ui::DCRendererLayerParams>>& overlays,
    bool needs_rebuild_visual_tree) {
  // Grow or shrink list of visual subtrees to match pending overlays.
  size_t old_visual_subtrees_size = visual_subtrees_.size();
  if (old_visual_subtrees_size != overlays.size()) {
    visual_subtrees_.resize(overlays.size());
    needs_rebuild_visual_tree = true;
  }

#if DCHECK_IS_ON()
  bool root_surface_visual_updated = false;
#endif
  bool needs_commit = false;
  // Build or update visual subtree for each overlay.
  for (size_t i = 0; i < overlays.size(); ++i) {
    DCHECK(visual_subtrees_[i] || i >= old_visual_subtrees_size);
    if (!visual_subtrees_[i])
      visual_subtrees_[i] = std::make_unique<VisualSubtree>();

    if (visual_subtrees_[i]->z_order() != overlays[i]->z_order) {
      visual_subtrees_[i]->set_z_order(overlays[i]->z_order);

      // Z-order is a property of the root visual's child list, not any property
      // on the subtree's nodes. If it changes, we need to rebuild the tree.
      needs_rebuild_visual_tree = true;
    }

    // We don't need to set |needs_rebuild_visual_tree_| here since that is only
    // needed when the root visual's children need to be reordered. |Update|
    // only affects the subtree for each child, so only a commit is needed in
    // this case.
    needs_commit |= visual_subtrees_[i]->Update(
        dcomp_device_.Get(), overlays[i]->dcomp_visual_content,
        overlays[i]->dcomp_surface_serial,
        overlays[i]->quad_rect.OffsetFromOrigin(), overlays[i]->transform,
        overlays[i]->clip_rect);

    // Zero z_order represents root layer.
    if (overlays[i]->z_order == 0) {
      DCHECK(root_surface_visual_.Get() ==
                 visual_subtrees_[i]->content_visual() ||
             needs_rebuild_visual_tree);
#if DCHECK_IS_ON()
      // Verify we have single root visual layer.
      DCHECK(!root_surface_visual_updated);
      root_surface_visual_updated = true;
#endif
      root_surface_visual_ = visual_subtrees_[i]->content_visual();
    }
  }

  // Rebuild root visual's child list.
  // Note: needs_rebuild_visual_tree might be set in the caller, this function
  // and can also be set in DCLayerTree::SetDelegatedInkTrailStartPoint to add a
  // delegated ink visual into the root surface's visual.
  if (needs_rebuild_visual_tree) {
    TRACE_EVENT0(
        "gpu", "DCLayerTree::CommitAndClearPendingOverlays::ReBuildVisualTree");
    dcomp_root_visual_->RemoveAllVisuals();

    for (size_t i = 0; i < visual_subtrees_.size(); ++i) {
      // We call AddVisual with insertAbove FALSE and referenceVisual nullptr
      // which is equivalent to saying that the visual should be below no
      // other visual, or in other words it should be above all other visuals.
      dcomp_root_visual_->AddVisual(visual_subtrees_[i]->container_visual(),
                                    FALSE, nullptr);
    }
    // Only add the ink visual to the tree if it has already been initialized.
    // It will only have been initialized if delegated ink has been used, so
    // this ensures the visual is only added when it is needed. The ink renderer
    // must be updated so that if the root swap chain or dcomp device have
    // changed the ink visual and delegated ink object can be updated
    // accordingly.
    if (ink_renderer_->HasBeenInitialized()) {
      // Reinitialize the ink renderer in case the root swap chain or dcomp
      // device changed since initialization.
      if (InitializeInkRenderer())
        AddDelegatedInkVisualToTree();
    }
    needs_commit = true;
  }

  if (needs_commit) {
    TRACE_EVENT0("gpu", "DCLayerTree::CommitAndClearPendingOverlays::Commit");
    HRESULT hr = dcomp_device_->Commit();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Commit failed with error 0x" << std::hex << hr;
      return false;
    }
  }

  return true;
}

bool DCLayerTree::ScheduleDCLayer(
    std::unique_ptr<ui::DCRendererLayerParams> params) {
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

void DCLayerTree::AddDelegatedInkVisualToTree() {
  DCHECK(SupportsDelegatedInk());
  DCHECK(ink_renderer_->HasBeenInitialized());

  root_surface_visual_->AddVisual(ink_renderer_->GetInkVisual(), FALSE,
                                  nullptr);

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
