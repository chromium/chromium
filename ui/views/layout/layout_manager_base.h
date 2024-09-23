// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
#define UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/types/pass_key.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Base class for layout managers that can do layout calculation separately
// from layout application. Derived classes must implement
// CalculateProposedLayout(). Used in interpolating and animating layouts.
class VIEWS_EXPORT LayoutManagerBase : public LayoutManager,
                                       public ViewObserver {
 public:
  using PassKeyType = base::NonCopyablePassKey<LayoutManagerBase>;

  LayoutManagerBase(const LayoutManagerBase&) = delete;
  LayoutManagerBase& operator=(const LayoutManagerBase&) = delete;

  ~LayoutManagerBase() override;

  View* host_view() { return host_view_; }
  const View* host_view() const { return host_view_; }

  // Fetches a proposed layout for a host view with size |host_size|. If the
  // result had already been calculated, a cached value may be returned.
  ProposedLayout GetProposedLayout(const gfx::Size& host_size) const;

  // Fetches a proposed layout for a host view with `size_bounds`. This function
  // does not require caching because it is generally used in combination with
  // other LayoutManager.
  ProposedLayout GetProposedLayout(const SizeBounds& size_bounds,
                                   PassKeyType) const;

  // LayoutManager:
  gfx::Size GetPreferredSize(const View* host) const override;
  gfx::Size GetPreferredSize(const View* host,
                             const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize(const View* host) const override;
  int GetPreferredHeightForWidth(const View* host, int width) const override;
  SizeBounds GetAvailableSize(const View* host,
                              const View* view) const override;
  void Layout(View* host) final;

  // ViewObserver:
  void OnViewPropertyChanged(View* observed_view,
                             const void* key,
                             int64_t old_value) final;

  // Returns whether the specified child view can be visible. To be able to be
  // visible, |child| must be a child of the host view, and must have been
  // visible when it was added or most recently had SetVisible(true) called on
  // it by non-layout code.
  bool CanBeVisible(const View* child) const;

 protected:
  LayoutManagerBase();

  PassKeyType PassKey() const { return PassKeyType(); }

  // LayoutManager:
  std::vector<raw_ptr<View, VectorExperimental>> GetChildViewsInPaintOrder(
      const View* host) const override;

  // Direct cache control for subclasses that want to override default caching
  // behavior. Use at your own risk.
  std::optional<gfx::Size> cached_minimum_size() const {
    return cached_minimum_size_;
  }
  void set_cached_minimum_size(
      const std::optional<gfx::Size>& minimum_size) const {
    cached_minimum_size_ = minimum_size;
  }
  const std::optional<gfx::Size>& cached_preferred_size() const {
    return cached_preferred_size_;
  }
  void set_cached_preferred_size(
      const std::optional<gfx::Size>& preferred_size) const {
    cached_preferred_size_ = preferred_size;
  }
  const std::optional<gfx::Size>& cached_height_for_width() const {
    return cached_height_for_width_;
  }
  void set_cached_height_for_width(
      const std::optional<gfx::Size>& height_for_width) const {
    cached_height_for_width_ = height_for_width;
  }
  const std::optional<gfx::Size>& cached_layout_size() const {
    return cached_layout_size_;
  }
  void set_cached_layout_size(
      const std::optional<gfx::Size>& layout_size) const {
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
  virtual bool OnChildViewIncludedInLayoutSet(View* child_view, bool included);
  virtual bool OnViewAdded(View* host, View* view);
  virtual bool OnViewRemoved(View* host, View* view);
  virtual bool OnViewVisibilitySet(View* host, View* view, bool visible);

  // Called when the layout is installed in a host view. Default is a no-op.
  virtual void OnInstalled(View* host);

  // Called whenever the layout manager is invalidated, or when the layout may
  // have changed as the result of an operation. Default behavior is to clear
  // all cached data.
  virtual void OnLayoutChanged();

  // Adds an owned layout. The primary layout propagates events (installation,
  // view addition, etc.) to all owned layouts. Subclasses of LayoutManagerBase
  // that need to compose or transform the output of one or more embedded
  // layouts should use the |owned_layouts| system.
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
  friend class ManualLayoutUtil;
  friend class LayoutManagerBaseAvailableSizeTest;

  // Holds bookkeeping data used to determine inclusion of children in the
  // layout.
  struct ChildInfo {
    bool can_be_visible = true;
    bool included_in_layout = true;
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
  bool PropagateChildViewIncludedInLayout(View* child_view, bool included);
  bool PropagateViewAdded(View* host, View* view);
  bool PropagateViewRemoved(View* host, View* view);
  bool PropagateViewVisibilitySet(View* host, View* view, bool visible);
  void PropagateInstalled(View* host);
  void PropagateInvalidateLayout();

  raw_ptr<View> host_view_ = nullptr;

  // Monitors child views so we will be notified if their "ignored by layout"
  // state changes. This should only ever be observing anything for the root
  // layout manager, which in turn will propagate changes to owned layout
  // managers as needed.
  base::ScopedMultiSourceObservation<View, ViewObserver> view_observations_{
      this};

  std::map<const View*, ChildInfo> child_infos_;
  std::vector<std::unique_ptr<LayoutManagerBase>> owned_layouts_;
  raw_ptr<LayoutManagerBase> parent_layout_ = nullptr;

  // Used to suspend invalidation while processing signals from the host view,
  // or while invalidating the host view without invalidating the layout.
  bool suppress_invalidate_ = false;

  // Used during layout to determine if available size has changed for children;
  // when it changes, children are always laid out regardless of visibility or
  // whether their bounds have changed.
  SizeBounds cached_available_size_;

#if (DCHECK_IS_ON())
  // Used to prevent GetProposedLayout() from being re-entrant.
  mutable bool calculating_layout_ = false;
#endif

  // Do some really simple caching because layout generation can cost as much
  // as 1ms or more for complex views.
  mutable std::optional<gfx::Size> cached_minimum_size_;
  mutable std::optional<gfx::Size> cached_preferred_size_;
  mutable std::optional<gfx::Size> cached_height_for_width_;
  mutable std::optional<gfx::Size> cached_layout_size_;
  mutable ProposedLayout cached_layout_;
};

// Provides methods for doing additional, manual manipulation of a
// `LayoutManagerBase` and its managed Views inside its host View's
// layout implementation, ideally before `LayoutManager::Layout()` is invoked.
//
// In most cases, the layout manager should do all of the layout. However, in
// some cases, specific children of the host may be explicitly manipulated; for
// example, to conditionally show a button which (if visible) should be included
// in the layout.
//
// All of the direct manipulation functions on `LayoutManagerBase` and `View`,
// such as `View::SetVisible()` and
// `LayoutManagerBase::SetChildIncludedInLayout()`, cause cascades of layout
// invalidation up the Views tree, so are not appropriate to be used inside of a
// `Layout()` override. In the case that manual layout manipulation is required
// alongside the use of a layout manager, a `ManualLayoutUtil` should be used
// instead of callin those other methods directly.
//
// This class should only be instantiated and used inside the `Layout()` method
// of a `View` or derived class, before `LayoutManager::Layout()` is invoked.
class VIEWS_EXPORT ManualLayoutUtil {
 public:
  explicit ManualLayoutUtil(LayoutManagerBase* layout_manager);
  ~ManualLayoutUtil();
  ManualLayoutUtil(const ManualLayoutUtil&) = delete;
  void operator=(const ManualLayoutUtil&) = delete;

  // Includes, or excludes and hides, `child_view`.
  //
  // Example:
  // ```
  //  MyView::Layout(PassKey) {
  //    // Only include `foo_button_` in the layout if the feature is enabled;
  //    // otherwise hide it.
  //    ManualLayoutUtil layout_util(flex_layout_.get());
  //    layout_util.SetViewHidden(foo_button_, !foo_enabled);
  //
  //    // Do the standard Views layout, which invokes the layout manager.
  //    LayoutSuperclass<View>(this);
  //  }
  // ```
  //
  // Note that if instead the code had read
  // `foo_button_.SetVisible(foo_enabled)`, the current view and every view up
  // the hierarchy would be invalidated, which could result in a layout loop.
  void SetViewHidden(View* child_view, bool hidden);

  // This is implementation for the method below; the exclusion is ended when
  // the TemporaryExclusion goes out of scope.
  using TemporaryExclusionData =
      std::pair<const raw_ptr<ManualLayoutUtil>, const raw_ptr<View>>;
  struct VIEWS_EXPORT TemporaryExclusionDeleter {
    void operator()(TemporaryExclusionData*) const;
  };
  using TemporaryExclusion =
      std::unique_ptr<TemporaryExclusionData,
                      ManualLayoutUtil::TemporaryExclusionDeleter>;

  // Temporarily removes `child_view` from the layout. Use during manual layout
  // to see how the layout would look without the child view. When the return
  // value goes out of scope, the exclusion is undone.
  [[nodiscard]] TemporaryExclusion TemporarilyExcludeFromLayout(
      View* child_view);

 private:
  void EndTemporaryExclusion(View* child_view);

  const raw_ptr<LayoutManagerBase> layout_manager_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_MANAGER_BASE_H_
