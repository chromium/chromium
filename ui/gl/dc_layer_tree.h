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

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/delegated_ink_point_renderer_gpu.h"
#include "ui/gl/hdr_metadata_helper_win.h"

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class DelegatedInkMetadata;
}  // namespace gfx

namespace gl {

class DirectCompositionChildSurfaceWin;
class SwapChainPresenter;

enum class VideoProcessorType { kSDR, kHDR };

// Cache video processor and its size.
struct VideoProcessorWrapper {
  VideoProcessorWrapper();
  ~VideoProcessorWrapper();
  VideoProcessorWrapper(VideoProcessorWrapper&& other);
  VideoProcessorWrapper& operator=(VideoProcessorWrapper&& other);
  VideoProcessorWrapper(const VideoProcessorWrapper&) = delete;
  VideoProcessorWrapper& operator=(VideoProcessorWrapper& other) = delete;

  // Input and output size of video processor .
  gfx::Size video_input_size;
  gfx::Size video_output_size;

  // The video processor is cached so SwapChains don't have to recreate it
  // whenever they're created.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator;
};

// DCLayerTree manages a tree of direct composition visuals, and associated
// swap chains for given overlay layers.  It maintains a list of pending layers
// submitted using ScheduleDCLayer() that are presented and committed in
// CommitAndClearPendingOverlays().
class DCLayerTree {
 public:
  using VideoProcessorMap =
      base::flat_map<VideoProcessorType, VideoProcessorWrapper>;
  using DelegatedInkRenderer =
      DelegatedInkPointRendererGpu<IDCompositionInkTrailDevice,
                                   IDCompositionDelegatedInkTrail,
                                   DCompositionInkTrailPoint>;

  DCLayerTree(bool disable_nv12_dynamic_textures,
              bool disable_vp_scaling,
              bool disable_vp_super_resolution,
              bool no_downscaled_overlay_promotion);

  DCLayerTree(const DCLayerTree&) = delete;
  DCLayerTree& operator=(const DCLayerTree&) = delete;

  ~DCLayerTree();

  // Returns true on success.
  bool Initialize(HWND window);

  // Present pending overlay layers, and perform a direct composition commit if
  // necessary.  Returns true if presentation and commit succeeded.
  bool CommitAndClearPendingOverlays(
      DirectCompositionChildSurfaceWin* root_surface);

  // Schedule an overlay layer for the next CommitAndClearPendingOverlays call.
  bool ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params);

  // Called by SwapChainPresenter to initialize video processor that can handle
  // at least given input and output size.  The video processor is shared across
  // layers so the same one can be reused if it's large enough.  Returns true on
  // success.
  VideoProcessorWrapper* InitializeVideoProcessor(const gfx::Size& input_size,
                                                  const gfx::Size& output_size,
                                                  bool is_hdr_output);

  bool disable_nv12_dynamic_textures() const {
    return disable_nv12_dynamic_textures_;
  }

  bool disable_vp_scaling() const { return disable_vp_scaling_; }

  bool disable_vp_super_resolution() const {
    return disable_vp_super_resolution_;
  }

  bool no_downscaled_overlay_promotion() const {
    return no_downscaled_overlay_promotion_;
  }

  VideoProcessorWrapper& GetOrCreateVideoProcessor(bool is_hdr);

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  void GetSwapChainVisualInfoForTesting(size_t index,
                                        gfx::Transform* transform,
                                        gfx::Point* offset,
                                        gfx::Rect* clip_rect) const;

  size_t GetSwapChainPresenterCountForTesting() const {
    return video_swap_chains_.size();
  }

  size_t GetDcompLayerCountForTesting() const {
    return visual_tree_ ? visual_tree_->GetDcompLayerCountForTesting() : 0;
  }
  IDCompositionVisual2* GetContentVisualForTesting(size_t index) const {
    return visual_tree_ ? visual_tree_->GetContentVisualForTesting(index)
                        : nullptr;
  }

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
    // Given pending overlays, builds or updates this visual tree.
    // Returns true if commit succeeded.
    bool UpdateTree(
        const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays,
        // True if the tree must rebuild.
        bool needs_rebuild_visual_tree);
    void GetSwapChainVisualInfoForTesting(size_t index,
                                          gfx::Transform* transform,
                                          gfx::Point* offset,
                                          gfx::Rect* clip_rect) const;
    size_t GetDcompLayerCountForTesting() const {
      return visual_subtrees_.size();
    }
    IDCompositionVisual2* GetContentVisualForTesting(size_t index) const {
      return visual_subtrees_[index]->content_visual();
    }
    // Returns true if the tree is optimized.
    // TODO(http://crbug.com/1380822): Implement tree optimization where the
    // tree is built incrementally and does not require full rebuild.
    bool tree_optimized() const { return tree_optimized_; }

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
      bool Update(IDCompositionDevice2* dcomp_device,
                  Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
                  uint64_t dcomp_surface_serial,
                  const gfx::Size& image_size,
                  const gfx::Rect& content_rect,
                  const gfx::Rect& quad_rect,
                  bool nearest_neighbor_filter,
                  const gfx::Transform& quad_to_root_transform,
                  const absl::optional<gfx::Rect>& clip_rect_in_root);

      IDCompositionVisual2* container_visual() const {
        return clip_visual_.Get();
      }
      IDCompositionVisual2* content_visual() const {
        return content_visual_.Get();
      }
      IUnknown* dcomp_visual_content() const {
        return dcomp_visual_content_.Get();
      }
      void GetSwapChainVisualInfoForTesting(gfx::Transform* transform,
                                            gfx::Point* offset,
                                            gfx::Rect* clip_rect) const;

      int z_order() const { return z_order_; }
      void set_z_order(int z_order) { z_order_ = z_order; }

     private:
      // The root of this subtree. In root space and contains the clip rect.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> clip_visual_;
      // The child of |clip_visual_|, transforms its children from quad to root
      // space. This visual exists because |offset_| is in quad space, so it
      // must be affected by |transform_|. They cannot be on the same visual
      // since |IDCompositionVisual::SetTransform| and
      // |IDCompositionVisual::SetOffset[XY]| are applied in the opposite order
      // than we want.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> transform_visual_;
      // The child of |transform_visual_|. In quad space, holds
      // |dcomp_visual_content_|.
      Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual_;

      // The content to be placed at the leaf of the visual subtree. Either an
      // IDCompositionSurface or an IDXGISwapChain.
      Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content_;
      // |dcomp_surface_serial_| is associated with |dcomp_visual_content_| of
      // IDCompositionSurface type. New value indicates that dcomp surface data
      // is updated.
      uint64_t dcomp_surface_serial_ = 0;

      // The portion of |dcomp_visual_content_| to display. This area will be
      // mapped to |quad_rect_|'s bounds.
      gfx::Rect content_rect_;

      // The bounds which contain this overlay. When mapped by |transform_|,
      // this is the bounds of the overlay in root space.
      gfx::Rect quad_rect_;

      // Whether or not to use nearest-neighbor filtering to scale
      // |dcomp_visual_content_|. This is applied to |transform_visual_| since
      // both it and |content_visual_| can scale the content.
      bool nearest_neighbor_filter_ = false;

      // Transform from quad space to root space.
      gfx::Transform transform_;

      // Clip rect in root space.
      absl::optional<gfx::Rect> clip_rect_;

      // The size of overlay image in |dcomp_visual_content_| which is in
      // pixels.
      gfx::Size image_size_;

      // The order relative to the root surface. Positive values means the
      // visual appears in front of the root surface (i.e. overlay) and negative
      // values means the visual appears below the root surface (i.e. underlay).
      int z_order_ = 0;
    };

   private:
    // Tree that owns `this`.
    const raw_ptr<DCLayerTree> dc_layer_tree_ = nullptr;
    // List of DCOMP visual subtrees for previous frame.
    std::vector<std::unique_ptr<VisualSubtree>> visual_subtrees_;
    // TODO(http://crbug.com/1380822): Implement tree optimization where the
    // tree is built incrementally and does not require full rebuild.
    const bool tree_optimized_ = false;
  };

 private:
  // Given pending overlays, builds or updates visual tree.
  // Returns true if commit succeeded.
  bool BuildVisualTreeHelper(
      const std::vector<std::unique_ptr<DCLayerOverlayParams>>& overlays,
      // True if the caller determined that rebuilding the tree is required.
      bool needs_rebuild_visual_tree);

  // This will add an ink visual to the visual tree to enable delegated ink
  // trails. This will initially always be called directly before an OS
  // delegated ink API is used. After that, it can also be added anytime the
  // visual tree is rebuilt.
  void AddDelegatedInkVisualToTreeIfNeeded(
      IDCompositionVisual2* root_surface_visual);

  // The ink renderer must be initialized before an OS API is used in order to
  // set up the delegated ink visual and delegated ink trail object.
  bool InitializeInkRenderer();

  const bool disable_nv12_dynamic_textures_;
  const bool disable_vp_scaling_;
  const bool disable_vp_super_resolution_;
  const bool no_downscaled_overlay_promotion_;

  HWND window_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;

  // Store video processor for SDR/HDR mode separately, which could avoid
  // problem in (http://crbug.com/1121061).
  VideoProcessorMap video_processor_map_;

  // Current video processor input and output colorspace.
  gfx::ColorSpace video_input_color_space_;
  gfx::ColorSpace video_output_color_space_;

  // Set to true if a direct composition root visual needs rebuild.
  // Each overlay is represented by a VisualSubtree, which is placed in the root
  // visual's child list in draw order. Whenever the number of overlays or their
  // draw order changes, the root visual needs to be rebuilt.
  bool needs_rebuild_visual_tree_ = false;

  // Set if root surface is using a swap chain currently.
  Microsoft::WRL::ComPtr<IDXGISwapChain1> root_swap_chain_;

  // Set if root surface is using a direct composition surface currently.
  Microsoft::WRL::ComPtr<IDCompositionSurface> root_dcomp_surface_;

  // Root direct composition visual for window dcomp target.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dcomp_root_visual_;

  // List of pending overlay layers from ScheduleDCLayer().
  std::vector<std::unique_ptr<DCLayerOverlayParams>> pending_overlays_;

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
  // platform supports delegated ink trails. It must be initialized via the
  // Initialize() method in order to be used for drawing delegated ink trails.
  std::unique_ptr<DelegatedInkRenderer> ink_renderer_;
};

}  // namespace gl

#endif  // UI_GL_DC_LAYER_TREE_H_
