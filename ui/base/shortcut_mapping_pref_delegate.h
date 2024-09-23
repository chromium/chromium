// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_SHORTCUT_MAPPING_PREF_DELEGATE_H_
#define UI_BASE_SHORTCUT_MAPPING_PREF_DELEGATE_H_

#include "base/component_export.h"

namespace ui {

// TODO(crbug.com/40203434): Remove this class once kDeviceI18nShortcutsEnabled
// policy is deprecated.
class COMPONENT_EXPORT(UI_BASE_FEATURES) ShortcutMappingPrefDelegate {
 public:
  static ShortcutMappingPrefDelegate* GetInstance();
  static bool IsInitialized();

  ShortcutMappingPrefDelegate();
  virtual ~ShortcutMappingPrefDelegate();

  virtual bool IsDeviceEnterpriseManaged() const = 0;

  virtual bool IsI18nShortcutPrefEnabled() const = 0;

 private:
  static ShortcutMappingPrefDelegate* instance_;
};

}  // namespace ui

#endif  // UI_BASE_SHORTCUT_MAPPING_PREF_DELEGATE_H_
