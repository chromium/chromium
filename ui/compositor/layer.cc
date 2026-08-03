// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/mirror_layer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/interpolated_transform.h"

namespace ui {
namespace {

const Layer* GetRoot(const Layer* layer) {
  // Parent walk cannot be done on a layer that is being used as a mask. Get the
  // layer to which this layer is a mask of.
  if (layer->layer_mask_back_link())
    layer = layer->layer_mask_back_link();
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

#if DCHECK_IS_ON()
void CheckSnapped(float snapped_position) {
  // The acceptable error epsilon should be small enough to detect visible
  // artifacts as well as large enough to not cause false crashes when an
  // uncommon device scale factor is applied.
  const float kEplison = 0.003f;
  float diff = std::abs(snapped_position - std::round(snapped_position));
  DCHECK_LT(diff, kEplison);
}
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Layer::LayerMirror:

class Layer::LayerMirror : public LayerDelegate, LayerObserver {
 public:
  LayerMirror(Layer* source, Layer* dest)
      : source_(source), dest_(dest) {
    dest->AddObserver(this);
    dest->set_delegate(this);
  }

  LayerMirror(const LayerMirror&) = delete;
  LayerMirror& operator=(const LayerMirror&) = delete;

  ~LayerMirror() override {
    dest_->RemoveObserver(this);
    dest_->set_delegate(nullptr);
  }

  Layer* dest() { return dest_; }

  // LayerDelegate:
  void OnPaintLayer(const PaintContext& context) override {
    if (auto* delegate = source_->delegate())
      delegate->OnPaintLayer(context);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // LayerObserver:
  void LayerDestroyed(Layer* layer) override {
    DCHECK_EQ(dest_, layer);
    source_->OnMirrorDestroyed(this);
  }

 private:
  const raw_ptr<Layer> source_;
  const raw_ptr<Layer> dest_;
};

////////////////////////////////////////////////////////////////////////////////
// Layer::SubpixelPositionOffsetCache:

// Manages the subpixel offset data for a given set of parameters (device
// scale factor and DIP offset from parent layer).
class Layer::SubpixelPositionOffsetCache {
 public:
  SubpixelPositionOffsetCache() = default;
  SubpixelPositionOffsetCache(const SubpixelPositionOffsetCache&) = delete;
  SubpixelPositionOffsetCache& operator=(const SubpixelPositionOffsetCache&) =
      delete;
  ~SubpixelPositionOffsetCache() = default;

  gfx::Vector2dF GetSubpixelOffset(float device_scale_factor,
                                   const gfx::Point& origin,
                                   const gfx::Transform& tm) const {
    if (has_explicit_subpixel_offset_)
      return offset_;

    if (device_scale_factor <= 0)
      return gfx::Vector2dF();

    // Compute the effective offset (position + transform) from the parent.
    gfx::PointF offset_from_parent(origin);
    if (!tm.IsIdentity() && tm.Preserves2dAxisAlignment())
      offset_from_parent += tm.To2dTranslation();

    if (device_scale_factor == device_scale_factor_ &&
        offset_from_parent == offset_from_parent_) {
      return offset_;
    }

    // Compute subpixel offset for the given parameters.
    gfx::PointF scaled_offset_from_parent(offset_from_parent);
    scaled_offset_from_parent.Scale(device_scale_factor, device_scale_factor);
    gfx::PointF snapped_offset_from_parent(
        gfx::ToRoundedPoint(scaled_offset_from_parent));

    gfx::Vector2dF offset =
        snapped_offset_from_parent - scaled_offset_from_parent;
    offset.InvScale(device_scale_factor);

    // Store key and value information for the cache.
    offset_ = offset;
    device_scale_factor_ = device_scale_factor;
    offset_from_parent_ = offset_from_parent;

#if DCHECK_IS_ON()
    const gfx::PointF snapped_position = offset_from_parent_ + offset_;
    CheckSnapped(snapped_position.x() * device_scale_factor);
    CheckSnapped(snapped_position.y() * device_scale_factor);
#endif
    return offset_;
  }

  void SetExplicitSubpixelPositionOffset(const gfx::Vector2dF& offset) {
    has_explicit_subpixel_offset_ = true;
    offset_ = offset;
  }

  bool has_explicit_subpixel_offset() const {
    return has_explicit_subpixel_offset_;
  }

 private:
  // The subpixel offset value.
  mutable gfx::Vector2dF offset_;

  // The device scale factor for which the |offset_| was computed.
  mutable float device_scale_factor_ = 1.f;

  // The offset of the layer from its parent for which |offset_| was computed.
  mutable gfx::PointF offset_from_parent_;

  // True if the subpixel offset was computed and set by an external source.
  bool has_explicit_subpixel_offset_ = false;
};

////////////////////////////////////////////////////////////////////////////////
// Layer, public:

std::unique_ptr<Layer> Layer::Create(LayerType type) {
  switch (type) {
    case LAYER_NOT_DRAWN:
      return std::make_unique<LayerNotDrawn>();
    case LAYER_TEXTURED:
      return std::make_unique<LayerTextured>();
    case LAYER_SOLID_COLOR:
      return std::make_unique<LayerSolidColor>();
    case LAYER_NINE_PATCH:
      return std::make_unique<LayerNinePatch>();
    case LAYER_SURFACE:
      return std::make_unique<LayerSurface>();
  }
}

LayerTextured* Layer::AsTextured() {
  return As<LayerTextured>();
}

const LayerTextured* Layer::AsTextured() const {
  return As<LayerTextured>();
}

LayerSolidColor* Layer::AsSolidColor() {
  return As<LayerSolidColor>();
}

const LayerSolidColor* Layer::AsSolidColor() const {
  return As<LayerSolidColor>();
}

LayerNinePatch* Layer::AsNinePatch() {
  return As<LayerNinePatch>();
}

const LayerNinePatch* Layer::AsNinePatch() const {
  return As<LayerNinePatch>();
}

LayerSurface* Layer::AsSurface() {
  return As<LayerSurface>();
}

const LayerSurface* Layer::AsSurface() const {
  return As<LayerSurface>();
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(nullptr),
      parent_(nullptr),
      subpixel_position_offset_(
          std::make_unique<SubpixelPositionOffsetCache>()),
      visible_(true),
      fills_bounds_opaquely_(true),
      background_blur_sigma_(0.0f),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_blur_sigma_(0.0f),
      layer_sepia_(0.0f),
      layer_hue_rotation_(0.0f),
      layer_mask_(nullptr),
      layer_mask_back_link_(nullptr),
      zoom_(1),
      zoom_inset_(0),
      owner_(nullptr),
      cc_layer_(nullptr),
      device_scale_factor_(1.0f),
      cache_render_surface_requests_(0),
      backdrop_filter_quality_(1.0f),
      trilinear_filtering_request_(0) {
}

Layer::~Layer() {
  CHECK(!cc_layer_);
}

void Layer::Destroy() {
  observer_list_.Notify(&LayerObserver::LayerDestroyed, this);

  // Destroying the animator may cause observers to use the layer. Destroy the
  // animator first so that the layer is still around.
  SetAnimator(nullptr);
  if (compositor_)
    compositor_->SetRootLayer(nullptr);
  if (parent_)
    parent_->Remove(this);
  if (layer_mask_)
    SetMaskLayer(nullptr);
  if (layer_mask_back_link_)
    layer_mask_back_link_->SetMaskLayer(nullptr);
  for (ui::Layer* child : children_) {
    child->parent_ = nullptr;
  }

  cc_layer_->RemoveFromParent();
  cc_layer_ = nullptr;

  Reset();
}

std::unique_ptr<Layer> Layer::Clone() const {
  std::unique_ptr<Layer> clone = Layer::Create(type_);

  // Background filters.
  clone->SetBackgroundBlur(background_blur_sigma_);
  clone->SetBackgroundInverted(background_inverted_);
  clone->SetBackgroundZoom(zoom_, zoom_inset_);
  clone->SetBackdropFilterQuality(backdrop_filter_quality_);
  if (has_explicit_backdrop_filter_bounds_) {
    auto backdrop_filter_bounds = cc_layer_->backdrop_filter_bounds();
    if (backdrop_filter_bounds) {
      clone->SetBackdropFilterBounds(*backdrop_filter_bounds);
    }
  }

  // Filters.
  clone->SetLayerSaturation(layer_saturation_);
  clone->SetLayerBrightness(GetTargetBrightness());
  clone->SetLayerGrayscale(GetTargetGrayscale());
  clone->SetLayerSepia(layer_sepia_);
  clone->SetLayerHueRotation(layer_hue_rotation_);
  if (layer_custom_color_matrix_) {
    clone->SetLayerCustomColorMatrix(*layer_custom_color_matrix_);
  }
  clone->SetLayerInverted(layer_inverted_);
  clone->SetLayerBlur(layer_blur_sigma_);
  if (alpha_shape_)
    clone->SetAlphaShape(std::make_unique<ShapeRects>(*alpha_shape_));
  clone->SetLayerOffset(layer_offset_);

  // cc::Layer state.
  clone->SetTransform(GetTargetTransform());
  clone->SetBounds(bounds_);
  if (subpixel_position_offset_->has_explicit_subpixel_offset())
    clone->SetSubpixelPositionOffset(GetSubpixelOffset());
  clone->SetMasksToBounds(GetMasksToBounds());
  clone->SetOpacity(GetTargetOpacity());
  clone->SetVisible(GetTargetVisibility());
  clone->SetClipRect(GetTargetClipRect());
  clone->SetAcceptEvents(accept_events());
  clone->SetRoundedCornerRadius(GetTargetRoundedCornerRadius());
  clone->SetGradientMask(gradient_mask());
  clone->SetIsFastRoundedCorner(is_fast_rounded_corner());
  clone->SetName(name_);

  // TODO(crbug.com/522627357): Move to LayerSolidColor.
  if (type() != LAYER_SOLID_COLOR) {
    clone->SetFillsBoundsOpaquely(fills_bounds_opaquely_);
  }

  // the |damaged_region_| will be sent to cc later in SendDamagedRects().
  clone->damaged_region_ = damaged_region_;

  return clone;
}

std::unique_ptr<Layer> Layer::Mirror() {
  return Mirror(LayerMirrorSettings());
}

std::unique_ptr<Layer> Layer::Mirror(const LayerMirrorSettings& settings) {
  auto mirror = CreateMirror(settings);
  mirrors_.emplace_back(std::make_unique<LayerMirror>(this, mirror.get()));
  return mirror;
}

std::unique_ptr<Layer> Layer::CreateMirror(
    const LayerMirrorSettings& settings) {
  auto mirror = Clone();
  mirror->mirror_settings_ = settings;
  return mirror;
}

const Compositor* Layer::GetCompositor() const {
  return GetRoot(this)->compositor_;
}

float Layer::opacity() const {
  return cc_layer_->opacity();
}

void Layer::SetCompositor(Compositor* compositor,
                          scoped_refptr<cc::Layer> root_layer) {
  // This function must only be called to set the compositor on the root ui
  // layer.
  DCHECK(compositor);
  DCHECK(!compositor_);
  DCHECK(compositor->root_layer() == this);
  DCHECK(!parent_);

  compositor_ = compositor;
  OnDeviceScaleFactorChanged(compositor->device_scale_factor());

  root_layer->AddChild(cc_layer_.get());
  SetCompositorForAnimatorsInTree(compositor);
}

void Layer::ResetCompositor() {
  DCHECK(!parent_);
  if (compositor_) {
    ResetCompositorForAnimatorsInTree(compositor_);
    compositor_ = nullptr;
  }
}

void Layer::AddObserver(LayerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Layer::RemoveObserver(LayerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Layer::Add(Layer* child) {
  DCHECK(!child->compositor_);
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
  cc_layer_->AddChild(child->cc_layer_.get());
  child->OnDeviceScaleFactorChanged(device_scale_factor_);
  Compositor* compositor = GetCompositor();
  if (compositor && compositor->animations_are_enabled())
    child->SetCompositorForAnimatorsInTree(compositor);
}

void Layer::Remove(Layer* child) {
  base::WeakPtr<Layer> weak_this = weak_ptr_factory_.GetWeakPtr();
  base::WeakPtr<Layer> weak_child = child->weak_ptr_factory_.GetWeakPtr();

  // Current bounds are used to calculate offsets when layers are reparented.
  // Stop (and complete) an ongoing animation to update the bounds immediately.
  LayerAnimator* child_animator = child->animator_.get();
  if (child_animator)
    child_animator->StopAnimatingProperty(LayerAnimationElement::BOUNDS);

  // Do not proceed if |this| or |child| is released by an animation observer
  // of |child|'s bounds animation.
  if (!weak_this || !weak_child)
    return;

  Compositor* compositor = GetCompositor();
  if (compositor && compositor->animations_are_enabled())
    child->ResetCompositorForAnimatorsInTree(compositor);

  auto i = std::ranges::find(children_, child);
  CHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = nullptr;
  child->cc_layer_->RemoveFromParent();
}

void Layer::StackAtTop(Layer* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // Already in front.
  StackAbove(child, children_.back());
}

void Layer::StackAbove(Layer* child, Layer* other) {
  StackRelativeTo(child, other, true);
}

void Layer::StackAtBottom(Layer* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // Already on bottom.
  StackBelow(child, children_.front());
}

void Layer::StackBelow(Layer* child, Layer* other) {
  StackRelativeTo(child, other, false);
}

bool Layer::Contains(const Layer* other) const {
  for (const Layer* parent = other; parent; parent = parent->parent()) {
    if (parent == this)
      return true;
  }
  return false;
}

void Layer::SetAnimator(scoped_refptr<LayerAnimator> animator) {
  Compositor* compositor = GetCompositor();

  if (animator_) {
    if (compositor && compositor->animations_are_enabled() &&
        !layer_mask_back_link())
      animator_->DetachLayerAndTimeline(compositor);
    animator_->SetDelegate(nullptr);
  }

  animator_ = std::move(animator);

  if (animator_) {
    animator_->SetDelegate(this);
    if (compositor && compositor->animations_are_enabled() &&
        !layer_mask_back_link())
      animator_->AttachLayerAndTimeline(compositor);
  }
}

LayerAnimator* Layer::GetAnimator() {
  if (!animator_)
    SetAnimator(LayerAnimator::CreateDefaultAnimator());
  return animator_.get();
}

void Layer::SetSubtreeCaptureId(viz::SubtreeCaptureId subtree_id) {
  cc_layer_->SetSubtreeCaptureId(subtree_id);
}

viz::SubtreeCaptureId Layer::GetSubtreeCaptureId() const {
  return cc_layer_->subtree_capture_id();
}

void Layer::SetTransform(const gfx::Transform& transform) {
  GetAnimator()->SetTransform(transform);
}

gfx::Transform Layer::GetTargetTransform() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::TRANSFORM)) {
    return animator_->GetTargetTransform();
  }
  return transform();
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  GetAnimator()->SetBounds(bounds);
}

void Layer::SetSubpixelPositionOffset(const gfx::Vector2dF& offset) {
  subpixel_position_offset_->SetExplicitSubpixelPositionOffset(offset);
  RecomputePosition();
}

const gfx::Vector2dF Layer::GetSubpixelOffset() const {
  return subpixel_position_offset_->GetSubpixelOffset(
      device_scale_factor_, GetTargetBounds().origin(), GetTargetTransform());
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::BOUNDS)) {
    return animator_->GetTargetBounds();
  }
  return bounds_;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  cc_layer_->SetMasksToBounds(masks_to_bounds);
}

bool Layer::GetMasksToBounds() const {
  return cc_layer_->masks_to_bounds();
}

gfx::Rect Layer::GetTargetClipRect() const {
  if (animator_ &&
      animator_->IsAnimatingProperty(LayerAnimationElement::CLIP)) {
    return animator_->GetTargetClipRect();
  }
  return clip_rect();
}

void Layer::SetClipRect(const gfx::Rect& clip_rect) {
  GetAnimator()->SetClipRect(clip_rect);
}

void Layer::SetOpacity(float opacity) {
  GetAnimator()->SetOpacity(opacity);
}

float Layer::GetCombinedOpacity() const {
  float opacity = this->opacity();
  Layer* current = this->parent_;
  while (current) {
    opacity *= current->opacity();
    current = current->parent_;
  }
  return opacity;
}

void Layer::SetBackdropFilterBounds(const SkPath& bounds) {
  has_explicit_backdrop_filter_bounds_ = true;
  cc_layer_->SetBackdropFilterBounds(bounds);
}

void Layer::SetBackdropFilterBounds(const gfx::RRectF& bounds) {
  SetBackdropFilterBounds(SkPath::RRect(SkRRect(bounds)));
}

void Layer::ClearBackdropFilterBounds() {
  has_explicit_backdrop_filter_bounds_ = false;
  cc_layer_->ClearBackdropFilterBounds();
  RecomputeBackdropFilterBounds();
}

void Layer::SetBackgroundBlur(float blur_sigma) {
  background_blur_sigma_ = blur_sigma;

  SetLayerBackgroundFilters();
}

void Layer::SetBackgroundInverted(bool inverted) {
  if (background_inverted_ == inverted) {
    return;
  }
  background_inverted_ = inverted;

  SetLayerBackgroundFilters();
}

void Layer::SetBackgroundZoom(float zoom, int inset) {
  zoom_ = zoom;
  zoom_inset_ = inset;

  SetLayerBackgroundFilters();
}

void Layer::SetLayerBlur(float blur_sigma) {
  layer_blur_sigma_ = blur_sigma;

  SetLayerFilters();
}

void Layer::SetLayerSaturation(float saturation) {
  layer_saturation_ = saturation;
  SetLayerFilters();
}

void Layer::SetLayerBrightness(float brightness) {
  GetAnimator()->SetBrightness(brightness);
}

float Layer::GetTargetBrightness() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::BRIGHTNESS)) {
    return animator_->GetTargetBrightness();
  }
  return layer_brightness();
}

void Layer::SetLayerGrayscale(float grayscale) {
  GetAnimator()->SetGrayscale(grayscale);
}

float Layer::GetTargetGrayscale() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::GRAYSCALE)) {
    return animator_->GetTargetGrayscale();
  }
  return layer_grayscale();
}

void Layer::SetLayerSepia(float amount) {
  layer_sepia_ = amount;
  SetLayerFilters();
}

void Layer::SetLayerHueRotation(float amount) {
  layer_hue_rotation_ = amount;
  SetLayerFilters();
}

void Layer::SetLayerCustomColorMatrix(
    const cc::FilterOperation::Matrix& matrix) {
  layer_custom_color_matrix_ =
      std::make_unique<cc::FilterOperation::Matrix>(matrix);
  SetLayerFilters();
}

const cc::FilterOperation::Matrix* Layer::GetLayerCustomColorMatrix() const {
  return layer_custom_color_matrix_.get();
}

bool Layer::LayerHasCustomColorMatrix() const {
  return layer_custom_color_matrix_.get() != nullptr;
}

void Layer::ClearLayerCustomColorMatrix() {
  layer_custom_color_matrix_.reset();
  SetLayerFilters();
}

void Layer::SetLayerOffset(const gfx::Point& offset) {
  layer_offset_ = offset;
  SetLayerFilters();
}

void Layer::SetLayerInverted(bool inverted) {
  layer_inverted_ = inverted;
  SetLayerFilters();
}

void Layer::SetMaskLayer(Layer* layer_mask) {
  if (layer_mask_ == layer_mask)
    return;
  // The provided mask should not have a layer mask itself.
  DCHECK(!layer_mask ||
         (!layer_mask->layer_mask_layer() && layer_mask->children().empty()));
  DCHECK(!layer_mask_back_link_);
  DCHECK(!layer_mask || layer_mask->type() == LAYER_TEXTURED);
  // Masks must be backed by a PictureLayer.
  DCHECK(!layer_mask || layer_mask->AsTextured()->content_layer());
  // We need to de-reference the currently linked object so that no problem
  // arises if the mask layer gets deleted before this object.
  if (layer_mask_) {
    layer_mask_->layer_mask_back_link_ = nullptr;
  }
  layer_mask_ = layer_mask;
  cc_layer_->SetMaskLayer(layer_mask ? layer_mask->AsTextured()->content_layer()
                                     : nullptr);
  // We need to reference the linked object so that it can properly break the
  // link to us when it gets deleted.
  if (layer_mask) {
    // A `layer_mask` of this would lead to recursion.
    CHECK(layer_mask != this);

    // Clears out other reference to `layer_mask` if there is one.
    if (layer_mask->layer_mask_back_link_) {
      layer_mask->layer_mask_back_link_->SetMaskLayer(nullptr);
    }

    layer_mask->layer_mask_back_link_ = this;
    layer_mask->OnDeviceScaleFactorChanged(device_scale_factor_);
  }
}

void Layer::SetAlphaShape(std::unique_ptr<ShapeRects> shape) {
  // Do some brief checks to avoid recomputing occlusion and layer filters.
  // See crbug.com/1493406.
  bool changed = true;
  if (alpha_shape_ == nullptr && shape == nullptr) {
    changed = false;
  }
  if (alpha_shape_ != nullptr && shape != nullptr) {
    // This doesn't catch the case where the `ShapeRects` are different but the
    // same if sorted, but we just want to prevent most unnecessary updates.
    changed = *alpha_shape_ != *shape;
  }
  alpha_shape_ = std::move(shape);

  if (!changed) {
    return;
  }

  SetLayerFilters();

  if (delegate_)
    delegate_->OnLayerAlphaShapeChanged();
}

void Layer::SetLayerFilters() {
  cc::FilterOperations filters;
  if (layer_custom_color_matrix_) {
    filters.Append(cc::FilterOperation::CreateColorMatrixFilter(
        *layer_custom_color_matrix_));
  }
  if (layer_hue_rotation_) {
    filters.Append(
        cc::FilterOperation::CreateHueRotateFilter(layer_hue_rotation_));
  }
  if (layer_saturation_) {
    filters.Append(cc::FilterOperation::CreateSaturateFilter(
        layer_saturation_));
  }
  if (layer_grayscale_) {
    filters.Append(cc::FilterOperation::CreateGrayscaleFilter(
        layer_grayscale_));
  }
  if (layer_inverted_)
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  if (layer_sepia_)
    filters.Append(cc::FilterOperation::CreateSepiaFilter(layer_sepia_));
  if (layer_blur_sigma_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(layer_blur_sigma_,
                                                         SkTileMode::kClamp));
  }
  // Brightness goes last, because the resulting colors neeed clamping, which
  // cause further color matrix filters to be applied separately. In this order,
  // they all can be combined in a single pass.
  if (layer_brightness_) {
    filters.Append(cc::FilterOperation::CreateSaturatingBrightnessFilter(
        layer_brightness_));
  }
  if (alpha_shape_) {
    filters.Append(
        cc::FilterOperation::CreateAlphaThresholdFilter(*alpha_shape_));
  }
  // An Offset as the last filter operation can almost always be converted to
  // a translation transform for free within Skia.
  if (!layer_offset_.IsOrigin()) {
    filters.Append(cc::FilterOperation::CreateOffsetFilter(layer_offset_));
  }

  cc_layer_->SetFilters(filters);
}

void Layer::SetLayerBackgroundFilters() {
  cc::FilterOperations filters;

  if (background_inverted_) {
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.0f));
  }

  if (background_blur_sigma_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(background_blur_sigma_,
                                                         SkTileMode::kClamp));
  }

  // The background zoom is applied after the background offset to support
  // positioning of the background *before* magnifying it. Offsetting after
  // magnifying is almost equivalent except it can lead to surprising clipping
  // at the layer bounds.
  if (zoom_ != 1) {
    filters.Append(cc::FilterOperation::CreateZoomFilter(zoom_, zoom_inset_));
  }

  cc_layer_->SetBackdropFilters(filters);
  RecomputeBackdropFilterBounds();
}

void Layer::RecomputeBackdropFilterBounds() {
  // Don't override explicitly set bounds.
  if (has_explicit_backdrop_filter_bounds_) {
    return;
  }

  // Only set bounds when backdrop filters are active.
  if (!background_blur_sigma_ && zoom_ == 1 && !background_inverted_) {
    cc_layer_->ClearBackdropFilterBounds();
    return;
  }

  const gfx::Size& size = bounds_.size();
  if (size.IsEmpty()) {
    cc_layer_->ClearBackdropFilterBounds();
    return;
  }

  // Set backdrop filter bounds to the layer's rectangular bounds in DIP.
  // Note: rounded corners are intentionally not included here. The layer's
  // SetRoundedCornerRadius() already clips the final rendered output. Adding
  // rounded corners to backdrop_filter_bounds would also clip the blur's
  // input sampling, causing edge artifacts and visual differences.
  cc_layer_->SetBackdropFilterBounds(
      gfx::RRectF(gfx::RectF(gfx::SizeF(size)), 0));
}

float Layer::GetTargetOpacity() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::OPACITY))
    return animator_->GetTargetOpacity();
  return opacity();
}

void Layer::SetVisible(bool visible) {
  GetAnimator()->SetVisibility(visible);
}

void Layer::SetAcceptEvents(bool accept_events) {
  if (accept_events_ == accept_events)
    return;
  accept_events_ = accept_events;
  cc_layer_->SetHitTestable(IsHitTestableForCC());
}

bool Layer::GetTargetVisibility() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::VISIBILITY))
    return animator_->GetTargetVisibility();
  return visible_;
}

bool Layer::IsVisible() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == nullptr;
}

gfx::RoundedCornersF Layer::GetTargetRoundedCornerRadius() const {
  if (animator_ &&
      animator_->IsAnimatingProperty(LayerAnimationElement::ROUNDED_CORNERS)) {
    return animator_->GetTargetRoundedCorners();
  }

  return rounded_corner_radii();
}

void Layer::SetRoundedCornerRadius(const gfx::RoundedCornersF& corner_radii) {
  GetAnimator()->SetRoundedCorners(corner_radii);
}

void Layer::SetGradientMask(const gfx::LinearGradient& gradient_mask) {
  GetAnimator()->SetGradientMask(gradient_mask);
}

void Layer::SetIsFastRoundedCorner(bool enable) {
  cc_layer_->SetIsFastRoundedCorner(enable);
  ScheduleDraw();

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetIsFastRoundedCorner(enable);
}

bool Layer::GetTargetTransformRelativeTo(const Layer* ancestor,
                                         gfx::Transform* transform) const {
  return GetTransformRelativeToImpl(ancestor, /*is_target_transform=*/true,
                                    transform);
}

bool Layer::GetTransformRelativeTo(const Layer* ancestor,
                                   gfx::Transform* transform) const {
  return GetTransformRelativeToImpl(ancestor, /*is_target_transform=*/false,
                                    transform);
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                bool use_target_transform,
                                gfx::PointF* point) {
  if (source == target)
    return;

  const Layer* source_root_layer = GetRoot(source);
  const Layer* target_root_layer = GetRoot(target);
  // TODO(b/319939913): Remove this log when the issue is fixed.
  if (source_root_layer != target_root_layer) {
    auto chain_name = [](const Layer* layer) {
      std::ostringstream out;
      out << "[";
      out << layer->name();
      while (layer->parent()) {
        layer = layer->parent();
        out << "]-[" << layer->name();
      }
      out << "]";
      return out.str();
    };
    LOG(ERROR) << "Source has different root than tareget: source chain="
               << chain_name(source) << ", target chain=" << chain_name(target);
  }
  CHECK_EQ(source_root_layer, target_root_layer);

  if (source != source_root_layer) {
    source->ConvertPointForAncestor(source_root_layer, use_target_transform,
                                    point);
  }
  if (target != source_root_layer) {
    target->ConvertPointFromAncestor(source_root_layer, use_target_transform,
                                     point);
  }
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  CHECK_NE(type_, LayerType::LAYER_SOLID_COLOR);
  SetFillsBoundsOpaquelyWithReason(fills_bounds_opaquely,
                                   PropertyChangeReason::NOT_FROM_ANIMATION);
}

void Layer::SetName(const std::string& name) {
  name_ = name;
  cc_layer_->SetDebugName(name);
}

bool Layer::FinishAnimationsBeforeSwitchToLayer() {
  // Finish animations being handled by the `cc_layer_`. Note that doing so
  // could cause animation observers to be notified and those observers could
  // destroy `this` Layer.
  if (animator_) {
    base::WeakPtr<Layer> weak_this = weak_ptr_factory_.GetWeakPtr();

    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
    if (!weak_this)
      return false;

    animator_->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    if (!weak_this)
      return false;
  }
  return true;
}

void Layer::SwitchToLayer(scoped_refptr<cc::Layer> new_layer) {
  if (animator_) {
    // Call `FinishAnimationsBeforeSwitchToLayer` before switching the layer.
    CHECK(!animator_->IsAnimatingProperty(LayerAnimationElement::TRANSFORM) &&
          !animator_->IsAnimatingProperty(LayerAnimationElement::OPACITY));
    animator_->SwitchToLayer(new_layer);
  }

  cc_layer_->RemoveAllChildren();
  if (cc_layer_->parent()) {
    cc_layer_->mutable_parent()->ReplaceChild(cc_layer_, new_layer);
  }
  cc_layer_->ClearDebugInfo();

  new_layer->SetOpacity(cc_layer_->opacity());
  new_layer->SetTransform(cc_layer_->transform());
  new_layer->SetPosition(cc_layer_->position());
  new_layer->SetBackgroundColor(cc_layer_->background_color());
  new_layer->SetSafeOpaqueBackgroundColor(
      cc_layer_->SafeOpaqueBackgroundColor());
  new_layer->SetSubtreeCaptureId(cc_layer_->subtree_capture_id());
  new_layer->SetCacheRenderSurface(cc_layer_->cache_render_surface());
  new_layer->SetTrilinearFiltering(cc_layer_->trilinear_filtering());
  new_layer->SetRoundedCorner(cc_layer_->corner_radii());
  new_layer->SetIsFastRoundedCorner(cc_layer_->is_fast_rounded_corner());
  new_layer->SetMasksToBounds(cc_layer_->masks_to_bounds());
  new_layer->SetGradientMask(cc_layer_->gradient_mask());

  cc_layer_ = new_layer.get();

  Reset();

  for (ui::Layer* child : children_) {
    DCHECK(child->cc_layer_);
    cc_layer_->AddChild(child->cc_layer_.get());
  }
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(fills_bounds_opaquely_);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetHitTestable(IsHitTestableForCC());
  cc_layer_->SetHideLayerAndSubtree(!visible_);
  cc_layer_->SetBackdropFilterQuality(backdrop_filter_quality_);
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));
  cc_layer_->SetDebugName(name_);

  SetLayerFilters();
  SetLayerBackgroundFilters();
}

bool Layer::SwitchCCLayerForTest() {
  return false;
}

void Layer::SetBackdropFilterQuality(const float quality) {
  backdrop_filter_quality_ = quality / GetDeviceScaleFactor();
  cc_layer_->SetBackdropFilterQuality(backdrop_filter_quality_);
}

////////////////////////////////////////////////////////////////////////////////
// Layer, private:

// Note: The code that sets this flag would be responsible to unset it on that
// Layer. We do not want to clone this flag to a cloned layer by accident,
// which could be a supprise. But we want to preserve it after switching to a
// new cc::Layer. There could be a whole subtree and the root changed, but does
// not mean we want to treat the cache all different.
void Layer::AddCacheRenderSurfaceRequest() {
  ++cache_render_surface_requests_;
  TRACE_COUNTER_ID1("ui", "CacheRenderSurfaceRequests", this,
                    cache_render_surface_requests_);
  if (cache_render_surface_requests_ == 1) {
    cc_layer_->SetCacheRenderSurface(true);
  }
}

void Layer::RemoveCacheRenderSurfaceRequest() {
  DCHECK_GT(cache_render_surface_requests_, 0u);

  --cache_render_surface_requests_;
  TRACE_COUNTER_ID1("ui", "CacheRenderSurfaceRequests", this,
                    cache_render_surface_requests_);
  if (cache_render_surface_requests_ == 0) {
    cc_layer_->SetCacheRenderSurface(false);
  }
}

// Note: The code that sets this flag would be responsible to unset it on that
// Layer. We do not want to clone this flag to a cloned layer by accident,
// which could be a supprise. But we want to preserve it after switching to a
// new cc::Layer. There could be a whole subtree and the root changed, but does
// not mean we want to treat the trilinear filtering all different.
void Layer::AddTrilinearFilteringRequest() {
  ++trilinear_filtering_request_;
  TRACE_COUNTER_ID1("ui", "TrilinearFilteringRequests", this,
                    trilinear_filtering_request_);
  if (trilinear_filtering_request_ == 1)
    cc_layer_->SetTrilinearFiltering(true);
}

void Layer::RemoveTrilinearFilteringRequest() {
  DCHECK_GT(trilinear_filtering_request_, 0u);

  --trilinear_filtering_request_;
  TRACE_COUNTER_ID1("ui", "TrilinearFilteringRequests", this,
                    trilinear_filtering_request_);
  if (trilinear_filtering_request_ == 0)
    cc_layer_->SetTrilinearFiltering(false);
}

base::WeakPtr<Layer> Layer::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool Layer::ContainsMirrorForTest(Layer* mirror) const {
  return std::ranges::contains(mirrors_, mirror, &LayerMirror::dest);
}

bool Layer::HasExternalContent() const {
  return false;
}

bool Layer::HasTransferableResource() const {
  return false;
}

bool Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  if (!ShouldSchedulePaint()) {
    return false;
  }

  damaged_region_.Union(invalid_rect);
  if (layer_mask_)
    layer_mask_->damaged_region_.Union(invalid_rect);

  OnPaintScheduled();
  return true;
}

void Layer::ScheduleDraw() {
  // Do not schedule draw if this layer does not contribute the content.
  if (type_ == LAYER_NOT_DRAWN && children_.size() == 0) {
    return;
  }
  Compositor* compositor = GetCompositor();
  if (compositor) {
    compositor->ScheduleDraw();
  }
}

void Layer::SendDamagedRects() {
  if (layer_mask_)
    layer_mask_->SendDamagedRects();
  if (delegate_)
    delegate_->UpdateVisualState();

  if (!ShouldCommitDamage()) {
    return;
  }

  CommitDamage(damaged_region_);
  damaged_region_.Clear();
}

void Layer::CommitDamage(const cc::Region& damage) {
  for (gfx::Rect damaged_rect : damage) {
    cc_layer_->SetNeedsDisplayRect(damaged_rect);
  }
}

bool Layer::ShouldCommitDamage() const {
  return !damaged_region_.IsEmpty();
}

void Layer::CompleteAllAnimations() {
  typedef std::vector<scoped_refptr<LayerAnimator>> LayerAnimatorVector;
  LayerAnimatorVector animators;
  CollectAnimators(&animators);
  for (LayerAnimatorVector::const_iterator it = animators.begin();
       it != animators.end();
       ++it) {
    (*it)->StopAnimating();
  }
}

void Layer::StackChildrenAtBottom(
    const std::vector<Layer*>& new_leading_children) {
  std::vector<raw_ptr<Layer, VectorExperimental>> new_children_order;
  new_children_order.reserve(children_.size());

  cc::LayerList new_cc_children_order;
  new_cc_children_order.reserve(cc_layer_->children().size());

  for (Layer* leading_child : new_leading_children) {
    DCHECK_EQ(leading_child->cc_layer_->parent(), cc_layer_);
    DCHECK_EQ(leading_child->parent(), this);
    new_children_order.emplace_back(leading_child);
    new_cc_children_order.emplace_back(
        scoped_refptr<cc::Layer>(leading_child->cc_layer_.get()));
  }

  base::flat_set<raw_ptr<Layer, VectorExperimental>> reordered_children(
      new_children_order);

  const cc::LayerList& old_cc_children_order = cc_layer_->children();

  for (size_t i = 0; i < children_.size(); ++i) {
    if (reordered_children.count(children_.at(i)) > 0)
      continue;
    new_children_order.emplace_back(children_.at(i));
    new_cc_children_order.emplace_back(old_cc_children_order.at(i));
  }

  children_ = std::move(new_children_order);
  cc_layer_->ReorderChildren(&new_cc_children_order);
}

void Layer::SuppressPaint() {
  if (!delegate_)
    return;
  delegate_ = nullptr;
  for (ui::Layer* child : children_) {
    child->SuppressPaint();
  }
}

void Layer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  base::WeakPtr<Layer> weak_this = weak_ptr_factory_.GetWeakPtr();

  // Some animation observers may mutate the tree (e.g. destroy the layer,
  // change ancestor/sibling z-order etc) when the animation ends. This break
  // the tree traversal and could lead to a crash. Collect all descendants (and
  // their mask layers) in a flattened WeakPtr list at the root level then stop
  // animations to let potential tree mutations happen before traversing the
  // tree. See https://crbug.com/1037852.
  const bool is_root_layer = !parent();
  if (is_root_layer) {
    std::vector<base::WeakPtr<Layer>> flattened;
    GetFlattenedWeakList(&flattened);
    for (auto& weak_layer : flattened) {
      // Skip if layer is gone or not animating.
      if (!weak_layer || !weak_layer->animator_)
        continue;

      weak_layer->animator_->StopAnimatingProperty(
          LayerAnimationElement::TRANSFORM);

      // Do not proceed if the root layer was destroyed due to an animation
      // observer.
      if (!weak_this)
        return;
    }
  }

  const float old_device_scale_factor = device_scale_factor_;
  device_scale_factor_ = device_scale_factor;

  HandleDeviceScaleFactorChange();
  if (delegate_) {
    delegate_->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                          device_scale_factor);
  }

  // We may add or remove children during child->OnDeviceScaleFactorChanged().
  std::vector<base::WeakPtr<Layer>> weak_children(children_.size());
  for (ui::Layer* child : children_) {
    weak_children.push_back(child->weak_ptr_factory_.GetWeakPtr());
  }
  for (auto& child : weak_children) {
    if (!child) {
      continue;
    }
    child->OnDeviceScaleFactorChanged(device_scale_factor);

    // A child layer may have triggered a delegate or an observer to delete
    // |this| layer. In which case return early to avoid crash.
    if (!weak_this)
      return;
  }
  if (layer_mask_)
    layer_mask_->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Layer::HandleDeviceScaleFactorChange() {
  RecomputeDrawsContentAndUVRect();
  RecomputePosition();
  SchedulePaint(gfx::Rect(bounds_.size()));
}

void Layer::SetDidScrollCallback(
    base::RepeatingCallback<void(const gfx::PointF&, const cc::ElementId&)>
        callback) {
  cc_layer_->SetDidScrollCallback(std::move(callback));
}

void Layer::SetScrollable(const gfx::Size& container_bounds) {
  cc_layer_->SetScrollable(container_bounds);
}

void Layer::SetMainSideScrollingEnabled(bool enabled) {
  main_side_scrolling_enabled_ = enabled;
}

gfx::PointF Layer::CurrentScrollOffset() const {
  const Compositor* compositor = GetCompositor();
  gfx::PointF offset;
  if (!main_side_scrolling_enabled() && compositor &&
      compositor->GetScrollOffsetForLayer(cc_layer_->element_id(), &offset)) {
    return offset;
  }
  return cc_layer_->scroll_offset();
}

void Layer::SetScrollOffset(const gfx::PointF& offset) {
  Compositor* compositor = GetCompositor();
  bool scrolled_on_impl_side =
      !main_side_scrolling_enabled() && compositor &&
      compositor->ScrollLayerTo(cc_layer_->element_id(), offset);

  if (!scrolled_on_impl_side) {
    cc_layer_->SetScrollOffset(offset);
  }

  // TODO(crbug.com/40772386): If this layer was also resized since the last
  // commit synchronizing |cc_layer_| with the cc::LayerImpl backing
  // |compositor|, the scroll might not be completed.
}

void Layer::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  if (!request->has_result_task_runner()) {
    CHECK(GetCompositor())
        << "A copy request must either have a task runner, or be added to the "
           "layer that has already been added to compositor.";
    request->set_result_task_runner(GetCompositor()->task_runner());
  }

  cc_layer_->RequestCopyOfOutput(std::move(request));
}

void Layer::CollectAnimators(
    std::vector<scoped_refptr<LayerAnimator>>* animators) {
  if (animator_ && animator_->is_animating())
    animators->push_back(animator_);
  for (ui::Layer* child : children_) {
    child->CollectAnimators(animators);
  }
}

void Layer::StackRelativeTo(Layer* child, Layer* other, bool above) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());

  const size_t child_i =
      std::ranges::find(children_, child) - children_.begin();
  const size_t other_i =
      std::ranges::find(children_, other) - children_.begin();
  DCHECK_LT(child_i, children_.size()) << " child not in vector";
  DCHECK_LT(other_i, children_.size()) << " other not in vector";
  if ((above && child_i == other_i + 1) || (!above && child_i + 1 == other_i))
    return;

  const size_t dest_i =
      above ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);

  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  cc_layer_->InsertChild(child->cc_layer_.get(), dest_i);
}

bool Layer::ConvertPointForAncestor(const Layer* ancestor,
                                    bool use_target_transform,
                                    gfx::PointF* point) const {
  gfx::Transform transform;
  bool result = use_target_transform
                    ? GetTargetTransformRelativeTo(ancestor, &transform)
                    : GetTransformRelativeTo(ancestor, &transform);
  *point = transform.MapPoint(*point);
  return result;
}

bool Layer::ConvertPointFromAncestor(const Layer* ancestor,
                                     bool use_target_transform,
                                     gfx::PointF* point) const {
  gfx::Transform transform;
  if (!(use_target_transform
            ? GetTargetTransformRelativeTo(ancestor, &transform)
            : GetTransformRelativeTo(ancestor, &transform))) {
    return false;
  }
  const std::optional<gfx::PointF> transformed_point =
      transform.InverseMapPoint(*point);
  if (!transformed_point.has_value())
    return false;
  *point = transformed_point.value();
  return true;
}

void Layer::SetBoundsFromAnimation(const gfx::Rect& bounds,
                                   PropertyChangeReason reason) {
  if (bounds == bounds_)
    return;

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;

  RecomputeDrawsContentAndUVRect();
  if (old_bounds.origin() != bounds_.origin())
    RecomputePosition();
  if (old_bounds.size() != bounds_.size()) {
    RecomputeBackdropFilterBounds();
  }

  auto ptr = weak_ptr_factory_.GetWeakPtr();

  if (delegate_)
    delegate_->OnLayerBoundsChanged(old_bounds, reason);

  // The layer may be deleted in the observer.
  if (!ptr)
    return;

  if (bounds.size() == old_bounds.size()) {
    // Don't schedule a draw if we're invisible. We'll schedule one
    // automatically when we get visible.
    if (IsVisible()) {
      ScheduleDraw();
    }
  } else {
    // Always schedule a paint, even if we're invisible.
    SchedulePaint(gfx::Rect(bounds.size()));
  }

  for (const auto& mirror : mirrors_) {
    Layer* mirror_dest = mirror->dest();
    if (mirror_dest->mirror_settings_.sync_bounds) {
      mirror_dest->SetBounds(bounds);
    }
  }

  for (Layer* reflecting_layer : subtree_reflecting_layers_) {
    reflecting_layer->MatchLayerSize(this);
  }
}

void Layer::SetTransformFromAnimation(const gfx::Transform& new_transform,
                                      PropertyChangeReason reason) {
  const gfx::Transform old_transform = transform();
  if (old_transform == new_transform)
    return;
  cc_layer_->SetTransform(new_transform);

  // Skip recomputing position if the subpixel offset does not need updating
  // which is the case if an explicit offset is set.
  if (!subpixel_position_offset_->has_explicit_subpixel_offset())
    RecomputePosition();
  if (delegate_)
    delegate_->OnLayerTransformed(old_transform, reason);
}

void Layer::SetOpacityFromAnimation(float opacity,
                                    PropertyChangeReason reason) {
  cc_layer_->SetOpacity(opacity);
  if (delegate_)
    delegate_->OnLayerOpacityChanged(reason);
  ScheduleDraw();
}

void Layer::SetVisibilityFromAnimation(bool visible,
                                       PropertyChangeReason reason) {
  // Sync changes with the mirror layers only if they want so.
  for (const auto& mirror : mirrors_) {
    Layer* mirror_dest = mirror->dest();
    if (mirror_dest->mirror_settings_.sync_visibility) {
      mirror_dest->SetVisible(visible);
    }
  }

  if (visible_ == visible)
    return;

  visible_ = visible;
  cc_layer_->SetHideLayerAndSubtree(!visible_);
  cc_layer_->SetHitTestable(IsHitTestableForCC());
}

void Layer::SetBrightnessFromAnimation(float brightness,
                                       PropertyChangeReason reason) {
  layer_brightness_ = brightness;
  SetLayerFilters();
}

void Layer::SetGrayscaleFromAnimation(float grayscale,
                                      PropertyChangeReason reason) {
  layer_grayscale_ = grayscale;
  SetLayerFilters();
}

void Layer::SetColorFromAnimation(SkColor4f color,
                                  PropertyChangeReason reason) {
  NOTREACHED();
}

void Layer::SetClipRectFromAnimation(const gfx::Rect& clip_rect,
                                     PropertyChangeReason reason) {
  const gfx::Rect old_rect = cc_layer_->clip_rect();
  if (old_rect == clip_rect)
    return;
  cc_layer_->SetClipRect(clip_rect);

  if (delegate_)
    delegate_->OnLayerClipRectChanged(old_rect, reason);
}

void Layer::SetRoundedCornersFromAnimation(
    const gfx::RoundedCornersF& rounded_corners,
    PropertyChangeReason reason) {
  cc_layer_->SetRoundedCorner(rounded_corners);

  for (const auto& mirror : mirrors_) {
    Layer* mirror_dest = mirror->dest();
    if (mirror_dest->mirror_settings_.sync_rounded_corners) {
      mirror_dest->SetRoundedCornersFromAnimation(rounded_corners, reason);
    }
  }
}

void Layer::SetGradientMaskFromAnimation(
    const gfx::LinearGradient& gradient_mask,
    PropertyChangeReason reason) {
  cc_layer_->SetGradientMask(gradient_mask);

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetGradientMaskFromAnimation(gradient_mask, reason);
}

void Layer::ScheduleDrawForAnimation() {
  ScheduleDraw();
}

const gfx::Rect& Layer::GetBoundsForAnimation() const {
  return bounds();
}

gfx::Transform Layer::GetTransformForAnimation() const {
  return transform();
}

float Layer::GetOpacityForAnimation() const {
  return opacity();
}

bool Layer::GetVisibilityForAnimation() const {
  return visible();
}

float Layer::GetBrightnessForAnimation() const {
  return layer_brightness();
}

float Layer::GetGrayscaleForAnimation() const {
  return layer_grayscale();
}

SkColor4f Layer::GetColorForAnimation() const {
  return SkColors::kBlack;
}

gfx::Rect Layer::GetClipRectForAnimation() const {
  if (clip_rect().IsEmpty())
    return gfx::Rect(size());
  return clip_rect();
}

gfx::RoundedCornersF Layer::GetRoundedCornersForAnimation() const {
  return rounded_corner_radii();
}

const gfx::LinearGradient& Layer::GetGradientMaskForAnimation() const {
  return gradient_mask();
}

float Layer::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

LayerAnimatorCollection* Layer::GetLayerAnimatorCollection() {
  Compositor* compositor = GetCompositor();
  return compositor ? compositor->layer_animator_collection() : nullptr;
}

float Layer::GetRefreshRate() const {
  const Compositor* compositor = GetCompositor();
  return compositor ? compositor->refresh_rate() : 60.0;
}

Layer* Layer::GetLayer() {
  return this;
}

cc::Layer* Layer::GetCcLayer() const {
  return cc_layer_;
}

LayerThreadedAnimationDelegate* Layer::GetThreadedAnimationDelegate() {
  DCHECK(animator_);
  return animator_.get();
}

void Layer::InitializeCcLayer() {
  CHECK(cc_layer_);
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetIsDrawable(true);
  cc_layer_->SetHitTestable(IsHitTestableForCC());
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));
  cc_layer_->SetBackgroundColor(SkColors::kTransparent);
  cc_layer_->SetSafeOpaqueBackgroundColor(SkColors::kWhite);
  cc_layer_->SetContentsOpaque(true);

  RecomputePosition();
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(cc_layer_);
  gfx::Size size(bounds_.size());
  cc_layer_->SetBounds(size);
}

void Layer::RecomputePosition() {
  cc_layer_->SetPosition(gfx::PointF(bounds_.origin()) + GetSubpixelOffset());
}

void Layer::SetCompositorForAnimatorsInTree(Compositor* compositor) {
  DCHECK(compositor);
  LayerAnimatorCollection* collection = compositor->layer_animator_collection();

  if (animator_) {
    if (animator_->is_animating())
      animator_->AddToCollection(collection);
    animator_->AttachLayerAndTimeline(compositor);
  }

  for (ui::Layer* child : children_) {
    child->SetCompositorForAnimatorsInTree(compositor);
  }
}

void Layer::ResetCompositorForAnimatorsInTree(Compositor* compositor) {
  DCHECK(compositor);
  LayerAnimatorCollection* collection = compositor->layer_animator_collection();

  if (animator_) {
    animator_->DetachLayerAndTimeline(compositor);
    animator_->RemoveFromCollection(collection);
  }

  for (ui::Layer* child : children_) {
    child->ResetCompositorForAnimatorsInTree(compositor);
  }
}

void Layer::OnMirrorDestroyed(LayerMirror* mirror) {
  const auto it =
      std::ranges::find(mirrors_, mirror, &std::unique_ptr<LayerMirror>::get);

  CHECK(it != mirrors_.end());
  mirrors_.erase(it);
}

void Layer::MatchLayerSize(const Layer* layer) {
  gfx::Rect new_bounds = bounds_;
  gfx::Size new_size = layer->bounds().size();
  new_bounds.set_size(new_size);
  SetBounds(new_bounds);
}

void Layer::GetFlattenedWeakList(
    std::vector<base::WeakPtr<Layer>>* flattened_list) {
  flattened_list->emplace_back(weak_ptr_factory_.GetWeakPtr());
  if (layer_mask_)
    flattened_list->emplace_back(layer_mask_->weak_ptr_factory_.GetWeakPtr());

  for (ui::Layer* child : children_) {
    child->GetFlattenedWeakList(flattened_list);
  }
}

void Layer::SetFillsBoundsOpaquelyWithReason(bool fills_bounds_opaquely,
                                             PropertyChangeReason reason) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  cc_layer_->SetContentsOpaque(fills_bounds_opaquely);

  if (delegate_)
    delegate_->OnLayerFillsBoundsOpaquelyChanged(reason);
}

bool Layer::GetTransformRelativeToImpl(const Layer* ancestor,
                                       bool is_target_transform,
                                       gfx::Transform* transform) const {
  const Layer* p = this;
  for (; p && p != ancestor; p = p->parent()) {
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->bounds().x()),
                          static_cast<float>(p->bounds().y()));
    const gfx::Transform& layer_transform =
        is_target_transform ? p->GetTargetTransform() : p->transform();
    if (!layer_transform.IsIdentity())
      transform->PostConcat(layer_transform);
    transform->PostConcat(translation);
  }
  return p == ancestor;
}

////////////////////////////////////////////////////////////////////////////////
// LayerNotDrawn, public:

LayerNotDrawn::LayerNotDrawn() : Layer(LAYER_NOT_DRAWN) {
  content_layer_ = cc::PictureLayer::Create(this);
  cc_layer_ = content_layer_.get();
  InitializeCcLayer();
  cc_layer_->SetIsDrawable(false);
}

LayerNotDrawn::~LayerNotDrawn() {
  Destroy();
}

bool LayerNotDrawn::ShouldSchedulePaint() const {
  // LayerNotDrawn does not draw any content, so it never needs to paint.
  return false;
}

bool LayerNotDrawn::SwitchCCLayerForTest() {
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return false;
  }

  scoped_refptr<cc::PictureLayer> new_layer = cc::PictureLayer::Create(this);
  SwitchToLayer(new_layer);

  content_layer_ = std::move(new_layer);
  return true;
}

scoped_refptr<cc::DisplayItemList> LayerNotDrawn::PaintContentsToDisplayList() {
  return base::MakeRefCounted<cc::DisplayItemList>();
}

bool LayerNotDrawn::FillsBoundsCompletely() const {
  return false;
}

void LayerNotDrawn::Reset() {
  content_layer_->ClearClient();
  content_layer_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LayerWithExternalTexture, public:

LayerWithExternalTexture::~LayerWithExternalTexture() = default;

void LayerWithExternalTexture::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK(!resource.is_empty());
  DCHECK(release_callback);
  DCHECK(!resource.GetIsSoftware());
  if (!texture_layer_.get()) {
    // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
    // destroyed.
    if (!FinishAnimationsBeforeSwitchToLayer()) {
      return;
    }
    // Incoming resource is assumed to have top-left origin which corresponds to
    // TextureLayer flipped being false.
    scoped_refptr<cc::TextureLayer> new_layer = cc::TextureLayer::Create(this);
    SwitchToLayer(new_layer);

    texture_layer_ = new_layer;
    // Reset the frame_size_in_dip_ so that SetTextureSize() will not early out,
    // the frame_size_in_dip_ was for a previous (different) |texture_layer_|.
    frame_size_in_dip_ = gfx::Size();
  }

  if (transfer_release_callback_) {
    std::move(transfer_release_callback_).Run(gpu::SyncToken(), false);
  }

  transfer_release_callback_ = std::move(release_callback);
  transfer_resource_ = resource;
  SetTextureSize(texture_size_in_dip);

  for (const auto& mirror : mirrors_) {
    // The release callbacks should be empty as only the source layer
    // should be able to release the texture resource.
    static_cast<LayerWithExternalTexture*>(mirror->dest())
        ->SetTransferableResource(
            transfer_resource_,
            base::BindOnce(
                [](const gpu::SyncToken& sync_token, bool is_lost) {}),
            frame_size_in_dip_);
  }
}

void LayerWithExternalTexture::SetTextureSize(gfx::Size texture_size_in_dip) {
  if (frame_size_in_dip_ == texture_size_in_dip) {
    return;
  }

  frame_size_in_dip_ = texture_size_in_dip;
  RecomputeDrawsContentAndUVRect();
  texture_layer_->SetNeedsDisplay();
}

std::unique_ptr<Layer> LayerWithExternalTexture::CreateMirror(
    const LayerMirrorSettings& settings) {
  auto mirror = Layer::CreateMirror(settings);

  if (HasTransferableResource()) {
    // Send an empty release callback because we don't want the resource to be
    // freed up until the original layer releases it.
    static_cast<LayerWithExternalTexture*>(mirror.get())
        ->SetTransferableResource(
            transfer_resource(),
            base::BindOnce(
                [](const gpu::SyncToken& sync_token, bool is_lost) {}),
            frame_size_in_dip_);
  }

  return mirror;
}

bool LayerWithExternalTexture::HasExternalContent() const {
  return texture_layer_.get();
}

bool LayerWithExternalTexture::HasTransferableResource() const {
  return !transfer_resource_.is_empty();
}

void LayerWithExternalTexture::RecomputeDrawsContentAndUVRect() {
  gfx::Size size(bounds_.size());
  if (texture_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(
        static_cast<float>(size.width()) / frame_size_in_dip_.width(),
        static_cast<float>(size.height()) / frame_size_in_dip_.height());
    texture_layer_->SetUV(uv_top_left, uv_bottom_right);
  }

  cc_layer_->SetBounds(size);
}

bool LayerWithExternalTexture::ShouldSchedulePaint() const {
  // A layer with an external texture needs to schedule a paint when it has a
  // transferable resource. Even though it doesn't use a delegate to paint
  // contents, scheduling a paint is necessary to accumulate damage and trigger
  // a frame draw in the compositor to display the updated texture.
  return HasTransferableResource();
}

void LayerWithExternalTexture::OnPaintScheduled() {
  ScheduleDraw();
}

bool LayerWithExternalTexture::ShouldCommitDamage() const {
  // A layer with an external texture needs to commit damage when it has a
  // transferable resource to display.
  return HasTransferableResource();
}

bool LayerWithExternalTexture::PrepareTransferableResource(
    viz::TransferableResource* resource,
    viz::ReleaseCallback* release_callback) {
  if (!transfer_release_callback_) {
    return false;
  }

  *resource = transfer_resource_;
  *release_callback = std::move(transfer_release_callback_);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// LayerWithExternalTexture, protected:

LayerWithExternalTexture::LayerWithExternalTexture(LayerType type)
    : Layer(type) {}

void LayerWithExternalTexture::Reset() {
  if (texture_layer_.get()) {
    texture_layer_->ClearClient();
  }

  texture_layer_ = nullptr;
  transfer_resource_ = viz::TransferableResource();
  if (transfer_release_callback_) {
    std::move(transfer_release_callback_).Run(gpu::SyncToken(), false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// LayerTextured, public:

LayerTextured::LayerTextured() : LayerWithExternalTexture(LAYER_TEXTURED) {
  content_layer_ = cc::PictureLayer::Create(this);
  cc_layer_ = content_layer_.get();
  InitializeCcLayer();
}

LayerTextured::~LayerTextured() {
  Destroy();
}

void LayerTextured::AddDeferredPaintRequest() {
  ++deferred_paint_requests_;
  TRACE_COUNTER("ui",
                perfetto::CounterTrack("DeferredPaintRequests",
                                       reinterpret_cast<uintptr_t>(this)),
                deferred_paint_requests_);
}

void LayerTextured::RemoveDeferredPaintRequest() {
  DCHECK_GT(deferred_paint_requests_, 0u);

  --deferred_paint_requests_;
  TRACE_COUNTER("ui",
                perfetto::CounterTrack("DeferredPaintRequests",
                                       reinterpret_cast<uintptr_t>(this)),
                deferred_paint_requests_);
  if (!deferred_paint_requests_ && !damaged_region().IsEmpty()) {
    ScheduleDraw();
  }
}

std::unique_ptr<Layer> LayerTextured::Clone() const {
  auto clone = Layer::Clone();
  clone->AsTextured()->SetFillsBoundsCompletely(FillsBoundsCompletely());
  return clone;
}

bool LayerTextured::ShouldSchedulePaint() const {
  // LayerTextured only needs to schedule paint if it has a delegate to paint
  // its contents, or if it has an external transferable resource to display.
  return delegate_ || LayerWithExternalTexture::ShouldSchedulePaint();
}

bool LayerTextured::SwitchCCLayerForTest() {
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return false;
  }

  scoped_refptr<cc::PictureLayer> new_layer = cc::PictureLayer::Create(this);
  SwitchToLayer(new_layer);

  content_layer_ = std::move(new_layer);
  return true;
}

scoped_refptr<cc::DisplayItemList> LayerTextured::PaintContentsToDisplayList() {
  TRACE_EVENT1("ui", "LayerTextured::PaintContentsToDisplayList", "name",
               name_);
  gfx::Rect local_bounds(bounds().size());
  gfx::Rect invalidation(
      gfx::IntersectRects(paint_region_.bounds(), local_bounds));
  paint_region_.Clear();
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  if (delegate_) {
    delegate_->OnPaintLayer(PaintContext(display_list.get(),
                                         device_scale_factor_, invalidation,
                                         GetCompositor()->is_pixel_canvas()));
  }
  display_list->Finalize();
  // TODO(domlaskowski): Move mirror invalidation to Layer::SchedulePaint.
  for (const auto& mirror : mirrors_) {
    mirror->dest()->SchedulePaint(invalidation);
  }

  return display_list;
}

bool LayerTextured::FillsBoundsCompletely() const {
  return fills_bounds_completely_;
}

void LayerTextured::SetFillsBoundsCompletely(bool fills_bounds_completely) {
  fills_bounds_completely_ = fills_bounds_completely;
}

bool LayerTextured::IsPaintDeferredForTesting() const {
  return deferred_paint_requests_;
}

void LayerTextured::OnPaintScheduled() {
  if (deferred_paint_requests_) {
    return;
  }

  ScheduleDraw();
}

bool LayerTextured::ShouldCommitDamage() const {
  // If painting is deferred, we don't commit the accumulated damage yet.
  if (deferred_paint_requests_) {
    return false;
  }

  // Otherwise, we commit damage if we have a delegate to paint our contents,
  // or if we have an external transferable resource.
  return delegate_ || LayerWithExternalTexture::ShouldCommitDamage();
}

void LayerTextured::CommitDamage(const cc::Region& damage) {
  Layer::CommitDamage(damage);
  paint_region_.Union(damage);
}

void LayerTextured::Reset() {
  LayerWithExternalTexture::Reset();

  if (content_layer_) {
    content_layer_->ClearClient();
  }

  content_layer_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LayerSolidColor, public:

LayerSolidColor::LayerSolidColor()
    : LayerWithExternalTexture(LAYER_SOLID_COLOR) {
  solid_color_layer_ = cc::SolidColorLayer::Create();
  cc_layer_ = solid_color_layer_.get();
  InitializeCcLayer();

  cc_layer_->SetSafeOpaqueBackgroundColor(SkColors::kBlack);
  cc_layer_->SetBackgroundColor(SkColors::kTransparent);

  // For LayerSolidColor, the background color dictates content opaqueness.
  cc_layer_->SetContentsOpaque(false);
  fills_bounds_opaquely_ = false;
}

LayerSolidColor::~LayerSolidColor() {
  Destroy();
}

std::unique_ptr<Layer> LayerSolidColor::Clone() const {
  auto clone = Layer::Clone();
  clone->AsSolidColor()->SetColor(GetTargetColor());
  return clone;
}

bool LayerSolidColor::ShouldSchedulePaint() const {
  // Only Schedule paint if LayerSolidColor has external content.
  return texture_layer() && LayerWithExternalTexture::ShouldSchedulePaint();
}

void LayerSolidColor::OnPaintScheduled() {
  ScheduleDraw();
}

bool LayerSolidColor::SwitchCCLayerForTest() {
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return false;
  }

  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  SwitchToLayer(new_layer);

  solid_color_layer_ = std::move(new_layer);
  return true;
}

void LayerSolidColor::SetShowReflectedLayerSubtree(
    Layer* subtree_reflected_layer) {
  DCHECK(subtree_reflected_layer);
  if (subtree_reflected_layer_ == subtree_reflected_layer) {
    return;
  }

  // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
  // destroyed.
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return;
  }

  scoped_refptr<cc::MirrorLayer> new_layer =
      cc::MirrorLayer::Create(subtree_reflected_layer->cc_layer_.get());
  SwitchToLayer(new_layer);

  mirror_layer_ = std::move(new_layer);

  subtree_reflected_layer_ = subtree_reflected_layer;
  auto insert_pair =
      subtree_reflected_layer_->subtree_reflecting_layers_.insert(this);
  DCHECK(insert_pair.second);

  MatchLayerSize(subtree_reflected_layer_);

  RecomputeDrawsContentAndUVRect();
}

void LayerSolidColor::SetShowSolidColorContent() {
  if (solid_color_layer_.get()) {
    return;
  }

  // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
  // destroyed.
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return;
  }

  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  SwitchToLayer(new_layer);

  solid_color_layer_ = new_layer;
  fills_bounds_opaquely_ = cc_layer_->background_color().isOpaque();

  RecomputeDrawsContentAndUVRect();
  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSolidColor()->SetShowSolidColorContent();
  }
}

void LayerSolidColor::SetColor(SkColor4f color) {
  GetAnimator()->SetColor(color);
}

SkColor4f LayerSolidColor::GetTargetColor() const {
  if (animator_ &&
      animator_->IsAnimatingProperty(LayerAnimationElement::COLOR)) {
    return animator_->GetTargetColor();
  }
  return cc_layer_->background_color();
}

SkColor4f LayerSolidColor::background_color() const {
  return cc_layer_->background_color();
}

void LayerSolidColor::Reset() {
  LayerWithExternalTexture::Reset();
  ResetSubtreeReflectedLayer();
  solid_color_layer_ = nullptr;
  mirror_layer_ = nullptr;
}

void LayerSolidColor::SetColorFromAnimation(SkColor4f color,
                                            PropertyChangeReason reason) {
  // For LayerSolidColor, the background color dictates content opaqueness.
  // And `SetContentOpaque()` is called in
  // `SolidColorLayer::SetBackgroundColor()`.
  cc_layer_->SetBackgroundColor(color);
  cc_layer_->SetSafeOpaqueBackgroundColor(color);
  SetFillsBoundsOpaquelyWithReason(color.isOpaque(), reason);
}

SkColor4f LayerSolidColor::GetColorForAnimation() const {
  // The NULL check is here since the underlying solid_color_layer can be
  // swapped with mirror_layer or textured_layer. See calls to
  // `SwitchToLayer()`
  return solid_color_layer_.get() ? solid_color_layer_->background_color()
                                  : SkColors::kBlack;
}

void LayerSolidColor::ResetSubtreeReflectedLayer() {
  if (!subtree_reflected_layer_) {
    return;
  }

  size_t result =
      subtree_reflected_layer_->subtree_reflecting_layers_.erase(this);
  DCHECK_EQ(1u, result);
  subtree_reflected_layer_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LayerNinePatch, public:

LayerNinePatch::LayerNinePatch() : Layer(LAYER_NINE_PATCH) {
  nine_patch_layer_ = cc::NinePatchLayer::Create();
  cc_layer_ = nine_patch_layer_.get();
  InitializeCcLayer();
}

LayerNinePatch::~LayerNinePatch() {
  Destroy();
}

bool LayerNinePatch::ShouldSchedulePaint() const {
  // LayerNinePatch draws a pre-defined image rather than requesting painted
  // content.
  return false;
}

void LayerNinePatch::UpdateNinePatchLayerImage(const gfx::ImageSkia& image) {
  nine_patch_layer_image_ = image;
  nine_patch_layer_->SetBitmap(
      image.GetRepresentation(device_scale_factor()).GetBitmap());
}

void LayerNinePatch::UpdateNinePatchLayerAperture(
    const gfx::Rect& aperture_in_dip) {
  nine_patch_layer_aperture_ = aperture_in_dip;

  // TODO(danakj): Specifying the aperture in DIPs as integers is not sufficient
  // and means the resulting aperture in pixels will not be exact.
  gfx::Rect aperture_in_pixel = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(aperture_in_dip, device_scale_factor()));
  nine_patch_layer_->SetAperture(aperture_in_pixel);
}

void LayerNinePatch::UpdateNinePatchLayerBorder(const gfx::Rect& border) {
  nine_patch_layer_->SetBorder(border);
}

void LayerNinePatch::UpdateNinePatchOcclusion(const gfx::Rect& occlusion) {
  nine_patch_layer_->SetLayerOcclusion(occlusion);
}

void LayerNinePatch::HandleDeviceScaleFactorChange() {
  Layer::HandleDeviceScaleFactorChange();

  if (!nine_patch_layer_image_.isNull()) {
    UpdateNinePatchLayerImage(nine_patch_layer_image_);
  }

  UpdateNinePatchLayerAperture(nine_patch_layer_aperture_);
}

void LayerNinePatch::Reset() {
  nine_patch_layer_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LayerSurface, public:

LayerSurface::LayerSurface() : Layer(LAYER_SURFACE) {
  surface_layer_ = cc::SurfaceLayer::Create();
  cc_layer_ = surface_layer_.get();

  InitializeCcLayer();
  surface_layer_->SetSurfaceHitTestable(true);
}

LayerSurface::~LayerSurface() {
  Destroy();
}

bool LayerSurface::HasExternalContent() const {
  return true;
}

bool LayerSurface::ShouldSchedulePaint() const {
  return false;
}

bool LayerSurface::SwitchCCLayerForTest() {
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return false;
  }

  scoped_refptr<cc::SurfaceLayer> new_layer = cc::SurfaceLayer::Create();
  SwitchToLayer(new_layer);

  surface_layer_ = std::move(new_layer);
  surface_layer_->SetSurfaceHitTestable(true);
  return true;
}

void LayerSurface::SetBackgroundColor(SkColor4f color) {
  surface_layer_->SetBackgroundColor(color);
  surface_layer_->SetSafeOpaqueBackgroundColor(color);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetBackgroundColor(color);
  }
}

SkColor4f LayerSurface::GetBackgroundColor() const {
  return surface_layer_->background_color();
}

std::unique_ptr<Layer> LayerSurface::Clone() const {
  auto clone = Layer::Clone();
  auto* cloned_surface = clone->AsSurface();

  cloned_surface->SetBackgroundColor(surface_layer_->background_color());
  cloned_surface->SetShowSurface(
      surface_layer_->surface_id(), frame_size_in_dip_,
      surface_layer_->deadline_in_frames()
          ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                *surface_layer_->deadline_in_frames())
          : cc::DeadlinePolicy::UseDefaultDeadline(),
      surface_layer_->stretch_content_to_fill_bounds());
  if (surface_layer_->oldest_acceptable_fallback()) {
    cloned_surface->SetOldestAcceptableFallback(
        *surface_layer_->oldest_acceptable_fallback());
  }

  return clone;
}

void LayerSurface::SetShowSurface(const viz::SurfaceId& surface_id,
                                  const gfx::Size& frame_size_in_dip,
                                  const cc::DeadlinePolicy& deadline_policy,
                                  bool stretch_content_to_fill_bounds) {
  surface_layer_->SetSurfaceId(surface_id, deadline_policy);
  surface_layer_->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);

  frame_size_in_dip_ = frame_size_in_dip;
  RecomputeDrawsContentAndUVRect();

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetShowSurface(surface_id, frame_size_in_dip,
                                                deadline_policy,
                                                stretch_content_to_fill_bounds);
  }
}

void LayerSurface::SetShowSurface(const viz::SurfaceId& surface_id,
                                  const cc::DeadlinePolicy& deadline_policy,
                                  bool stretch_content_to_fill_bounds) {
  // Assumes `frame_size_in_dip_` is already set.
  // TODO(crbug.com/40285157): with surface sync, it should use on `bounds_`.
  surface_layer_->SetSurfaceId(surface_id, deadline_policy);
  surface_layer_->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetShowSurface(surface_id, deadline_policy,
                                                stretch_content_to_fill_bounds);
  }
}

void LayerSurface::SetShowReflectedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_pixels) {
  surface_layer_->SetSurfaceId(surface_id,
                               cc::DeadlinePolicy::UseInfiniteDeadline());
  surface_layer_->SetBackgroundColor(SkColors::kBlack);
  surface_layer_->SetSafeOpaqueBackgroundColor(SkColors::kBlack);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetIsReflection(true);

  // The reflecting surface uses the native size of the reflected display.
  frame_size_in_dip_ = frame_size_in_pixels;
  RecomputeDrawsContentAndUVRect();
}

void LayerSurface::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  surface_layer_->SetOldestAcceptableFallback(surface_id);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetOldestAcceptableFallback(surface_id);
  }
}

const viz::SurfaceId* LayerSurface::GetOldestAcceptableFallback() const {
  if (surface_layer_->oldest_acceptable_fallback()) {
    return &surface_layer_->oldest_acceptable_fallback().value();
  }

  return nullptr;
}

bool LayerSurface::StretchContentToFillBounds() const {
  return surface_layer_->stretch_content_to_fill_bounds();
}

void LayerSurface::SetSurfaceSize(gfx::Size surface_size_in_dip) {
  if (frame_size_in_dip_ == surface_size_in_dip) {
    return;
  }

  frame_size_in_dip_ = surface_size_in_dip;
  RecomputeDrawsContentAndUVRect();
}

const viz::SurfaceId* LayerSurface::GetSurfaceId() const {
  if (surface_layer_->surface_id().is_valid()) {
    return &surface_layer_->surface_id();
  }

  return nullptr;
}

void LayerSurface::RecomputeDrawsContentAndUVRect() {
  gfx::Size size(bounds_.size());
  // TODO(crbug.com/40285157): with surface sync, size shouldn't rely on
  // `frame_size_in_dip_` anymore.
  size.SetToMin(frame_size_in_dip_);
  cc_layer_->SetBounds(size);
}

void LayerSurface::Reset() {
  surface_layer_ = nullptr;
}

}  // namespace ui
