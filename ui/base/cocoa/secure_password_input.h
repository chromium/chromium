// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_SECURE_PASSWORD_INPUT_H_
#define UI_BASE_COCOA_SECURE_PASSWORD_INPUT_H_

#include "base/component_export.h"

namespace ui {

// Enables the secure password input mode while in scope.
class COMPONENT_EXPORT(UI_BASE) ScopedPasswordInputEnabler {
 public:
  ScopedPasswordInputEnabler();

  ScopedPasswordInputEnabler(const ScopedPasswordInputEnabler&) = delete;
  ScopedPasswordInputEnabler& operator=(const ScopedPasswordInputEnabler&) =
      delete;

  ~ScopedPasswordInputEnabler();

  // Returns true if the password input mode is currently enabled. Useful for
  // unit tests.
  static bool IsPasswordInputEnabled();
};

}  // namespace ui

#endif  // UI_BASE_COCOA_SECURE_PASSWORD_INPUT_H_
