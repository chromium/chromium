// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_position.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"

namespace ui {

template class EXPORT_TEMPLATE_DEFINE(AX_EXPORT)
    AXPosition<AXNodePosition, AXNode>;

}  // namespace ui
