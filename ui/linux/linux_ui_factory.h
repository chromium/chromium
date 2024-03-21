// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_FACTORY_H_
#define UI_LINUX_LINUX_UI_FACTORY_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace ui {

class LinuxUi;
class LinuxUiTheme;
enum class SystemTheme : int;

// Returns a LinuxUi for the default toolkit.  May create a LinuxUi instance if
// one does not exist.  May return nullptr if no toolkits are available.
COMPONENT_EXPORT(LINUX_UI_FACTORY)
LinuxUi* GetDefaultLinuxUi();

// Returns a LinuxUiTheme for the default toolkit.  May create a LinuxUiTheme
// instance if one does not exist.  May return nullptr if no toolkits are
// available.  Should only be used by tests or LinuxUi internals. Otherwise, use
// the accessors in LinuxUiTheme instead.
COMPONENT_EXPORT(LINUX_UI_FACTORY)
LinuxUiTheme* GetDefaultLinuxUiTheme();

COMPONENT_EXPORT(LINUX_UI_FACTORY)
LinuxUiTheme* GetLinuxUiTheme(SystemTheme system_theme);

// Returns all `LinuxUiTheme`s that have been created.
COMPONENT_EXPORT(LINUX_UI_FACTORY)
const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>&
GetLinuxUiThemes();

COMPONENT_EXPORT(LINUX_UI_FACTORY)
SystemTheme GetDefaultSystemTheme();

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_FACTORY_H_
