// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/linux_desktop.h"

#include <optional>
#include <vector>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/values.h"
#include "ui/display/util/gpu_info_util.h"

namespace ui {

base::Value::List GetDesktopEnvironmentInfo() {
  base::Value::List result;
  auto env(base::Environment::Create());

  std::optional<std::string> value =
      env->GetVar(base::nix::kXdgCurrentDesktopEnvVar);
  if (value.has_value()) {
    result.Append(display::BuildGpuInfoEntry(
        base::nix::kXdgCurrentDesktopEnvVar, *value));
  }

  value = env->GetVar(base::nix::kXdgSessionTypeEnvVar);
  if (value.has_value()) {
    result.Append(
        display::BuildGpuInfoEntry(base::nix::kXdgSessionTypeEnvVar, *value));
  }
  constexpr char kGDMSession[] = "GDMSESSION";
  value = env->GetVar(kGDMSession);
  if (value.has_value()) {
    result.Append(display::BuildGpuInfoEntry(kGDMSession, *value));
  }

  return result;
}

}  // namespace ui
