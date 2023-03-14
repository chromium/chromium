// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker_mac.h"

#include <map>
#include <memory>

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

class ElementTrackerMac::MenuData {
 public:
  explicit MenuData(ElementContext context) : context_(context) {
    DCHECK(context);
  }

  ~MenuData() {
    for (const auto& element : elements_) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          element.second.get());
    }
  }

  MenuData(const MenuData& other) = delete;
  void operator=(const MenuData& other) = delete;

  ElementContext context() const { return context_; }

  void AddElement(ElementIdentifier identifier,
                  const gfx::Rect& screen_bounds) {
    const auto result =
        elements_.emplace(identifier, std::make_unique<TrackedElementMac>(
                                          identifier, context_, screen_bounds));
    DCHECK(result.second);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
        result.first->second.get());
  }

  void ActivateElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    if (it != elements_.end()) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
          it->second.get());
    } else {
      NOTREACHED() << "Element " << identifier
                   << " had its activation sent after it was hidden. This may "
                      "be due to a race condition with renderer context menus; "
                      "see crbug.com/1418614 for the state of current efforts "
                      "to diagnose and fix the problem.";
    }
  }

  void HideElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    DCHECK(it != elements_.end());
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
        it->second.get());
    elements_.erase(it);
  }

 private:
  const ElementContext context_;
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
