// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_
#define UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class Widget;

// Wraps a View in an ui::TrackedElement.
class VIEWS_EXPORT TrackedElementViews : public ui::TrackedElement {
 public:
  TrackedElementViews(View* view,
                      ui::ElementIdentifier identifier,
                      ui::ElementContext context);
  ~TrackedElementViews() override;

  View* view() { return view_; }
  const View* view() const { return view_; }

  // TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  std::string ToString() const override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

 private:
  const raw_ptr<View> view_;
};

// Manages TrackedElements associated with View objects.
class VIEWS_EXPORT ElementTrackerViews {
 public:
  using ViewList = std::vector<View*>;

  // Returns the context to use for `primary_widget`, which will be a logical
  // top-level widget, or null to use the default computation. Used e.g. to
  // ensure that all system widgets on ChromeOS share a context.
  using ContextOverrideCallback =
      base::RepeatingCallback<ui::ElementContext(Widget* primary_widget)>;

  // Changes the computation of contexts for some or all widgets. See
  // `ContextOverrideCallback` for details.
  static void SetContextOverrideCallback(ContextOverrideCallback callback);

  // Gets the global instance of the tracker for Views.
  static ElementTrackerViews* GetInstance();

  // Returns the context associated with a particular View. The context will be
  // the same across all Views associated with a root Widget (such as an
  // application window), or as specified by the current
  // `ContextOverrideCallback` if one is set.
  static ui::ElementContext GetContextForView(View* view);

  // Returns the context associated with a particular Widget. The context will
  // be the same across all Widgets associated with a root Widget (such as an
  // application window), or as specified by the current
  // `ContextOverrideCallback` if one is set.
  static ui::ElementContext GetContextForWidget(Widget* widget);

  // ----------
  // The following methods retrieve Views directly without having to retrieve a
  // TrackedElement from ElementTracker, call, AsA<>, etc. Use if you know that
  // the element(s) in question are guaranteed to be Views.

  // Returns the corresponding TrackedElementViews for the given view, or
  // null if none exists. Note that views which are not visible or not added to
  // a Widget may not have associated elements, and that the returned object
  // may be transient.
  //
  // For the non-const version, if `assign_temporary_id` is set and `view` does
  // not have an identifier, ui::ElementTracker::kTemporaryIdentifier will be
  // assigned.
  TrackedElementViews* GetElementForView(View* view,
                                         bool assign_temporary_id = false);
  const TrackedElementViews* GetElementForView(const View* view) const;

  // Returns either the unique View matching the given `id` in the given
  // `context`, or null if there is none.
  //
  // Use if you are sure there's at most one matching element in the context
  // and that (if present) the element is a View; will DCHECK/crash otherwise.
  View* GetUniqueView(ui::ElementIdentifier id, ui::ElementContext context);

  // Convenience method that calls GetUniqueView() and then safely converts the
  // result to `T`, which must be a View subclass with metadata. Fails if a View
  // is found but is not of the expected subtype.
  template <class T>
  T* GetUniqueViewAs(ui::ElementIdentifier id, ui::ElementContext context);

  // Returns the first View with the given `id` in the given `context`; null if
  // none is found. Ignores all other Views and any matching elements that are
  // not Views.
  //
  // Use when you just need *a* View in the given context, and don't care if
  // there's more than one.
  View* GetFirstMatchingView(ui::ElementIdentifier id,
                             ui::ElementContext context);

  // Convenience method that calls GetFirstMatchingView() and then safely
  // converts the result to `T`, which must be a view subclass with metadata.
  // Fails if a View is found but is not of the expected subtype.
  template <class T>
  T* GetFirstMatchingViewAs(ui::ElementIdentifier id,
                            ui::ElementContext context);

  // Returns a list of all visible Views with identifier `id` in `context`.
  // The list may be empty. Ignores any non-Views elements which might match.
  ViewList GetAllMatchingViews(ui::ElementIdentifier id,
                               ui::ElementContext context);

  // Returns a list of all visible Views with identifier `id` in any context.
  // Order is not guaranteed. Ignores any non-Views elements with the same
  // identifier.
  ViewList GetAllMatchingViewsInAnyContext(ui::ElementIdentifier id);

  // Returns a widget that matches the given context. A valid
  // TrackedElementViews must exist within the widget.
  Widget* GetWidgetForContext(ui::ElementContext context);

  // ----------
  // Notifies listeners that a specific custom event has occurred for the given
  // view. Calls GetElementForView(view, true) under the hood; returns false if
  // an element cannot be found or created for the view (e.g. in the case where
  // it is not visible or associated with a widget).
  bool NotifyCustomEvent(ui::CustomElementEventType event_type, View* view);

  // ----------
  // The following methods are used by View and derived classes to send events
  // to ElementTracker. ElementTrackerViews maintains additional observers and
  // states that make sure shown and hidden events get sent at the correct
  // times.

  // Called by View after the kUniqueElementKey property is set.
  void RegisterView(ui::ElementIdentifier element_id, View* view);

  // Called by View if the kUniqueElementKey property is changed from a non-null
  // value.
  void UnregisterView(ui::ElementIdentifier element_id, View* view);

  // Called by a View when the user activates it (clicks a button, selects a
  // menu item, etc.)
  void NotifyViewActivated(ui::ElementIdentifier element_id, View* view);

 private:
  friend class base::NoDestructor<ElementTrackerViews>;
  FRIEND_TEST_ALL_PREFIXES(ElementTrackerViewsTest, CleansUpWidgetTrackers);
  class ElementDataViews;
  class WidgetTracker;

  ElementTrackerViews();
  ~ElementTrackerViews();

  // Returns the current `ContextOverrideCallback`, which may be null.
  static ContextOverrideCallback& GetContextOverrideCallback();

  // We do not get notified at the View level if a view's widget has not yet
  // been shown. We need this notification to know when the view is actually
  // visible to the user. So if a view is added to the trakcer or is added to
  // a widget, and its widget is not visible, we watch it until it is (or it is
  // destroyed).
  void MaybeTrackWidget(Widget* widget);

  // Keep track of widgets for which we've received an
  // OnWidgetVisibilityChanged(true) event for but which are still reporting
  // IsVisible() = false. This happens because visibility of native window in
  // Aura is not exactly synced with our event reporting.
  bool IsWidgetVisible(const Widget* widget) const;

  std::map<ui::ElementIdentifier, ElementDataViews> element_data_;
  std::map<const Widget*, WidgetTracker> widget_trackers_;
};

// Template implementations.

template <class T>
T* ElementTrackerViews::GetUniqueViewAs(ui::ElementIdentifier id,
                                        ui::ElementContext context) {
  views::View* const view = GetUniqueView(id, context);
  if (!view)
    return nullptr;
  T* const result = views::AsViewClass<T>(view);
  DCHECK(result);
  return result;
}

template <class T>
T* ElementTrackerViews::GetFirstMatchingViewAs(ui::ElementIdentifier id,
                                               ui::ElementContext context) {
  views::View* const view = GetFirstMatchingView(id, context);
  if (!view)
    return nullptr;
  T* const result = views::AsViewClass<T>(view);
  DCHECK(result);
  return result;
}

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_ELEMENT_TRACKER_VIEWS_H_
