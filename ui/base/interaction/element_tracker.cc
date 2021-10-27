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

  const std::list<TrackedElement*>& elements() const { return elements_; }

  Subscription AddElementShownCallback(Callback callback) {
    return shown_callbacks_.Add(callback);
  }

  Subscription AddElementActivatedCallback(Callback callback) {
    return activated_callbacks_.Add(callback);
  }

  Subscription AddElementHiddenCallback(Callback callback) {
    return hidden_callbacks_.Add(callback);
  }

  void NotifyElementShown(TrackedElement* element) {
    DCHECK_EQ(identifier().raw_value(), element->identifier().raw_value());
    DCHECK_EQ(static_cast<intptr_t>(context()),
              static_cast<intptr_t>(element->context()));
    const auto it = elements_.insert(elements_.end(), element);
    const bool success = element_lookup_.emplace(element, it).second;
    DCHECK(success);
    shown_callbacks_.Notify(element);
  }

  void NotifyElementActivated(TrackedElement* element) {
    DCHECK(base::Contains(element_lookup_, element));
    activated_callbacks_.Notify(element);
  }

  void NotifyElementHidden(TrackedElement* element) {
    const auto it = element_lookup_.find(element);
    DCHECK(it != element_lookup_.end());
    elements_.erase(it->second);
    element_lookup_.erase(it);
    hidden_callbacks_.Notify(element);
  }

 private:
  const ElementIdentifier identifier_;
  const ElementContext context_;

  // Holds elements in the order they were added to this data block, so that the
  // first element or the first element that matches some criterion can be
  // easily found.
  std::list<TrackedElement*> elements_;

  // Provides a fast lookup into `elements_` by element for checking and
  // removal. Since there could be many elements (e.g. tabs in a browser) we
  // don't want removing a series of them to turn into an O(n^2) operation.
  std::map<TrackedElement*, std::list<TrackedElement*>::iterator>
      element_lookup_;

  base::RepeatingCallbackList<void(TrackedElement*)> shown_callbacks_;
  base::RepeatingCallbackList<void(TrackedElement*)> activated_callbacks_;
  base::RepeatingCallbackList<void(TrackedElement*)> hidden_callbacks_;
};

// Ensures that ElementData objects get cleaned up, but only after all callbacks
// have returned. Otherwise a subscription could be canceled during a callback,
// resulting in the ElementData and the callback list being deleted before the
// callback has returned.
class ElementTracker::GarbageCollector {
 public:
  // Represents a call stack frame in which garbage collection can happen.
  // Garbage collection doesn't actually occur until all nested Frames are
  // destructed.
  class Frame {
   public:
    explicit Frame(GarbageCollector* gc) : gc_(gc) {
      gc_->IncrementFrameCount();
    }

    ~Frame() { gc_->DecrementFrameCount(); }

    void Add(ElementData* data) { gc_->AddCandidate(data); }

   private:
    GarbageCollector* const gc_;
  };

  explicit GarbageCollector(ElementTracker* tracker) : tracker_(tracker) {}

 private:
  void AddCandidate(ElementData* data) {
    DCHECK_GE(frame_count_, 0);
    candidates_.insert(data);
  }

  void IncrementFrameCount() { ++frame_count_; }

  void DecrementFrameCount() {
    DCHECK_GE(frame_count_, 0);
    if (--frame_count_ > 0)
      return;

    for (ElementData* data : candidates_) {
      if (data->empty()) {
        const auto result = tracker_->element_data_.erase(
            LookupKey(data->identifier(), data->context()));
        DCHECK(result);
      }
    }
    candidates_.clear();
  }

  ElementTracker* const tracker_;
  std::set<ElementData*> candidates_;
  int frame_count_ = 0;
};

TrackedElement::TrackedElement(ElementIdentifier id, ElementContext context)
    : identifier_(id), context_(context) {}

TrackedElement::~TrackedElement() = default;

// static
ElementTracker* ElementTracker::GetElementTracker() {
  static base::NoDestructor<ElementTracker> instance;
  return instance.get();
}

// static
ElementTrackerFrameworkDelegate* ElementTracker::GetFrameworkDelegate() {
  return static_cast<ElementTrackerFrameworkDelegate*>(GetElementTracker());
}

TrackedElement* ElementTracker::GetUniqueElement(ElementIdentifier id,
                                                 ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  if (it == element_data_.end() || it->second->num_elements() == 0)
    return nullptr;
  DCHECK_EQ(1U, it->second->num_elements());
  return it->second->elements().front();
}

TrackedElement* ElementTracker::GetFirstMatchingElement(
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
  DCHECK(id);
  DCHECK(context);
  return GetOrAddElementData(id, context)->AddElementShownCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddElementActivatedCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  DCHECK(id);
  DCHECK(context);
  return GetOrAddElementData(id, context)
      ->AddElementActivatedCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddElementHiddenCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  DCHECK(id);
  DCHECK(context);
  return GetOrAddElementData(id, context)->AddElementHiddenCallback(callback);
}

ElementTracker::ElementTracker()
    : gc_(std::make_unique<GarbageCollector>(this)) {}

ElementTracker::~ElementTracker() {
  NOTREACHED();
}

void ElementTracker::NotifyElementShown(TrackedElement* element) {
  GarbageCollector::Frame gc_frame(gc_.get());
  DCHECK(!base::Contains(element_to_data_lookup_, element));
  ElementData* const element_data =
      GetOrAddElementData(element->identifier(), element->context());
  element_to_data_lookup_.emplace(element, element_data);
  element_data->NotifyElementShown(element);
}

void ElementTracker::NotifyElementActivated(TrackedElement* element) {
  GarbageCollector::Frame gc_frame(gc_.get());
  const auto it = element_to_data_lookup_.find(element);
  DCHECK(it != element_to_data_lookup_.end());
  it->second->NotifyElementActivated(element);
}

void ElementTracker::NotifyElementHidden(TrackedElement* element) {
  GarbageCollector::Frame gc_frame(gc_.get());
  const auto it = element_to_data_lookup_.find(element);
  DCHECK(it != element_to_data_lookup_.end());
  ElementData* const data = it->second;
  element_to_data_lookup_.erase(it);
  data->NotifyElementHidden(element);
  gc_frame.Add(data);
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
  GarbageCollector::Frame gc_frame(gc_.get());
  gc_frame.Add(data);
}

SafeElementReference::SafeElementReference() = default;

SafeElementReference::SafeElementReference(TrackedElement* element)
    : element_(element) {
  Subscribe();
}

SafeElementReference::SafeElementReference(SafeElementReference&& other)
    : element_(other.element_) {
  // Have to rebind instead of moving the subscription since the other
  // reference's this pointer is bound.
  Subscribe();
  other.subscription_ = ElementTracker::Subscription();
  other.element_ = nullptr;
}

SafeElementReference::SafeElementReference(const SafeElementReference& other)
    : element_(other.element_) {
  Subscribe();
}

SafeElementReference& SafeElementReference::operator=(
    SafeElementReference&& other) {
  if (&other != this) {
    element_ = other.element_;
    // Have to rebind instead of moving the subscription since the other
    // reference's this pointer is bound.
    Subscribe();
    other.subscription_ = ElementTracker::Subscription();
    other.element_ = nullptr;
  }
  return *this;
}

SafeElementReference& SafeElementReference::operator=(
    const SafeElementReference& other) {
  if (&other != this) {
    element_ = other.element_;
    Subscribe();
  }
  return *this;
}

SafeElementReference::~SafeElementReference() = default;

void SafeElementReference::Subscribe() {
  if (!element_) {
    if (subscription_)
      subscription_ = ElementTracker::Subscription();
    return;
  }

  subscription_ = ElementTracker::GetElementTracker()->AddElementHiddenCallback(
      element_->identifier(), element_->context(),
      base::BindRepeating(&SafeElementReference::OnElementHidden,
                          base::Unretained(this)));
}

void SafeElementReference::OnElementHidden(TrackedElement* element) {
  if (element != element_)
    return;

  subscription_ = ElementTracker::Subscription();
  element_ = nullptr;
}

}  // namespace ui
