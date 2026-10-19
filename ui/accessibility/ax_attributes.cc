// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_attributes.h"

#include "base/strings/string_util.h"

namespace ui {

template <>
const std::string& ObjectAttributeTraits<std::string>::GetDefault() {
  return base::EmptyString();
}

}  // namespace ui
