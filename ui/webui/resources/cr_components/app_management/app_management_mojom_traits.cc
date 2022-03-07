// Copyright 2022 The Chromium Authors. All rights reserved.
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
    case apps::AppType::kMacOs:
      return AppType::kMacOs;
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
    case AppType::kMacOs:
      *output = apps::AppType::kMacOs;
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
  }
}

bool StructTraits<PermissionDataView, apps::PermissionPtr>::Read(
    PermissionDataView data,
    apps::PermissionPtr* out) {
  apps::PermissionType permission_type;
  if (!data.ReadPermissionType(&permission_type))
    return false;

  apps::PermissionValuePtr value;
  if (!data.ReadValue(&value))
    return false;

  *out = std::make_unique<apps::Permission>(permission_type, std::move(value),
                                            data.is_managed());
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
UnionTraits<PermissionValueDataView, apps::PermissionValuePtr>::GetTag(
    const apps::PermissionValuePtr& r) {
  if (r->bool_value.has_value()) {
    return PermissionValueDataView::Tag::BOOL_VALUE;
  } else if (r->tristate_value.has_value()) {
    return PermissionValueDataView::Tag::TRISTATE_VALUE;
  }
  NOTREACHED();
  return PermissionValueDataView::Tag::BOOL_VALUE;
}

bool UnionTraits<PermissionValueDataView, apps::PermissionValuePtr>::Read(
    PermissionValueDataView data,
    apps::PermissionValuePtr* out) {
  switch (data.tag()) {
    case PermissionValueDataView::Tag::BOOL_VALUE: {
      *out = std::make_unique<apps::PermissionValue>(data.bool_value());
      return true;
    }
    case PermissionValueDataView::Tag::TRISTATE_VALUE: {
      apps::TriState tristate_value;
      if (!data.ReadTristateValue(&tristate_value))
        return false;
      *out = std::make_unique<apps::PermissionValue>(tristate_value);
      return true;
    }
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
