// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_views.h"

#include <algorithm>
#include <list>
#include <map>
#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

// Returns whether the specified view is visible to the user. Takes the view
// hierarchy and widget into account.
bool IsViewVisibleToUser(View* view, bool force_widget_visible = false) {
  const Widget* const widget = view->GetWidget();
  if (!widget || (!force_widget_visible && !widget->IsVisible()))
    return false;
  for (; view; view = view->parent()) {
    if (!view->GetVisible())
      return false;
  }
  return true;
}

}  // namespace

TrackedElementViews::TrackedElementViews(View* view,
                                         ui::ElementIdentifier identifier,
                                         ui::ElementContext context)
    : TrackedElement(identifier, context), view_(view) {}

TrackedElementViews::~TrackedElementViews() = default;

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementViews)

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
    tracker_->MaybeObserveWidget(view->GetWidget());
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
    DCHECK(it != view_data_lookup_.end());
    return it->second->element.get();
  }

  void NotifyViewActivated(View* view) {
    const auto it = view_data_lookup_.find(view);
    DCHECK(it != view_data_lookup_.end());
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
        UpdateVisible(entry.view, /* is_remove */ false,
                      /* force_widget_visible */ true);
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
    std::transform(view_data_lookup_.begin(), view_data_lookup_.end(),
                   std::back_inserter(result),
                   [](const auto& pr) { return pr.first; });
    return result;
  }

 private:
  struct ViewData {
    explicit ViewData(View* v, ui::ElementContext initial_context)
        : view(v), context(initial_context) {}
    bool visible() const { return static_cast<bool>(element); }
    const raw_ptr<View> view;
    ui::ElementContext context;
    std::unique_ptr<TrackedElementViews> element;
  };

  using ViewDataList = std::list<ViewData>;

  // ViewObserver:
  void OnViewVisibilityChanged(View* observed_view,
                               View* starting_view) override {
    UpdateVisible(observed_view);
  }

  void OnViewAddedToWidget(View* observed_view) override {
    tracker_->MaybeObserveWidget(observed_view->GetWidget());
    UpdateVisible(observed_view);
  }

  void OnViewRemovedFromWidget(View* observed_view) override {
    UpdateVisible(observed_view, /* is_remove */ true);
  }

  void OnViewIsDeleting(View* observed_view) override {
    RemoveView(observed_view);
  }

  void UpdateVisible(View* view,
                     bool is_remove = false,
                     bool force_widget_visible = false) {
    const auto it = view_data_lookup_.find(view);
    DCHECK(it != view_data_lookup_.end());
    ViewData& data = *it->second;
    const ui::ElementContext old_context = data.context;
    data.context = is_remove ? ui::ElementContext() : GetContextForView(view);
    const bool was_visible = data.visible();
    const bool visible =
        it->second->context && IsViewVisibleToUser(view, force_widget_visible);
    if (visible && !was_visible) {
      data.element =
          std::make_unique<TrackedElementViews>(view, id_, data.context);
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
          data.element.get());
    } else if (!visible && was_visible) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          data.element.get());
      data.element.reset();
    }
    DCHECK(!visible || !was_visible || old_context == data.context)
        << "We should always get a removed-from-widget notification before an "
           "added-to-widget notification, the context should never change "
           "while a view is visible.";
  }

  const raw_ptr<ElementTrackerViews> tracker_;
  const ui::ElementIdentifier id_;
  ViewDataList view_data_;
  std::map<View*, ViewDataList::iterator> view_data_lookup_;
  base::ScopedMultiSourceObservation<View, ViewObserver> view_observer_{this};
};

ElementTrackerViews::ElementTrackerViews() = default;
ElementTrackerViews::~ElementTrackerViews() = default;

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
  return ui::ElementContext(widget->GetPrimaryWindowWidget());
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
  DCHECK(it != element_data_.end());
  it->second.RemoveView(view);
}

void ElementTrackerViews::NotifyViewActivated(ui::ElementIdentifier element_id,
                                              View* view) {
  DCHECK(view);
  const auto it = element_data_.find(element_id);
  DCHECK(it != element_data_.end());
  it->second.NotifyViewActivated(view);
}

void ElementTrackerViews::OnWidgetVisibilityChanged(Widget* widget,
                                                    bool visible) {
  if (!visible)
    return;
  for (auto& [id, data] : element_data_)
    data.UpdateViewVisibilityForWidget(widget);
  widget_observer_.RemoveObservation(widget);
}

void ElementTrackerViews::OnWidgetDestroying(Widget* widget) {
  widget_observer_.RemoveObservation(widget);
}

void ElementTrackerViews::MaybeObserveWidget(Widget* widget) {
  if (!widget || widget->IsVisible() ||
      widget_observer_.IsObservingSource(widget)) {
    return;
  }
  widget_observer_.AddObservation(widget);
}

}  // namespace views
