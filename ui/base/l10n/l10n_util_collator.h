// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_COLLATOR_H_
#define UI_BASE_L10N_L10N_UTIL_COLLATOR_H_

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace l10n_util {

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using a collator.
template <class T, class Method>
class StringMethodComparatorWithCollator {
 public:
  StringMethodComparatorWithCollator(icu::Collator* collator, Method method)
      : collator_(collator),
        method_(method) { }

  // Returns true if lhs precedes rhs.
  bool operator()(const std::unique_ptr<T>& lhs_t,
                  const std::unique_ptr<T>& rhs_t) {
    return base::i18n::CompareString16WithCollator(
               *collator_, (lhs_t.get()->*method_)(),
               (rhs_t.get()->*method_)()) == UCOL_LESS;
  }

 private:
  raw_ptr<icu::Collator> collator_;
  Method method_;
};

// Used by SortStringsUsingMethod. Invokes a method on the objects passed to
// operator (), comparing the string results using <.
template <class T, class Method>
class StringMethodComparator {
 public:
  explicit StringMethodComparator(Method method) : method_(method) { }

  // Returns true if lhs precedes rhs.
  bool operator()(const std::unique_ptr<T>& lhs_t,
                  const std::unique_ptr<T>& rhs_t) {
    return (lhs_t.get()->*method_)() < (rhs_t.get()->*method_)();
  }

 private:
  Method method_;
};

// Sorts the objects in |elements| using the method |method|, which must return
// a string. Sorting is done using a collator, unless a collator can not be
// found in which case the strings are sorted using the operator <.
template <class T, class Method>
void SortStringsUsingMethod(const std::string& locale,
                            std::vector<std::unique_ptr<T>>* elements,
                            Method method) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(loc, error));
  if (U_FAILURE(error)) {
    sort(elements->begin(), elements->end(),
         StringMethodComparator<T, Method>(method));
    return;
  }

  std::sort(elements->begin(), elements->end(),
      StringMethodComparatorWithCollator<T, Method>(collator.get(), method));
}

// Compares two elements' string keys and returns true if the first element's
// string key is less than the second element's string key. The Element must
// have a method like the follow format to return the string key.
// const std::u16string& GetStringKey() const;
// This uses the locale specified in the constructor.
template <class Element>
class StringComparator {
 public:
  explicit StringComparator(icu::Collator* collator)
      : collator_(collator) { }

  // Returns true if lhs precedes rhs.
  bool operator()(const Element& lhs, const Element& rhs) const {
    const std::u16string& lhs_string_key = lhs.GetStringKey();
    const std::u16string& rhs_string_key = rhs.GetStringKey();

    return StringComparator<std::u16string>(collator_)(lhs_string_key,
                                                       rhs_string_key);
  }

 private:
  raw_ptr<icu::Collator> collator_;
};

// Specialization of operator() method for std::u16string version.
template <>
COMPONENT_EXPORT(UI_BASE)
inline bool StringComparator<std::u16string>::operator()(
    const std::u16string& lhs,
    const std::u16string& rhs) const {
  // If we can not get collator instance for specified locale, just do simple
  // string compare.
  if (!collator_)
    return lhs < rhs;
  return base::i18n::CompareString16WithCollator(*collator_, lhs, rhs) ==
         UCOL_LESS;
}

// In place sorting of |elements| of a vector according to the string key of
// each element in the vector by using collation rules for |locale|.
// |begin_index| points to the start position of elements in the vector which
// want to be sorted. |end_index| points to the end position of elements in the
// vector which want to be sorted.
template <class Element>
void SortVectorWithStringKey(const std::string& locale,
                             std::vector<Element>* elements,
                             size_t begin_index,
                             size_t end_index,
                             bool needs_stable_sort) {
  DCHECK_LT(begin_index, end_index);
  DCHECK_LE(end_index, elements->size());
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale loc(locale.c_str());
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(loc, error));
  if (U_FAILURE(error))
    collator.reset();
  StringComparator<Element> c(collator.get());
  const auto begin = elements->begin() + static_cast<ptrdiff_t>(begin_index);
  const auto end = elements->begin() + static_cast<ptrdiff_t>(end_index);
  if (needs_stable_sort) {
    stable_sort(begin, end, c);
  } else {
    sort(begin, end, c);
  }
}

template <class Element>
void SortVectorWithStringKey(const std::string& locale,
                             std::vector<Element>* elements,
                             bool needs_stable_sort) {
  SortVectorWithStringKey<Element>(locale, elements, 0, elements->size(),
                                   needs_stable_sort);
}

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_COLLATOR_H_
