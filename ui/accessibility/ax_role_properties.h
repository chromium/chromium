// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
#define UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

// This file contains various helper functions that determine whether an
// accessibility node data or a specific accessibility role meets certain
// criteria.
//
// Please keep these functions in alphabetic order.

// Checks if the given role is an alert or alert-dialog type.
AX_EXPORT bool IsAlert(const ax::mojom::Role role);

// Checks if the given ax node data should belong to a control that can respond
// to clicks.
AX_EXPORT bool IsClickable(const AXNodeData& data);

// Returns true if the provided role belongs to a cell or a table header.
AX_EXPORT bool IsCellOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to a container with selectable
// children.
AX_EXPORT bool IsContainerWithSelectableChildren(const ax::mojom::Role role);

// Returns true if the provided role is a control.
AX_EXPORT bool IsControl(const ax::mojom::Role role);

// Returns true if the provided role belongs to a document.
AX_EXPORT bool IsDocument(const ax::mojom::Role role);

// Returns true if the provided role represents a dialog.
AX_EXPORT bool IsDialog(const ax::mojom::Role role);

// Returns true if the given ax node data should belong to a control that is a
// a plain textfield.
AX_EXPORT bool IsPlainTextField(const AXNodeData& data);

// Returns true if the provided role belongs to a heading.
AX_EXPORT bool IsHeading(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading or a table header.
AX_EXPORT bool IsHeadingOrTableHeader(const ax::mojom::Role role);

// Returns true if the given AXNodeData has ignored state or ignored role.
AX_EXPORT bool IsIgnored(const AXNodeData& data);

// Returns true if the provided role is for any kind of image or video.
AX_EXPORT bool IsImageOrVideo(const ax::mojom::Role role);

// Returns true if the provided role belongs to an image, graphic, canvas, etc.
AX_EXPORT bool IsImage(const ax::mojom::Role role);

// Returns true if the provided ax node data is invokable.
AX_EXPORT bool IsInvokable(const AXNodeData& data);

// Returns true if the provided role is item-like, specifically if it can hold
// pos_in_set and set_size values.
AX_EXPORT bool IsItemLike(const ax::mojom::Role role);

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

// Returns true if this object supports range based value
AX_EXPORT bool IsRangeValueSupported(const AXNodeData& data);

// Returns true if the provided role belongs to a widget that can contain a
// table or grid row.
AX_EXPORT bool IsRowContainer(const ax::mojom::Role role);

// Returns true if the provided role is ordered-set like, specifically if it
// can hold set_size values.
AX_EXPORT bool IsSetLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a non-interactive list.
AX_EXPORT bool IsStaticList(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid column, and the
// table is not used for layout purposes.
AX_EXPORT bool IsTableColumn(ax::mojom::Role role);

// Returns true if the provided role belongs to a table header.
AX_EXPORT bool IsTableHeader(ax::mojom::Role role);

// Returns true if the provided role belongs to a table, a grid or a treegrid.
AX_EXPORT bool IsTableLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid row, and the
// table is not used for layout purposes.
AX_EXPORT bool IsTableRow(ax::mojom::Role role);

// Returns true if it's a text or line break node.
AX_EXPORT bool IsTextOrLineBreak(ax::mojom::Role role);

// Return true if this object supports readonly.
// Note: This returns false for table cells and headers, it is up to the
//       caller to make sure that they are included IFF they are within an
//       ARIA-1.1+ role='grid' or 'treegrid', and not role='table'.
AX_EXPORT bool IsReadOnlySupported(const ax::mojom::Role role);

// Returns true if the provided ax node data supports expand/collapse.
AX_EXPORT bool SupportsExpandCollapse(const AXNodeData& data);

// Returns true if the provided role can have an orientation.
AX_EXPORT bool SupportsOrientation(const ax::mojom::Role role);

// Returns true if the provided role supports toggle.
AX_EXPORT bool SupportsToggle(const ax::mojom::Role role);

// Returns true if the node should be read only by default
AX_EXPORT bool ShouldHaveReadonlyStateByDefault(const ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
