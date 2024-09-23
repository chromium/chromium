// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_LAYER_TREE_H_
#define UI_GL_DC_LAYER_TREE_H_

#include <windows.h>

#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include <memory>

#include "base/check_is_test.h"
#include "base/containers/flat_map.h"
#include "base/moving_window.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/delegated_ink_point_renderer_gpu.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class DelegatedInkMetadata;
}  // namespace gfx

namespace gl {

class SwapChainPresenter;

// Cache video processor and its size.
struct VideoProcessorWrapper {
  VideoProcessorWrapper();
  ~VideoProcessorWrapper();
  VideoProcessorWrapper(VideoProcessorWrapper&& other) = delete;
  VideoProcessorWrapper& operator=(VideoProcessorWrapper&& other) = delete;
  VideoProcessorWrapper(const VideoProcessorWrapper&) = delete;
  VideoProcessorWrapper& operator=(VideoProcessorWrapper& other) = delete;

  class SizeSmoother {
   public:
    SizeSmoother();
    ~SizeSmoother();
    SizeSmoother(SizeSmoother&& other) = delete;
    SizeSmoother& operator=(SizeSmoother&& other) = delete;
    SizeSmoother(const SizeSmoother& other) = delete;
    SizeSmoother& operator=(SizeSmoother& other) = delete;
    void PutSize(gfx::Size size);
    gfx::Size GetSize() const;

   private:
    base::MovingMax<int> width_;
    base::MovingMax<int> height_;
  };

  // Input and output size of video processor.
  gfx::Size video_input_size;
  gfx::Size video_output_size;

  // Max window filter for each dimension for input and output size.
  // Used to calculate required size in case there are many different
  // sizes in use.
  SizeSmoother input_size_smoother;
  SizeSmoother output_size_smoother;

  bool GetDriverSupportsVpAutoHdr() { return driver_supports_vp_auto_hdr; }
  void SetDriverSupportsVpAutoHdr(bool value) {
    driver_supports_vp_auto_hdr = value;
  }

  // The video processor is cached so SwapChains don't have to recreate it
  // whenever they're created.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator;

 private:
  // Whether the GPU driver supports video processor auto HDR.
  bool driver_supports_vp_auto_hdr = false;
};

class SolidColorSurface;

// A resource pool that contains DComp surfaces containing solid color fills.
class SolidColorSurfacePool final {
 public:
  SolidColorSurfacePool(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device);
  ~SolidColorSurfacePool();

  SolidColorSurfacePool(const SolidColorSurfacePool&) = delete;
  SolidColorSurfacePool& operator=(const SolidColorSurfacePool&) = delete;

  // The resulting surface only contains the opaque parts of |color| and needs
  // to be scaled by |color.fA|. Its contents are only valid until the next
  // |TrimAfterCommit| call, since surfaces can be reused (and recolored) on
  // subsequent frames.
  IDCompositionSurface* GetSolidColorSurface(const SkColor4f& color);

  // Clean up any unused resources in the pool after DComp commit.
  void TrimAfterCommit();

  // Returns the number of surfaces currently tracked by this pool.
  size_t GetNumSurfacesInPoolForTesting() const;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device_;

  // Solid color surfaces that are tracked by this pool.
  std::vector<SolidColorSurface> tracked_surfaces_;
  // Index into |tracked_surfaces_| that partitions the surfaces used this frame
  // (<num_used_this_frame_) and the surfaces free to use by subsequent
  // |GetSolidColorSurface| calls (>=num_used_this_frame_).
  size_t num_used_this_frame_ = 0;

  struct Stats {
    // The number of times |GetSolidColorSurface| was called. This represents
    // the number of solid color overlays in the frame.
    int num_surfaces_requested = 0;

    // The number of surfaces that were filled.
    int num_surfaces_recolored = 0;
  };

  // Stats about this pool since the last |TrimAfterCommit| call.
  Stats stats_since_last_trim_;
};

// DCLayerTree manages a tree of direct composition visuals, and associated
// swap chains for given overlay layers.
class GL_EXPORT DCLayerTree {
 public:
  using DelegatedInkRenderer = DelegatedInkPointRendererGpu;

  DCLayerTree(bool disable_nv12_dynamic_textures,
              bool disable_vp_auto_hdr,
              bool disable_vp_scaling,
              bool disable_vp_super_resolution,
              bool force_dcomp_triple_buffer_video_swap_chain,
              bool no_downscaled_overlay_promotion);

  DCLayerTree(const DCLayerTree&) = delete;
  DCLayerTree& operator=(const DCLayerTree&) = delete;

  ~DCLayerTree();

  void Initialize(HWND window,
                  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  // Present overlay layers, and perform a direct composition commit if
  // necessary. Returns true if presentation and commit succeeded.
  bool CommitAndClearPendingOverlays(
      std::vector<std::unique_ptr<DCLayerOverlayParams>> overlays);

  // Called by SwapChainPresenter to initialize video processor that can handle
  // at least given input and output size.  The video processor is shared across
  // layers so the same one can be reused if it's large enough.  Returns true on
  // success.
  VideoProcessorWrapper* InitializeVideoProcessor(
      const gfx::Size& input_size,
      const gfx::Size& output_size,
      bool is_hdr_output,
      bool& video_processor_recreated);

  bool disable_nv12_dynamic_textures() const {
    return disable_nv12_dynamic_textures_;
  }

  bool disable_vp_auto_hdr() const { return disable_vp_auto_hdr_; }

  bool disable_vp_scaling() const { return disable_vp_scaling_; }

  bool disable_vp_super_resolution() const {
    return disable_vp_super_resolution_;
  }

  bool force_dcomp_triple_buffer_video_swap_chain() const {
    return force_dcomp_triple_buffer_video_swap_chain_;
  }

  bool no_downscaled_overlay_promotion() const {
    return no_downscaled_overlay_promotion_;
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  void GetSwapChainVisualInfoForTesting(size_t index,
                                        gfx::Transform* transform,
                                        gfx::Point* offset,
                                        gfx::Rect* clip_rect) const;

  size_t GetSwapChainPresenterCountForTesting() const {
    CHECK_IS_TEST();
    return video_swap_chains_.size();
  }

  size_t GetDcompLayerCountForTesting() const {
    CHECK_IS_TEST();
    return visual_tree_ ? visual_tree_->GetDcompLayerCountForTesting() : 0;
  }
  IDCompositionVisual2* GetContentVisualForTesting(size_t index) const {
    CHECK_IS_TEST();
    return visual_tree_ ? visual_tree_->GetContentVisualForTesting(index)
                        : nullptr;
  }
  IDCompositionSurface* GetBackgroundColorSurfaceForTesting(
      size_t index) const {
    CHECK_IS_TEST();
    return visual_tree_
               ? visual_tree_->GetBackgroundColorSurfaceForTesting(index)
               : nullptr;
  }
  size_t GetNumSurfacesInPoolForTesting() const;
#if DCHECK_IS_ON()
  bool GetAttachedToRootFromPreviousFrameForTesting(size_t index) const;
#endif  // DCHECK_IS_ON()

  void SetFrameRate(float frame_rate);

  const std::unique_ptr<HDRMetadataHelperWin>& GetHDRMetadataHelper() {
    return hdr_metadata_helper_;
  }

  HWND window() const { return window_; }

  bool SupportsDelegatedInk();

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata>);

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver);

  DelegatedInkRenderer* GetInkRendererForTesting() const {
    CHECK_IS_TEST();
    return ink_renderer_.get();
  }

  // Owns a list of |VisualSubtree|s that represent visual layers.
  class VisualTree {
   public:
    VisualTree(DCLayerTree* tree);

    VisualTree(VisualTree&&) = delete;
    VisualTree(const VisualTree&) = delete;
    VisualTree& operator=(const VisualTree&) = delete;

    ~VisualTree();
    // Given overlays, builds or updates this visual tree.
    // Returns true if commit succeeded.
    bool BuildTree(
        const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays);

    void GetSwapChainVisualInfoForTesting(size_t index,
                                          gfx::Transform* transform,
                                          gfx::Point* offset,
                                          gfx::Rect* clip_rect) const;
    size_t GetDcompLayerCountForTesting() const {
      CHECK_IS_TEST();
      return visual_subtrees_.size();
    }
    IDCompositionVisual2* GetContentVisualForTesting(size_t index) const {
      CHECK_IS_TEST();
      return visual_subtrees_[index]->content_visual();
    }
    IDCompositionSurface* GetBackgroundColorSurfaceForTesting(
        size_t index) const {
      CHECK_IS_TEST();
      return visual_subtrees_[index]->background_color_surface_for_testing();
    }
#if DCHECK_IS_ON()
    bool GetAttachedToRootFromPreviousFrameForTesting(size_t index) const {
      CHECK_IS_TEST();
      return visual_subtrees_[index]
          ->GetAttachedToRootFromPreviousFrameForTesting();
    }
#endif  // DCHECK_IS_ON()
    // Maps the visual content to its corresponding subtree index.
    // This is used to find matching subtrees from the previous frame
    // that can be reused in the current frame.
    // It's safe to use raw pointers here since we have a ComPtr to the visual
    // content in the visual subtrees list for the previous frame.
    using VisualSubtreeMap = base::flat_map<raw_ptr<IUnknown>, size_t>;
    // Owns a subtree of DComp visual that apply clip, offset, etc. and contains
    // some content at its leaf.
    // This class keeps track about what properties are currently set on the
    // visuals.
    class VisualSubtree {
     public:
      VisualSubtree();
      ~VisualSubtree();
      VisualSubtree(VisualSubtree&& other) = delete;
      VisualSubtree& operator=(VisualSubtree&& other) = delete;
      VisualSubtree(const VisualSubtree&) = delete;
      VisualSubtree& operator=(VisualSubtree& other) = delete;

      // Returns true if something was changed.
      bool Update(
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
          bool allow_antialiasing);

      IDCompositionVisual2* container_visual() const {
        return clip_visual_.Get();
      }
      IDCompositionVisual2* content_visual() const {
        return content_visual_.Get();
      }
      IUnknown* dcomp_visual_content() const {
        return dcomp_visual_content_.Get();
      }
      IDCompositionSurface* background_color_surface_for_testing() const {
        CHECK_IS_TEST();
        return background_color_surface_.Get();
      }
      void GetSwapChainVisualInfoForTesting(gfx::Transform* transform,
                                            gfx::Point* offset,
                                            gfx::Rect* clip_rect) const;
#if DCHECK_IS_ON()
      bool GetAttachedToRootFromPreviousFrameForTesting() const {
        CHECK_IS_TEST();
        return attached_to_root_from_previous_frame_;
      }
#endif  // DCHECK_IS_ON()

      int z_order() const { return z_order_; }
      void set_z_order(int z_order) { z_order_ = z_order; }

     private:
#if DCHECK_IS_ON()
      friend class VisualTree;
#endif  // DCHECK_IS_ON()
      // The root of this subtree. In root space and contains the clip rect and
      // controls subtree opacity.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> clip_visual_;
      // In root space and contains the rounded rectangle clip. This is separate
      // from |clip_visual_| since an overlay layer can have both a rectangular
      // and a rounded rectangular clip rects.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> rounded_corners_visual_;
      // The child of |clip_visual_|, transforms its children from quad to root
      // space. This visual exists because |offset_| is in quad space, so it
      // must be affected by |transform_|. They cannot be on the same visual
      // since |IDCompositionVisual::SetTransform| and
      // |IDCompositionVisual::SetOffset[XY]| are applied in the opposite order
      // than we want.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> transform_visual_;
      // A child of |transform_visual_|. In quad space, holds
      // |dcomp_visual_content_|. Visually, this is behind |content_visual_|.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> background_color_visual_;
      // A child of |transform_visual_|. In quad space, holds
      // |dcomp_visual_content_|.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual_;

      // The content to be placed at a leaf of the visual subtree. Either an
      // IDCompositionSurface or an IDXGISwapChain.
      Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content_;
      // |dcomp_surface_serial_| is associated with |dcomp_visual_content_| of
      // IDCompositionSurface type. New value indicates that dcomp surface data
      // is updated.
      uint64_t dcomp_surface_serial_ = 0;

      // The portion of |dcomp_visual_content_| to display. This area will be
      // mapped to |quad_rect_|'s bounds.
      gfx::RectF content_rect_;

      // The surface for the background color fill to be placed at a leaf of the
      // visual subtree. Since |SolidColorSurfacePool::GetSolidColorSurface|
      // returns a surface that is opaque, |background_color_visual_|'s opacity
      // will be set to |background_color_.fA|. Must be present if
      // |background_color_| is non-transparent. Must be re-updated from
      // |SolidColorSurfacePool::GetSolidColorSurface| every frame it is
      // present.
      Microsoft::WRL::ComPtr<IDCompositionSurface> background_color_surface_;

      // The color of |background_color_surface_|.
      SkColor4f background_color_;

      // The bounds which contain this overlay. When mapped by |transform_|,
      // this is the bounds of the overlay in root space.
      gfx::Rect quad_rect_;

      // Whether or not to use nearest-neighbor filtering to scale
      // |dcomp_visual_content_|. This is applied to |transform_visual_| since
      // both it and |content_visual_| can scale the content.
      bool nearest_neighbor_filter_ = false;

      // Transform from quad space to root space.
      gfx::Transform quad_to_root_transform_;

      // Clip rect in root space.
      std::optional<gfx::Rect> clip_rect_in_root_;

      // Rounded corner clip in root space
      gfx::RRectF rounded_corner_bounds_;

      // The opacity of the entire visual subtree
      float opacity_ = 1.0;

      // The size of overlay image in |dcomp_visual_content_| which is in
      // pixels.
      gfx::Size image_size_;

      // If false, force |transform_visual_| to use the hard border mode.
      bool allow_antialiasing_ = true;

      // The order relative to the root surface. Positive values means the
      // visual appears in front of the root surface (i.e. overlay) and negative
      // values means the visual appears below the root surface (i.e. underlay).
      int z_order_ = 0;

#if DCHECK_IS_ON()
      // True if the subtree is reused from the previous frame and keeps its
      // attachment to the root from the previous frame. Used for testing.
      bool attached_to_root_from_previous_frame_ = false;
#endif  // DCHECK_IS_ON()
    };

   private:
    // This function is called as part of |BuildTreeOptimized|.
    // For each given overlay:
    // 1. Populate visual subtree map with visual content.
    // 2. Find the matching subtree from the previous frame. The subtree matches
    // if it owns identical visual content. If the match is found:
    //    2.1. Updates |overlay_index_to_reused_subtree| with the
    //    index to the matching subtree.
    //    2.2. Updates |subtree_index_to_overlay| with the overlay index the
    //    previous frame subtree is matched to.
    // Returns populated visual subtree map.
    VisualSubtreeMap BuildMapAndAssignMatchingSubtrees(
        const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays,
        std::vector<std::unique_ptr<VisualSubtree>>& visual_subtrees,
        std::vector<std::optional<size_t>>& overlay_index_to_reused_subtree,
        std::vector<std::optional<size_t>>& subtree_index_to_overlay);

    // This function is called as part of |BuildTreeOptimized|.
    // For each overlay that has no match attempts to find unused subtree of
    // the previous frame to be reused in the current frame. If such a subtree
    // is identified:
    // 1. Updates |overlay_index_to_reused_subtree| with the
    //    index to the found subtree.
    // 2. Updates |subtree_index_to_overlay| with the overlay index the
    //    found subtree is assigned to.
    // Returns previous frame subtree first unused index.
    size_t ReuseUnmatchedSubtrees(
        std::vector<std::unique_ptr<VisualSubtree>>& new_visual_subtrees,
        std::vector<std::optional<size_t>>& overlay_index_to_reused_subtree,
        std::vector<std::optional<size_t>>& subtree_index_to_overlay);

    // This function is called as part of |BuildTreeOptimized|.
    // Detaches unused subtrees of the previous frame from root starting with
    // |first_prev_frame_subtree_unused_index| returned from
    // |ReuseUnmatchedSubtrees|.
    // Updates |prev_subtree_is_attached_to_root| accordingly.
    // Returns true if commit is needed.
    bool DetachUnusedSubtreesFromRoot(
        size_t first_prev_frame_subtree_unused_index,
        std::vector<bool>& prev_subtree_is_attached_to_root);

    // This function is called as part of |BuildTreeOptimized|.
    // Removes reused subtrees of the previous frame from the root that need to
    // be repositioned in the current frame.
    // Updates |prev_subtree_is_attached_to_root| accordingly.
    // Returns true if commit is needed.
    bool DetachReusedSubtreesThatNeedRepositioningFromRoot(
        const std::vector<std::unique_ptr<VisualSubtree>>& new_visual_subtrees,
        const std::vector<std::optional<size_t>>&
            overlay_index_to_reused_subtree,
        const std::vector<std::optional<size_t>>& subtree_index_to_overlay,
        std::vector<bool>& prev_subtree_is_attached_to_root);

    // Detaches given subtree from the root.
    void DetachSubtreeFromRoot(VisualSubtree* subtree);

    // Tree that owns `this`.
    const raw_ptr<DCLayerTree> dc_layer_tree_ = nullptr;
    // List of DCOMP visual subtrees for previous frame.
    std::vector<std::unique_ptr<VisualSubtree>> visual_subtrees_;
    VisualSubtreeMap subtree_map_;
  };

 private:
  const bool disable_nv12_dynamic_textures_;
  const bool disable_vp_auto_hdr_;
  const bool disable_vp_scaling_;
  const bool disable_vp_super_resolution_;
  const bool force_dcomp_triple_buffer_video_swap_chain_;
  const bool no_downscaled_overlay_promotion_;

  HWND window_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;

  // Resource pool which owns surfaces for solid color overlays. This is needed
  // since there is no way to procedurally fill a DComp visual.
  std::unique_ptr<SolidColorSurfacePool> solid_color_surface_pool_;

  // Store the largest video processor for SDR and HDR content
  // to avoid problems in (http://crbug.com/1121061) and
  // (http://crbug.com/1472975).
  VideoProcessorWrapper video_processor_wrapper_sdr_;
  VideoProcessorWrapper video_processor_wrapper_hdr_;

  // Current video processor input and output colorspace.
  gfx::ColorSpace video_input_color_space_;
  gfx::ColorSpace video_output_color_space_;

  // Root direct composition visual for window dcomp target.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dcomp_root_visual_;

  // List of swap chain presenters for previous frame.
  std::vector<std::unique_ptr<SwapChainPresenter>> video_swap_chains_;

  // A tree that owns all DCOMP visuals for overlays along with attributes
  // required to build DCOMP tree. It's updated for each frame.
  std::unique_ptr<VisualTree> visual_tree_;

  // Number of frames per second.
  float frame_rate_ = 0.f;

  // dealing with hdr metadata
  std::unique_ptr<HDRMetadataHelperWin> hdr_metadata_helper_;

  // Renderer for drawing delegated ink trails using OS APIs. This is created
  // when the DCLayerTree is created, but can only be queried to check if the
  // platform supports delegated ink trails. It will be initialized via the
  // call to MakeDelegatedInkOverlay when DCLayerTree has received a
  // delegated_ink_metadata_ and CommitAndClearPendingOverlays is underway.
  std::unique_ptr<DelegatedInkRenderer> ink_renderer_;

  // Cache the metadata received by the DCLayerTree until it is time to
  // CommitAndClearPendingOverlays. At that point the metadata will be moved
  // to DelegatedInkPointRendererGPU.
  std::unique_ptr<gfx::DelegatedInkMetadata> pending_delegated_ink_metadata_;
};

}  // namespace gl

#endif  // UI_GL_DC_LAYER_TREE_H_
