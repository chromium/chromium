// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_H_
#define UI_COMPOSITOR_LAYER_H_

#include <stddef.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/paint/filter_operation.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace cc {
class Layer;
class MirrorLayer;
class NinePatchLayer;
class SolidColorLayer;
class SurfaceLayer;
class TextureLayer;
}

namespace gfx {
class RoundedCornersF;
class Transform;
class LinearGradient;
}  // namespace gfx

namespace viz {
class CopyOutputRequest;
struct TransferableResource;
}

namespace ui {

class Compositor;
class LayerAnimator;
class LayerDelegate;
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
  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;
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

  // This method is relevant only if this layer is a mirror destination layer.
  // Sets whether this mirror layer's rounded corners are synchronized with the
  // source layer's rounded corners.
  void set_sync_rounded_corners_with_source(bool sync_rounded_corners) {
    sync_rounded_corners_with_source_ = sync_rounded_corners;
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

  // These should be private, but they're used by HideHelper, which needs to
  // do part but not all of what SetCompositor/ResetCompositor do.
  void SetCompositorForAnimatorsInTree(Compositor* compositor);
  void ResetCompositorForAnimatorsInTree(Compositor* compositor);

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
  const std::vector<raw_ptr<Layer, VectorExperimental>>& children() const {
    return children_;
  }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  LayerType type() const { return type_; }

  // Returns true if this Layer contains |other| somewhere in its children.
  bool Contains(const Layer* other) const;

  // The layer's animator is responsible for causing automatic animations when
  // properties are set. It also manages a queue of pending animations and
  // handles blending of animations. The layer takes ownership of the animator.
  void SetAnimator(scoped_refptr<LayerAnimator> animator);

  // Returns the layer's animator. Creates a default animator of one has not
  // been set. Will not return NULL.
  LayerAnimator* GetAnimator();

  // Sets the given |subtree_id| on the cc::Layer associated with this, so that
  // the layer subtree rooted here can be uniquely identified by a
  // FrameSinkVideoCapturer. The existence of a valid SubtreeCaptureId on this
  // layer will force it to be drawn into a separate CompositorRenderPass.
  // Setting a non-valid (i.e. default-constructed SubtreeCaptureId) will clear
  // this property.
  // It is not allowed to change this ID from a valid ID to another valid ID,
  // since a client might already using the existing valid ID to make this layer
  // subtree identifiable by a capturer.
  //
  // Note that this is useful when it's desired to video record a layer subtree
  // of a non-root layer using a FrameSinkVideoCapturer, since non-root layers
  // are usually not drawn into their own CompositorRenderPass, while the ui
  // compositor's root layer always is.
  void SetSubtreeCaptureId(viz::SubtreeCaptureId subtree_id);
  viz::SubtreeCaptureId GetSubtreeCaptureId() const;

  // The transform, relative to the parent.
  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return cc_layer_->transform(); }

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
  gfx::Rect GetTargetClipRect() const;
  void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect clip_rect() const { return cc_layer_->clip_rect(); }

  // The opacity of the layer. The opacity is applied to each pixel of the
  // texture (resulting alpha = opacity * alpha).
  float opacity() const;
  void SetOpacity(float opacity);

  // Returns the actual opacity, which the opacity of this layer multipled by
  // the combined opacity of the parent.
  float GetCombinedOpacity() const;

  // Blur pixels by 3 * this amount in anything below the layer and visible
  // through the layer.
  float background_blur() const { return background_blur_sigma_; }
  void SetBackgroundBlur(float blur_sigma);

  // Blur pixels of this layer by 3 * this amount.
  float layer_blur() const { return layer_blur_sigma_; }
  void SetLayerBlur(float blur_sigma);

  // Saturate all pixels of this layer by this amount.
  // The effect of invert, brightness, greyscale, saturate, sepia, and
  // custom color matrix settings are combined.
  float layer_saturation() const { return layer_saturation_; }
  void SetLayerSaturation(float saturation);

  // Change the brightness of all pixels from this layer by this amount.
  // The effect of invert, brightness, greyscale, saturate, sepia, and
  // custom color matrix settings are combined.
  float layer_brightness() const { return layer_brightness_; }
  void SetLayerBrightness(float brightness);

  // Return the target brightness if animator is running, or the current
  // brightness otherwise.
  float GetTargetBrightness() const;

  // Change the grayscale of all pixels from this layer by this amount.
  // The effect of invert, brightness, greyscale, saturate, sepia, and
  // custom color matrix settings are combined.
  float layer_grayscale() const { return layer_grayscale_; }
  void SetLayerGrayscale(float grayscale);

  // Return the target grayscale if animator is running, or the current
  // grayscale otherwise.
  float GetTargetGrayscale() const;

  // Applies a sepia filter to all pixels from this layer by this amount.
  // Amounts may be between 0 (no change) and 1 (completely sepia).
  // The effect of invert, brightness, greyscale, saturate, sepia, and
  // custom color matrix settings are combined.
  void SetLayerSepia(float amount);
  float layer_sepia() const { return layer_sepia_; }

  // Applies a hue rotation by this amount.
  // Amounts may be between 0 (no change) and 359 (completely rotated).
  // Amounts over 359 will wrap back to 0.
  // The effect of invert, brightness, greyscale, saturate, sepia, and
  // custom color matrix settings are combined.
  void SetLayerHueRotation(float amount);
  float layer_hue_rotation() const { return layer_hue_rotation_; }

  // Applies a custom color filter to all pixels from this layer with the given
  // matrix. This effect will get "combined" with the invert, saturate and
  // brightness setting.
  void SetLayerCustomColorMatrix(const cc::FilterOperation::Matrix& matrix);
  const cc::FilterOperation::Matrix* GetLayerCustomColorMatrix() const;
  bool LayerHasCustomColorMatrix() const;
  // If a custom layer color matrix was set, this clears it.
  void ClearLayerCustomColorMatrix();

  // Applies an offset to the Layer, after all other backdrop and filter effects
  // other than clipping. This offset is not reflected in the layer bounds.
  void SetLayerOffset(const gfx::Point& offset);
  const gfx::Point& layer_offset() const { return layer_offset_; }

  // Zoom the background by a factor of |zoom|. The effect is blended along the
  // edge across |inset| pixels.
  // NOTE: Background zoom does not currently work with software compositing,
  // see crbug.com/1451898. Usage should generally be limited to ash chrome,
  // which does not rely on software compositing. Elsewhere, background zoom can
  // still be set, but it will have no effect when software compositing is used
  // (e.g. as a fallback when the GPU process has crashed too many times).
  void SetBackgroundZoom(float zoom, int inset);

  // Set the rounded clip bounds of the backdrop filter effect, relative to
  // this Layer's coordinate space. Backdrop effects are only visible and can
  // only sample from the intersection of the Layer's bounds and any set
  // backdrop filter bounds.
  void SetBackdropFilterBounds(const gfx::RRectF& backdrop_filter_bounds);
  void ClearBackdropFilterBounds();

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

  // Sets the visibility of the Layer. A Layer itself may be visible but not
  // fully visible in the layer tree.  This happens if any ancestor of a
  // Layer is not visible.  Any changes made to this in the source layer will
  // override the visibility of its mirror layer.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Returns the target visibility if the animator is running. Otherwise, it
  // returns the current visibility.
  bool GetTargetVisibility() const;

  // Returns true if this Layer is visible. A Layer is visible only if
  // all ancestors are visible.
  bool IsVisible() const;

  // If set to true, this layer can receive hit test events, this property does
  // not affect the layer's descendants.
  void SetAcceptEvents(bool accept_events);
  bool accept_events() const { return accept_events_; }

  // Gets/sets a rounded corner clip on the layer.
  gfx::RoundedCornersF GetTargetRoundedCornerRadius() const;
  void SetRoundedCornerRadius(const gfx::RoundedCornersF& corner_radii);
  const gfx::RoundedCornersF& rounded_corner_radii() const {
    return cc_layer_->corner_radii();
  }

  // Gets/sets a gradient mask that is applied to the clip bounds on the layer
  void SetGradientMask(const gfx::LinearGradient& linear_gradient);
  const gfx::LinearGradient& gradient_mask() const {
    return cc_layer_->gradient_mask();
  }
  bool HasGradientMask() { return !cc_layer_->gradient_mask().IsEmpty(); }

  // If set to true, this layer would not trigger a render surface (if possible)
  // due to having a rounded corner resulting in a better performance at the
  // cost of maybe having some blending artifacts.
  void SetIsFastRoundedCorner(bool enable);
  bool is_fast_rounded_corner() const {
    return cc_layer_->is_fast_rounded_corner();
  }

  // Converts a point from the coordinates of |source| to the coordinates of
  // |target|. Necessarily, |source| and |target| must inhabit the same Layer
  // tree. If `use_target_transform` is true, the target transform is used in
  // coordinate conversions; otherwise, the current transform is used. If there
  // is no animation ongoing, the target transform is the same as the current
  // transform.
  static void ConvertPointToLayer(const Layer* source,
                                  const Layer* target,
                                  bool use_target_transform,
                                  gfx::PointF* point);

  // Calculates the relative transform. See the comment of
  // `GetTransformRelativeToImpl()` for further details.
  bool GetTransformRelativeTo(const Layer* ancestor,
                              gfx::Transform* transform) const;
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
  void SetName(const std::string& name);

  // Set new TransferableResource for this layer. This method only supports
  // a gpu-backed |resource| which is assumed to have top-left origin. Clients
  // should call SetTextureFlipped(true) for bottom-left origin resources.
  void SetTransferableResource(const viz::TransferableResource& resource,
                               viz::ReleaseCallback release_callback,
                               gfx::Size texture_size_in_dip);
  void SetTextureSize(gfx::Size texture_size_in_dip);
  void SetTextureFlipped(bool flipped);
  bool TextureFlipped() const;

  // Begins showing content from a surface with a particular ID.
  // TODO(crbug.com/40285157): with surface sync, size shouldn't rely on
  // `frame_size_in_dip` anymore, so this method can be deleted, and
  // surface_size uses `bounds_` instead.
  void SetShowSurface(const viz::SurfaceId& surface_id,
                      const gfx::Size& frame_size_in_dip,
                      SkColor default_background_color,
                      const cc::DeadlinePolicy& deadline_policy,
                      bool stretch_content_to_fill_bounds);

  // Updates the surface to a particular ID without changing size.
  void SetShowSurface(const viz::SurfaceId& surface_id,
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

  // Requests a copy of the layer's output as a texture or bitmap. If the
  // request does not have the result task runner, this will be set to
  // the compositor's task runner, which means the layer must be added to
  // compositor before requesting.
  void RequestCopyOfOutput(std::unique_ptr<viz::CopyOutputRequest> request);

  // Invoked when scrolling performed by the cc::InputHandler is committed. This
  // will only occur if the Layer has set scroll container bounds.
  void SetDidScrollCallback(
      base::RepeatingCallback<void(const gfx::PointF&, const cc::ElementId&)>
          callback);

  cc::ElementId element_id() const { return cc_layer_->element_id(); }

  // Marks this layer as scrollable inside the provided bounds. This size only
  // affects scrolling so if clipping is desired, a separate clipping layer
  // needs to be created.
  void SetScrollable(const gfx::Size& container_bounds);

  // Gets and sets the current scroll offset of the layer.
  gfx::PointF CurrentScrollOffset() const;
  void SetScrollOffset(const gfx::PointF& offset);

  // ContentLayerClient implementation.
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

  cc::MirrorLayer* mirror_layer_for_testing() { return mirror_layer_.get(); }
  cc::Layer* cc_layer_for_testing() { return cc_layer_; }
  const cc::Layer* cc_layer_for_testing() const { return cc_layer_; }

  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registar,
      viz::TransferableResource* resource,
      viz::ReleaseCallback* release_callback) override;

  float device_scale_factor() const { return device_scale_factor_; }

  // Triggers a call to SwitchToLayer.
  bool SwitchCCLayerForTest();

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

  base::WeakPtr<Layer> AsWeakPtr();

  bool ContainsMirrorForTest(Layer* mirror) const;

  void SetCompositorForTesting(Compositor* compositor) {
    compositor_ = compositor;
  }

  void set_no_mutation(bool no_mutation) { no_mutation_ = no_mutation; }

 private:
  // TODO(crbug.com/40786876): temporary while tracking down crash.
  friend class Compositor;
  friend class LayerOwner;
  class LayerMirror;
  class SubpixelPositionOffsetCache;

  void CollectAnimators(std::vector<scoped_refptr<LayerAnimator>>* animators);

  // Stacks |child| above or below |other|.  Helper method for StackAbove() and
  // StackBelow().
  void StackRelativeTo(Layer* child, Layer* other, bool above);

  // If `use_target_transform` is true, coordinate conversions use the target
  // transform. The target transform is the end value of a transform animation.
  // If `use_target_transform` is false, coordinate conversions use the current
  // transform. If there is no animation ongoing, the target transform is the
  // same as the current transform.
  bool ConvertPointForAncestor(const Layer* ancestor,
                               bool use_target_transform,
                               gfx::PointF* point) const;
  bool ConvertPointFromAncestor(const Layer* ancestor,
                                bool use_target_transform,
                                gfx::PointF* point) const;

  // Implementation of LayerAnimatorDelegate
  void SetBoundsFromAnimation(const gfx::Rect& bounds,
                              PropertyChangeReason reason) override;
  void SetTransformFromAnimation(const gfx::Transform& new_transform,
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
  void SetGradientMaskFromAnimation(const gfx::LinearGradient& gradient_mask,
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
  const gfx::LinearGradient& GetGradientMaskForAnimation() const override;
  float GetDeviceScaleFactor() const override;
  Layer* GetLayer() override;
  cc::Layer* GetCcLayer() const override;
  LayerThreadedAnimationDelegate* GetThreadedAnimationDelegate() override;
  LayerAnimatorCollection* GetLayerAnimatorCollection() override;
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

  // Cleanup |cc_layer_| and replaces it with |new_layer|. When stopping
  // animations handled by old cc layer before the switch, |this| could be
  // released by an animation observer. Returns false when it happens and
  // callers should take cautions as well. Otherwise returns true.
  [[nodiscard]] bool SwitchToLayer(scoped_refptr<cc::Layer> new_layer);

  void OnMirrorDestroyed(LayerMirror* mirror);

  void CreateSurfaceLayerIfNecessary();

  // Changes the size of |this| to match that of |layer|.
  void MatchLayerSize(const Layer* layer);

  // Resets |subtree_reflected_layer_| and updates the reflected layer's
  // |subtree_reflecting_layers_| list accordingly.
  void ResetSubtreeReflectedLayer();

  bool IsHitTestableForCC() const { return visible_ && accept_events_; }

  // Gets a flattened WeakPtr list of all layers and layer masks in the tree
  // rooted from |this|.
  void GetFlattenedWeakList(std::vector<base::WeakPtr<Layer>>* flattened_list);

  // Same as SetFillsBoundsOpaque but with a reason how it's changed.
  void SetFillsBoundsOpaquelyWithReason(bool fills_bounds_opaquely,
                                        PropertyChangeReason reason);

  // Converts a transform to be relative to the given |ancestor|. If
  // `is_target_transform` is true, the target transform is used in the
  // coordinate conversions; otherwise, the current transform is used. Returns
  // whether success (that is, whether the given ancestor was really an ancestor
  // of this layer).
  bool GetTransformRelativeToImpl(const Layer* ancestor,
                                  bool is_target_transform,
                                  gfx::Transform* transform) const;

  const LayerType type_;

  raw_ptr<Compositor> compositor_;

  raw_ptr<Layer> parent_;

  // This layer's children, in bottom-to-top stacking order.
  std::vector<raw_ptr<Layer, VectorExperimental>> children_;

  std::vector<std::unique_ptr<LayerMirror>> mirrors_;

  // The layer being reflected with its subtree by this one, if any.
  raw_ptr<Layer> subtree_reflected_layer_ = nullptr;

  // List of layers reflecting this layer and its subtree, if any.
  base::flat_set<raw_ptr<Layer, CtnExperimental>> subtree_reflecting_layers_;

  // If true, and this is a destination mirror layer, changes to the bounds of
  // the source layer are propagated to this mirror layer.
  bool sync_bounds_with_source_ = false;

  // If true, and this is a destination mirror layer, changes in the source
  // layer's visibility are propagated to this mirror layer.
  bool sync_visibility_with_source_ = true;

  // If true, and this is a destination mirror layer, changes in the rounded
  // corners of the source layer are propagated to this mirror layer.
  bool sync_rounded_corners_with_source_ = true;

  gfx::Rect bounds_;

  std::unique_ptr<SubpixelPositionOffsetCache> subpixel_position_offset_;

  // Visibility of this layer. See SetVisible/IsVisible for more details.
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
  float layer_sepia_;
  float layer_hue_rotation_;
  std::unique_ptr<cc::FilterOperation::Matrix> layer_custom_color_matrix_;
  // Offset to apply when drawing pixels for the layer.
  gfx::Point layer_offset_;

  // The associated mask layer with this layer.
  raw_ptr<Layer> layer_mask_;
  // The back link from the mask layer to it's associated masked layer.
  // We keep this reference for the case that if the mask layer gets deleted
  // while attached to the main layer before the main layer is deleted.
  raw_ptr<Layer> layer_mask_back_link_;

  // The zoom factor to scale the layer by.  Zooming is disabled when this is
  // set to 1.
  float zoom_;

  // Width of the border in pixels, where the scaling is blended.
  int zoom_inset_;

  // Shape of the window.
  std::unique_ptr<ShapeRects> alpha_shape_;

  std::string name_;

  raw_ptr<LayerDelegate, DanglingUntriaged> delegate_ = nullptr;

  base::ObserverList<LayerObserver>::UncheckedAndDanglingUntriaged
      observer_list_;

  raw_ptr<LayerOwner> owner_;

  scoped_refptr<LayerAnimator> animator_;

  // Ownership of the layer is held through one of the strongly typed layer
  // pointers, depending on which sort of layer this is.
  scoped_refptr<cc::PictureLayer> content_layer_;
  scoped_refptr<cc::MirrorLayer> mirror_layer_;
  scoped_refptr<cc::NinePatchLayer> nine_patch_layer_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  raw_ptr<cc::Layer> cc_layer_;

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
  viz::ReleaseCallback transfer_release_callback_;

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

  // TODO(crbug.com/40786876): temporary while tracking down crash.
  bool in_send_damaged_rects_ = false;
  bool sending_damaged_rects_for_descendants_ = false;
  bool no_mutation_ = false;  // CHECK on Add/SetMakeLayer if true.

  base::WeakPtrFactory<Layer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_H_
