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
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/trace_event/trace_event.h"
#include "components/metal_util/hdr_copier_layer.h"
#include "media/base/mac/color_space_util_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/io_surface_hdr_metadata.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_image_io_surface.h"

namespace ui {

namespace {

// This will enqueue |io_surface| to be drawn by |av_layer|. This will
// retain |cv_pixel_buffer| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
    AVSampleBufferDisplayLayer* av_layer,
    CVPixelBufferRef cv_pixel_buffer) {
  base::ScopedCFTypeRef<CMVideoFormatDescriptionRef> video_info;
  OSStatus os_status = CMVideoFormatDescriptionCreateForImageBuffer(
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
    IOSurfaceRef io_surface,
    const gfx::ColorSpace& io_surface_color_space) {
  CVReturn cv_return = kCVReturnSuccess;

  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  cv_return = CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, nullptr, cv_pixel_buffer.InitializeInto());
  if (cv_return != kCVReturnSuccess) {
    LOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed with " << cv_return;
    return false;
  }

  if (__builtin_available(macos 11.0, *)) {
    if (io_surface_color_space ==
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                            gfx::ColorSpace::TransferID::PQ,
                            gfx::ColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED) ||
        io_surface_color_space ==
            gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                            gfx::ColorSpace::TransferID::HLG,
                            gfx::ColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED)) {
      CVBufferSetAttachment(cv_pixel_buffer, kCVImageBufferColorPrimariesKey,
                            kCVImageBufferColorPrimaries_ITU_R_2020,
                            kCVAttachmentMode_ShouldPropagate);
      CVBufferSetAttachment(cv_pixel_buffer, kCVImageBufferYCbCrMatrixKey,
                            kCVImageBufferYCbCrMatrix_ITU_R_2020,
                            kCVAttachmentMode_ShouldPropagate);
      CVBufferSetAttachment(
          cv_pixel_buffer, kCVImageBufferTransferFunctionKey,
          io_surface_color_space.GetTransferID() ==
                  gfx::ColorSpace::TransferID::HLG
              ? kCVImageBufferTransferFunction_ITU_R_2100_HLG
              : kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
          kCVAttachmentMode_ShouldPropagate);

      // Transfer stashed HDR metadata from the IOSurface to the CVPixelBuffer.
      //
      // Note: It'd be nice to find a way to set this on the IOSurface itself
      // in some way that propagates to the CVPixelBuffer, but thus far we
      // haven't been able to find a way.
      gfx::HDRMetadata hdr_metadata;
      if (IOSurfaceGetHDRMetadata(io_surface, hdr_metadata)) {
        if (!(hdr_metadata.color_volume_metadata ==
              gfx::ColorVolumeMetadata())) {
          CVBufferSetAttachment(
              cv_pixel_buffer, kCVImageBufferMasteringDisplayColorVolumeKey,
              media::GenerateMasteringDisplayColorVolume(hdr_metadata),
              kCVAttachmentMode_ShouldPropagate);
        }
        if (hdr_metadata.max_content_light_level ||
            hdr_metadata.max_frame_average_light_level) {
          CVBufferSetAttachment(
              cv_pixel_buffer, kCVImageBufferContentLightLevelInfoKey,
              media::GenerateContentLightLevelInfo(hdr_metadata),
              kCVAttachmentMode_ShouldPropagate);
        }
      }
    }
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
  for (auto& clip_layer : clip_and_sorting_layers_) {
    for (auto& transform_layer : clip_layer.transform_layers_) {
      for (auto& content_layer : transform_layer.content_layers_) {
        // Detached mode requires that no layers be on top of the video layer.
        if (found_video_layer)
          return false;

        // See if this is the video layer.
        if (content_layer.type_ == CALayerType::kVideo) {
          found_video_layer = true;
          if (!transform_layer.transform_.IsPositiveScaleOrTranslation())
            return false;
          if (content_layer.opacity_ != 1)
            return false;
          continue;
        }

        // If we haven't found the video layer yet, make sure everything is
        // solid black or transparent
        if (content_layer.io_surface_)
          return false;
        if (content_layer.background_color_ != SK_ColorBLACK &&
            content_layer.background_color_ != SK_ColorTRANSPARENT) {
          return false;
        }
      }
    }
  }
  return found_video_layer;
}

void CARendererLayerTree::RootLayer::DowngradeAVLayersToCALayers() {
  for (auto& clip_layer : clip_and_sorting_layers_) {
    for (auto& transform_layer : clip_layer.transform_layers_) {
      for (auto& content_layer : transform_layer.content_layers_) {
        if (content_layer.type_ == CALayerType::kVideo &&
            content_layer.video_type_can_downgrade_) {
          content_layer.type_ = CALayerType::kDefault;
        }
      }
    }
  }
}

id CARendererLayerTree::ContentsForSolidColorForTesting(SkColor color) {
  return SolidColorContents::Get(color)->GetContents();
}

IOSurfaceRef CARendererLayerTree::GetContentIOSurface() const {
  size_t clip_count = root_layer_.clip_and_sorting_layers_.size();
  if (clip_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "ClipAndSortingLayer, there are " << clip_count << ".";
    return nullptr;
  }
  const ClipAndSortingLayer& clip_and_sorting =
      root_layer_.clip_and_sorting_layers_[0];
  size_t transform_count = clip_and_sorting.transform_layers_.size();
  if (transform_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "TransformLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const TransformLayer& transform = clip_and_sorting.transform_layers_[0];
  size_t content_count = transform.content_layers_.size();
  if (content_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "ContentLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const ContentLayer& content = transform.content_layers_[0];
  return content.io_surface_.get();
}

CARendererLayerTree::RootLayer::RootLayer() {}

// Note that for all destructors, the the CALayer will have been reset to nil if
// another layer has taken it.
CARendererLayerTree::RootLayer::~RootLayer() {
  [ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    bool is_clipped,
    gfx::Rect clip_rect,
    gfx::RRectF rounded_corner_bounds_arg,
    unsigned sorting_context_id,
    bool is_singleton_sorting_context)
    : is_clipped_(is_clipped),
      clip_rect_(clip_rect),
      rounded_corner_bounds_(rounded_corner_bounds_arg),
      sorting_context_id_(sorting_context_id),
      is_singleton_sorting_context_(is_singleton_sorting_context) {}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    ClipAndSortingLayer&& layer)
    : transform_layers_(std::move(layer.transform_layers_)),
      is_clipped_(layer.is_clipped_),
      clip_rect_(layer.clip_rect_),
      rounded_corner_bounds_(layer.rounded_corner_bounds_),
      sorting_context_id_(layer.sorting_context_id_),
      is_singleton_sorting_context_(layer.is_singleton_sorting_context_),
      clipping_ca_layer_(layer.clipping_ca_layer_),
      rounded_corner_ca_layer_(layer.rounded_corner_ca_layer_) {
  // Ensure that the ca_layer be reset, so that when the destructor is called,
  // the layer hierarchy is unaffected.
  // TODO(ccameron): Add a move constructor for scoped_nsobject to do this
  // automatically.
  layer.clipping_ca_layer_.reset();
  layer.rounded_corner_ca_layer_.reset();
}

CARendererLayerTree::ClipAndSortingLayer::~ClipAndSortingLayer() {
  [clipping_ca_layer_ removeFromSuperlayer];
  [rounded_corner_ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::TransformLayer::TransformLayer(
    const gfx::Transform& transform)
    : transform_(transform) {}

CARendererLayerTree::TransformLayer::TransformLayer(TransformLayer&& layer)
    : transform_(layer.transform_),
      content_layers_(std::move(layer.content_layers_)),
      ca_layer_(layer.ca_layer_) {
  layer.ca_layer_.reset();
}

CARendererLayerTree::TransformLayer::~TransformLayer() {
  [ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::ContentLayer::ContentLayer(
    CARendererLayerTree* tree,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    unsigned background_color,
    const gfx::ColorSpace& io_surface_color_space,
    unsigned edge_aa_mask,
    float opacity,
    unsigned filter,
    gfx::ProtectedVideoType protected_video_type)
    : io_surface_(io_surface),
      cv_pixel_buffer_(cv_pixel_buffer),
      contents_rect_(contents_rect),
      rect_(rect),
      background_color_(background_color),
      io_surface_color_space_(io_surface_color_space),
      ca_edge_aa_mask_(0),
      opacity_(opacity),
      ca_filter_(filter == GL_LINEAR ? kCAFilterLinear : kCAFilterNearest),
      protected_video_type_(protected_video_type) {
  DCHECK(filter == GL_LINEAR || filter == GL_NEAREST);

  // On Mac OS Sierra, solid color layers are not color converted to the output
  // monitor color space, but IOSurface-backed layers are color converted. Note
  // that this is only the case when the CALayers are shared across processes.
  // To make colors consistent across both solid color and IOSurface-backed
  // layers, use a cache of solid-color IOSurfaces as contents. Black and
  // transparent layers must use real colors to be eligible for low power
  // detachment in fullscreen.
  // https://crbug.com/633805
  if (!io_surface && !tree->allow_solid_color_layers_ &&
      background_color_ != SK_ColorBLACK &&
      background_color_ != SK_ColorTRANSPARENT) {
    solid_color_contents_ = SolidColorContents::Get(background_color);
    contents_rect_ = gfx::RectF(0, 0, 1, 1);
  }

  // Because the root layer has setGeometryFlipped:YES, there is some ambiguity
  // about what exactly top and bottom mean. This ambiguity is resolved in
  // different ways for solid color CALayers and for CALayers that have content
  // (surprise!). For CALayers with IOSurface content, the top edge in the AA
  // mask refers to what appears as the bottom edge on-screen. For CALayers
  // without content (solid color layers), the top edge in the AA mask is the
  // top edge on-screen.
  // https://crbug.com/567946
  if (edge_aa_mask & CALayerEdge::kLayerEdgeLeft)
    ca_edge_aa_mask_ |= kCALayerLeftEdge;
  if (edge_aa_mask & CALayerEdge::kLayerEdgeRight)
    ca_edge_aa_mask_ |= kCALayerRightEdge;
  if (io_surface || solid_color_contents_) {
    if (edge_aa_mask & CALayerEdge::kLayerEdgeTop)
      ca_edge_aa_mask_ |= kCALayerBottomEdge;
    if (edge_aa_mask & CALayerEdge::kLayerEdgeBottom)
      ca_edge_aa_mask_ |= kCALayerTopEdge;
  } else {
    if (edge_aa_mask & CALayerEdge::kLayerEdgeTop)
      ca_edge_aa_mask_ |= kCALayerTopEdge;
    if (edge_aa_mask & CALayerEdge::kLayerEdgeBottom)
      ca_edge_aa_mask_ |= kCALayerBottomEdge;
  }

  // Determine which type of CALayer subclass we should use.
  if (metal::ShouldUseHDRCopier(io_surface, io_surface_color_space)) {
    type_ = CALayerType::kHDRCopier;
  } else if (io_surface) {
    // Only allow 4:2:0 frames which fill the layer's contents or protected
    // video to be promoted to AV layers.
    if (tree->allow_av_sample_buffer_display_layer_) {
      if (contents_rect == gfx::RectF(0, 0, 1, 1)) {
        switch (IOSurfaceGetPixelFormat(io_surface)) {
          case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            type_ = CALayerType::kVideo;
            video_type_can_downgrade_ = !io_surface_color_space.IsHDR();
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
            type_ = CALayerType::kVideo;
            video_type_can_downgrade_ = false;
            break;
          default:
            break;
        }
      }

      if (protected_video_type_ != gfx::ProtectedVideoType::kClear) {
        if (@available(macOS 10.15, *)) {
          type_ = CALayerType::kVideo;
          video_type_can_downgrade_ = false;
        }
      }
    }
  }

  if (type_ == CALayerType::kVideo) {
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
    const CGFloat layer_ratio = rect_.width() / rect_.height();
    const CGFloat ratio_error = av_ratio / layer_ratio;

    if (ratio_error > 1) {
      const float width_correction =
          rect_.width() * ratio_error - rect_.width();
      if (width_correction < 1)
        rect_.Inset(gfx::InsetsF::VH(0, -width_correction / 2));
    } else if (ratio_error < 1) {
      const float height_correction =
          rect_.height() / ratio_error - rect_.height();
      if (height_correction < 1)
        rect_.Inset(gfx::InsetsF::VH(-height_correction / 2, 0));
    }
  }
}

CARendererLayerTree::ContentLayer::ContentLayer(ContentLayer&& layer)
    : io_surface_(layer.io_surface_),
      cv_pixel_buffer_(layer.cv_pixel_buffer_),
      solid_color_contents_(layer.solid_color_contents_),
      contents_rect_(layer.contents_rect_),
      rect_(layer.rect_),
      background_color_(layer.background_color_),
      io_surface_color_space_(layer.io_surface_color_space_),
      ca_edge_aa_mask_(layer.ca_edge_aa_mask_),
      opacity_(layer.opacity_),
      ca_filter_(layer.ca_filter_),
      type_(layer.type_),
      video_type_can_downgrade_(layer.video_type_can_downgrade_),
      protected_video_type_(layer.protected_video_type_),
      ca_layer_(std::move(layer.ca_layer_)),
      av_layer_(std::move(layer.av_layer_)),
      update_indicator_layer_(std::move(layer.update_indicator_layer_)) {
  DCHECK(!layer.ca_layer_);
  DCHECK(!layer.av_layer_);
  DCHECK(!update_indicator_layer_);
}

CARendererLayerTree::ContentLayer::~ContentLayer() {
  [ca_layer_ removeFromSuperlayer];
  [update_indicator_layer_ removeFromSuperlayer];
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

  if (!clip_and_sorting_layers_.empty()) {
    ClipAndSortingLayer& current_layer = clip_and_sorting_layers_.back();
    // It is in error to change the clipping settings within a non-zero sorting
    // context. The result will be incorrect layering and intersection.
    if (params.sorting_context_id &&
        current_layer.sorting_context_id_ == params.sorting_context_id &&
        (current_layer.is_clipped_ != params.is_clipped ||
         current_layer.clip_rect_ != params.clip_rect ||
         current_layer.rounded_corner_bounds_ !=
             params.rounded_corner_bounds)) {
      DLOG(ERROR) << "CALayer changed clip inside non-zero sorting context.";
      return false;
    }
    if (!is_singleton_sorting_context &&
        !current_layer.is_singleton_sorting_context_ &&
        current_layer.is_clipped_ == params.is_clipped &&
        current_layer.clip_rect_ == params.clip_rect &&
        current_layer.rounded_corner_bounds_ == params.rounded_corner_bounds &&
        current_layer.sorting_context_id_ == params.sorting_context_id) {
      needs_new_clip_and_sorting_layer = false;
    }
  }
  if (needs_new_clip_and_sorting_layer) {
    clip_and_sorting_layers_.push_back(ClipAndSortingLayer(
        params.is_clipped, params.clip_rect, params.rounded_corner_bounds,
        params.sorting_context_id, is_singleton_sorting_context));
  }
  clip_and_sorting_layers_.back().AddContentLayer(tree, params);
  return true;
}

void CARendererLayerTree::ClipAndSortingLayer::AddContentLayer(
    CARendererLayerTree* tree,
    const CARendererLayerParams& params) {
  bool needs_new_transform_layer = true;
  if (!transform_layers_.empty()) {
    const TransformLayer& current_layer = transform_layers_.back();
    if (current_layer.transform_ == params.transform)
      needs_new_transform_layer = false;
  }
  if (needs_new_transform_layer)
    transform_layers_.push_back(TransformLayer(params.transform));
  transform_layers_.back().AddContentLayer(tree, params);
}

void CARendererLayerTree::TransformLayer::AddContentLayer(
    CARendererLayerTree* tree,
    const CARendererLayerParams& params) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  gfx::ColorSpace io_surface_color_space;
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
    io_surface_color_space = params.image->color_space();
  }
  content_layers_.push_back(ContentLayer(
      tree, io_surface, cv_pixel_buffer, params.contents_rect, params.rect,
      params.background_color, io_surface_color_space, params.edge_aa_mask,
      params.opacity, params.filter, params.protected_video_type));
}

void CARendererLayerTree::RootLayer::CommitToCA(CALayer* superlayer,
                                                RootLayer* old_layer,
                                                const gfx::Size& pixel_size,
                                                float scale_factor) {
  if (old_layer) {
    DCHECK(old_layer->ca_layer_);
    std::swap(ca_layer_, old_layer->ca_layer_);
  } else {
    ca_layer_.reset([[CALayer alloc] init]);
    [ca_layer_ setAnchorPoint:CGPointZero];
    [superlayer setSublayers:nil];
    [superlayer addSublayer:ca_layer_];
    [superlayer setBorderWidth:0];
  }
  if ([ca_layer_ superlayer] != superlayer) {
    DLOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  if (WantsFullcreenLowPowerBackdrop()) {
    // In fullscreen low power mode there exists a single video layer on a
    // solid black background.
    const gfx::RectF bg_rect(
        ScaleSize(gfx::SizeF(pixel_size), 1 / scale_factor));
    if (gfx::RectF([ca_layer_ frame]) != bg_rect)
      [ca_layer_ setFrame:bg_rect.ToCGRect()];
    if (![ca_layer_ backgroundColor])
      [ca_layer_ setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
  } else {
    if (gfx::RectF([ca_layer_ frame]) != gfx::RectF())
      [ca_layer_ setFrame:CGRectZero];
    if ([ca_layer_ backgroundColor])
      [ca_layer_ setBackgroundColor:nil];
    // We know that we are not in fullscreen low power mode, so there is no
    // power savings (and a slight power cost) to using
    // AVSampleBufferDisplayLayer.
    // https://crbug.com/1143477
    // We also want to minimize our use of AVSampleBufferDisplayLayer because we
    // don't track which video element corresponded to which CALayer, and
    // AVSampleBufferDisplayLayer is not updated with the CATransaction.
    // Combined, these can result in result in videos jumping around.
    // https://crbug.com/923427
    DowngradeAVLayersToCALayers();
  }

  for (size_t i = 0; i < clip_and_sorting_layers_.size(); ++i) {
    ClipAndSortingLayer* old_clip_and_sorting_layer = nullptr;
    if (old_layer && i < old_layer->clip_and_sorting_layers_.size()) {
      old_clip_and_sorting_layer = &old_layer->clip_and_sorting_layers_[i];
    }
    clip_and_sorting_layers_[i].CommitToCA(
        ca_layer_.get(), old_clip_and_sorting_layer, scale_factor);
  }
}

void CARendererLayerTree::ClipAndSortingLayer::CommitToCA(
    CALayer* superlayer,
    ClipAndSortingLayer* old_layer,
    float scale_factor) {
  bool update_is_clipped = true;
  bool update_clip_rect = true;
  if (old_layer) {
    DCHECK(old_layer->clipping_ca_layer_);
    DCHECK(old_layer->rounded_corner_ca_layer_);
    std::swap(clipping_ca_layer_, old_layer->clipping_ca_layer_);
    std::swap(rounded_corner_ca_layer_, old_layer->rounded_corner_ca_layer_);
    update_is_clipped = old_layer->is_clipped_ != is_clipped_;
    update_clip_rect = update_is_clipped || old_layer->clip_rect_ != clip_rect_;
  } else {
    clipping_ca_layer_.reset([[CALayer alloc] init]);
    [clipping_ca_layer_ setAnchorPoint:CGPointZero];
    [superlayer addSublayer:clipping_ca_layer_];
    rounded_corner_ca_layer_.reset([[CALayer alloc] init]);
    [rounded_corner_ca_layer_ setAnchorPoint:CGPointZero];
    [clipping_ca_layer_ addSublayer:rounded_corner_ca_layer_];
  }

  if (!rounded_corner_bounds_.IsEmpty()) {
    if (!old_layer ||
        old_layer->rounded_corner_bounds_ != rounded_corner_bounds_) {
      gfx::RectF dip_rounded_corner_bounds =
          gfx::RectF(rounded_corner_bounds_.rect());
      dip_rounded_corner_bounds.Scale(1 / scale_factor);

      [rounded_corner_ca_layer_ setMasksToBounds:true];

      [rounded_corner_ca_layer_
          setPosition:CGPointMake(dip_rounded_corner_bounds.x(),
                                  dip_rounded_corner_bounds.y())];
      [rounded_corner_ca_layer_
          setBounds:CGRectMake(0, 0, dip_rounded_corner_bounds.width(),
                               dip_rounded_corner_bounds.height())];
      [rounded_corner_ca_layer_
          setSublayerTransform:CATransform3DMakeTranslation(
                                   -dip_rounded_corner_bounds.x(),
                                   -dip_rounded_corner_bounds.y(), 0)];

      [rounded_corner_ca_layer_
          setCornerRadius:rounded_corner_bounds_.GetSimpleRadius() /
                          scale_factor];
    }
  } else {
    [rounded_corner_ca_layer_ setMasksToBounds:false];
    [rounded_corner_ca_layer_ setPosition:CGPointZero];
    [rounded_corner_ca_layer_ setBounds:CGRectZero];
    [rounded_corner_ca_layer_ setSublayerTransform:CATransform3DIdentity];
    [rounded_corner_ca_layer_ setCornerRadius:0];
  }
  if ([clipping_ca_layer_ superlayer] != superlayer) {
    DLOG(ERROR) << "CARendererLayerTree root layer not attached to tree.";
  }

  if (update_is_clipped)
    [clipping_ca_layer_ setMasksToBounds:is_clipped_];

  if (update_clip_rect) {
    if (is_clipped_) {
      gfx::RectF dip_clip_rect = gfx::RectF(clip_rect_);
      dip_clip_rect.Scale(1 / scale_factor);
      [clipping_ca_layer_
          setPosition:CGPointMake(dip_clip_rect.x(), dip_clip_rect.y())];
      [clipping_ca_layer_ setBounds:CGRectMake(0, 0, dip_clip_rect.width(),
                                               dip_clip_rect.height())];
      [clipping_ca_layer_
          setSublayerTransform:CATransform3DMakeTranslation(
                                   -dip_clip_rect.x(), -dip_clip_rect.y(), 0)];
    } else {
      [clipping_ca_layer_ setPosition:CGPointZero];
      [clipping_ca_layer_ setBounds:CGRectZero];
      [clipping_ca_layer_ setSublayerTransform:CATransform3DIdentity];
    }
  }

  for (size_t i = 0; i < transform_layers_.size(); ++i) {
    TransformLayer* old_transform_layer = nullptr;
    if (old_layer && i < old_layer->transform_layers_.size())
      old_transform_layer = &old_layer->transform_layers_[i];
    transform_layers_[i].CommitToCA(rounded_corner_ca_layer_,
                                    old_transform_layer, scale_factor);
  }
}

void CARendererLayerTree::TransformLayer::CommitToCA(CALayer* superlayer,
                                                     TransformLayer* old_layer,
                                                     float scale_factor) {
  bool update_transform = true;
  if (old_layer) {
    DCHECK(old_layer->ca_layer_);
    std::swap(ca_layer_, old_layer->ca_layer_);
    update_transform = old_layer->transform_ != transform_;
  } else {
    ca_layer_.reset([[CATransformLayer alloc] init]);
    [superlayer addSublayer:ca_layer_];
  }
  DCHECK_EQ([ca_layer_ superlayer], superlayer);

  if (update_transform) {
    gfx::Transform pre_scale;
    gfx::Transform post_scale;
    pre_scale.Scale(1 / scale_factor, 1 / scale_factor);
    post_scale.Scale(scale_factor, scale_factor);
    gfx::Transform conjugated_transform = pre_scale * transform_ * post_scale;

    CATransform3D ca_transform =
        conjugated_transform.matrix().ToCATransform3D();
    [ca_layer_ setTransform:ca_transform];
  }

  for (size_t i = 0; i < content_layers_.size(); ++i) {
    ContentLayer* old_content_layer = nullptr;
    if (old_layer && i < old_layer->content_layers_.size())
      old_content_layer = &old_layer->content_layers_[i];
    content_layers_[i].CommitToCA(ca_layer_.get(), old_content_layer,
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
  bool update_ca_edge_aa_mask = true;
  bool update_opacity = true;
  bool update_ca_filter = true;
  if (old_layer && old_layer->type_ == type_) {
    DCHECK(old_layer->ca_layer_);
    std::swap(ca_layer_, old_layer->ca_layer_);
    std::swap(av_layer_, old_layer->av_layer_);
    update_contents = old_layer->io_surface_ != io_surface_ ||
                      old_layer->cv_pixel_buffer_ != cv_pixel_buffer_ ||
                      old_layer->solid_color_contents_ != solid_color_contents_;
    update_contents_rect = old_layer->contents_rect_ != contents_rect_;
    update_rect = old_layer->rect_ != rect_;
    update_background_color = old_layer->background_color_ != background_color_;
    update_ca_edge_aa_mask = old_layer->ca_edge_aa_mask_ != ca_edge_aa_mask_;
    update_opacity = old_layer->opacity_ != opacity_;
    update_ca_filter = old_layer->ca_filter_ != ca_filter_;
  } else {
    switch (type_) {
      case CALayerType::kHDRCopier:
        ca_layer_.reset(metal::CreateHDRCopierLayer());
        break;
      case CALayerType::kVideo:
        av_layer_.reset([[AVSampleBufferDisplayLayer alloc] init]);
        ca_layer_.reset([av_layer_ retain]);
        [av_layer_ setVideoGravity:AVLayerVideoGravityResize];
        if (protected_video_type_ != gfx::ProtectedVideoType::kClear) {
          if (@available(macOS 10.15, *)) {
            [av_layer_ setPreventsCapture:true];
          }
        }
        break;
      case CALayerType::kDefault:
        ca_layer_.reset([[CALayer alloc] init]);
    }
    [ca_layer_ setAnchorPoint:CGPointZero];
    if (old_layer && old_layer->ca_layer_)
      [superlayer replaceSublayer:old_layer->ca_layer_ with:ca_layer_];
    else
      [superlayer addSublayer:ca_layer_];
  }
  DCHECK_EQ([ca_layer_ superlayer], superlayer);
  bool update_anything = update_contents || update_contents_rect ||
                         update_rect || update_background_color ||
                         update_ca_edge_aa_mask || update_opacity ||
                         update_ca_filter;

  switch (type_) {
    case CALayerType::kHDRCopier:
      if (update_contents) {
        metal::UpdateHDRCopierLayer(ca_layer_.get(), io_surface_.get(),
                                    io_surface_color_space_);
      }
      break;
    case CALayerType::kVideo:
      if (update_contents) {
        bool result = false;
        if (cv_pixel_buffer_) {
          result = AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
              av_layer_, cv_pixel_buffer_);
          if (!result) {
            LOG(ERROR)
                << "AVSampleBufferDisplayLayerEnqueueCVPixelBuffer failed";
          }
        } else {
          result = AVSampleBufferDisplayLayerEnqueueIOSurface(
              av_layer_, io_surface_, io_surface_color_space_);
          if (!result) {
            LOG(ERROR) << "AVSampleBufferDisplayLayerEnqueueIOSurface failed";
          }
        }
        // TODO(ccameron): Recreate the AVSampleBufferDisplayLayer on failure.
        // This is not being done yet, to determine if this happens concurrently
        // with video flickering.
        // https://crbug.com/702369
      }
      break;
    case CALayerType::kDefault:
      if (update_contents) {
        if (io_surface_) {
          [ca_layer_ setContents:static_cast<id>(io_surface_.get())];
        } else if (solid_color_contents_) {
          [ca_layer_ setContents:solid_color_contents_->GetContents()];
        } else {
          [ca_layer_ setContents:nil];
        }
        if ([ca_layer_ respondsToSelector:(@selector(setContentsScale:))])
          [ca_layer_ setContentsScale:scale_factor];
      }
      break;
  }

  if (update_contents_rect) {
    if (type_ != CALayerType::kVideo)
      [ca_layer_ setContentsRect:contents_rect_.ToCGRect()];
  }
  if (update_rect) {
    gfx::RectF dip_rect = gfx::RectF(rect_);
    dip_rect.Scale(1 / scale_factor);
    [ca_layer_ setPosition:CGPointMake(dip_rect.x(), dip_rect.y())];
    [ca_layer_ setBounds:CGRectMake(0, 0, dip_rect.width(), dip_rect.height())];
  }
  if (update_background_color) {
    CGFloat rgba_color_components[4] = {
        SkColorGetR(background_color_) / 255.,
        SkColorGetG(background_color_) / 255.,
        SkColorGetB(background_color_) / 255.,
        SkColorGetA(background_color_) / 255.,
    };
    base::ScopedCFTypeRef<CGColorRef> srgb_background_color(CGColorCreate(
        CGColorSpaceCreateWithName(kCGColorSpaceSRGB), rgba_color_components));
    [ca_layer_ setBackgroundColor:srgb_background_color];
  }
  if (update_ca_edge_aa_mask)
    [ca_layer_ setEdgeAntialiasingMask:ca_edge_aa_mask_];
  if (update_opacity)
    [ca_layer_ setOpacity:opacity_];
  if (update_ca_filter) {
    [ca_layer_ setMagnificationFilter:ca_filter_];
    [ca_layer_ setMinificationFilter:ca_filter_];
  }

  static bool show_borders = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowMacOverlayBorders);
  static bool fill_layers = false;
  if (show_borders || fill_layers) {
    uint32_t pixel_format =
        io_surface_ ? IOSurfaceGetPixelFormat(io_surface_) : 0;
    float red = 0;
    float green = 0;
    float blue = 0;
    switch (type_) {
      case CALayerType::kHDRCopier:
        // Blue represents a copied HDR layer.
        blue = 1.0;
        break;
      case CALayerType::kVideo:
        switch (pixel_format) {
          case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            // Yellow is NV12 AVSampleBufferDisplayLayer
            red = green = 1;
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
            // Cyan is P010 AVSampleBufferDisplayLayer
            green = blue = 1;
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      case CALayerType::kDefault:
        switch (pixel_format) {
          case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            // Green is NV12 AVSampleBufferDisplayLayer
            green = 1;
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
            // Red is P010 AVSampleBufferDisplayLayer
            red = 1;
            break;
          case 0:
            // Grey is no IOSurface (a solid color layer).
            red = green = blue = 0.5;
            break;
          default:
            // Magenta is a non-video IOSurface.
            red = blue = 1;
            break;
        }
        break;
    }

    // If content did not change this frame, then use 0.5 opacity and a 1 pixel
    // border. If it did change, then use full opacity and a 2 pixel border.
    float alpha = update_anything ? 1.f : 0.5f;
    [ca_layer_ setBorderWidth:update_anything ? 2 : 1];

    // Set the layer color based on usage.
    base::ScopedCFTypeRef<CGColorRef> color(
        CGColorCreateGenericRGB(red, green, blue, alpha));
    [ca_layer_ setBorderColor:color];

    // Flash indication of updates.
    if (fill_layers) {
      color.reset(CGColorCreateGenericRGB(red, green, blue, 1.0));
      if (!update_indicator_layer_)
        update_indicator_layer_.reset([[CALayer alloc] init]);
      if (update_anything) {
        [update_indicator_layer_ setBackgroundColor:color];
        [update_indicator_layer_ setOpacity:0.25];
        [ca_layer_ addSublayer:update_indicator_layer_];
        [update_indicator_layer_
            setFrame:CGRectMake(0, 0, CGRectGetWidth([ca_layer_ bounds]),
                                CGRectGetHeight([ca_layer_ bounds]))];
      } else {
        [update_indicator_layer_ setOpacity:0.1];
      }
    }
  }
}

}  // namespace ui
