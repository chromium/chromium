// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TEXT_BOUNDARY_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TEXT_BOUNDARY_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/buildflags.h"

#if BUILDFLAG(USE_ATK)
#include <atk/atk.h>
#endif  // BUILDFLAG(USE_ATK)

#if BUILDFLAG(IS_WIN)
#include <oleacc.h>

#include "third_party/iaccessible2/ia2_api_all.h"

#include <uiautomation.h>
#endif  // BUILDFLAG(IS_WIN)

namespace ui {

#if BUILDFLAG(USE_ATK)
// Converts from an ATK text boundary to an ax::mojom::TextBoundary.
COMPONENT_EXPORT(AX_PLATFORM)
ax::mojom::TextBoundary FromAtkTextBoundary(AtkTextBoundary boundary);

#if ATK_CHECK_VERSION(2, 10, 0)
// Same as above, but for an older version of the API.
COMPONENT_EXPORT(AX_PLATFORM)
ax::mojom::TextBoundary FromAtkTextGranularity(AtkTextGranularity granularity);
#endif  // ATK_CHECK_VERSION(2, 10, 0)
#endif  // BUILDFLAG(USE_ATK)

#if BUILDFLAG(IS_WIN)
// Converts from an IAccessible2 text boundary to an ax::mojom::TextBoundary.
COMPONENT_EXPORT(AX_PLATFORM)
ax::mojom::TextBoundary FromIA2TextBoundary(IA2TextBoundaryType boundary);

// Converts from a UI Automation text unit to an ax::mojom::TextBoundary.
COMPONENT_EXPORT(AX_PLATFORM)
ax::mojom::TextBoundary FromUIATextUnit(TextUnit unit);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_TEXT_BOUNDARY_H_
