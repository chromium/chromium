// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker_mac.h"

#include <map>
#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedElementMac)

TrackedElementMac::TrackedElementMac(ElementIdentifier identifier,
                                     ElementContext context,
                                     const gfx::Rect& screen_bounds)
    : TrackedElement(identifier, context), screen_bounds_(screen_bounds) {}

TrackedElementMac::~TrackedElementMac() = default;

gfx::Rect TrackedElementMac::GetScreenBounds() const {
  return screen_bounds_;
}

// Holds all data regarding elements in a specific NSMenu or its children, and
// handles dispatching of ElementTracker events.
class ElementTrackerMac::MenuData {
 public:
  explicit MenuData(ElementContext context) : context_(context) {
    DCHECK(context);
  }

  ~MenuData() {
    HideElements(recycle_bin_);
    LOG_IF(WARNING, !elements_.empty())
        << "Destroying menu data before all elements are hidden.";
    HideElements(elements_);
  }

  MenuData(const MenuData& other) = delete;
  void operator=(const MenuData& other) = delete;

  ElementContext context() const { return context_; }

  // Adds an element representing a menu item. The item must not already exist.
  void AddElement(ElementIdentifier identifier,
                  const gfx::Rect& screen_bounds) {
    // On show of a new menu or submenu, clear out any menu items waiting to be
    // garbage-collected, since an activation won't happen.
    HideElements(recycle_bin_);

    // Insert an entry for the new element and verify that it is correct.
    const auto result =
        elements_.emplace(identifier, std::make_unique<TrackedElementMac>(
                                          identifier, context_, screen_bounds));
    DCHECK(result.second);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
        result.first->second.get());
  }

  // Notifies that the specified element has been activated. Also checks
  // elements in the recycle bin, in case hide and activate came arrived in the
  // wrong order.
  void ActivateElement(ElementIdentifier identifier) {
    auto it = elements_.find(identifier);
    if (it == elements_.end()) {
      it = recycle_bin_.find(identifier);
      if (it == recycle_bin_.end()) {
        NOTREACHED()
            << "Element " << identifier
            << " had its activation sent after its menu was destroyed. This "
               "may be due to a race condition with renderer context menus; "
               "see crbug.com/1418614 for the state of current efforts to "
               "diagnose and fix the problem.";
        return;
      }
      LOG(WARNING) << "Element " << identifier
                   << " activated after hide signal received.";
    }
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        it->second.get());
  }

  // Notifies that the element with `identifier` was hidden. Instead of deleting
  // the element right away, it is instead put in the `recycle_bin_` for
  // disposal when it is certain no activation event will occur.
  void HideElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    if (it == elements_.end()) {
      if (base::Contains(recycle_bin_, identifier)) {
        LOG(WARNING) << "Element " << identifier
                     << " hidden multiple times in a row.";
      } else {
        NOTREACHED() << "Element " << identifier
                     << " hidden after its menu was destroyed.";
      }
      return;
    }
    auto result = recycle_bin_.emplace(identifier, std::move(it->second));
    DCHECK(result.second);
    elements_.erase(it);
  }

 private:
  using ElementMap =
      std::map<ElementIdentifier, std::unique_ptr<TrackedElementMac>>;

  // Hides all of the elements in `map` and then clears the map. Used to do
  // final disposal of TrackedElementMac objects owned by this object.
  void HideElements(ElementMap& map) {
    for (auto& [identifier, element] : map) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          element.get());
    }
    map.clear();
  }

  const ElementContext context_;

  // Keeps track of all "live" elements being tracked by this object. When they
  // are hidden, they are moved to `recycle_bin_` to await final disposal.
  ElementMap elements_;

  // Because activation and hide events can come in reverse order in some corner
  // cases (see crbug.com/1432480) elements are kept around between the time
  // they are notified as hidden and when they are actually destroyed, which is
  // when the menu is destroyed or when a new submenu opens.
  ElementMap recycle_bin_;
};

// static
ElementTrackerMac* ElementTrackerMac::GetInstance() {
  static base::NoDestructor<ElementTrackerMac> instance;
  return instance.get();
}

void ElementTrackerMac::NotifyMenuWillShow(NSMenu* menu,
                                           ElementContext context) {
  const auto result = root_menu_to_data_.emplace(menu, context);
  DCHECK(result.second);
}

void ElementTrackerMac::NotifyMenuDoneShowing(NSMenu* menu) {
  const auto result = root_menu_to_data_.erase(menu);
  DCHECK(result);
}

void ElementTrackerMac::NotifyMenuItemShown(NSMenu* menu,
                                            ElementIdentifier identifier,
                                            const gfx::Rect& screen_bounds) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.AddElement(identifier, screen_bounds);
  }
}

void ElementTrackerMac::NotifyMenuItemActivated(NSMenu* menu,
                                                ElementIdentifier identifier) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.ActivateElement(identifier);
  }
}

void ElementTrackerMac::NotifyMenuItemHidden(NSMenu* menu,
                                             ElementIdentifier identifier) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.HideElement(identifier);
  }
}

NSMenu* ElementTrackerMac::GetRootMenuForContext(ElementContext context) {
  for (auto& [menu, data] : root_menu_to_data_) {
    if (data.context() == context) {
      return menu;
    }
  }
  return nullptr;
}

ElementTrackerMac::ElementTrackerMac() = default;
ElementTrackerMac::~ElementTrackerMac() = default;

NSMenu* ElementTrackerMac::GetRootMenu(NSMenu* menu) const {
  while ([menu supermenu]) {
    menu = [menu supermenu];
  }
  return menu;
}

}  // namespace ui
