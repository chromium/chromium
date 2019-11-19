// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_

#include <IOSurface/IOSurface.h>
#include <QuartzCore/QuartzCore.h>

#include <memory>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"

@class AVSampleBufferDisplayLayer;

namespace ui {

struct CARendererLayerParams;

// The CARendererLayerTree will construct a hierarchy of CALayers from a linear
// list provided by the CoreAnimation renderer using the algorithm and structure
// referenced described in
// https://docs.google.com/document/d/1DtSN9zzvCF44_FQPM7ie01UxGHagQ66zfF5L9HnigQY/edit?usp=sharing
class ACCELERATED_WIDGET_MAC_EXPORT CARendererLayerTree {
 public:
  CARendererLayerTree(bool allow_av_sample_buffer_display_layer,
                      bool allow_solid_color_layers);

  // This will remove all CALayers from this tree from their superlayer.
  ~CARendererLayerTree();

  // Append the description of a new CALayer to the tree. This will not
  // create any new CALayers until CommitScheduledCALayers is called. This
  // cannot be called anymore after CommitScheduledCALayers has been called.
  bool ScheduleCALayer(const CARendererLayerParams& params);

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
  id ContentsForSolidColorForTesting(unsigned int color);

  // If there exists only a single content layer, return the IOSurface of that
  // layer.
  IOSurfaceRef GetContentIOSurface() const;

 private:
  class SolidColorContents;
  struct RootLayer;
  struct ClipAndSortingLayer;
  struct TransformLayer;
  struct ContentLayer;
  friend struct ContentLayer;

  struct RootLayer {
    RootLayer();

    // This will remove |ca_layer| from its superlayer, if |ca_layer| is
    // non-nil.
    ~RootLayer();

    // Append a new content layer, without modifying the actual CALayer
    // structure.
    bool AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);

    // Workaround for https://crbug.com/923427. Only allow any
    // AVSampleBufferDisplayLayer if there is exactly one video quad.
    void EnforceOnlyOneAVLayer();

    // Allocate CALayers for this layer and its children, and set their
    // properties appropriately. Re-use the CALayers from |old_layer| if
    // possible. If re-using a CALayer from |old_layer|, reset its |ca_layer|
    // to nil, so that its destructor will not remove an active CALayer.
    void CommitToCA(CALayer* superlayer,
                    RootLayer* old_layer,
                    const gfx::Size& pixel_size,
                    float scale_factor);

    // Return true if the CALayer tree is just a video layer on a black or
    // transparent background, false otherwise.
    bool WantsFullcreenLowPowerBackdrop() const;

    std::vector<ClipAndSortingLayer> clip_and_sorting_layers;
    base::scoped_nsobject<CALayer> ca_layer;

   private:
    DISALLOW_COPY_AND_ASSIGN(RootLayer);
  };
  struct ClipAndSortingLayer {
    ClipAndSortingLayer(bool is_clipped,
                        gfx::Rect clip_rect,
                        gfx::RRectF rounded_corner_bounds,
                        unsigned sorting_context_id,
                        bool is_singleton_sorting_context);
    ClipAndSortingLayer(ClipAndSortingLayer&& layer);

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~ClipAndSortingLayer();
    void AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);
    void CommitToCA(CALayer* superlayer,
                    ClipAndSortingLayer* old_layer,
                    float scale_factor);

    std::vector<TransformLayer> transform_layers;
    bool is_clipped = false;
    gfx::Rect clip_rect;
    gfx::RRectF rounded_corner_bounds;
    unsigned sorting_context_id = 0;
    bool is_singleton_sorting_context = false;
    base::scoped_nsobject<CALayer> clipping_ca_layer;
    base::scoped_nsobject<CALayer> rounded_corner_ca_layer;

   private:
    DISALLOW_COPY_AND_ASSIGN(ClipAndSortingLayer);
  };
  struct TransformLayer {
    TransformLayer(const gfx::Transform& transform);
    TransformLayer(TransformLayer&& layer);

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~TransformLayer();
    void AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);
    void CommitToCA(CALayer* superlayer,
                    TransformLayer* old_layer,
                    float scale_factor);

    gfx::Transform transform;
    std::vector<ContentLayer> content_layers;
    base::scoped_nsobject<CALayer> ca_layer;

   private:
    DISALLOW_COPY_AND_ASSIGN(TransformLayer);
  };
  struct ContentLayer {
    ContentLayer(CARendererLayerTree* tree,
                 base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                 base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
                 const gfx::RectF& contents_rect,
                 const gfx::Rect& rect,
                 unsigned background_color,
                 bool triggers_hdr,
                 unsigned edge_aa_mask,
                 float opacity,
                 unsigned filter);
    ContentLayer(ContentLayer&& layer);

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~ContentLayer();
    void CommitToCA(CALayer* parent,
                    ContentLayer* old_layer,
                    float scale_factor);

    // Ensure that the IOSurface be marked as in-use as soon as it is received.
    // When they are committed to the window server, that will also increment
    // their use count.
    const gfx::ScopedInUseIOSurface io_surface;
    const base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
    scoped_refptr<SolidColorContents> solid_color_contents;
    gfx::RectF contents_rect;
    gfx::RectF rect;
    unsigned background_color = 0;
    const bool triggers_hdr;
    // Note that the CoreAnimation edge antialiasing mask is not the same as
    // the edge antialiasing mask passed to the constructor.
    CAEdgeAntialiasingMask ca_edge_aa_mask = 0;
    float opacity = 1;
    NSString* const ca_filter = nil;
    base::scoped_nsobject<CALayer> ca_layer;

    // If this layer's contents can be represented as an
    // AVSampleBufferDisplayLayer, then |ca_layer| will point to |av_layer|.
    base::scoped_nsobject<AVSampleBufferDisplayLayer> av_layer;
    bool use_av_layer = false;

   private:
    DISALLOW_COPY_AND_ASSIGN(ContentLayer);
  };

  RootLayer root_layer_;
  float scale_factor_ = 1;
  bool has_committed_ = false;
  const bool allow_av_sample_buffer_display_layer_ = true;
  const bool allow_solid_color_layers_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(CARendererLayerTree);
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_MAC_H_
