// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_H_
#define UI_COMPOSITOR_LAYER_H_

#include <stddef.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/transform.h"

namespace cc {
class Layer;
class MirrorLayer;
class NinePatchLayer;
class SolidColorLayer;
class SurfaceLayer;
class TextureLayer;
}

namespace viz {
class CopyOutputRequest;
struct TransferableResource;
}

namespace ui {

class Compositor;
class LayerAnimator;
class LayerObserver;
class LayerOwner;
class LayerThreadedAnimationDelegate;

// Layer manages a texture, transform and a set of child Layers. Any View that
// has enabled layers ends up creating a Layer to manage the texture.
// A Layer can also be created without a texture, in which case it renders
// nothing and is simply used as a node in a hierarchy of layers.
// Coordinate system used in layers is DIP (Density Independent Pixel)
// coordinates unless explicitly mentioned as pixel coordinates.
//
// NOTE: Unlike Views, each Layer does *not* own its child Layers. If you
// delete a Layer and it has children, the parent of each child Layer is set to
// NULL, but the children are not deleted.
class COMPOSITOR_EXPORT Layer : public LayerAnimationDelegate,
                                public cc::ContentLayerClient,
                                public cc::TextureLayerClient {
 public:
  using ShapeRects = std::vector<gfx::Rect>;
  explicit Layer(LayerType type = LAYER_TEXTURED);
  ~Layer() override;

  // Note that only solid color and surface content is copied.
  std::unique_ptr<Layer> Clone() const;

  // Returns a new layer that mirrors this layer and is optionally synchronized
  // with the bounds thereof. Note that children are not mirrored, and that the
  // content is only mirrored if painted by a delegate or backed by a surface.
  // As the mirror layer rasterizes its contents separately, this might have
  // some negative impact on performance.
  std::unique_ptr<Layer> Mirror();

  // This method is relevant only if this layer is a mirror destination layer.
  // Sets whether this mirror layer's bounds are synchronized with the source
  // layer's bounds.
  void set_sync_bounds_with_source(bool sync_bounds) {
    sync_bounds_with_source_ = sync_bounds;
  }

  // This method is relevant only if this layer is a mirror destination layer.
  // Sets whether this mirror layer's visibility is synchronized with the source
  // layer's visibility.
  void set_sync_visibility_with_source(bool sync_visibility) {
    sync_visibility_with_source_ = sync_visibility;
  }

  // Sets up this layer to mirror output of |subtree_reflected_layer|, including
  // its entire hierarchy. |this| should be of type LAYER_SOLID_COLOR and should
  // not be a descendant of |subtree_reflected_layer|. This is achieved by using
  // cc::MirrorLayer which forces a render surface for |subtree_reflected_layer|
  // to be able to embed it. This might cause extra GPU memory bandwidth and/or
  // read/writes which can impact performance negatively.
  void SetShowReflectedLayerSubtree(Layer* subtree_reflected_layer);

  // Retrieves the Layer's compositor. The Layer will walk up its parent chain
  // to locate it. Returns NULL if the Layer is not attached to a compositor.
  Compositor* GetCompositor() {
    return const_cast<Compositor*>(
        const_cast<const Layer*>(this)->GetCompositor());
  }
  const Compositor* GetCompositor() const;

  // Called by the compositor when the Layer is set as its root Layer. This can
  // only ever be called on the root layer.
  void SetCompositor(Compositor* compositor,
                     scoped_refptr<cc::Layer> root_layer);
  void ResetCompositor();

  LayerDelegate* delegate() { return delegate_; }
  void set_delegate(LayerDelegate* delegate) { delegate_ = delegate; }

  LayerOwner* owner() { return owner_; }

  void AddObserver(LayerObserver* observer);
  void RemoveObserver(LayerObserver* observer);

  // Adds a new Layer to this Layer.
  void Add(Layer* child);

  // Removes a Layer from this Layer.
  void Remove(Layer* child);

  // Stacks |child| above all other children.
  void StackAtTop(Layer* child);

  // Stacks |child| directly above |other|.  Both must be children of this
  // layer.  Note that if |child| is initially stacked even higher, calling this
  // method will result in |child| being lowered in the stacking order.
  void StackAbove(Layer* child, Layer* other);

  // Stacks |child| below all other children.
  void StackAtBottom(Layer* child);

  // Stacks |child| directly below |other|.  Both must be children of this
  // layer.
  void StackBelow(Layer* child, Layer* other);

  // Returns the child Layers.
  const std::vector<Layer*>& children() const { return children_; }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  LayerType type() const { return type_; }

  // Returns true if this Layer contains |other| somewhere in its children.
  bool Contains(const Layer* other) const;

  // The layer's animator is responsible for causing automatic animations when
  // properties are set. It also manages a queue of pending animations and
  // handles blending of animations. The layer takes ownership of the animator.
  void SetAnimator(LayerAnimator* animator);

  // Returns the layer's animator. Creates a default animator of one has not
  // been set. Will not return NULL.
  LayerAnimator* GetAnimator();

  // The transform, relative to the parent.
  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return cc_layer_->transform(); }

  gfx::PointF position() const { return cc_layer_->position(); }

  // Return the target transform if animator is running, or the current
  // transform otherwise.
  gfx::Transform GetTargetTransform() const;

  // The bounds, relative to the parent.
  void SetBounds(const gfx::Rect& bounds);
  const gfx::Rect& bounds() const { return bounds_; }
  const gfx::Size& size() const { return bounds_.size(); }

  // The offset from our parent (stored in bounds.origin()) is an integer but we
  // may need to be at a fractional pixel offset to align properly on screen. If
  // this is not set, the layer will auto compute its sub pixel offset
  // information with respect to its parent layer.
  void SetSubpixelPositionOffset(const gfx::Vector2dF& offset);
  const gfx::Vector2dF GetSubpixelOffset() const;

  // Return the target bounds if animator is running, or the current bounds
  // otherwise.
  gfx::Rect GetTargetBounds() const;

  // Sets/gets whether or not drawing of child layers, including drawing in
  // this layer, should be clipped to the bounds of this layer.
  void SetMasksToBounds(bool masks_to_bounds);
  bool GetMasksToBounds() const;

  // Sets/gets the clip rect for the layer. |clip_rect| is in layer space and
  // relative to |this| layer. Prefer SetMasksToBounds() to set the clip to the
  // bounds of |this| layer. This clips the subtree rooted at |this| layer.
  void SetClipRect(const gfx::Rect& clip_rect);
  const gfx::Rect& clip_rect() const { return cc_layer_->clip_rect(); }

  // The opacity of the layer. The opacity is applied to each pixel of the
  // texture (resulting alpha = opacity * alpha).
  float opacity() const;
  void SetOpacity(float opacity);

  // Returns the actual opacity, which the opacity of this layer multipled by
  // the combined opacity of the parent.
  float GetCombinedOpacity() const;

  // Returns the target color temperature if animator is running, or the current
  // temperature otherwise.
  float GetTargetTemperature() const;

  // Blur pixels by 3 * this amount in anything below the layer and visible
  // through the layer.
  float background_blur() const { return background_blur_sigma_; }
  void SetBackgroundBlur(float blur_sigma);

  // Blur pixels of this layer by 3 * this amount.
  float layer_blur() const { return layer_blur_sigma_; }
  void SetLayerBlur(float blur_sigma);

  // Saturate all pixels of this layer by this amount.
  // This effect will get "combined" with the inverted,
  // brightness and grayscale setting.
  float layer_saturation() const { return layer_saturation_; }
  void SetLayerSaturation(float saturation);

  // Change the brightness of all pixels from this layer by this amount.
  // This effect will get "combined" with the inverted, saturate
  // and grayscale setting.
  float layer_brightness() const { return layer_brightness_; }
  void SetLayerBrightness(float brightness);

  // Return the target brightness if animator is running, or the current
  // brightness otherwise.
  float GetTargetBrightness() const;

  // Change the grayscale of all pixels from this layer by this amount.
  // This effect will get "combined" with the inverted, saturate
  // and brightness setting.
  float layer_grayscale() const { return layer_grayscale_; }
  void SetLayerGrayscale(float grayscale);

  // Return the target grayscale if animator is running, or the current
  // grayscale otherwise.
  float GetTargetGrayscale() const;

  // Zoom the background by a factor of |zoom|. The effect is blended along the
  // edge across |inset| pixels.
  void SetBackgroundZoom(float zoom, int inset);

  // Set the shape of this layer.
  const ShapeRects* alpha_shape() const { return alpha_shape_.get(); }
  void SetAlphaShape(std::unique_ptr<ShapeRects> shape);

  // Invert the layer.
  bool layer_inverted() const { return layer_inverted_; }
  void SetLayerInverted(bool inverted);

  // Return the target opacity if animator is running, or the current opacity
  // otherwise.
  float GetTargetOpacity() const;

  // Set a layer mask for a layer.
  // Note the provided layer mask can neither have a layer mask itself nor can
  // it have any children. The ownership of |layer_mask| will not be
  // transferred with this call.
  // Furthermore: A mask layer can only be set to one layer.
  void SetMaskLayer(Layer* layer_mask);
  Layer* layer_mask_layer() { return layer_mask_; }
  const Layer* layer_mask_layer() const { return layer_mask_; }

  // Sets the visibility of the Layer. A Layer may be visible but not drawn.
  // This happens if any ancestor of a Layer is not visible.
  // Any changes made to this in the source layer will override the visibility
  // of its mirror layer.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Returns the target visibility if the animator is running. Otherwise, it
  // returns the current visibility.
  bool GetTargetVisibility() const;

  // Returns true if this Layer is drawn. A Layer is drawn only if all ancestors
  // are visible.
  bool IsDrawn() const;

  // If set to true, this layer can receive hit test events, this property does
  // not affect the layer's descendants.
  void SetAcceptEvents(bool accept_events);
  bool accept_events() const { return accept_events_; }

  // Sets a rounded corner clip on the layer.
  void SetRoundedCornerRadius(const gfx::RoundedCornersF& corner_radii);
  const gfx::RoundedCornersF& rounded_corner_radii() const {
    return cc_layer_->corner_radii();
  }

  // If set to true, this layer would not trigger a render surface (if possible)
  // due to having a rounded corner resulting in a better performance at the
  // cost of maybe having some blending artifacts.
  void SetIsFastRoundedCorner(bool enable);
  bool is_fast_rounded_corner() const {
    return cc_layer_->is_fast_rounded_corner();
  }

  // Converts a point from the coordinates of |source| to the coordinates of
  // |target|. Necessarily, |source| and |target| must inhabit the same Layer
  // tree.
  static void ConvertPointToLayer(const Layer* source,
                                  const Layer* target,
                                  gfx::PointF* point);

  // Converts a transform to be relative to the given |ancestor|. Returns
  // whether success (that is, whether the given ancestor was really an
  // ancestor of this layer).
  bool GetTargetTransformRelativeTo(const Layer* ancestor,
                                    gfx::Transform* transform) const;

  // Note: Setting a layer non-opaque has significant performance impact,
  // especially on low-end Chrome OS devices. Please ensure you are not
  // adding unnecessary overdraw. When in doubt, talk to the graphics team.
  void SetFillsBoundsOpaquely(bool fills_bounds_opaquely);
  bool fills_bounds_opaquely() const { return fills_bounds_opaquely_; }

  // Set to true if this layer always paints completely within its bounds. If so
  // we can omit an unnecessary clear, even if the layer is transparent.
  void SetFillsBoundsCompletely(bool fills_bounds_completely);

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  // Set new TransferableResource for this layer. This method only supports
  // a gpu-backed |resource|.
  void SetTransferableResource(
      const viz::TransferableResource& resource,
      std::unique_ptr<viz::SingleReleaseCallback> release_callback,
      gfx::Size texture_size_in_dip);
  void SetTextureSize(gfx::Size texture_size_in_dip);
  void SetTextureFlipped(bool flipped);
  bool TextureFlipped() const;

  // TODO(fsamuel): Update this comment.
  // Begins showing content from a surface with a particular ID.
  void SetShowSurface(const viz::SurfaceId& surface_id,
                      const gfx::Size& frame_size_in_dip,
                      SkColor default_background_color,
                      const cc::DeadlinePolicy& deadline_policy,
                      bool stretch_content_to_fill_bounds);

  // In the event that the primary surface is not yet available in the
  // display compositor, the fallback surface will be used.
  void SetOldestAcceptableFallback(const viz::SurfaceId& surface_id);

  // Begins mirroring content from a reflected surface, e.g. a software mirrored
  // display. |surface_id| should be the root surface for a display.
  void SetShowReflectedSurface(const viz::SurfaceId& surface_id,
                               const gfx::Size& frame_size_in_pixels);

  // Returns the primary SurfaceId set by SetShowSurface.
  const viz::SurfaceId* GetSurfaceId() const;

  // Returns the fallback SurfaceId set by SetOldestAcceptableFallback.
  const viz::SurfaceId* GetOldestAcceptableFallback() const;

  bool has_external_content() const {
    return texture_layer_.get() || surface_layer_.get();
  }

  // Show a solid color instead of delegated or surface contents.
  void SetShowSolidColorContent();

  // Reorder the children to have all children inside |new_leading_children| to
  // be at the front of the children vector, and the remaining children will
  // stay in their relative order. |this| must be a parent of all the Layer*
  // inside |new_leading_children|.
  void StackChildrenAtBottom(const std::vector<Layer*>& new_leading_children);

  // Sets the layer's fill color.  May only be called for LAYER_SOLID_COLOR.
  void SetColor(SkColor color);
  SkColor GetTargetColor() const;
  SkColor background_color() const;

  // Updates the nine patch layer's image, aperture and border. May only be
  // called for LAYER_NINE_PATCH.
  void UpdateNinePatchLayerImage(const gfx::ImageSkia& image);
  void UpdateNinePatchLayerAperture(const gfx::Rect& aperture_in_dip);
  void UpdateNinePatchLayerBorder(const gfx::Rect& border);
  // Updates the area completely occluded by another layer, this can be an
  // empty rectangle if nothing is occluded.
  void UpdateNinePatchOcclusion(const gfx::Rect& occlusion);

  // Adds |invalid_rect| to the Layer's pending invalid rect and calls
  // ScheduleDraw(). Returns false if the paint request is ignored.
  bool SchedulePaint(const gfx::Rect& invalid_rect);

  // Schedules a redraw of the layer tree at the compositor.
  // Note that this _does not_ invalidate any region of this layer; use
  // SchedulePaint() for that.
  void ScheduleDraw();

  // Uses damaged rectangles recorded in |damaged_region_| to invalidate the
  // |cc_layer_|.
  void SendDamagedRects();

  const cc::Region& damaged_region() const { return damaged_region_; }

  void CompleteAllAnimations();

  // Suppresses painting the content by disconnecting |delegate_|.
  void SuppressPaint();

  // Notifies the layer that the device scale factor has changed.
  void OnDeviceScaleFactorChanged(float device_scale_factor);

  // Requets a copy of the layer's output as a texture or bitmap.
  void RequestCopyOfOutput(std::unique_ptr<viz::CopyOutputRequest> request);

  // Invoked when scrolling performed by the cc::InputHandler is committed. This
  // will only occur if the Layer has set scroll container bounds.
  void SetDidScrollCallback(
      base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                   const cc::ElementId&)> callback);

  cc::ElementId element_id() const { return cc_layer_->element_id(); }

  // Marks this layer as scrollable inside the provided bounds. This size only
  // affects scrolling so if clipping is desired, a separate clipping layer
  // needs to be created.
  void SetScrollable(const gfx::Size& container_bounds);

  // Gets and sets the current scroll offset of the layer.
  gfx::ScrollOffset CurrentScrollOffset() const;
  void SetScrollOffset(const gfx::ScrollOffset& offset);

  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() override;
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      ContentLayerClient::PaintingControlSetting painting_control) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

  cc::MirrorLayer* mirror_layer_for_testing() { return mirror_layer_.get(); }
  cc::Layer* cc_layer_for_testing() { return cc_layer_; }
  const cc::Layer* cc_layer_for_testing() const { return cc_layer_; }

  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registar,
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override;

  float device_scale_factor() const { return device_scale_factor_; }

  // Triggers a call to SwitchToLayer.
  void SwitchCCLayerForTest();

  const cc::Region& damaged_region_for_testing() const {
    return damaged_region_;
  }

  const gfx::Size& frame_size_in_dip_for_testing() const {
    return frame_size_in_dip_;
  }

  // Force use of and cache render surface. Note that this also disables
  // occlusion culling in favor of efficient caching. This should
  // only be used when paying the cost of creating a render
  // surface even if layer is invisible is not a problem.
  void AddCacheRenderSurfaceRequest();
  void RemoveCacheRenderSurfaceRequest();

  // Request deferring painting for layer.
  void AddDeferredPaintRequest();
  void RemoveDeferredPaintRequest();

  // |quality| is used as a multiplier to scale the temporary surface
  // that might be created by the compositor to apply the backdrop filters.
  // The filter will be applied on a surface |quality|^2 times the area of the
  // original background.
  // |quality| lower than one will decrease memory usage and increase
  // performance.
  void SetBackdropFilterQuality(const float quality);

  bool IsPaintDeferredForTesting() const { return deferred_paint_requests_; }

  // Request trilinear filtering for layer.
  void AddTrilinearFilteringRequest();
  void RemoveTrilinearFilteringRequest();

  // The back link from the mask layer to it's associated masked layer.
  // We keep this reference for the case that if the mask layer gets deleted
  // while attached to the main layer before the main layer is deleted.
  const Layer* layer_mask_back_link() const { return layer_mask_back_link_; }

  // If |surface_layer_| exists, return whether the contents should stretch to
  // fill the bounds of |this|. Defaults to false.
  bool StretchContentToFillBounds() const;

  // If |surface_layer_| exists, update the size. The updated size is necessary
  // for proper scaling if the embedder is resized and the |surface_layer_| is
  // set to stretch to fill bounds.
  void SetSurfaceSize(gfx::Size surface_size_in_dip);

  bool ContainsMirrorForTest(Layer* mirror) const;

 private:
  friend class LayerOwner;
  class LayerMirror;
  class SubpixelPositionOffsetCache;

  void CollectAnimators(std::vector<scoped_refptr<LayerAnimator> >* animators);

  // Stacks |child| above or below |other|.  Helper method for StackAbove() and
  // StackBelow().
  void StackRelativeTo(Layer* child, Layer* other, bool above);

  bool ConvertPointForAncestor(const Layer* ancestor, gfx::PointF* point) const;
  bool ConvertPointFromAncestor(const Layer* ancestor,
                                gfx::PointF* point) const;

  // Implementation of LayerAnimatorDelegate
  void SetBoundsFromAnimation(const gfx::Rect& bounds,
                              PropertyChangeReason reason) override;
  void SetTransformFromAnimation(const gfx::Transform& transform,
                                 PropertyChangeReason reason) override;
  void SetOpacityFromAnimation(float opacity,
                               PropertyChangeReason reason) override;
  void SetVisibilityFromAnimation(bool visibility,
                                  PropertyChangeReason reason) override;
  void SetBrightnessFromAnimation(float brightness,
                                  PropertyChangeReason reason) override;
  void SetGrayscaleFromAnimation(float grayscale,
                                 PropertyChangeReason reason) override;
  void SetColorFromAnimation(SkColor color,
                             PropertyChangeReason reason) override;
  void SetClipRectFromAnimation(const gfx::Rect& clip_rect,
                                PropertyChangeReason reason) override;
  void SetRoundedCornersFromAnimation(
      const gfx::RoundedCornersF& rounded_corners,
      PropertyChangeReason reason) override;
  void ScheduleDrawForAnimation() override;
  const gfx::Rect& GetBoundsForAnimation() const override;
  gfx::Transform GetTransformForAnimation() const override;
  float GetOpacityForAnimation() const override;
  bool GetVisibilityForAnimation() const override;
  float GetBrightnessForAnimation() const override;
  float GetGrayscaleForAnimation() const override;
  SkColor GetColorForAnimation() const override;
  gfx::Rect GetClipRectForAnimation() const override;
  gfx::RoundedCornersF GetRoundedCornersForAnimation() const override;
  float GetDeviceScaleFactor() const override;
  ui::Layer* GetLayer() override;
  cc::Layer* GetCcLayer() const override;
  LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() override;
  LayerAnimatorCollection* GetLayerAnimatorCollection() override;
  int GetFrameNumber() const override;
  float GetRefreshRate() const override;

  // Creates a corresponding composited layer for |type_|.
  void CreateCcLayer();

  // Recomputes and sets to |cc_layer_|.
  void RecomputeDrawsContentAndUVRect();
  void RecomputePosition();

  // Set all filters which got applied to the layer.
  void SetLayerFilters();

  // Set all filters which got applied to the layer background.
  void SetLayerBackgroundFilters();

  // Cleanup |cc_layer_| and replaces it with |new_layer|.
  void SwitchToLayer(scoped_refptr<cc::Layer> new_layer);

  void SetCompositorForAnimatorsInTree(Compositor* compositor);
  void ResetCompositorForAnimatorsInTree(Compositor* compositor);

  void OnMirrorDestroyed(LayerMirror* mirror);

  void CreateSurfaceLayerIfNecessary();

  // Changes the size of |this| to match that of |layer|.
  void MatchLayerSize(const Layer* layer);

  // Resets |subtree_reflected_layer_| and updates the reflected layer's
  // |subtree_reflecting_layers_| list accordingly.
  void ResetSubtreeReflectedLayer();

  bool IsHitTestableForCC() const { return visible_ && accept_events_; }

  const LayerType type_;

  Compositor* compositor_;

  Layer* parent_;

  // This layer's children, in bottom-to-top stacking order.
  std::vector<Layer*> children_;

  std::vector<std::unique_ptr<LayerMirror>> mirrors_;

  // The layer being reflected with its subtree by this one, if any.
  Layer* subtree_reflected_layer_ = nullptr;

  // List of layers reflecting this layer and its subtree, if any.
  base::flat_set<Layer*> subtree_reflecting_layers_;

  // If true, and this is a destination mirror layer, changes to the bounds of
  // the source layer are propagated to this mirror layer.
  bool sync_bounds_with_source_ = false;

  // If true, and this is a destination mirror layer, changes in the source
  // layer's visibility are propagated to this mirror layer.
  bool sync_visibility_with_source_ = true;

  gfx::Rect bounds_;

  std::unique_ptr<SubpixelPositionOffsetCache> subpixel_position_offset_;

  // Visibility of this layer. See SetVisible/IsDrawn for more details.
  bool visible_;

  // Whether or not the layer wants to receive hit testing events. When set to
  // false, the layer will be ignored in hit testing even if it is visible. It
  // does not affect the layer's descendants.
  bool accept_events_ = true;

  // See SetFillsBoundsOpaquely(). Defaults to true.
  bool fills_bounds_opaquely_;

  bool fills_bounds_completely_;

  // Union of damaged rects, in layer space, that SetNeedsDisplayRect should
  // be called on.
  cc::Region damaged_region_;

  // Union of damaged rects, in layer space, to be used when compositor is ready
  // to paint the content.
  cc::Region paint_region_;

  float background_blur_sigma_;

  // Several variables which will change the visible representation of
  // the layer.
  float layer_saturation_;
  float layer_brightness_;
  float layer_grayscale_;
  bool layer_inverted_;
  float layer_blur_sigma_;

  // The associated mask layer with this layer.
  Layer* layer_mask_;
  // The back link from the mask layer to it's associated masked layer.
  // We keep this reference for the case that if the mask layer gets deleted
  // while attached to the main layer before the main layer is deleted.
  Layer* layer_mask_back_link_;

  // The zoom factor to scale the layer by.  Zooming is disabled when this is
  // set to 1.
  float zoom_;

  // Width of the border in pixels, where the scaling is blended.
  int zoom_inset_;

  // Shape of the window.
  std::unique_ptr<ShapeRects> alpha_shape_;

  std::string name_;

  LayerDelegate* delegate_;

  base::ObserverList<LayerObserver>::Unchecked observer_list_;

  LayerOwner* owner_;

  scoped_refptr<LayerAnimator> animator_;

  // Ownership of the layer is held through one of the strongly typed layer
  // pointers, depending on which sort of layer this is.
  scoped_refptr<cc::PictureLayer> content_layer_;
  scoped_refptr<cc::MirrorLayer> mirror_layer_;
  scoped_refptr<cc::NinePatchLayer> nine_patch_layer_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  cc::Layer* cc_layer_;

  // A cached copy of |Compositor::device_scale_factor()|.
  float device_scale_factor_;

  // A cached copy of the nine patch layer's image and aperture.
  // These are required for device scale factor change.
  gfx::ImageSkia nine_patch_layer_image_;
  gfx::Rect nine_patch_layer_aperture_;

  // The external resource used by texture_layer_.
  viz::TransferableResource transfer_resource_;

  // The callback to release the mailbox. This is only set after
  // SetTransferableResource() is called, before we give it to the TextureLayer.
  std::unique_ptr<viz::SingleReleaseCallback> transfer_release_callback_;

  // The size of the frame or texture in DIP, set when SetShowDelegatedContent
  // or SetTransferableResource() was called.
  gfx::Size frame_size_in_dip_;

  // The counter to maintain how many cache render surface requests we have. If
  // the value > 0, means we need to cache the render surface. If the value
  // == 0, means we should not cache the render surface.
  unsigned cache_render_surface_requests_;

  // The counter to maintain how many deferred paint requests we have. If the
  // value > 0, means we need to defer painting the layer. If the value == 0,
  // means we should paint the layer.
  unsigned deferred_paint_requests_;

  float backdrop_filter_quality_;

  // The counter to maintain how many trilinear filtering requests we have. If
  // the value > 0, means we need to perform trilinear filtering on the layer.
  // If the value == 0, means we should not perform trilinear filtering on the
  // layer.
  unsigned trilinear_filtering_request_;

  base::WeakPtrFactory<Layer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_H_
