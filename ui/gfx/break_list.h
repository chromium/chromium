// Copyright 2012 The Chromium Authors
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
  using Break = std::pair<size_t, T>;
  using const_iterator = typename std::vector<Break>::const_iterator;

  // Initialize a break at position 0 with the default or supplied |value|.
  BreakList();
  explicit BreakList(T value);

  const std::vector<Break>& breaks() const { return breaks_; }

  // Clear the breaks and set a break at position 0 with the supplied |value|.
  // Returns whether or not the breaks changed while applying the |value|.
  bool ClearAndSetInitialValue(T value);

  // Adjust the breaks to apply |value| over the supplied |range|.
  // Range |range| must be between [0, max_).
  // Returns true if the breaks changed while applying the |value|.
  bool ApplyValue(T value, const Range& range);

  // Set the max position and trim any breaks at or beyond that position.
  void SetMax(size_t max);
  size_t max() const { return max_; }

  // Get the break applicable to |position| (at or preceding |position|).
  // |position| must be between [0, max_).
  // Returns a valid iterator. Can't return |break_.end()|.
  const_iterator GetBreak(size_t position) const;

  // Get the range of the supplied break; returns the break's start position and
  // the next break's start position (or |max_| for the terminal break).
  // Iterator |i| must be valid and must not be |break_.end()|.
  Range GetRange(const const_iterator& i) const;

  // Clears the breaks, and sets a break at position 0 with the value of the
  // existing break at position 0. Only applicable on non-empty breaklists.
  void Reset() {
    DCHECK(breaks().size() > 0);
    ClearAndSetInitialValue(breaks().front().second);
  }

  // Comparison functions for testing purposes.
  bool EqualsValueForTesting(T value) const;
  bool EqualsForTesting(const std::vector<Break>& breaks) const;

 private:
#ifndef NDEBUG
  // Check for ordered breaks [0, |max_|) with no adjacent equivalent values.
  void CheckBreaks();
#endif

  std::vector<Break> breaks_;
  size_t max_ = 0;
};

template <class T>
BreakList<T>::BreakList() : breaks_(1, Break(0, T())) {}

template <class T>
BreakList<T>::BreakList(T value) : breaks_(1, Break(0, value)) {}

template <class T>
bool BreakList<T>::ClearAndSetInitialValue(T value) {
  // Return false if setting |value| does not change the breaks.
  if (breaks_.size() == 1 && breaks_[0].second == value)
    return false;

  breaks_.clear();
  breaks_.push_back(Break(0, value));
  return true;
}

template <class T>
bool BreakList<T>::ApplyValue(T value, const Range& range) {
  if (!range.IsValid() || range.is_empty())
    return false;
  DCHECK(!breaks_.empty());
  DCHECK(!range.is_reversed());
  DCHECK(Range(0, static_cast<uint32_t>(max_)).Contains(range));

  // Return false if setting |value| does not change the breaks.
  const_iterator start = GetBreak(range.start());
  if (start->second == value && GetRange(start).Contains(range))
    return false;

  // Erase any breaks in |range|, then add start and end breaks as needed.
  start += start->first < range.start() ? 1 : 0;
  const_iterator end =
      range.end() == max_ ? breaks_.cend() - 1 : GetBreak(range.end());
  T trailing_value = end->second;
  const_iterator i =
      start == breaks_.cend() ? start : breaks_.erase(start, end + 1);
  if (range.start() == 0 || (i - 1)->second != value)
    i = breaks_.insert(i, Break(range.start(), value)) + 1;
  if (trailing_value != value && range.end() != max_)
    breaks_.insert(i, Break(range.end(), trailing_value));

#ifndef NDEBUG
  CheckBreaks();
#endif

  return true;
}

template<class T>
void BreakList<T>::SetMax(size_t max) {
  if (max < max_) {
    const_iterator i = GetBreak(max);
    if (i == breaks_.begin() || i->first < max)
      i++;
    breaks_.erase(i, breaks_.end());
  }
  max_ = max;

#ifndef NDEBUG
  CheckBreaks();
#endif
}

template <class T>
typename BreakList<T>::const_iterator BreakList<T>::GetBreak(
    size_t position) const {
  DCHECK(!breaks_.empty());
  DCHECK_LT(position, max_);
  // Find the iterator with a 'strictly greater' position and return the
  // previous one.
  return std::upper_bound(breaks_.cbegin(), breaks_.cend(), position,
                          [](size_t offset, const Break& value) {
                            return offset < value.first;
                          }) -
         1;
}

template<class T>
Range BreakList<T>::GetRange(
    const typename BreakList<T>::const_iterator& i) const {
  // BreakLists are never empty. Iterator should always be valid.
  DCHECK(i != breaks_.end());
  const const_iterator next = i + 1;
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
  DCHECK(!breaks_.empty()) << "BreakList cannot be empty";
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
