// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/resources/cr_components/app_management/app_management_mojom_traits.h"

#include <utility>

namespace mojo {

AppType EnumTraits<AppType, apps::AppType>::ToMojom(apps::AppType input) {
  switch (input) {
    case apps::AppType::kUnknown:
      return AppType::kUnknown;
    case apps::AppType::kArc:
      return AppType::kArc;
    case apps::AppType::kBuiltIn:
      return AppType::kBuiltIn;
    case apps::AppType::kCrostini:
      return AppType::kCrostini;
    case apps::AppType::kChromeApp:
      return AppType::kChromeApp;
    case apps::AppType::kWeb:
      return AppType::kWeb;
    case apps::AppType::kPluginVm:
      return AppType::kPluginVm;
    case apps::AppType::kStandaloneBrowser:
      return AppType::kStandaloneBrowser;
    case apps::AppType::kRemote:
      return AppType::kRemote;
    case apps::AppType::kBorealis:
      return AppType::kBorealis;
    case apps::AppType::kSystemWeb:
      return AppType::kSystemWeb;
    case apps::AppType::kStandaloneBrowserChromeApp:
      return AppType::kStandaloneBrowserChromeApp;
    case apps::AppType::kExtension:
      return AppType::kExtension;
    case apps::AppType::kStandaloneBrowserExtension:
      return AppType::kStandaloneBrowserExtension;
    case apps::AppType::kBruschetta:
      return AppType::kBruschetta;
  }
}

bool EnumTraits<AppType, apps::AppType>::FromMojom(AppType input,
                                                   apps::AppType* output) {
  switch (input) {
    case AppType::kUnknown:
      *output = apps::AppType::kUnknown;
      return true;
    case AppType::kArc:
      *output = apps::AppType::kArc;
      return true;
    case AppType::kBuiltIn:
      *output = apps::AppType::kBuiltIn;
      return true;
    case AppType::kCrostini:
      *output = apps::AppType::kCrostini;
      return true;
    case AppType::kChromeApp:
      *output = apps::AppType::kChromeApp;
      return true;
    case AppType::kWeb:
      *output = apps::AppType::kWeb;
      return true;
    case AppType::kPluginVm:
      *output = apps::AppType::kPluginVm;
      return true;
    case AppType::kStandaloneBrowser:
      *output = apps::AppType::kStandaloneBrowser;
      return true;
    case AppType::kRemote:
      *output = apps::AppType::kRemote;
      return true;
    case AppType::kBorealis:
      *output = apps::AppType::kBorealis;
      return true;
    case AppType::kSystemWeb:
      *output = apps::AppType::kSystemWeb;
      return true;
    case AppType::kStandaloneBrowserChromeApp:
      *output = apps::AppType::kStandaloneBrowserChromeApp;
      return true;
    case AppType::kExtension:
      *output = apps::AppType::kExtension;
      return true;
    case AppType::kStandaloneBrowserExtension:
      *output = apps::AppType::kStandaloneBrowserExtension;
      return true;
    case AppType::kBruschetta:
      *output = apps::AppType::kBruschetta;
      return true;
  }
}

bool StructTraits<PermissionDataView, apps::PermissionPtr>::Read(
    PermissionDataView data,
    apps::PermissionPtr* out) {
  apps::PermissionType permission_type;
  if (!data.ReadPermissionType(&permission_type))
    return false;

  apps::Permission::PermissionValue value;
  if (!data.ReadValue(&value))
    return false;

  std::optional<std::string> details;
  if (!data.ReadDetails(&details)) {
    return false;
  }

  *out = std::make_unique<apps::Permission>(permission_type, std::move(value),
                                            data.is_managed(), details);
  return true;
}

PermissionType EnumTraits<PermissionType, apps::PermissionType>::ToMojom(
    apps::PermissionType input) {
  switch (input) {
    case apps::PermissionType::kUnknown:
      return PermissionType::kUnknown;
    case apps::PermissionType::kCamera:
      return PermissionType::kCamera;
    case apps::PermissionType::kLocation:
      return PermissionType::kLocation;
    case apps::PermissionType::kMicrophone:
      return PermissionType::kMicrophone;
    case apps::PermissionType::kNotifications:
      return PermissionType::kNotifications;
    case apps::PermissionType::kContacts:
      return PermissionType::kContacts;
    case apps::PermissionType::kStorage:
      return PermissionType::kStorage;
    case apps::PermissionType::kPrinting:
      return PermissionType::kPrinting;
    case apps::PermissionType::kFileHandling:
      return PermissionType::kFileHandling;
  }
}

bool EnumTraits<PermissionType, apps::PermissionType>::FromMojom(
    PermissionType input,
    apps::PermissionType* output) {
  switch (input) {
    case PermissionType::kUnknown:
      *output = apps::PermissionType::kUnknown;
      return true;
    case PermissionType::kCamera:
      *output = apps::PermissionType::kCamera;
      return true;
    case PermissionType::kLocation:
      *output = apps::PermissionType::kLocation;
      return true;
    case PermissionType::kMicrophone:
      *output = apps::PermissionType::kMicrophone;
      return true;
    case PermissionType::kNotifications:
      *output = apps::PermissionType::kNotifications;
      return true;
    case PermissionType::kContacts:
      *output = apps::PermissionType::kContacts;
      return true;
    case PermissionType::kStorage:
      *output = apps::PermissionType::kStorage;
      return true;
    case PermissionType::kPrinting:
      *output = apps::PermissionType::kPrinting;
      return true;
    case PermissionType::kFileHandling:
      *output = apps::PermissionType::kFileHandling;
      return true;
  }
}

TriState EnumTraits<TriState, apps::TriState>::ToMojom(apps::TriState input) {
  switch (input) {
    case apps::TriState::kAllow:
      return TriState::kAllow;
    case apps::TriState::kBlock:
      return TriState::kBlock;
    case apps::TriState::kAsk:
      return TriState::kAsk;
  }
}

bool EnumTraits<TriState, apps::TriState>::FromMojom(TriState input,
                                                     apps::TriState* output) {
  switch (input) {
    case TriState::kAllow:
      *output = apps::TriState::kAllow;
      return true;
    case TriState::kBlock:
      *output = apps::TriState::kBlock;
      return true;
    case TriState::kAsk:
      *output = apps::TriState::kAsk;
      return true;
  }
}

PermissionValueDataView::Tag
UnionTraits<PermissionValueDataView, apps::Permission::PermissionValue>::GetTag(
    const apps::Permission::PermissionValue& r) {
  if (absl::holds_alternative<bool>(r)) {
    return PermissionValueDataView::Tag::kBoolValue;
  } else if (absl::holds_alternative<apps::TriState>(r)) {
    return PermissionValueDataView::Tag::kTristateValue;
  }
  NOTREACHED_IN_MIGRATION();
  return PermissionValueDataView::Tag::kBoolValue;
}

bool UnionTraits<PermissionValueDataView, apps::Permission::PermissionValue>::
    Read(PermissionValueDataView data, apps::Permission::PermissionValue* out) {
  switch (data.tag()) {
    case PermissionValueDataView::Tag::kBoolValue: {
      *out = data.bool_value();
      return true;
    }
    case PermissionValueDataView::Tag::kTristateValue: {
      apps::TriState tristate_value;
      if (!data.ReadTristateValue(&tristate_value))
        return false;
      *out = tristate_value;
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

InstallReason EnumTraits<InstallReason, apps::InstallReason>::ToMojom(
    apps::InstallReason input) {
  switch (input) {
    case apps::InstallReason::kUnknown:
      return InstallReason::kUnknown;
    case apps::InstallReason::kSystem:
      return InstallReason::kSystem;
    case apps::InstallReason::kPolicy:
      return InstallReason::kPolicy;
    case apps::InstallReason::kOem:
      return InstallReason::kOem;
    case apps::InstallReason::kDefault:
      return InstallReason::kDefault;
    case apps::InstallReason::kSync:
      return InstallReason::kSync;
    case apps::InstallReason::kUser:
      return InstallReason::kUser;
    case apps::InstallReason::kSubApp:
      return InstallReason::kSubApp;
    case apps::InstallReason::kKiosk:
      return InstallReason::kKiosk;
    case apps::InstallReason::kCommandLine:
      return InstallReason::kCommandLine;
  }
}

bool EnumTraits<InstallReason, apps::InstallReason>::FromMojom(
    InstallReason input,
    apps::InstallReason* output) {
  switch (input) {
    case InstallReason::kUnknown:
      *output = apps::InstallReason::kUnknown;
      return true;
    case InstallReason::kSystem:
      *output = apps::InstallReason::kSystem;
      return true;
    case InstallReason::kPolicy:
      *output = apps::InstallReason::kPolicy;
      return true;
    case InstallReason::kOem:
      *output = apps::InstallReason::kOem;
      return true;
    case InstallReason::kDefault:
      *output = apps::InstallReason::kDefault;
      return true;
    case InstallReason::kSync:
      *output = apps::InstallReason::kSync;
      return true;
    case InstallReason::kUser:
      *output = apps::InstallReason::kUser;
      return true;
    case InstallReason::kSubApp:
      *output = apps::InstallReason::kSubApp;
      return true;
    case InstallReason::kKiosk:
      *output = apps::InstallReason::kKiosk;
      return true;
    case InstallReason::kCommandLine:
      *output = apps::InstallReason::kCommandLine;
      return true;
  }
}

InstallSource EnumTraits<InstallSource, apps::InstallSource>::ToMojom(
    apps::InstallSource input) {
  switch (input) {
    case apps::InstallSource::kUnknown:
      return InstallSource::kUnknown;
    case apps::InstallSource::kSystem:
      return InstallSource::kSystem;
    case apps::InstallSource::kSync:
      return InstallSource::kSync;
    case apps::InstallSource::kPlayStore:
      return InstallSource::kPlayStore;
    case apps::InstallSource::kChromeWebStore:
      return InstallSource::kChromeWebStore;
    case apps::InstallSource::kBrowser:
      return InstallSource::kBrowser;
  }
}

bool EnumTraits<InstallSource, apps::InstallSource>::FromMojom(
    InstallSource input,
    apps::InstallSource* output) {
  switch (input) {
    case InstallSource::kUnknown:
      *output = apps::InstallSource::kUnknown;
      return true;
    case InstallSource::kSystem:
      *output = apps::InstallSource::kSystem;
      return true;
    case InstallSource::kSync:
      *output = apps::InstallSource::kSync;
      return true;
    case InstallSource::kPlayStore:
      *output = apps::InstallSource::kPlayStore;
      return true;
    case InstallSource::kChromeWebStore:
      *output = apps::InstallSource::kChromeWebStore;
      return true;
    case InstallSource::kBrowser:
      *output = apps::InstallSource::kBrowser;
      return true;
  }
}

WindowMode EnumTraits<WindowMode, apps::WindowMode>::ToMojom(
    apps::WindowMode input) {
  switch (input) {
    case apps::WindowMode::kUnknown:
      return WindowMode::kUnknown;
    case apps::WindowMode::kWindow:
      return WindowMode::kWindow;
    case apps::WindowMode::kBrowser:
      return WindowMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return WindowMode::kTabbedWindow;
  }
}

bool EnumTraits<WindowMode, apps::WindowMode>::FromMojom(
    WindowMode input,
    apps::WindowMode* output) {
  switch (input) {
    case WindowMode::kUnknown:
      *output = apps::WindowMode::kUnknown;
      return true;
    case WindowMode::kWindow:
      *output = apps::WindowMode::kWindow;
      return true;
    case WindowMode::kBrowser:
      *output = apps::WindowMode::kBrowser;
      return true;
    case WindowMode::kTabbedWindow:
      *output = apps::WindowMode::kTabbedWindow;
      return true;
  }
}

RunOnOsLoginMode EnumTraits<RunOnOsLoginMode, apps::RunOnOsLoginMode>::ToMojom(
    apps::RunOnOsLoginMode input) {
  switch (input) {
    case apps::RunOnOsLoginMode::kUnknown:
      return RunOnOsLoginMode::kUnknown;
    case apps::RunOnOsLoginMode::kNotRun:
      return RunOnOsLoginMode::kNotRun;
    case apps::RunOnOsLoginMode::kWindowed:
      return RunOnOsLoginMode::kWindowed;
  }
}

bool EnumTraits<RunOnOsLoginMode, apps::RunOnOsLoginMode>::FromMojom(
    RunOnOsLoginMode input,
    apps::RunOnOsLoginMode* output) {
  switch (input) {
    case RunOnOsLoginMode::kUnknown:
      *output = apps::RunOnOsLoginMode::kUnknown;
      return true;
    case RunOnOsLoginMode::kNotRun:
      *output = apps::RunOnOsLoginMode::kNotRun;
      return true;
    case RunOnOsLoginMode::kWindowed:
      *output = apps::RunOnOsLoginMode::kWindowed;
      return true;
  }
}

bool StructTraits<RunOnOsLoginDataView, apps::RunOnOsLoginPtr>::Read(
    RunOnOsLoginDataView data,
    apps::RunOnOsLoginPtr* out) {
  apps::RunOnOsLoginMode login_mode;
  if (!data.ReadLoginMode(&login_mode))
    return false;

  *out = std::make_unique<apps::RunOnOsLogin>(login_mode, data.is_managed());
  return true;
}

}  // namespace mojo
