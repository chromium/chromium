// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_H_
#define UI_VIEWS_VIEW_H_

#include <stddef.h>

#include <concepts>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/class_property.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/base/metadata/metadata_utils.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_cache.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/paint_info.h"
#include "ui/views/view_targeter.h"
#include "ui/views/views_export.h"

using ui::OSExchangeData;

namespace gfx {
class Canvas;
class Insets;
}  // namespace gfx

namespace ui {
struct AXActionData;
class ColorProvider;
class Compositor;
class InputMethod;
class Layer;
class LayerTreeOwner;
class NativeTheme;
class PaintContext;
class ThemeProvider;
class TransformRecorder;
}  // namespace ui

namespace views {

class Background;
class Border;
class ContextMenuController;
class DragController;
class FillLayout;
class FocusManager;
class FocusTraversable;
class LayoutProvider;
class ScrollView;
class SizeBounds;
class ViewAccessibility;
class ViewMaskLayer;
class ViewObserver;
class Widget;
class WordLookupClient;

namespace internal {
class PreEventDispatchHandler;
class PostEventDispatchHandler;
class RootView;
class ScopedChildrenLock;
}  // namespace internal

// Struct used to describe how a View hierarchy has changed. See
// View::ViewHierarchyChanged.
// TODO(pbos): Move to a separate view_hierarchy_changed_details.h header.
struct VIEWS_EXPORT ViewHierarchyChangedDetails {
  ViewHierarchyChangedDetails(bool is_add,
                              View* parent,
                              View* child,
                              View* move_view)
      : is_add(is_add), parent(parent), child(child), move_view(move_view) {}
  const bool is_add;
  // New parent if |is_add| is true, old parent if |is_add| is false.
  const raw_ptr<View> parent;
  // The view being added or removed.
  const raw_ptr<View> child;
  // If this is a move (reparent), meaning AddChildViewAt() is invoked with an
  // existing parent, then a notification for the remove is sent first,
  // followed by one for the add.  This case can be distinguished by a
  // non-null |move_view|.
  // For the remove part of move, |move_view| is the new parent of the View
  // being removed.
  // For the add part of move, |move_view| is the old parent of the View being
  // added.
  const raw_ptr<View> move_view;
};

using PropertyChangedCallback = ui::metadata::PropertyChangedCallback;

// The elements in PropertyEffects represent bits which define what effect(s) a
// changed Property has on the containing class. Additional elements should
// use the next most significant bit.
enum PropertyEffects {
  kPropertyEffectsNone = 0,
  // Any changes to the property should cause the container to invalidate the
  // current layout state.
  kPropertyEffectsLayout = 0x00000001,
  // Changes to the property should cause the container to schedule a painting
  // update.
  kPropertyEffectsPaint = 0x00000002,
  // Changes to the property should cause the preferred size to change. This
  // implies kPropertyEffectsLayout.
  kPropertyEffectsPreferredSizeChanged = 0x00000004,
};

// When adding layers to the view, this indicates the region into which the
// layer is placed, in the region above or beneath the view.
enum class LayerRegion {
  kAbove,
  kBelow,
};

// When calling |GetLayersInOrder|, this will indicate whether the View's
// own layer should be included in the returned vector or not.
enum class ViewLayer {
  kInclude,
  kExclude,
};

/////////////////////////////////////////////////////////////////////////////
//
// View class
//
//   A View is a rectangle within the views View hierarchy. It is the base
//   class for all Views.
//
//   A View is a container of other Views (there is no such thing as a Leaf
//   View - makes code simpler, reduces type conversion headaches, design
//   mistakes etc)
//
//   The View contains basic properties for sizing (bounds), layout (flex,
//   orientation, etc), painting of children and event dispatch.
//
//   The View also uses a simple Box Layout Manager similar to XUL's
//   SprocketLayout system. Alternative Layout Managers implementing the
//   LayoutManager interface can be used to lay out children if required.
//
//   It is up to the subclass to implement Painting and storage of subclass -
//   specific properties and functionality.
//
//   Unless otherwise documented, views is not thread safe and should only be
//   accessed from the main thread.
//
//   Properties ------------------
//
//   Properties which are intended to be dynamically visible through metadata to
//   other subsystems, such as dev-tools must adhere to a naming convention,
//   usage and implementation patterns.
//
//   Properties start with their base name, such as "Frobble" (note the
//   capitalization). The method to set the property must be called SetXXXX and
//   the method to retrieve the value is called GetXXXX. For the aforementioned
//   Frobble property, this would be SetFrobble and GetFrobble.
//
//   void SetFrobble(bool is_frobble);
//   bool GetFrobble() const;
//
//   In the SetXXXX method, after the value storage location has been updated,
//   OnPropertyChanged() must be called using the address of the storage
//   location as a key. Additionally, any combination of PropertyEffects are
//   also passed in. This will ensure that any desired side effects are properly
//   invoked.
//
//   void View::SetFrobble(bool is_frobble) {
//     if (is_frobble == frobble_)
//       return;
//     frobble_ = is_frobble;
//     OnPropertyChanged(&frobble_, kPropertyEffectsPaint);
//   }
//
//   Each property should also have a way to "listen" to changes by registering
//   a callback.
//
//   base::CallbackListSubscription AddFrobbleChangedCallback(
//       PropertyChangedCallback callback);
//
//   Each callback uses the the existing base::Bind mechanisms which allow for
//   various kinds of callbacks; object methods, normal functions and lambdas.
//
//   Example:
//
//   class FrobbleView : public View {
//    ...
//    private:
//     void OnFrobbleChanged();
//     base::CallbackListSubscription frobble_changed_subscription_;
//   }
//
//   ...
//     frobble_changed_subscription_ = AddFrobbleChangedCallback(
//         base::BindRepeating(&FrobbleView::OnFrobbleChanged,
//         base::Unretained(this)));
//
//   Example:
//
//   void MyView::ValidateFrobbleChanged() {
//     bool frobble_changed = false;
//     base::CallbackListSubscription subscription =
//       frobble_view_->AddFrobbleChangedCallback(
//           base::BindRepeating([](bool* frobble_changed_ptr) {
//             *frobble_changed_ptr = true;
//           }, &frobble_changed));
//     frobble_view_->SetFrobble(!frobble_view_->GetFrobble());
//     LOG() << frobble_changed ? "Frobble changed" : "Frobble NOT changed!";
//   }
//
//   Property metadata -----------
//
//   For Views that expose properties which are intended to be dynamically
//   discoverable by other subsystems, each View and its descendants must
//   include metadata. These other subsystems, such as dev tools or a
//   declarative layout system, can then enumerate the properties on any given
//   instance or class. Using the enumerated information, the actual values of
//   the properties can be read or written. This will be done by getting and
//   setting the values using string representations. The metadata can also be
//   used to instantiate and initialize a View (or descendant) class from a
//   declarative "script".
//
//   For each View class in their respective header declaration, place the macro
//   METADATA_HEADER(<classname>, <view ancestor class>) in the initial private
//   section.
//
//   In the implementing .cc file, add the following macros to the same
//   namespace in which the class resides.
//
//   BEGIN_METADATA(View)
//   ADD_PROPERTY_METADATA(bool, Frobble)
//   END_METADATA
//
//   For each property, add a definition using ADD_PROPERTY_METADATA() between
//   the begin and end macros.
//
//   BEGIN_METADATA(MyView)
//   ADD_PROPERTY_METADATA(int, Bobble)
//   END_METADATA
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT View : public ui::LayerDelegate,
                          public ui::LayerObserver,
                          public ui::LayerOwner,
                          public ui::AcceleratorTarget,
                          public ui::EventTarget,
                          public ui::EventHandler,
                          public ui::PropertyHandler,
                          public ui::metadata::MetaDataProvider {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  using PassKey = base::NonCopyablePassKey<View>;
  using Views = std::vector<raw_ptr<View, VectorExperimental>>;

  // TODO(crbug.com/40212171): The |event| parameter is being removed. Do not
  // add new callers.
  using DropCallback = base::OnceCallback<void(
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner)>;

  METADATA_HEADER_BASE(View);

  enum class FocusBehavior {
    // Use when the View is never focusable. Default.
    NEVER,

    // Use when the View is to be focusable both in regular and accessibility
    // mode.
    ALWAYS,

    // Use when the View is focusable only during accessibility mode.
    ACCESSIBLE_ONLY,
  };

  // During paint, the origin of each view in physical pixel is calculated by
  //   view_origin_pixel = ROUND(view.origin() * device_scale_factor)
  //
  // Thus in a view hierarchy, the offset between two views, view_i and view_j,
  // is calculated by:
  //   view_offset_ij_pixel = SUM [view_origin_pixel.OffsetFromOrigin()]
  //                        {For all views along the path from view_i to view_j}
  //
  // But the offset between the two layers, the layer in view_i and the layer in
  // view_j, is computed by
  //   view_offset_ij_dip = SUM [view.origin().OffsetFromOrigin()]
  //                        {For all views along the path from view_i to view_j}
  //
  //   layer_offset_ij_pixel = ROUND (view_offset_ij_dip * device_scale_factor)
  //
  // Due to this difference in the logic for computation of offset, the values
  // view_offset_ij_pixel and layer_offset_ij_pixel may not always be equal.
  // They will differ by some subpixel_offset. This leads to bugs like
  // crbug.com/734787.
  // The subpixel offset needs to be applied to the layer to get the correct
  // output during paint.
  //
  // This class manages the computation of subpixel offset internally when
  // working with offsets.
  class LayerOffsetData {
   public:
    explicit LayerOffsetData(float device_scale_factor = 1.f,
                             const gfx::Vector2d& offset = gfx::Vector2d())
        : device_scale_factor_(device_scale_factor) {
      AddOffset(offset);
    }

    const gfx::Vector2d& offset() const { return offset_; }

    const gfx::Vector2dF GetSubpixelOffset() const {
      // |rounded_pixel_offset_| is stored in physical pixel space. Convert it
      // into DIP space before returning.
      gfx::Vector2dF subpixel_offset(rounded_pixel_offset_);
      subpixel_offset.InvScale(device_scale_factor_);
      return subpixel_offset;
    }

    LayerOffsetData& operator+=(const gfx::Vector2d& offset) {
      AddOffset(offset);
      return *this;
    }

    LayerOffsetData operator+(const gfx::Vector2d& offset) const {
      LayerOffsetData offset_data(*this);
      offset_data.AddOffset(offset);
      return offset_data;
    }

   private:
    // Adds the |offset_to_parent| to the total |offset_| and updates the
    // |rounded_pixel_offset_| value.
    void AddOffset(const gfx::Vector2d& offset_to_parent) {
      // Add the DIP |offset_to_parent| amount to the total offset.
      offset_ += offset_to_parent;

      // Convert |offset_to_parent| to physical pixel coordinates.
      gfx::Vector2dF fractional_pixel_offset(
          offset_to_parent.x() * device_scale_factor_,
          offset_to_parent.y() * device_scale_factor_);

      // Since pixels cannot be fractional, we need to round the offset to get
      // the correct physical pixel coordinate.
      gfx::Vector2d integral_pixel_offset =
          gfx::ToRoundedVector2d(fractional_pixel_offset);

      // |integral_pixel_offset - fractional_pixel_offset| gives the subpixel
      // offset amount for |offset_to_parent|. This is added to
      // |rounded_pixel_offset_| to update the total subpixel offset.
      rounded_pixel_offset_ += integral_pixel_offset - fractional_pixel_offset;
    }

    // Total offset so far. This stores the offset between two nodes in the view
    // hierarchy.
    gfx::Vector2d offset_;

    // This stores the value such that if added to
    // |offset_ * device_scale_factor| will give the correct aligned offset in
    // physical pixels.
    gfx::Vector2dF rounded_pixel_offset_;

    // The device scale factor at which the subpixel offset is being computed.
    float device_scale_factor_;
  };

  // Creation and lifetime -----------------------------------------------------

  View();
  View(const View&) = delete;
  View& operator=(const View&) = delete;
  ~View() override;

  // By default a View is owned by its parent unless specified otherwise here.
  void set_owned_by_client() { owned_by_client_ = true; }
  bool owned_by_client() const { return owned_by_client_; }

  // Tree operations -----------------------------------------------------------

  // Get the Widget that hosts this View, if any.
  virtual const Widget* GetWidget() const;
  virtual Widget* GetWidget();

  // Adds |view| as a child of this view, optionally at |index|.
  // Returns the raw pointer for callers which want to hold a pointer to the
  // added view. This requires declaring the function as a template in order to
  // return the actual passed-in type.
  template <typename T>
  T* AddChildView(std::unique_ptr<T> view) {
    DCHECK(!view->owned_by_client())
        << "This should only be called if the client is passing ownership of "
           "|view| to the parent View.";
    CHECK_CLASS_HAS_METADATA(T)
    return AddChildView<T>(view.release());
  }
  template <typename T>
  T* AddChildViewAt(std::unique_ptr<T> view, size_t index) {
    DCHECK(!view->owned_by_client())
        << "This should only be called if the client is passing ownership of "
           "|view| to the parent View.";
    CHECK_CLASS_HAS_METADATA(T)
    return AddChildViewAt<T>(view.release(), index);
  }

  // Prefer using the AddChildView(std::unique_ptr) overloads over raw pointers
  // for new code.
  template <typename T>
  T* AddChildView(T* view) {
    CHECK_CLASS_HAS_METADATA(T)
    AddChildViewAtImpl(view, children_.size());
    return view;
  }
  template <typename T>
  T* AddChildViewAt(T* view, size_t index) {
    CHECK_CLASS_HAS_METADATA(T)
    AddChildViewAtImpl(view, index);
    return view;
  }

  template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
  T* AddChildView(raw_ptr<T, Traits> view) {
    CHECK_CLASS_HAS_METADATA(T)
    AddChildViewAtImpl(view.get(), children_.size());
    return view;
  }
  template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
  T* AddChildViewAt(raw_ptr<T, Traits> view, size_t index) {
    CHECK_CLASS_HAS_METADATA(T)
    AddChildViewAtImpl(view.get(), index);
    return view;
  }

  // Moves |view| to the specified |index|. An |index| at least as large as that
  // of the last child moves the view to the end.
  void ReorderChildView(View* view, size_t index);

  // Removes |view| from this view. The view's parent will change to null.
  void RemoveChildView(View* view);

  // Removes |view| from this view and transfers ownership back to the caller in
  // the form of a std::unique_ptr<T>.
  // TODO(kylixrd): Rename back to RemoveChildView() once the code is refactored
  //                to eliminate the uses of the old RemoveChildView().
  template <typename T>
  std::unique_ptr<T> RemoveChildViewT(T* view) {
    DCHECK(!view->owned_by_client())
        << "This should only be called if the client doesn't already have "
           "ownership of |view|.";
    DCHECK(base::Contains(children_, view));
    RemoveChildView(view);
    return base::WrapUnique(view);
  }

  // Partially specialized version to directly take a raw_ptr<T>.
  template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
  std::unique_ptr<T> RemoveChildViewT(raw_ptr<T, Traits> view) {
    return RemoveChildViewT(view.get());
  }

  // Removes all the children from this view. This deletes all children that are
  // not set_owned_by_client(), which is deprecated.
  void RemoveAllChildViews();

  // TODO(pbos): Remove this method, deleting children when removing them should
  // not be optional. If ownership needs to be preserved, use RemoveChildViewT()
  // to retain ownership of the removed children.
  void RemoveAllChildViewsWithoutDeleting();

  const Views& children() const { return children_; }

  // Returns the parent view.
  const View* parent() const { return parent_; }
  View* parent() { return parent_; }

  // Returns true if |view| is contained within this View's hierarchy, even as
  // an indirect descendant. Will return true if child is also this view.
  bool Contains(const View* view) const;

  // Returns an iterator pointing to |view|, or children_.cend() if |view| is
  // not a child of this view.
  Views::const_iterator FindChild(const View* view) const;

  // Returns the index of |view|, or nullopt if |view| is not a child of this
  // view.
  std::optional<size_t> GetIndexOf(const View* view) const;

  // Size and disposition ------------------------------------------------------
  // Methods for obtaining and modifying the position and size of the view.
  // Position is in the coordinate system of the view's parent.
  // Position is NOT flipped for RTL. See "RTL positioning" for RTL-sensitive
  // position accessors.
  // Transformations are not applied on the size/position. For example, if
  // bounds is (0, 0, 100, 100) and it is scaled by 0.5 along the X axis, the
  // width will still be 100 (although when painted, it will be 50x100, painted
  // at location (0, 0)).

  void SetBounds(int x, int y, int width, int height);
  void SetBoundsRect(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);
  void SetPosition(const gfx::Point& position);
  void SetX(int x);
  void SetY(int y);

  // No transformation is applied on the size or the locations.
  const gfx::Rect& bounds() const { return bounds_; }
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }
  const gfx::Point& origin() const { return bounds_.origin(); }
  const gfx::Size& size() const { return bounds_.size(); }

  // Returns the bounds of the content area of the view, i.e. the rectangle
  // enclosed by the view's border.
  gfx::Rect GetContentsBounds() const;

  // Returns the bounds of the view in its own coordinates (i.e. position is
  // 0, 0).
  gfx::Rect GetLocalBounds() const;

  // Returns the insets of the current border. If there is no border an empty
  // insets is returned.
  virtual gfx::Insets GetInsets() const;

  // Returns the visible bounds of the receiver in the receivers coordinate
  // system.
  //
  // When traversing the View hierarchy in order to compute the bounds, the
  // function takes into account the mirroring setting and transformation for
  // each View and therefore it will return the mirrored and transformed version
  // of the visible bounds if need be.
  gfx::Rect GetVisibleBounds() const;

  // Return the bounds of the View in screen coordinate system.
  gfx::Rect GetBoundsInScreen() const;

  // Return the bounds that an anchored widget should anchor to. These can be
  // different from |GetBoundsInScreen()| when a view is larger than its visible
  // size, for instance to provide a larger hittable area.
  virtual gfx::Rect GetAnchorBoundsInScreen() const;

  // Returns the baseline of this view, or -1 if this view has no baseline. The
  // return value is relative to the preferred height.
  virtual int GetBaseline() const;

  // Get the size the View would like to be given `available_size`, ignoring the
  // current bounds.
  gfx::Size GetPreferredSize(const SizeBounds& available_size = {}) const;

  // Sets or unsets the size that this View will request during layout. The
  // actual size may differ. It should rarely be necessary to set this; usually
  // the right approach is controlling the parent's layout via a LayoutManager.
  void SetPreferredSize(std::optional<gfx::Size> size);

  // Convenience method that sizes this view to its preferred size.
  void SizeToPreferredSize();

  // Gets the minimum size of the view. View's implementation invokes
  // GetPreferredSize.
  virtual gfx::Size GetMinimumSize() const;

  // Gets the maximum size of the view. Currently only used for sizing shell
  // windows.
  virtual gfx::Size GetMaximumSize() const;

  // Return the preferred height for a specific width. It is a helper function
  // of GetPreferredSize(SizeBounds(w, SizeBound())).height().
  int GetHeightForWidth(int w) const;

  // Returns a bound on the available space for a child view, for example, in
  // case the child view wants to play an animation that would cause it to
  // become larger. Default is not to bound the available size; it is the
  // responsibility of specific view/layout manager implementations to determine
  // if and when a bound applies.
  virtual SizeBounds GetAvailableSize(const View* child) const;

  // The |Visible| property. See comment above for instructions on declaring and
  // implementing a property.
  //
  // Sets whether this view is visible. Painting is scheduled as needed. Also,
  // clears focus if the focused view or one of its ancestors is set to be
  // hidden.
  virtual void SetVisible(bool visible);
  // Return whether a view is visible.
  bool GetVisible() const;

  // Adds a callback associated with the above Visible property. The callback
  // will be invoked whenever the Visible property changes.
  [[nodiscard]] base::CallbackListSubscription AddVisibleChangedCallback(
      PropertyChangedCallback callback);

  // Returns true if this view is drawn on screen.
  virtual bool IsDrawn() const;

  // The |Enabled| property. See comment above for instructions on declaring and
  // implementing a property.
  //
  // Set whether this view is enabled. A disabled view does not receive keyboard
  // or mouse inputs. If |enabled| differs from the current value, SchedulePaint
  // is invoked. Also, clears focus if the focused view is disabled.
  void SetEnabled(bool enabled);
  // Returns whether the view is enabled.
  bool GetEnabled() const;

  // Adds a callback associated with the above |Enabled| property. The callback
  // will be invoked whenever the property changes.
  [[nodiscard]] base::CallbackListSubscription AddEnabledChangedCallback(
      PropertyChangedCallback callback);

  // Returns the child views ordered in reverse z-order. That is, views later in
  // the returned vector have a higher z-order (are painted later) than those
  // early in the vector. The returned vector has exactly the same number of
  // Views as |children_|. The default implementation returns |children_|,
  // subclass if the paint order should differ from that of |children_|.
  // This order is taken into account by painting and targeting implementations.
  // NOTE: see SetPaintToLayer() for details on painting and views with layers.
  virtual Views GetChildrenInZOrder();

  // Transformations -----------------------------------------------------------

  // Methods for setting transformations for a view (e.g. rotation, scaling).
  // Care should be taken not to transform a view in such a way that its bounds
  // lie outside those of its parent, or else the default ViewTargeterDelegate
  // implementation will not pass mouse events to the view.

  gfx::Transform GetTransform() const;

  // Clipping is done relative to the view's local bounds.
  void SetClipPath(const SkPath& path);
  const SkPath& clip_path() const { return clip_path_; }

  // Sets the transform to the supplied transform.
  void SetTransform(const gfx::Transform& transform);

  // Accelerated painting ------------------------------------------------------

  // Sets whether this view paints to a layer. A view paints to a layer if
  // either of the following are true:
  // . the view has a non-identity transform.
  // . SetPaintToLayer(ui::LayerType) has been invoked.
  // View creates the Layer only when it exists in a Widget with a non-NULL
  // Compositor.
  // Enabling a view to have a layer impacts painting of sibling views.
  // Specifically views with layers effectively paint in a z-order that is
  // always above any sibling views that do not have layers. This happens
  // regardless of the ordering returned by GetChildrenInZOrder().
  void SetPaintToLayer(ui::LayerType layer_type = ui::LAYER_TEXTURED);

  // Cancels layer painting triggered by a call to |SetPaintToLayer()|. Note
  // that this will not actually destroy the layer if the view paints to a layer
  // for another reason.
  void DestroyLayer();

  // Add or remove layers above or below this view. This view does not take
  // ownership of the layers. It is the caller's responsibility to keep track of
  // this View's size and update their layer accordingly.
  //
  // In very rare cases, it may be necessary to override these. If any of this
  // view's contents must be painted to the same layer as its parent, or can't
  // handle being painted with transparency, overriding might be appropriate.
  // One example is LabelButton, where the label must paint below any added
  // layers for subpixel rendering reasons. Overrides should be made
  // judiciously, and generally they should just forward the calls to a child
  // view. They must be overridden together for correctness.
  virtual void AddLayerToRegion(ui::Layer* new_layer, LayerRegion region);
  virtual void RemoveLayerFromRegions(ui::Layer* old_layer);

  // This is like RemoveLayerFromRegions() but doesn't remove |old_layer| from
  // its parent. This is useful for when a layer beneth this view is owned by a
  // ui::LayerOwner which just recreated it (by calling RecreateLayer()). In
  // this case, this function can be called to remove it from |layers_below_| or
  // |layers_above_|, and to stop observing it, but it remains in the layer tree
  // since the expectation of ui::LayerOwner::RecreateLayer() is that the old
  // layer remains under the same parent, and stacked above the newly cloned
  // layer.
  void RemoveLayerFromRegionsKeepInLayerTree(ui::Layer* old_layer);

  // Gets the layers associated with this view that should be immediate children
  // of the parent layer. They are returned in bottom-to-top order. This
  // optionally includes |this->layer()| and any layers added with
  // |AddLayerToRegion()|.
  // Returns an empty vector if this view doesn't paint to a layer.
  std::vector<ui::Layer*> GetLayersInOrder(
      ViewLayer view_layer = ViewLayer::kInclude);

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  // Overridden from ui::LayerOwner:
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  // RTL positioning -----------------------------------------------------------

  // Methods for accessing the bounds and position of the view, relative to its
  // parent. The position returned is mirrored if the parent view is using a RTL
  // layout.
  //
  // NOTE: in the vast majority of the cases, the mirroring implementation is
  //       transparent to the View subclasses and therefore you should use the
  //       bounds() accessor instead.
  gfx::Rect GetMirroredBounds() const;
  gfx::Rect GetMirroredContentsBounds() const;
  gfx::Point GetMirroredPosition() const;
  int GetMirroredX() const;

  // Given a rectangle specified in this View's coordinate system, the function
  // computes the 'left' value for the mirrored rectangle within this View. If
  // the View's UI layout is not right-to-left, then bounds.x() is returned.
  //
  // UI mirroring is transparent to most View subclasses and therefore there is
  // no need to call this routine from anywhere within your subclass
  // implementation.
  int GetMirroredXForRect(const gfx::Rect& rect) const;

  // Given a rectangle specified in this View's coordinate system, the function
  // computes the mirrored rectangle.
  gfx::Rect GetMirroredRect(const gfx::Rect& rect) const;

  // Given the X coordinate of a point inside the View, this function returns
  // the mirrored X coordinate of the point if the View's UI layout is
  // right-to-left. If the layout is left-to-right, the same X coordinate is
  // returned.
  //
  // Following are a few examples of the values returned by this function for
  // a View with the bounds {0, 0, 100, 100} and a right-to-left layout:
  //
  // GetMirroredXCoordinateInView(0) -> 100
  // GetMirroredXCoordinateInView(20) -> 80
  // GetMirroredXCoordinateInView(99) -> 1
  int GetMirroredXInView(int x) const;

  // Given a X coordinate and a width inside the View, this function returns
  // the mirrored X coordinate if the View's UI layout is right-to-left. If the
  // layout is left-to-right, the same X coordinate is returned.
  //
  // Following are a few examples of the values returned by this function for
  // a View with the bounds {0, 0, 100, 100} and a right-to-left layout:
  //
  // GetMirroredXCoordinateInView(0, 10) -> 90
  // GetMirroredXCoordinateInView(20, 20) -> 60
  int GetMirroredXWithWidthInView(int x, int w) const;

  // Layout --------------------------------------------------------------------

  // Lays out the child Views (sets their bounds based on sizing heuristics
  // specific to the current LayoutManager).
  //
  // To customize layout behavior, use LayoutManagers; see
  // https://chromium.googlesource.com/chromium/src/+/main/docs/ui/learn/bestpractices/layout.md?pli=1#Use-LayoutManagers.
  // For now, classes may override Layout() to customize this manually, but this
  // will eventually be removed; see https://crbug.com/1005568. Subclasses which
  // need to invoke a superclass' Layout() method during their own
  // implementation of Layout() can do so via LayoutSuperclass<SuperT>(this);
  // calling this in any other way or context is forbidden (and will likely
  // break at compile or run time).
  //
  // To cause a view to be laid out, use InvalidateLayout(), which will
  // perform layout asynchronously; see
  // https://chromium.googlesource.com/chromium/src/+/main/docs/ui/learn/bestpractices/layout.md?pli=1#don_t-invoke-layout_directly.
  // For now, classes may also call DeprecatedLayoutImmediately() to
  // synchronously lay out a view, but this will eventually be removed; see
  // https://crbug.com/1521108. Neither of these methods should be called from
  // Layout(); see https://crbug.com/1121681.
  void DeprecatedLayoutImmediately();
  virtual void Layout(PassKey);

  bool needs_layout() const { return needs_layout_; }

  // Mark this view and all parents to require a relayout. This ensures the
  // next layout will propagate to this view, even if the bounds of parent views
  // do not change.
  void InvalidateLayout();

  // Sets whether or not the layout manager need to respect the available space.
  //
  // TODO(crbug.com/40232718): Remove this. When the vertical flexlayout with
  // cross axis is stretched, it will be (width, GetHeightForWidth(width)) when
  // calculating preferredsize, thus setting the width to an incorrect value.
  // This will cause unexpected results in some client code. This problem also
  // exists in BoxLayout. When we switch GetHeightForWidth in them to
  // GetPreferredSize, this problem should be solved.
  void SetLayoutManagerUseConstrainedSpace(
      bool layout_manager_use_constrained_space);

  // TODO(kylixrd): Update comment once UseDefaultFillLayout is true by default.
  // UseDefaultFillLayout will be set to true by default once the codebase is
  // audited and refactored.
  //
  // Gets/Sets the Layout Manager used by this view to size and place its
  // children. NOTE: This will force UseDefaultFillLayout to false if it had
  // been set to true.
  //
  // The LayoutManager is owned by the View and is deleted when the view is
  // deleted, or when a new LayoutManager is installed. Call
  // SetLayoutManager(nullptr) to clear it.
  //
  // SetLayoutManager returns a bare pointer version of the input parameter
  // (now owned by the view). If code needs to use the layout manager after
  // being assigned, use this pattern:
  //
  //   views::BoxLayout* box_layout = SetLayoutManager(
  //       std::make_unique<views::BoxLayout>(...));
  //   box_layout->Foo();
  LayoutManager* GetLayoutManager() const;
  template <typename LayoutManager>
  LayoutManager* SetLayoutManager(
      std::unique_ptr<LayoutManager> layout_manager) {
    LayoutManager* lm = layout_manager.get();
    SetLayoutManagerImpl(std::move(layout_manager));
    return lm;
  }
  void SetLayoutManager(std::nullptr_t);

  // Sets whether or not the default layout manager should be used for this
  // view. NOTE: this can only be set if |layout_manager_| isn't assigned.
  bool GetUseDefaultFillLayout() const;
  void SetUseDefaultFillLayout(bool value);

  // Attributes ----------------------------------------------------------------

  // Recursively descends the view tree starting at this view, and returns
  // the first child that it encounters that has the given ID.
  // Returns NULL if no matching child view is found.
  const View* GetViewByID(int id) const;
  View* GetViewByID(int id);

  // Gets and sets the ID for this view. ID should be unique within the subtree
  // that you intend to search for it. 0 is the default ID for views.
  int GetID() const { return id_; }
  void SetID(int id);

  // Adds a callback associated with the above |ID| property. The callback will
  // be invoked whenever the property changes.
  [[nodiscard]] base::CallbackListSubscription AddIDChangedCallback(
      PropertyChangedCallback callback);

  // A group id is used to tag views which are part of the same logical group.
  // Focus can be moved between views with the same group using the arrow keys.
  // Groups are currently used to implement radio button mutual exclusion.
  // The group id is immutable once it's set.
  void SetGroup(int gid);
  // Returns the group id of the view, or -1 if the id is not set yet.
  int GetGroup() const;

  // Adds a callback associated with the above |Group| property. The callback
  // will be invoked whenever the property changes.
  [[nodiscard]] base::CallbackListSubscription AddGroupChangedCallback(
      PropertyChangedCallback callback);

  // If this returns true, the views from the same group can each be focused
  // when moving focus with the Tab/Shift-Tab key.  If this returns false,
  // only the selected view from the group (obtained with
  // GetSelectedViewForGroup()) is focused.
  virtual bool IsGroupFocusTraversable() const;

  // Fills |views| with all the available views which belong to the provided
  // |group|.
  void GetViewsInGroup(int group, Views* views);

  // Returns the View that is currently selected in |group|.
  // The default implementation simply returns the first View found for that
  // group.
  virtual View* GetSelectedViewForGroup(int group);

  // Returns the name of this particular instance of the class. This is useful
  // to identify multiple instances of the same class within the same view
  // hierarchy. The default value returned is GetClassName().
  // Note: GetClassName() will eventually be made non-virtual. Override this
  // method instead to provide a more unique object name for the instance.
  virtual std::string GetObjectName() const;

  // Coordinate conversion -----------------------------------------------------

  // Note that the utility coordinate conversions functions always operate on
  // the mirrored position of the child Views if the parent View uses a
  // right-to-left UI layout.

  // Converts a point from the coordinate system of one View to another.
  //
  // |source| and |target| must be in the same widget, but don't need to be in
  // the same view hierarchy.
  // Neither |source| nor |target| can be null.
  [[nodiscard]] static gfx::Point ConvertPointToTarget(const View* source,
                                                       const View* target,
                                                       const gfx::Point& point);
  // The in-place version of this method is strongly discouraged, please use the
  // by-value version above for improved const-compatability and readability.
  static void ConvertPointToTarget(const View* source,
                                   const View* target,
                                   gfx::Point* point);

  // Converts |rect| from the coordinate system of |source| to the coordinate
  // system of |target|.
  //
  // |source| and |target| must be in the same widget, but don't need to be in
  // the same view hierarchy.
  // Neither |source| nor |target| can be null.
  [[nodiscard]] static gfx::RectF ConvertRectToTarget(const View* source,
                                                      const View* target,
                                                      const gfx::RectF& rect);
  // The in-place version of this method is strongly discouraged, please use the
  // by-value version above for improved const-compatability and readability.
  static void ConvertRectToTarget(const View* source,
                                  const View* target,
                                  gfx::RectF* rect);

  // Converts |rect| from the coordinate system of |source| to the
  // coordinate system of |target|.
  //
  // |source| and |target| must be in the same widget, but don't need to be in
  // the same view hierarchy.
  // Neither |source| nor |target| can be null.
  //
  // Returns the enclosed rect with default allowed conversion error
  // (0.00001f).
  static gfx::Rect ConvertRectToTarget(const View* source,
                                       const View* target,
                                       const gfx::Rect& rect);

  // Converts a point from a View's coordinate system to that of its Widget.
  static void ConvertPointToWidget(const View* src, gfx::Point* point);

  // Converts a point from the coordinate system of a View's Widget to that
  // View's coordinate system.
  static void ConvertPointFromWidget(const View* dest, gfx::Point* p);

  // Converts a point from a View's coordinate system to that of the screen.
  [[nodiscard]] static gfx::Point ConvertPointToScreen(const View* src,
                                                       const gfx::Point& point);
  // The in-place version of this method is strongly discouraged, please use the
  // by-value version above for improved const-compatability and readability.
  static void ConvertPointToScreen(const View* src, gfx::Point* point);

  // Converts a point from the screen coordinate system to that View's
  // coordinate system.
  [[nodiscard]] static gfx::Point ConvertPointFromScreen(
      const View* src,
      const gfx::Point& point);
  // The in-place version of this method is strongly discouraged, please use the
  // by-value version above for improved const-compatability and readability.
  static void ConvertPointFromScreen(const View* dst, gfx::Point* point);

  // Converts a rect from a View's coordinate system to that of the screen.
  static void ConvertRectToScreen(const View* src, gfx::Rect* rect);

  // Applies transformation on the rectangle, which is in the view's coordinate
  // system, to convert it into the parent's coordinate system.
  gfx::Rect ConvertRectToParent(const gfx::Rect& rect) const;

  // Converts a rectangle from this views coordinate system to its widget
  // coordinate system.
  gfx::Rect ConvertRectToWidget(const gfx::Rect& rect) const;

  // Painting ------------------------------------------------------------------

  // Mark all or part of the View's bounds as dirty (needing repaint).
  // |r| is in the View's coordinates.
  // TODO(beng): Make protected.
  void SchedulePaint();
  void SchedulePaintInRect(const gfx::Rect& r);

  // Called by the framework to paint a View. Performs translation and clipping
  // for View coordinates and language direction as required, allows the View
  // to paint itself via the various OnPaint*() event handlers and then paints
  // the hierarchy beneath it.
  void Paint(const PaintInfo& parent_paint_info);

  // The background object may be null.
  void SetBackground(std::unique_ptr<Background> b);
  Background* GetBackground() const;
  const Background* background() const { return background_.get(); }
  Background* background() { return background_.get(); }

  // The border object may be null.
  virtual void SetBorder(std::unique_ptr<Border> b);
  Border* GetBorder() const;

  // Get the theme provider from the parent widget.
  const ui::ThemeProvider* GetThemeProvider() const;

  // Get the layout provider for the View.
  const LayoutProvider* GetLayoutProvider() const;

  // Returns the ColorProvider from the ColorProviderManager.
  ui::ColorProvider* GetColorProvider() {
    return const_cast<ui::ColorProvider*>(
        std::as_const(*this).GetColorProvider());
  }
  const ui::ColorProvider* GetColorProvider() const;

  // Returns the NativeTheme to use for this View. This calls through to
  // GetNativeTheme() on the Widget this View is in, or provides a default
  // theme if there's no widget. Warning: the default theme might not be
  // correct; you should probably override OnThemeChanged().
  ui::NativeTheme* GetNativeTheme() {
    return const_cast<ui::NativeTheme*>(std::as_const(*this).GetNativeTheme());
  }
  const ui::NativeTheme* GetNativeTheme() const;

  // RTL painting --------------------------------------------------------------

  // Returns whether the gfx::Canvas object passed to Paint() needs to be
  // transformed such that anything drawn on the canvas object during Paint()
  // is flipped horizontally.
  bool GetFlipCanvasOnPaintForRTLUI() const;
  // Enables or disables flipping of the gfx::Canvas during Paint(). Note that
  // if canvas flipping is enabled, the canvas will be flipped only if the UI
  // layout is right-to-left; that is, the canvas will be flipped only if
  // GetMirrored() is true.
  //
  // Enabling canvas flipping is useful for leaf views that draw an image that
  // needs to be flipped horizontally when the UI layout is right-to-left
  // (views::Button, for example). This method is helpful for such classes
  // because their drawing logic stays the same and they can become agnostic to
  // the UI directionality.
  void SetFlipCanvasOnPaintForRTLUI(bool enable);

  // Adds a callback associated with the above FlipCanvasOnPaintForRTLUI
  // property. The callback will be invoked whenever the
  // FlipCanvasOnPaintForRTLUI property changes.
  [[nodiscard]] base::CallbackListSubscription
  AddFlipCanvasOnPaintForRTLUIChangedCallback(PropertyChangedCallback callback);

  // When set, this view will ignore base::l18n::IsRTL() and instead be drawn
  // according to |is_mirrored|.
  //
  // This is useful for views that should be displayed the same regardless of UI
  // direction. Unlike SetFlipCanvasOnPaintForRTLUI this setting has an effect
  // on the visual order of child views.
  //
  // This setting does not propagate to child views. So while the visual order
  // of this view's children may change, the visual order of this view's
  // grandchildren in relation to their parents are unchanged.
  void SetMirrored(bool is_mirrored);
  bool GetMirrored() const;

  // Input ---------------------------------------------------------------------
  // The points, rects, mouse locations, and touch locations in the following
  // functions are in the view's coordinates, except for a RootView.

  // A convenience function which calls into GetEventHandlerForRect() with
  // a 1x1 rect centered at |point|. |point| is in the local coordinate
  // space of |this|.
  View* GetEventHandlerForPoint(const gfx::Point& point);

  // Returns the View that should be the target of an event having |rect| as
  // its location, or NULL if no such target exists. |rect| is in the local
  // coordinate space of |this|.
  View* GetEventHandlerForRect(const gfx::Rect& rect);

  // Returns the deepest visible descendant that contains the specified point
  // and supports tooltips. If the view does not contain the point, returns
  // NULL.
  virtual View* GetTooltipHandlerForPoint(const gfx::Point& point);

  // Return the cursor that should be used for this view or the default cursor.
  // The event location is in the receiver's coordinate system. The caller is
  // responsible for managing the lifetime of the returned object, though that
  // lifetime may vary from platform to platform. On Windows and Aura,
  // the cursor is a shared resource.
  virtual ui::Cursor GetCursor(const ui::MouseEvent& event);

  // A convenience function which calls HitTestRect() with a rect of size
  // 1x1 and an origin of |point|. |point| is in the local coordinate space
  // of |this|.
  bool HitTestPoint(const gfx::Point& point) const;

  // Returns true if |rect| intersects this view's bounds. |rect| is in the
  // local coordinate space of |this|.
  bool HitTestRect(const gfx::Rect& rect) const;

  // Returns true if this view or any of its descendants are permitted to
  // be the target of an event.
  virtual bool GetCanProcessEventsWithinSubtree() const;

  // Sets whether this view or any of its descendants are permitted to be the
  // target of an event.
  void SetCanProcessEventsWithinSubtree(bool can_process);

  // Returns true if the mouse cursor is over |view| and mouse events are
  // enabled.
  bool IsMouseHovered() const;

  // This method is invoked when the user clicks on this view.
  // The provided event is in the receiver's coordinate system.
  //
  // Return true if you processed the event and want to receive subsequent
  // MouseDragged and MouseReleased events.  This also stops the event from
  // bubbling.  If you return false, the event will bubble through parent
  // views.
  //
  // If you remove yourself from the tree while processing this, event bubbling
  // stops as if you returned true, but you will not receive future events.
  // The return value is ignored in this case.
  //
  // Default implementation returns true if a ContextMenuController has been
  // set, false otherwise. Override as needed.
  //
  virtual bool OnMousePressed(const ui::MouseEvent& event);

  // This method is invoked when the user clicked on this control.
  // and is still moving the mouse with a button pressed.
  // The provided event is in the receiver's coordinate system.
  //
  // Return true if you processed the event and want to receive
  // subsequent MouseDragged and MouseReleased events.
  //
  // Default implementation returns true if a ContextMenuController has been
  // set, false otherwise. Override as needed.
  //
  virtual bool OnMouseDragged(const ui::MouseEvent& event);

  // This method is invoked when the user releases the mouse
  // button. The event is in the receiver's coordinate system.
  //
  // Default implementation notifies the ContextMenuController is appropriate.
  // Subclasses that wish to honor the ContextMenuController should invoke
  // super.
  virtual void OnMouseReleased(const ui::MouseEvent& event);

  // This method is invoked when the mouse press/drag was canceled by a
  // system/user gesture.
  virtual void OnMouseCaptureLost();

  // This method is invoked when the mouse is above this control
  // The event is in the receiver's coordinate system.
  //
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseMoved(const ui::MouseEvent& event);

  // This method is invoked when the mouse enters this control.
  //
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseEntered(const ui::MouseEvent& event);

  // This method is invoked when the mouse exits this control
  // The provided event location is always (0, 0)
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseExited(const ui::MouseEvent& event);

  // Set both the MouseHandler and the GestureHandler for a drag session.
  //
  // A drag session is a stream of mouse events starting
  // with a MousePressed event, followed by several MouseDragged
  // events and finishing with a MouseReleased event.
  //
  // This method should be only invoked while processing a
  // MouseDragged or MousePressed event.
  //
  // All further mouse dragged and mouse up events will be sent
  // the MouseHandler, even if it is reparented to another window.
  //
  // The MouseHandler is automatically cleared when the control
  // comes back from processing the MouseReleased event.
  //
  // Note: if the mouse handler is no longer connected to a
  // view hierarchy, events won't be sent.
  virtual void SetMouseAndGestureHandler(View* new_handler);

  // Sets a new mouse handler.
  virtual void SetMouseHandler(View* new_handler);

  // Invoked when a key is pressed or released.
  // Subclasses should return true if the event has been processed and false
  // otherwise. If the event has not been processed, the parent will be given a
  // chance.
  virtual bool OnKeyPressed(const ui::KeyEvent& event);
  virtual bool OnKeyReleased(const ui::KeyEvent& event);

  // Invoked when the user uses the mousewheel. Implementors should return true
  // if the event has been processed and false otherwise. This message is sent
  // if the view is focused. If the event has not been processed, the parent
  // will be given a chance.
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event);

  // See field for description.
  void SetNotifyEnterExitOnChild(bool notify);
  bool GetNotifyEnterExitOnChild() const;

  // Convenience method to retrieve the InputMethod associated with the
  // Widget that contains this view.
  ui::InputMethod* GetInputMethod() {
    return const_cast<ui::InputMethod*>(std::as_const(*this).GetInputMethod());
  }
  const ui::InputMethod* GetInputMethod() const;

  // Sets a new ViewTargeter for the view, and returns the previous
  // ViewTargeter.
  std::unique_ptr<ViewTargeter> SetEventTargeter(
      std::unique_ptr<ViewTargeter> targeter);

  // Returns the ViewTargeter installed on |this| if one exists,
  // otherwise returns the ViewTargeter installed on our root view.
  // The return value is guaranteed to be non-null.
  ViewTargeter* GetEffectiveViewTargeter() const;

  ViewTargeter* targeter() const { return targeter_.get(); }

  // Returns the WordLookupClient associated with this view.
  virtual WordLookupClient* GetWordLookupClient();

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  ui::EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;
  void ConvertEventToTarget(const ui::EventTarget* target,
                            ui::LocatedEvent* event) const override;
  gfx::PointF GetScreenLocationF(const ui::LocatedEvent& event) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) final;
  void OnGestureEvent(ui::GestureEvent* event) override;
  std::string_view GetLogContext() const override;

  // Accelerators --------------------------------------------------------------

  // Sets a keyboard accelerator for that view. When the user presses the
  // accelerator key combination, the AcceleratorPressed method is invoked.
  // Note that you can set multiple accelerators for a view by invoking this
  // method several times. Note also that AcceleratorPressed is invoked only
  // when CanHandleAccelerators() is true.
  void AddAccelerator(const ui::Accelerator& accelerator);

  // Removes the specified accelerator for this view.
  void RemoveAccelerator(const ui::Accelerator& accelerator);

  // Removes all the keyboard accelerators for this view.
  void ResetAccelerators();

  // Overridden from AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // Returns whether accelerators are enabled for this view. Accelerators are
  // enabled if the containing widget is visible and the view is enabled() and
  // IsDrawn()
  bool CanHandleAccelerators() const override;

  // Focus ---------------------------------------------------------------------

  // Returns whether this view currently has the focus.
  virtual bool HasFocus() const;

  // Returns the view that is a candidate to be focused next when pressing Tab.
  //
  // The returned view might not be `IsFocusable`, but it's children can be
  // traversed to evaluate if one of them `IsFocusable`.
  //
  // If this returns `nullptr` then it is the last focusable candidate view in
  // the list including its siblings.
  View* GetNextFocusableView();
  const View* GetNextFocusableView() const;

  // Returns the view that is a candidate to be focused next when pressing
  // Shift-Tab.
  //
  // The returned view might not be `IsFocusable`, but it's children can be
  // traversed to evaluate if one of them `IsFocusable`.
  //
  // If this returns `nullptr` then it is the first focusable candidate view in
  // the list including its siblings.
  View* GetPreviousFocusableView();

  // Removes |this| from its focus list, updating the previous and next
  // views' points accordingly.
  void RemoveFromFocusList();

  // Insert |this| before or after |view| in the focus list.
  void InsertBeforeInFocusList(View* view);
  void InsertAfterInFocusList(View* view);

  // Returns the list of children in the order of their focus. Each child might
  // not be `IsFocusable`. Children that are not `IsFocusable` might still have
  // children of its own that are `IsFocusable`.
  Views GetChildrenFocusList();

  // Gets/sets |FocusBehavior|. SetFocusBehavior() advances focus if necessary.
  virtual FocusBehavior GetFocusBehavior() const;
  void SetFocusBehavior(FocusBehavior focus_behavior);

  // Set this to suppress default handling of focus for this View. By default
  // native focus will be cleared and a11y events announced based on the new
  // View focus.
  // TODO(pbos): This is here to make removing focus behavior from the base
  // implementation of OnFocus a no-op. Try to avoid new uses of this. Also
  // investigate if this can be configured with more granularity (which event
  // to fire on focus etc.).
  void set_suppress_default_focus_handling() {
    suppress_default_focus_handling_ = true;
  }

  // Returns true if this view is focusable, |enabled_| and drawn.
  bool IsFocusable() const;

  // Convenience method to retrieve the FocusManager associated with the
  // Widget that contains this view.  This can return NULL if this view is not
  // part of a view hierarchy with a Widget.
  FocusManager* GetFocusManager();
  const FocusManager* GetFocusManager() const;

  // Request keyboard focus. The receiving view will become the focused view.
  virtual void RequestFocus();

  // Invoked when a view is about to be requested for focus due to the focus
  // traversal. Reverse is this request was generated going backward
  // (Shift-Tab).
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) {}

  // Invoked when a key is pressed or released before the key event is processed
  // (and potentially eaten) by the focus manager for tab traversal,
  // accelerators and other focus related actions.
  // The default implementation returns false, ensuring that tab traversal and
  // accelerators processing is performed.
  // Subclasses should return true if they want to process the key event and not
  // have it processed as an accelerator (if any) or as a tab traversal (if the
  // key event is for the TAB key).  In that case, OnKeyPressed/OnKeyReleased
  // will subsequently be invoked for that event.
  virtual bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event);

  // Subclasses that contain traversable children that are not directly
  // accessible through the children hierarchy should return the associated
  // FocusTraversable for the focus traversal to work properly.
  virtual FocusTraversable* GetFocusTraversable();

  // Subclasses that can act as a "pane" must implement their own
  // FocusTraversable to keep the focus trapped within the pane.
  // If this method returns an object, any view that's a direct or
  // indirect child of this view will always use this FocusTraversable
  // rather than the one from the widget.
  virtual FocusTraversable* GetPaneFocusTraversable();

  // Tooltips ------------------------------------------------------------------

  // Gets the tooltip for this View. If the View does not have a tooltip,
  // the returned value should be empty.
  // Any time the tooltip text that a View is displaying changes, it must
  // invoke TooltipTextChanged.
  // |p| provides the coordinates of the mouse (relative to this view).
  virtual std::u16string GetTooltipText(const gfx::Point& p) const;

  // Views will normally display tooltips (if any) when they are focused
  // (which usually happens via a keyboard event). Because they are both
  // visible and displayed asynchronously, some tests may wish to disable
  // them so that they don't interfere with whatever is being tested. If the
  // tooltips are disabled via a feature flag, these routines will have no
  // effect (i.e., the feature flag overrides them).
  static void DisableKeyboardTooltipsForTesting();
  static void EnableKeyboardTooltipsForTesting();

  // Context menus -------------------------------------------------------------

  // Sets the ContextMenuController. Setting this to non-null makes the View
  // process mouse events.
  ContextMenuController* context_menu_controller() {
    return context_menu_controller_;
  }
  void set_context_menu_controller(ContextMenuController* menu_controller);

  // Provides default implementation for context menu handling. The default
  // implementation calls the ShowContextMenu of the current
  // ContextMenuController (if it is not NULL). Overridden in subclassed views
  // to provide right-click menu display triggered by the keyboard (i.e. for the
  // Chrome toolbar Back and Forward buttons). No source needs to be specified,
  // as it is always equal to the current View.
  // Note that this call is asynchronous for views menu and synchronous for
  // mac's native menu.
  virtual void ShowContextMenu(const gfx::Point& p,
                               ui::MenuSourceType source_type);

  // Returns the location, in screen coordinates, to show the context menu at
  // when the context menu is shown from the keyboard. This implementation
  // returns the middle of the visible region of this view.
  //
  // This method is invoked when the context menu is shown by way of the
  // keyboard.
  virtual gfx::Point GetKeyboardContextMenuLocation();

  // Drag and drop -------------------------------------------------------------

  DragController* drag_controller() { return drag_controller_; }
  void set_drag_controller(DragController* drag_controller) {
    drag_controller_ = drag_controller;
  }

  // During a drag and drop session when the mouse moves the view under the
  // mouse is queried for the drop types it supports by way of the
  // GetDropFormats methods. If the view returns true and the drag site can
  // provide data in one of the formats, the view is asked if the drop data
  // is required before any other drop events are sent. Once the
  // data is available the view is asked if it supports the drop (by way of
  // the CanDrop method). If a view returns true from CanDrop,
  // OnDragEntered is sent to the view when the mouse first enters the view,
  // as the mouse moves around within the view OnDragUpdated is invoked.
  // If the user releases the mouse over the view and OnDragUpdated returns a
  // valid drop, then GetDropCallback is invoked. If the mouse moves outside the
  // view or over another view that wants the drag, OnDragExited is invoked.
  //
  // Similar to mouse events, the deepest view under the mouse is first checked
  // if it supports the drop (Drop). If the deepest view under
  // the mouse does not support the drop, the ancestors are walked until one
  // is found that supports the drop.

  // Override and return the set of formats that can be dropped on this view.
  // |formats| is a bitmask of the formats defined bye OSExchangeData::Format.
  // The default implementation returns false, which means the view doesn't
  // support dropping.
  virtual bool GetDropFormats(int* formats,
                              std::set<ui::ClipboardFormatType>* format_types);

  // Override and return true if the data must be available before any drop
  // methods should be invoked. The default is false.
  virtual bool AreDropTypesRequired();

  // A view that supports drag and drop must override this and return true if
  // data contains a type that may be dropped on this view.
  virtual bool CanDrop(const OSExchangeData& data);

  // OnDragEntered is invoked when the mouse enters this view during a drag and
  // drop session and CanDrop returns true. This is immediately
  // followed by an invocation of OnDragUpdated, and eventually one of
  // OnDragExited or GetDropCallback.
  virtual void OnDragEntered(const ui::DropTargetEvent& event);

  // Invoked during a drag and drop session while the mouse is over the view.
  // This should return a bitmask of the DragDropTypes::DragOperation supported
  // based on the location of the event. Return 0 to indicate the drop should
  // not be accepted.
  virtual int OnDragUpdated(const ui::DropTargetEvent& event);

  // Invoked during a drag and drop session when the mouse exits the views, or
  // when the drag session was canceled and the mouse was over the view.
  virtual void OnDragExited();

  // Invoked from DoDrag after the drag completes. This implementation does
  // nothing, and is intended for subclasses to do cleanup.
  virtual void OnDragDone();

  // Invoked during a drag and drop session when OnDragUpdated returns a valid
  // operation and the user release the mouse but the drop is held because of
  // DataTransferPolicyController. When calling, ensure that the |event|
  // uses View local coordinates.
  virtual DropCallback GetDropCallback(const ui::DropTargetEvent& event);

  // Returns true if the mouse was dragged enough to start a drag operation.
  // delta_x and y are the distance the mouse was dragged.
  static bool ExceededDragThreshold(const gfx::Vector2d& delta);

  // Accessibility -------------------------------------------------------------

  // Get the object managing the accessibility interface for this View.
  ViewAccessibility& GetViewAccessibility() const;

  // Modifies `node_data` to reflect the current accessible state of this view.
  // It accomplishes this by keeping the data up-to-date in response to the use
  // of the accessible-property setters.
  // NOTE: View authors should use the available property setters rather than
  // overriding this function. Views which need to expose accessibility
  // properties which are currently not supported View properties should ensure
  // their view's `GetAccessibleNodeData` calls `GetAccessibleNodeData` on the
  // parent class. This ensures that if an owning view customizes an accessible
  // property, such as the name, role, or description, that customization is
  // included in your view's `AXNodeData`.
  virtual void GetAccessibleNodeData(ui::AXNodeData* node_data) {}

  // This method allows lazy loading of some accessibility attributes. It is
  // used only for accessibility attributes that can be expensive to compute
  // and/or heavy to store, such as long string attributes. Views that override
  // this method must not call the ViewAccessibility setters directly in the
  // function implementation, but instead should set the attributes directly on
  // the `data` object.
  //
  // Accessibility initialization happens once in the lifetime of a view: either
  // when accessibility usage is suddenly enabled or when the view is first
  // added to the views hierarchy after accessibility is enabled. This method
  // must only be called on attributes that haven't been set in the
  // ViewAccessibility cache before, otherwise it defeats the purpose of the
  // lazy loading.
  //
  // Here's an example of how to use this method:
  //
  // class MyView : public View {
  //  public:
  //  void MyView::OnAccessibilityInitializing(ui::AXNodeData* data) {
  //    std::string very_long_name = ComputeVeryLongName();
  //    data->SetName(very_long_name);
  //  }
  //  void MyView::OnNameChanged() {
  //    // Only set the expensive name when the view is initialized.
  //    if (GetViewAccessibility().is_initialized()) {
  //      GetViewAccessibility().SetName(ComputeVeryLongName());
  //    }
  //  }
  // };
  virtual void OnAccessibilityInitializing(ui::AXNodeData* data) {}

  // DEPRECATED: Use `ViewAccessibility::SetName` instead.
  //
  // Sets/gets the accessible name.
  // The value of the accessible name is a localized, end-user-consumable string
  // which may be derived from visible information (e.g. the text on a button)
  // or invisible information (e.g. the alternative text describing an icon).
  // In the case of focusable objects, the name will be presented by the screen
  // reader when that object gains focus and is critical to understanding the
  // purpose of that object non-visually.
  void SetAccessibleName(const std::u16string& name);

  // This function is deprecated. Use `ViewAccessibility::GetCachedName`
  // instead.
  std::u16string GetAccessibleName() const;

  // DEPRECATED: Use `ViewAccessibility::SetName` instead.
  //
  // Sets the accessible name to the specified string and source type.
  // To indicate that this view should never have an accessible name, e.g. to
  // prevent screen readers from speaking redundant information, set the type to
  // `kAttributeExplicitlyEmpty`. NOTE: Do not use `kAttributeExplicitlyEmpty`
  // on a view which may or may not have a name depending on circumstances. Also
  // please seek review from accessibility OWNERs when removing the name,
  // especially for views which are focusable or otherwise interactive.
  void SetAccessibleName(std::u16string name, ax::mojom::NameFrom name_from);

  // DEPRECATED: Use `ViewAccessibility::SetName` instead.
  //
  // Sets the accessible name of this view to that of `naming_view`. Often
  // `naming_view` is a `views::Label`, but any view with an accessible name
  // will work.
  void SetAccessibleName(View* naming_view);

  // DEPRECATED: Use ViewAccessibility::SetRole instead.
  // See https://crbug.com/324485311.
  //
  // Sets/gets the accessible role.
  void SetAccessibleRole(const ax::mojom::Role role);
  ax::mojom::Role GetAccessibleRole() const;

  // DEPRECATED: Use ViewAccessibility::SetRole instead.
  // See https://crbug.com/324485311.
  //
  // Sets the accessible role along with a customized string to be used by
  // assistive technologies to present the role. When there is no role
  // description provided, assisitive technologies will use either the default
  // role descriptions we provide (which are currently located in a number of
  // places. See crbug.com/1290866) or the value provided by their platform. As
  // a general rule, it is preferable to not override the role string. Please
  // seek review from accessibility OWNERs when using this function.
  void SetAccessibleRole(const ax::mojom::Role role,
                         const std::u16string& role_description);

  // DEPRECATED: Use ViewAccessibility::SetDescription instead.
  //
  // Sets/gets the accessible description string.
  void SetAccessibleDescription(const std::u16string& description);

  // DEPRECATED: Use ViewAccessibility::GetCachedDescription instead.
  std::u16string GetAccessibleDescription() const;

  // DEPRECATED: Use ViewAccessibility::SetDescription instead.
  //
  // Sets the accessible description to the specified string and source type.
  // To remove the description and prevent alternatives (such as tooltip text)
  // from being used, set the type to `kAttributeExplicitlyEmpty`
  void SetAccessibleDescription(const std::u16string& description,
                                ax::mojom::DescriptionFrom description_from);

  // DEPRECATED: Use ViewAccessibility::SetDescription instead.
  //
  // Sets the accessible description of this view to the accessible name of
  // `describing_view`. Often `describing_view` is a `views::Label`, but any
  // view with an accessible name will work.
  void SetAccessibleDescription(View* describing_view);

  // Handle a request from assistive technology to perform an action on this
  // view. Returns true on success, but note that the success/failure is
  // not propagated to the client that requested the action, since the
  // request is sometimes asynchronous. The right way to send a response is
  // via NotifyAccessibilityEvent(), below.
  virtual bool HandleAccessibleAction(const ui::AXActionData& action_data);

  // Returns an instance of the native accessibility interface for this view.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible();

  // DEPRECATED: Use `ViewAccessibility::NotifyEvent` instead.
  //
  // Notifies assistive technology that an accessibility event has
  // occurred on this view, such as when the view is focused or when its
  // value changes. Pass true for |send_native_event| except for rare
  // cases where the view is a native control that's already sending a
  // native accessibility event and the duplicate event would cause
  // problems.
  void NotifyAccessibilityEvent(ax::mojom::Event event_type,
                                bool send_native_event);

  // Views may override this function to know when an accessibility
  // event is fired. This will be called by NotifyAccessibilityEvent.
  virtual void OnAccessibilityEvent(ax::mojom::Event event_type);

  // Scrolling -----------------------------------------------------------------
  // TODO(beng): Figure out if this can live somewhere other than View, i.e.
  //             closer to ScrollView.

  // Scrolls the specified region, in this View's coordinate system, to be
  // visible. View's implementation passes the call onto the parent View (after
  // adjusting the coordinates). It is up to views that only show a portion of
  // the child view, such as Viewport, to override appropriately.
  virtual void ScrollRectToVisible(const gfx::Rect& rect);

  // Scrolls the view's bounds or some subset thereof to be visible. By default
  // this function calls ScrollRectToVisible(GetLocalBounds()).
  void ScrollViewToVisible();

  void AddObserver(ViewObserver* observer);
  void RemoveObserver(ViewObserver* observer);
  bool HasObserver(const ViewObserver* observer) const;

  // Called when the accessible name of the View changed.
  virtual void OnAccessibleNameChanged(const std::u16string& new_name) {}

  // Called by `SetAccessibleName` to allow subclasses to adjust the new name.
  // Potential use cases include setting the accessible name to the tooltip
  // text when the new name is empty and prepending/appending additional text
  // to the new name.
  virtual void AdjustAccessibleName(std::u16string& new_name,
                                    ax::mojom::NameFrom& name_from) {}

  // View Controller Interfaces -----------------------------------------------
  // These functions provide a common interface for view controllers to interact
  // with views.

  virtual std::unique_ptr<ActionViewInterface> GetActionViewInterface();

  // Registers a callback that can be used to notify a view controller of any
  // changes. This is more general than the property changed callbacks as view
  // controllers may need to recompute logic based on changes not captured by
  // view properties.
  base::CallbackListSubscription RegisterNotifyViewControllerCallback(
      base::RepeatingClosureList::CallbackType callback);

  void NotifyViewControllerCallback();

  // http://crbug.com/1162949 : Instrumentation that indicates if this is alive.
  // Callers should not depend on this as it is meant to be temporary.
  enum class LifeCycleState : uint32_t {
    kAlive = 0x600D600D,
    kDestroying = 0x90141013,
    kDestroyed = 0xBAADBAAD,
  };

  LifeCycleState life_cycle_state() const { return life_cycle_state_; }

 protected:
  // Used to track a drag. RootView passes this into
  // ProcessMousePressed/Dragged.
  struct DragInfo {
    // Sets possible_drag to false and start_x/y to 0. This is invoked by
    // RootView prior to invoke ProcessMousePressed.
    void Reset();

    // Sets possible_drag to true and start_pt to the specified point.
    // This is invoked by the target view if it detects the press may generate
    // a drag.
    void PossibleDrag(const gfx::Point& p);

    // Whether the press may generate a drag.
    bool possible_drag = false;

    // Coordinates of the mouse press.
    gfx::Point start_pt;
  };

  // Size and disposition ------------------------------------------------------

  // Calculates the preferred size for the View given `available_size`.
  // `preferred_size_` will take precedence over CalculatePreferredSize() if
  // it exists.
  virtual gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const;

  // Override to be notified when the bounds of the view have changed.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) {}

  // Called when the preferred size of a child view changed.  This gives the
  // parent an opportunity to do a fresh layout if that makes sense.
  virtual void ChildPreferredSizeChanged(View* child) {}

  // Called when the visibility of a child view changed.  This gives the parent
  // an opportunity to do a fresh layout if that makes sense.
  virtual void ChildVisibilityChanged(View* child) {}

  // Invalidates the layout and calls ChildPreferredSizeChanged() on the parent
  // if there is one. Be sure to call PreferredSizeChanged() when overriding
  // such that the layout is properly invalidated.
  virtual void PreferredSizeChanged();

  // Override returning true when the view needs to be notified when its visible
  // bounds relative to the root view may have changed. Only used by
  // NativeViewHost.
  virtual bool GetNeedsNotificationWhenVisibleBoundsChange() const;

  // Notification that this View's visible bounds relative to the root view may
  // have changed. The visible bounds are the region of the View not clipped by
  // its ancestors. This is used for clipping NativeViewHost.
  virtual void OnVisibleBoundsChanged();

  // Tree operations -----------------------------------------------------------

  // This method is invoked when the tree changes.
  //
  // When a view is removed, it is invoked for all children and grand
  // children. For each of these views, a notification is sent to the
  // view and all parents.
  //
  // When a view is added, a notification is sent to the view, all its
  // parents, and all its children (and grand children)
  //
  // Default implementation does nothing. Override to perform operations
  // required when a view is added or removed from a view hierarchy
  //
  // Refer to comments in struct |ViewHierarchyChangedDetails| for |details|.
  //
  // See also AddedToWidget() and RemovedFromWidget() for detecting when the
  // view is added to/removed from a widget.
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details);

  // When SetVisible() changes the visibility of a view, this method is
  // invoked for that view as well as all the children recursively.
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  // This method is invoked when the parent NativeView of the widget that the
  // view is attached to has changed and the view hierarchy has not changed.
  // ViewHierarchyChanged() is called when the parent NativeView of the widget
  // that the view is attached to is changed as a result of changing the view
  // hierarchy. Overriding this method is useful for tracking which
  // FocusManager manages this view.
  virtual void NativeViewHierarchyChanged();

  // This method is invoked for a view when it is attached to a hierarchy with
  // a widget, i.e. GetWidget() starts returning a non-null result.
  // It is also called when the view is moved to a different widget.
  virtual void AddedToWidget();

  // This method is invoked for a view when it is removed from a hierarchy with
  // a widget or moved to a different widget.
  virtual void RemovedFromWidget();

  // Painting ------------------------------------------------------------------

  // Override to control paint redirection or to provide a different Rectangle
  // |r| to be repainted. This is a function with an empty implementation in
  // view.cc and is purely intended for subclasses to override.
  virtual void OnDidSchedulePaint(const gfx::Rect& r);

  // Responsible for calling Paint() on child Views. Override to control the
  // order child Views are painted.
  virtual void PaintChildren(const PaintInfo& info);

  // Override to provide rendering in any part of the View's bounds. Typically
  // this is the "contents" of the view. If you override this method you will
  // have to call the subsequent OnPaint*() methods manually.
  virtual void OnPaint(gfx::Canvas* canvas);

  // Override to paint a background before any content is drawn. Typically this
  // is done if you are satisfied with a default OnPaint handler but wish to
  // supply a different background.
  virtual void OnPaintBackground(gfx::Canvas* canvas);

  // Override to paint a border not specified by SetBorder().
  virtual void OnPaintBorder(gfx::Canvas* canvas);

  // Returns the type of scaling to be done for this View. Override this to
  // change the default scaling type from |kScaleToFit|. You would want to
  // override this for a view and return |kScaleToScaleFactor| in cases where
  // scaling should cause no distortion. Such as in the case of an image or
  // an icon.
  virtual PaintInfo::ScaleType GetPaintScaleType() const;

  // Accelerated painting ------------------------------------------------------

  // Returns the offset from this view to the nearest ancestor with a layer. If
  // |layer_parent| is non-NULL it is set to the nearest ancestor with a layer.
  virtual LayerOffsetData CalculateOffsetToAncestorWithLayer(
      ui::Layer** layer_parent);

  // Updates the view's layer's parent. Called when a view is added to a view
  // hierarchy, responsible for parenting the view's layer to the enclosing
  // layer in the hierarchy.
  virtual void UpdateParentLayer();

  // If this view has a layer, the layer is reparented to |parent_layer| and its
  // bounds is set based on |point|. If this view does not have a layer, then
  // recurses through all children. This is used when adding a layer to an
  // existing view to make sure all descendants that have layers are parented to
  // the right layer.
  void MoveLayerToParent(ui::Layer* parent_layer,
                         const LayerOffsetData& offset_data);

  // Called to update the bounds of any child layers within this View's
  // hierarchy when something happens to the hierarchy.
  void UpdateChildLayerBounds(const LayerOffsetData& offset_data);

  // Overridden from ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnLayerTransformed(const gfx::Transform& old_transform,
                          ui::PropertyChangeReason reason) final;
  void OnLayerClipRectChanged(const gfx::Rect& old_rect,
                              ui::PropertyChangeReason reason) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // Finds the layer that this view paints to (it may belong to an ancestor
  // view), then reorders the immediate children of that layer to match the
  // order of the view tree.
  void ReorderLayers();

  // This reorders the immediate children of |*parent_layer| to match the
  // order of the view tree. Child layers which are owned by a view are
  // reordered so that they are below any child layers not owned by a view.
  // Widget::ReorderNativeViews() should be called to reorder any child layers
  // with an associated view. Widget::ReorderNativeViews() may reorder layers
  // below layers owned by a view.
  virtual void ReorderChildLayers(ui::Layer* parent_layer);

  // Notifies parents about a layer being created or destroyed in a child. An
  // example where a subclass may override this method is when it wants to clip
  // the child by adding its own layer.
  virtual void OnChildLayerChanged(View* child);

  // Layout --------------------------------------------------------------------

  // Invokes Layout() on a superclass on behalf of the subclass. This is to be
  // used only inside a Layout() override, where a subclass needs to do the
  // superclass portion of layout. Invoke like `LayoutSuperclass<SuperT>(this)`,
  // where SuperT is the relevant superclass type.
  template <typename Super, typename This>
    requires std::derived_from<Super, View> && std::derived_from<This, Super> &&
             (!std::same_as<Super, This>)
  void LayoutSuperclass(This* ptr) {
    CHECK(layout_allowed_);
    static_cast<Super*>(ptr)->Super::Layout(PassKey());
  }

  // Input ---------------------------------------------------------------------

  virtual DragInfo* GetDragInfo();

  // Focus ---------------------------------------------------------------------

  // Override to be notified when focus has changed either to or from this View.
  virtual void OnFocus();
  virtual void OnBlur();

  // Handle view focus/blur events for this view.
  void Focus();
  void Blur();

  // System events -------------------------------------------------------------

  // Called when either the UI theme or the NativeTheme associated with this
  // View changes. This is also called when the NativeTheme first becomes
  // available (after the view is added to a widget hierarchy). Overriding
  // allows individual Views to do special cleanup and processing (such as
  // dropping resource caches). To dispatch a theme changed notification, call
  // Widget::ThemeChanged().
  virtual void OnThemeChanged();

  // Tooltips ------------------------------------------------------------------

  // Views must invoke this when the tooltip text they are to display changes.
  void TooltipTextChanged();

  // Propagates UpdateTooltipForFocus() to the TooltipManager for the Widget.
  // This must be invoked whenever the focus changes in the View hierarchy.
  // Subclasses may override this to disable keyboard-based tooltips.
  virtual void UpdateTooltipForFocus();

  // Drag and drop -------------------------------------------------------------

  // These are cover methods that invoke the method of the same name on
  // the DragController. Subclasses may wish to override rather than install
  // a DragController.
  // See DragController for a description of these methods.
  virtual int GetDragOperations(const gfx::Point& press_pt);
  virtual void WriteDragData(const gfx::Point& press_pt, OSExchangeData* data);

  // Returns whether we're in the middle of a drag session that was initiated
  // by us.
  bool InDrag() const;

  // Returns how much the mouse needs to move in one direction to start a
  // drag. These methods cache in a platform-appropriate way. These values are
  // used by the public static method ExceededDragThreshold().
  static int GetHorizontalDragThreshold();
  static int GetVerticalDragThreshold();

  // PropertyHandler -----------------------------------------------------------

  // Note: you MUST call this base method from derived classes that override it
  // or else your class  will not properly register for ElementTrackerViews and
  // won't be available for interactive tests or in-product help/tutorials which
  // use that system.
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  // Property Support ----------------------------------------------------------

  void OnPropertyChanged(ui::metadata::PropertyKey property,
                         PropertyEffects property_effects);

 private:
  friend class internal::PreEventDispatchHandler;
  friend class internal::PostEventDispatchHandler;
  friend class internal::RootView;
  friend class internal::ScopedChildrenLock;
  friend class FocusManager;
  friend class ViewDebugWrapperImpl;
  friend class ViewLayerTest;
  friend class ViewLayerPixelCanvasTest;
  friend class ViewTestApi;
  friend class Widget;
  FRIEND_TEST_ALL_PREFIXES(ViewTest, PaintWithMovedViewUsesCache);
  FRIEND_TEST_ALL_PREFIXES(ViewTest, PaintWithMovedViewUsesCacheInRTL);
  FRIEND_TEST_ALL_PREFIXES(ViewTest, PaintWithUnknownInvalidation);

  // Painting  -----------------------------------------------------------------

  // Responsible for propagating SchedulePaint() to the view's layer. If there
  // is no associated layer, the requested paint rect is propagated up the
  // view hierarchy by calling this function on the parent view. Rectangle |r|
  // is in the view's coordinate system. The transformations are applied to it
  // to convert it into the parent coordinate system before propagating
  // SchedulePaint() up the view hierarchy. This function should NOT be directly
  // called. Instead call SchedulePaint() or SchedulePaintInRect(), which will
  // call into this as necessary.
  void SchedulePaintInRectImpl(const gfx::Rect& r);

  // Invoked before and after the bounds change to schedule painting the old and
  // new bounds.
  void SchedulePaintBoundsChanged(bool size_changed);

  // Schedules a paint on the parent View if it exists.
  void SchedulePaintOnParent();

  // Returns whether this view is eligible for painting, i.e. is visible and
  // nonempty.  Note that this does not behave like IsDrawn(), since it doesn't
  // check ancestors recursively; rather, it's used to prune subtrees of views
  // during painting.
  bool ShouldPaint() const;

  // Adjusts the transform of |recorder| in advance of painting.
  void SetUpTransformRecorderForPainting(
      const gfx::Vector2d& offset_from_parent,
      ui::TransformRecorder* recorder) const;

  // Recursively calls the painting method |func| on all non-layered children,
  // in Z order.
  void RecursivePaintHelper(void (View::*func)(const PaintInfo&),
                            const PaintInfo& info);

  // Invokes Paint() and, if necessary, PaintDebugRects().  Should be called
  // only on the root of a widget/layer.  PaintDebugRects() is invoked as a
  // separate pass, instead of being rolled into Paint(), so that siblings will
  // not obscure debug rects.
  void PaintFromPaintRoot(const ui::PaintContext& parent_context);

  // Draws a semitransparent rect to indicate the bounds of this view.
  // Recursively does the same for all children.  Invoked only with
  // --draw-view-bounds-rects.
  void PaintDebugRects(const PaintInfo& paint_info);

  // Tree operations -----------------------------------------------------------

  // Adds |view| as a child of this view at |index|.
  void AddChildViewAtImpl(View* view, size_t index);

  // Removes |view| from the hierarchy tree. If |update_tool_tip| is
  // true, the tooltip is updated. If |delete_removed_view| is true, the
  // view is also deleted (if it is parent owned). If |new_parent| is
  // not null, the remove is the result of AddChildView() to a new
  // parent. For this case, |new_parent| is the View that |view| is
  // going to be added to after the remove completes.
  void DoRemoveChildView(View* view,
                         bool update_tool_tip,
                         bool delete_removed_view,
                         View* new_parent);

  // Call ViewHierarchyChanged() for all child views and all parents.
  // |old_parent| is the original parent of the View that was removed.
  // If |new_parent| is not null, the View that was removed will be reparented
  // to |new_parent| after the remove operation.
  // If is_removed_from_widget is true, calls RemovedFromWidget for all
  // children.
  void PropagateRemoveNotifications(View* old_parent,
                                    View* new_parent,
                                    bool is_removed_from_widget);

  // Call ViewHierarchyChanged() for all children.
  // If is_added_to_widget is true, calls AddedToWidget for all children.
  void PropagateAddNotifications(const ViewHierarchyChangedDetails& details,
                                 bool is_added_to_widget);

  // Propagates NativeViewHierarchyChanged() notification through all the
  // children.
  void PropagateNativeViewHierarchyChanged();

  // Calls ViewHierarchyChanged() and notifies observers.
  void ViewHierarchyChangedImpl(const ViewHierarchyChangedDetails& details);

  // Size and disposition ------------------------------------------------------

  // Call VisibilityChanged() recursively for all children.
  void PropagateVisibilityNotifications(View* from, bool is_visible);

  // Registers/unregisters accelerators as necessary and calls
  // VisibilityChanged().
  void VisibilityChangedImpl(View* starting_from, bool is_visible);

  // Visible bounds notification registration.
  // When a view is added to a hierarchy, it and all its children are asked if
  // they need to be registered for "visible bounds within root" notifications
  // (see comment on OnVisibleBoundsChanged()). If they do, they are registered
  // with every ancestor between them and the root of the hierarchy.
  static void RegisterChildrenForVisibleBoundsNotification(View* view);
  static void UnregisterChildrenForVisibleBoundsNotification(View* view);
  void RegisterForVisibleBoundsNotification();
  void UnregisterForVisibleBoundsNotification();

  // Adds/removes view to the list of descendants that are notified any time
  // this views location and possibly size are changed.
  void AddDescendantToNotify(View* view);
  void RemoveDescendantToNotify(View* view);

  // Non-templatized backend for SetLayoutManager().
  void SetLayoutManagerImpl(std::unique_ptr<LayoutManager> layout);

  void SetToDefaultFillLayout();

  // Transformations -----------------------------------------------------------

  // Returns in |transform| the transform to get from coordinates of |ancestor|
  // to this. Returns true if |ancestor| is found. If |ancestor| is not found,
  // or NULL, |transform| is set to convert from root view coordinates to this.
  bool GetTransformRelativeTo(const View* ancestor,
                              gfx::Transform* transform) const;

  // Coordinate conversion -----------------------------------------------------

  // Converts a point in the view's coordinate to an ancestor view's coordinate
  // system using necessary transformations. Returns whether the point was
  // successfully converted to the ancestor's coordinate system.
  bool ConvertPointForAncestor(const View* ancestor, gfx::Point* point) const;

  // Converts a point in the ancestor's coordinate system to the view's
  // coordinate system using necessary transformations. Returns whether the
  // point was successfully converted from the ancestor's coordinate system
  // to the view's coordinate system.
  bool ConvertPointFromAncestor(const View* ancestor, gfx::Point* point) const;

  // Converts a rect in the view's coordinate to an ancestor view's coordinate
  // system using necessary transformations. Returns whether the rect was
  // successfully converted to the ancestor's coordinate system.
  bool ConvertRectForAncestor(const View* ancestor, gfx::RectF* rect) const;

  // Converts a rect in the ancestor's coordinate system to the view's
  // coordinate system using necessary transformations. Returns whether the
  // rect was successfully converted from the ancestor's coordinate system
  // to the view's coordinate system.
  bool ConvertRectFromAncestor(const View* ancestor, gfx::RectF* rect) const;

  // Accelerated painting ------------------------------------------------------

  // Creates the layer and related fields for this view.
  void CreateLayer(ui::LayerType layer_type);

  // Recursively calls UpdateParentLayers() on all descendants, stopping at any
  // Views that have layers. Calls UpdateParentLayer() for any Views that have
  // a layer with no parent. If at least one descendant had an unparented layer
  // true is returned.
  bool UpdateParentLayers();

  // Parents this view's layer to |parent_layer|, and sets its bounds and other
  // properties in accordance to the layer hierarchy.
  void ReparentLayer(ui::Layer* parent_layer);

  // Called to update the layer visibility. The layer will be visible if the
  // View itself, and all its parent Views are visible. This also updates
  // visibility of the child layers.
  void UpdateLayerVisibility();
  void UpdateChildLayerVisibility(bool visible);

  enum class LayerChangeNotifyBehavior {
    // Notify the parent chain about the layer change.
    NOTIFY,
    // Don't notify the parent chain about the layer change.
    DONT_NOTIFY
  };

  // Destroys the layer associated with this view, and reparents any descendants
  // to the destroyed layer's parent. If the view does not currently have a
  // layer, this has no effect.
  // The |notify_parents| enum controls whether a notification about the layer
  // change is sent to the parents.
  void DestroyLayerImpl(LayerChangeNotifyBehavior notify_parents);

  // Determines whether we need to be painting to a layer, checks whether we
  // currently have a layer, and creates or destroys the layer if necessary.
  void CreateOrDestroyLayer();

  // Notifies parents about layering changes in the view. This includes layer
  // creation and destruction.
  void NotifyParentsOfLayerChange();

  // Orphans the layers in this subtree that are parented to layers outside of
  // this subtree.
  void OrphanLayers();

  // Adjust the layer's offset so that it snaps to the physical pixel boundary.
  // This has no effect if the view does not have an associated layer.
  void SnapLayerToPixelBoundary(const LayerOffsetData& offset_data);

  // Sets the layer's bounds given in DIP coordinates.
  void SetLayerBounds(const gfx::Size& size_in_dip,
                      const LayerOffsetData& layer_offset_data);

  // Creates a mask layer for the current view using |clip_path_|.
  void CreateMaskLayer();

  // Implementation for adding a layer above or beneath the view layer. Called
  // from |AddLayerToRegion()|.
  void AddLayerToRegionImpl(
      ui::Layer* new_layer,
      std::vector<raw_ptr<ui::Layer, VectorExperimental>>& layer_vector);

  // Sets this view's layer and the layers above and below's parent to the given
  // parent_layer. This will also ensure the layers are added to the given
  // parent in the correct order.
  void SetLayerParent(ui::Layer* parent_layer);

  // Layout --------------------------------------------------------------------

  // Returns whether a layout is deferred to a layout manager, either the
  // default fill layout or the assigned layout manager.
  bool HasLayoutManager() const;

  // Implementation of synchronous layout. DeprecatedLayoutImmediately() is a
  // temporary public accessor to this; this is the access point for the few
  // blessed uses.
  void LayoutImmediately();

  // Input ---------------------------------------------------------------------

  bool ProcessMousePressed(const ui::MouseEvent& event);
  void ProcessMouseDragged(ui::MouseEvent* event);
  void ProcessMouseReleased(const ui::MouseEvent& event);

  // Accelerators --------------------------------------------------------------

  // Registers this view's keyboard accelerators that are not registered to
  // FocusManager yet, if possible.
  void RegisterPendingAccelerators();

  // Unregisters all the keyboard accelerators associated with this view.
  // |leave_data_intact| if true does not remove data from accelerators_ array,
  // so it could be re-registered with other focus manager
  void UnregisterAccelerators(bool leave_data_intact);

  // Focus ---------------------------------------------------------------------

  // Sets previous/next focusable views for both |view| and other children
  // assuming we've just inserted |view| at |pos|.
  void SetFocusSiblings(View* view, Views::const_iterator pos);

  // Helper function to advance focus, in case the currently focused view has
  // become unfocusable.
  void AdvanceFocusIfNecessary();

  // System events -------------------------------------------------------------

  // Used to propagate UI theme changed or NativeTheme changed notifications
  // from the root view to all views in the hierarchy.
  void PropagateThemeChanged();

  // Used to propagate device scale factor changed notifications from the root
  // view to all views in the hierarchy.
  void PropagateDeviceScaleFactorChanged(float old_device_scale_factor,
                                         float new_device_scale_factor);

  // Tooltips ------------------------------------------------------------------

  // Propagates UpdateTooltip() to the TooltipManager for the Widget.
  // This must be invoked any time the View hierarchy changes in such a way
  // the view under the mouse differs. For example, if the bounds of a View is
  // changed, this is invoked. Similarly, as Views are added/removed, this
  // is invoked.
  void UpdateTooltip();

  // Drag and drop -------------------------------------------------------------

  // Starts a drag and drop operation originating from this view. This invokes
  // WriteDragData to write the data and GetDragOperations to determine the
  // supported drag operations. When done, OnDragDone is invoked. |press_pt| is
  // in the view's coordinate system.
  // Returns true if a drag was started.
  bool DoDrag(const ui::LocatedEvent& event,
              const gfx::Point& press_pt,
              ui::mojom::DragEventSource source);

  // Property support ----------------------------------------------------------

  // Called from OnPropertyChanged with the given set of property effects. This
  // function is NOT called if effects == kPropertyEffectsNone.
  void HandlePropertyChangeEffects(PropertyEffects effects);

  // The following methods are used by the property access system described in
  // the comments above. They follow the required naming convention in order to
  // allow them to be visible via the metadata.
  int GetX() const;
  int GetY() const;
  int GetWidth() const;
  int GetHeight() const;
  void SetWidth(int width);
  void SetHeight(int height);
  bool GetIsDrawn() const;

  // Special property accessor used by metadata to get the ToolTip text.
  std::u16string GetTooltip() const;

  //////////////////////////////////////////////////////////////////////////////

  // Observers -----------------------------------------------------------------

  base::ObserverList<ViewObserver>::Unchecked observers_;

  // Creation and lifetime -----------------------------------------------------

  // False if this View is owned by its parent - i.e. it will be deleted by its
  // parent during its parents destruction. False is the default.
  bool owned_by_client_ = false;

  // http://crbug.com/1162949 : Instrumentation that indicates if this is alive.
  LifeCycleState life_cycle_state_ = LifeCycleState::kAlive;

  // Attributes ----------------------------------------------------------------

  // The id of this View. Used to find this View.
  int id_ = 0;

  // The group of this view. Some view subclasses use this id to find other
  // views of the same group. For example radio button uses this information
  // to find other radio buttons.
  int group_ = -1;

  // Tree operations -----------------------------------------------------------

  // This view's parent.
  raw_ptr<View> parent_ = nullptr;

  // This view's children.
  Views children_;

#if DCHECK_IS_ON()
  // True while iterating over |children_|. Used to detect and DCHECK when
  // |children_| is mutated during iteration.
  mutable bool iterating_ = false;
#endif

  bool can_process_events_within_subtree_ = true;

  // Size and disposition ------------------------------------------------------

  std::optional<gfx::Size> preferred_size_;

  // This View's bounds in the parent coordinate system.
  gfx::Rect bounds_;

  // Whether this view is visible.
  bool visible_ = true;

  // Whether this view is enabled.
  bool enabled_ = true;

  // When this flag is on, a View receives a mouse-enter and mouse-leave event
  // even if a descendant View is the event-recipient for the real mouse
  // events. When this flag is turned on, and mouse moves from outside of the
  // view into a child view, both the child view and this view receives
  // mouse-enter event. Similarly, if the mouse moves from inside a child view
  // and out of this view, then both views receive a mouse-leave event.
  // When this flag is turned off, if the mouse moves from inside this view into
  // a child view, then this view receives a mouse-leave event. When this flag
  // is turned on, it does not receive the mouse-leave event in this case.
  // When the mouse moves from inside the child view out of the child view but
  // still into this view, this view receives a mouse-enter event if this flag
  // is turned off, but doesn't if this flag is turned on.
  // This flag is initialized to false.
  bool notify_enter_exit_on_child_ = false;

  // Whether or not RegisterViewForVisibleBoundsNotification on the RootView
  // has been invoked.
  bool registered_for_visible_bounds_notification_ = false;

  // List of descendants wanting notification when their visible bounds change.
  std::unique_ptr<Views> descendants_to_notify_;

  // Transformations -----------------------------------------------------------

  // Painting will be clipped to this path.
  SkPath clip_path_;

  // Layout --------------------------------------------------------------------

  // Whether the view needs to be laid out.
  bool needs_layout_ = true;

  // Whether Layout() access is currently legal. This is used to prevent calls
  // to LayoutSuperclass() outside the implementation of Layout().
  bool layout_allowed_ = false;

  // Whether this view is in the middle of InvalidateLayout().
  bool invalidating_ = false;

  // Whether the layout manager requires constrained space.
  //
  // TODO(crbug.com/40232718): All layout management needs to respect the
  // available space. But there are some problems with `FlexLayout`. After we
  // fix the problem with FlexLayout. Remove this.
  bool layout_manager_use_constrained_space_ = true;

  // Used to generate an UMA metric for the maximum reentrant call depth seen
  // during layout. Normally the metric value will be one (Layout() was not
  // reentered). But, we know Layout() is reentered at least sometimes and
  // want to measure how often that is. We also want to know if it is ever
  // reentered more than two deep.
  int max_layout_call_depth_ = 0;

  // Current Layout() reentrant call depth (used to help determine the
  // max_layout_call_depth_, above).
  int current_layout_call_depth_ = 0;

  // How many times this view has done layout since the last time it was
  // painted. This is used to compute metrics around unnecessary layout calls.
  int layouts_since_last_paint_ = 0;

  // How many times InvalidateLayout() is called during a Layout() call.
  // This should never be necessary, but we don't yet know how often
  // it is happening.
  int invalidates_during_layout_ = 0;

  // The View's LayoutManager defines the sizing heuristics applied to child
  // Views. The default is absolute positioning according to bounds_.
  std::unique_ptr<LayoutManager> layout_manager_;

  // Having UseDefaultFillLayout true by default wreaks a bit of havoc right
  // now, so it is false for the time being. Once the various sites which
  // currently use FillLayout are converted to using this and the other places
  // that either override Layout() or do nothing are also validated, this can
  // be switched to true.
  static constexpr bool kUseDefaultFillLayout = false;

  // Is the default "fill" layout manager active? Setting this to true via
  // SetUseDefaultFillLayout() will set |layout_manager_| to a FillLayout. Call
  // SetLayoutManager(layout_manager) to override. If this is true and
  // SetLayoutManager(nullptr) is called, |layout_manager_| be set back to a
  // FillLayout.
  bool use_default_fill_layout_ = kUseDefaultFillLayout;
  bool has_default_fill_layout_ = false;

  // Whether this View's layer should be snapped to the pixel boundary.
  bool snap_layer_to_pixel_boundary_ = false;

  // Painting ------------------------------------------------------------------

  // Border.
  std::unique_ptr<Border> border_;

  // Background may rely on Border, so it must be declared last and destroyed
  // first.
  std::unique_ptr<Background> background_;

  // Cached output of painting to be reused in future frames until invalidated.
  ui::PaintCache paint_cache_;

  // Whether SchedulePaintInRect() was invoked on this View.
  bool needs_paint_ = false;

  // RTL painting --------------------------------------------------------------

  // Indicates whether or not the gfx::Canvas object passed to Paint() is going
  // to be flipped horizontally (using the appropriate transform) on
  // right-to-left locales for this View.
  bool flip_canvas_on_paint_for_rtl_ui_ = false;

  // Controls whether GetTransform(), the mirroring functions, and the like
  // horizontally mirror. This controls how child views are physically
  // positioned onscreen. The default behavior should be correct in most cases,
  // but can be overridden if a particular view must always be laid out in some
  // direction regardless of the application's default UI direction.
  std::optional<bool> is_mirrored_;

  // Accelerated painting ------------------------------------------------------

  // Whether layer painting was explicitly set by a call to |SetPaintToLayer()|.
  bool paint_to_layer_explicitly_set_ = false;

  // Whether we are painting to a layer because of a non-identity transform.
  bool paint_to_layer_for_transform_ = false;

  // Set of layers that should be painted above and beneath this View's layer.
  // These layers are maintained as siblings of this View's layer and are
  // stacked above and beneath, respectively.
  std::vector<raw_ptr<ui::Layer, VectorExperimental>> layers_above_;
  std::vector<raw_ptr<ui::Layer, VectorExperimental>> layers_below_;

  // If painting to a layer |mask_layer_| will mask the current layer and all
  // child layers to within the |clip_path_|.
  std::unique_ptr<views::ViewMaskLayer> mask_layer_;

  // Accelerators --------------------------------------------------------------

  // Focus manager accelerators registered on.
  raw_ptr<FocusManager> accelerator_focus_manager_ = nullptr;

  // The list of accelerators. List elements in the range
  // [0, registered_accelerator_count_) are already registered to FocusManager,
  // and the rest are not yet.
  std::unique_ptr<std::vector<ui::Accelerator>> accelerators_;
  size_t registered_accelerator_count_ = 0;

  // Focus ---------------------------------------------------------------------

  // Next view to be focused when the Tab key is pressed.
  raw_ptr<View> next_focusable_view_ = nullptr;

  // Next view to be focused when the Shift-Tab key combination is pressed.
  raw_ptr<View> previous_focusable_view_ = nullptr;

  // The focus behavior of the view in regular and accessibility mode.
  FocusBehavior focus_behavior_ = FocusBehavior::NEVER;

  // By default, we should show tooltips when a View is focused via a
  // key event. For testing purposes, we may not want that behavior.
  // This is controlled by DisableKeyboardTooltipsForTesting() and
  // EnableKeyboardTooltipsForTesting(), above.
  static bool kShouldDisableKeyboardTooltipsForTesting;

  // This is set when focus events should be skipped after focus reaches this
  // View.
  bool suppress_default_focus_handling_ = false;

  // Context menus -------------------------------------------------------------

  // The menu controller.
  raw_ptr<ContextMenuController, DanglingUntriaged> context_menu_controller_ =
      nullptr;

  // Drag and drop -------------------------------------------------------------

  raw_ptr<DragController> drag_controller_ = nullptr;

  // Input  --------------------------------------------------------------------

  std::unique_ptr<ViewTargeter> targeter_;

  // System events -------------------------------------------------------------

#if DCHECK_IS_ON()
  bool on_theme_changed_called_ = false;
#endif

  // Accessibility -------------------------------------------------------------

  // Manages the accessibility interface for this View. Some ViewAccessibility
  // implementations are `ViewObserver`s, so this must be ordered after
  // `observers_`.
  mutable std::unique_ptr<ViewAccessibility> view_accessibility_;

  // Keeps track of whether accessibility checks for this View have run yet.
  // They run once inside ::OnPaint() to keep overhead low. The idea is that if
  // a View is ready to paint it should also be set up to be accessible.
  bool has_run_accessibility_paint_checks_ = false;

  // View Controller Interfaces
  base::RepeatingClosureList notify_view_controller_callback_list_;
};

namespace internal {

#if DCHECK_IS_ON()
class ScopedChildrenLock {
 public:
  explicit ScopedChildrenLock(const View* view);

  ScopedChildrenLock(const ScopedChildrenLock&) = delete;
  ScopedChildrenLock& operator=(const ScopedChildrenLock&) = delete;

  ~ScopedChildrenLock();

 private:
  base::AutoReset<bool> reset_;
};
#else
class ScopedChildrenLock {
 public:
  explicit ScopedChildrenLock(const View* view);
  ~ScopedChildrenLock();
};
#endif

}  // namespace internal

class VIEWS_EXPORT BaseActionViewInterface : public ActionViewInterface {
 public:
  explicit BaseActionViewInterface(View* action_view);
  ~BaseActionViewInterface() override = default;
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<View> action_view_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, View, BaseView)
template <typename LayoutManager>
BuilderT& SetLayoutManager(std::unique_ptr<LayoutManager> layout_manager) & {
  auto setter = std::make_unique<::views::internal::PropertySetter<
      ViewClass_, std::unique_ptr<LayoutManager>,
      decltype((static_cast<LayoutManager* (
                    ViewClass_::*)(std::unique_ptr<LayoutManager>)>(
          &ViewClass_::SetLayoutManager))),
      &ViewClass_::SetLayoutManager>>(std::move(layout_manager));
  ::views::internal::ViewBuilderCore::AddPropertySetter(std::move(setter));
  return *static_cast<BuilderT*>(this);
}
template <typename LayoutManager>
BuilderT&& SetLayoutManager(std::unique_ptr<LayoutManager> layout_manager) && {
  return std::move(this->SetLayoutManager(std::move(layout_manager)));
}

VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleName, const std::u16string&)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleName, View*)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleName,
                             std::u16string,
                             ax::mojom::NameFrom)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleDescription, const std::u16string&)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleDescription, View*)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleDescription,
                             const std::u16string&,
                             ax::mojom::DescriptionFrom)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleRole, ax::mojom::Role)
VIEW_BUILDER_OVERLOAD_METHOD(SetAccessibleRole,
                             ax::mojom::Role,
                             const std::u16string&)
VIEW_BUILDER_PROPERTY(std::unique_ptr<Background>, Background)
VIEW_BUILDER_PROPERTY(std::unique_ptr<Border>, Border)
VIEW_BUILDER_PROPERTY(gfx::Rect, BoundsRect)
VIEW_BUILDER_PROPERTY(gfx::Size, Size)
VIEW_BUILDER_PROPERTY(gfx::Point, Position)
VIEW_BUILDER_PROPERTY(int, X)
VIEW_BUILDER_PROPERTY(int, Y)
VIEW_BUILDER_PROPERTY(gfx::Size, PreferredSize)
VIEW_BUILDER_PROPERTY(SkPath, ClipPath)
VIEW_BUILDER_PROPERTY_DEFAULT(ui::LayerType, PaintToLayer, ui::LAYER_TEXTURED)
VIEW_BUILDER_PROPERTY(bool, Enabled)
VIEW_BUILDER_PROPERTY(bool, FlipCanvasOnPaintForRTLUI)
VIEW_BUILDER_PROPERTY(views::View::FocusBehavior, FocusBehavior)
VIEW_BUILDER_PROPERTY(int, Group)
VIEW_BUILDER_PROPERTY(int, ID)
VIEW_BUILDER_PROPERTY(bool, Mirrored)
VIEW_BUILDER_PROPERTY(bool, NotifyEnterExitOnChild)
VIEW_BUILDER_PROPERTY(gfx::Transform, Transform)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
VIEW_BUILDER_PROPERTY(bool, UseDefaultFillLayout)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, View)

#endif  // UI_VIEWS_VIEW_H_
