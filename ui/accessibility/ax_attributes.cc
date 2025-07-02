// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_attributes.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace ui {

template <>
const std::string& ObjectAttributeTraits<std::string>::GetDefault() {
  return base::EmptyString();
}

template <>
const std::vector<int32_t>&
ObjectAttributeTraits<std::vector<int32_t>>::GetDefault() {
  static const base::NoDestructor<std::vector<int32_t>> empty_vector;
  return *empty_vector;
}

template <>
const std::vector<std::string>&
ObjectAttributeTraits<std::vector<std::string>>::GetDefault() {
  static const base::NoDestructor<std::vector<std::string>> empty_vector;
  return *empty_vector;
}

}  // namespace ui
