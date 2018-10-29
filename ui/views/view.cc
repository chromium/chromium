// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/clip_recorder.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/transform_recorder.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "ui/native_theme/native_theme_win.h"
#endif

namespace views {

namespace {

#if defined(OS_WIN)
const bool kContextMenuOnMousePress = false;
#else
const bool kContextMenuOnMousePress = true;
#endif

// Default horizontal drag threshold in pixels.
// Same as what gtk uses.
const int kDefaultHorizontalDragThreshold = 8;

// Default vertical drag threshold in pixels.
// Same as what gtk uses.
const int kDefaultVerticalDragThreshold = 8;

// Returns the top view in |view|'s hierarchy.
const View* GetHierarchyRoot(const View* view) {
  const View* root = view;
  while (root && root->parent())
    root = root->parent();
  return root;
}

}  // namespace

namespace internal {

#if DCHECK_IS_ON()
class ScopedChildrenLock {
 public:
  explicit ScopedChildrenLock(const View* view)
      : reset_(&view->iterating_, true) {}
  ~ScopedChildrenLock() {}

 private:
  base::AutoReset<bool> reset_;
  DISALLOW_COPY_AND_ASSIGN(ScopedChildrenLock);
};
#else
class ScopedChildrenLock {
 public:
  explicit ScopedChildrenLock(const View* view) {}
  ~ScopedChildrenLock() {}
};
#endif

}  // namespace internal

// static
const char View::kViewClassName[] = "View";

////////////////////////////////////////////////////////////////////////////////
// View, public:

// Creation and lifetime -------------------------------------------------------

View::View()
    : owned_by_client_(false),
      id_(0),
      group_(-1),
      parent_(nullptr),
#if DCHECK_IS_ON()
      iterating_(false),
#endif
      can_process_events_within_subtree_(true),
      visible_(true),
      enabled_(true),
      notify_enter_exit_on_child_(false),
      registered_for_visible_bounds_notification_(false),
      needs_layout_(true),
      snap_layer_to_pixel_boundary_(false),
      flip_canvas_on_paint_for_rtl_ui_(false),
      paint_to_layer_(false),
      accelerator_focus_manager_(nullptr),
      registered_accelerator_count_(0),
      next_focusable_view_(nullptr),
      previous_focusable_view_(nullptr),
      focus_behavior_(FocusBehavior::NEVER),
      context_menu_controller_(nullptr),
      drag_controller_(nullptr) {
  SetTargetHandler(this);
}

View::~View() {
  if (parent_)
    parent_->RemoveChildView(this);

  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_) {
      child->parent_ = nullptr;
      if (!child->owned_by_client_)
        delete child;
    }
  }

  for (ViewObserver& observer : observers_)
    observer.OnViewIsDeleting(this);
}

// Tree operations -------------------------------------------------------------

const Widget* View::GetWidget() const {
  // The root view holds a reference to this view hierarchy's Widget.
  return parent_ ? parent_->GetWidget() : nullptr;
}

Widget* View::GetWidget() {
  return const_cast<Widget*>(const_cast<const View*>(this)->GetWidget());
}

void View::AddChildView(View* view) {
  if (view->parent_ == this)
    return;
  AddChildViewAt(view, child_count());
}

void View::AddChildViewAt(View* view, int index) {
  CHECK_NE(view, this) << "You cannot add a view as its own child";
  DCHECK_GE(index, 0);
  DCHECK_LE(index, child_count());

  // If |view| has a parent, remove it from its parent.
  View* parent = view->parent_;
  ui::NativeTheme* old_theme = nullptr;
  Widget* old_widget = nullptr;
  if (parent) {
    old_theme = view->GetNativeTheme();
    old_widget = view->GetWidget();
    if (parent == this) {
      ReorderChildView(view, index);
      return;
    }
    parent->DoRemoveChildView(view, true, true, false, this);
  }

  // Sets the prev/next focus views.
  InitFocusSiblings(view, index);

  view->parent_ = this;
#if DCHECK_IS_ON()
  DCHECK(!iterating_);
#endif
  children_.insert(children_.begin() + index, view);

  // Ensure the layer tree matches the view tree before calling to any client
  // code. This way if client code further modifies the view tree we are in a
  // sane state.
  const bool did_reparent_any_layers = view->UpdateParentLayers();
  Widget* widget = GetWidget();
  if (did_reparent_any_layers && widget)
    widget->LayerTreeChanged();

  ReorderLayers();

  // Make sure the visibility of the child layers are correct.
  // If any of the parent View is hidden, then the layers of the subtree
  // rooted at |this| should be hidden. Otherwise, all the child layers should
  // inherit the visibility of the owner View.
  view->UpdateLayerVisibility();

  if (widget) {
    const ui::NativeTheme* new_theme = view->GetNativeTheme();
    if (new_theme != old_theme)
      view->PropagateNativeThemeChanged(new_theme);
  }

  ViewHierarchyChangedDetails details(true, this, view, parent);

  for (View* v = this; v; v = v->parent_)
    v->ViewHierarchyChangedImpl(false, details);

  view->PropagateAddNotifications(details, widget && widget != old_widget);

  UpdateTooltip();

  if (widget) {
    RegisterChildrenForVisibleBoundsNotification(view);

    if (view->visible())
      view->SchedulePaint();
  }

  if (layout_manager_.get())
    layout_manager_->ViewAdded(this, view);

  for (ViewObserver& observer : observers_)
    observer.OnChildViewAdded(this, view);
}

void View::ReorderChildView(View* view, int index) {
  DCHECK_EQ(view->parent_, this);
  if (index < 0)
    index = child_count() - 1;
  else if (index >= child_count())
    return;
  if (children_[index] == view)
    return;

  const Views::iterator i(std::find(children_.begin(), children_.end(), view));
  DCHECK(i != children_.end());
#if DCHECK_IS_ON()
  DCHECK(!iterating_);
#endif
  children_.erase(i);

  // Unlink the view first
  View* next_focusable = view->next_focusable_view_;
  View* prev_focusable = view->previous_focusable_view_;
  if (prev_focusable)
    prev_focusable->next_focusable_view_ = next_focusable;
  if (next_focusable)
    next_focusable->previous_focusable_view_ = prev_focusable;

  // Add it in the specified index now.
  InitFocusSiblings(view, index);
  children_.insert(children_.begin() + index, view);

  for (ViewObserver& observer : observers_)
    observer.OnChildViewReordered(this, view);

  ReorderLayers();
}

void View::RemoveChildView(View* view) {
  DoRemoveChildView(view, true, true, false, nullptr);
}

void View::RemoveAllChildViews(bool delete_children) {
  while (!children_.empty())
    DoRemoveChildView(children_.front(), false, false, delete_children,
                      nullptr);
  UpdateTooltip();
}

bool View::Contains(const View* view) const {
  for (const View* v = view; v; v = v->parent_) {
    if (v == this)
      return true;
  }
  return false;
}

int View::GetIndexOf(const View* view) const {
  auto i(std::find(children_.begin(), children_.end(), view));
  return i != children_.end() ? static_cast<int>(i - children_.begin()) : -1;
}

// Size and disposition --------------------------------------------------------

void View::SetBounds(int x, int y, int width, int height) {
  SetBoundsRect(gfx::Rect(x, y, std::max(0, width), std::max(0, height)));
}

void View::SetBoundsRect(const gfx::Rect& bounds) {
  if (bounds == bounds_) {
    if (needs_layout_) {
      needs_layout_ = false;
      TRACE_EVENT1("views", "View::Layout(set_bounds)", "class",
                   GetClassName());
      Layout();
    }
    return;
  }

  if (visible_) {
    // Paint where the view is currently.
    SchedulePaintBoundsChanged(
        bounds_.size() == bounds.size() ? SCHEDULE_PAINT_SIZE_SAME :
        SCHEDULE_PAINT_SIZE_CHANGED);
  }

  gfx::Rect prev = bounds_;
  bounds_ = bounds;
  BoundsChanged(prev);

  for (ViewObserver& observer : observers_)
    observer.OnViewBoundsChanged(this);
}

void View::SetSize(const gfx::Size& size) {
  SetBounds(x(), y(), size.width(), size.height());
}

void View::SetPosition(const gfx::Point& position) {
  SetBounds(position.x(), position.y(), width(), height());
}

void View::SetX(int x) {
  SetBounds(x, y(), width(), height());
}

void View::SetY(int y) {
  SetBounds(x(), y, width(), height());
}

gfx::Rect View::GetContentsBounds() const {
  gfx::Rect contents_bounds(GetLocalBounds());
  if (border_.get())
    contents_bounds.Inset(border_->GetInsets());
  return contents_bounds;
}

gfx::Rect View::GetLocalBounds() const {
  return gfx::Rect(size());
}

gfx::Insets View::GetInsets() const {
  return border_.get() ? border_->GetInsets() : gfx::Insets();
}

gfx::Rect View::GetVisibleBounds() const {
  if (!IsDrawn())
    return gfx::Rect();
  gfx::Rect vis_bounds(GetLocalBounds());
  gfx::Rect ancestor_bounds;
  const View* view = this;
  gfx::Transform transform;

  while (view != nullptr && !vis_bounds.IsEmpty()) {
    transform.ConcatTransform(view->GetTransform());
    gfx::Transform translation;
    translation.Translate(static_cast<float>(view->GetMirroredX()),
                          static_cast<float>(view->y()));
    transform.ConcatTransform(translation);

    vis_bounds = view->ConvertRectToParent(vis_bounds);
    const View* ancestor = view->parent_;
    if (ancestor != nullptr) {
      ancestor_bounds.SetRect(0, 0, ancestor->width(), ancestor->height());
      vis_bounds.Intersect(ancestor_bounds);
    } else if (!view->GetWidget()) {
      // If the view has no Widget, we're not visible. Return an empty rect.
      return gfx::Rect();
    }
    view = ancestor;
  }
  if (vis_bounds.IsEmpty())
    return vis_bounds;
  // Convert back to this views coordinate system.
  gfx::RectF views_vis_bounds(vis_bounds);
  transform.TransformRectReverse(&views_vis_bounds);
  // Partially visible pixels should be considered visible.
  return gfx::ToEnclosingRect(views_vis_bounds);
}

gfx::Rect View::GetBoundsInScreen() const {
  gfx::Point origin;
  View::ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, size());
}

gfx::Rect View::GetAnchorBoundsInScreen() const {
  return GetBoundsInScreen();
}

gfx::Size View::GetPreferredSize() const {
  if (preferred_size_)
    return *preferred_size_;
  return CalculatePreferredSize();
}

int View::GetBaseline() const {
  return -1;
}

void View::SetPreferredSize(const gfx::Size& size) {
  if (preferred_size_ && *preferred_size_ == size)
    return;

  preferred_size_ = size;
  PreferredSizeChanged();
}

void View::SizeToPreferredSize() {
  SetSize(GetPreferredSize());
}

gfx::Size View::GetMinimumSize() const {
  return GetPreferredSize();
}

gfx::Size View::GetMaximumSize() const {
  return gfx::Size();
}

int View::GetHeightForWidth(int w) const {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredHeightForWidth(this, w);
  return GetPreferredSize().height();
}

void View::SetVisible(bool visible) {
  if (visible != visible_) {
    // If the View is currently visible, schedule paint to refresh parent.
    // TODO(beng): not sure we should be doing this if we have a layer.
    if (visible_)
      SchedulePaint();

    visible_ = visible;
    AdvanceFocusIfNecessary();

    // Notify the parent.
    if (parent_) {
      parent_->ChildVisibilityChanged(this);
      parent_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                        false);
    }

    for (ViewObserver& observer : observers_)
      observer.OnViewVisibilityChanged(this);

    // This notifies all sub-views recursively.
    PropagateVisibilityNotifications(this, visible_);
    UpdateLayerVisibility();

    // If we are newly visible, schedule paint.
    if (visible_)
      SchedulePaint();
  }
}

bool View::IsDrawn() const {
  return visible_ && parent_ ? parent_->IsDrawn() : false;
}

void View::SetEnabled(bool enabled) {
  if (enabled != enabled_) {
    enabled_ = enabled;
    AdvanceFocusIfNecessary();

    OnEnabledChanged();

    for (ViewObserver& observer : observers_)
      observer.OnViewEnabledChanged(this);
  }
}

void View::OnEnabledChanged() {
  SchedulePaint();
}

View::Views View::GetChildrenInZOrder() {
  return children_;
}

// Transformations -------------------------------------------------------------

gfx::Transform View::GetTransform() const {
  if (!layer())
    return gfx::Transform();

  gfx::Transform transform = layer()->transform();
  gfx::ScrollOffset scroll_offset = layer()->CurrentScrollOffset();
  // Offsets for layer-based scrolling are never negative, but the horizontal
  // scroll direction is reversed in RTL via canvas flipping.
  transform.Translate((base::i18n::IsRTL() ? 1 : -1) * scroll_offset.x(),
                      -scroll_offset.y());
  return transform;
}

void View::SetTransform(const gfx::Transform& transform) {
  if (transform.IsIdentity()) {
    if (layer()) {
      layer()->SetTransform(transform);
      if (!paint_to_layer_)
        DestroyLayer();
    } else {
      // Nothing.
    }
  } else {
    if (!layer())
      CreateLayer(ui::LAYER_TEXTURED);
    layer()->SetTransform(transform);
    layer()->ScheduleDraw();
  }
}

void View::SetPaintToLayer(ui::LayerType layer_type) {
  if (paint_to_layer_ && (layer()->type() == layer_type))
    return;

  DestroyLayerImpl(LayerChangeNotifyBehavior::DONT_NOTIFY);
  CreateLayer(layer_type);
  paint_to_layer_ = true;

  // Notify the parent chain about the layer change.
  NotifyParentsOfLayerChange();
}

void View::DestroyLayer() {
  DestroyLayerImpl(LayerChangeNotifyBehavior::NOTIFY);
}

std::unique_ptr<ui::Layer> View::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer = LayerOwner::RecreateLayer();
  Widget* widget = GetWidget();
  if (widget)
    widget->LayerTreeChanged();
  return old_layer;
}

// RTL positioning -------------------------------------------------------------

gfx::Rect View::GetMirroredBounds() const {
  gfx::Rect bounds(bounds_);
  bounds.set_x(GetMirroredX());
  return bounds;
}

gfx::Point View::GetMirroredPosition() const {
  return gfx::Point(GetMirroredX(), y());
}

int View::GetMirroredX() const {
  return parent_ ? parent_->GetMirroredXForRect(bounds_) : x();
}

int View::GetMirroredXForRect(const gfx::Rect& rect) const {
  return base::i18n::IsRTL() ? (width() - rect.x() - rect.width()) : rect.x();
}

gfx::Rect View::GetMirroredRect(const gfx::Rect& rect) const {
  gfx::Rect mirrored_rect = rect;
  mirrored_rect.set_x(GetMirroredXForRect(rect));
  return mirrored_rect;
}

int View::GetMirroredXInView(int x) const {
  return base::i18n::IsRTL() ? width() - x : x;
}

int View::GetMirroredXWithWidthInView(int x, int w) const {
  return base::i18n::IsRTL() ? width() - x - w : x;
}

// Layout ----------------------------------------------------------------------

void View::Layout() {
  needs_layout_ = false;

  // If we have a layout manager, let it handle the layout for us.
  if (layout_manager_.get())
    layout_manager_->Layout(this);

  // Make sure to propagate the Layout() call to any children that haven't
  // received it yet through the layout manager and need to be laid out. This
  // is needed for the case when the child requires a layout but its bounds
  // weren't changed by the layout manager. If there is no layout manager, we
  // just propagate the Layout() call down the hierarchy, so whoever receives
  // the call can take appropriate action.
  internal::ScopedChildrenLock lock(this);
  for (auto* child : children_) {
    if (child->needs_layout_ || !layout_manager_.get()) {
      TRACE_EVENT1("views", "View::Layout", "class", child->GetClassName());
      child->needs_layout_ = false;
      child->Layout();
    }
  }
}

void View::InvalidateLayout() {
  // Always invalidate up. This is needed to handle the case of us already being
  // valid, but not our parent.
  needs_layout_ = true;
  if (parent_)
    parent_->InvalidateLayout();
}

LayoutManager* View::GetLayoutManager() const {
  return layout_manager_.get();
}

void View::SetLayoutManager(std::nullptr_t) {
  SetLayoutManagerImpl(nullptr);
}

// Attributes ------------------------------------------------------------------

const char* View::GetClassName() const {
  return kViewClassName;
}

const View* View::GetAncestorWithClassName(const std::string& name) const {
  for (const View* view = this; view; view = view->parent_) {
    if (!strcmp(view->GetClassName(), name.c_str()))
      return view;
  }
  return nullptr;
}

View* View::GetAncestorWithClassName(const std::string& name) {
  return const_cast<View*>(const_cast<const View*>(this)->
      GetAncestorWithClassName(name));
}

const View* View::GetViewByID(int id) const {
  if (id == id_)
    return const_cast<View*>(this);

  internal::ScopedChildrenLock lock(this);
  for (auto* child : children_) {
    const View* view = child->GetViewByID(id);
    if (view)
      return view;
  }
  return nullptr;
}

View* View::GetViewByID(int id) {
  return const_cast<View*>(const_cast<const View*>(this)->GetViewByID(id));
}

void View::SetGroup(int gid) {
  // Don't change the group id once it's set.
  DCHECK(group_ == -1 || group_ == gid);
  group_ = gid;
}

int View::GetGroup() const {
  return group_;
}

bool View::IsGroupFocusTraversable() const {
  return true;
}

void View::GetViewsInGroup(int group, Views* views) {
  if (group_ == group)
    views->push_back(this);

  internal::ScopedChildrenLock lock(this);
  for (auto* child : children_)
    child->GetViewsInGroup(group, views);
}

View* View::GetSelectedViewForGroup(int group) {
  Views views;
  GetWidget()->GetRootView()->GetViewsInGroup(group, &views);
  return views.empty() ? nullptr : views[0];
}

// Coordinate conversion -------------------------------------------------------

// static
void View::ConvertPointToTarget(const View* source,
                                const View* target,
                                gfx::Point* point) {
  DCHECK(source);
  DCHECK(target);
  if (source == target)
    return;

  const View* root = GetHierarchyRoot(target);
  CHECK_EQ(GetHierarchyRoot(source), root);

  if (source != root)
    source->ConvertPointForAncestor(root, point);

  if (target != root)
    target->ConvertPointFromAncestor(root, point);
}

// static
void View::ConvertRectToTarget(const View* source,
                               const View* target,
                               gfx::RectF* rect) {
  DCHECK(source);
  DCHECK(target);
  if (source == target)
    return;

  const View* root = GetHierarchyRoot(target);
  CHECK_EQ(GetHierarchyRoot(source), root);

  if (source != root)
    source->ConvertRectForAncestor(root, rect);

  if (target != root)
    target->ConvertRectFromAncestor(root, rect);
}

// static
void View::ConvertPointToWidget(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  src->ConvertPointForAncestor(nullptr, p);
}

// static
void View::ConvertPointFromWidget(const View* dest, gfx::Point* p) {
  DCHECK(dest);
  DCHECK(p);

  dest->ConvertPointFromAncestor(nullptr, p);
}

// static
void View::ConvertPointToScreen(const View* src, gfx::Point* p) {
  DCHECK(src);
  DCHECK(p);

  // If the view is not connected to a tree, there's nothing we can do.
  const Widget* widget = src->GetWidget();
  if (widget) {
    ConvertPointToWidget(src, p);
    *p += widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
  }
}

// static
void View::ConvertPointFromScreen(const View* dst, gfx::Point* p) {
  DCHECK(dst);
  DCHECK(p);

  const views::Widget* widget = dst->GetWidget();
  if (!widget)
    return;
  *p -= widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
  ConvertPointFromWidget(dst, p);
}

// static
void View::ConvertRectToScreen(const View* src, gfx::Rect* rect) {
  DCHECK(src);
  DCHECK(rect);

  gfx::Point new_origin = rect->origin();
  views::View::ConvertPointToScreen(src, &new_origin);
  rect->set_origin(new_origin);
}

gfx::Rect View::ConvertRectToParent(const gfx::Rect& rect) const {
  gfx::RectF x_rect = gfx::RectF(rect);
  GetTransform().TransformRect(&x_rect);
  x_rect.Offset(GetMirroredPosition().OffsetFromOrigin());
  // Pixels we partially occupy in the parent should be included.
  return gfx::ToEnclosingRect(x_rect);
}

gfx::Rect View::ConvertRectToWidget(const gfx::Rect& rect) const {
  gfx::Rect x_rect = rect;
  for (const View* v = this; v; v = v->parent_)
    x_rect = v->ConvertRectToParent(x_rect);
  return x_rect;
}

// Painting --------------------------------------------------------------------

void View::SchedulePaint() {
  SchedulePaintInRect(GetLocalBounds());
}

void View::SchedulePaintInRect(const gfx::Rect& rect) {
  if (!visible_)
    return;

  if (layer()) {
    layer()->SchedulePaint(rect);
  } else if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    parent_->SchedulePaintInRect(ConvertRectToParent(rect));
  }
}

void View::Paint(const PaintInfo& parent_paint_info) {
  if (!ShouldPaint())
    return;

  const gfx::Rect& parent_bounds =
      !parent() ? GetMirroredBounds() : parent()->GetMirroredBounds();

  PaintInfo paint_info = PaintInfo::CreateChildPaintInfo(
      parent_paint_info, GetMirroredBounds(), parent_bounds.size(),
      GetPaintScaleType(), !!layer());

  const ui::PaintContext& context = paint_info.context();
  bool is_invalidated = true;
  if (paint_info.context().CanCheckInvalid()) {
#if DCHECK_IS_ON()
    if (!context.is_pixel_canvas()) {
      gfx::Vector2d offset;
      context.Visited(this);
      View* view = this;
      while (view->parent() && !view->layer()) {
        DCHECK(view->GetTransform().IsIdentity());
        offset += view->GetMirroredPosition().OffsetFromOrigin();
        view = view->parent();
      }
      // The offset in the PaintContext should be the offset up to the paint
      // root, which we compute and verify here.
      DCHECK_EQ(context.PaintOffset().x(), offset.x());
      DCHECK_EQ(context.PaintOffset().y(), offset.y());
      // The above loop will stop when |view| is the paint root, which should be
      // the root of the current paint walk, as verified by storing the root in
      // the PaintContext.
      DCHECK_EQ(context.RootVisited(), view);
    }
#endif

    // If the View wasn't invalidated, don't waste time painting it, the output
    // would be culled.
    is_invalidated =
        context.IsRectInvalid(gfx::Rect(paint_info.paint_recording_size()));
  }

  TRACE_EVENT1("views", "View::Paint", "class", GetClassName());

  // If the view is backed by a layer, it should paint with itself as the origin
  // rather than relative to its parent.
  // TODO(danakj): Rework clip and transform recorder usage here to use
  // std::optional once we can do so.
  ui::ClipRecorder clip_recorder(parent_paint_info.context());
  if (!layer()) {
    // Set the clip rect to the bounds of this View, or |clip_path_| if it's
    // been set. Note that the X (or left) position we pass to ClipRect takes
    // into consideration whether or not the View uses a right-to-left layout so
    // that we paint the View in its mirrored position if need be.
    if (clip_path_.isEmpty()) {
      clip_recorder.ClipRect(gfx::Rect(paint_info.paint_recording_size()) +
                             paint_info.offset_from_parent());
    } else {
      gfx::Path clip_path_in_parent = clip_path_;

      // Transform |clip_path_| from local space to parent recording space.
      gfx::Transform to_parent_recording_space;

      to_parent_recording_space.Translate(paint_info.offset_from_parent());
      to_parent_recording_space.Scale(
          SkFloatToScalar(paint_info.paint_recording_scale_x()),
          SkFloatToScalar(paint_info.paint_recording_scale_y()));

      clip_path_in_parent.transform(to_parent_recording_space.matrix());
      clip_recorder.ClipPathWithAntiAliasing(clip_path_in_parent);
    }
  }

  ui::TransformRecorder transform_recorder(context);
  SetUpTransformRecorderForPainting(paint_info.offset_from_parent(),
                                    &transform_recorder);

  // Note that the cache is not aware of the offset of the view
  // relative to the parent since painting is always done relative to
  // the top left of the individual view.
  if (is_invalidated ||
      !paint_cache_.UseCache(context, paint_info.paint_recording_size())) {
    ui::PaintRecorder recorder(context, paint_info.paint_recording_size(),
                               paint_info.paint_recording_scale_x(),
                               paint_info.paint_recording_scale_y(),
                               &paint_cache_);
    gfx::Canvas* canvas = recorder.canvas();
    gfx::ScopedRTLFlipCanvas scoped_canvas(canvas, width(),
                                           flip_canvas_on_paint_for_rtl_ui_);

    // Delegate painting the contents of the View to the virtual OnPaint method.
    OnPaint(canvas);
  }

  // View::Paint() recursion over the subtree.
  PaintChildren(paint_info);
}

void View::SetBackground(std::unique_ptr<Background> b) {
  background_ = std::move(b);
}

void View::SetBorder(std::unique_ptr<Border> b) {
  border_ = std::move(b);
}

const ui::ThemeProvider* View::GetThemeProvider() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetThemeProvider() : nullptr;
}

const ui::NativeTheme* View::GetNativeTheme() const {
  if (native_theme_)
    return native_theme_;

  if (parent())
    return parent()->GetNativeTheme();

  const Widget* widget = GetWidget();
  if (widget)
    return widget->GetNativeTheme();

  return ui::NativeTheme::GetInstanceForNativeUi();
}

void View::SetNativeTheme(ui::NativeTheme* theme) {
  ui::NativeTheme* original_native_theme = GetNativeTheme();
  native_theme_ = theme;
  if (native_theme_ != original_native_theme)
    PropagateNativeThemeChanged(theme);
}

// RTL painting ----------------------------------------------------------------

void View::EnableCanvasFlippingForRTLUI(bool enable) {
  flip_canvas_on_paint_for_rtl_ui_ = enable;
}

// Input -----------------------------------------------------------------------

View* View::GetEventHandlerForPoint(const gfx::Point& point) {
  return GetEventHandlerForRect(gfx::Rect(point, gfx::Size(1, 1)));
}

View* View::GetEventHandlerForRect(const gfx::Rect& rect) {
  return GetEffectiveViewTargeter()->TargetForRect(this, rect);
}

bool View::CanProcessEventsWithinSubtree() const {
  return can_process_events_within_subtree_;
}

View* View::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // TODO(tdanderson): Move this implementation into ViewTargetDelegate.
  if (!HitTestPoint(point) || !CanProcessEventsWithinSubtree())
    return nullptr;

  // Walk the child Views recursively looking for the View that most
  // tightly encloses the specified point.
  View::Views children = GetChildrenInZOrder();
  DCHECK_EQ(child_count(), static_cast<int>(children.size()));
  for (auto* child : base::Reversed(children)) {
    if (!child->visible())
      continue;

    gfx::Point point_in_child_coords(point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    View* handler = child->GetTooltipHandlerForPoint(point_in_child_coords);
    if (handler)
      return handler;
  }
  return this;
}

gfx::NativeCursor View::GetCursor(const ui::MouseEvent& event) {
#if defined(OS_WIN)
  static ui::Cursor arrow;
  if (!arrow.platform())
    arrow.SetPlatformCursor(LoadCursor(nullptr, IDC_ARROW));
  return arrow;
#else
  return gfx::kNullCursor;
#endif
}

bool View::HitTestPoint(const gfx::Point& point) const {
  return HitTestRect(gfx::Rect(point, gfx::Size(1, 1)));
}

bool View::HitTestRect(const gfx::Rect& rect) const {
  return GetEffectiveViewTargeter()->DoesIntersectRect(this, rect);
}

bool View::IsMouseHovered() const {
  // If we haven't yet been placed in an onscreen view hierarchy, we can't be
  // hovered.
  if (!GetWidget())
    return false;

  // If mouse events are disabled, then the mouse cursor is invisible and
  // is therefore not hovering over this button.
  if (!GetWidget()->IsMouseEventsEnabled())
    return false;

  gfx::Point cursor_pos(display::Screen::GetScreen()->GetCursorScreenPoint());
  ConvertPointFromScreen(this, &cursor_pos);
  return HitTestPoint(cursor_pos);
}

bool View::OnMousePressed(const ui::MouseEvent& event) {
  return false;
}

bool View::OnMouseDragged(const ui::MouseEvent& event) {
  return false;
}

void View::OnMouseReleased(const ui::MouseEvent& event) {
}

void View::OnMouseCaptureLost() {
}

void View::OnMouseMoved(const ui::MouseEvent& event) {
}

void View::OnMouseEntered(const ui::MouseEvent& event) {
}

void View::OnMouseExited(const ui::MouseEvent& event) {
}

void View::SetMouseHandler(View* new_mouse_handler) {
  // |new_mouse_handler| may be nullptr.
  if (parent_)
    parent_->SetMouseHandler(new_mouse_handler);
}

bool View::OnKeyPressed(const ui::KeyEvent& event) {
  return false;
}

bool View::OnKeyReleased(const ui::KeyEvent& event) {
  return false;
}

bool View::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return false;
}

void View::OnKeyEvent(ui::KeyEvent* event) {
  bool consumed = (event->type() == ui::ET_KEY_PRESSED) ? OnKeyPressed(*event) :
                                                          OnKeyReleased(*event);
  if (consumed)
    event->StopPropagation();
}

void View::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (ProcessMousePressed(*event))
        event->SetHandled();
      return;

    case ui::ET_MOUSE_MOVED:
      if ((event->flags() & (ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_RIGHT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON)) == 0) {
        OnMouseMoved(*event);
        return;
      }
      FALLTHROUGH;
    case ui::ET_MOUSE_DRAGGED:
      if (ProcessMouseDragged(*event))
        event->SetHandled();
      return;

    case ui::ET_MOUSE_RELEASED:
      ProcessMouseReleased(*event);
      return;

    case ui::ET_MOUSEWHEEL:
      if (OnMouseWheel(*event->AsMouseWheelEvent()))
        event->SetHandled();
      break;

    case ui::ET_MOUSE_ENTERED:
      if (event->flags() & ui::EF_TOUCH_ACCESSIBILITY)
        NotifyAccessibilityEvent(ax::mojom::Event::kHover, true);
      OnMouseEntered(*event);
      break;

    case ui::ET_MOUSE_EXITED:
      OnMouseExited(*event);
      break;

    default:
      return;
  }
}

void View::OnScrollEvent(ui::ScrollEvent* event) {
}

void View::OnTouchEvent(ui::TouchEvent* event) {
  NOTREACHED() << "Views should not receive touch events.";
}

void View::OnGestureEvent(ui::GestureEvent* event) {
}

const ui::InputMethod* View::GetInputMethod() const {
  Widget* widget = const_cast<Widget*>(GetWidget());
  return widget ? const_cast<const ui::InputMethod*>(widget->GetInputMethod())
                : nullptr;
}

std::unique_ptr<ViewTargeter> View::SetEventTargeter(
    std::unique_ptr<ViewTargeter> targeter) {
  std::unique_ptr<ViewTargeter> old_targeter = std::move(targeter_);
  targeter_ = std::move(targeter);
  return old_targeter;
}

ViewTargeter* View::GetEffectiveViewTargeter() const {
  DCHECK(GetWidget());
  ViewTargeter* view_targeter = targeter();
  if (!view_targeter)
    view_targeter = GetWidget()->GetRootView()->targeter();
  CHECK(view_targeter);
  return view_targeter;
}

WordLookupClient* View::GetWordLookupClient() {
  return nullptr;
}

bool View::CanAcceptEvent(const ui::Event& event) {
  return IsDrawn();
}

ui::EventTarget* View::GetParentTarget() {
  return parent_;
}

std::unique_ptr<ui::EventTargetIterator> View::GetChildIterator() const {
  return std::make_unique<ui::EventTargetIteratorPtrImpl<View>>(children_);
}

ui::EventTargeter* View::GetEventTargeter() {
  return targeter_.get();
}

void View::ConvertEventToTarget(ui::EventTarget* target,
                                ui::LocatedEvent* event) {
  event->ConvertLocationToTarget(this, static_cast<View*>(target));
}

gfx::PointF View::GetScreenLocationF(const ui::LocatedEvent& event) const {
  DCHECK_EQ(this, event.target());
  gfx::Point screen_location(event.location());
  ConvertPointToScreen(this, &screen_location);
  return gfx::PointF(screen_location);
}

// Accelerators ----------------------------------------------------------------

void View::AddAccelerator(const ui::Accelerator& accelerator) {
  if (!accelerators_.get())
    accelerators_.reset(new std::vector<ui::Accelerator>());

  if (!base::ContainsValue(*accelerators_.get(), accelerator))
    accelerators_->push_back(accelerator);

  RegisterPendingAccelerators();
}

void View::RemoveAccelerator(const ui::Accelerator& accelerator) {
  if (!accelerators_.get()) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  auto i(std::find(accelerators_->begin(), accelerators_->end(), accelerator));
  if (i == accelerators_->end()) {
    NOTREACHED() << "Removing non-existing accelerator";
    return;
  }

  size_t index = i - accelerators_->begin();
  accelerators_->erase(i);
  if (index >= registered_accelerator_count_) {
    // The accelerator is not registered to FocusManager.
    return;
  }
  --registered_accelerator_count_;

  // Providing we are attached to a Widget and registered with a focus manager,
  // we should de-register from that focus manager now.
  if (GetWidget() && accelerator_focus_manager_)
    accelerator_focus_manager_->UnregisterAccelerator(accelerator, this);
}

void View::ResetAccelerators() {
  if (accelerators_.get())
    UnregisterAccelerators(false);
}

bool View::AcceleratorPressed(const ui::Accelerator& accelerator) {
  return false;
}

bool View::CanHandleAccelerators() const {
  const Widget* widget = GetWidget();
  if (!enabled() || !IsDrawn() || !widget || !widget->IsVisible())
    return false;
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // Non-ChromeOS aura windows have an associated FocusManagerEventHandler which
  // adds currently focused view as an event PreTarget (see
  // DesktopNativeWidgetAura::InitNativeWidget). However, the focused view isn't
  // always the right view to handle accelerators: It should only handle them
  // when active. Only top level widgets can be active, so for child widgets
  // check if they are focused instead. ChromeOS also behaves different than
  // Linux when an extension popup is about to handle the accelerator.
  bool child = widget && widget->GetTopLevelWidget() != widget;
  bool focus_in_child =
      widget &&
      widget->GetRootView()->Contains(GetFocusManager()->GetFocusedView());
  if ((child && !focus_in_child) || (!child && !widget->IsActive()))
    return false;
#endif
  return true;
}

// Focus -----------------------------------------------------------------------

bool View::HasFocus() const {
  const FocusManager* focus_manager = GetFocusManager();
  return focus_manager && (focus_manager->GetFocusedView() == this);
}

View* View::GetNextFocusableView() {
  return next_focusable_view_;
}

const View* View::GetNextFocusableView() const {
  return next_focusable_view_;
}

View* View::GetPreviousFocusableView() {
  return previous_focusable_view_;
}

void View::SetNextFocusableView(View* view) {
  if (view)
    view->previous_focusable_view_ = this;
  next_focusable_view_ = view;
}

void View::SetFocusBehavior(FocusBehavior focus_behavior) {
  if (focus_behavior_ == focus_behavior)
    return;

  focus_behavior_ = focus_behavior;
  AdvanceFocusIfNecessary();
}

bool View::IsFocusable() const {
  return focus_behavior_ == FocusBehavior::ALWAYS && enabled_ && IsDrawn();
}

bool View::IsAccessibilityFocusable() const {
  return focus_behavior_ != FocusBehavior::NEVER && enabled_ && IsDrawn();
}

FocusManager* View::GetFocusManager() {
  Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : nullptr;
}

const FocusManager* View::GetFocusManager() const {
  const Widget* widget = GetWidget();
  return widget ? widget->GetFocusManager() : nullptr;
}

void View::RequestFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    bool focusable = focus_manager->keyboard_accessible()
                         ? IsAccessibilityFocusable()
                         : IsFocusable();
    if (focusable)
      focus_manager->SetFocusedView(this);
  }
}

bool View::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  return false;
}

FocusTraversable* View::GetFocusTraversable() {
  return nullptr;
}

FocusTraversable* View::GetPaneFocusTraversable() {
  return nullptr;
}

// Tooltips --------------------------------------------------------------------

bool View::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  return false;
}

bool View::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* loc) const {
  return false;
}

// Context menus ---------------------------------------------------------------

void View::ShowContextMenu(const gfx::Point& p,
                           ui::MenuSourceType source_type) {
  if (!context_menu_controller_)
    return;

  context_menu_controller_->ShowContextMenuForView(this, p, source_type);
}

// static
bool View::ShouldShowContextMenuOnMousePress() {
  return kContextMenuOnMousePress;
}

gfx::Point View::GetKeyboardContextMenuLocation() {
  gfx::Rect vis_bounds = GetVisibleBounds();
  gfx::Point screen_point(vis_bounds.x() + vis_bounds.width() / 2,
                          vis_bounds.y() + vis_bounds.height() / 2);
  ConvertPointToScreen(this, &screen_point);
  return screen_point;
}

// Drag and drop ---------------------------------------------------------------

bool View::GetDropFormats(int* formats,
                          std::set<ui::Clipboard::FormatType>* format_types) {
  return false;
}

bool View::AreDropTypesRequired() {
  return false;
}

bool View::CanDrop(const OSExchangeData& data) {
  // TODO(sky): when I finish up migration, this should default to true.
  return false;
}

void View::OnDragEntered(const ui::DropTargetEvent& event) {
}

int View::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragExited() {
}

int View::OnPerformDrop(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_NONE;
}

void View::OnDragDone() {
}

// static
bool View::ExceededDragThreshold(const gfx::Vector2d& delta) {
  return (abs(delta.x()) > GetHorizontalDragThreshold() ||
          abs(delta.y()) > GetVerticalDragThreshold());
}

// Accessibility----------------------------------------------------------------

ViewAccessibility& View::GetViewAccessibility() {
  if (!view_accessibility_)
    view_accessibility_ = ViewAccessibility::Create(this);
  return *view_accessibility_;
}

bool View::HandleAccessibleAction(const ui::AXActionData& action_data) {
  switch (action_data.action) {
    case ax::mojom::Action::kBlur:
      if (HasFocus()) {
        GetFocusManager()->ClearFocus();
        return true;
      }
      break;
    case ax::mojom::Action::kDoDefault: {
      const gfx::Point center = GetLocalBounds().CenterPoint();
      ui::MouseEvent press(ui::ET_MOUSE_PRESSED, center, center,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
      OnEvent(&press);
      ui::MouseEvent release(ui::ET_MOUSE_RELEASED, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
      OnEvent(&release);
      return true;
    }
    case ax::mojom::Action::kFocus:
      if (IsAccessibilityFocusable()) {
        RequestFocus();
        return true;
      }
      break;
    case ax::mojom::Action::kScrollToMakeVisible:
      ScrollRectToVisible(GetLocalBounds());
      return true;
    case ax::mojom::Action::kShowContextMenu:
      ShowContextMenu(GetBoundsInScreen().CenterPoint(),
                      ui::MENU_SOURCE_KEYBOARD);
      return true;
    default:
      // Some actions are handled by subclasses of View.
      break;
  }

  return false;
}

gfx::NativeViewAccessible View::GetNativeViewAccessible() {
  return GetViewAccessibility().GetNativeObject();
}

void View::NotifyAccessibilityEvent(ax::mojom::Event event_type,
                                    bool send_native_event) {
  if (ViewsDelegate::GetInstance())
    ViewsDelegate::GetInstance()->NotifyAccessibilityEvent(this, event_type);

  if (send_native_event && GetWidget())
    GetViewAccessibility().NotifyAccessibilityEvent(event_type);

  OnAccessibilityEvent(event_type);
}

void View::OnAccessibilityEvent(ax::mojom::Event event_type) {}

// Scrolling -------------------------------------------------------------------

void View::ScrollRectToVisible(const gfx::Rect& rect) {
  if (parent_)
    parent_->ScrollRectToVisible(rect + bounds().OffsetFromOrigin());
}

void View::ScrollViewToVisible() {
  ScrollRectToVisible(GetLocalBounds());
}

int View::GetPageScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

int View::GetLineScrollIncrement(ScrollView* scroll_view,
                                 bool is_horizontal, bool is_positive) {
  return 0;
}

void View::AddObserver(ViewObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool View::HasObserver(const ViewObserver* observer) const {
  return observers_.HasObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// View, protected:

// Size and disposition --------------------------------------------------------

gfx::Size View::CalculatePreferredSize() const {
  if (layout_manager_.get())
    return layout_manager_->GetPreferredSize(this);
  return gfx::Size();
}

void View::OnBoundsChanged(const gfx::Rect& previous_bounds) {
}

void View::PreferredSizeChanged() {
  InvalidateLayout();
  if (parent_)
    parent_->ChildPreferredSizeChanged(this);
  for (ViewObserver& observer : observers_)
    observer.OnViewPreferredSizeChanged(this);
}

bool View::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return false;
}

void View::OnVisibleBoundsChanged() {
}

// Tree operations -------------------------------------------------------------

void View::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
}

void View::VisibilityChanged(View* starting_from, bool is_visible) {
}

void View::NativeViewHierarchyChanged() {
  FocusManager* focus_manager = GetFocusManager();
  if (accelerator_focus_manager_ != focus_manager) {
    UnregisterAccelerators(true);

    if (focus_manager)
      RegisterPendingAccelerators();
  }
}

void View::AddedToWidget() {}

void View::RemovedFromWidget() {}

// Painting --------------------------------------------------------------------

void View::PaintChildren(const PaintInfo& paint_info) {
  TRACE_EVENT1("views", "View::PaintChildren", "class", GetClassName());
  RecursivePaintHelper(&View::Paint, paint_info);
}

void View::OnPaint(gfx::Canvas* canvas) {
  TRACE_EVENT1("views", "View::OnPaint", "class", GetClassName());
  OnPaintBackground(canvas);
  OnPaintBorder(canvas);
}

void View::OnPaintBackground(gfx::Canvas* canvas) {
  if (background_) {
    TRACE_EVENT0("views", "View::OnPaintBackground");
    background_->Paint(canvas, this);
  }
}

void View::OnPaintBorder(gfx::Canvas* canvas) {
  if (border_) {
    TRACE_EVENT0("views", "View::OnPaintBorder");
    border_->Paint(*this, canvas);
  }
}

// Accelerated Painting --------------------------------------------------------

View::LayerOffsetData View::CalculateOffsetToAncestorWithLayer(
    ui::Layer** layer_parent) {
  if (layer()) {
    if (layer_parent)
      *layer_parent = layer();
    return LayerOffsetData(layer()->device_scale_factor());
  }
  if (!parent_)
    return LayerOffsetData();

  LayerOffsetData offset_data =
      parent_->CalculateOffsetToAncestorWithLayer(layer_parent);

  return offset_data + GetMirroredPosition().OffsetFromOrigin();
}

void View::UpdateParentLayer() {
  if (!layer())
    return;

  ui::Layer* parent_layer = nullptr;
  gfx::Vector2d offset(GetMirroredX(), y());

  if (parent_) {
    offset +=
        parent_->CalculateOffsetToAncestorWithLayer(&parent_layer).offset();
  }

  ReparentLayer(offset, parent_layer);
}

void View::MoveLayerToParent(ui::Layer* parent_layer,
                             const LayerOffsetData& offset_data) {
  LayerOffsetData local_offset_data(offset_data);
  if (parent_layer != layer())
    local_offset_data += GetMirroredPosition().OffsetFromOrigin();

  if (layer() && parent_layer != layer()) {
    parent_layer->Add(layer());
    SetLayerBounds(size(), local_offset_data);
  } else {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : GetChildrenInZOrder())
      child->MoveLayerToParent(parent_layer, local_offset_data);
  }
}

void View::UpdateLayerVisibility() {
  bool visible = visible_;
  for (const View* v = parent_; visible && v && !v->layer(); v = v->parent_)
    visible = v->visible();

  UpdateChildLayerVisibility(visible);
}

void View::UpdateChildLayerVisibility(bool ancestor_visible) {
  if (layer()) {
    layer()->SetVisible(ancestor_visible && visible_);
  } else {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->UpdateChildLayerVisibility(ancestor_visible && visible_);
  }
}

void View::DestroyLayerImpl(LayerChangeNotifyBehavior notify_parents) {
  if (!paint_to_layer_)
    return;

  paint_to_layer_ = false;
  if (!layer())
    return;

  ui::Layer* new_parent = layer()->parent();
  std::vector<ui::Layer*> children = layer()->children();
  for (size_t i = 0; i < children.size(); ++i) {
    layer()->Remove(children[i]);
    if (new_parent)
      new_parent->Add(children[i]);
  }

  LayerOwner::DestroyLayer();

  if (new_parent)
    ReorderLayers();

  UpdateChildLayerBounds(CalculateOffsetToAncestorWithLayer(nullptr));

  SchedulePaint();

  // Notify the parent chain about the layer change.
  if (notify_parents == LayerChangeNotifyBehavior::NOTIFY)
    NotifyParentsOfLayerChange();

  Widget* widget = GetWidget();
  if (widget)
    widget->LayerTreeChanged();
}

void View::NotifyParentsOfLayerChange() {
  // Notify the parent chain about the layer change.
  View* view_parent = parent();
  while (view_parent) {
    view_parent->OnChildLayerChanged(this);
    view_parent = view_parent->parent();
  }
}

void View::UpdateChildLayerBounds(const LayerOffsetData& offset_data) {
  if (layer()) {
    SetLayerBounds(size(), offset_data);
  } else {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_) {
      child->UpdateChildLayerBounds(
          offset_data + child->GetMirroredPosition().OffsetFromOrigin());
    }
  }
}

void View::OnPaintLayer(const ui::PaintContext& context) {
  PaintFromPaintRoot(context);
}

void View::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                      float new_device_scale_factor) {
  snap_layer_to_pixel_boundary_ =
      (new_device_scale_factor - std::floor(new_device_scale_factor)) != 0.0f;

  if (!layer())
    return;

  // There can be no subpixel offset if the layer has no parent.
  if (!parent() || !layer()->parent())
    return;

  if (layer()->parent() && layer()->GetCompositor() &&
      layer()->GetCompositor()->is_pixel_canvas()) {
    LayerOffsetData offset_data(
        parent()->CalculateOffsetToAncestorWithLayer(nullptr));
    offset_data += GetMirroredPosition().OffsetFromOrigin();
    SnapLayerToPixelBoundary(offset_data);
  } else {
    SnapLayerToPixelBoundary(LayerOffsetData());
  }
}

void View::ReorderLayers() {
  View* v = this;
  while (v && !v->layer())
    v = v->parent();

  Widget* widget = GetWidget();
  if (!v) {
    if (widget) {
      ui::Layer* layer = widget->GetLayer();
      if (layer)
        widget->GetRootView()->ReorderChildLayers(layer);
    }
  } else {
    v->ReorderChildLayers(v->layer());
  }

  if (widget) {
    // Reorder the widget's child NativeViews in case a child NativeView is
    // associated with a view (e.g. via a NativeViewHost). Always do the
    // reordering because the associated NativeView's layer (if it has one)
    // is parented to the widget's layer regardless of whether the host view has
    // an ancestor with a layer.
    widget->ReorderNativeViews();
  }
}

void View::ReorderChildLayers(ui::Layer* parent_layer) {
  if (layer() && layer() != parent_layer) {
    DCHECK_EQ(parent_layer, layer()->parent());
    parent_layer->StackAtBottom(layer());
  } else {
    // Iterate backwards through the children so that a child with a layer
    // which is further to the back is stacked above one which is further to
    // the front.
    View::Views children = GetChildrenInZOrder();
    DCHECK_EQ(child_count(), static_cast<int>(children.size()));
    for (auto* child : base::Reversed(children))
      child->ReorderChildLayers(parent_layer);
  }
}

void View::OnChildLayerChanged(View* child) {}

// Input -----------------------------------------------------------------------

View::DragInfo* View::GetDragInfo() {
  return parent_ ? parent_->GetDragInfo() : nullptr;
}

// Focus -----------------------------------------------------------------------

void View::OnFocus() {
  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // By default, we clear the native focus. This ensures that no visible native
  // view as the focus and that we still receive keyboard inputs.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->ClearNativeFocus();

  // TODO(beng): Investigate whether it's possible for us to move this to
  //             Focus().
  // Notify assistive technologies of the focus change.
  NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void View::OnBlur() {
}

void View::Focus() {
  OnFocus();
  ScrollViewToVisible();

  for (ViewObserver& observer : observers_)
    observer.OnViewFocused(this);
}

void View::Blur() {
  ViewTracker tracker(this);
  OnBlur();

  if (tracker.view()) {
    for (ViewObserver& observer : observers_)
      observer.OnViewBlurred(this);
  }
}

// Tooltips --------------------------------------------------------------------

void View::TooltipTextChanged() {
  Widget* widget = GetWidget();
  // TooltipManager may be null if there is a problem creating it.
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->TooltipTextChanged(this);
}

// Drag and drop ---------------------------------------------------------------

int View::GetDragOperations(const gfx::Point& press_pt) {
  return drag_controller_ ?
      drag_controller_->GetDragOperationsForView(this, press_pt) :
      ui::DragDropTypes::DRAG_NONE;
}

void View::WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) {
  DCHECK(drag_controller_);
  drag_controller_->WriteDragDataForView(this, press_pt, data);
}

bool View::InDrag() const {
  const Widget* widget = GetWidget();
  return widget ? widget->dragged_view() == this : false;
}

int View::GetHorizontalDragThreshold() {
  // TODO(jennyz): This value may need to be adjusted for different platforms
  // and for different display density.
  return kDefaultHorizontalDragThreshold;
}

int View::GetVerticalDragThreshold() {
  // TODO(jennyz): This value may need to be adjusted for different platforms
  // and for different display density.
  return kDefaultVerticalDragThreshold;
}

PaintInfo::ScaleType View::GetPaintScaleType() const {
  return PaintInfo::ScaleType::kScaleWithEdgeSnapping;
}

////////////////////////////////////////////////////////////////////////////////
// View, private:

// DropInfo --------------------------------------------------------------------

void View::DragInfo::Reset() {
  possible_drag = false;
  start_pt = gfx::Point();
}

void View::DragInfo::PossibleDrag(const gfx::Point& p) {
  possible_drag = true;
  start_pt = p;
}

// Painting --------------------------------------------------------------------

void View::SchedulePaintBoundsChanged(SchedulePaintType type) {
  // If we have a layer and the View's size did not change, we do not need to
  // schedule any paints since the layer will be redrawn at its new location
  // during the next Draw() cycle in the compositor.
  if (!layer() || type == SCHEDULE_PAINT_SIZE_CHANGED) {
    // Otherwise, if the size changes or we don't have a layer then we need to
    // use SchedulePaint to invalidate the area occupied by the View.
    SchedulePaint();
  } else if (parent_ && type == SCHEDULE_PAINT_SIZE_SAME) {
    // The compositor doesn't Draw() until something on screen changes, so
    // if our position changes but nothing is being animated on screen, then
    // tell the compositor to redraw the scene. We know layer() exists due to
    // the above if clause.
    layer()->ScheduleDraw();
  }
}

void View::SchedulePaintOnParent() {
  if (parent_) {
    // Translate the requested paint rect to the parent's coordinate system
    // then pass this notification up to the parent.
    parent_->SchedulePaintInRect(ConvertRectToParent(GetLocalBounds()));
  }
}

bool View::ShouldPaint() const {
  return visible_ && !size().IsEmpty();
}

void View::SetUpTransformRecorderForPainting(
    const gfx::Vector2d& offset_from_parent,
    ui::TransformRecorder* recorder) const {
  // If the view is backed by a layer, it should paint with itself as the origin
  // rather than relative to its parent.
  if (layer())
    return;

  // Translate the graphics such that 0,0 corresponds to where this View is
  // located relative to its parent.
  gfx::Transform transform_from_parent;
  transform_from_parent.Translate(offset_from_parent.x(),
                                  offset_from_parent.y());
  recorder->Transform(transform_from_parent);
}

void View::RecursivePaintHelper(void (View::*func)(const PaintInfo&),
                                const PaintInfo& info) {
  View::Views children = GetChildrenInZOrder();
  DCHECK_EQ(child_count(), static_cast<int>(children.size()));
  for (auto* child : children) {
    if (!child->layer())
      (child->*func)(info);
  }
}

void View::PaintFromPaintRoot(const ui::PaintContext& parent_context) {
  PaintInfo paint_info = PaintInfo::CreateRootPaintInfo(
      parent_context, layer() ? layer()->size() : size());
  Paint(paint_info);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDrawViewBoundsRects))
    PaintDebugRects(paint_info);
}

void View::PaintDebugRects(const PaintInfo& parent_paint_info) {
  if (!ShouldPaint())
    return;

  const gfx::Rect& parent_bounds = (layer() || !parent())
                                       ? GetMirroredBounds()
                                       : parent()->GetMirroredBounds();
  PaintInfo paint_info = PaintInfo::CreateChildPaintInfo(
      parent_paint_info, GetMirroredBounds(), parent_bounds.size(),
      GetPaintScaleType(), !!layer());

  const ui::PaintContext& context = paint_info.context();

  ui::TransformRecorder transform_recorder(context);
  SetUpTransformRecorderForPainting(paint_info.offset_from_parent(),
                                    &transform_recorder);

  RecursivePaintHelper(&View::PaintDebugRects, paint_info);

  // Draw outline rects for debugging.
  ui::PaintRecorder recorder(context, paint_info.paint_recording_size(),
                             paint_info.paint_recording_scale_x(),
                             paint_info.paint_recording_scale_y(),
                             &paint_cache_);
  gfx::Canvas* canvas = recorder.canvas();
  const float scale = canvas->UndoDeviceScaleFactor();
  gfx::RectF outline_rect(ScaleToEnclosedRect(GetLocalBounds(), scale));
  gfx::RectF content_outline_rect(
      ScaleToEnclosedRect(GetContentsBounds(), scale));
  if (content_outline_rect != outline_rect) {
    content_outline_rect.Inset(0.5f, 0.5f);
    const SkColor content_color = SkColorSetARGB(0x30, 0, 0, 0xff);
    canvas->DrawRect(content_outline_rect, content_color);
  }
  outline_rect.Inset(0.5f, 0.5f);
  const SkColor color = SkColorSetARGB(0x30, 0xff, 0, 0);
  canvas->DrawRect(outline_rect, color);
}

// Tree operations -------------------------------------------------------------

void View::DoRemoveChildView(View* view,
                             bool update_focus_cycle,
                             bool update_tool_tip,
                             bool delete_removed_view,
                             View* new_parent) {
  DCHECK(view);

  const Views::iterator i(std::find(children_.begin(), children_.end(), view));
  if (i == children_.end())
    return;

  std::unique_ptr<View> view_to_be_deleted;
  if (update_focus_cycle) {
    View* next_focusable = view->next_focusable_view_;
    View* prev_focusable = view->previous_focusable_view_;
    if (prev_focusable)
      prev_focusable->next_focusable_view_ = next_focusable;
    if (next_focusable)
      next_focusable->previous_focusable_view_ = prev_focusable;
  }

  Widget* widget = GetWidget();
  bool is_removed_from_widget = false;
  if (widget) {
    UnregisterChildrenForVisibleBoundsNotification(view);
    if (view->visible())
      view->SchedulePaint();

    is_removed_from_widget = !new_parent || new_parent->GetWidget() != widget;
    if (is_removed_from_widget)
      widget->NotifyWillRemoveView(view);
  }

  // Make sure the layers belonging to the subtree rooted at |view| get
  // removed.
  view->OrphanLayers();
  if (widget)
    widget->LayerTreeChanged();

  view->PropagateRemoveNotifications(this, new_parent, is_removed_from_widget);
  view->parent_ = nullptr;

  if (delete_removed_view && !view->owned_by_client_)
    view_to_be_deleted.reset(view);

#if DCHECK_IS_ON()
  DCHECK(!iterating_);
#endif
  children_.erase(i);

  if (update_tool_tip)
    UpdateTooltip();

  if (layout_manager_)
    layout_manager_->ViewRemoved(this, view);

  for (ViewObserver& observer : observers_)
    observer.OnChildViewRemoved(this, view);
}

void View::PropagateRemoveNotifications(View* old_parent,
                                        View* new_parent,
                                        bool is_removed_from_widget) {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_) {
      child->PropagateRemoveNotifications(old_parent, new_parent,
                                          is_removed_from_widget);
    }
  }

  ViewHierarchyChangedDetails details(false, old_parent, this, new_parent);
  for (View* v = this; v; v = v->parent_)
    v->ViewHierarchyChangedImpl(true, details);

  if (is_removed_from_widget)
    RemovedFromWidget();
}

void View::PropagateAddNotifications(const ViewHierarchyChangedDetails& details,
                                     bool is_added_to_widget) {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->PropagateAddNotifications(details, is_added_to_widget);
  }
  ViewHierarchyChangedImpl(true, details);
  if (is_added_to_widget)
    AddedToWidget();
}

void View::PropagateNativeViewHierarchyChanged() {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->PropagateNativeViewHierarchyChanged();
  }
  NativeViewHierarchyChanged();
}

void View::ViewHierarchyChangedImpl(
    bool register_accelerators,
    const ViewHierarchyChangedDetails& details) {
  if (register_accelerators) {
    if (details.is_add) {
      // If you get this registration, you are part of a subtree that has been
      // added to the view hierarchy.
      if (GetFocusManager())
        RegisterPendingAccelerators();
    } else {
      if (details.child == this)
        UnregisterAccelerators(true);
    }
  }

  ViewHierarchyChanged(details);
  details.parent->needs_layout_ = true;
}

void View::PropagateNativeThemeChanged(const ui::NativeTheme* theme) {
  if (native_theme_ && native_theme_ != theme)
    return;

  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->PropagateNativeThemeChanged(theme);
  }
  OnNativeThemeChanged(theme);
  for (ViewObserver& observer : observers_)
    observer.OnViewNativeThemeChanged(this);
}

// Size and disposition --------------------------------------------------------

void View::PropagateVisibilityNotifications(View* start, bool is_visible) {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->PropagateVisibilityNotifications(start, is_visible);
  }
  VisibilityChangedImpl(start, is_visible);
}

void View::VisibilityChangedImpl(View* starting_from, bool is_visible) {
  VisibilityChanged(starting_from, is_visible);
}

void View::SnapLayerToPixelBoundary(const LayerOffsetData& offset_data) {
  if (!layer())
    return;

  if (snap_layer_to_pixel_boundary_ && layer()->parent() &&
      layer()->GetCompositor()) {
    if (layer()->GetCompositor()->is_pixel_canvas()) {
      layer()->SetSubpixelPositionOffset(offset_data.GetSubpixelOffset());
    } else {
      ui::SnapLayerToPhysicalPixelBoundary(layer()->parent(), layer());
    }
  } else {
    // Reset the offset.
    layer()->SetSubpixelPositionOffset(gfx::Vector2dF());
  }
}

void View::BoundsChanged(const gfx::Rect& previous_bounds) {
  if (visible_) {
    // Paint the new bounds.
    SchedulePaintBoundsChanged(
        bounds_.size() == previous_bounds.size() ? SCHEDULE_PAINT_SIZE_SAME :
        SCHEDULE_PAINT_SIZE_CHANGED);
  }

  if (layer()) {
    if (parent_) {
      LayerOffsetData offset_data(
          parent_->CalculateOffsetToAncestorWithLayer(nullptr));
      offset_data += GetMirroredPosition().OffsetFromOrigin();
      SetLayerBounds(size(), offset_data);
    } else {
      SetLayerBounds(bounds_.size(),
                     LayerOffsetData() + bounds_.OffsetFromOrigin());
    }

    // In RTL mode, if our width has changed, our children's mirrored bounds
    // will have changed. Update the child's layer bounds, or if it is not a
    // layer, the bounds of any layers inside the child.
    if (base::i18n::IsRTL() && bounds_.width() != previous_bounds.width()) {
      for (int i = 0; i < child_count(); ++i) {
        View* child = child_at(i);
        child->UpdateChildLayerBounds(
            LayerOffsetData(layer()->device_scale_factor(),
                            child->GetMirroredPosition().OffsetFromOrigin()));
      }
    }
  } else {
    // If our bounds have changed, then any descendant layer bounds may have
    // changed. Update them accordingly.
    UpdateChildLayerBounds(CalculateOffsetToAncestorWithLayer(nullptr));
  }

  OnBoundsChanged(previous_bounds);
  if (bounds_ != previous_bounds)
    NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged, false);

  if (needs_layout_ || previous_bounds.size() != size()) {
    needs_layout_ = false;
    TRACE_EVENT1("views", "View::Layout(bounds_changed)", "class",
                 GetClassName());
    Layout();
  }

  if (GetNeedsNotificationWhenVisibleBoundsChange())
    OnVisibleBoundsChanged();

  // Notify interested Views that visible bounds within the root view may have
  // changed.
  if (descendants_to_notify_.get()) {
    for (auto i(descendants_to_notify_->begin());
         i != descendants_to_notify_->end(); ++i) {
      (*i)->OnVisibleBoundsChanged();
    }
  }
}

// static
void View::RegisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->GetNeedsNotificationWhenVisibleBoundsChange())
    view->RegisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    RegisterChildrenForVisibleBoundsNotification(view->child_at(i));
}

// static
void View::UnregisterChildrenForVisibleBoundsNotification(View* view) {
  if (view->GetNeedsNotificationWhenVisibleBoundsChange())
    view->UnregisterForVisibleBoundsNotification();
  for (int i = 0; i < view->child_count(); ++i)
    UnregisterChildrenForVisibleBoundsNotification(view->child_at(i));
}

void View::RegisterForVisibleBoundsNotification() {
  if (registered_for_visible_bounds_notification_)
    return;

  registered_for_visible_bounds_notification_ = true;
  for (View* ancestor = parent_; ancestor; ancestor = ancestor->parent_)
    ancestor->AddDescendantToNotify(this);
}

void View::UnregisterForVisibleBoundsNotification() {
  if (!registered_for_visible_bounds_notification_)
    return;

  registered_for_visible_bounds_notification_ = false;
  for (View* ancestor = parent_; ancestor; ancestor = ancestor->parent_)
    ancestor->RemoveDescendantToNotify(this);
}

void View::AddDescendantToNotify(View* view) {
  DCHECK(view);
  if (!descendants_to_notify_.get())
    descendants_to_notify_.reset(new Views);
  descendants_to_notify_->push_back(view);
}

void View::RemoveDescendantToNotify(View* view) {
  DCHECK(view && descendants_to_notify_.get());
  auto i(std::find(descendants_to_notify_->begin(),
                   descendants_to_notify_->end(), view));
  DCHECK(i != descendants_to_notify_->end());
  descendants_to_notify_->erase(i);
  if (descendants_to_notify_->empty())
    descendants_to_notify_.reset();
}

void View::SetLayoutManagerImpl(std::unique_ptr<LayoutManager> layout_manager) {
  // Some code keeps a bare pointer to the layout manager for calling
  // derived-class-specific-functions. It's an easy mistake to create a new
  // unique_ptr and re-set the layout manager with a new unique_ptr, which
  // will cause a crash. Re-setting to nullptr is OK.
  CHECK(!layout_manager.get() || layout_manager_.get() != layout_manager.get());

  layout_manager_ = std::move(layout_manager);
  if (layout_manager_)
    layout_manager_->Installed(this);
}

void View::SetLayerBounds(const gfx::Size& size,
                          const LayerOffsetData& offset_data) {
  layer()->SetBounds(gfx::Rect(size) + offset_data.offset());
  SnapLayerToPixelBoundary(offset_data);
}

// Transformations -------------------------------------------------------------

bool View::GetTransformRelativeTo(const View* ancestor,
                                  gfx::Transform* transform) const {
  const View* p = this;

  while (p && p != ancestor) {
    transform->ConcatTransform(p->GetTransform());
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->GetMirroredX()),
                          static_cast<float>(p->y()));
    transform->ConcatTransform(translation);

    p = p->parent_;
  }

  return p == ancestor;
}

// Coordinate conversion -------------------------------------------------------

bool View::ConvertPointForAncestor(const View* ancestor,
                                   gfx::Point* point) const {
  gfx::Transform trans;
  // TODO(sad): Have some way of caching the transformation results.
  bool result = GetTransformRelativeTo(ancestor, &trans);
  auto p = gfx::Point3F(gfx::PointF(*point));
  trans.TransformPoint(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool View::ConvertPointFromAncestor(const View* ancestor,
                                    gfx::Point* point) const {
  gfx::Transform trans;
  bool result = GetTransformRelativeTo(ancestor, &trans);
  auto p = gfx::Point3F(gfx::PointF(*point));
  trans.TransformPointReverse(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool View::ConvertRectForAncestor(const View* ancestor,
                                  gfx::RectF* rect) const {
  gfx::Transform trans;
  // TODO(sad): Have some way of caching the transformation results.
  bool result = GetTransformRelativeTo(ancestor, &trans);
  trans.TransformRect(rect);
  return result;
}

bool View::ConvertRectFromAncestor(const View* ancestor,
                                   gfx::RectF* rect) const {
  gfx::Transform trans;
  bool result = GetTransformRelativeTo(ancestor, &trans);
  trans.TransformRectReverse(rect);
  return result;
}

// Accelerated painting --------------------------------------------------------

void View::CreateLayer(ui::LayerType layer_type) {
  // A new layer is being created for the view. So all the layers of the
  // sub-tree can inherit the visibility of the corresponding view.
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : children_)
      child->UpdateChildLayerVisibility(true);
  }

  SetLayer(std::make_unique<ui::Layer>(layer_type));
  layer()->set_delegate(this);
  layer()->set_name(GetClassName());

  UpdateParentLayers();
  UpdateLayerVisibility();

  // The new layer needs to be ordered in the layer tree according
  // to the view tree. Children of this layer were added in order
  // in UpdateParentLayers().
  if (parent())
    parent()->ReorderLayers();

  Widget* widget = GetWidget();
  if (widget)
    widget->LayerTreeChanged();

  // Before having its own Layer, this View may have painted in to a Layer owned
  // by an ancestor View. Scheduling a paint on the parent View will erase this
  // View's painting effects on the ancestor View's Layer.
  // (See crbug.com/551492)
  SchedulePaintOnParent();
}

bool View::UpdateParentLayers() {
  // Attach all top-level un-parented layers.
  if (layer()) {
    if (!layer()->parent()) {
      UpdateParentLayer();
      return true;
    }
    // The layers of any child views are already in place, so we can stop
    // iterating here.
    return false;
  }
  bool result = false;
  internal::ScopedChildrenLock lock(this);
  for (auto* child : children_) {
    if (child->UpdateParentLayers())
      result = true;
  }
  return result;
}

void View::OrphanLayers() {
  if (layer()) {
    if (layer()->parent())
      layer()->parent()->Remove(layer());

    // The layer belonging to this View has already been orphaned. It is not
    // necessary to orphan the child layers.
    return;
  }
  internal::ScopedChildrenLock lock(this);
  for (auto* child : children_)
    child->OrphanLayers();
}

void View::ReparentLayer(const gfx::Vector2d& offset, ui::Layer* parent_layer) {
  layer()->SetBounds(GetLocalBounds() + offset);
  DCHECK_NE(layer(), parent_layer);
  if (parent_layer)
    parent_layer->Add(layer());
  layer()->SchedulePaint(GetLocalBounds());
  MoveLayerToParent(layer(), LayerOffsetData(layer()->device_scale_factor()));
}

// Input -----------------------------------------------------------------------

bool View::ProcessMousePressed(const ui::MouseEvent& event) {
  int drag_operations =
      (enabled_ && event.IsOnlyLeftMouseButton() &&
       HitTestPoint(event.location())) ?
       GetDragOperations(event.location()) : 0;
  ContextMenuController* context_menu_controller = event.IsRightMouseButton() ?
      context_menu_controller_ : 0;
  View::DragInfo* drag_info = GetDragInfo();

  const bool enabled = enabled_;
  const bool result = OnMousePressed(event);

  if (!enabled)
    return result;

  if (event.IsOnlyRightMouseButton() && context_menu_controller &&
      kContextMenuOnMousePress) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse pressed.
    if (HitTestPoint(event.location())) {
      gfx::Point location(event.location());
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location, ui::MENU_SOURCE_MOUSE);
      return true;
    }
  }

  // WARNING: we may have been deleted, don't use any View variables.
  if (drag_operations != ui::DragDropTypes::DRAG_NONE) {
    drag_info->PossibleDrag(event.location());
    return true;
  }
  return !!context_menu_controller || result;
}

bool View::ProcessMouseDragged(const ui::MouseEvent& event) {
  // Copy the field, that way if we're deleted after drag and drop no harm is
  // done.
  ContextMenuController* context_menu_controller = context_menu_controller_;
  const bool possible_drag = GetDragInfo()->possible_drag;
  if (possible_drag &&
      ExceededDragThreshold(GetDragInfo()->start_pt - event.location()) &&
      (!drag_controller_ ||
       drag_controller_->CanStartDragForView(
           this, GetDragInfo()->start_pt, event.location()))) {
    DoDrag(event, GetDragInfo()->start_pt,
           ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  } else {
    if (OnMouseDragged(event))
      return true;
    // Fall through to return value based on context menu controller.
  }
  // WARNING: we may have been deleted.
  return (context_menu_controller != nullptr) || possible_drag;
}

void View::ProcessMouseReleased(const ui::MouseEvent& event) {
  if (!kContextMenuOnMousePress && context_menu_controller_ &&
      event.IsOnlyRightMouseButton()) {
    // Assume that if there is a context menu controller we won't be deleted
    // from mouse released.
    gfx::Point location(event.location());
    OnMouseReleased(event);
    if (HitTestPoint(location)) {
      ConvertPointToScreen(this, &location);
      ShowContextMenu(location, ui::MENU_SOURCE_MOUSE);
    }
  } else {
    OnMouseReleased(event);
  }
  // WARNING: we may have been deleted.
}

// Accelerators ----------------------------------------------------------------

void View::RegisterPendingAccelerators() {
  if (!accelerators_.get() ||
      registered_accelerator_count_ == accelerators_->size()) {
    // No accelerators are waiting for registration.
    return;
  }

  if (!GetWidget()) {
    // The view is not yet attached to a widget, defer registration until then.
    return;
  }

  accelerator_focus_manager_ = GetFocusManager();
  if (!accelerator_focus_manager_) {
    // Some crash reports seem to show that we may get cases where we have no
    // focus manager (see bug #1291225).  This should never be the case, just
    // making sure we don't crash.
    NOTREACHED();
    return;
  }
  for (std::vector<ui::Accelerator>::const_iterator i(
           accelerators_->begin() + registered_accelerator_count_);
       i != accelerators_->end(); ++i) {
    accelerator_focus_manager_->RegisterAccelerator(
        *i, ui::AcceleratorManager::kNormalPriority, this);
  }
  registered_accelerator_count_ = accelerators_->size();
}

void View::UnregisterAccelerators(bool leave_data_intact) {
  if (!accelerators_.get())
    return;

  if (GetWidget()) {
    if (accelerator_focus_manager_) {
      accelerator_focus_manager_->UnregisterAccelerators(this);
      accelerator_focus_manager_ = nullptr;
    }
    if (!leave_data_intact) {
      accelerators_->clear();
      accelerators_.reset();
    }
    registered_accelerator_count_ = 0;
  }
}

// Focus -----------------------------------------------------------------------

void View::InitFocusSiblings(View* v, int index) {
  int count = child_count();

  if (count == 0) {
    v->next_focusable_view_ = nullptr;
    v->previous_focusable_view_ = nullptr;
  } else {
    if (index == count) {
      // We are inserting at the end, but the end of the child list may not be
      // the last focusable element. Let's try to find an element with no next
      // focusable element to link to.
      View* last_focusable_view = nullptr;
      {
        internal::ScopedChildrenLock lock(this);
        for (auto* child : children_) {
          if (!child->next_focusable_view_) {
            last_focusable_view = child;
            break;
          }
        }
      }
      if (last_focusable_view == nullptr) {
        // Hum... there is a cycle in the focus list. Let's just insert ourself
        // after the last child.
        View* prev = children_[index - 1];
        v->previous_focusable_view_ = prev;
        v->next_focusable_view_ = prev->next_focusable_view_;
        prev->next_focusable_view_->previous_focusable_view_ = v;
        prev->next_focusable_view_ = v;
      } else {
        last_focusable_view->next_focusable_view_ = v;
        v->next_focusable_view_ = nullptr;
        v->previous_focusable_view_ = last_focusable_view;
      }
    } else {
      View* prev = children_[index]->GetPreviousFocusableView();
      v->previous_focusable_view_ = prev;
      v->next_focusable_view_ = children_[index];
      if (prev)
        prev->next_focusable_view_ = v;
      children_[index]->previous_focusable_view_ = v;
    }
  }
}

void View::AdvanceFocusIfNecessary() {
  // Focus should only be advanced if this is the focused view and has become
  // unfocusable. If the view is still focusable or is not focused, we can
  // return early avoiding further unnecessary checks. Focusability check is
  // performed first as it tends to be faster.
  if (IsAccessibilityFocusable() || !HasFocus())
    return;

  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->AdvanceFocusIfNecessary();
}

// System events ---------------------------------------------------------------

void View::PropagateThemeChanged() {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : base::Reversed(children_))
      child->PropagateThemeChanged();
  }
  OnThemeChanged();
}

void View::PropagateDeviceScaleFactorChanged(float old_device_scale_factor,
                                             float new_device_scale_factor) {
  {
    internal::ScopedChildrenLock lock(this);
    for (auto* child : base::Reversed(children_)) {
      child->PropagateDeviceScaleFactorChanged(old_device_scale_factor,
                                               new_device_scale_factor);
    }
  }

  // If the view is drawing to the layer, OnDeviceScaleFactorChanged() is called
  // through LayerDelegate callback.
  if (!layer())
    OnDeviceScaleFactorChanged(old_device_scale_factor,
                               new_device_scale_factor);
}

// Tooltips --------------------------------------------------------------------

void View::UpdateTooltip() {
  Widget* widget = GetWidget();
  // TODO(beng): The TooltipManager nullptr check can be removed when we
  //             consolidate Init() methods and make views_unittests Init() all
  //             Widgets that it uses.
  if (widget && widget->GetTooltipManager())
    widget->GetTooltipManager()->UpdateTooltip();
}

// Drag and drop ---------------------------------------------------------------

bool View::DoDrag(const ui::LocatedEvent& event,
                  const gfx::Point& press_pt,
                  ui::DragDropTypes::DragEventSource source) {
  int drag_operations = GetDragOperations(press_pt);
  if (drag_operations == ui::DragDropTypes::DRAG_NONE)
    return false;

  Widget* widget = GetWidget();
  // We should only start a drag from an event, implying we have a widget.
  DCHECK(widget);

  // Don't attempt to start a drag while in the process of dragging. This is
  // especially important on X where we can get multiple mouse move events when
  // we start the drag.
  if (widget->dragged_view())
    return false;

  OSExchangeData data;
  WriteDragData(press_pt, &data);

  // Message the RootView to do the drag and drop. That way if we're removed
  // the RootView can detect it and avoid calling us back.
  gfx::Point widget_location(event.location());
  ConvertPointToWidget(this, &widget_location);
  widget->RunShellDrag(this, data, widget_location, drag_operations, source);
  // WARNING: we may have been deleted.
  return true;
}

}  // namespace views
