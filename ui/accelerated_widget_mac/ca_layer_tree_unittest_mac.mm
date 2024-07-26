// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import <AVFoundation/AVFoundation.h>

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/ca_renderer_layer_params.h"

@interface CALayer (Private)
@property BOOL wantsExtendedDynamicRangeContent;
@end

namespace gpu {

namespace {

struct CALayerProperties {
  CALayerProperties() = default;
  ~CALayerProperties() = default;

  bool is_clipped = true;
  gfx::Rect clip_rect;
  gfx::RRectF rounded_corner_bounds;
  int sorting_context_id = 0;
  gfx::Transform transform;
  gfx::RectF contents_rect = gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f);
  gfx::Rect rect = gfx::Rect(0, 0, 256, 256);
  SkColor4f background_color = SkColors::kWhite;
  unsigned edge_aa_mask = 0;
  float opacity = 1.0f;
  float scale_factor = 1.0f;
  unsigned filter = GL_LINEAR;
  gfx::ScopedIOSurface io_surface;
  gfx::ColorSpace color_space;
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;

  bool allow_av_layers = true;
  bool allow_solid_color_layers = true;
};

base::apple::ScopedCFTypeRef<CVPixelBufferRef> CreateCVPixelBuffer(
    gfx::ScopedIOSurface io_surface) {
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  CVPixelBufferCreateWithIOSurface(nullptr, io_surface.get(), nullptr,
                                   cv_pixel_buffer.InitializeInto());
  return cv_pixel_buffer;
}

bool ScheduleCALayer(ui::CARendererLayerTree* tree,
                     CALayerProperties* properties) {
  gfx::ScopedIOSurface io_surface;
  gfx::ColorSpace io_surface_color_space;
  if (properties->io_surface) {
    io_surface = properties->io_surface;
    io_surface_color_space = properties->color_space;
  }
  return tree->ScheduleCALayer(ui::CARendererLayerParams(
      properties->is_clipped, properties->clip_rect,
      properties->rounded_corner_bounds, properties->sorting_context_id,
      properties->transform, io_surface, io_surface_color_space,
      properties->contents_rect, properties->rect, properties->background_color,
      properties->edge_aa_mask, properties->opacity, properties->filter,
      gfx::HDRMetadata(), gfx::ProtectedVideoType::kClear, false));
}

void UpdateCALayerTree(std::unique_ptr<ui::CARendererLayerTree>& ca_layer_tree,
                       CALayerProperties* properties,
                       CALayer* superlayer) {
  std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
      new ui::CARendererLayerTree(properties->allow_av_layers,
                                  properties->allow_solid_color_layers));
  bool result = ScheduleCALayer(new_ca_layer_tree.get(), properties);
  EXPECT_TRUE(result);
  new_ca_layer_tree->CommitScheduledCALayers(
      superlayer, std::move(ca_layer_tree), properties->rect.size(),
      properties->scale_factor);
  std::swap(new_ca_layer_tree, ca_layer_tree);
}

}  // namespace

class CALayerTreeTest : public testing::Test {
 protected:
  void SetUp() override { superlayer_ = [[CALayer alloc] init]; }
  // Traverse the tree. Validate that there exists only one content layer, and
  // return that layer.
  CALayer* GetOnlyContentLayer() {
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    CALayer* transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    return [transform_layer sublayers][0];
  }
  CALayer* __strong superlayer_;
};

// Test updating each layer's properties.
class CALayerTreePropertyUpdatesTest : public CALayerTreeTest {
 public:
  void RunTest(bool allow_solid_color_layers) {
    CALayerProperties properties;
    properties.allow_solid_color_layers = allow_solid_color_layers;
    properties.clip_rect = gfx::Rect(2, 4, 8, 16);
    properties.rounded_corner_bounds = gfx::RRectF(2, 4, 8, 16, 13);
    properties.transform.Translate(10, 20);
    properties.contents_rect = gfx::RectF(0.0f, 0.25f, 0.5f, 0.75f);
    properties.rect = gfx::Rect(16, 32, 64, 128);
    properties.background_color = SkColors::kRed;
    properties.edge_aa_mask = ui::CALayerEdge::kLayerEdgeLeft;
    properties.opacity = 0.5f;
    properties.io_surface =
        gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);

    std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
    CALayer* root_layer = nil;
    CALayer* clip_and_sorting_layer = nil;
    CALayer* rounded_rect_layer = nil;
    CALayer* transform_layer = nil;
    CALayer* content_layer = nil;

    // Validate the initial values.
    {
      std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
          new ui::CARendererLayerTree(true, allow_solid_color_layers));

      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      root_layer = [superlayer_ sublayers][0];
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      clip_and_sorting_layer = [root_layer sublayers][0];
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);

      CALayer* superlayer_for_transform = clip_and_sorting_layer;
      if (!properties.rounded_corner_bounds.IsEmpty()) {
        rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
        EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
        superlayer_for_transform = rounded_rect_layer;
      }
      transform_layer = [superlayer_for_transform sublayers][0];
      EXPECT_EQ(1u, [[transform_layer sublayers] count]);
      content_layer = [transform_layer sublayers][0];

      // Validate the clip and sorting context layer.
      EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
      EXPECT_EQ(gfx::Rect(properties.clip_rect.size()),
                gfx::Rect([clip_and_sorting_layer bounds]));
      EXPECT_EQ(properties.rounded_corner_bounds.GetSimpleRadius(),
                [rounded_rect_layer cornerRadius]);
      EXPECT_EQ(properties.clip_rect.origin(),
                gfx::Point([clip_and_sorting_layer position]));
      EXPECT_EQ(-properties.clip_rect.origin().x(),
                [clip_and_sorting_layer sublayerTransform].m41);
      EXPECT_EQ(-properties.clip_rect.origin().y(),
                [clip_and_sorting_layer sublayerTransform].m42);

      // Validate the transform layer.
      EXPECT_EQ(properties.transform.rc(3, 0),
                [transform_layer sublayerTransform].m41);
      EXPECT_EQ(properties.transform.rc(3, 1),
                [transform_layer sublayerTransform].m42);

      // Validate the content layer.
      EXPECT_EQ((__bridge id)properties.io_surface.get(),
                [content_layer contents]);
      EXPECT_EQ(properties.contents_rect,
                gfx::RectF([content_layer contentsRect]));
      EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
      EXPECT_EQ(gfx::Rect(properties.rect.size()),
                gfx::Rect([content_layer bounds]));
      EXPECT_EQ(kCALayerLeftEdge, [content_layer edgeAntialiasingMask]);
      EXPECT_EQ(properties.opacity, [content_layer opacity]);
      EXPECT_NSEQ(kCAFilterNearest, [content_layer minificationFilter]);
      EXPECT_NSEQ(kCAFilterNearest, [content_layer magnificationFilter]);
      EXPECT_EQ(properties.scale_factor, [content_layer contentsScale]);
    }

    // Update just the clip rect and re-commit.
    {
      properties.clip_rect = gfx::Rect(4, 8, 16, 32);
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);

      // Validate the clip and sorting context layer.
      EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
      EXPECT_EQ(gfx::Rect(properties.clip_rect.size()),
                gfx::Rect([clip_and_sorting_layer bounds]));
      EXPECT_EQ(properties.clip_rect.origin(),
                gfx::Point([clip_and_sorting_layer position]));
      EXPECT_EQ(-properties.clip_rect.origin().x(),
                [clip_and_sorting_layer sublayerTransform].m41);
      EXPECT_EQ(-properties.clip_rect.origin().y(),
                [clip_and_sorting_layer sublayerTransform].m42);
    }

    // Disable clipping and re-commit.
    {
      properties.is_clipped = false;
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);

      // Validate the clip and sorting context layer.
      EXPECT_FALSE([clip_and_sorting_layer masksToBounds]);
      EXPECT_EQ(gfx::Rect(), gfx::Rect([clip_and_sorting_layer bounds]));
      EXPECT_EQ(gfx::Point(), gfx::Point([clip_and_sorting_layer position]));
      EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m41);
      EXPECT_EQ(0.0, [clip_and_sorting_layer sublayerTransform].m42);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    }

    // Change the transform and re-commit.
    {
      properties.transform.Translate(5, 5);
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);

      // Validate the transform layer.
      EXPECT_EQ(properties.transform.rc(3, 0),
                [transform_layer sublayerTransform].m41);
      EXPECT_EQ(properties.transform.rc(3, 1),
                [transform_layer sublayerTransform].m42);
    }

    // Change the edge antialiasing mask and commit.
    {
      properties.edge_aa_mask = ui::CALayerEdge::kLayerEdgeTop;
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer. Note that top and bottom edges flip.
      EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
    }

    // Change the contents and commit.
    {
      properties.io_surface = gfx::ScopedIOSurface();
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer. Note that edge anti-aliasing does not flip
      // for solid colors.
      if (allow_solid_color_layers) {
        EXPECT_EQ(nil, [content_layer contents]);
        EXPECT_EQ(kCALayerTopEdge, [content_layer edgeAntialiasingMask]);
      } else {
        EXPECT_EQ(ca_layer_tree->ContentsForSolidColorForTesting(
                      properties.background_color),
                  [content_layer contents]);
        EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
      }
    }

    // Change the rect size.
    {
      properties.rect = gfx::Rect(properties.rect.origin(), gfx::Size(32, 16));
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer.
      EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
      EXPECT_EQ(gfx::Rect(properties.rect.size()),
                gfx::Rect([content_layer bounds]));
    }

    // Change the rect position.
    {
      properties.rect = gfx::Rect(gfx::Point(16, 4), properties.rect.size());
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer.
      EXPECT_EQ(properties.rect.origin(), gfx::Point([content_layer position]));
      EXPECT_EQ(gfx::Rect(properties.rect.size()),
                gfx::Rect([content_layer bounds]));
    }

    // Change the opacity.
    {
      properties.opacity = 1.0f;
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer.
      EXPECT_EQ(properties.opacity, [content_layer opacity]);
    }

    // Change the filter.
    {
      properties.filter = GL_NEAREST;
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer.
      EXPECT_NSEQ(kCAFilterNearest, [content_layer minificationFilter]);
      EXPECT_NSEQ(kCAFilterNearest, [content_layer magnificationFilter]);
    }

    // Add the clipping and IOSurface contents back.
    {
      properties.is_clipped = true;
      properties.io_surface = gfx::CreateIOSurface(
          gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);

      // Validate the content layer.
      EXPECT_EQ((__bridge id)properties.io_surface.get(),
                [content_layer contents]);
      EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
    }

    // Change the scale factor. This should result in a new tree being created.
    {
      properties.scale_factor = 2.0f;
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_NE(root_layer, [superlayer_ sublayers][0]);
      root_layer = [superlayer_ sublayers][0];
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_NE(clip_and_sorting_layer, [root_layer sublayers][0]);
      clip_and_sorting_layer = [root_layer sublayers][0];
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);

      EXPECT_NE(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      rounded_rect_layer = [clip_and_sorting_layer sublayers][0];

      // Under a 2.0 scale factor, the corner-radius should be halved.
      EXPECT_EQ(properties.rounded_corner_bounds.GetSimpleRadius() / 2.0f,
                [rounded_rect_layer cornerRadius]);

      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_NE(transform_layer, [clip_and_sorting_layer sublayers][0]);
      transform_layer = [rounded_rect_layer sublayers][0];
      EXPECT_EQ(1u, [[transform_layer sublayers] count]);
      EXPECT_NE(content_layer, [transform_layer sublayers][0]);
      content_layer = [transform_layer sublayers][0];

      // Validate the clip and sorting context layer.
      EXPECT_TRUE([clip_and_sorting_layer masksToBounds]);
      EXPECT_EQ(
          gfx::ToFlooredRectDeprecated(gfx::ConvertRectToDips(
              gfx::Rect(properties.clip_rect.size()), properties.scale_factor)),
          gfx::Rect([clip_and_sorting_layer bounds]));
      EXPECT_EQ(gfx::ToFlooredPoint(gfx::ConvertPointToDips(
                    properties.clip_rect.origin(), properties.scale_factor)),
                gfx::Point([clip_and_sorting_layer position]));
      EXPECT_EQ(-properties.clip_rect.origin().x() / properties.scale_factor,
                [clip_and_sorting_layer sublayerTransform].m41);
      EXPECT_EQ(-properties.clip_rect.origin().y() / properties.scale_factor,
                [clip_and_sorting_layer sublayerTransform].m42);

      // Validate the transform layer.
      EXPECT_EQ(properties.transform.rc(3, 0) / properties.scale_factor,
                [transform_layer sublayerTransform].m41);
      EXPECT_EQ(properties.transform.rc(3, 1) / properties.scale_factor,
                [transform_layer sublayerTransform].m42);

      // Validate the content layer.
      EXPECT_EQ((__bridge id)properties.io_surface.get(),
                [content_layer contents]);
      EXPECT_EQ(properties.contents_rect,
                gfx::RectF([content_layer contentsRect]));
      EXPECT_EQ(gfx::ToFlooredPoint(gfx::ConvertPointToDips(
                    properties.rect.origin(), properties.scale_factor)),
                gfx::Point([content_layer position]));
      EXPECT_EQ(
          gfx::ToFlooredRectDeprecated(gfx::ConvertRectToDips(
              gfx::Rect(properties.rect.size()), properties.scale_factor)),
          gfx::Rect([content_layer bounds]));
      EXPECT_EQ(kCALayerBottomEdge, [content_layer edgeAntialiasingMask]);
      EXPECT_EQ(properties.opacity, [content_layer opacity]);
      EXPECT_EQ(properties.scale_factor, [content_layer contentsScale]);
    }

    // Remove the rounded corners. This should result in the rounded corners
    // being removed on that layer.
    {
      properties.rounded_corner_bounds = gfx::RRectF();
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(0, [rounded_rect_layer cornerRadius]);
      EXPECT_FALSE([rounded_rect_layer masksToBounds]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(1u, [[transform_layer sublayers] count]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);
    }

    {
      // A no-op update should not invalidate any of the layers.
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(1u, [[transform_layer sublayers] count]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);
    }

    // Re-add rounded corners.
    {
      properties.rounded_corner_bounds = gfx::RRectF(1, 2, 3, 4, 5);
      UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

      // Validate the tree structure.
      EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
      EXPECT_EQ(root_layer, [superlayer_ sublayers][0]);
      EXPECT_EQ(1u, [[root_layer sublayers] count]);
      EXPECT_EQ(clip_and_sorting_layer, [root_layer sublayers][0]);
      EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
      EXPECT_EQ(rounded_rect_layer, [clip_and_sorting_layer sublayers][0]);
      // Under a 2.0 scale factor, the corer-radius should be halved.
      EXPECT_EQ(properties.rounded_corner_bounds.GetSimpleRadius() / 2.0f,
                [rounded_rect_layer cornerRadius]);
      EXPECT_TRUE([rounded_rect_layer masksToBounds]);
      EXPECT_EQ(transform_layer, [rounded_rect_layer sublayers][0]);
      EXPECT_EQ(1u, [[transform_layer sublayers] count]);
      EXPECT_EQ(content_layer, [transform_layer sublayers][0]);
    }
  }
};

TEST_F(CALayerTreePropertyUpdatesTest, AllowSolidColors) {
  RunTest(true);
}

TEST_F(CALayerTreePropertyUpdatesTest, DisallowSolidColors) {
  RunTest(false);
}

// Verify that sorting context zero is split at non-flat transforms.
TEST_F(CALayerTreeTest, SplitSortingContextZero) {
  CALayerProperties properties;
  properties.is_clipped = false;
  properties.clip_rect = gfx::Rect();
  properties.rect = gfx::Rect(0, 0, 256, 256);

  // We'll use the IOSurface contents to identify the content layers.
  gfx::ScopedIOSurface io_surfaces[5];
  for (size_t i = 0; i < 5; ++i) {
    io_surfaces[i] =
        gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);
  }

  // Have 5 transforms:
  // * 2 flat but different (1 sorting context layer, 2 transform layers)
  // * 1 non-flat (new sorting context layer)
  // * 2 flat and the same (new sorting context layer, 1 transform layer)
  gfx::Transform transforms[5];
  transforms[0].Translate(10, 10);
  transforms[1].RotateAboutZAxis(45.0f);
  transforms[2].RotateAboutYAxis(45.0f);
  transforms[3].Translate(10, 10);
  transforms[4].Translate(10, 10);

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree(true, true));
  for (size_t i = 0; i < 5; ++i) {
    properties.io_surface = io_surfaces[i];
    properties.transform = transforms[i];
    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(
      superlayer_, nullptr, properties.rect.size(), properties.scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [superlayer_ sublayers][0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [root_layer sublayers][0];
  CALayer* clip_and_sorting_layer_1 = [root_layer sublayers][1];
  CALayer* clip_and_sorting_layer_2 = [root_layer sublayers][2];
  CALayer* rounded_rect_layer_0 = [clip_and_sorting_layer_0 sublayers][0];
  CALayer* rounded_rect_layer_1 = [clip_and_sorting_layer_1 sublayers][0];
  CALayer* rounded_rect_layer_2 = [clip_and_sorting_layer_2 sublayers][0];

  // Validate that the first sorting context has 2 transform layers each with
  // one content layer.
  EXPECT_EQ(2u, [[rounded_rect_layer_0 sublayers] count]);
  CALayer* transform_layer_0_0 = [rounded_rect_layer_0 sublayers][0];
  CALayer* transform_layer_0_1 = [rounded_rect_layer_0 sublayers][1];
  EXPECT_EQ(1u, [[transform_layer_0_0 sublayers] count]);
  CALayer* content_layer_0 = [transform_layer_0_0 sublayers][0];
  EXPECT_EQ(1u, [[transform_layer_0_1 sublayers] count]);
  CALayer* content_layer_1 = [transform_layer_0_1 sublayers][0];

  // Validate that the second sorting context has 1 transform layer with one
  // content layer.
  EXPECT_EQ(1u, [[rounded_rect_layer_1 sublayers] count]);
  CALayer* transform_layer_1_0 = [rounded_rect_layer_1 sublayers][0];
  EXPECT_EQ(1u, [[transform_layer_1_0 sublayers] count]);
  CALayer* content_layer_2 = [transform_layer_1_0 sublayers][0];

  // Validate that the third sorting context has 1 transform layer with two
  // content layers.
  EXPECT_EQ(1u, [[rounded_rect_layer_2 sublayers] count]);
  CALayer* transform_layer_2_0 = [rounded_rect_layer_2 sublayers][0];
  EXPECT_EQ(2u, [[transform_layer_2_0 sublayers] count]);
  CALayer* content_layer_3 = [transform_layer_2_0 sublayers][0];
  CALayer* content_layer_4 = [transform_layer_2_0 sublayers][1];

  // Validate that the layers come out in order.
  EXPECT_EQ((__bridge id)io_surfaces[0].get(), [content_layer_0 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[1].get(), [content_layer_1 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[2].get(), [content_layer_2 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[3].get(), [content_layer_3 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[4].get(), [content_layer_4 contents]);
}

// Verify that sorting contexts are allocated appropriately.
TEST_F(CALayerTreeTest, SortingContexts) {
  CALayerProperties properties;
  properties.is_clipped = false;
  properties.clip_rect = gfx::Rect();
  properties.rect = gfx::Rect(0, 0, 256, 256);

  // We'll use the IOSurface contents to identify the content layers.
  gfx::ScopedIOSurface io_surfaces[3];
  for (size_t i = 0; i < 3; ++i) {
    io_surfaces[i] =
        gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);
  }

  int sorting_context_ids[3] = {3, -1, 0};

  // Schedule and commit the layers.
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree(true, true));
  for (size_t i = 0; i < 3; ++i) {
    properties.sorting_context_id = sorting_context_ids[i];
    properties.io_surface = io_surfaces[i];
    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
  ca_layer_tree->CommitScheduledCALayers(
      superlayer_, nullptr, properties.rect.size(), properties.scale_factor);

  // Validate the root layer.
  EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
  CALayer* root_layer = [superlayer_ sublayers][0];

  // Validate that we have 3 sorting context layers.
  EXPECT_EQ(3u, [[root_layer sublayers] count]);
  CALayer* clip_and_sorting_layer_0 = [root_layer sublayers][0];
  CALayer* clip_and_sorting_layer_1 = [root_layer sublayers][1];
  CALayer* clip_and_sorting_layer_2 = [root_layer sublayers][2];
  CALayer* rounded_rect_layer_0 = [clip_and_sorting_layer_0 sublayers][0];
  CALayer* rounded_rect_layer_1 = [clip_and_sorting_layer_1 sublayers][0];
  CALayer* rounded_rect_layer_2 = [clip_and_sorting_layer_2 sublayers][0];

  // Validate that each sorting context has 1 transform layer.
  EXPECT_EQ(1u, [[rounded_rect_layer_0 sublayers] count]);
  CALayer* transform_layer_0 = [rounded_rect_layer_0 sublayers][0];
  EXPECT_EQ(1u, [[rounded_rect_layer_1 sublayers] count]);
  CALayer* transform_layer_1 = [rounded_rect_layer_1 sublayers][0];
  EXPECT_EQ(1u, [[rounded_rect_layer_2 sublayers] count]);
  CALayer* transform_layer_2 = [rounded_rect_layer_2 sublayers][0];

  // Validate that each transform has 1 content layer.
  EXPECT_EQ(1u, [[transform_layer_0 sublayers] count]);
  CALayer* content_layer_0 = [transform_layer_0 sublayers][0];
  EXPECT_EQ(1u, [[transform_layer_1 sublayers] count]);
  CALayer* content_layer_1 = [transform_layer_1 sublayers][0];
  EXPECT_EQ(1u, [[transform_layer_2 sublayers] count]);
  CALayer* content_layer_2 = [transform_layer_2 sublayers][0];

  // Validate that the layers come out in order.
  EXPECT_EQ((__bridge id)io_surfaces[0].get(), [content_layer_0 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[1].get(), [content_layer_1 contents]);
  EXPECT_EQ((__bridge id)io_surfaces[2].get(), [content_layer_2 contents]);
}

// Verify that sorting contexts must all have the same clipping properties.
TEST_F(CALayerTreeTest, SortingContextMustHaveConsistentClip) {
  CALayerProperties properties;

  // Vary the clipping parameters within sorting contexts.
  bool is_clippeds[3] = { true, true, false};
  gfx::Rect clip_rects[3] = {
      gfx::Rect(0, 0, 16, 16),
      gfx::Rect(4, 8, 16, 32),
      gfx::Rect(0, 0, 16, 16)
  };

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree(
      new ui::CARendererLayerTree(true, true));
  // First send the various clip parameters to sorting context zero. This is
  // legitimate.
  for (size_t i = 0; i < 3; ++i) {
    properties.is_clipped = is_clippeds[i];
    properties.clip_rect = clip_rects[i];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }

  // Next send the various clip parameters to a non-zero sorting context. This
  // will fail when we try to change the clip within the sorting context.
  for (size_t i = 0; i < 3; ++i) {
    properties.sorting_context_id = 3;
    properties.is_clipped = is_clippeds[i];
    properties.clip_rect = clip_rects[i];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    if (i == 0)
      EXPECT_TRUE(result);
    else
      EXPECT_FALSE(result);
  }
  // Try once more with the original clip and verify it works.
  {
    properties.is_clipped = is_clippeds[0];
    properties.clip_rect = clip_rects[0];

    bool result = ScheduleCALayer(ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
  }
}

// Test updating each layer's properties.
TEST_F(CALayerTreeTest, AVLayer) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({ui::kFullscreenLowPowerBackdropMac}, {});

  CALayerProperties properties;
  properties.io_surface =
      gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* content_layer_old = nil;
  CALayer* content_layer_new = nil;

  // Validate the initial values.
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_FALSE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
  }
  content_layer_old = content_layer_new;

  // Pass a YUV 420 frame. This will become an AVSampleBufferDisplayLayer
  // because it is in fullscreen low power mode.
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR);
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_TRUE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer_new, content_layer_old);
  }
  content_layer_old = content_layer_new;

  // Pass a similar frame. Nothing should change.
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(256, 128), gfx::BufferFormat::YUV_420_BIPLANAR);
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_TRUE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_EQ(content_layer_new, content_layer_old);
  }
  content_layer_old = content_layer_new;

  // Break fullscreen low power mode by changing opacity. This should cause
  // us to drop out of using AVSampleBufferDisplayLayer.
  properties.opacity = 0.9;
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_FALSE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer_new, content_layer_old);
  }
  content_layer_old = content_layer_new;

  // Now try a P010 frame. Because this may be HDR, we should jump back to
  // having an AVSampleBufferDisplayLayer.
  properties.io_surface =
      gfx::CreateIOSurface(gfx::Size(128, 256), gfx::BufferFormat::P010);
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_TRUE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer_new, content_layer_old);
  }
  content_layer_old = content_layer_new;

  // Go back to testing AVSampleBufferLayer and fullscreen low power.
  properties.opacity = 1.0;

  // Pass a frame with a CVPixelBuffer which, when scaled down, will have a
  // fractional dimension.
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(513, 512), gfx::BufferFormat::YUV_420_BIPLANAR);
  properties.cv_pixel_buffer = CreateCVPixelBuffer(properties.io_surface);
  properties.color_space = gfx::ColorSpace::CreateREC709();
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();

    // Validate that the layer's size is adjusted to include the fractional
    // width, which works around a macOS bug (https://crbug.com/792632).
    CGSize layer_size = content_layer_new.bounds.size;
    EXPECT_EQ(256.5, layer_size.width);
    EXPECT_EQ(256, layer_size.height);
  }
  content_layer_old = content_layer_new;

  // Pass a frame that is clipped.
  properties.contents_rect = gfx::RectF(0, 0, 1, 0.9);
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR);
  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);
    content_layer_new = GetOnlyContentLayer();
    EXPECT_FALSE([content_layer_new
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer_new, content_layer_old);
  }
  content_layer_old = content_layer_new;
}

// Ensure that blocklisting AVSampleBufferDisplayLayer works.
TEST_F(CALayerTreeTest, AVLayerBlocklist) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({ui::kFullscreenLowPowerBackdropMac}, {});

  CALayerProperties properties;
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR);

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;
  CALayer* root_layer = nil;
  CALayer* clip_and_sorting_layer = nil;
  CALayer* rounded_rect_layer = nil;
  CALayer* transform_layer = nil;
  CALayer* content_layer1 = nil;
  CALayer* content_layer2 = nil;

  {
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer1 = [transform_layer sublayers][0];

    // Validate the content layer.
    EXPECT_TRUE([content_layer1
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
  }

  {
    properties.allow_av_layers = false;
    UpdateCALayerTree(ca_layer_tree, &properties, superlayer_);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);
    content_layer2 = [transform_layer sublayers][0];

    // Validate the content layer.
    EXPECT_FALSE([content_layer2
        isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")]);
    EXPECT_NE(content_layer1, content_layer2);
  }
}

// Test fullscreen low power detection.
TEST_F(CALayerTreeTest, FullscreenLowPower) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({ui::kFullscreenLowPowerBackdropMac}, {});

  CALayerProperties properties;
  properties.io_surface = gfx::CreateIOSurface(
      gfx::Size(256, 256), gfx::BufferFormat::YUV_420_BIPLANAR);
  properties.cv_pixel_buffer = CreateCVPixelBuffer(properties.io_surface);
  properties.color_space = gfx::ColorSpace::CreateREC709();
  properties.is_clipped = false;

  CALayerProperties properties_black;
  properties_black.is_clipped = false;
  properties_black.background_color = SkColors::kBlack;
  CALayerProperties properties_white;
  properties_white.is_clipped = false;
  properties_white.background_color = SkColors::kWhite;

  std::unique_ptr<ui::CARendererLayerTree> ca_layer_tree;

  // Test a configuration with no background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree(true, true));
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.rect.size(),
        properties.scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    CALayer* transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(1u, [[transform_layer sublayers] count]);

    // Validate the content layer and fullscreen low power mode.
    EXPECT_FALSE(CGRectEqualToRect([root_layer frame], CGRectZero));
    EXPECT_NE([root_layer backgroundColor], nil);
  }

  // Test a configuration with a black background.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree(true, true));
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_black);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.rect.size(),
        properties.scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    CALayer* transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);

    // Validate the content layer and fullscreen low power mode.
    EXPECT_FALSE(CGRectEqualToRect([root_layer frame], CGRectZero));
    EXPECT_NE([root_layer backgroundColor], nil);
  }

  // Test a configuration with a white background. It will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree(true, true));
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_white);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.rect.size(),
        properties.scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    CALayer* transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE(CGRectEqualToRect([root_layer frame], CGRectZero));
    EXPECT_EQ([root_layer backgroundColor], nil);
  }

  // Test a configuration with a black foreground. It too will fail.
  {
    std::unique_ptr<ui::CARendererLayerTree> new_ca_layer_tree(
        new ui::CARendererLayerTree(true, true));
    bool result = ScheduleCALayer(new_ca_layer_tree.get(), &properties);
    EXPECT_TRUE(result);
    result = ScheduleCALayer(new_ca_layer_tree.get(), &properties_black);
    EXPECT_TRUE(result);
    new_ca_layer_tree->CommitScheduledCALayers(
        superlayer_, std::move(ca_layer_tree), properties.rect.size(),
        properties.scale_factor);
    std::swap(new_ca_layer_tree, ca_layer_tree);

    // Validate the tree structure.
    EXPECT_EQ(1u, [[superlayer_ sublayers] count]);
    CALayer* root_layer = [superlayer_ sublayers][0];
    EXPECT_EQ(1u, [[root_layer sublayers] count]);
    CALayer* clip_and_sorting_layer = [root_layer sublayers][0];
    EXPECT_EQ(1u, [[clip_and_sorting_layer sublayers] count]);
    CALayer* rounded_rect_layer = [clip_and_sorting_layer sublayers][0];
    EXPECT_EQ(1u, [[rounded_rect_layer sublayers] count]);
    CALayer* transform_layer = [rounded_rect_layer sublayers][0];
    EXPECT_EQ(2u, [[transform_layer sublayers] count]);

    // Validate the content layer and fullscreen low power mode.
    EXPECT_TRUE(CGRectEqualToRect([root_layer frame], CGRectZero));
    EXPECT_EQ([root_layer backgroundColor], nil);
  }
}

// Verify that HDR is triggered appropriately.
TEST_F(CALayerTreeTest, HDRTrigger) {
  std::unique_ptr<ui::CARendererLayerTree> ca_layer_trees[4]{
      std::make_unique<ui::CARendererLayerTree>(true, true),
      std::make_unique<ui::CARendererLayerTree>(true, true),
      std::make_unique<ui::CARendererLayerTree>(true, true),
      std::make_unique<ui::CARendererLayerTree>(true, true),
  };
  CALayerProperties properties;
  properties.is_clipped = false;
  properties.clip_rect = gfx::Rect();
  properties.rect = gfx::Rect(0, 0, 256, 256);
  bool result = false;

  // We only copy images that have both high-bit-depth and an HDR color space.
  auto sdr_image =
      gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);
  auto tricky_sdr_image =
      gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::BGRA_8888);
  auto hdr_image =
      gfx::CreateIOSurface(gfx::Size(256, 256), gfx::BufferFormat::RGBA_F16);

  // Schedule and commit the HDR layer.
  properties.io_surface = hdr_image;
  properties.color_space = gfx::ColorSpace::CreateExtendedSRGB();
  result = ScheduleCALayer(ca_layer_trees[0].get(), &properties);
  EXPECT_TRUE(result);
  ca_layer_trees[0]->CommitScheduledCALayers(
      superlayer_, nullptr, properties.rect.size(), properties.scale_factor);

  // Validate that the root layer has is triggering HDR.
  CALayer* content_layer = GetOnlyContentLayer();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  EXPECT_TRUE([content_layer wantsExtendedDynamicRangeContent]);
#pragma clang diagnostic pop

  // Commit the SDR layer.
  properties.io_surface = sdr_image;
  properties.color_space = gfx::ColorSpace::CreateSRGB();
  result = ScheduleCALayer(ca_layer_trees[1].get(), &properties);
  EXPECT_TRUE(result);
  ca_layer_trees[1]->CommitScheduledCALayers(
      superlayer_, std::move(ca_layer_trees[0]), properties.rect.size(),
      properties.scale_factor);

  // Validate that HDR is off. The previous content layer should have been
  // un-parented.
  EXPECT_EQ([content_layer superlayer], nil);
  content_layer = GetOnlyContentLayer();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  EXPECT_FALSE([content_layer wantsExtendedDynamicRangeContent]);
#pragma clang diagnostic pop

  // Commit the tricky SDR layer.
  properties.io_surface = tricky_sdr_image;
  properties.color_space = gfx::ColorSpace::CreateExtendedSRGB();
  result = ScheduleCALayer(ca_layer_trees[2].get(), &properties);
  EXPECT_TRUE(result);
  ca_layer_trees[2]->CommitScheduledCALayers(
      superlayer_, std::move(ca_layer_trees[1]), properties.rect.size(),
      properties.scale_factor);

  // Validate that HDR is still off, and that the content layer hasn't changed.
  EXPECT_EQ(content_layer, GetOnlyContentLayer());
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  EXPECT_FALSE([content_layer wantsExtendedDynamicRangeContent]);
#pragma clang diagnostic pop

  // Commit the HDR layer.
  properties.io_surface = hdr_image;
  properties.color_space = gfx::ColorSpace::CreateExtendedSRGB();
  result = ScheduleCALayer(ca_layer_trees[3].get(), &properties);
  EXPECT_TRUE(result);
  ca_layer_trees[3]->CommitScheduledCALayers(
      superlayer_, std::move(ca_layer_trees[2]), properties.rect.size(),
      properties.scale_factor);

  // Validate that HDR is back on. The previous content layer should have
  // been un-parented.
  EXPECT_EQ([content_layer superlayer], nil);
  content_layer = GetOnlyContentLayer();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  EXPECT_TRUE([content_layer wantsExtendedDynamicRangeContent]);
#pragma clang diagnostic pop
}

}  // namespace gpu
