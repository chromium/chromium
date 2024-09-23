// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
#define UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

// This file contains various helper functions that determine whether a
// specific accessibility role meets certain criteria.
//
// Please keep these functions in alphabetic order.

// When using these functions in Blink, it's necessary to add the function names
// to third_party/blink/tools/blinkpy/presubmit/audit_non_blink_usage.py, in
// order to pass presubmit.

// Returns true for text parents that can have inline text box children.
AX_BASE_EXPORT bool CanHaveInlineTextBoxChildren(ax::mojom::Role role);

// Returns true for object roles that have the attribute "Children
// Presentational: True" as defined in the ARIA Specification.
// https://www.w3.org/TR/wai-aria-1.1/#childrenArePresentational.
AX_BASE_EXPORT bool HasPresentationalChildren(const ax::mojom::Role role);

// Returns true if the given role is an alert or alert-dialog type.
AX_BASE_EXPORT bool IsAlert(const ax::mojom::Role role);

// Returns true if the given role is a candidate to be labeled with a classname
// of TextView on Android.
AX_BASE_EXPORT bool IsAndroidTextViewCandidate(const ax::mojom::Role role);

// Returns true if the provided role belongs to a native or an ARIA button.
AX_BASE_EXPORT bool IsButton(const ax::mojom::Role role);

// Returns true if the provided role belongs to a cell or a table header.
AX_BASE_EXPORT bool IsCellOrTableHeader(const ax::mojom::Role role);

// Returns true if the provided role belongs to an object on which a click
// handler is commonly attached, or to an object that carries out an action when
// clicked, such as activating itself, opening a dialog or closing a menu.
//
// A button and a checkbox fall in the first category, whilst a color well and a
// list menu option in the second. Note that a text field, or a similar element,
// also carries out an action when clicked. It focuses itself, so the action
// verb is "activate". Not all roles that inherently support a click handler or
// that can potentially be focused are included, because in that case even a div
// could be made clickable or focusable.
//
// The reason for the existence of this function is that certain screen readers,
// such as Jaws, might need to report such objects as clickable to their users,
// so that users will know that they could activate them if they so choose.
AX_BASE_EXPORT bool IsClickable(const ax::mojom::Role role);

// Returns true if the provided role is any of the checkbox-related roles.
AX_BASE_EXPORT bool IsCheckBox(ax::mojom::Role role);

// Returns true if the provided role is any of the combobox-related roles.
AX_BASE_EXPORT bool IsComboBox(ax::mojom::Role role);

// Returns true if the provided role is a container that can hold the choices
// for a combobox. For example, a dialog could pop up a list of choices for a
// user, and so a dialog is a potential combobox container.
AX_BASE_EXPORT bool IsComboBoxContainer(const ax::mojom::Role role);

// Returns true if the provided role belongs to a container with selectable
// children.
AX_BASE_EXPORT bool IsContainerWithSelectableChildren(
    const ax::mojom::Role role);

// Returns true if the provided role is a control.
AX_BASE_EXPORT bool IsControl(const ax::mojom::Role role);

// Returns true if the provided role is a control on the Android platform.
AX_BASE_EXPORT bool IsControlOnAndroid(const ax::mojom::Role role,
                                       bool isFocusable);

// Returns true for an <input> used for a date or time.
AX_BASE_EXPORT bool IsDateOrTimeInput(const ax::mojom::Role role);

// Returns true if the provided role represents a dialog.
AX_BASE_EXPORT bool IsDialog(const ax::mojom::Role role);

// Returns true if the role represents an HTML embedding element, i.e. an
// element that can embed content from another source such as an <iframe> or a
// <embed>.
//
// Can return false for an embedding element if an ARIA role was used, e.g.
// <iframe role="region">.
AX_BASE_EXPORT bool IsEmbeddingElement(const ax::mojom::Role role);

// Returns true if the provided role is a form.
AX_BASE_EXPORT bool IsForm(const ax::mojom::Role role);

// Returns true if crossing into or out of the provided role should count as
// crossing a format boundary.
AX_BASE_EXPORT bool IsFormatBoundary(const ax::mojom::Role role);

// Returns true if the provided role belongs to a grid, treegrid, listgrid
// or tree. Returns false for table.
AX_BASE_EXPORT bool IsGridLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a heading.
AX_BASE_EXPORT bool IsHeading(const ax::mojom::Role role);

// Returns true if the provided role belongs to an iframe.
AX_BASE_EXPORT bool IsIframe(const ax::mojom::Role role);

// Returns true if the provided role belongs to an image, graphic, canvas, etc.
AX_BASE_EXPORT bool IsImage(const ax::mojom::Role role);

// Returns true if the provided role is for any kind of image or video.
AX_BASE_EXPORT bool IsImageOrVideo(const ax::mojom::Role role);

// Returns true if the provided role is item-like, specifically if it can hold
// pos_in_set and set_size values. Roles that are item-like are not set-like.
AX_BASE_EXPORT bool IsItemLike(const ax::mojom::Role role);

// Returns true if the provided role is likely to be an active descendant.
AX_BASE_EXPORT bool IsLikelyActiveDescendantRole(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Landmark abstract role.
AX_BASE_EXPORT bool IsLandmark(const ax::mojom::Role role);

// Returns true if the provided role belongs to a link.
AX_BASE_EXPORT bool IsLink(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list.
AX_BASE_EXPORT bool IsList(const ax::mojom::Role role);

// Returns true if the provided role belongs to a list item.
AX_BASE_EXPORT bool IsListItem(const ax::mojom::Role role);

// Returns true if the provided role belongs to a menu item, including menu item
// checkbox and menu item radio buttons.
AX_BASE_EXPORT bool IsMenuItem(ax::mojom::Role role);

// Returns true if the provided role belongs to a menu or related control.
AX_BASE_EXPORT bool IsMenuRelated(const ax::mojom::Role role);

// Returns true if the provided role belongs to a node that is at the root of
// what most accessibility APIs consider to be a document, such as the root of a
// webpage, an iframe, or a PDF.
AX_BASE_EXPORT bool IsPlatformDocument(const ax::mojom::Role role);

// Returns true if the provided role is presentational in nature, i.e. a node
// whose implicit native role semantics will not be mapped to the accessibility
// API.
AX_BASE_EXPORT bool IsPresentational(const ax::mojom::Role role);

// Returns true if the provided role belongs to a radio.
AX_BASE_EXPORT bool IsRadio(const ax::mojom::Role role);

// Returns true if the provided role supports a range-based value, such as a
// slider.
AX_BASE_EXPORT bool IsRangeValueSupported(const ax::mojom::Role role);

// Returns true if this object supports readonly.
//
// Note: This returns false for table headers, it is up to the caller to make
//       sure that they are included IFF they are within an ARIA-1.1+
//       role='grid' or 'treegrid', and not role='table'.
AX_BASE_EXPORT bool IsReadOnlySupported(const ax::mojom::Role role);

// Returns true if the provided role is at the root of a window-like container,
// (AKA a widget in Views terminology), such as the root of the web contents, a
// window, a dialog or the whole desktop.
AX_BASE_EXPORT bool IsRootLike(ax::mojom::Role role);

// Returns true if the provided role belongs to a widget that can contain a
// table or grid row.
AX_BASE_EXPORT bool IsRowContainer(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Section abstract role.
AX_BASE_EXPORT bool IsSection(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Sectionhead role.
AX_BASE_EXPORT bool IsSectionhead(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Select abstract role.
AX_BASE_EXPORT bool IsSelect(const ax::mojom::Role role);

// Returns true if the role is one of those exposed by the HTML <select>
// element.
AX_BASE_EXPORT bool IsSelectElement(const ax::mojom::Role role);

// Returns true if the provided role either requires or has an implicit value
// for aria-selected state.
AX_BASE_EXPORT bool IsSelectRequiredOrImplicit(const ax::mojom::Role role);

// Returns true if the provided role supports aria-selected state.
AX_BASE_EXPORT bool IsSelectSupported(const ax::mojom::Role role);

// Returns true if the provided role is ordered-set like, specifically if it
// can hold set_size values. Roles that are set-like are not item-like.
AX_BASE_EXPORT bool IsSetLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a non-interactive list.
AX_BASE_EXPORT bool IsStaticList(const ax::mojom::Role role);

// Returns true if the role is a subclass of the ARIA Structure abstract role.
AX_BASE_EXPORT bool IsStructure(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid column, and the
// table is not used for layout purposes.
AX_BASE_EXPORT bool IsTableColumn(ax::mojom::Role role);

// Returns true if the provided role belongs to a table header.
AX_BASE_EXPORT bool IsTableHeader(ax::mojom::Role role);

// Returns true if the provided role belongs to an item that could be contained
// in a table-like container. For example, a cell or a column header.
AX_BASE_EXPORT bool IsTableItem(ax::mojom::Role role);

// Returns true if the provided role belongs to a table, a grid or a treegrid.
// On Android this also returns true for the roles: lists, list boxes,
// trees, description lists and directories.
AX_BASE_EXPORT bool IsTableLike(const ax::mojom::Role role);

// Returns true if the provided role belongs to a table or grid row, and the
// table is not used for layout purposes.
AX_BASE_EXPORT bool IsTableRow(ax::mojom::Role role);

// Returns true if the provided role belongs to a table, a grid, or a treegrid.
AX_BASE_EXPORT bool IsTableWithColumns(ax::mojom::Role role);

// Returns true if the provided role is text-related, e.g., static text, line
// break, or inline text box.
AX_BASE_EXPORT bool IsText(ax::mojom::Role role);

// Returns true if the provided role belongs to a native text field, i.e.
// <input> or <textarea>.
AX_BASE_EXPORT bool IsTextField(ax::mojom::Role role);

// Returns true if the provided role fits the description of a UIA embedded
// objects. See the method definition for more details.
AX_BASE_EXPORT bool IsUIAEmbeddedObject(ax::mojom::Role role);

// Returns false if |role| is a layout table, or whatever `IsTableLike` returns.
AX_BASE_EXPORT bool IsUIATableLike(ax::mojom::Role role);

// Returns false if |role| is a layout table cell, or whatever
// `IsCellOrTableHeader` returns.
AX_BASE_EXPORT bool IsUIACellOrTableHeader(ax::mojom::Role role);

// Returns true if the provided role represents a window.
AX_BASE_EXPORT bool IsWindow(const ax::mojom::Role role);

// Returns true if the node should be read only by default
AX_BASE_EXPORT bool ShouldHaveReadonlyStateByDefault(
    const ax::mojom::Role role);

// Returns true if the role supports expand/collapse.
AX_BASE_EXPORT bool SupportsExpandCollapse(const ax::mojom::Role role);

// Returns true if the role supports hierarchical level.
AX_BASE_EXPORT bool SupportsHierarchicalLevel(const ax::mojom::Role role);

// Returns true if the provided role can have an orientation.
AX_BASE_EXPORT bool SupportsOrientation(const ax::mojom::Role role);

// Returns true if the provided role can have the required attribute,
// e.g. <div contenteditable aria-required></div> or <input required>
AX_BASE_EXPORT bool SupportsRequired(const ax::mojom::Role role);

// Returns true if the provided role supports toggle.
AX_BASE_EXPORT bool SupportsToggle(const ax::mojom::Role role);

// Returns true if the provided role is one of the roles considered plain
// elements. Check AXObject::IsPlainContent()
AX_BASE_EXPORT bool IsPlainContentElement(const ax::mojom::Role role);

// Returns true if the provided role allows for arrow keys expanding or
// collapsing the element.
// https://www.w3.org/WAI/ARIA/apg/patterns/treeview/examples/treeview-1b/
AX_BASE_EXPORT bool SupportsArrowKeysForExpandCollapse(
    const ax::mojom::Role role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
