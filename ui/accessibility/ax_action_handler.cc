// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_handler.h"

#include "ui/accessibility/ax_action_handler_registry.h"

namespace ui {

AXActionHandler::AXActionHandler()
    : AXActionHandlerBase(
          AXActionHandlerRegistry::GetInstance()->GetOrCreateAXTreeID(this)) {}

}  // namespace ui
