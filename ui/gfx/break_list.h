// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BREAK_LIST_H_
#define UI_GFX_BREAK_LIST_H_

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "ui/gfx/range/range.h"

namespace gfx {

// BreakLists manage ordered, non-overlapping, and non-repeating ranged values.
// These may be used to apply ranged colors and styles to text, for an example.
//
// Each break stores the start position and value of its associated range.
// A solitary break at position 0 applies to the entire space [0, max_).
// |max_| is initially 0 and should be set to match the available ranged space.
// The first break always has position 0, to ensure all positions have a value.
// The value of the terminal break applies to the range [break.first, max_).
// The value of other breaks apply to the range [break.first, (break+1).first).
template <typename T>
class BreakList {
 public:
  // The break type and const iterator, typedef'ed for convenience.
  typedef std::pair<size_t, T> Break;
  typedef typename std::vector<Break>::const_iterator const_iterator;

  // Initialize a break at position 0 with the default or supplied |value|.
  BreakList();
  explicit BreakList(T value);

  const std::vector<Break>& breaks() const { return breaks_; }

  // Clear the breaks and set a break at position 0 with the supplied |value|.
  void SetValue(T value);

  // Adjust the breaks to apply |value| over the supplied |range|.
  void ApplyValue(T value, const Range& range);

  // Set the max position and trim any breaks at or beyond that position.
  void SetMax(size_t max);
  size_t max() const { return max_; }

  // Get the break applicable to |position| (at or preceeding |position|).
  typename std::vector<Break>::iterator GetBreak(size_t position);
  typename std::vector<Break>::const_iterator GetBreak(size_t position) const;

  // Get the range of the supplied break; returns the break's start position and
  // the next break's start position (or |max_| for the terminal break).
  Range GetRange(const typename BreakList<T>::const_iterator& i) const;

  // Comparison functions for testing purposes.
  bool EqualsValueForTesting(T value) const;
  bool EqualsForTesting(const std::vector<Break>& breaks) const;

 private:
#ifndef NDEBUG
  // Check for ordered breaks [0, |max_|) with no adjacent equivalent values.
  void CheckBreaks();
#endif

  std::vector<Break> breaks_;
  size_t max_;
};

template<class T>
BreakList<T>::BreakList() : breaks_(1, Break(0, T())), max_(0) {
}

template<class T>
BreakList<T>::BreakList(T value) : breaks_(1, Break(0, value)), max_(0) {
}

template<class T>
void BreakList<T>::SetValue(T value) {
  breaks_.clear();
  breaks_.push_back(Break(0, value));
}

template<class T>
void BreakList<T>::ApplyValue(T value, const Range& range) {
  if (!range.IsValid() || range.is_empty())
    return;
  DCHECK(!breaks_.empty());
  DCHECK(!range.is_reversed());
  DCHECK(Range(0, static_cast<uint32_t>(max_)).Contains(range));

  // Erase any breaks in |range|, then add start and end breaks as needed.
  typename std::vector<Break>::iterator start = GetBreak(range.start());
  start += start->first < range.start() ? 1 : 0;
  typename std::vector<Break>::iterator end = GetBreak(range.end());
  T trailing_value = end->second;
  typename std::vector<Break>::iterator i =
      start == breaks_.end() ? start : breaks_.erase(start, end + 1);
  if (range.start() == 0 || (i - 1)->second != value)
    i = breaks_.insert(i, Break(range.start(), value)) + 1;
  if (trailing_value != value && range.end() != max_)
    breaks_.insert(i, Break(range.end(), trailing_value));

#ifndef NDEBUG
  CheckBreaks();
#endif
}

template<class T>
void BreakList<T>::SetMax(size_t max) {
  typename std::vector<Break>::iterator i = GetBreak(max);
  i += (i == breaks_.begin() || i->first < max) ? 1 : 0;
  breaks_.erase(i, breaks_.end());
  max_ = max;

#ifndef NDEBUG
  CheckBreaks();
#endif
}

template<class T>
typename std::vector<std::pair<size_t, T> >::iterator BreakList<T>::GetBreak(
    size_t position) {
  typename std::vector<Break>::iterator i = breaks_.end() - 1;
  for (; i != breaks_.begin() && i->first > position; --i);
  return i;
}

template<class T>
typename std::vector<std::pair<size_t, T> >::const_iterator
    BreakList<T>::GetBreak(size_t position) const {
  typename std::vector<Break>::const_iterator i = breaks_.end() - 1;
  for (; i != breaks_.begin() && i->first > position; --i);
  return i;
}

template<class T>
Range BreakList<T>::GetRange(
    const typename BreakList<T>::const_iterator& i) const {
  const typename BreakList<T>::const_iterator next = i + 1;
  return Range(i->first, next == breaks_.end() ? max_ : next->first);
}

template<class T>
bool BreakList<T>::EqualsValueForTesting(T value) const {
  return breaks_.size() == 1 && breaks_[0] == Break(0, value);
}

template<class T>
bool BreakList<T>::EqualsForTesting(const std::vector<Break>& breaks) const {
  if (breaks_.size() != breaks.size())
    return false;
  for (size_t i = 0; i < breaks.size(); ++i)
    if (breaks_[i] != breaks[i])
      return false;
  return true;
}

#ifndef NDEBUG
template <class T>
void BreakList<T>::CheckBreaks() {
  DCHECK_EQ(breaks_[0].first, 0U) << "The first break must be at position 0.";
  for (size_t i = 0; i < breaks_.size() - 1; ++i) {
    DCHECK_LT(breaks_[i].first, breaks_[i + 1].first) << "Break out of order.";
    DCHECK_NE(breaks_[i].second, breaks_[i + 1].second) << "Redundant break.";
  }
  if (max_ > 0)
    DCHECK_LT(breaks_.back().first, max_) << "Break beyond max position.";
}
#endif

}  // namespace gfx

#endif  // UI_GFX_BREAK_LIST_H_
