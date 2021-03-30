// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/linux_desktop.h"

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace ui {

namespace {

// Must be in sync with the copy in //content/browser/gpu/gpu_internals_ui.cc.
base::Value NewDescriptionValuePair(base::StringPiece desc,
                                    base::StringPiece value) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("description", base::Value(desc));
  dict.SetKey("value", base::Value(value));
  return dict;
}

}  // namespace

base::Value GetDesktopEnvironmentInfoAsListValue() {
  base::Value result(base::Value::Type::LIST);
  auto env(base::Environment::Create());
  std::string value;
  if (env->GetVar(base::nix::kXdgCurrentDesktopEnvVar, &value)) {
    result.Append(
        NewDescriptionValuePair(base::nix::kXdgCurrentDesktopEnvVar, value));
  }
  if (env->GetVar(base::nix::kXdgSessionTypeEnvVar, &value)) {
    result.Append(
        NewDescriptionValuePair(base::nix::kXdgSessionTypeEnvVar, value));
  }
  constexpr char kGDMSession[] = "GDMSESSION";
  if (env->GetVar(kGDMSession, &value))
    result.Append(NewDescriptionValuePair(kGDMSession, value));
  return result;
}

}  // namespace ui
