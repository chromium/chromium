// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/scoped_startup_resource_bundle.h"

#include <string>

#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

ScopedStartupResourceBundle::ScopedStartupResourceBundle() {
  // Ensure the ResourceBundle is initialized for string resource access.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    // Try to determine the best locale source to use.
    std::string locale;
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kLang)) {
      locale = command_line.GetSwitchValueASCII(::switches::kLang);
    } else {
      locale = l10n_util::GetApplicationLocale(std::string());
    }

    if (locale.empty())
      locale = "en-US";

    // Load a temporary resource bundle. The temporary instance must be unloaded
    // when exiting the scoper.
    cleanup_resource_bundle_ = true;
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        locale, nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }
}

ScopedStartupResourceBundle::~ScopedStartupResourceBundle() {
  if (cleanup_resource_bundle_)
    ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace ui
