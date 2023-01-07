// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_GETTER_H_
#define UI_LINUX_LINUX_UI_GETTER_H_

#include "base/component_export.h"

class Profile;

namespace aura {
class Window;
}

namespace ui {

class LinuxUiTheme;

// Use the getters in LinuxUiTheme instead of using this class directly.
class COMPONENT_EXPORT(LINUX_UI) LinuxUiGetter {
 public:
  LinuxUiGetter& operator=(const LinuxUiGetter&) = delete;
  LinuxUiGetter(const LinuxUiGetter&) = delete;

  LinuxUiGetter();
  virtual ~LinuxUiGetter();

  virtual LinuxUiTheme* GetForWindow(aura::Window* window) = 0;
  virtual LinuxUiTheme* GetForProfile(Profile* profile) = 0;

  static LinuxUiGetter* instance() { return instance_; }
  static void set_instance(LinuxUiGetter* instance) { instance_ = instance; }

 private:
  static LinuxUiGetter* instance_;
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_GETTER_H_
