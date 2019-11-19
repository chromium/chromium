// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"

#import <AVFoundation/AVFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_image_io_surface.h"

@interface CALayer (Private)
@property BOOL wantsExtendedDynamicRangeContent;
@end

namespace ui {

namespace {

// This will enqueue |io_surface| to be drawn by |av_layer|. This will
// retain |cv_pixel_buffer| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
    AVSampleBufferDisplayLayer* av_layer,
    CVPixelBufferRef cv_pixel_buffer) {
  OSStatus os_status = noErr;

  base::ScopedCFTypeRef<CMVideoFormatDescriptionRef> video_info;
  os_status = CMVideoFormatDescriptionCreateForImageBuffer(
      nullptr, cv_pixel_buffer, video_info.InitializeInto());
  if (os_status != noErr) {
    LOG(ERROR) << "CMVideoFormatDescriptionCreateForImageBuffer failed with "
               << os_status;
    return false;
  }

  // The frame time doesn't matter because we will specify to display
  // immediately.
  CMTime frame_time = CMTimeMake(0, 1);
  CMSampleTimingInfo timing_info = {frame_time, frame_time, kCMTimeInvalid};

  base::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  os_status = CMSampleBufferCreateForImageBuffer(
      nullptr, cv_pixel_buffer, YES, nullptr, nullptr, video_info, &timing_info,
      sample_buffer.InitializeInto());
  if (os_status != noErr) {
    LOG(ERROR) << "CMSampleBufferCreateForImageBuffer failed with "
               << os_status;
    return false;
  }

  // Specify to display immediately via the sample buffer attachments.
  CFArrayRef attachments =
      CMSampleBufferGetSampleAttachmentsArray(sample_buffer, YES);
  if (!attachments) {
    LOG(ERROR) << "CMSampleBufferGetSampleAttachmentsArray failed";
    return false;
  }
  if (CFArrayGetCount(attachments) < 1) {
    LOG(ERROR) << "CMSampleBufferGetSampleAttachmentsArray result was empty";
    return false;
  }
  CFMutableDictionaryRef attachments_dictionary =
      reinterpret_cast<CFMutableDictionaryRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(attachments, 0)));
  if (!attachments_dictionary) {
    LOG(ERROR) << "Failed to get attachments dictionary";
    return false;
  }
  CFDictionarySetValue(attachments_dictionary,
                       kCMSampleAttachmentKey_DisplayImmediately,
                       kCFBooleanTrue);

  [av_layer enqueueSampleBuffer:sample_buffer];

  AVQueuedSampleBufferRenderingStatus status = [av_layer status];
  switch (status) {
    case AVQueuedSampleBufferRenderingStatusUnknown:
      LOG(ERROR) << "AVSampleBufferDisplayLayer has status unknown, but should "
                    "be rendering.";
      return false;
    case AVQueuedSampleBufferRenderingStatusFailed:
      LOG(ERROR) << "AVSampleBufferDisplayLayer has status failed, error: "
                 << [[[av_layer error] description]
                        cStringUsingEncoding:NSUTF8StringEncoding];
      return false;
    case AVQueuedSampleBufferRenderingStatusRendering:
      break;
  }

  return true;
}

// This will enqueue |io_surface| to be drawn by |av_layer| by wrapping
// |io_surface| in a CVPixelBuffer. This will increase the in-use count
// of and retain |io_surface| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueIOSurface(
    AVSampleBufferDisplayLayer* av_layer,
    IOSurfaceRef io_surface) {
  CVReturn cv_return = kCVReturnSuccess;

  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  cv_return = CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, nullptr, cv_pixel_buffer.InitializeInto());
  if (cv_return != kCVReturnSuccess) {
    LOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed with " << cv_return;
    return false;
  }

  return AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(av_layer,
                                                        cv_pixel_buffer);
}

}  // namespace

class CARendererLayerTree::SolidColorContents
    : public base::RefCounted<CARendererLayerTree::SolidColorContents> {
 public:
  static scoped_refptr<SolidColorContents> Get(SkColor color);
  id GetContents() const;

 private:
  friend class base::RefCounted<SolidColorContents>;

  SolidColorContents(SkColor color, IOSurfaceRef io_surface);
  ~SolidColorContents();

  static std::map<SkColor, SolidColorContents*>* GetMap();

  SkColor color_ = 0;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
};

// static
scoped_refptr<CARendererLayerTree::SolidColorContents>
CARendererLayerTree::SolidColorContents::Get(SkColor color) {
  const int kSolidColorContentsSize = 16;

  auto* map = GetMap();
  auto found = map->find(color);
  if (found != map->end())
    return found->second;

  IOSurfaceRef io_surface = CreateIOSurface(
      gfx::Size(kSolidColorContentsSize, kSolidColorContentsSize),
      gfx::BufferFormat::BGRA_8888);
  if (!io_surface)
    return nullptr;

  size_t bytes_per_row = IOSurfaceGetBytesPerRowOfPlane(io_surface, 0);
  IOSurfaceLock(io_surface, 0, NULL);
  char* row_base_address =
      reinterpret_cast<char*>(IOSurfaceGetBaseAddress(io_surface));
  for (int i = 0; i < kSolidColorContentsSize; ++i) {
    unsigned int* pixel = reinterpret_cast<unsigned int*>(row_base_address);
    for (int j = 0; j < kSolidColorContentsSize; ++j)
      *(pixel++) = color;
    row_base_address += bytes_per_row;
  }
  IOSurfaceUnlock(io_surface, 0, NULL);

  return new SolidColorContents(color, io_surface);
}

id CARendererLayerTree::SolidColorContents::GetContents() const {
  return static_cast<id>(io_surface_.get());
}

CARendererLayerTree::SolidColorContents::SolidColorContents(
    SkColor color,
    IOSurfaceRef io_surface)
    : color_(color), io_surface_(io_surface) {
  auto* map = GetMap();
  DCHECK(map->find(color_) == map->end());
  map->insert(std::make_pair(color_, this));
}

CARendererLayerTree::SolidColorContents::~SolidColorContents() {
  auto* map = GetMap();
  auto found = map->find(color_);
  DCHECK(found != map->end());
  DCHECK(found->second == this);
  map->erase(color_);
}

// static
std::map<SkColor, CARendererLayerTree::SolidColorContents*>*
CARendererLayerTree::SolidColorContents::GetMap() {
  static auto* map = new std::map<SkColor, SolidColorContents*>();
  return map;
}

CARendererLayerTree::CARendererLayerTree(
    bool allow_av_sample_buffer_display_layer,
    bool allow_solid_color_layers)
    : allow_av_sample_buffer_display_layer_(
          allow_av_sample_buffer_display_layer),
      allow_solid_color_layers_(allow_solid_color_layers) {}
CARendererLayerTree::~CARendererLayerTree() {}

bool CARendererLayerTree::ScheduleCALayer(const CARendererLayerParams& params) {
  if (has_committed_) {
    DLOG(ERROR) << "ScheduleCALayer called after CommitScheduledCALayers.";
    return false;
  }
  return root_layer_.AddContentLayer(this, params);
}

void CARendererLayerTree::CommitScheduledCALayers(
    CALayer* superlayer,
    std::unique_ptr<CARendererLayerTree> old_tree,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("gpu", "CARendererLayerTree::CommitScheduledCALayers");
  RootLayer* old_root_layer = nullptr;
  if (old_tree) {
    DCHECK(old_tree->has_committed_);
    if (old_tree->scale_factor_ == scale_factor)
      old_root_layer = &old_tree->root_layer_;
  }

  root_layer_.CommitToCA(superlayer, old_root_layer, pixel_size, scale_factor);
  // If there are any extra CALayers in |old_tree| that were not stolen by this
  // tree, they will be removed from the CALayer tree in this deallocation.
  old_tree.reset();
  has_committed_ = true;
  scale_factor_ = scale_factor;
}

bool CARendererLayerTree::RootLayer::WantsFullcreenLowPowerBackdrop() const {
  bool found_video_layer = false;
  for (auto& clip_layer : clip_and_sorting_layers) {
    for (auto& transform_layer : clip_layer.transform_layers) {
      for (auto& content_layer : transform_layer.content_layers) {
        // Detached mode requires that no layers be on top of the video layer.
        if (found_video_layer)
          return false;

        // See if this is the video layer.
        if (content_layer.use_av_layer) {
          found_video_layer = true;
          if (!transform_layer.transform.IsPositiveScaleOrTranslation())
            return false;
          if (content_layer.opacity != 1)
            return false;
          continue;
        }

        // If we haven't found the video layer yet, make sure everything is
        // solid black or transparent
        if (content_layer.io_surface)
          return false;
        if (content_layer.background_color != SK_ColorBLACK &&
            content_layer.background_color != SK_ColorTRANSPARENT) {
          return false;
        }
      }
    }
  }
  return found_video_layer;
}

void CARendererLayerTree::RootLayer::EnforceOnlyOneAVLayer() {
  size_t video_layer_count = 0;
  for (auto& clip_layer : clip_and_sorting_layers) {
    for (auto& transform_layer : clip_layer.transform_layers) {
      for (auto& content_layer : transform_layer.content_layers) {
        if (content_layer.use_av_layer)
          video_layer_count += 1;
      }
    }
  }
  if (video_layer_count <= 1)
    return;
  for (auto& clip_layer : clip_and_sorting_layers) {
    for (auto& transform_layer : clip_layer.transform_layers) {
      for (auto& content_layer : transform_layer.content_layers) {
        if (content_layer.use_av_layer)
          content_layer.use_av_layer = false;
      }
    }
  }
}

id CARendererLayerTree::ContentsForSolidColorForTesting(SkColor color) {
  return SolidColorContents::Get(color)->GetContents();
}

IOSurfaceRef CARendererLayerTree::GetContentIOSurface() const {
  size_t clip_count = root_layer_.clip_and_sorting_layers.size();
  if (clip_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "ClipAndSortingLayer, there are " << clip_count << ".";
    return nullptr;
  }
  const ClipAndSortingLayer& clip_and_sorting =
      root_layer_.clip_and_sorting_layers[0];
  size_t transform_count = clip_and_sorting.transform_layers.size();
  if (transform_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "TransformLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const TransformLayer& transform = clip_and_sorting.transform_layers[0];
  size_t content_count = transform.content_layers.size();
  if (content_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "ContentLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const ContentLayer& content = transform.content_layers[0];
  return content.io_surface.get();
}

CARendererLayerTree::RootLayer::RootLayer() {}

// Note that for all destructors, the the CALayer will have been reset to nil if
// another layer has taken it.
CARendererLayerTree::RootLayer::~RootLayer() {
  [ca_layer removeFromSuperlayer];
}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    bool is_clipped,
    gfx::Rect clip_rect,
    gfx::RRectF rounded_corner_bounds_arg,
    unsigned sorting_context_id,
    bool is_singleton_sorting_context)
    : is_clipped(is_clipped),
      clip_rect(clip_rect),
      rounded_corner_bounds(rounded_corner_bounds_arg),
      sorting_context_id(sorting_context_id),
      is_singleton_sorting_context(is_singleton_sorting_context) {}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    ClipAndSortingLayer&& layer)
    : transform_layers(std::move(layer.transform_layers)),
      is_clipped(layer.is_clipped),
      clip_rect(layer.clip_rect),
      rounded_corner_bounds(layer.rounded_corner_bounds),
      sorting_context_id(layer.sorting_context_id),
      is_singleton_sorting_context(layer.is_singleton_sorting_context),
      clipping_ca_layer(layer.clipping_ca_layer),
      rounded_corner_ca_layer(layer.rounded_corner_ca_layer) {
  // Ensure that the ca_layer be reset, so that when the destructor is called,
  // the layer hierarchy is unaffected.
  // TODO(ccameron): Add a move constructor for scoped_nsobject to do this
  // automatically.
  layer.clipping_ca_layer.reset();
  layer.rounded_corner_ca_layer.reset();
}

CARendererLayerTree::ClipAndSortingLayer::~ClipAndSortingLayer() {
  [clipping_ca_layer removeFromSuperlayer];
  [rounded_corner_ca_layer removeFromSuperlayer];
}

CARendererLayerTree::TransformLayer::TransformLayer(
    const gfx::Transform& transform)
    : transform(transform) {}

CARendererLayerTree::TransformLayer::TransformLayer(TransformLayer&& layer)
    : transform(layer.transform),
      content_layers(std::move(layer.content_layers)),
      ca_layer(layer.ca_layer) {
  layer.ca_layer.reset();
}

CARendererLayerTree::TransformLayer::~TransformLayer() {
  [ca_layer removeFromSuperlayer];
}

CARendererLayerTree::ContentLayer::ContentLayer(
    CARendererLayerTree* tree,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect_in,
    unsigned background_color,
    bool triggers_hdr,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter)
    : io_surface(io_surface),
      cv_pixel_buffer(cv_pixel_buffer),
      contents_rect(contents_rect),
      rect(rect_in),
      background_color(background_color),
      triggers_hdr(triggers_hdr),
      ca_edge_aa_mask(0),
      opacity(opacity),
      ca_filter(filter == GL_LINEAR ? kCAFilterLinear : kCAFilterNearest) {
  DCHECK(filter == GL_LINEAR || filter == GL_NEAREST);

  // On Mac OS Sierra, solid color layers are not color color corrected to the
  // output monitor color space, but IOSurface-backed layers are color
  // corrected. Note that this is only the case when the CALayers are shared
  // across processes. To make colors consistent across both solid color and
  // IOSurface-backed layers, use a cache of solid-color IOSurfaces as
  // contents. Black and transparent layers must use real colors to be eligible
  // for low power detachment in fullscreen.
  // https://crbug.com/633805
  if (!io_surface && !tree->allow_solid_color_layers_ &&
      background_color != SK_ColorBLACK &&
      background_color != SK_ColorTRANSPARENT) {
    solid_color_contents = SolidColorContents::Get(background_color);
    ContentLayer::contents_rect = gfx::RectF(0, 0, 1, 1);
  }

  // Because the root layer has setGeometryFlipped:YES, there is some ambiguity
  // about what exactly top and bottom mean. This ambiguity is resolved in
  // different ways for solid color CALayers and for CALayers that have content
  // (surprise!). For CALayers with IOSurface content, the top edge in the AA
  // mask refers to what appears as the bottom edge on-screen. For CALayers
  // without content (solid color layers), the top edge in the AA mask is the
  // top edge on-screen.
  // https://crbug.com/567946
  if (edge_aa_mask & GL_CA_LAYER_EDGE_LEFT_CHROMIUM)
    ca_edge_aa_mask |= kCALayerLeftEdge;
  if (edge_aa_mask & GL_CA_LAYER_EDGE_RIGHT_CHROMIUM)
    ca_edge_aa_mask |= kCALayerRightEdge;
  if (io_surface || solid_color_contents) {
    if (edge_aa_mask & GL_CA_LAYER_EDGE_TOP_CHROMIUM)
      ca_edge_aa_mask |= kCALayerBottomEdge;
    if (edge_aa_mask & GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM)
      ca_edge_aa_mask |= kCALayerTopEdge;
  } else {
    if (edge_aa_mask & GL_CA_LAYER_EDGE_TOP_CHROMIUM)
      ca_edge_aa_mask |= kCALayerTopEdge;
    if (edge_aa_mask & GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM)
      ca_edge_aa_mask |= kCALayerBottomEdge;
  }

  // Only allow 4:2:0 frames which fill the layer's contents to be promoted to
  // AV layers.
  if (tree->allow_av_sample_buffer_display_layer_ &&
      IOSurfaceGetPixelFormat(io_surface) ==
          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
      contents_rect == gfx::RectF(0, 0, 1, 1)) {
    use_av_layer = true;

    // If the layer's aspect ratio could be made to match the video's aspect
    // ratio by expanding either dimension by a fractional pixel, do so. The
    // mismatch probably resulted from rounding the dimensions to integers.
    // This works around a macOS 10.13 bug which breaks detached fullscreen
    // playback of slightly distorted videos (https://crbug.com/792632).
    const auto av_rect(cv_pixel_buffer
                           ? gfx::RectF(CVPixelBufferGetWidth(cv_pixel_buffer),
                                        CVPixelBufferGetHeight(cv_pixel_buffer))
                           : gfx::RectF(IOSurfaceGetWidth(io_surface),
                                        IOSurfaceGetHeight(io_surface)));
    const CGFloat av_ratio = av_rect.width() / av_rect.height();
    const CGFloat layer_ratio = rect.width() / rect.height();
    const CGFloat ratio_error = av_ratio / layer_ratio;

    if (ratio_error > 1) {
      const float width_correction = rect.width() * ratio_error - rect.width();
      if (width_correction < 1)
        rect.Inset(-width_correction / 2, 0);
    } else if (ratio_error < 1) {
      const float height_correction =
          rect.height() / ratio_error - rect.height();
      if (height_correction < 1)
        rect.Inset(0, -height_correction / 2);
    }
  }
}

CARendererLayerTree::ContentLayer::ContentLayer(ContentLayer&& layer)
    : io_surface(layer.io_surface),
      cv_pixel_buffer(layer.cv_pixel_buffer),
      solid_color_contents(layer.solid_color_contents),
      contents_rect(layer.contents_rect),
      rect(layer.rect),
      background_color(layer.background_color),
      triggers_hdr(layer.triggers_hdr),
      ca_edge_aa_mask(layer.ca_edge_aa_mask),
      opacity(layer.opacity),
      ca_filter(layer.ca_filter),
      ca_layer(std::move(layer.ca_layer)),
      av_layer(std::move(layer.av_layer)),
      use_av_layer(layer.use_av_layer) {
  DCHECK(!layer.ca_layer);
  DCHECK(!layer.av_layer);
}

CARendererLayerTree::ContentLayer::~ContentLayer() {
  [ca_layer removeFromSuperlayer];
}

bool CARendererLayerTree::RootLayer::AddContentLayer(
    CARendererLayerTree* tree,
    const CARendererLayerParams& params) {
  bool needs_new_clip_and_sorting_layer = true;

  // In sorting_context_id 0, all quads are listed in back-to-front order.
  // This is accomplished by having the CALayers be siblings of each other.
  // If a quad has a 3D transform, it is necessary to put it in its own sorting
  // context, so that it will not intersect with quads before and after it.
  bool is_singleton_sorting_context =
      !params.sorting_context_id && !params.transform.IsFlat();

  if (!clip_and_sorting_layers.empty()) {
    ClipAndSortingLayer& current_layer = clip_and_sorting_layers.back();
    // It is in error to change the clipping settings within a non-zero sorting
    // context. The result will be incorrect layering and intersection.
    if (params.sorting_context_id &&
        current_layer.sorting_context_id == params.sorting_context_id &&
        (current_layer.is_clipped != params.is_clipped ||
         current_layer.clip_rect != params.clip_rect ||
         current_layer.rounded_corner_bounds != params.rounded_corner_bounds)) {
      DLOG(ERROR) << "CALayer changed clip inside non-zero sorting context.";
      return false;
    }
    if (!is_singleton_sorting_context &&
        !current_layer.is_singleton_sorting_context &&
        current_layer.is_clipped == params.is_clipped &&
        current_layer.clip_rect == params.clip_rect &&
        current_layer.rounded_corner_bounds == params.rounded_corner_bounds &&
        current_layer.sorting_context_id == params.sorting_context_id) {
      needs_new_clip_and_sorting_layer = false;
    }
  }
  if (needs_new_clip_and_sorting_layer) {
    clip_and_sorting_layers.push_back(ClipAndSortingLayer(
        params.is_clipped, params.clip_rect, params.rounded_corner_bounds,
        params.sorting_context_id, is_singleton_sorting_context));
  }
  clip_and_sorting_layers.back().AddContentLayer(tree, params);
  return true;
}

void CARendererLayerTree::ClipAndSortingLayer::AddContentLayer(
    CARendererLayerTree* tree,
    const CARendererLayerParams& params) {
  bool needs_new_transform_layer = true;
  if (!transform_layers.empty()) {
    const TransformLayer& current_layer = transform_layers.back();
    if (current_layer.transform == params.transform)
      needs_new_transform_layer = false;
  }
  if (needs_new_transform_layer)
    transform_layers.push_back(TransformLayer(params.transform));
  transform_layers.back().AddContentLayer(tree, params);
}

void CARendererLayerTree::TransformLayer::AddContentLayer(
    CARendererLayerTree* tree,
    const CARendererLayerParams& params) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  bool triggers_hdr = false;
  if (params.image) {
    gl::GLImageIOSurface* io_surface_image =
        gl::GLImageIOSurface::FromGLImage(params.image);
    DCHECK(io_surface_image);
    io_surface = io_surface_image->io_surface();
    // Temporary investagtive fix for https://crbug.com/702369. It appears upon
    // investigation that not using the original CVPixelBufferRef which came
    // from the VTDecompressionSession prevents or minimizes flashing of
    // incorrect content. Disable the CVPixelBufferRef path for the moment to
    // determine if this fixes the bug for users.
    // TODO(ccameron): If this indeed causes the bug to disappear, then
    // extirpate the CVPixelBufferRef path.
    // cv_pixel_buffer = io_surface_image->cv_pixel_buffer();
    triggers_hdr = params.image->color_space().IsHDR();
  }
  content_layers.push_back(
      ContentLayer(tree, io_surface, cv_pixel_buffer, params.contents_rect,
                   params.rect, params.background_color, triggers_hdr,
                   params.edge_aa_mask, params.opacity, params.filter));
}

void CARendererLayerTree::RootLayer::CommitToCA(CALayer* superlayer,
                                                RootLayer* old_layer,
                                                const gfx::Size& pixel_size,
                                                float scale_factor) {
  if (old_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
  } else {
    ca_layer.reset([[CALayer alloc] init]);
    [ca_layer setAnchorPoint:CGPointZero];
    [superlayer setSublayers:nil];
    [superlayer addSublayer:ca_layer];
    [superlayer setBorderWidth:0];
  }
  if ([ca_layer superlayer] != superlayer) {
    DLOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  EnforceOnlyOneAVLayer();

  if (WantsFullcreenLowPowerBackdrop()) {
    const gfx::RectF bg_rect(
        ScaleSize(gfx::SizeF(pixel_size), 1 / scale_factor));
    if (gfx::RectF([ca_layer frame]) != bg_rect)
      [ca_layer setFrame:bg_rect.ToCGRect()];
    if (![ca_layer backgroundColor])
      [ca_layer setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
  } else {
    if (gfx::RectF([ca_layer frame]) != gfx::RectF())
      [ca_layer setFrame:CGRectZero];
    if ([ca_layer backgroundColor])
      [ca_layer setBackgroundColor:nil];
  }

  for (size_t i = 0; i < clip_and_sorting_layers.size(); ++i) {
    ClipAndSortingLayer* old_clip_and_sorting_layer = nullptr;
    if (old_layer && i < old_layer->clip_and_sorting_layers.size()) {
      old_clip_and_sorting_layer = &old_layer->clip_and_sorting_layers[i];
    }
    clip_and_sorting_layers[i].CommitToCA(
        ca_layer.get(), old_clip_and_sorting_layer, scale_factor);
  }
}

void CARendererLayerTree::ClipAndSortingLayer::CommitToCA(
    CALayer* superlayer,
    ClipAndSortingLayer* old_layer,
    float scale_factor) {
  bool update_is_clipped = true;
  bool update_clip_rect = true;
  if (old_layer) {
    DCHECK(old_layer->clipping_ca_layer);
    DCHECK(old_layer->rounded_corner_ca_layer);
    std::swap(clipping_ca_layer, old_layer->clipping_ca_layer);
    std::swap(rounded_corner_ca_layer, old_layer->rounded_corner_ca_layer);
    update_is_clipped = old_layer->is_clipped != is_clipped;
    update_clip_rect = update_is_clipped || old_layer->clip_rect != clip_rect;
  } else {
    clipping_ca_layer.reset([[CALayer alloc] init]);
    [clipping_ca_layer setAnchorPoint:CGPointZero];
    [superlayer addSublayer:clipping_ca_layer];
    rounded_corner_ca_layer.reset([[CALayer alloc] init]);
    [rounded_corner_ca_layer setAnchorPoint:CGPointZero];
    [clipping_ca_layer addSublayer:rounded_corner_ca_layer];
  }

  if (!rounded_corner_bounds.IsEmpty()) {
    if (!old_layer ||
        old_layer->rounded_corner_bounds != rounded_corner_bounds) {
      gfx::RectF dip_rounded_corner_bounds =
          gfx::RectF(rounded_corner_bounds.rect());
      dip_rounded_corner_bounds.Scale(1 / scale_factor);

      [rounded_corner_ca_layer setMasksToBounds:true];

      [rounded_corner_ca_layer
          setPosition:CGPointMake(dip_rounded_corner_bounds.x(),
                                  dip_rounded_corner_bounds.y())];
      [rounded_corner_ca_layer
          setBounds:CGRectMake(0, 0, dip_rounded_corner_bounds.width(),
                               dip_rounded_corner_bounds.height())];
      [rounded_corner_ca_layer
          setSublayerTransform:CATransform3DMakeTranslation(
                                   -dip_rounded_corner_bounds.x(),
                                   -dip_rounded_corner_bounds.y(), 0)];

      [rounded_corner_ca_layer
          setCornerRadius:rounded_corner_bounds.GetSimpleRadius() /
                          scale_factor];
    }
  } else {
    [rounded_corner_ca_layer setMasksToBounds:false];
    [rounded_corner_ca_layer setPosition:CGPointZero];
    [rounded_corner_ca_layer setBounds:CGRectZero];
    [rounded_corner_ca_layer setSublayerTransform:CATransform3DIdentity];
    [rounded_corner_ca_layer setCornerRadius:0];
  }
  if ([clipping_ca_layer superlayer] != superlayer) {
    DLOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  if (update_is_clipped)
    [clipping_ca_layer setMasksToBounds:is_clipped];

  if (update_clip_rect) {
    if (is_clipped) {
      gfx::RectF dip_clip_rect = gfx::RectF(clip_rect);
      dip_clip_rect.Scale(1 / scale_factor);
      [clipping_ca_layer
          setPosition:CGPointMake(dip_clip_rect.x(), dip_clip_rect.y())];
      [clipping_ca_layer setBounds:CGRectMake(0, 0, dip_clip_rect.width(),
                                              dip_clip_rect.height())];
      [clipping_ca_layer
          setSublayerTransform:CATransform3DMakeTranslation(
                                   -dip_clip_rect.x(), -dip_clip_rect.y(), 0)];
    } else {
      [clipping_ca_layer setPosition:CGPointZero];
      [clipping_ca_layer setBounds:CGRectZero];
      [clipping_ca_layer setSublayerTransform:CATransform3DIdentity];
    }
  }

  for (size_t i = 0; i < transform_layers.size(); ++i) {
    TransformLayer* old_transform_layer = nullptr;
    if (old_layer && i < old_layer->transform_layers.size())
      old_transform_layer = &old_layer->transform_layers[i];
    transform_layers[i].CommitToCA(rounded_corner_ca_layer, old_transform_layer,
                                   scale_factor);
  }
}

void CARendererLayerTree::TransformLayer::CommitToCA(CALayer* superlayer,
                                                     TransformLayer* old_layer,
                                                     float scale_factor) {
  bool update_transform = true;
  if (old_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
    update_transform = old_layer->transform != transform;
  } else {
    ca_layer.reset([[CATransformLayer alloc] init]);
    [superlayer addSublayer:ca_layer];
  }
  DCHECK_EQ([ca_layer superlayer], superlayer);

  if (update_transform) {
    gfx::Transform pre_scale;
    gfx::Transform post_scale;
    pre_scale.Scale(1 / scale_factor, 1 / scale_factor);
    post_scale.Scale(scale_factor, scale_factor);
    gfx::Transform conjugated_transform = pre_scale * transform * post_scale;

    CATransform3D ca_transform;
    conjugated_transform.matrix().asColMajord(&ca_transform.m11);
    [ca_layer setTransform:ca_transform];
  }

  for (size_t i = 0; i < content_layers.size(); ++i) {
    ContentLayer* old_content_layer = nullptr;
    if (old_layer && i < old_layer->content_layers.size())
      old_content_layer = &old_layer->content_layers[i];
    content_layers[i].CommitToCA(ca_layer.get(), old_content_layer,
                                 scale_factor);
  }
}

void CARendererLayerTree::ContentLayer::CommitToCA(CALayer* superlayer,
                                                   ContentLayer* old_layer,
                                                   float scale_factor) {
  bool update_contents = true;
  bool update_contents_rect = true;
  bool update_rect = true;
  bool update_background_color = true;
  bool update_triggers_hdr = true;
  bool update_ca_edge_aa_mask = true;
  bool update_opacity = true;
  bool update_ca_filter = true;
  if (old_layer && old_layer->use_av_layer == use_av_layer) {
    DCHECK(old_layer->ca_layer);
    std::swap(ca_layer, old_layer->ca_layer);
    std::swap(av_layer, old_layer->av_layer);
    update_contents = old_layer->io_surface != io_surface ||
                      old_layer->cv_pixel_buffer != cv_pixel_buffer ||
                      old_layer->solid_color_contents != solid_color_contents;
    update_contents_rect = old_layer->contents_rect != contents_rect;
    update_rect = old_layer->rect != rect;
    update_background_color = old_layer->background_color != background_color;
    update_triggers_hdr = old_layer->triggers_hdr != triggers_hdr;
    update_ca_edge_aa_mask = old_layer->ca_edge_aa_mask != ca_edge_aa_mask;
    update_opacity = old_layer->opacity != opacity;
    update_ca_filter = old_layer->ca_filter != ca_filter;
  } else {
    if (use_av_layer) {
      av_layer.reset([[AVSampleBufferDisplayLayer alloc] init]);
      ca_layer.reset([av_layer retain]);
      [av_layer setVideoGravity:AVLayerVideoGravityResize];
    } else {
      ca_layer.reset([[CALayer alloc] init]);
    }
    [ca_layer setAnchorPoint:CGPointZero];
    if (old_layer && old_layer->ca_layer)
      [superlayer replaceSublayer:old_layer->ca_layer with:ca_layer];
    else
      [superlayer addSublayer:ca_layer];
  }
  DCHECK_EQ([ca_layer superlayer], superlayer);
  bool update_anything = update_contents || update_contents_rect ||
                         update_rect || update_background_color ||
                         update_triggers_hdr || update_ca_edge_aa_mask ||
                         update_opacity || update_ca_filter;
  if (use_av_layer) {
    if (update_contents) {
      bool result = false;
      if (cv_pixel_buffer) {
        result = AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
            av_layer, cv_pixel_buffer);
        if (!result) {
          LOG(ERROR) << "AVSampleBufferDisplayLayerEnqueueCVPixelBuffer failed";
        }
      } else {
        result =
            AVSampleBufferDisplayLayerEnqueueIOSurface(av_layer, io_surface);
        if (!result) {
          LOG(ERROR) << "AVSampleBufferDisplayLayerEnqueueIOSurface failed";
        }
      }
      // TODO(ccameron): Recreate the AVSampleBufferDisplayLayer on failure.
      // This is not being done yet, to determine if this happens concurrently
      // with video flickering.
      // https://crbug.com/702369
    }
  } else {
    if (update_contents) {
      if (io_surface) {
        [ca_layer setContents:static_cast<id>(io_surface.get())];
      } else if (solid_color_contents) {
        [ca_layer setContents:solid_color_contents->GetContents()];
      } else {
        [ca_layer setContents:nil];
      }
      if ([ca_layer respondsToSelector:(@selector(setContentsScale:))])
        [ca_layer setContentsScale:scale_factor];
    }
    if (update_contents_rect)
      [ca_layer setContentsRect:contents_rect.ToCGRect()];
  }
  if (update_rect) {
    gfx::RectF dip_rect = gfx::RectF(rect);
    dip_rect.Scale(1 / scale_factor);
    [ca_layer setPosition:CGPointMake(dip_rect.x(), dip_rect.y())];
    [ca_layer setBounds:CGRectMake(0, 0, dip_rect.width(), dip_rect.height())];
  }
  if (update_background_color) {
    CGFloat rgba_color_components[4] = {
        SkColorGetR(background_color) / 255.,
        SkColorGetG(background_color) / 255.,
        SkColorGetB(background_color) / 255.,
        SkColorGetA(background_color) / 255.,
    };
    base::ScopedCFTypeRef<CGColorRef> srgb_background_color(CGColorCreate(
        CGColorSpaceCreateWithName(kCGColorSpaceSRGB), rgba_color_components));
    [ca_layer setBackgroundColor:srgb_background_color];
  }
  if (update_triggers_hdr) {
    if (@available(macos 10.15, *)) {
      if ([ca_layer
              respondsToSelector:(@selector
                                  (setWantsExtendedDynamicRangeContent:))]) {
        [ca_layer setWantsExtendedDynamicRangeContent:triggers_hdr];
      }
    }
  }
  if (update_ca_edge_aa_mask)
    [ca_layer setEdgeAntialiasingMask:ca_edge_aa_mask];
  if (update_opacity)
    [ca_layer setOpacity:opacity];
  if (update_ca_filter) {
    [ca_layer setMagnificationFilter:ca_filter];
    [ca_layer setMinificationFilter:ca_filter];
  }

  static bool show_borders = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowMacOverlayBorders);
  if (show_borders) {
    base::ScopedCFTypeRef<CGColorRef> color;
    if (update_anything) {
      if (use_av_layer) {
        // Yellow represents an AV layer that changed this frame.
        color.reset(CGColorCreateGenericRGB(1, 1, 0, 1));
      } else if (io_surface) {
        // Magenta represents a CALayer that changed this frame.
        color.reset(CGColorCreateGenericRGB(1, 0, 1, 1));
      } else if (solid_color_contents) {
        // Cyan represents a solid color IOSurface-backed layer.
        color.reset(CGColorCreateGenericRGB(0, 1, 1, 1));
      } else {
        // Red represents a solid color layer.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
    } else {
      // Grey represents a CALayer that has not changed.
      color.reset(CGColorCreateGenericRGB(0.5, 0.5, 0.5, 1));
    }
    [ca_layer setBorderWidth:1];
    [ca_layer setBorderColor:color];
  }
}

}  // namespace ui
