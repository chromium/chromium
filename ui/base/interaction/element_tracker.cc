// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker.h"

#include <algorithm>
#include <iterator>
#include <list>
#include <map>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

class ElementTracker::ElementData {
 public:
  ElementData(ElementTracker* tracker,
              ElementIdentifier id,
              ElementContext context)
      : identifier_(id), context_(context) {
    auto removal_callback =
        base::BindRepeating(&ElementTracker::MaybeCleanup,
                            base::Unretained(tracker), base::Unretained(this));
    shown_callbacks_.set_removal_callback(removal_callback);
    activated_callbacks_.set_removal_callback(removal_callback);
    hidden_callbacks_.set_removal_callback(removal_callback);
  }

  ~ElementData() = default;

  ElementIdentifier identifier() const { return identifier_; }
  ElementContext context() const { return context_; }

  bool empty() const {
    return elements_.empty() && shown_callbacks_.empty() &&
           activated_callbacks_.empty() && hidden_callbacks_.empty();
  }

  size_t num_elements() const {
    // Guaranteed O(1) in C++11 and later.
    return elements_.size();
  }

  bool processing_removal() const { return processing_removal_; }

  const std::list<ElementTrackerElement*>& elements() const {
    return elements_;
  }

  Subscription AddElementShownCallback(Callback callback) {
    return shown_callbacks_.Add(callback);
  }

  Subscription AddElementActivatedCallback(Callback callback) {
    return activated_callbacks_.Add(callback);
  }

  Subscription AddElementHiddenCallback(Callback callback) {
    return hidden_callbacks_.Add(callback);
  }

  void NotifyElementShown(ElementTrackerElement* element) {
    DCHECK_EQ(identifier().raw_value(), element->identifier().raw_value());
    DCHECK_EQ(static_cast<intptr_t>(context()),
              static_cast<intptr_t>(element->context()));
    const auto it = elements_.insert(elements_.end(), element);
    const bool success = element_lookup_.emplace(element, it).second;
    DCHECK(success);
    shown_callbacks_.Notify(element);
  }

  void NotifyElementActivated(ElementTrackerElement* element) {
    DCHECK(base::Contains(element_lookup_, element));
    activated_callbacks_.Notify(element);
  }

  void NotifyElementHidden(ElementTrackerElement* element) {
    // We don't want to delete this object during a callback while we're in the
    // middle of cleaning up the object, so put a guard around this operation.
    base::AutoReset<bool> guard(&processing_removal_, true);
    const auto it = element_lookup_.find(element);
    DCHECK(it != element_lookup_.end());
    elements_.erase(it->second);
    element_lookup_.erase(it);
    hidden_callbacks_.Notify(element);
  }

 private:
  const ElementIdentifier identifier_;
  const ElementContext context_;
  bool processing_removal_ = false;

  // Holds elements in the order they were added to this data block, so that the
  // first element or the first element that matches some criterion can be
  // easily found.
  std::list<ElementTrackerElement*> elements_;

  // Provides a fast lookup into `elements_` by element for checking and
  // removal. Since there could be many elements (e.g. tabs in a browser) we
  // don't want removing a series of them to turn into an O(n^2) operation.
  std::map<ElementTrackerElement*, std::list<ElementTrackerElement*>::iterator>
      element_lookup_;

  base::RepeatingCallbackList<void(ElementTrackerElement*)> shown_callbacks_;
  base::RepeatingCallbackList<void(ElementTrackerElement*)>
      activated_callbacks_;
  base::RepeatingCallbackList<void(ElementTrackerElement*)> hidden_callbacks_;
};

ElementTrackerElement::ElementTrackerElement(ElementIdentifier id,
                                             ElementContext context)
    : identifier_(id), context_(context) {}

ElementTrackerElement::~ElementTrackerElement() = default;

// static
ElementTracker* ElementTracker::GetElementTracker() {
  static base::NoDestructor<ElementTracker> instance;
  return instance.get();
}

// static
ElementTrackerFrameworkDelegate* ElementTracker::GetFrameworkDelegate() {
  return static_cast<ElementTrackerFrameworkDelegate*>(GetElementTracker());
}

ElementTrackerElement* ElementTracker::GetUniqueElement(
    ElementIdentifier id,
    ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  if (it == element_data_.end() || it->second->num_elements() == 0)
    return nullptr;
  DCHECK_EQ(1U, it->second->num_elements());
  return it->second->elements().front();
}

ElementTrackerElement* ElementTracker::GetFirstMatchingElement(
    ElementIdentifier id,
    ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  if (it == element_data_.end() || it->second->num_elements() == 0)
    return nullptr;
  return it->second->elements().front();
}

ElementTracker::ElementList ElementTracker::GetAllMatchingElements(
    ElementIdentifier id,
    ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  ElementList result;
  if (it != element_data_.end()) {
    std::copy(it->second->elements().begin(), it->second->elements().end(),
              std::back_inserter(result));
  }
  return result;
}

bool ElementTracker::IsElementVisible(ElementIdentifier id,
                                      ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  return it != element_data_.end() && it->second->num_elements() > 0;
}

ElementTracker::Subscription ElementTracker::AddElementShownCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  return GetOrAddElementData(id, context)->AddElementShownCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddElementActivatedCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  return GetOrAddElementData(id, context)
      ->AddElementActivatedCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddElementHiddenCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  return GetOrAddElementData(id, context)->AddElementHiddenCallback(callback);
}

ElementTracker::ElementTracker() = default;
ElementTracker::~ElementTracker() = default;

void ElementTracker::NotifyElementShown(ElementTrackerElement* element) {
  DCHECK(!base::Contains(element_to_data_lookup_, element));
  ElementData* const element_data =
      GetOrAddElementData(element->identifier(), element->context());
  element_data->NotifyElementShown(element);
  element_to_data_lookup_.emplace(element, element_data);
}

void ElementTracker::NotifyElementActivated(ElementTrackerElement* element) {
  const auto it = element_to_data_lookup_.find(element);
  DCHECK(it != element_to_data_lookup_.end());
  it->second->NotifyElementActivated(element);
}

void ElementTracker::NotifyElementHidden(ElementTrackerElement* element) {
  const auto it = element_to_data_lookup_.find(element);
  DCHECK(it != element_to_data_lookup_.end());
  ElementData* const data = it->second;
  data->NotifyElementHidden(element);
  element_to_data_lookup_.erase(it);
  MaybeCleanup(data);
}

ElementTracker::ElementData* ElementTracker::GetOrAddElementData(
    ElementIdentifier id,
    ElementContext context) {
  const LookupKey key(id, context);
  auto it = element_data_.find(key);
  if (it == element_data_.end()) {
    const auto result = element_data_.emplace(
        key, std::make_unique<ElementData>(this, id, context));
    DCHECK(result.second);
    it = result.first;
  }
  return it->second.get();
}

void ElementTracker::MaybeCleanup(ElementData* data) {
  // If there is still data, or we're in the middle of processing an element
  // being removed, do not clean up this data block.
  if (!data->empty() || data->processing_removal())
    return;

  const auto result =
      element_data_.erase(LookupKey(data->identifier(), data->context()));
  DCHECK(result);
}

}  // namespace ui
