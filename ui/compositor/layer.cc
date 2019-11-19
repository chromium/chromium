// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
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
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
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
  float diff = std::abs(snapped_position - gfx::ToRoundedInt(snapped_position));
  DCHECK_LT(diff, kEplison);
}
#endif

}  // namespace

class Layer::LayerMirror : public LayerDelegate, LayerObserver {
 public:
  LayerMirror(Layer* source, Layer* dest)
      : source_(source), dest_(dest) {
    dest->AddObserver(this);
    dest->set_delegate(this);
  }

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
  Layer* const source_;
  Layer* const dest_;

  DISALLOW_COPY_AND_ASSIGN(LayerMirror);
};

// Manages the subpixel offset data for a given set of parameters (device
// scale factor and DIP offset from parent layer).
class Layer::SubpixelPositionOffsetCache {
 public:
  SubpixelPositionOffsetCache() = default;
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
    offset.Scale(1.f / device_scale_factor);

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

  DISALLOW_COPY_AND_ASSIGN(SubpixelPositionOffsetCache);
};

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(nullptr),
      parent_(nullptr),
      subpixel_position_offset_(
          std::make_unique<SubpixelPositionOffsetCache>()),
      visible_(true),
      fills_bounds_opaquely_(true),
      fills_bounds_completely_(false),
      background_blur_sigma_(0.0f),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_blur_sigma_(0.0f),
      layer_mask_(nullptr),
      layer_mask_back_link_(nullptr),
      zoom_(1),
      zoom_inset_(0),
      delegate_(nullptr),
      owner_(nullptr),
      cc_layer_(nullptr),
      device_scale_factor_(1.0f),
      cache_render_surface_requests_(0),
      deferred_paint_requests_(0),
      backdrop_filter_quality_(1.0f),
      trilinear_filtering_request_(0) {
  CreateCcLayer();
}

Layer::~Layer() {
  for (auto& observer : observer_list_)
    observer.LayerDestroyed(this);

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
  for (auto* child : children_)
    child->parent_ = nullptr;

  if (content_layer_)
    content_layer_->ClearClient();
  cc_layer_->RemoveFromParent();
  if (transfer_release_callback_)
    transfer_release_callback_->Run(gpu::SyncToken(), false);

  ResetSubtreeReflectedLayer();
}

std::unique_ptr<Layer> Layer::Clone() const {
  auto clone = std::make_unique<Layer>(type_);

  // Background filters.
  clone->SetBackgroundBlur(background_blur_sigma_);
  clone->SetBackgroundZoom(zoom_, zoom_inset_);

  // Filters.
  clone->SetLayerSaturation(layer_saturation_);
  clone->SetLayerBrightness(GetTargetBrightness());
  clone->SetLayerGrayscale(GetTargetGrayscale());
  clone->SetLayerInverted(layer_inverted_);
  clone->SetLayerBlur(layer_blur_sigma_);
  if (alpha_shape_)
    clone->SetAlphaShape(std::make_unique<ShapeRects>(*alpha_shape_));

  // cc::Layer state.
  if (surface_layer_) {
    clone->SetShowSurface(surface_layer_->surface_id(), frame_size_in_dip_,
                          surface_layer_->background_color(),
                          surface_layer_->deadline_in_frames()
                              ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                                    *surface_layer_->deadline_in_frames())
                              : cc::DeadlinePolicy::UseDefaultDeadline(),
                          surface_layer_->stretch_content_to_fill_bounds());
    if (surface_layer_->oldest_acceptable_fallback())
      clone->SetOldestAcceptableFallback(
          *surface_layer_->oldest_acceptable_fallback());
  } else if (type_ == LAYER_SOLID_COLOR) {
    clone->SetColor(GetTargetColor());
  }

  clone->SetTransform(GetTargetTransform());
  clone->SetBounds(bounds_);
  if (subpixel_position_offset_->has_explicit_subpixel_offset())
    clone->SetSubpixelPositionOffset(GetSubpixelOffset());
  clone->SetMasksToBounds(GetMasksToBounds());
  clone->SetOpacity(GetTargetOpacity());
  clone->SetVisible(GetTargetVisibility());
  clone->SetAcceptEvents(accept_events());
  clone->SetFillsBoundsOpaquely(fills_bounds_opaquely_);
  clone->SetFillsBoundsCompletely(fills_bounds_completely_);
  clone->SetRoundedCornerRadius(rounded_corner_radii());
  clone->SetIsFastRoundedCorner(is_fast_rounded_corner());
  clone->set_name(name_);

  return clone;
}

std::unique_ptr<Layer> Layer::Mirror() {
  auto mirror = Clone();
  mirrors_.emplace_back(std::make_unique<LayerMirror>(this, mirror.get()));

  if (!transfer_resource_.mailbox_holder.mailbox.IsZero()) {
    // Send an empty release callback because we don't want the resource to be
    // freed up until the original layer releases it.
    mirror->SetTransferableResource(
        transfer_resource_,
        viz::SingleReleaseCallback::Create(base::BindOnce(
            [](const gpu::SyncToken& sync_token, bool is_lost) {})),
        frame_size_in_dip_);
  }

  return mirror;
}

void Layer::SetShowReflectedLayerSubtree(Layer* subtree_reflected_layer) {
  DCHECK(subtree_reflected_layer);
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);

  if (subtree_reflected_layer_ == subtree_reflected_layer)
    return;

  scoped_refptr<cc::MirrorLayer> new_layer =
      cc::MirrorLayer::Create(subtree_reflected_layer->cc_layer_);
  SwitchToLayer(new_layer);
  mirror_layer_ = std::move(new_layer);

  subtree_reflected_layer_ = subtree_reflected_layer;
  auto insert_pair =
      subtree_reflected_layer_->subtree_reflecting_layers_.insert(this);
  DCHECK(insert_pair.second);

  MatchLayerSize(subtree_reflected_layer_);

  RecomputeDrawsContentAndUVRect();
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

  root_layer->AddChild(cc_layer_);
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
  cc_layer_->AddChild(child->cc_layer_);
  child->OnDeviceScaleFactorChanged(device_scale_factor_);
  Compositor* compositor = GetCompositor();
  if (compositor)
    child->SetCompositorForAnimatorsInTree(compositor);
}

void Layer::Remove(Layer* child) {
  // Current bounds are used to calculate offsets when layers are reparented.
  // Stop (and complete) an ongoing animation to update the bounds immediately.
  LayerAnimator* child_animator = child->animator_.get();
  if (child_animator)
    child_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

  Compositor* compositor = GetCompositor();
  if (compositor)
    child->ResetCompositorForAnimatorsInTree(compositor);

  auto i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
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

void Layer::SetAnimator(LayerAnimator* animator) {
  Compositor* compositor = GetCompositor();

  if (animator_) {
    if (compositor && !layer_mask_back_link())
      animator_->DetachLayerAndTimeline(compositor);
    animator_->SetDelegate(nullptr);
  }

  animator_ = animator;

  if (animator_) {
    animator_->SetDelegate(this);
    if (compositor && !layer_mask_back_link())
      animator_->AttachLayerAndTimeline(compositor);
  }
}

LayerAnimator* Layer::GetAnimator() {
  if (!animator_)
    SetAnimator(LayerAnimator::CreateDefaultAnimator());
  return animator_.get();
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

void Layer::SetBackgroundBlur(float blur_sigma) {
  background_blur_sigma_ = blur_sigma;

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

void Layer::SetLayerInverted(bool inverted) {
  layer_inverted_ = inverted;
  SetLayerFilters();
}

void Layer::SetMaskLayer(Layer* layer_mask) {
  if (layer_mask_ == layer_mask)
    return;
  // The provided mask should not have a layer mask itself.
  DCHECK(!layer_mask ||
         (!layer_mask->layer_mask_layer() && layer_mask->children().empty() &&
          !layer_mask->layer_mask_back_link_));
  DCHECK(!layer_mask_back_link_);
  DCHECK(!layer_mask || layer_mask->type_ == LAYER_TEXTURED);
  // Masks must be backed by a PictureLayer.
  DCHECK(!layer_mask || layer_mask->content_layer_);
  // We need to de-reference the currently linked object so that no problem
  // arises if the mask layer gets deleted before this object.
  if (layer_mask_)
    layer_mask_->layer_mask_back_link_ = nullptr;
  layer_mask_ = layer_mask;
  cc_layer_->SetMaskLayer(layer_mask ? layer_mask->content_layer_.get()
                                     : nullptr);
  // We need to reference the linked object so that it can properly break the
  // link to us when it gets deleted.
  if (layer_mask) {
    layer_mask->layer_mask_back_link_ = this;
    layer_mask->OnDeviceScaleFactorChanged(device_scale_factor_);
  }
}

void Layer::SetBackgroundZoom(float zoom, int inset) {
  zoom_ = zoom;
  zoom_inset_ = inset;

  SetLayerBackgroundFilters();
}

void Layer::SetAlphaShape(std::unique_ptr<ShapeRects> shape) {
  alpha_shape_ = std::move(shape);

  SetLayerFilters();

  if (delegate_)
    delegate_->OnLayerAlphaShapeChanged();
}

void Layer::SetLayerFilters() {
  cc::FilterOperations filters;
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
  if (layer_blur_sigma_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(
        layer_blur_sigma_, SkBlurImageFilter::kClamp_TileMode));
  }
  // Brightness goes last, because the resulting colors neeed clamping, which
  // cause further color matrix filters to be applied separately. In this order,
  // they all can be combined in a single pass.
  if (layer_brightness_) {
    filters.Append(cc::FilterOperation::CreateSaturatingBrightnessFilter(
        layer_brightness_));
  }
  if (alpha_shape_) {
    filters.Append(cc::FilterOperation::CreateAlphaThresholdFilter(
            *alpha_shape_, 0.f, 0.f));
  }

  cc_layer_->SetFilters(filters);
}

void Layer::SetLayerBackgroundFilters() {
  cc::FilterOperations filters;
  if (zoom_ != 1)
    filters.Append(cc::FilterOperation::CreateZoomFilter(zoom_, zoom_inset_));

  if (background_blur_sigma_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(
        background_blur_sigma_, SkBlurImageFilter::kClamp_TileMode));
  }
  cc_layer_->SetBackdropFilters(filters);
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

bool Layer::IsDrawn() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == nullptr;
}

void Layer::SetRoundedCornerRadius(const gfx::RoundedCornersF& corner_radii) {
  GetAnimator()->SetRoundedCorners(corner_radii);
}

void Layer::SetIsFastRoundedCorner(bool enable) {
  cc_layer_->SetIsFastRoundedCorner(enable);
  ScheduleDraw();

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetIsFastRoundedCorner(enable);
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                gfx::PointF* point) {
  if (source == target)
    return;

  const Layer* root_layer = GetRoot(source);
  CHECK_EQ(root_layer, GetRoot(target));

  if (source != root_layer)
    source->ConvertPointForAncestor(root_layer, point);
  if (target != root_layer)
    target->ConvertPointFromAncestor(root_layer, point);
}

bool Layer::GetTargetTransformRelativeTo(const Layer* ancestor,
                                         gfx::Transform* transform) const {
  const Layer* p = this;
  for (; p && p != ancestor; p = p->parent()) {
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->bounds().x()),
                          static_cast<float>(p->bounds().y()));
    // Use target transform so that result will be correct once animation is
    // finished.
    if (!p->GetTargetTransform().IsIdentity())
      transform->ConcatTransform(p->GetTargetTransform());
    transform->ConcatTransform(translation);
  }
  return p == ancestor;
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  cc_layer_->SetContentsOpaque(fills_bounds_opaquely);

  if (delegate_)
    delegate_->OnLayerFillsBoundsOpaquelyChanged();
}

void Layer::SetFillsBoundsCompletely(bool fills_bounds_completely) {
  fills_bounds_completely_ = fills_bounds_completely;
}

void Layer::SwitchToLayer(scoped_refptr<cc::Layer> new_layer) {
  // Finish animations being handled by cc_layer_.
  if (animator_) {
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
    animator_->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    animator_->SwitchToLayer(new_layer);
  }

  ResetSubtreeReflectedLayer();

  if (texture_layer_.get())
    texture_layer_->ClearClient();

  cc_layer_->RemoveAllChildren();
  if (cc_layer_->parent()) {
    cc_layer_->parent()->ReplaceChild(cc_layer_, new_layer);
  }
  cc_layer_->ClearDebugInfo();

  new_layer->SetOpacity(cc_layer_->opacity());
  new_layer->SetTransform(cc_layer_->transform());
  new_layer->SetPosition(cc_layer_->position());
  new_layer->SetBackgroundColor(cc_layer_->background_color());
  new_layer->SetSafeOpaqueBackgroundColor(
      cc_layer_->SafeOpaqueBackgroundColor());
  new_layer->SetCacheRenderSurface(cc_layer_->cache_render_surface());
  new_layer->SetTrilinearFiltering(cc_layer_->trilinear_filtering());
  new_layer->SetRoundedCorner(cc_layer_->corner_radii());
  new_layer->SetIsFastRoundedCorner(cc_layer_->is_fast_rounded_corner());
  new_layer->SetMasksToBounds(cc_layer_->masks_to_bounds());

  cc_layer_ = new_layer.get();
  if (content_layer_) {
    content_layer_->ClearClient();
    content_layer_ = nullptr;
  }
  solid_color_layer_ = nullptr;
  texture_layer_ = nullptr;
  surface_layer_ = nullptr;
  mirror_layer_ = nullptr;

  for (auto* child : children_) {
    DCHECK(child->cc_layer_);
    cc_layer_->AddChild(child->cc_layer_);
  }
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(fills_bounds_opaquely_);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetHitTestable(IsHitTestableForCC());
  cc_layer_->SetHideLayerAndSubtree(!visible_);
  cc_layer_->SetBackdropFilterQuality(backdrop_filter_quality_);
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));
  cc_layer_->EnsureDebugInfo().name = name_;

  SetLayerFilters();
  SetLayerBackgroundFilters();
}

void Layer::SwitchCCLayerForTest() {
  scoped_refptr<cc::PictureLayer> new_layer = cc::PictureLayer::Create(this);
  SwitchToLayer(new_layer);
  content_layer_ = std::move(new_layer);
}

// Note: The code that sets this flag would be responsible to unset it on that
// ui::Layer. We do not want to clone this flag to a cloned layer by accident,
// which could be a supprise. But we want to preserve it after switching to a
// new cc::Layer. There could be a whole subtree and the root changed, but does
// not mean we want to treat the cache all different.
void Layer::AddCacheRenderSurfaceRequest() {
  ++cache_render_surface_requests_;
  TRACE_COUNTER_ID1("ui", "CacheRenderSurfaceRequests", this,
                    cache_render_surface_requests_);
  if (cache_render_surface_requests_ == 1)
    cc_layer_->SetCacheRenderSurface(true);
}

void Layer::RemoveCacheRenderSurfaceRequest() {
  DCHECK_GT(cache_render_surface_requests_, 0u);

  --cache_render_surface_requests_;
  TRACE_COUNTER_ID1("ui", "CacheRenderSurfaceRequests", this,
                    cache_render_surface_requests_);
  if (cache_render_surface_requests_ == 0)
    cc_layer_->SetCacheRenderSurface(false);
}

void Layer::SetBackdropFilterQuality(const float quality) {
  backdrop_filter_quality_ = quality / GetDeviceScaleFactor();
  cc_layer_->SetBackdropFilterQuality(backdrop_filter_quality_);
}
void Layer::AddDeferredPaintRequest() {
  ++deferred_paint_requests_;
  TRACE_COUNTER_ID1("ui", "DeferredPaintRequests", this,
                    deferred_paint_requests_);
}

void Layer::RemoveDeferredPaintRequest() {
  DCHECK_GT(deferred_paint_requests_, 0u);

  --deferred_paint_requests_;
  TRACE_COUNTER_ID1("ui", "DeferredPaintRequests", this,
                    deferred_paint_requests_);
  if (!deferred_paint_requests_ && !damaged_region_.IsEmpty())
    ScheduleDraw();
}

// Note: The code that sets this flag would be responsible to unset it on that
// ui::Layer. We do not want to clone this flag to a cloned layer by accident,
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

bool Layer::StretchContentToFillBounds() const {
  DCHECK(surface_layer_);
  return surface_layer_->stretch_content_to_fill_bounds();
}

void Layer::SetSurfaceSize(gfx::Size surface_size_in_dip) {
  DCHECK(surface_layer_);
  if (frame_size_in_dip_ == surface_size_in_dip)
    return;
  frame_size_in_dip_ = surface_size_in_dip;
  RecomputeDrawsContentAndUVRect();
}

bool Layer::ContainsMirrorForTest(Layer* mirror) const {
  const auto it =
      std::find_if(mirrors_.begin(), mirrors_.end(),
                   [mirror](const std::unique_ptr<LayerMirror>& mirror_ptr) {
                     return mirror_ptr.get()->dest() == mirror;
                   });
  return it != mirrors_.end();
}

void Layer::SetTransferableResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);
  DCHECK(!resource.mailbox_holder.mailbox.IsZero());
  DCHECK(release_callback);
  DCHECK(!resource.is_software);
  if (!texture_layer_.get()) {
    scoped_refptr<cc::TextureLayer> new_layer =
        cc::TextureLayer::CreateForMailbox(this);
    new_layer->SetFlipped(true);
    SwitchToLayer(new_layer);
    texture_layer_ = new_layer;
    // Reset the frame_size_in_dip_ so that SetTextureSize() will not early out,
    // the frame_size_in_dip_ was for a previous (different) |texture_layer_|.
    frame_size_in_dip_ = gfx::Size();
  }
  if (transfer_release_callback_)
    transfer_release_callback_->Run(gpu::SyncToken(), false);
  transfer_release_callback_ = std::move(release_callback);
  transfer_resource_ = resource;
  SetTextureSize(texture_size_in_dip);

  for (const auto& mirror : mirrors_) {
    // The release callbacks should be empty as only the source layer
    // should be able to release the texture resource.
    mirror->dest()->SetTransferableResource(
        transfer_resource_,
        viz::SingleReleaseCallback::Create(base::BindOnce(
            [](const gpu::SyncToken& sync_token, bool is_lost) {})),
        frame_size_in_dip_);
  }
}

void Layer::SetTextureSize(gfx::Size texture_size_in_dip) {
  DCHECK(texture_layer_.get());
  if (frame_size_in_dip_ == texture_size_in_dip)
    return;
  frame_size_in_dip_ = texture_size_in_dip;
  RecomputeDrawsContentAndUVRect();
  texture_layer_->SetNeedsDisplay();
}

void Layer::SetTextureFlipped(bool flipped) {
  DCHECK(texture_layer_.get());
  texture_layer_->SetFlipped(flipped);
}

bool Layer::TextureFlipped() const {
  DCHECK(texture_layer_.get());
  return texture_layer_->flipped();
}

void Layer::SetShowSurface(const viz::SurfaceId& surface_id,
                           const gfx::Size& frame_size_in_dip,
                           SkColor default_background_color,
                           const cc::DeadlinePolicy& deadline_policy,
                           bool stretch_content_to_fill_bounds) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);

  CreateSurfaceLayerIfNecessary();

  surface_layer_->SetSurfaceId(surface_id, deadline_policy);
  surface_layer_->SetBackgroundColor(default_background_color);
  surface_layer_->SetSafeOpaqueBackgroundColor(default_background_color);
  surface_layer_->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);

  frame_size_in_dip_ = frame_size_in_dip;
  RecomputeDrawsContentAndUVRect();

  for (const auto& mirror : mirrors_) {
    mirror->dest()->SetShowSurface(surface_id, frame_size_in_dip,
                                   default_background_color, deadline_policy,
                                   stretch_content_to_fill_bounds);
  }
}

void Layer::SetOldestAcceptableFallback(const viz::SurfaceId& surface_id) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);

  CreateSurfaceLayerIfNecessary();

  surface_layer_->SetOldestAcceptableFallback(surface_id);

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetOldestAcceptableFallback(surface_id);
}

void Layer::SetShowReflectedSurface(const viz::SurfaceId& surface_id,
                                    const gfx::Size& frame_size_in_pixels) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);

  if (!surface_layer_) {
    scoped_refptr<cc::SurfaceLayer> new_layer = cc::SurfaceLayer::Create();
    SwitchToLayer(new_layer);
    surface_layer_ = new_layer;
  }

  surface_layer_->SetSurfaceId(surface_id,
                               cc::DeadlinePolicy::UseInfiniteDeadline());
  surface_layer_->SetBackgroundColor(SK_ColorBLACK);
  surface_layer_->SetSafeOpaqueBackgroundColor(SK_ColorBLACK);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetIsReflection(true);

  // The reflecting surface uses the native size of the reflected display.
  frame_size_in_dip_ = frame_size_in_pixels;
  RecomputeDrawsContentAndUVRect();
}

const viz::SurfaceId* Layer::GetSurfaceId() const {
  if (surface_layer_)
    return &surface_layer_->surface_id();
  return nullptr;
}

const viz::SurfaceId* Layer::GetOldestAcceptableFallback() const {
  if (surface_layer_ && surface_layer_->oldest_acceptable_fallback())
    return &surface_layer_->oldest_acceptable_fallback().value();
  return nullptr;
}

void Layer::SetShowSolidColorContent() {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);

  if (solid_color_layer_.get())
    return;

  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  SwitchToLayer(new_layer);
  solid_color_layer_ = new_layer;

  transfer_resource_ = viz::TransferableResource();
  if (transfer_release_callback_) {
    transfer_release_callback_->Run(gpu::SyncToken(), false);
    transfer_release_callback_.reset();
  }
  RecomputeDrawsContentAndUVRect();

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetShowSolidColorContent();
}

void Layer::UpdateNinePatchLayerImage(const gfx::ImageSkia& image) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());

  nine_patch_layer_image_ = image;
  // TODO(estade): we don't clean up old bitmaps in the UIResourceManager when
  // the scale factor changes. Currently for the way NinePatchLayers are used,
  // we don't need/want to, but we should address this in the future if it
  // becomes an issue.
  nine_patch_layer_->SetBitmap(
      image.GetRepresentation(device_scale_factor_).GetBitmap());
}

void Layer::UpdateNinePatchLayerAperture(const gfx::Rect& aperture_in_dip) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_aperture_ = aperture_in_dip;
  gfx::Rect aperture_in_pixel = ConvertRectToPixel(this, aperture_in_dip);
  nine_patch_layer_->SetAperture(aperture_in_pixel);
}

void Layer::UpdateNinePatchLayerBorder(const gfx::Rect& border) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_->SetBorder(border);
}

void Layer::UpdateNinePatchOcclusion(const gfx::Rect& occlusion) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_->SetLayerOcclusion(occlusion);
}

void Layer::SetColor(SkColor color) { GetAnimator()->SetColor(color); }

SkColor Layer::GetTargetColor() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::COLOR))
    return animator_->GetTargetColor();
  return cc_layer_->background_color();
}

SkColor Layer::background_color() const {
  return cc_layer_->background_color();
}

bool Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  if (type_ == LAYER_SOLID_COLOR && !texture_layer_)
    return false;
  if (type_ == LAYER_NINE_PATCH)
    return false;
  if (!delegate_ && transfer_resource_.mailbox_holder.mailbox.IsZero())
    return false;

  damaged_region_.Union(invalid_rect);
  if (layer_mask_)
    layer_mask_->damaged_region_.Union(invalid_rect);

  if (!content_layer_ || !deferred_paint_requests_)
    ScheduleDraw();
  return true;
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::SendDamagedRects() {
  if (layer_mask_)
    layer_mask_->SendDamagedRects();
  if (delegate_)
    delegate_->UpdateVisualState();

  if (damaged_region_.IsEmpty())
    return;
  if (!delegate_ && transfer_resource_.mailbox_holder.mailbox.IsZero())
    return;
  if (content_layer_ && deferred_paint_requests_)
    return;

  for (gfx::Rect damaged_rect : damaged_region_)
    cc_layer_->SetNeedsDisplayRect(damaged_rect);

  if (content_layer_)
    paint_region_.Union(damaged_region_);
  damaged_region_.Clear();
}

void Layer::CompleteAllAnimations() {
  typedef std::vector<scoped_refptr<LayerAnimator> > LayerAnimatorVector;
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
  std::vector<Layer*> new_children_order;
  new_children_order.reserve(children_.size());

  cc::LayerList new_cc_children_order;
  new_cc_children_order.reserve(cc_layer_->children().size());

  for (Layer* leading_child : new_leading_children) {
    DCHECK_EQ(leading_child->cc_layer_->parent(), cc_layer_);
    DCHECK_EQ(leading_child->parent(), this);
    new_children_order.emplace_back(leading_child);
    new_cc_children_order.emplace_back(
        scoped_refptr<cc::Layer>(leading_child->cc_layer_));
  }

  base::flat_set<Layer*> reordered_children(new_children_order);

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
  for (auto* child : children_)
    child->SuppressPaint();
}

void Layer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;

  base::WeakPtr<Layer> weak_this = weak_ptr_factory_.GetWeakPtr();

  // NOTE: Some animation observers destroy the layer when the animation ends.
  if (animator_) {
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);

    // Do not proceed if the layer was destroyed due to an animation
    // observer.
    if (!weak_this)
      return;
  }

  const float old_device_scale_factor = device_scale_factor_;
  device_scale_factor_ = device_scale_factor;
  RecomputeDrawsContentAndUVRect();
  RecomputePosition();
  if (nine_patch_layer_) {
    if (!nine_patch_layer_image_.isNull())
      UpdateNinePatchLayerImage(nine_patch_layer_image_);
    UpdateNinePatchLayerAperture(nine_patch_layer_aperture_);
  }
  SchedulePaint(gfx::Rect(bounds_.size()));
  if (delegate_) {
    delegate_->OnDeviceScaleFactorChanged(old_device_scale_factor,
                                          device_scale_factor);
  }
  for (auto* child : children_) {
    child->OnDeviceScaleFactorChanged(device_scale_factor);

    // A child layer may have triggered a delegate or an observer to delete
    // |this| layer. In which case return early to avoid crash.
    if (!weak_this)
      return;
  }
  if (layer_mask_)
    layer_mask_->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Layer::SetDidScrollCallback(
    base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                 const cc::ElementId&)> callback) {
  cc_layer_->SetDidScrollCallback(std::move(callback));
}

void Layer::SetScrollable(const gfx::Size& container_bounds) {
  cc_layer_->SetScrollable(container_bounds);
  cc_layer_->SetUserScrollable(true, true);
}

gfx::ScrollOffset Layer::CurrentScrollOffset() const {
  const Compositor* compositor = GetCompositor();
  gfx::ScrollOffset offset;
  if (compositor &&
      compositor->GetScrollOffsetForLayer(cc_layer_->element_id(), &offset))
    return offset;
  return cc_layer_->CurrentScrollOffset();
}

void Layer::SetScrollOffset(const gfx::ScrollOffset& offset) {
  Compositor* compositor = GetCompositor();
  bool scrolled_on_impl_side =
      compositor && compositor->ScrollLayerTo(cc_layer_->element_id(), offset);

  if (!scrolled_on_impl_side)
    cc_layer_->SetScrollOffset(offset);

  DCHECK_EQ(offset.x(), CurrentScrollOffset().x());
  DCHECK_EQ(offset.y(), CurrentScrollOffset().y());
}

void Layer::RequestCopyOfOutput(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  cc_layer_->RequestCopyOfOutput(std::move(request));
}

gfx::Rect Layer::PaintableRegion() {
  return gfx::Rect(size());
}

scoped_refptr<cc::DisplayItemList> Layer::PaintContentsToDisplayList(
    ContentLayerClient::PaintingControlSetting painting_control) {
  TRACE_EVENT1("ui", "Layer::PaintContentsToDisplayList", "name", name_);
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
  for (const auto& mirror : mirrors_)
    mirror->dest()->SchedulePaint(invalidation);
  return display_list;
}

bool Layer::FillsBoundsCompletely() const { return fills_bounds_completely_; }

size_t Layer::GetApproximateUnsharedMemoryUsage() const {
  // Most of the "picture memory" is shared with the cc::DisplayItemList, so
  // there's nothing significant to report here.
  return 0;
}

bool Layer::PrepareTransferableResource(
    cc::SharedBitmapIdRegistrar* bitmap_registar,
    viz::TransferableResource* resource,
    std::unique_ptr<viz::SingleReleaseCallback>* release_callback) {
  if (!transfer_release_callback_)
    return false;
  *resource = transfer_resource_;
  *release_callback = std::move(transfer_release_callback_);
  return true;
}

void Layer::CollectAnimators(
    std::vector<scoped_refptr<LayerAnimator>>* animators) {
  if (animator_ && animator_->is_animating())
    animators->push_back(animator_);
  for (auto* child : children_)
    child->CollectAnimators(animators);
}

void Layer::StackRelativeTo(Layer* child, Layer* other, bool above) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t other_i =
      std::find(children_.begin(), children_.end(), other) - children_.begin();
  if ((above && child_i == other_i + 1) || (!above && child_i + 1 == other_i))
    return;

  const size_t dest_i =
      above ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);

  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  child->cc_layer_->RemoveFromParent();
  cc_layer_->InsertChild(child->cc_layer_, dest_i);
}

bool Layer::ConvertPointForAncestor(const Layer* ancestor,
                                    gfx::PointF* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  auto p = gfx::Point3F(*point);
  transform.TransformPoint(&p);
  *point = p.AsPointF();
  return result;
}

bool Layer::ConvertPointFromAncestor(const Layer* ancestor,
                                     gfx::PointF* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  auto p = gfx::Point3F(*point);
  transform.TransformPointReverse(&p);
  *point = p.AsPointF();
  return result;
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

  if (delegate_)
    delegate_->OnLayerBoundsChanged(old_bounds, reason);

  if (bounds.size() == old_bounds.size()) {
    // Don't schedule a draw if we're invisible. We'll schedule one
    // automatically when we get visible.
    if (IsDrawn())
      ScheduleDraw();
  } else {
    // Always schedule a paint, even if we're invisible.
    SchedulePaint(gfx::Rect(bounds.size()));
  }

  for (const auto& mirror : mirrors_) {
    Layer* mirror_dest = mirror->dest();
    if (mirror_dest->sync_bounds_with_source_)
      mirror_dest->SetBounds(bounds);
  }

  for (auto* reflecting_layer : subtree_reflecting_layers_)
    reflecting_layer->MatchLayerSize(this);
}

void Layer::SetTransformFromAnimation(const gfx::Transform& transform,
                                      PropertyChangeReason reason) {
  const gfx::Transform old_transform = this->transform();
  cc_layer_->SetTransform(transform);

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
    if (mirror_dest->sync_visibility_with_source_)
      mirror_dest->SetVisible(visible);
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

void Layer::SetColorFromAnimation(SkColor color, PropertyChangeReason reason) {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);
  cc_layer_->SetBackgroundColor(color);
  cc_layer_->SetSafeOpaqueBackgroundColor(color);
  SetFillsBoundsOpaquely(SkColorGetA(color) == 0xFF);
}

void Layer::SetClipRectFromAnimation(const gfx::Rect& clip_rect,
                                     PropertyChangeReason reason) {
  cc_layer_->SetClipRect(clip_rect);
}

void Layer::SetRoundedCornersFromAnimation(
    const gfx::RoundedCornersF& rounded_corners,
    PropertyChangeReason reason) {
  cc_layer_->SetRoundedCorner(rounded_corners);

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetRoundedCornersFromAnimation(rounded_corners, reason);
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

SkColor Layer::GetColorForAnimation() const {
  // The NULL check is here since this is invoked regardless of whether we have
  // been configured as LAYER_SOLID_COLOR.
  return solid_color_layer_.get() ?
      solid_color_layer_->background_color() : SK_ColorBLACK;
}

gfx::Rect Layer::GetClipRectForAnimation() const {
  if (clip_rect().IsEmpty())
    return gfx::Rect(size());
  return clip_rect();
}

gfx::RoundedCornersF Layer::GetRoundedCornersForAnimation() const {
  return rounded_corner_radii();
}

float Layer::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

LayerAnimatorCollection* Layer::GetLayerAnimatorCollection() {
  Compositor* compositor = GetCompositor();
  return compositor ? compositor->layer_animator_collection() : nullptr;
}

int Layer::GetFrameNumber() const {
  const Compositor* compositor = GetCompositor();
  return compositor ? compositor->activated_frame_count() : 0;
}

float Layer::GetRefreshRate() const {
  const Compositor* compositor = GetCompositor();
  return compositor ? compositor->refresh_rate() : 60.0;
}

ui::Layer* Layer::GetLayer() {
  return this;
}

cc::Layer* Layer::GetCcLayer() const {
  return cc_layer_;
}

LayerThreadedAnimationDelegate* Layer::GetThreadedAnimationDelegate() {
  DCHECK(animator_);
  return animator_.get();
}

void Layer::CreateCcLayer() {
  if (type_ == LAYER_SOLID_COLOR) {
    solid_color_layer_ = cc::SolidColorLayer::Create();
    cc_layer_ = solid_color_layer_.get();
  } else if (type_ == LAYER_NINE_PATCH) {
    nine_patch_layer_ = cc::NinePatchLayer::Create();
    cc_layer_ = nine_patch_layer_.get();
  } else {
    content_layer_ = cc::PictureLayer::Create(this);
    cc_layer_ = content_layer_.get();
  }
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(true);
  cc_layer_->SetSafeOpaqueBackgroundColor(SK_ColorWHITE);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetHitTestable(IsHitTestableForCC());
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));
  RecomputePosition();
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(cc_layer_);
  gfx::Size size(bounds_.size());
  if (texture_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(
      static_cast<float>(size.width()) / frame_size_in_dip_.width(),
      static_cast<float>(size.height()) / frame_size_in_dip_.height());
    texture_layer_->SetUV(uv_top_left, uv_bottom_right);
  } else if (surface_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
  }
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

  for (auto* child : children_)
    child->SetCompositorForAnimatorsInTree(compositor);
}

void Layer::ResetCompositorForAnimatorsInTree(Compositor* compositor) {
  DCHECK(compositor);
  LayerAnimatorCollection* collection = compositor->layer_animator_collection();

  if (animator_) {
    animator_->DetachLayerAndTimeline(compositor);
    animator_->RemoveFromCollection(collection);
  }

  for (auto* child : children_)
    child->ResetCompositorForAnimatorsInTree(compositor);
}

void Layer::OnMirrorDestroyed(LayerMirror* mirror) {
  const auto it = std::find_if(mirrors_.begin(), mirrors_.end(),
      [mirror](const std::unique_ptr<LayerMirror>& mirror_ptr) {
        return mirror_ptr.get() == mirror;
      });

  DCHECK(it != mirrors_.end());
  mirrors_.erase(it);
}

void Layer::CreateSurfaceLayerIfNecessary() {
  if (surface_layer_)
    return;
  scoped_refptr<cc::SurfaceLayer> new_layer = cc::SurfaceLayer::Create();
  new_layer->SetSurfaceHitTestable(true);
  SwitchToLayer(new_layer);
  surface_layer_ = new_layer;
}

void Layer::MatchLayerSize(const Layer* layer) {
  gfx::Rect new_bounds = bounds_;
  gfx::Size new_size = layer->bounds().size();
  new_bounds.set_size(new_size);
  SetBounds(new_bounds);
}

void Layer::ResetSubtreeReflectedLayer() {
  if (!subtree_reflected_layer_)
    return;

  size_t result =
      subtree_reflected_layer_->subtree_reflecting_layers_.erase(this);
  DCHECK_EQ(1u, result);
  subtree_reflected_layer_ = nullptr;
}

}  // namespace ui
