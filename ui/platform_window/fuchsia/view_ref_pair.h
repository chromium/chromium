// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_FUCHSIA_VIEW_REF_PAIR_H_
#define UI_PLATFORM_WINDOW_FUCHSIA_VIEW_REF_PAIR_H_

#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/zx/eventpair.h>
#include <utility>

#include "base/component_export.h"

namespace ui {

struct COMPONENT_EXPORT(PLATFORM_WINDOW) ViewRefPair {
  // ViewRef creation for the legacy graphics API. For Flatland, use
  // scenic::NewViewIdentityOnCreation().
  static ViewRefPair New();

  fuchsia::ui::views::ViewRefControl control_ref;
  fuchsia::ui::views::ViewRef view_ref;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_FUCHSIA_VIEW_REF_PAIR_H_
