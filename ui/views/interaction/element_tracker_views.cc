// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_views.h"

#include <list>
#include <map>

#include "base/containers/contains.h"
#include "base/logging.h"
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

DEFINE_ELEMENT_TRACKER_METADATA(TrackedElementViews)

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

 private:
  struct ViewData {
    explicit ViewData(View* v, ui::ElementContext initial_context)
        : view(v), context(initial_context) {}
    bool visible() const { return static_cast<bool>(element); }
    View* const view;
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

  ElementTrackerViews* const tracker_;
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
  const Widget* const widget = view->GetWidget();
  return widget ? ui::ElementContext(widget->GetPrimaryWindowWidget())
                : ui::ElementContext();
}

TrackedElementViews* ElementTrackerViews::GetElementForView(View* view) {
  const auto identifier = view->GetProperty(kElementIdentifierKey);
  if (!identifier)
    return nullptr;
  const auto it = element_data_.find(identifier);
  if (it == element_data_.end())
    return nullptr;
  return it->second->GetElementForView(view);
}

void ElementTrackerViews::RegisterView(ui::ElementIdentifier element_id,
                                       View* view) {
  auto it = element_data_.find(element_id);
  if (it == element_data_.end()) {
    it = element_data_
             .emplace(element_id,
                      std::make_unique<ElementDataViews>(this, element_id))
             .first;
  }
  it->second->AddView(view);
}

void ElementTrackerViews::UnregisterView(ui::ElementIdentifier element_id,
                                         View* view) {
  DCHECK(view);
  const auto it = element_data_.find(element_id);
  DCHECK(it != element_data_.end());
  it->second->RemoveView(view);
}

void ElementTrackerViews::NotifyViewActivated(ui::ElementIdentifier element_id,
                                              View* view) {
  DCHECK(view);
  const auto it = element_data_.find(element_id);
  DCHECK(it != element_data_.end());
  it->second->NotifyViewActivated(view);
}

void ElementTrackerViews::OnWidgetVisibilityChanged(Widget* widget,
                                                    bool visible) {
  if (!visible)
    return;
  for (auto& entry : element_data_)
    entry.second->UpdateViewVisibilityForWidget(widget);
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
