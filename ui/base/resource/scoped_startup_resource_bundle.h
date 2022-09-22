// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_SCOPED_STARTUP_RESOURCE_BUNDLE_H_
#define UI_BASE_RESOURCE_SCOPED_STARTUP_RESOURCE_BUNDLE_H_

#include "base/component_export.h"

namespace ui {

// Ensures there is a loaded shared ResourceBundle for accesses to localized
// strings. This class should be used to scope calls that access early startup
// messages through ResourceBundle::GetLocalizedString(...). Since the user
// locale is stored into Local State, it can be accessed during early
// startup phases. Therefore, locale from the command-line or the system locale
// is used instead.
// TODO(http://crbug.com/1365983): This class is scoping the early localized
// string accesses. A better way to handle locale and Chrome resource bundle on
// startup should be used.
class COMPONENT_EXPORT(UI_BASE) ScopedStartupResourceBundle final {
 public:
  ScopedStartupResourceBundle();
  ScopedStartupResourceBundle(const ScopedStartupResourceBundle&) = delete;
  ScopedStartupResourceBundle& operator=(const ScopedStartupResourceBundle&) =
      delete;
  ~ScopedStartupResourceBundle();

 private:
  bool cleanup_resource_bundle_ = false;
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_SCOPED_STARTUP_RESOURCE_BUNDLE_H_
