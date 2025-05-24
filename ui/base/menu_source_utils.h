// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MENU_SOURCE_UTILS_H_
#define UI_BASE_MENU_SOURCE_UTILS_H_

#include "base/component_export.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

namespace ui {

class Event;

COMPONENT_EXPORT(UI_BASE)
mojom::MenuSourceType GetMenuSourceTypeForEvent(const Event& event);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns the menu source type based on `event_flags`.
COMPONENT_EXPORT(UI_BASE)
mojom::MenuSourceType GetMenuSourceType(int event_flags);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ui

#endif  // UI_BASE_MENU_SOURCE_UTILS_H_
