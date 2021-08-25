// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker_mac.h"

#include <map>
#include <memory>

#include "base/logging.h"

namespace ui {

DEFINE_ELEMENT_TRACKER_METADATA(TrackedElementMac)

TrackedElementMac::TrackedElementMac(ElementIdentifier identifier,
                                     ElementContext context)
    : TrackedElement(identifier, context) {}

TrackedElementMac::~TrackedElementMac() = default;

class ElementTrackerMac::ContextData {
 public:
  explicit ContextData(ElementContext context) : context_(context) {
    DCHECK(context);
  }

  ~ContextData() {
    DCHECK(elements_.empty());
    DCHECK(to_be_hidden_.empty());
  }

  ContextData(const ContextData& other) = delete;
  void operator=(const ContextData& other) = delete;

  void AddElement(ElementIdentifier identifier) {
    const auto result = elements_.emplace(
        identifier, std::make_unique<TrackedElementMac>(identifier, context_));
    DCHECK(result.second);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
        result.first->second.get());
  }

  void ActivateElement(ElementIdentifier identifier) {
    const auto it = elements_.find(identifier);
    DCHECK(it != elements_.end());
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        it->second.get());
  }

  void HideElement(ElementIdentifier identifier) {
    const auto result = to_be_hidden_.insert(identifier);
    DCHECK(result.second);
  }

  void DoPendingHides() {
    for (const ElementIdentifier identifier : to_be_hidden_) {
      const auto it = elements_.find(identifier);
      DCHECK(it != elements_.end());
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
          it->second.get());
      elements_.erase(it);
    }
    to_be_hidden_.clear();
  }

 private:
  const ElementContext context_;
  std::map<ElementIdentifier, std::unique_ptr<TrackedElementMac>> elements_;
  std::set<ElementIdentifier> to_be_hidden_;
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
  it2->second->DoPendingHides();
  root_menu_to_context_.erase(it);
  context_to_data_.erase(it2);
}

void ElementTrackerMac::NotifyMenuItemShown(NSMenu* menu,
                                            ElementIdentifier identifier) {
  const ElementContext context = GetContextForMenu(menu);
  DCHECK(context);
  context_to_data_[context]->DoPendingHides();
  context_to_data_[context]->AddElement(identifier);
}

void ElementTrackerMac::NotifyMenuItemHidden(NSMenu* menu,
                                             ElementIdentifier identifier) {
  const ElementContext context = GetContextForMenu(menu);
  DCHECK(context);
  context_to_data_[context]->HideElement(identifier);
}

void ElementTrackerMac::NotifyMenuItemActivated(NSMenu* menu,
                                                ElementIdentifier identifier) {
  const ElementContext context = GetContextForMenu(menu);
  DCHECK(context);
  context_to_data_[context]->ActivateElement(identifier);
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
  DCHECK(it != root_menu_to_context_.end());
  return it->second;
}

}  // namespace ui
