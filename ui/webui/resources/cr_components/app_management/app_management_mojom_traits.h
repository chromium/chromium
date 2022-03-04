// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_
#define UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_

#include "components/services/app_service/public/cpp/permission.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

namespace mojo {

namespace {

using PermissionDataView = app_management::mojom::PermissionDataView;
using PermissionType = app_management::mojom::PermissionType;
using TriState = app_management::mojom::TriState;
using PermissionValueDataView = app_management::mojom::PermissionValueDataView;

}  // namespace

template <>
struct StructTraits<PermissionDataView, apps::PermissionPtr> {
  static apps::PermissionType permission_type(const apps::PermissionPtr& r) {
    return r->permission_type;
  }

  static const apps::PermissionValuePtr& value(const apps::PermissionPtr& r) {
    return r->value;
  }

  static bool is_managed(const apps::PermissionPtr& r) { return r->is_managed; }

  static bool Read(PermissionDataView, apps::PermissionPtr* out);
};

template <>
struct EnumTraits<PermissionType, apps::PermissionType> {
  static PermissionType ToMojom(apps::PermissionType input);
  static bool FromMojom(PermissionType input, apps::PermissionType* output);
};

template <>
struct EnumTraits<TriState, apps::TriState> {
  static TriState ToMojom(apps::TriState input);
  static bool FromMojom(TriState input, apps::TriState* output);
};

template <>
struct UnionTraits<PermissionValueDataView, apps::PermissionValuePtr> {
  static PermissionValueDataView::Tag GetTag(const apps::PermissionValuePtr& r);

  static bool IsNull(const apps::PermissionValuePtr& r) {
    return !r->bool_value.has_value() && !r->tristate_value.has_value();
  }

  static void SetToNull(apps::PermissionValuePtr* out) { out->reset(); }

  static bool bool_value(const apps::PermissionValuePtr& r) {
    return r->bool_value.value();
  }

  static apps::TriState tristate_value(const apps::PermissionValuePtr& r) {
    return r->tristate_value.value();
  }

  static bool Read(PermissionValueDataView data, apps::PermissionValuePtr* out);
};

}  // namespace mojo

#endif  // UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_
