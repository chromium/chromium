// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_

#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>
#include <QuartzCore/QuartzCore.h>

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gfx/video_types.h"

@class AVSampleBufferDisplayLayer;

namespace ui {

ACCELERATED_WIDGET_MAC_EXPORT BASE_DECLARE_FEATURE(
    kFullscreenLowPowerBackdropMac);

struct CARendererLayerParams;

enum class CALayerType {
  // A CALayer with contents set to an IOSurface by setContents.
  kDefault,
  // An AVSampleBufferDisplayLayer.
  kVideo,
  // A CAMetalLayer that copies half-float or 10-bit IOSurfaces.
  kHDRCopier,
};

// The CARendererLayerTree will construct a hierarchy of CALayers from a linear
// list provided by the CoreAnimation renderer using the algorithm and structure
// referenced described in
// https://docs.google.com/document/d/1DtSN9zzvCF44_FQPM7ie01UxGHagQ66zfF5L9HnigQY/edit?usp=sharing
class ACCELERATED_WIDGET_MAC_EXPORT CARendererLayerTree {
 public:
  CARendererLayerTree(bool allow_av_sample_buffer_display_layer,
                      bool allow_solid_color_layers);

  CARendererLayerTree(const CARendererLayerTree&) = delete;
  CARendererLayerTree& operator=(const CARendererLayerTree&) = delete;

  // This will remove all CALayers from this tree from their superlayer.
  ~CARendererLayerTree();

  // Append the description of a new CALayer to the tree. This will not
  // create any new CALayers until CommitScheduledCALayers is called. This
  // cannot be called anymore after CommitScheduledCALayers has been called.
  bool ScheduleCALayer(const CARendererLayerParams& params);

  // Set the MTLDevice to use for any CAMetalLayers.
  void SetMetalDevice(id<MTLDevice> metal_device) {
    metal_device_ = metal_device;
  }

  void SetDisplayHDRHeadroom(float display_hdr_headroom) {
    display_hdr_headroom_ = display_hdr_headroom;
  }

  // Create a CALayer tree for the scheduled layers, and set |superlayer| to
  // have only this tree as its sublayers. If |old_tree| is non-null, then try
  // to re-use the CALayers of |old_tree| as much as possible. |old_tree| will
  // be destroyed at the end of the function, and any CALayers in it which were
  // not re-used by |this| will be removed from the CALayer hierarchy.
  void CommitScheduledCALayers(CALayer* superlayer,
                               std::unique_ptr<CARendererLayerTree> old_tree,
                               const gfx::Size& pixel_size,
                               float scale_factor);

  // Returns the contents used for a given solid color.
  id ContentsForSolidColorForTesting(SkColor4f color);

  // If there exists only a single content layer, return the IOSurface of that
  // layer.
  IOSurfaceRef GetContentIOSurface() const;

 private:
  class SolidColorContents;
  class RootLayer;
  class ClipAndSortingLayer;
  class TransformLayer;
  class ContentLayer;
  friend class ContentLayer;

  using CALayerMap =
      std::unordered_map<IOSurfaceRef, base::WeakPtr<ContentLayer>>;

  void MatchLayersToOldTree(CARendererLayerTree* old_tree);

  class RootLayer {
   public:
    RootLayer(CARendererLayerTree* tree);

    RootLayer(RootLayer&&) = delete;
    RootLayer(const RootLayer&) = delete;
    RootLayer& operator=(const RootLayer&) = delete;

    // This will remove |ca_layer| from its superlayer, if |ca_layer| is
    // non-nil.
    ~RootLayer();

    // Append a new content layer, without modifying the actual CALayer
    // structure.
    bool AddContentLayer(const CARendererLayerParams& params);

    // Downgrade all downgradeable AVSampleBufferDisplayLayers to be normal
    // CALayers.
    // https://crbug.com/923427, https://crbug.com/1143477
    void DowngradeAVLayersToCALayers();

    // Allocate CALayers for this layer and its children, and set their
    // properties appropriately. Re-use the CALayers from |old_layer| if
    // possible. If re-using a CALayer from |old_layer|, reset its |ca_layer|
    // to nil, so that its destructor will not remove an active CALayer.
    void CommitToCA(CALayer* superlayer, const gfx::Size& pixel_size);

    void CALayerFallBack();

    // Return true if the CALayer tree is just a video layer on a black or
    // transparent background, false otherwise.
    bool WantsFullscreenLowPowerBackdrop() const;

    // Tree that owns `this`.
    const raw_ptr<CARendererLayerTree> tree_;

    std::list<ClipAndSortingLayer> clip_and_sorting_layers_;
    CALayer* __strong ca_layer_;

    // Weak pointer to the layer in the old CARendererLayerTree that will be
    // reused by this layer, and the weak factory used to make that pointer.
    base::WeakPtr<RootLayer> old_layer_;
    base::WeakPtrFactory<RootLayer> weak_factory_for_new_layer_{this};
  };
  class ClipAndSortingLayer {
   public:
    ClipAndSortingLayer(RootLayer* root_layer,
                        bool is_clipped,
                        gfx::Rect clip_rect,
                        gfx::RRectF rounded_corner_bounds,
                        unsigned sorting_context_id,
                        bool is_singleton_sorting_context);

    ClipAndSortingLayer(ClipAndSortingLayer&& layer) = delete;
    ClipAndSortingLayer(const ClipAndSortingLayer&) = delete;
    ClipAndSortingLayer& operator=(const ClipAndSortingLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~ClipAndSortingLayer();
    void AddContentLayer(const CARendererLayerParams& params);

    void CommitToCA(CALayer* last_committed_clip_ca_layer);
    void CALayerFallBack();

    CARendererLayerTree* tree() { return parent_layer_->tree_; }

    // Parent layer that owns `this`, and child layers that `this` owns.
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION RootLayer* const parent_layer_ = nullptr;
    std::list<TransformLayer> transform_layers_;

    bool is_clipped_ = false;
    gfx::Rect clip_rect_;
    gfx::RRectF rounded_corner_bounds_;
    unsigned sorting_context_id_ = 0;
    bool is_singleton_sorting_context_ = false;
    CALayer* __strong clipping_ca_layer_;
    CALayer* __strong rounded_corner_ca_layer_;

    // The status when used as an old layer.
    bool ca_layer_used_ = false;

    // Weak pointer to the layer in the old CARendererLayerTree that will be
    // reused by this layer, and the weak factory used to make that pointer.
    base::WeakPtr<ClipAndSortingLayer> old_layer_;
    base::WeakPtrFactory<ClipAndSortingLayer> weak_factory_for_new_layer_{this};
  };
  class TransformLayer {
   public:
    TransformLayer(ClipAndSortingLayer* parent_layer,
                   const gfx::Transform& transform);

    TransformLayer(TransformLayer&& layer) = delete;
    TransformLayer(const TransformLayer&) = delete;
    TransformLayer& operator=(const TransformLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~TransformLayer();
    void AddContentLayer(const CARendererLayerParams& params);
    void CommitToCA(CALayer* last_committed_transform_ca_layer);

    void CALayerFallBack();

    CARendererLayerTree* tree() { return parent_layer_->tree(); }

    // Parent layer that owns `this`, and child layers that `this` owns.
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION ClipAndSortingLayer* const parent_layer_ = nullptr;
    std::list<ContentLayer> content_layers_;

    gfx::Transform transform_;
    CALayer* __strong ca_layer_;

    // The ca layer status when used as an old layer.
    bool ca_layer_used_ = false;

    // Weak pointer to the layer in the old CARendererLayerTree that will be
    // reused by this layer, and the weak factory used to make that pointer.
    base::WeakPtr<TransformLayer> old_layer_;
    base::WeakPtrFactory<TransformLayer> weak_factory_for_new_layer_{this};
  };
  class ContentLayer {
   public:
    ContentLayer(TransformLayer* parent_layer,
                 base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                 base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
                 const gfx::RectF& contents_rect,
                 const gfx::Rect& rect,
                 SkColor4f background_color,
                 const gfx::ColorSpace& color_space,
                 unsigned edge_aa_mask,
                 float opacity,
                 bool nearest_neighbor_filter,
                 const gfx::HDRMetadata& hdr_metadata,
                 gfx::ProtectedVideoType protected_video_type,
                 bool is_render_pass_draw_quad);

    ContentLayer(ContentLayer&& layer) = delete;
    ContentLayer(const ContentLayer&) = delete;
    ContentLayer& operator=(const ContentLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions.
    ~ContentLayer();
    void CommitToCA(CALayer* last_committed_ca_layer);

    CARendererLayerTree* tree() { return parent_layer_->tree(); }
    void UpdateMapAndMatchOldLayers(CALayerMap& old_ca_layer_map,
                                    int& layer_order,
                                    int& last_old_layer_order);

    // Parent layer that owns `this`.
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION TransformLayer* const parent_layer_ = nullptr;

    // Ensure that the IOSurface be marked as in-use as soon as it is received.
    // When they are committed to the window server, that will also increment
    // their use count.
    const gfx::ScopedInUseIOSurface io_surface_;
    const base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
    scoped_refptr<SolidColorContents> solid_color_contents_;
    gfx::RectF contents_rect_;
    gfx::RectF rect_;
    SkColor4f background_color_ = SkColors::kTransparent;
    // The color space of |io_surface|. Used for HDR tonemapping.
    gfx::ColorSpace io_surface_color_space_;
    // Note that the CoreAnimation edge antialiasing mask is not the same as
    // the edge antialiasing mask passed to the constructor.
    CAEdgeAntialiasingMask ca_edge_aa_mask_ = 0;
    float opacity_ = 1;
    NSString* const ca_filter_ = nil;

    CALayerType type_ = CALayerType::kDefault;

    // If |type| is CALayerType::kVideo and |video_type_can_downgrade| then
    // |type| can be downgraded to kDefault. This can be set to false for
    // HDR video (that cannot be displayed by a regular CALayer) or for
    // protected content (see https://crbug.com/1026703).
    bool video_type_can_downgrade_ = true;

    gfx::HDRMetadata hdr_metadata_;

    gfx::ProtectedVideoType protected_video_type_ =
        gfx::ProtectedVideoType::kClear;

    CALayer* __strong ca_layer_;

    // If this layer's contents can be represented as an
    // AVSampleBufferDisplayLayer, then |ca_layer| will point to |av_layer|.
    AVSampleBufferDisplayLayer* __strong av_layer_;

    // Layer used to colorize content when it updates, if borders are
    // enabled.
    CALayer* __strong update_indicator_layer_;

    // Indicate the content layer order in the whole layer tree.
    int layer_order_ = 0;

    // The status when used as an old layer.
    bool ca_layer_used_ = false;

    bool is_render_pass_draw_quad_ = false;

    // Weak pointer to the layer in the old CARendererLayerTree that will be
    // reused by this layer, and the weak factory used to make that pointer.
    base::WeakPtr<ContentLayer> old_layer_;
    base::WeakPtrFactory<ContentLayer> weak_factory_for_new_layer_{this};
  };

  RootLayer root_layer_{this};
  float scale_factor_ = 1;
  bool has_committed_ = false;
  const bool allow_av_sample_buffer_display_layer_ = true;
  const bool allow_solid_color_layers_ = true;
  float display_hdr_headroom_ = 1.f;
  id<MTLDevice> __strong metal_device_ = nil;

  // Map of content IOSurface.
  CALayerMap ca_layer_map_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_
