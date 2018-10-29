// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
#define UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// This file contains various helper functions that determine whether a specific
// accessibility role meets certain criteria.
//
// Please keep these functions in alphabetic order.

// Checks if the given role should belong to a control that can respond to
// clicks.
AX_EXPORT bool IsClickable(const ax::mojom::Role role);

// Returns true if the provided role belongs to a cell or a table header.
AX_EXPORT bool IsCellOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to a container with selectable
// children.
AX_EXPORT bool IsContainerWithSelectableChildren(const ax::mojom::Role role);

// Returns true if the provided role is a control.
AX_EXPORT bool IsControl(const ax::mojom::Role role);

// Returns true if the provided role belongs to a document.
AX_EXPORT bool IsDocument(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading.
AX_EXPORT bool IsHeading(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading or a table header.
AX_EXPORT bool IsHeadingOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to an image, graphic, canvas, etc.
AX_EXPORT bool IsImage(const ax::mojom::Role role);

// Returns true if the provided role belongs to a link.
AX_EXPORT bool IsLink(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list.
AX_EXPORT bool IsList(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list item.
AX_EXPORT bool IsListItem(const ax::mojom::Role role);

// Returns true if the provided role belongs to a menu item, including menu item
// checkbox and menu item radio buttons.
AX_EXPORT bool IsMenuItem(ax::mojom::Role role);

// Returns true if the provided role belongs to a menu or related control.
AX_EXPORT bool IsMenuRelated(const ax::mojom::Role role);

// Returns true if the provided role belongs to a widget that can contain a
// table or grid row.
AX_EXPORT bool IsRowContainer(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table header.
AX_EXPORT bool IsTableHeader(ax::mojom::Role role);

// Returns true if the provided role belongs to a table, a grid or a treegrid.
AX_EXPORT bool IsTableLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid row.
AX_EXPORT bool IsTableRow(ax::mojom::Role role);

// Returns true if the provided role supports expand/collapse.
AX_EXPORT bool SupportsExpandCollapse(const ax::mojom::Role role);

// Returns true if the provided role can have an orientation.
AX_EXPORT bool SupportsOrientation(const ax::mojom::Role role);

// Returns true if the provided role supports toggle.
AX_EXPORT bool SupportsToggle(const ax::mojom::Role role);

// Returns true if the provided role is selectable from the standpoint of UI
// Automation.
AX_EXPORT bool IsUIASelectable(const ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
