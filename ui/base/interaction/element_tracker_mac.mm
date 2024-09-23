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

// Note the variation in logging used in this file. For assertions about values
// passed within Chromium, CHECK is used, but for callbacks derived from OS
// notifications, LOG(ERROR) is used, as hard failure is not desired for
// something out of Chromium's control, but a very noisy failure is desired so
// that it can be noticed and fixed.

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
    CHECK(context);
  }

  ~MenuData() {
    LOG_IF(ERROR, !elements_.empty())
        << "Destroying menu data before all elements are hidden.";
    for (auto& [identifier, element] : elements_) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          element.get());
    }
  }

  MenuData(const MenuData& other) = delete;
  void operator=(const MenuData& other) = delete;

  ElementContext context() const { return context_; }

  // Adds an element representing a menu item. The item must not already exist.
  void AddElement(ElementIdentifier identifier,
                  const gfx::Rect& screen_bounds) {
    const auto result =
        elements_.emplace(identifier, std::make_unique<TrackedElementMac>(
                                          identifier, context_, screen_bounds));
    LOG_IF(ERROR, !result.second) << "Element " << identifier << " added twice";
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
        result.first->second.get());
  }

  // Notifies that the specified element has been activated.
  void ActivateElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    if (it == elements_.end()) {
      LOG(ERROR) << "Element " << identifier << " activated after being hidden";
    }
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        it->second.get());
  }

  // Notifies that the element with `identifier` was hidden.
  void HideElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    if (it == elements_.end()) {
      LOG(ERROR) << "Element " << identifier << " hidden multiple times";
      return;
    }
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
        it->second.get());
    elements_.erase(it);
  }

 private:
  const ElementContext context_;

  // Keeps track of all "live" elements being tracked by this object.
  std::map<ElementIdentifier, std::unique_ptr<TrackedElementMac>> elements_;
};

// static
ElementTrackerMac* ElementTrackerMac::GetInstance() {
  static base::NoDestructor<ElementTrackerMac> instance;
  return instance.get();
}

void ElementTrackerMac::NotifyMenuWillShow(NSMenu* menu,
                                           ElementContext context) {
  const auto result = root_menu_to_data_.emplace(menu, context);
  LOG_IF(ERROR, !result.second) << "Menu added twice";
}

void ElementTrackerMac::NotifyMenuDoneShowing(NSMenu* menu) {
  const auto result = root_menu_to_data_.erase(menu);
  LOG_IF(ERROR, !result) << "Menu removed twice";
}

void ElementTrackerMac::NotifyMenuItemShown(NSMenu* menu,
                                            ElementIdentifier identifier,
                                            const gfx::Rect& screen_bounds) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.AddElement(identifier, screen_bounds);
  } else {
    LOG(ERROR) << "Element " << identifier << " shown with unknown menu";
  }
}

void ElementTrackerMac::NotifyMenuItemActivated(NSMenu* menu,
                                                ElementIdentifier identifier) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.ActivateElement(identifier);
  } else {
    LOG(ERROR) << "Element " << identifier << " activated with unknown menu";
  }
}

void ElementTrackerMac::NotifyMenuItemHidden(NSMenu* menu,
                                             ElementIdentifier identifier) {
  const auto it = root_menu_to_data_.find(GetRootMenu(menu));
  if (it != root_menu_to_data_.end()) {
    it->second.HideElement(identifier);
  } else {
    LOG(ERROR) << "Element " << identifier << " hidden with unknown menu";
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
  while (menu.supermenu) {
    menu = menu.supermenu;
  }
  return menu;
}

}  // namespace ui
