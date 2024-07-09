// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_views.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

TrackedElementViews::TrackedElementViews(View* view,
                                         ui::ElementIdentifier identifier,
                                         ui::ElementContext context)
    : TrackedElement(identifier, context), view_(view) {}

TrackedElementViews::~TrackedElementViews() = default;

gfx::Rect TrackedElementViews::GetScreenBounds() const {
  return view()->GetBoundsInScreen();
}

std::string TrackedElementViews::ToString() const {
  auto result = TrackedElement::ToString();
  result.append(" with view ");
  result.append(view()->GetClassName());
  return result;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementViews)

// Tracks views associated with a specific ui::ElementIdentifier, whether or not
// they are visible or attached to a widget.
class ElementTrackerViews::ElementDataViews : public ViewObserver,
                                              public WidgetObserver {
 public:
  ElementDataViews(ElementTrackerViews* tracker,
                   ui::ElementIdentifier identifier)
      : tracker_(tracker), id_(identifier) {}
  ~ElementDataViews() override = default;

  void AddView(View* view) {
    if (base::Contains(view_data_lookup_, view))
      return;

    const auto it = view_data_.insert(view_data_.end(),
                                      ViewData(view, GetContextForView(view)));
    view_data_lookup_.emplace(view, it);
    view_observer_.AddObservation(view);
    tracker_->MaybeTrackWidget(view->GetWidget());
    UpdateVisible(view);
  }

  void RemoveView(View* view) {
    const auto it = view_data_lookup_.find(view);
    if (it == view_data_lookup_.end())
      return;
    if (it->second->visible()) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          it->second->element.get());
    }
    view_observer_.RemoveObservation(view);
    view_data_.erase(it->second);
    view_data_lookup_.erase(it);
    if (view_data_.empty())
      tracker_->element_data_.erase(id_);
  }

  TrackedElementViews* GetElementForView(View* view) {
    const auto it = view_data_lookup_.find(view);
    CHECK(it != view_data_lookup_.end(), base::NotFatalUntil::M130);
    return it->second->element.get();
  }

  void NotifyViewActivated(View* view) {
    const auto it = view_data_lookup_.find(view);
    CHECK(it != view_data_lookup_.end(), base::NotFatalUntil::M130);
    if (it->second->visible()) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
          it->second->element.get());
    } else {
      DLOG(WARNING)
          << "View " << view->GetClassName()
          << " activated before it was made visible. This probably happened"
             " during a test; it should never happen in a live browser.";
    }
  }

  // When a widget we were previously watching because it had not yet been shown
  // becomes visible, we manually update the visibility of any view on that
  // widget.
  void UpdateViewVisibilityForWidget(Widget* widget) {
    for (auto& entry : view_data_) {
      if (!entry.visible() && entry.view->GetWidget() == widget)
        UpdateVisible(entry.view);
    }
  }

  View* FindFirstViewInContext(ui::ElementContext context) {
    for (const ViewData& data : view_data_) {
      if (data.context == context)
        return data.view;
    }
    return nullptr;
  }

  ViewList FindAllViewsInContext(ui::ElementContext context) {
    ViewList result;
    for (const ViewData& data : view_data_) {
      if (data.context == context)
        result.push_back(data.view);
    }
    return result;
  }

  ViewList GetAllViews() {
    ViewList result;
    base::ranges::transform(view_data_lookup_, std::back_inserter(result),
                            &ViewDataMap::value_type::first);
    return result;
  }

 private:
  enum class UpdateReason { kGeneral, kVisbilityFromRoot, kRemoveFromWidget };

  struct ViewData {
    explicit ViewData(View* v, ui::ElementContext initial_context)
        : view(v), context(initial_context) {}
    bool visible() const { return static_cast<bool>(element); }
    const raw_ptr<View> view;
    ui::ElementContext context;
    std::unique_ptr<TrackedElementViews> element;
  };

  using ViewDataList = std::list<ViewData>;
  using ViewDataMap = std::map<View*, ViewDataList::iterator>;

  // ViewObserver:
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view) override {
    UpdateVisible(observed_view, starting_view->parent()
                                     ? UpdateReason::kGeneral
                                     : UpdateReason::kVisbilityFromRoot);
  }

  void OnViewAddedToWidget(View* observed_view) override {
    tracker_->MaybeTrackWidget(observed_view->GetWidget());
    UpdateVisible(observed_view);
  }

  void OnViewRemovedFromWidget(View* observed_view) override {
    UpdateVisible(observed_view, UpdateReason::kRemoveFromWidget);
  }

  void OnViewIsDeleting(View* observed_view) override {
    RemoveView(observed_view);
  }

  // Returns whether the specified view is visible to the user. Takes the view
  // hierarchy and widget into account.
  bool IsViewVisibleToUser(View* view) {
    const Widget* const widget = view->GetWidget();
    if (!widget || widget->IsClosed() || !tracker_->IsWidgetVisible(widget))
      return false;
    for (; view; view = view->parent()) {
      if (!view->GetVisible())
        return false;
    }
    return true;
  }

  void UpdateVisible(View* view,
                     UpdateReason update_reason = UpdateReason::kGeneral) {
    const auto it = view_data_lookup_.find(view);
    CHECK(it != view_data_lookup_.end(), base::NotFatalUntil::M130);
    ViewData& data = *it->second;
    const ui::ElementContext old_context = data.context;
    data.context = (update_reason == UpdateReason::kRemoveFromWidget)
                       ? ui::ElementContext()
                       : GetContextForView(view);
    const bool was_visible = data.visible();
    const bool visible = it->second->context && IsViewVisibleToUser(view);
    if (visible && !was_visible) {
      data.element =
          std::make_unique<TrackedElementViews>(view, id_, data.context);
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
          data.element.get());
    } else if (!visible && was_visible) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          data.element.get());
      data.element.reset();
    } else if (visible && old_context != data.context) {
      CHECK(update_reason == UpdateReason::kVisbilityFromRoot)
          << "We should always get a removed-from-widget notification before "
             "an added-to-widget notification, the context should never "
             "change while a view is visible.";
      // This can happen in some tests where a widget is closed before it
      // actually becomes visible, or a parent widget is closed underneath us.
      if (!view->GetWidget()->IsVisible()) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
            data.element.get());
        data.element.reset();
      }
    }
  }

  const raw_ptr<ElementTrackerViews> tracker_;
  const ui::ElementIdentifier id_;
  ViewDataList view_data_;
  ViewDataMap view_data_lookup_;
  base::ScopedMultiSourceObservation<View, ViewObserver> view_observer_{this};
};

// Tracks Widgets which are not yet visible, or for which we have received an
// OnWidgetVisibilityChanged(true) event but IsVisible() does not yet report
// true for the Widget.
//
// Therefore, it should only be created and maintained for a Widget for which
// IsVisible() does not return true.
class ElementTrackerViews::WidgetTracker : public WidgetObserver {
 public:
  WidgetTracker(ElementTrackerViews* tracker, Widget* widget)
      : tracker_(tracker), widget_(widget) {
    observation_.Observe(widget);

    // We never want to observe a visible widget; it's unnecessary.
    DCHECK(!widget->IsVisible());
  }

  bool visible() const { return visible_; }

 private:
  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override { Remove(); }
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    // Need to save this for later in case |this| gets deleted.
    auto* const tracker = tracker_.get();

    if (!visible || widget->IsVisible()) {
      // We're in a state in which Widget::IsVisible() should accurately reflect
      // the state of the widget, and therefore do not need to track the Widget.
      Remove();
    } else {
      // We have been told the widget is visible, but the widget is not
      // reporting as visible; therefore we must note this since additional
      // views may be added to the tracker before Widget::IsVisible() becomes
      // true.
      visible_ = true;
    }

    // We might be deleted here so don't use any local data!
    if (visible) {
      for (auto& [id, data] : tracker->element_data_)
        data.UpdateViewVisibilityForWidget(widget);
    }
  }

  void Remove() {
    // Side effect is that `this` is destroyed.
    tracker_->widget_trackers_.erase(widget_);
  }

  const raw_ptr<ElementTrackerViews> tracker_;
  const raw_ptr<Widget> widget_;
  bool visible_ = false;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

ElementTrackerViews::ElementTrackerViews() = default;
ElementTrackerViews::~ElementTrackerViews() = default;

// static
void ElementTrackerViews::SetContextOverrideCallback(
    ContextOverrideCallback callback) {
  GetContextOverrideCallback() = callback;
}

// static
ElementTrackerViews* ElementTrackerViews::GetInstance() {
  static base::NoDestructor<ElementTrackerViews> instance;
  return instance.get();
}

// static
ui::ElementContext ElementTrackerViews::GetContextForView(View* view) {
  Widget* const widget = view->GetWidget();
  return widget ? GetContextForWidget(widget) : ui::ElementContext();
}

// static
ui::ElementContext ElementTrackerViews::GetContextForWidget(Widget* widget) {
  auto* const primary = widget->GetPrimaryWindowWidget();
  if (auto& callback = GetContextOverrideCallback()) {
    if (ui::ElementContext context = callback.Run(primary)) {
      return context;
    }
  }
  return ui::ElementContext(primary);
}

TrackedElementViews* ElementTrackerViews::GetElementForView(
    View* view,
    bool assign_temporary_id) {
  ui::ElementIdentifier identifier = view->GetProperty(kElementIdentifierKey);
  if (!identifier) {
    if (!assign_temporary_id)
      return nullptr;

    // We shouldn't be assigning temporary IDs to views which are not yet on
    // widgets (how did we even get a reference to the view?)
    DCHECK(view->GetWidget());
    identifier = ui::ElementTracker::kTemporaryIdentifier;
    view->SetProperty(kElementIdentifierKey, identifier);
  }
  const auto it = element_data_.find(identifier);
  if (it == element_data_.end()) {
    DCHECK(!assign_temporary_id);
    return nullptr;
  }
  return it->second.GetElementForView(view);
}

const TrackedElementViews* ElementTrackerViews::GetElementForView(
    const View* view) const {
  // Const casts are justified as `assign_temporary_id` = false will not result
  // in any modification to existing data.
  return const_cast<ElementTrackerViews*>(this)->GetElementForView(
      const_cast<View*>(view), false);
}

View* ElementTrackerViews::GetUniqueView(ui::ElementIdentifier id,
                                         ui::ElementContext context) {
  ui::TrackedElement* const element =
      ui::ElementTracker::GetElementTracker()->GetUniqueElement(id, context);
  // Note: this will crash if element is not a TrackedElementViews, but this
  // method *should* crash if the element is present but of the wrong type.
  return element ? element->AsA<TrackedElementViews>()->view() : nullptr;
}

View* ElementTrackerViews::GetFirstMatchingView(ui::ElementIdentifier id,
                                                ui::ElementContext context) {
  const auto it = element_data_.find(id);
  if (it == element_data_.end())
    return nullptr;
  return it->second.FindFirstViewInContext(context);
}

ElementTrackerViews::ViewList ElementTrackerViews::GetAllMatchingViews(
    ui::ElementIdentifier id,
    ui::ElementContext context) {
  const auto it = element_data_.find(id);
  if (it == element_data_.end())
    return ViewList();
  return it->second.FindAllViewsInContext(context);
}

ElementTrackerViews::ViewList
ElementTrackerViews::GetAllMatchingViewsInAnyContext(ui::ElementIdentifier id) {
  const auto it = element_data_.find(id);
  if (it == element_data_.end())
    return ViewList();
  return it->second.GetAllViews();
}

Widget* ElementTrackerViews::GetWidgetForContext(ui::ElementContext context) {
  for (auto& [id, data] : element_data_) {
    auto* const view = data.FindFirstViewInContext(context);
    if (view)
      return view->GetWidget();
  }
  return nullptr;
}

bool ElementTrackerViews::NotifyCustomEvent(
    ui::CustomElementEventType event_type,
    View* view) {
  auto* const element = GetElementForView(view, /* assign_temporary_id =*/true);
  if (!element)
    return false;
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(element,
                                                                event_type);
  return true;
}

void ElementTrackerViews::RegisterView(ui::ElementIdentifier element_id,
                                       View* view) {
  const auto [it, added] =
      element_data_.try_emplace(element_id, this, element_id);
  it->second.AddView(view);
}

void ElementTrackerViews::UnregisterView(ui::ElementIdentifier element_id,
                                         View* view) {
  DCHECK(view);
  const auto it = element_data_.find(element_id);
  CHECK(it != element_data_.end(), base::NotFatalUntil::M130);
  it->second.RemoveView(view);
}

void ElementTrackerViews::NotifyViewActivated(ui::ElementIdentifier element_id,
                                              View* view) {
  DCHECK(view);
  const auto it = element_data_.find(element_id);
  CHECK(it != element_data_.end(), base::NotFatalUntil::M130);
  it->second.NotifyViewActivated(view);
}

// static
ElementTrackerViews::ContextOverrideCallback&
ElementTrackerViews::GetContextOverrideCallback() {
  static base::NoDestructor<ContextOverrideCallback> callback;
  return *callback.get();
}

void ElementTrackerViews::MaybeTrackWidget(Widget* widget) {
  if (!widget || widget->IsVisible())
    return;
  widget_trackers_.try_emplace(widget, this, widget);
}

bool ElementTrackerViews::IsWidgetVisible(const Widget* widget) const {
  if (widget->IsVisible())
    return true;

  const auto it = widget_trackers_.find(widget);
  return it != widget_trackers_.end() && it->second.visible();
}

}  // namespace views
