// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_SETTINGS_PROVIDER_H_
#define UI_GTK_SETTINGS_PROVIDER_H_

namespace gtk {

// This class is just a switch between SettingsProviderGSettings and
// SettingsProviderGtk.  Currently, it is empty and it's only purpose is so
// that GtkUi can store just a std::unique_ptr<SettingsProvider> and not have to
// have the two impls each guarded by their own macros.
class SettingsProvider {
 public:
  virtual ~SettingsProvider() {}

 protected:
  // Even though this class is not pure virtual, it should not be instantiated
  // directly.  Use SettingsProviderGSettings or SettingsProviderGtk instead.
  SettingsProvider() {}
};

}  // namespace gtk

#endif  // UI_GTK_SETTINGS_PROVIDER_H_
