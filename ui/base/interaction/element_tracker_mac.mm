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

class ElementTrackerMac::ContextData {
 public:
  explicit ContextData(ElementContext context) : context_(context) {
    DCHECK(context);
  }

  ~ContextData() {
    for (const auto& element : elements_) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          element.second.get());
    }
  }

  ContextData(const ContextData& other) = delete;
  void operator=(const ContextData& other) = delete;

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
    CHECK(it != elements_.end());
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        it->second.get());
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
  const auto result = root_menu_to_context_.emplace(menu, context);
  DCHECK(result.second);
  const auto result2 =
      context_to_data_.emplace(context, std::make_unique<ContextData>(context));
  DCHECK(result2.second);
}

void ElementTrackerMac::NotifyMenuDoneShowing(NSMenu* menu) {
  const auto it = root_menu_to_context_.find(menu);
  DCHECK(it != root_menu_to_context_.end());
  const auto it2 = context_to_data_.find(it->second);
  DCHECK(it2 != context_to_data_.end());
  root_menu_to_context_.erase(it);
  context_to_data_.erase(it2);
}

void ElementTrackerMac::NotifyMenuItemShown(NSMenu* menu,
                                            ElementIdentifier identifier,
                                            const gfx::Rect& screen_bounds) {
  const ElementContext context = GetContextForMenu(menu);
  if (context)
    context_to_data_[context]->AddElement(identifier, screen_bounds);
}

void ElementTrackerMac::NotifyMenuItemActivated(NSMenu* menu,
                                                ElementIdentifier identifier) {
  const ElementContext context = GetContextForMenu(menu);
  if (context)
    context_to_data_[context]->ActivateElement(identifier);
}

void ElementTrackerMac::NotifyMenuItemHidden(NSMenu* menu,
                                             ElementIdentifier identifier) {
  const ElementContext context = GetContextForMenu(menu);
  if (context)
    context_to_data_[context]->HideElement(identifier);
}

NSMenu* ElementTrackerMac::GetRootMenuForContext(ElementContext context) {
  for (auto [menu, menu_context] : root_menu_to_context_) {
    if (menu_context == context)
      return menu;
  }
  return nullptr;
}

ElementTrackerMac::ElementTrackerMac() = default;
ElementTrackerMac::~ElementTrackerMac() = default;

NSMenu* ElementTrackerMac::GetRootMenu(NSMenu* menu) const {
  while ([menu supermenu])
    menu = [menu supermenu];
  return menu;
}

ElementContext ElementTrackerMac::GetContextForMenu(NSMenu* menu) const {
  menu = GetRootMenu(menu);
  const auto it = root_menu_to_context_.find(menu);
  return it == root_menu_to_context_.end() ? ElementContext() : it->second;
}

}  // namespace ui
