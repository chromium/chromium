// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WEBUI_RESOURCE_PATH_H_
#define UI_BASE_WEBUI_RESOURCE_PATH_H_

#include <optional>

#include "build/buildflag.h"
#include "ui/base/buildflags.h"

namespace webui {
struct ResourcePath {
  const char* const path;
  int id;
#if BUILDFLAG(LOAD_WEBUI_FROM_DISK)
  std::optional<const char* const> filepath;
#endif
};
}  // namespace webui

#endif  // UI_BASE_WEBUI_RESOURCE_PATH_H_
