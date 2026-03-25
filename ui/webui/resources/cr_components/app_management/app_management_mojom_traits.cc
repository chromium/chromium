// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/resources/cr_components/app_management/app_management_mojom_traits.h"

#include <utility>
#include <variant>

#include "base/notreached.h"

namespace mojo {

AppType EnumTraits<AppType, apps::AppType>::ToMojom(apps::AppType input) {
  switch (input) {
    case apps::AppType::kUnknown:
      return AppType::kUnknown;
    case apps::AppType::kArc:
      return AppType::kArc;
    case apps::AppType::kCrostini:
      return AppType::kCrostini;
    case apps::AppType::kChromeApp:
      return AppType::kChromeApp;
    case apps::AppType::kWeb:
      return AppType::kWeb;
    case apps::AppType::kPluginVm:
      return AppType::kPluginVm;
    case apps::AppType::kRemote:
      return AppType::kRemote;
    case apps::AppType::kBorealis:
      return AppType::kBorealis;
    case apps::AppType::kSystemWeb:
      return AppType::kSystemWeb;
    case apps::AppType::kExtension:
      return AppType::kExtension;
    case apps::AppType::kBruschetta:
      return AppType::kBruschetta;
  }
}

apps::AppType EnumTraits<AppType, apps::AppType>::FromMojom(AppType input) {
  switch (input) {
    case AppType::kUnknown:
      return apps::AppType::kUnknown;
    case AppType::kArc:
      return apps::AppType::kArc;
    case AppType::kCrostini:
      return apps::AppType::kCrostini;
    case AppType::kChromeApp:
      return apps::AppType::kChromeApp;
    case AppType::kWeb:
      return apps::AppType::kWeb;
    case AppType::kPluginVm:
      return apps::AppType::kPluginVm;
    case AppType::kRemote:
      return apps::AppType::kRemote;
    case AppType::kBorealis:
      return apps::AppType::kBorealis;
    case AppType::kSystemWeb:
      return apps::AppType::kSystemWeb;
    case AppType::kExtension:
      return apps::AppType::kExtension;
    case AppType::kBruschetta:
      return apps::AppType::kBruschetta;
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

apps::PermissionType
EnumTraits<PermissionType, apps::PermissionType>::FromMojom(
    PermissionType input) {
  switch (input) {
    case PermissionType::kUnknown:
      return apps::PermissionType::kUnknown;
    case PermissionType::kCamera:
      return apps::PermissionType::kCamera;
    case PermissionType::kLocation:
      return apps::PermissionType::kLocation;
    case PermissionType::kMicrophone:
      return apps::PermissionType::kMicrophone;
    case PermissionType::kNotifications:
      return apps::PermissionType::kNotifications;
    case PermissionType::kContacts:
      return apps::PermissionType::kContacts;
    case PermissionType::kStorage:
      return apps::PermissionType::kStorage;
    case PermissionType::kPrinting:
      return apps::PermissionType::kPrinting;
    case PermissionType::kFileHandling:
      return apps::PermissionType::kFileHandling;
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

apps::TriState EnumTraits<TriState, apps::TriState>::FromMojom(TriState input) {
  switch (input) {
    case TriState::kAllow:
      return apps::TriState::kAllow;
    case TriState::kBlock:
      return apps::TriState::kBlock;
    case TriState::kAsk:
      return apps::TriState::kAsk;
  }
}

PermissionValueDataView::Tag
UnionTraits<PermissionValueDataView, apps::Permission::PermissionValue>::GetTag(
    const apps::Permission::PermissionValue& r) {
  if (std::holds_alternative<bool>(r)) {
    return PermissionValueDataView::Tag::kBoolValue;
  } else if (std::holds_alternative<apps::TriState>(r)) {
    return PermissionValueDataView::Tag::kTristateValue;
  }
  NOTREACHED();
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
  NOTREACHED();
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

apps::InstallReason EnumTraits<InstallReason, apps::InstallReason>::FromMojom(
    InstallReason input) {
  switch (input) {
    case InstallReason::kUnknown:
      return apps::InstallReason::kUnknown;
    case InstallReason::kSystem:
      return apps::InstallReason::kSystem;
    case InstallReason::kPolicy:
      return apps::InstallReason::kPolicy;
    case InstallReason::kOem:
      return apps::InstallReason::kOem;
    case InstallReason::kDefault:
      return apps::InstallReason::kDefault;
    case InstallReason::kSync:
      return apps::InstallReason::kSync;
    case InstallReason::kUser:
      return apps::InstallReason::kUser;
    case InstallReason::kSubApp:
      return apps::InstallReason::kSubApp;
    case InstallReason::kKiosk:
      return apps::InstallReason::kKiosk;
    case InstallReason::kCommandLine:
      return apps::InstallReason::kCommandLine;
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

apps::InstallSource EnumTraits<InstallSource, apps::InstallSource>::FromMojom(
    InstallSource input) {
  switch (input) {
    case InstallSource::kUnknown:
      return apps::InstallSource::kUnknown;
    case InstallSource::kSystem:
      return apps::InstallSource::kSystem;
    case InstallSource::kSync:
      return apps::InstallSource::kSync;
    case InstallSource::kPlayStore:
      return apps::InstallSource::kPlayStore;
    case InstallSource::kChromeWebStore:
      return apps::InstallSource::kChromeWebStore;
    case InstallSource::kBrowser:
      return apps::InstallSource::kBrowser;
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

apps::WindowMode EnumTraits<WindowMode, apps::WindowMode>::FromMojom(
    WindowMode input) {
  switch (input) {
    case WindowMode::kUnknown:
      return apps::WindowMode::kUnknown;
    case WindowMode::kWindow:
      return apps::WindowMode::kWindow;
    case WindowMode::kBrowser:
      return apps::WindowMode::kBrowser;
    case WindowMode::kTabbedWindow:
      return apps::WindowMode::kTabbedWindow;
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

apps::RunOnOsLoginMode
EnumTraits<RunOnOsLoginMode, apps::RunOnOsLoginMode>::FromMojom(
    RunOnOsLoginMode input) {
  switch (input) {
    case RunOnOsLoginMode::kUnknown:
      return apps::RunOnOsLoginMode::kUnknown;
    case RunOnOsLoginMode::kNotRun:
      return apps::RunOnOsLoginMode::kNotRun;
    case RunOnOsLoginMode::kWindowed:
      return apps::RunOnOsLoginMode::kWindowed;
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
