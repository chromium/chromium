// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"

#import <AVFoundation/AVFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <GLES2/gl2extchromium.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/metal_util/hdr_copier_layer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/hdr_metadata_mac.h"
#include "ui/gl/ca_renderer_layer_params.h"

namespace ui {

// Transitioning between AVSampleBufferDisplayLayer and CALayer with IOSurface
// contents can cause flickering.
// https://crbug.com/1441762
BASE_FEATURE(kFullscreenLowPowerBackdropMac,
             "FullscreenLowPowerBackdropMac",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Show borders around RenderPassDrawQuad CALayers. which is the output of a
// non-root render pass.
BASE_FEATURE(kShowMacRenderPassDrawQuadBorders,
             "ShowMacRenderPassDrawQuadBorders",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

namespace {

class ComparatorSkColor4f {
 public:
  bool operator()(const SkColor4f& a, const SkColor4f& b) const {
    return std::tie(a.fR, a.fG, a.fB, a.fA) < std::tie(b.fR, b.fG, b.fB, b.fA);
  }
};

// This will enqueue |io_surface| to be drawn by |av_layer|. This will
// retain |cv_pixel_buffer| until it is no longer being displayed.
bool AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
    AVSampleBufferDisplayLayer* av_layer,
    CVPixelBufferRef cv_pixel_buffer) {
  base::apple::ScopedCFTypeRef<CMVideoFormatDescriptionRef> video_info;
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

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
  os_status = CMSampleBufferCreateForImageBuffer(
      nullptr, cv_pixel_buffer, YES, nullptr, nullptr, video_info.get(),
      &timing_info, sample_buffer.InitializeInto());
  if (os_status != noErr) {
    LOG(ERROR) << "CMSampleBufferCreateForImageBuffer failed with "
               << os_status;
    return false;
  }

  // Specify to display immediately via the sample buffer attachments.
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(
      sample_buffer.get(), /*createIfNecessary=*/YES);
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

  [av_layer enqueueSampleBuffer:sample_buffer.get()];

  switch (av_layer.status) {
    case AVQueuedSampleBufferRenderingStatusUnknown:
      LOG(ERROR) << "AVSampleBufferDisplayLayer has status unknown, but should "
                    "be rendering.";
      return false;
    case AVQueuedSampleBufferRenderingStatusFailed:
      LOG(ERROR) << "AVSampleBufferDisplayLayer has status failed, error: "
                 << base::SysNSStringToUTF8(av_layer.error.description);
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
    const gfx::ColorSpace& io_surface_color_space,
    std::optional<gfx::HDRMetadata> hdr_metadata) {
  CVReturn cv_return = kCVReturnSuccess;

  base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer;
  cv_return = CVPixelBufferCreateWithIOSurface(
      nullptr, io_surface, /*pixelBufferAttributes=*/nullptr,
      cv_pixel_buffer.InitializeInto());
  if (cv_return != kCVReturnSuccess) {
    LOG(ERROR) << "CVPixelBufferCreateWithIOSurface failed with " << cv_return;
    return false;
  }

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
    CVBufferSetAttachment(cv_pixel_buffer.get(),
                          kCVImageBufferColorPrimariesKey,
                          kCVImageBufferColorPrimaries_ITU_R_2020,
                          kCVAttachmentMode_ShouldPropagate);
    CVBufferSetAttachment(cv_pixel_buffer.get(), kCVImageBufferYCbCrMatrixKey,
                          kCVImageBufferYCbCrMatrix_ITU_R_2020,
                          kCVAttachmentMode_ShouldPropagate);
    switch (io_surface_color_space.GetTransferID()) {
      case gfx::ColorSpace::TransferID::HLG:
        CVBufferSetAttachment(cv_pixel_buffer.get(),
                              kCVImageBufferTransferFunctionKey,
                              kCVImageBufferTransferFunction_ITU_R_2100_HLG,
                              kCVAttachmentMode_ShouldPropagate);
        break;
      case gfx::ColorSpace::TransferID::PQ:
        CVBufferSetAttachment(cv_pixel_buffer.get(),
                              kCVImageBufferTransferFunctionKey,
                              kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
                              kCVAttachmentMode_ShouldPropagate);
        CVBufferSetAttachment(
            cv_pixel_buffer.get(), kCVImageBufferMasteringDisplayColorVolumeKey,
            gfx::GenerateMasteringDisplayColorVolume(hdr_metadata).get(),
            kCVAttachmentMode_ShouldPropagate);
        CVBufferSetAttachment(
            cv_pixel_buffer.get(), kCVImageBufferContentLightLevelInfoKey,
            gfx::GenerateContentLightLevelInfo(hdr_metadata).get(),
            kCVAttachmentMode_ShouldPropagate);
        break;
      default:
        break;
    }
  }

  return AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(av_layer,
                                                        cv_pixel_buffer.get());
}

CATransform3D ToCATransform3D(const gfx::Transform& t) {
  CATransform3D result;
  auto* dst = &result.m11;
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      *dst++ = t.rc(row, col);
    }
  }
  return result;
}

}  // namespace

class CARendererLayerTree::SolidColorContents
    : public base::RefCounted<CARendererLayerTree::SolidColorContents> {
 public:
  static scoped_refptr<SolidColorContents> Get(SkColor4f color);
  id GetContents() const;
  IOSurfaceRef GetIOSurfaceRef() const;

 private:
  friend class base::RefCounted<SolidColorContents>;

  SolidColorContents(SkColor4f color,
                     base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface);
  ~SolidColorContents();

  using Map = std::map<SkColor4f,
                       CARendererLayerTree::SolidColorContents*,
                       ComparatorSkColor4f>;
  static Map* GetMap();

  const SkColor4f color_;
  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
};

// static
scoped_refptr<CARendererLayerTree::SolidColorContents>
CARendererLayerTree::SolidColorContents::Get(SkColor4f color) {
  const int kSolidColorContentsSize = 16;

  auto* map = GetMap();
  auto found = map->find(color);
  if (found != map->end())
    return found->second;

  const gfx::Size size(kSolidColorContentsSize, kSolidColorContentsSize);
  gfx::BufferFormat buffer_format = gfx::BufferFormat::BGRA_8888;
  SkColorType color_type = kBGRA_8888_SkColorType;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  // Use P3 for non-sRGB solid colors, because that is likely the tile
  // rasterization color space.
  // https://crbug.com/1376717
  if (!color.fitsInBytes()) {
    color_space = gfx::ColorSpace::CreateDisplayP3D65();
  }

  base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface =
      CreateIOSurface(size, buffer_format);
  if (!io_surface)
    return nullptr;
  IOSurfaceSetColorSpace(io_surface.get(), color_space);

  {
    size_t bytes_per_row =
        IOSurfaceGetBytesPerRowOfPlane(io_surface.get(), /*planeIndex=*/0);
    IOSurfaceLock(io_surface.get(), /*options=*/0, /*seed=*/nullptr);
    char* base_address =
        reinterpret_cast<char*>(IOSurfaceGetBaseAddress(io_surface.get()));
    SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                         color_type, kPremul_SkAlphaType);
    auto canvas = SkCanvas::MakeRasterDirect(info, base_address, bytes_per_row);
    DCHECK(canvas);
    canvas->clear(color);

    IOSurfaceUnlock(io_surface.get(), /*options=*/0, /*seed=*/nullptr);
  }
  return new SolidColorContents(color, io_surface);
}

id CARendererLayerTree::SolidColorContents::GetContents() const {
  return (__bridge id)io_surface_.get();
}

IOSurfaceRef CARendererLayerTree::SolidColorContents::GetIOSurfaceRef() const {
  return io_surface_.get();
}

CARendererLayerTree::SolidColorContents::SolidColorContents(
    SkColor4f color,
    base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface)
    : color_(color), io_surface_(std::move(io_surface)) {
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
CARendererLayerTree::SolidColorContents::Map*
CARendererLayerTree::SolidColorContents::GetMap() {
  static base::NoDestructor<Map> map;
  return map.get();
}

CARendererLayerTree::CARendererLayerTree(
    bool allow_av_sample_buffer_display_layer,
    bool allow_solid_color_layers)
    : allow_av_sample_buffer_display_layer_(
          allow_av_sample_buffer_display_layer),
      allow_solid_color_layers_(allow_solid_color_layers) {}
CARendererLayerTree::~CARendererLayerTree() = default;

bool CARendererLayerTree::ScheduleCALayer(const CARendererLayerParams& params) {
  if (has_committed_) {
    DLOG(ERROR) << "ScheduleCALayer called after CommitScheduledCALayers.";
    return false;
  }
  return root_layer_.AddContentLayer(params);
}

void CARendererLayerTree::CommitScheduledCALayers(
    CALayer* superlayer,
    std::unique_ptr<CARendererLayerTree> old_tree,
    const gfx::Size& pixel_size,
    float scale_factor) {
  TRACE_EVENT0("gpu", "CARendererLayerTree::CommitScheduledCALayers");
  scale_factor_ = scale_factor;

  // The CALayerTree optimization reuses the matched CALayer from the previous.
  MatchLayersToOldTree(old_tree.get());

  root_layer_.CommitToCA(superlayer, pixel_size);
  // If there are any extra CALayers in |old_tree| that were not stolen by this
  // tree, they will be removed from the CALayer tree in this deallocation.
  old_tree.reset();
  has_committed_ = true;
}

void CARendererLayerTree::MatchLayersToOldTree(CARendererLayerTree* old_tree) {
  if (!old_tree)
    return;
  DCHECK(old_tree->has_committed_);

  // Match the root layer.
  if (old_tree->scale_factor_ != scale_factor_)
    return;

  DCHECK(ca_layer_map_.empty()) << "ca_layer_map_ is not empty.";

  root_layer_.old_layer_ =
      old_tree->root_layer_.weak_factory_for_new_layer_.GetWeakPtr();

  int layer_order = 0;
  int last_old_layer_order;
  for (auto& clip_and_sorting_layer : root_layer_.clip_and_sorting_layers_) {
    for (auto& transform_layer : clip_and_sorting_layer.transform_layers_) {
      for (auto& content_layer : transform_layer.content_layers_) {
        content_layer.UpdateMapAndMatchOldLayers(
            old_tree->ca_layer_map_, layer_order, last_old_layer_order);
      }
    }
  }

  // Try to match unused old layers to saving reallocation of CALayer even
  // though the IOSurface will be different.
  root_layer_.CALayerFallBack();
}

void CARendererLayerTree::ContentLayer::UpdateMapAndMatchOldLayers(
    CALayerMap& old_ca_layer_map,
    int& layer_order,
    int& last_old_layer_order) {
  IOSurfaceRef io_surface_ref = io_surface_.get();

  if (!io_surface_ref)
    return;

  // Add this ContentLayer to the map for this tree.
  tree()->ca_layer_map_.insert(
      std::make_pair(io_surface_ref, weak_factory_for_new_layer_.GetWeakPtr()));

  layer_order_ = ++layer_order;

  // Find a matched io surface from the old tree.
  auto it = old_ca_layer_map.find(io_surface_ref);
  if (it == old_ca_layer_map.end())
    return;

  auto matched_content_layer = it->second;

  // Should we try multimap for the same IOSurface used twice in the old tree?
  if (matched_content_layer->ca_layer_used_)
    return;

  auto* matched_transform_layer = matched_content_layer->parent_layer_;
  auto* matched_clip_layer = matched_transform_layer->parent_layer_;

  // If the parent is different, the superlayer must have changed. It should be
  // removed from its superlayer and inserted back to the new superlayer in
  // CommitToCa().

  // clip_and_sorting_layer
  if (!parent_layer_->parent_layer_->old_layer_) {
    if (!matched_clip_layer->ca_layer_used_) {
      // Use this clip_and_sorting_layer as an old layer.
      parent_layer_->parent_layer_->old_layer_ =
          matched_clip_layer->weak_factory_for_new_layer_.GetWeakPtr();
      matched_clip_layer->ca_layer_used_ = true;
    } else {
      [matched_transform_layer->ca_layer_ removeFromSuperlayer];
    }
  }

  // transform_layer
  if (!parent_layer_->old_layer_) {
    if (!matched_transform_layer->ca_layer_used_) {
      // Use this clip_and_sorting_layer as an old layer.
      parent_layer_->old_layer_ =
          matched_transform_layer->weak_factory_for_new_layer_.GetWeakPtr();
      matched_transform_layer->ca_layer_used_ = true;
    } else {
      [matched_content_layer->ca_layer_ removeFromSuperlayer];
    }
  }

  if (matched_clip_layer != parent_layer_->parent_layer_->old_layer_.get()) {
    [matched_transform_layer->ca_layer_ removeFromSuperlayer];
  }

  if (matched_transform_layer != parent_layer_->old_layer_.get()) {
    [matched_content_layer->ca_layer_ removeFromSuperlayer];
  } else if (matched_content_layer->layer_order_ < last_old_layer_order) {
    // For the content layers with the same superlayer, if the order changes.
    // this matched old layer should be removed from its superlayer first.
    [matched_content_layer->ca_layer_ removeFromSuperlayer];
    [matched_transform_layer->ca_layer_ removeFromSuperlayer];
    [matched_clip_layer->clipping_ca_layer_ removeFromSuperlayer];
  }

  // This is the one to be used as an old layer.
  old_layer_ = matched_content_layer;
  old_layer_->ca_layer_used_ = true;
  last_old_layer_order = matched_content_layer->layer_order_;

  // Debug print
  std::string str;
  if (matched_transform_layer->ca_layer_.superlayer == nil) {
    str = ", transform layer's superlayer has changed";
  }
  if (matched_content_layer->ca_layer_.superlayer == nil) {
    str = ",  clip layer's superlayer has changed ";
  }
}

void CARendererLayerTree::RootLayer::CALayerFallBack() {
  if (old_layer_) {
    auto old_layer_child_it = old_layer_->clip_and_sorting_layers_.begin();
    for (auto& child : clip_and_sorting_layers_) {
      if (child.old_layer_) {
        // Remove any children of `old_layer_` that appear before
        // `child.old_layer_`. They may be re-parented (in the case of
        // transposed content), or removed entirely.
        while (old_layer_child_it !=
               old_layer_->clip_and_sorting_layers_.end()) {
          auto* old_layer_child = &(*old_layer_child_it);
          if (child.old_layer_.get() == old_layer_child) {
            ++old_layer_child_it;
            break;
          }
          [old_layer_child->clipping_ca_layer_ removeFromSuperlayer];
          ++old_layer_child_it;
        }
      } else {
        // If `child.old_layer_` is unset, then set it to the next child of
        // `old_layer_` (if it exists and has not been taken).
        if (old_layer_child_it != old_layer_->clip_and_sorting_layers_.end()) {
          if (!old_layer_child_it->ca_layer_used_) {
            child.old_layer_ =
                old_layer_child_it->weak_factory_for_new_layer_.GetWeakPtr();
            ++old_layer_child_it;
          } else {
            // keep the current |old_layer_child_it|.
          }
        }
      }

      child.CALayerFallBack();
    }
  } else {
    for (auto& child : clip_and_sorting_layers_)
      child.CALayerFallBack();
  }
}

void CARendererLayerTree::ClipAndSortingLayer::CALayerFallBack() {
  if (old_layer_) {
    auto old_layer_child_it = old_layer_->transform_layers_.begin();
    for (auto& child : transform_layers_) {
      if (child.old_layer_) {
        // Remove any children of `old_layer_` that appear before
        // `child.old_layer_`. They may be re-parented (in the case of
        // transposed content), or removed entirely.
        while (old_layer_child_it != old_layer_->transform_layers_.end()) {
          auto* old_layer_child = &(*old_layer_child_it);
          if (child.old_layer_.get() == old_layer_child) {
            ++old_layer_child_it;
            break;
          }
          [old_layer_child->ca_layer_ removeFromSuperlayer];
          ++old_layer_child_it;
        }
      } else {
        // If `child.old_layer_` is unset, then set it to the next child of
        // `old_layer_` (if it exists and has not been taken).
        if (old_layer_child_it != old_layer_->transform_layers_.end()) {
          if (!old_layer_child_it->ca_layer_used_) {
            child.old_layer_ =
                old_layer_child_it->weak_factory_for_new_layer_.GetWeakPtr();
            ++old_layer_child_it;
          } else {
            // keep the current |old_layer_child_it|.
          }
        }
      }

      child.CALayerFallBack();
    }
  } else {
    for (auto& child : transform_layers_)
      child.CALayerFallBack();
  }
}

void CARendererLayerTree::TransformLayer::CALayerFallBack() {
  if (old_layer_) {
    auto old_layer_child_it = old_layer_->content_layers_.begin();
    for (auto& child : content_layers_) {
      if (child.old_layer_) {
        // Remove any children of `old_layer_` that appear before
        // `child.old_layer_`. They may be re-parented (in the case of
        // transposed content), or removed entirely.
        while (old_layer_child_it != old_layer_->content_layers_.end()) {
          auto* old_layer_child = &(*old_layer_child_it);
          if (child.old_layer_.get() == old_layer_child) {
            ++old_layer_child_it;
            break;
          }
          [old_layer_child->ca_layer_ removeFromSuperlayer];
          ++old_layer_child_it;
        }
      } else {
        // If `child.old_layer_` is unset, then set it to the next child of
        // `old_layer_` (if it exists and has not been taken).
        if (old_layer_child_it != old_layer_->content_layers_.end()) {
          if (!old_layer_child_it->ca_layer_used_) {
            child.old_layer_ =
                old_layer_child_it->weak_factory_for_new_layer_.GetWeakPtr();
            ++old_layer_child_it;
          } else {
            // keep the current |old_layer_child_it|.
          }
        }
      }
    }
  }
}

bool CARendererLayerTree::RootLayer::WantsFullscreenLowPowerBackdrop() const {
  if (!base::FeatureList::IsEnabled(kFullscreenLowPowerBackdropMac)) {
    return false;
  }

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
        if (content_layer.background_color_ != SkColors::kBlack &&
            content_layer.background_color_ != SkColors::kTransparent) {
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

id CARendererLayerTree::ContentsForSolidColorForTesting(SkColor4f color) {
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
      root_layer_.clip_and_sorting_layers_.front();
  size_t transform_count = clip_and_sorting.transform_layers_.size();
  if (transform_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "TransformLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const TransformLayer& transform = clip_and_sorting.transform_layers_.front();
  size_t content_count = transform.content_layers_.size();
  if (content_count != 1) {
    DLOG(ERROR) << "Can only return contents IOSurface when there is 1 "
                << "ContentLayer, there are " << transform_count << ".";
    return nullptr;
  }
  const ContentLayer& content = transform.content_layers_.front();
  return content.io_surface_.get();
}

CARendererLayerTree::RootLayer::RootLayer(CARendererLayerTree* tree)
    : tree_(tree) {}

// Note that for all destructors, the the CALayer will have been reset to nil if
// another layer has taken it.
CARendererLayerTree::RootLayer::~RootLayer() {
  [ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::ClipAndSortingLayer::ClipAndSortingLayer(
    RootLayer* parent_layer,
    bool is_clipped,
    gfx::Rect clip_rect,
    gfx::RRectF rounded_corner_bounds_arg,
    unsigned sorting_context_id,
    bool is_singleton_sorting_context)
    : parent_layer_(parent_layer),
      is_clipped_(is_clipped),
      clip_rect_(clip_rect),
      rounded_corner_bounds_(rounded_corner_bounds_arg),
      sorting_context_id_(sorting_context_id),
      is_singleton_sorting_context_(is_singleton_sorting_context) {}

CARendererLayerTree::ClipAndSortingLayer::~ClipAndSortingLayer() {
  [clipping_ca_layer_ removeFromSuperlayer];
  [rounded_corner_ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::TransformLayer::TransformLayer(
    ClipAndSortingLayer* parent_layer,
    const gfx::Transform& transform)
    : parent_layer_(parent_layer), transform_(transform) {}

CARendererLayerTree::TransformLayer::~TransformLayer() {
  [ca_layer_ removeFromSuperlayer];
}

CARendererLayerTree::ContentLayer::ContentLayer(
    TransformLayer* parent_layer,
    base::apple::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
    const gfx::RectF& contents_rect,
    const gfx::Rect& rect,
    SkColor4f background_color,
    const gfx::ColorSpace& io_surface_color_space,
    unsigned edge_aa_mask,
    float opacity,
    bool nearest_neighbor_filter,
    const gfx::HDRMetadata& hdr_metadata,
    gfx::ProtectedVideoType protected_video_type,
    bool is_render_pass_draw_quad)
    : parent_layer_(parent_layer),
      io_surface_(io_surface),
      cv_pixel_buffer_(cv_pixel_buffer),
      contents_rect_(contents_rect),
      rect_(rect),
      background_color_(background_color),
      io_surface_color_space_(io_surface_color_space),
      ca_edge_aa_mask_(0),
      opacity_(opacity),
      ca_filter_(nearest_neighbor_filter ? kCAFilterNearest : kCAFilterLinear),
      hdr_metadata_(hdr_metadata),
      protected_video_type_(protected_video_type),
      is_render_pass_draw_quad_(is_render_pass_draw_quad) {
  // On macOS 10.12, solid color layers are not color converted to the output
  // monitor color space, but IOSurface-backed layers are color converted. Note
  // that this is only the case when the CALayers are shared across processes.
  // To make colors consistent across both solid color and IOSurface-backed
  // layers, use a cache of solid-color IOSurfaces as contents. Black and
  // transparent layers must use real colors to be eligible for low power
  // detachment in fullscreen.
  // https://crbug.com/633805
  if (!io_surface && !tree()->allow_solid_color_layers_ &&
      background_color_ != SkColors::kBlack &&
      background_color_ != SkColors::kTransparent) {
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
  if (metal::ShouldUseHDRCopier(io_surface.get(), hdr_metadata_,
                                io_surface_color_space)) {
    type_ = CALayerType::kHDRCopier;
  } else if (io_surface) {
    // Only allow YUV frames which fill the layer's contents or protected
    // video to be promoted to AV layers.
    if (tree()->allow_av_sample_buffer_display_layer_) {
      if (contents_rect == gfx::RectF(0, 0, 1, 1)) {
        switch (IOSurfaceGetPixelFormat(io_surface.get())) {
          case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
          case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
            type_ = CALayerType::kVideo;
            video_type_can_downgrade_ = !io_surface_color_space.IsHDR();
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
            type_ = CALayerType::kVideo;
            video_type_can_downgrade_ = false;
            break;
          default:
            break;
        }
      }

      if (protected_video_type_ != gfx::ProtectedVideoType::kClear) {
        type_ = CALayerType::kVideo;
        video_type_can_downgrade_ = false;
      }
    }
  }

  if (type_ == CALayerType::kVideo) {
    // If the layer's aspect ratio could be made to match the video's aspect
    // ratio by expanding either dimension by a fractional pixel, do so. The
    // mismatch probably resulted from rounding the dimensions to integers. This
    // works around a macOS bug which breaks detached fullscreen playback of
    // slightly distorted videos (https://crbug.com/792632).
    const auto av_rect(
        cv_pixel_buffer
            ? gfx::RectF(CVPixelBufferGetWidth(cv_pixel_buffer.get()),
                         CVPixelBufferGetHeight(cv_pixel_buffer.get()))
            : gfx::RectF(IOSurfaceGetWidth(io_surface.get()),
                         IOSurfaceGetHeight(io_surface.get())));
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

CARendererLayerTree::ContentLayer::~ContentLayer() {
  [ca_layer_ removeFromSuperlayer];
  [update_indicator_layer_ removeFromSuperlayer];
}

bool CARendererLayerTree::RootLayer::AddContentLayer(
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
    clip_and_sorting_layers_.emplace_back(
        this, params.is_clipped, params.clip_rect, params.rounded_corner_bounds,
        params.sorting_context_id, is_singleton_sorting_context);
  }
  clip_and_sorting_layers_.back().AddContentLayer(params);
  return true;
}

void CARendererLayerTree::ClipAndSortingLayer::AddContentLayer(
    const CARendererLayerParams& params) {
  bool needs_new_transform_layer = true;
  if (!transform_layers_.empty()) {
    const TransformLayer& current_layer = transform_layers_.back();
    if (current_layer.transform_ == params.transform)
      needs_new_transform_layer = false;
  }
  if (needs_new_transform_layer)
    transform_layers_.emplace_back(this, params.transform);

  transform_layers_.back().AddContentLayer(params);
}

void CARendererLayerTree::TransformLayer::AddContentLayer(
    const CARendererLayerParams& params) {
  content_layers_.emplace_back(
      this, params.io_surface, base::apple::ScopedCFTypeRef<CVPixelBufferRef>(),
      params.contents_rect, params.rect, params.background_color,
      params.io_surface_color_space, params.edge_aa_mask, params.opacity,
      params.nearest_neighbor_filter, params.hdr_metadata,
      params.protected_video_type, params.is_render_pass_draw_quad);
}

void CARendererLayerTree::RootLayer::CommitToCA(CALayer* superlayer,
                                                const gfx::Size& pixel_size) {
  if (old_layer_) {
    DCHECK(old_layer_->ca_layer_);
    std::swap(ca_layer_, old_layer_->ca_layer_);
  } else {
    ca_layer_ = [[CALayer alloc] init];
    ca_layer_.anchorPoint = CGPointZero;
    superlayer.sublayers = nil;
    [superlayer addSublayer:ca_layer_];
    superlayer.borderWidth = 0;
  }

  DCHECK_EQ(ca_layer_.superlayer, superlayer)
      << "CARendererLayerTree root layer not attached to tree.";

  if (WantsFullscreenLowPowerBackdrop()) {
    // In fullscreen low power mode there exists a single video layer on a
    // solid black background.
    const gfx::RectF bg_rect(
        ScaleSize(gfx::SizeF(pixel_size), 1 / tree_->scale_factor_));
    if (gfx::RectF(ca_layer_.frame) != bg_rect) {
      ca_layer_.frame = bg_rect.ToCGRect();
    }
    if (!ca_layer_.backgroundColor) {
      ca_layer_.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    }
  } else {
    if (gfx::RectF(ca_layer_.frame) != gfx::RectF()) {
      ca_layer_.frame = CGRectZero;
    }
    if (ca_layer_.backgroundColor) {
      ca_layer_.backgroundColor = nil;
    }
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

  CALayer* last_committed_clip_ca_layer = nullptr;
  for (auto& child_layer : clip_and_sorting_layers_) {
    child_layer.CommitToCA(last_committed_clip_ca_layer);
    last_committed_clip_ca_layer = child_layer.clipping_ca_layer_;
  }
}

void CARendererLayerTree::ClipAndSortingLayer::CommitToCA(
    CALayer* last_committed_clip_ca_layer) {
  CALayer* superlayer = parent_layer_->ca_layer_;
  bool update_is_clipped = true;
  bool update_clip_rect = true;
  if (old_layer_) {
    DCHECK(old_layer_->clipping_ca_layer_);
    DCHECK(old_layer_->rounded_corner_ca_layer_);
    std::swap(clipping_ca_layer_, old_layer_->clipping_ca_layer_);
    std::swap(rounded_corner_ca_layer_, old_layer_->rounded_corner_ca_layer_);
    update_is_clipped = old_layer_->is_clipped_ != is_clipped_;
    update_clip_rect =
        update_is_clipped || old_layer_->clip_rect_ != clip_rect_;

  } else {
    clipping_ca_layer_ = [[CALayer alloc] init];
    clipping_ca_layer_.anchorPoint = CGPointZero;

    rounded_corner_ca_layer_ = [[CALayer alloc] init];
    rounded_corner_ca_layer_.anchorPoint = CGPointZero;
    [clipping_ca_layer_ addSublayer:rounded_corner_ca_layer_];
  }

  if (clipping_ca_layer_.superlayer != superlayer) {
    DCHECK_EQ(clipping_ca_layer_.superlayer, nil);
    if (last_committed_clip_ca_layer == nullptr) {
      [superlayer insertSublayer:clipping_ca_layer_ atIndex:0];
    } else {
      [superlayer insertSublayer:clipping_ca_layer_
                           above:last_committed_clip_ca_layer];
    }
  }

  if (!rounded_corner_bounds_.IsEmpty()) {
    if (!old_layer_ ||
        old_layer_->rounded_corner_bounds_ != rounded_corner_bounds_) {
      gfx::RectF dip_rounded_corner_bounds =
          gfx::RectF(rounded_corner_bounds_.rect());
      dip_rounded_corner_bounds.Scale(1 / tree()->scale_factor_);

      rounded_corner_ca_layer_.masksToBounds = true;

      rounded_corner_ca_layer_.position = CGPointMake(
          dip_rounded_corner_bounds.x(), dip_rounded_corner_bounds.y());
      rounded_corner_ca_layer_.bounds =
          CGRectMake(0, 0, dip_rounded_corner_bounds.width(),
                     dip_rounded_corner_bounds.height());
      rounded_corner_ca_layer_.sublayerTransform = CATransform3DMakeTranslation(
          -dip_rounded_corner_bounds.x(), -dip_rounded_corner_bounds.y(), 0);

      rounded_corner_ca_layer_.cornerRadius =
          rounded_corner_bounds_.GetSimpleRadius() / tree()->scale_factor_;
    }
  } else {
    rounded_corner_ca_layer_.masksToBounds = false;
    rounded_corner_ca_layer_.position = CGPointZero;
    rounded_corner_ca_layer_.bounds = CGRectZero;
    rounded_corner_ca_layer_.sublayerTransform = CATransform3DIdentity;
    rounded_corner_ca_layer_.cornerRadius = 0;
  }

  DCHECK_EQ(clipping_ca_layer_.superlayer, superlayer)
      << "CARendererLayerTree root layer not attached to tree."
      << "clipping_ca_layer_: " << clipping_ca_layer_
      << " last clilp ca_layer: " << last_committed_clip_ca_layer;

  if (update_is_clipped)
    clipping_ca_layer_.masksToBounds = is_clipped_;

  if (update_clip_rect) {
    if (is_clipped_) {
      gfx::RectF dip_clip_rect = gfx::RectF(clip_rect_);
      dip_clip_rect.Scale(1 / tree()->scale_factor_);
      clipping_ca_layer_.position =
          CGPointMake(dip_clip_rect.x(), dip_clip_rect.y());
      clipping_ca_layer_.bounds =
          CGRectMake(0, 0, dip_clip_rect.width(), dip_clip_rect.height());
      clipping_ca_layer_.sublayerTransform = CATransform3DMakeTranslation(
          -dip_clip_rect.x(), -dip_clip_rect.y(), 0);
    } else {
      clipping_ca_layer_.position = CGPointZero;
      clipping_ca_layer_.bounds = CGRectZero;
      clipping_ca_layer_.sublayerTransform = CATransform3DIdentity;
    }
  }

  CALayer* last_committed_transform_ca_layer = nullptr;
  for (auto& child_layer : transform_layers_) {
    child_layer.CommitToCA(last_committed_transform_ca_layer);
    last_committed_transform_ca_layer = child_layer.ca_layer_;
  }
}

void CARendererLayerTree::TransformLayer::CommitToCA(
    CALayer* last_committed_transform_ca_layer) {
  CALayer* superlayer = parent_layer_->rounded_corner_ca_layer_;
  bool update_transform = true;

  if (old_layer_) {
    DCHECK(old_layer_->ca_layer_);
    std::swap(ca_layer_, old_layer_->ca_layer_);
    update_transform = old_layer_->transform_ != transform_;
  } else {
    ca_layer_ = [[CATransformLayer alloc] init];
  }

  if (ca_layer_.superlayer != superlayer) {
    DCHECK_EQ(ca_layer_.superlayer, nil);
    if (last_committed_transform_ca_layer == nullptr) {
      [superlayer insertSublayer:ca_layer_ atIndex:0];
    } else {
      [superlayer insertSublayer:ca_layer_
                           above:last_committed_transform_ca_layer];
    }
  }

  DCHECK_EQ(ca_layer_.superlayer, superlayer)
      << "ca_layer: " << ca_layer_
      << " last transform ca_layer: " << last_committed_transform_ca_layer;

  if (update_transform) {
    gfx::Transform pre_scale;
    gfx::Transform post_scale;
    pre_scale.Scale(1 / tree()->scale_factor_, 1 / tree()->scale_factor_);
    post_scale.Scale(tree()->scale_factor_, tree()->scale_factor_);
    gfx::Transform conjugated_transform = pre_scale * transform_ * post_scale;

    CATransform3D ca_transform = ToCATransform3D(conjugated_transform);
    ca_layer_.transform = ca_transform;
  }

  CALayer* last_committed_content_ca_layer_ = nullptr;
  for (auto& child_layer : content_layers_) {
    child_layer.CommitToCA(last_committed_content_ca_layer_);
    last_committed_content_ca_layer_ = child_layer.ca_layer_;
  }
}

void CARendererLayerTree::ContentLayer::CommitToCA(
    CALayer* last_committed_ca_layer) {
  CALayer* superlayer = parent_layer_->ca_layer_;
  bool update_contents = true;
  bool update_contents_rect = true;
  bool update_rect = true;
  bool update_background_color = true;
  bool update_ca_edge_aa_mask = true;
  bool update_opacity = true;
  bool update_ca_filter = true;

  if (old_layer_ && old_layer_->type_ == type_) {
    DCHECK(old_layer_->ca_layer_);
    std::swap(ca_layer_, old_layer_->ca_layer_);
    std::swap(av_layer_, old_layer_->av_layer_);
    update_contents =
        old_layer_->io_surface_ != io_surface_ ||
        old_layer_->cv_pixel_buffer_ != cv_pixel_buffer_ ||
        old_layer_->solid_color_contents_ != solid_color_contents_ ||
        old_layer_->hdr_metadata_ != hdr_metadata_;
    update_contents_rect = old_layer_->contents_rect_ != contents_rect_;
    update_rect = old_layer_->rect_ != rect_;
    update_background_color =
        old_layer_->background_color_ != background_color_;
    update_ca_edge_aa_mask = old_layer_->ca_edge_aa_mask_ != ca_edge_aa_mask_;
    update_opacity = old_layer_->opacity_ != opacity_;
    update_ca_filter = old_layer_->ca_filter_ != ca_filter_;
  } else {
    switch (type_) {
      case CALayerType::kHDRCopier:
        ca_layer_ = metal::MakeHDRCopierLayer();
        break;
      case CALayerType::kVideo:
        av_layer_ = [[AVSampleBufferDisplayLayer alloc] init];
        ca_layer_ = av_layer_;
        av_layer_.videoGravity = AVLayerVideoGravityResize;
        if (protected_video_type_ != gfx::ProtectedVideoType::kClear) {
          av_layer_.preventsCapture = true;
        }
        break;
      case CALayerType::kDefault:
        ca_layer_ = [[CALayer alloc] init];
    }
    ca_layer_.anchorPoint = CGPointZero;
  }

  if (ca_layer_.superlayer != superlayer) {
    DCHECK_EQ(ca_layer_.superlayer, nil);
    if (last_committed_ca_layer == nullptr) {
      [superlayer insertSublayer:ca_layer_ atIndex:0];
    } else {
      [superlayer insertSublayer:ca_layer_ above:last_committed_ca_layer];
    }
  }

  DCHECK_EQ(ca_layer_.superlayer, superlayer)
      << " last content ca_layer: " << last_committed_ca_layer;

#if BUILDFLAG(IS_MAC)
  bool update_anything = update_contents || update_contents_rect ||
                         update_rect || update_background_color ||
                         update_ca_edge_aa_mask || update_opacity ||
                         update_ca_filter;
#endif

  switch (type_) {
    case CALayerType::kHDRCopier:
      if (update_contents) {
        metal::UpdateHDRCopierLayer(ca_layer_, io_surface_.get(),
                                    tree()->metal_device_,
                                    tree()->display_hdr_headroom_,
                                    io_surface_color_space_, hdr_metadata_);
      }
      break;
    case CALayerType::kVideo:
      if (update_contents) {
        bool result = false;
        if (cv_pixel_buffer_) {
          result = AVSampleBufferDisplayLayerEnqueueCVPixelBuffer(
              av_layer_, cv_pixel_buffer_.get());
          if (!result) {
            LOG(ERROR)
                << "AVSampleBufferDisplayLayerEnqueueCVPixelBuffer failed";
          }
        } else {
          result = AVSampleBufferDisplayLayerEnqueueIOSurface(
              av_layer_, io_surface_.get(), io_surface_color_space_,
              hdr_metadata_);
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
          ca_layer_.contents = (__bridge id)io_surface_.get();
        } else if (solid_color_contents_) {
          ca_layer_.contents = solid_color_contents_->GetContents();
        } else {
          ca_layer_.contents = nil;
        }
        ca_layer_.contentsScale = tree()->scale_factor_;
      }
      break;
  }

  if (update_contents_rect) {
    if (type_ != CALayerType::kVideo)
      ca_layer_.contentsRect = contents_rect_.ToCGRect();
  }
  if (update_rect) {
    gfx::RectF dip_rect = gfx::RectF(rect_);
    dip_rect.Scale(1 / tree()->scale_factor_);
    ca_layer_.position = CGPointMake(dip_rect.x(), dip_rect.y());
    ca_layer_.bounds = CGRectMake(0, 0, dip_rect.width(), dip_rect.height());
  }
  if (update_background_color) {
    CGFloat rgba_color_components[4] = {
        background_color_.fR,
        background_color_.fG,
        background_color_.fB,
        background_color_.fA,
    };
    base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
        CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB));
    base::apple::ScopedCFTypeRef<CGColorRef> srgb_background_color(
        CGColorCreate(color_space.get(), rgba_color_components));
    ca_layer_.backgroundColor = srgb_background_color.get();
  }
  if (update_ca_edge_aa_mask) {
    ca_layer_.edgeAntialiasingMask = ca_edge_aa_mask_;
  }
  if (update_opacity) {
    ca_layer_.opacity = opacity_;
  }
  if (update_ca_filter) {
    ca_layer_.magnificationFilter = ca_filter_;
    ca_layer_.minificationFilter = ca_filter_;
  }

#if BUILDFLAG(IS_MAC)
  static bool show_overlay_borders =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowMacOverlayBorders);
  static bool show_rpdq_borders =
      base::FeatureList::IsEnabled(kShowMacRenderPassDrawQuadBorders);

  static bool fill_layers = false;
  if (show_overlay_borders || fill_layers ||
      (show_rpdq_borders && is_render_pass_draw_quad_)) {
    uint32_t pixel_format =
        io_surface_ ? IOSurfaceGetPixelFormat(io_surface_.get()) : 0;
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
          case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
            // Yellow is NV12/NV16/NV24 AVSampleBufferDisplayLayer
            red = green = 1;
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
            // Cyan is P010/P210/P410 AVSampleBufferDisplayLayer
            green = blue = 1;
            break;
          default:
            NOTREACHED_IN_MIGRATION();
            break;
        }
        break;
      case CALayerType::kDefault:
        switch (pixel_format) {
          case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
          case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
            // Green is NV12/NV16/NV24 AVSampleBufferDisplayLayer
            green = 1;
            break;
          case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
          case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
            // Red is P010/P210/P410 AVSampleBufferDisplayLayer
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
    // For a RenderPassDrawQuad, use 6 pixel border.
    float alpha = update_anything ? 1.f : 0.5f;
    ca_layer_.borderWidth =
        is_render_pass_draw_quad_ ? 6 : (update_anything ? 2 : 1);

    // Set the layer color based on usage.
    base::apple::ScopedCFTypeRef<CGColorRef> color(
        CGColorCreateGenericRGB(red, green, blue, alpha));
    ca_layer_.borderColor = color.get();

    // Flash indication of updates.
    if (fill_layers) {
      color.reset(CGColorCreateGenericRGB(red, green, blue, 1.0));
      if (!update_indicator_layer_)
        update_indicator_layer_ = [[CALayer alloc] init];
      if (update_anything) {
        update_indicator_layer_.backgroundColor = color.get();
        update_indicator_layer_.opacity = 0.25;
        [ca_layer_ addSublayer:update_indicator_layer_];
        update_indicator_layer_.frame =
            CGRectMake(0, 0, CGRectGetWidth(ca_layer_.bounds),
                       CGRectGetHeight(ca_layer_.bounds));
      } else {
        [update_indicator_layer_ setOpacity:0.1];
      }
    }
  }
#endif
}

}  // namespace ui
