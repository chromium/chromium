// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
#define UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Base class for layout managers that can do layout calculation separately
// from layout application. Derived classes must implement
// CalculateProposedLayout(). Used in interpolating and animating layouts.
class VIEWS_EXPORT LayoutManagerBase : public LayoutManager {
 public:
  ~LayoutManagerBase() override;

  View* host_view() { return host_view_; }
  const View* host_view() const { return host_view_; }

  // Fetches a proposed layout for a host view with size |host_size|. If the
  // result had already been calculated, a cached value may be returned.
  ProposedLayout GetProposedLayout(const gfx::Size& host_size) const;

  // Excludes a specific view from the layout when doing layout calculations.
  // Useful when a child view is meant to be displayed but has its size and
  // position managed elsewhere in code. By default, all child views are
  // included in the layout unless they are hidden.
  void SetChildViewIgnoredByLayout(View* child_view, bool ignored);
  bool IsChildViewIgnoredByLayout(const View* child_view) const;

  // LayoutManager:
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;
  SizeBounds GetAvailableSize(const View* host,
                              const View* view) const override;
  void Layout(View* host) final;

 protected:
  LayoutManagerBase();

  // LayoutManager:
  std::vector<View*> GetChildViewsInPaintOrder(const View* host) const override;

  // Direct cache control for subclasses that want to override default caching
  // behavior. Use at your own risk.
  base::Optional<gfx::Size> cached_minimum_size() const {
    return cached_minimum_size_;
  }
  void set_cached_minimum_size(
      const base::Optional<gfx::Size>& minimum_size) const {
    cached_minimum_size_ = minimum_size;
  }
  const base::Optional<gfx::Size>& cached_preferred_size() const {
    return cached_preferred_size_;
  }
  void set_cached_preferred_size(
      const base::Optional<gfx::Size>& preferred_size) const {
    cached_preferred_size_ = preferred_size;
  }
  const base::Optional<gfx::Size>& cached_height_for_width() const {
    return cached_height_for_width_;
  }
  void set_cached_height_for_width(
      const base::Optional<gfx::Size>& height_for_width) const {
    cached_height_for_width_ = height_for_width;
  }
  const base::Optional<gfx::Size>& cached_layout_size() const {
    return cached_layout_size_;
  }
  void set_cached_layout_size(
      const base::Optional<gfx::Size>& layout_size) const {
    cached_layout_size_ = layout_size;
  }
  const ProposedLayout& cached_layout() const { return cached_layout_; }
  void set_cached_layout(const ProposedLayout& layout) const {
    cached_layout_ = layout;
  }

  // Returns the size available to the host view from its parent.
  SizeBounds GetAvailableHostSize() const;

  // Returns true if the specified view is a child of the host view and is not
  // ignored. Views hidden by external code are only included if
  // |include_hidden| is set.
  bool IsChildIncludedInLayout(const View* child,
                               bool include_hidden = false) const;

  // Returns whether the specified child view can be visible. To be able to be
  // visible, |child| must be a child of the host view, and must have been
  // visible when it was added or most recently had GetVisible(true) called on
  // it by non-layout code.
  bool CanBeVisible(const View* child) const;

  // Creates a proposed layout for the host view, including bounds and
  // visibility for all children currently included in the layout.
  virtual ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const = 0;

  // Does the actual work of laying out the host view and its children.
  // Default implementation is just getting the proposed layout for the host
  // size and then applying it.
  virtual void LayoutImpl();

  // Applies |layout| to the children of the host view.
  void ApplyLayout(const ProposedLayout& layout);

  // Invalidates the host view (if present).
  //
  // If |mark_layouts_changed| is true, OnLayoutChanged() will also be called
  // for each layout associated with the host, as if the host were invalidated
  // by external code. If there is no host (yet), the behavior is simulated by
  // invalidating the root layout manager - see GetRootLayoutManager() below.
  void InvalidateHost(bool mark_layouts_changed);

  // The following methods are called on this layout and any owned layouts when
  // e.g. InvalidateLayout(), Installed(), etc. are called, in order to do any
  // additional layout-specific work required. Returns whether the host view
  // must be invalidated as a result of the update. All of these call
  // OnLayoutChanged() by default (see below).
  virtual bool OnChildViewIgnoredByLayout(View* child_view, bool ignored);
  virtual bool OnViewAdded(View* host, View* view);
  virtual bool OnViewRemoved(View* host, View* view);
  virtual bool OnViewVisibilitySet(View* host, View* view, bool visible);

  // Called when the layout is installed in a host view. Default is a no-op.
  virtual void OnInstalled(View* host);

  // Called whenever the layout manager is invalidated, or when the layout may
  // have changed as the result of an operation. Default behavior is to clear
  // all cached data.
  virtual void OnLayoutChanged();

  // Adds an owned layout. Owned layouts receive the same events (Installed(),
  // ViewAdded(), InvalidateLayout(), etc.) as the primary layout. Subclasses of
  // LayoutManagerBase that need to compose or transform the output of one or
  // more embedded layouts should use the |owned_layouts| system.
  template <class T>
  T* AddOwnedLayout(std::unique_ptr<T> owned_layout) {
    T* layout = owned_layout.get();
    AddOwnedLayoutInternal(std::move(owned_layout));
    return layout;
  }

  size_t num_owned_layouts() const { return owned_layouts_.size(); }
  LayoutManagerBase* owned_layout(size_t index) {
    return owned_layouts_[index].get();
  }
  const LayoutManagerBase* owned_layout(size_t index) const {
    return owned_layouts_[index].get();
  }

 private:
  friend class LayoutManagerBaseAvailableSizeTest;

  // Holds bookkeeping data used to determine inclusion of children in the
  // layout.
  struct ChildInfo {
    bool can_be_visible = true;
    bool ignored = false;
  };

  // LayoutManager:
  void InvalidateLayout() final;
  void Installed(View* host) final;
  void ViewAdded(View* host, View* view) final;
  void ViewRemoved(View* host, View* view) final;
  void ViewVisibilitySet(View* host,
                         View* view,
                         bool old_visibility,
                         bool new_visibility) final;

  void AddOwnedLayoutInternal(std::unique_ptr<LayoutManagerBase> owned_layout);

  // Gets the top layout in the ownership chain that includes this layout.
  LayoutManagerBase* GetRootLayoutManager();

  // Do the work of propagating events to owned layouts. Returns true if the
  // host view must be invalidated.
  bool PropagateChildViewIgnoredByLayout(View* child_view, bool ignored);
  bool PropagateViewAdded(View* host, View* view);
  bool PropagateViewRemoved(View* host, View* view);
  bool PropagateViewVisibilitySet(View* host, View* view, bool visible);
  void PropagateInstalled(View* host);
  void PropagateInvalidateLayout();

  View* host_view_ = nullptr;
  std::map<const View*, ChildInfo> child_infos_;
  std::vector<std::unique_ptr<LayoutManagerBase>> owned_layouts_;
  LayoutManagerBase* parent_layout_ = nullptr;

  // Used to suspend invalidation while processing signals from the host view,
  // or while invalidating the host view without invalidating the layout.
  bool suppress_invalidate_ = false;

  // Used during layout to determine if available size has changed for children;
  // when it changes, children are always laid out regardless of visibility or
  // whether their bounds have changed.
  SizeBounds cached_available_size_;

  // Do some really simple caching because layout generation can cost as much
  // as 1ms or more for complex views.
  mutable base::Optional<gfx::Size> cached_minimum_size_;
  mutable base::Optional<gfx::Size> cached_preferred_size_;
  mutable base::Optional<gfx::Size> cached_height_for_width_;
  mutable base::Optional<gfx::Size> cached_layout_size_;
  mutable ProposedLayout cached_layout_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManagerBase);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
