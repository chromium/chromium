// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_TARGET_ITERATOR_H_
#define UI_EVENTS_EVENT_TARGET_ITERATOR_H_

#include <memory>
#include <vector>

namespace ui {

class EventTarget;

// An interface that allows iterating over a set of EventTargets.
class EventTargetIterator {
 public:
  virtual ~EventTargetIterator() {}
  virtual EventTarget* GetNextTarget() = 0;
};

namespace internal {

// Provides EventTargetIterator implementations for iterating over a list of
// EventTargets. The list is iterated in the reverse order, since typically the
// EventTargets are maintained in increasing z-order in the lists.
template <typename T>
class EventTargetIteratorPtrImpl : public EventTargetIterator {
 public:
  explicit EventTargetIteratorPtrImpl(const std::vector<T>& children)
      : begin_(children.rbegin()), end_(children.rend()) {}
  EventTargetIteratorPtrImpl(EventTargetIteratorPtrImpl&&) = default;
  ~EventTargetIteratorPtrImpl() override = default;

  EventTarget* GetNextTarget() override {
    if (begin_ == end_) {
      return nullptr;
    }
    // This works with all pointer-like objects, including raw_ptr, unique_ptr,
    // etc.
    return &**begin_++;
  }

 private:
  typename std::vector<T>::const_reverse_iterator begin_;
  typename std::vector<T>::const_reverse_iterator end_;
};

}  // namespace internal

// Convenience method for wrapping `targets` in an event target iterator.
// Targets should be a vector of some pointer-like object; this can be actual
// pointers, `raw_ptr`, `unique_ptr`, etc.
template <typename T>
std::unique_ptr<EventTargetIterator> MakeEventTargetIterator(
    const std::vector<T>& targets) {
  return std::make_unique<internal::EventTargetIteratorPtrImpl<T>>(targets);
}

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGET_ITERATOR_H_
