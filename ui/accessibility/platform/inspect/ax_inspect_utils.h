// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_H_

#include <string>

#include "ui/accessibility/ax_export.h"

namespace base {
class Value;
}  // namespace base

namespace ui {

/**
 * Constructs a const formattable value. Refers to FormatValue.
 */
std::string AX_EXPORT AXMakeConst(const std::string& value);

/**
 * Constructs a key for a formattable set represented by dictionary. It adds
 * the _setkey_ prefix to a string key.  Refers to FormatValue.
 */
std::string AX_EXPORT AXMakeSetKey(const std::string& key_name);

/**
 * Constructs an ordered key for a formattable dictionary by appending position
 * number to a string key for sorting. It makes the keys to be traversed
 * according to their position when the dictionary is formatted. Refers to
 * FormatValue.
 */
std::string AX_EXPORT AXMakeOrderedKey(const std::string& key_name,
                                       int position);

/**
 * Formats a value.
 */
std::string AX_EXPORT AXFormatValue(const base::Value& value);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_H_
