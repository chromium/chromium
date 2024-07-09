// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_tracker.h"

#include <iterator>
#include <list>
#include <map>
#include <sstream>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ElementTracker, kTemporaryIdentifier);

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
    custom_event_callbacks_.set_removal_callback(removal_callback);
  }
  ~ElementData() = default;

  ElementIdentifier identifier() const { return identifier_; }
  ElementContext context() const { return context_; }

  bool HasElement(const TrackedElement* element) const {
    return base::Contains(element_lookup_, element);
  }

  bool empty() const {
    return elements_.empty() && shown_callbacks_.empty() &&
           activated_callbacks_.empty() && hidden_callbacks_.empty() &&
           custom_event_callbacks_.empty();
  }

  size_t num_elements() const {
    // Guaranteed O(1) in C++11 and later.
    return elements_.size();
  }

  const std::list<raw_ptr<TrackedElement, CtnExperimental>>& elements() const {
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

  Subscription AddCustomEventCallback(Callback callback) {
    return custom_event_callbacks_.Add(callback);
  }

  void NotifyElementShown(raw_ptr<TrackedElement, CtnExperimental>& element) {
    DCHECK(element);
    DCHECK_EQ(identifier(), element->identifier());
    // Zero context data is the "all contexts" entry and doesn't actually store
    // new elements, just calls callbacks.
    if (context()) {
      DCHECK_EQ(static_cast<intptr_t>(context()),
                static_cast<intptr_t>(element->context()));
      const auto it = elements_.insert(elements_.end(), element);
      const bool success = element_lookup_.emplace(element, it).second;
      DCHECK(success);
    }
    shown_callbacks_.Notify(element);
  }

  void NotifyElementActivated(
      raw_ptr<TrackedElement, CtnExperimental>& element) {
    // Note: "All contexts" does not require the element to be present here.
    DCHECK(!context_ || base::Contains(element_lookup_, element));
    activated_callbacks_.Notify(element);
  }

  void NotifyElementHidden(TrackedElement* element) {
    if (context_) {
      const auto it = element_lookup_.find(element);
      CHECK(it != element_lookup_.end(), base::NotFatalUntil::M130);
      elements_.erase(it->second);
      element_lookup_.erase(it);
    }
    hidden_callbacks_.Notify(element);
  }

  void NotifyCustomEvent(TrackedElement* element) {
    custom_event_callbacks_.Notify(element);
  }

 private:
  const ElementIdentifier identifier_;
  const ElementContext context_;

  // Holds elements in the order they were added to this data block, so that the
  // first element or the first element that matches some criterion can be
  // easily found.
  std::list<raw_ptr<TrackedElement, CtnExperimental>> elements_;

  // Provides a fast lookup into `elements_` by element for checking and
  // removal. Since there could be many elements (e.g. tabs in a browser) we
  // don't want removing a series of them to turn into an O(n^2) operation.
  std::map<const TrackedElement*,
           std::list<raw_ptr<TrackedElement, CtnExperimental>>::iterator>
      element_lookup_;

  base::RepeatingCallbackList<void(TrackedElement*)> shown_callbacks_;
  base::RepeatingCallbackList<void(TrackedElement*)> activated_callbacks_;
  base::RepeatingCallbackList<void(TrackedElement*)> hidden_callbacks_;
  base::RepeatingCallbackList<void(TrackedElement*)> custom_event_callbacks_;
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
    const raw_ptr<GarbageCollector> gc_;
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

  const raw_ptr<ElementTracker> tracker_;
  std::set<raw_ptr<ElementData, SetExperimental>> candidates_;
  int frame_count_ = 0;
};

TrackedElement::TrackedElement(ElementIdentifier id, ElementContext context)
    : identifier_(id), context_(context) {}

TrackedElement::~TrackedElement() = default;

gfx::Rect TrackedElement::GetScreenBounds() const {
  return gfx::Rect();
}

std::string TrackedElement::ToString() const {
  std::ostringstream oss;
  oss << GetImplementationName() << "(" << identifier() << ", " << context()
      << ")";
  return oss.str();
}

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
  if (it == element_data_.end() || it->second.num_elements() == 0)
    return nullptr;
  DCHECK_EQ(1U, it->second.num_elements());
  return it->second.elements().front();
}

TrackedElement* ElementTracker::GetFirstMatchingElement(
    ElementIdentifier id,
    ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  if (it == element_data_.end() || it->second.num_elements() == 0)
    return nullptr;
  return it->second.elements().front();
}

TrackedElement* ElementTracker::GetElementInAnyContext(ElementIdentifier id) {
  for (const auto& [key, data] : element_data_) {
    if (key.first == id && !data.elements().empty())
      return data.elements().front();
  }
  return nullptr;
}

ElementTracker::ElementList ElementTracker::GetAllMatchingElements(
    ElementIdentifier id,
    ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  ElementList result;
  if (it != element_data_.end()) {
    base::ranges::copy(it->second.elements(), std::back_inserter(result));
  }
  return result;
}

ElementTracker::ElementList ElementTracker::GetAllMatchingElementsInAnyContext(
    ElementIdentifier id) {
  ElementList result;
  for (const auto& [key, data] : element_data_) {
    if (key.first == id) {
      base::ranges::copy(data.elements(), std::back_inserter(result));
    }
  }
  return result;
}

bool ElementTracker::IsElementVisible(ElementIdentifier id,
                                      ElementContext context) {
  const auto it = element_data_.find(LookupKey(id, context));
  return it != element_data_.end() && it->second.num_elements() > 0;
}

ElementTracker::Contexts ElementTracker::GetAllContextsForTesting() const {
  Contexts result;
  for (const auto& [key, data] : element_data_) {
    result.insert(key.second);
  }
  return result;
}

ElementTracker::ElementList ElementTracker::GetAllElementsForTesting(
    std::optional<ElementContext> in_context) {
  ElementList result;
  for (const auto& [key, data] : element_data_) {
    if (!in_context.has_value() || in_context.value() == key.second) {
      std::copy(data.elements().begin(), data.elements().end(),
                std::back_inserter(result));
    }
  }
  return result;
}

ElementTracker::Subscription
ElementTracker::AddAnyElementShownCallbackForTesting(Callback callback) {
  return any_element_shown_callbacks_.Add(std::move(callback));
}

ElementTracker::Subscription ElementTracker::AddElementShownCallback(
    ElementIdentifier id,
    ElementContext context,
    Callback callback) {
  DCHECK(id);
  DCHECK(context);
  return GetOrAddElementData(id, context)->AddElementShownCallback(callback);
}

ElementTracker::Subscription
ElementTracker::AddElementShownInAnyContextCallback(ElementIdentifier id,
                                                    Callback callback) {
  DCHECK(id);
  return GetOrAddElementData(id, ElementContext())
      ->AddElementShownCallback(callback);
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

ElementTracker::Subscription
ElementTracker::AddElementActivatedInAnyContextCallback(ElementIdentifier id,
                                                        Callback callback) {
  DCHECK(id);
  return GetOrAddElementData(id, ElementContext())
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

ElementTracker::Subscription
ElementTracker::AddElementHiddenInAnyContextCallback(ElementIdentifier id,
                                                     Callback callback) {
  DCHECK(id);
  return GetOrAddElementData(id, ElementContext())
      ->AddElementHiddenCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddCustomEventCallback(
    CustomElementEventType event_type,
    ElementContext context,
    Callback callback) {
  DCHECK(event_type);
  DCHECK(context);
  // Because custom event callbacks are indexed by event type (and because we
  // use the same underlying type for both element ids and custom events), we
  // can store both in the same lookup table.
  return GetOrAddElementData(event_type, context)
      ->AddCustomEventCallback(callback);
}

ElementTracker::Subscription ElementTracker::AddCustomEventInAnyContextCallback(
    CustomElementEventType event_type,
    Callback callback) {
  DCHECK(event_type);
  // Because custom event callbacks are indexed by event type (and because we
  // use the same underlying type for both element ids and custom events), we
  // can store both in the same lookup table.
  return GetOrAddElementData(event_type, ElementContext())
      ->AddCustomEventCallback(callback);
}

ElementTracker::ElementTracker()
    : gc_(std::make_unique<GarbageCollector>(this)) {}

ElementTracker::~ElementTracker() = default;

void ElementTracker::NotifyElementShown(TrackedElement* element) {
  notification_elements_.push_back(element);
  auto& safe_element = notification_elements_.back();

  // Prevent garbage collection of dead entries until after we send
  // notifications and all callbacks happen.
  GarbageCollector::Frame gc_frame(gc_.get());
  ElementData* const element_data =
      GetOrAddElementData(element->identifier(), element->context());
  DCHECK(!element_data->HasElement(element));
  element_data->NotifyElementShown(safe_element);

  // Do "all contexts" notification:
  if (safe_element) {
    const auto it =
        element_data_.find(LookupKey(element->identifier(), ElementContext()));
    if (it != element_data_.end())
      it->second.NotifyElementShown(safe_element);
  }

  // Do the "all elements" notification:
  if (safe_element)
    any_element_shown_callbacks_.Notify(element);

  notification_elements_.pop_back();
}

void ElementTracker::NotifyElementActivated(TrackedElement* element) {
  notification_elements_.push_back(element);
  auto& safe_element = notification_elements_.back();

  // Prevent garbage collection of dead entries until after we send
  // notifications and all callbacks happen.
  GarbageCollector::Frame gc_frame(gc_.get());
  const auto it =
      element_data_.find(LookupKey(element->identifier(), element->context()));
  CHECK(it != element_data_.end(), base::NotFatalUntil::M130);
  it->second.NotifyElementActivated(safe_element);

  // Do "all contexts" notification:
  if (safe_element) {
    const auto all_it =
        element_data_.find(LookupKey(element->identifier(), ElementContext()));
    if (all_it != element_data_.end()) {
      all_it->second.NotifyElementActivated(safe_element);
    }
  }

  notification_elements_.pop_back();
}

void ElementTracker::NotifyElementHidden(TrackedElement* element) {
  // Clear out any elements we're in the process of sending events for.
  for (auto& safe_element : notification_elements_) {
    if (safe_element == element)
      safe_element = nullptr;
  }

  // Prevent garbage collection of dead entries until after we send
  // notifications and all callbacks happen.
  GarbageCollector::Frame gc_frame(gc_.get());

  // Call context-specific callbacks and erase entry.
  const auto it =
      element_data_.find(LookupKey(element->identifier(), element->context()));
  CHECK(it != element_data_.end(), base::NotFatalUntil::M130);
  ElementData* const data = &it->second;
  data->NotifyElementHidden(element);
  gc_frame.Add(data);

  // Call "in any context" callbacks.
  const auto all_it =
      element_data_.find(LookupKey(element->identifier(), ElementContext()));
  if (all_it != element_data_.end()) {
    all_it->second.NotifyElementHidden(element);
  }
}

void ElementTracker::NotifyCustomEvent(TrackedElement* element,
                                       CustomElementEventType event_type) {
  // Prevent garbage collection of dead entries until after we send
  // notifications and all callbacks happen.
  GarbageCollector::Frame gc_frame(gc_.get());

  // We'd like to verify that this element is valid, but don't need to expend
  // the effort on an extra lookup if we're not doing checks.
#if DCHECK_IS_ON()
  const auto entry =
      element_data_.find(LookupKey(element->identifier(), element->context()));
  DCHECK(entry != element_data_.end() && entry->second.HasElement(element));
#endif

  notification_elements_.push_back(element);
  auto& safe_element = notification_elements_.back();

  // Since event types are identifiers, we store callbacks by event type rather
  // than element identifier.
  const auto it = element_data_.find(LookupKey(event_type, element->context()));
  // If we don't find a match, that's fine; it means nobody was listening for
  // that event type.
  if (it != element_data_.end()) {
    it->second.NotifyCustomEvent(safe_element);
  }

  // Do "all contexts" notification:
  const auto all_it =
      element_data_.find(LookupKey(event_type, ElementContext()));
  if (all_it != element_data_.end()) {
    all_it->second.NotifyCustomEvent(safe_element);
  }

  notification_elements_.pop_back();
}

ElementTracker::ElementData* ElementTracker::GetOrAddElementData(
    ElementIdentifier id,
    ElementContext context) {
  const LookupKey key(id, context);
  const auto [it, added] = element_data_.try_emplace(key, this, id, context);
  // This might be the first time we've referenced this identifier, so make
  // sure it's registered.
  if (added)
    ElementIdentifier::RegisterKnownIdentifier(id);
  return &it->second;
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
