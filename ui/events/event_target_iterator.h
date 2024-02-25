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

// Provides EventTargetIterator implementations for iterating over a list of
// EventTargets. The list is iterated in the reverse order, since typically the
// EventTargets are maintained in increasing z-order in the lists.
template <typename T>
class EventTargetIteratorPtrImpl : public EventTargetIterator {
 public:
  explicit EventTargetIteratorPtrImpl(
      const std::vector<raw_ptr<T, VectorExperimental>>& children)
      : begin_(children.rbegin()), end_(children.rend()) {}
  ~EventTargetIteratorPtrImpl() override {}

  EventTarget* GetNextTarget() override {
    if (begin_ == end_)
      return nullptr;
    EventTarget* target = *(begin_);
    ++begin_;
    return target;
  }

 private:
  typename std::vector<raw_ptr<T, VectorExperimental>>::const_reverse_iterator
      begin_;
  typename std::vector<raw_ptr<T, VectorExperimental>>::const_reverse_iterator
      end_;
};

template <typename T>
class EventTargetIteratorUniquePtrImpl : public EventTargetIterator {
 public:
  explicit EventTargetIteratorUniquePtrImpl(
      const std::vector<std::unique_ptr<T>>& children)
      : begin_(children.rbegin()), end_(children.rend()) {}
  ~EventTargetIteratorUniquePtrImpl() override {}

  EventTarget* GetNextTarget() override {
    if (begin_ == end_)
      return nullptr;
    EventTarget* target = begin_->get();
    ++begin_;
    return target;
  }

 private:
  typename std::vector<std::unique_ptr<T>>::const_reverse_iterator begin_;
  typename std::vector<std::unique_ptr<T>>::const_reverse_iterator end_;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGET_ITERATOR_H_
