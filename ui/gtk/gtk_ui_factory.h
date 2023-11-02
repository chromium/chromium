// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_GTK_UI_FACTORY_H_
#define UI_GTK_GTK_UI_FACTORY_H_

#include <memory>

#include "base/component_export.h"

namespace ui {
class LinuxUiAndTheme;
}

// Access point to the GTK desktop system.  This should be the only symbol
// exported from this component.
COMPONENT_EXPORT(GTK)
std::unique_ptr<ui::LinuxUiAndTheme> BuildGtkUi();

#endif  // UI_GTK_GTK_UI_FACTORY_H_
