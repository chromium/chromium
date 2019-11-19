// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_role_properties.h"

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

namespace {

#if defined(OS_WIN) || defined(OS_CHROMEOS)
constexpr bool kExposeLayoutTableAsDataTable = true;
#else
constexpr bool kExposeLayoutTableAsDataTable = false;
#endif  // defined(OS_WIN)

}  // namespace

bool IsAlert(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kAlert:
    case ax::mojom::Role::kAlertDialog:
      return true;
    default:
      return false;
  }
}

bool IsClickable(const AXNodeData& data) {
  // If it has a custom default action verb except for
  // ax::mojom::DefaultActionVerb::kClickAncestor, it's definitely clickable.
  // ax::mojom::DefaultActionVerb::kClickAncestor is used when an element with a
  // click listener is present in its ancestry chain.
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb) &&
      (data.GetDefaultActionVerb() !=
       ax::mojom::DefaultActionVerb::kClickAncestor))
    return true;

  switch (data.role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton:
      return true;
    default:
      return false;
  }
}

bool IsCellOrTableHeader(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
      return true;
    case ax::mojom::Role::kLayoutTableCell:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsContainerWithSelectableChildren(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListGrid:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
      return true;
    default:
      return false;
  }
}

bool IsControl(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListGrid:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kTree:
      return true;
    default:
      return false;
  }
}

bool IsDocument(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return true;
    default:
      return false;
  }
}

bool IsDialog(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kAlertDialog:
    case ax::mojom::Role::kDialog:
      return true;
    default:
      return false;
  }
}

bool IsPlainTextField(const AXNodeData& data) {
  // We need to check both the role and editable state, because some ARIA text
  // fields may in fact not be editable, whilst some editable fields might not
  // have the role.
  return !data.HasState(ax::mojom::State::kRichlyEditable) &&
         (data.role == ax::mojom::Role::kTextField ||
          data.role == ax::mojom::Role::kTextFieldWithComboBox ||
          data.role == ax::mojom::Role::kSearchBox ||
          data.GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot));
}

bool IsHeading(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kDocSubtitle:
      return true;
    default:
      return false;
  }
}

bool IsHeadingOrTableHeader(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kRowHeader:
      return true;
    default:
      return false;
  }
}

bool IsIgnored(const AXNodeData& data) {
  if (data.HasState(ax::mojom::State::kIgnored) ||
      data.role == ax::mojom::Role::kIgnored)
    return true;
  return false;
}

bool IsImage(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kSvgRoot:
      return true;
    default:
      return false;
  }
}

bool IsImageOrVideo(const ax::mojom::Role role) {
  return IsImage(role) || role == ax::mojom::Role::kVideo;
}

bool IsInvokable(const AXNodeData& data) {
  // A control is "invokable" if it initiates an action when activated but
  // does not maintain any state. A control that maintains state when activated
  // would be considered a toggle or expand-collapse element - these elements
  // are "clickable" but not "invokable".
  return IsClickable(data) && !SupportsExpandCollapse(data) &&
         !SupportsToggle(data.role);
}

bool IsItemLike(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kTreeItem:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kDescriptionListTerm:
    case ax::mojom::Role::kTerm:
      return true;
    default:
      return false;
  }
}

bool IsLink(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
    case ax::mojom::Role::kLink:
      return true;
    default:
      return false;
  }
}

bool IsList(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDirectory:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListGrid:
      return true;
    default:
      return false;
  }
}

bool IsListItem(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kDescriptionListTerm:
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kTerm:
      return true;
    default:
      return false;
  }
}

bool IsMenuItem(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
      return true;
    default:
      return false;
  }
}

bool IsMenuRelated(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
      return true;
    default:
      return false;
  }
}

bool IsRangeValueSupported(const AXNodeData& data) {
  // https://www.w3.org/TR/wai-aria-1.1/#aria-valuenow
  // https://www.w3.org/TR/wai-aria-1.1/#aria-valuetext
  // Roles that support aria-valuetext / aria-valuenow
  switch (data.role) {
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
      return true;
    case ax::mojom::Role::kSplitter:
      return data.HasState(ax::mojom::State::kFocusable);
    default:
      return false;
  }
}

bool IsRowContainer(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListGrid:
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
      return true;
    case ax::mojom::Role::kLayoutTable:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsSetLike(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDirectory:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kFeed:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListGrid:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kPopUpButton:
      return true;
    default:
      return false;
  }
}

bool IsStaticList(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kDescriptionList:
      return true;
    default:
      return false;
  }
}

bool IsTableColumn(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kColumn:
      return true;
    case ax::mojom::Role::kLayoutTableColumn:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsTableHeader(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
      return true;
    default:
      return false;
  }
}

bool IsTableLike(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListGrid:
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kTreeGrid:
      return true;
    case ax::mojom::Role::kLayoutTable:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsTableRow(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kRow:
      return true;
    case ax::mojom::Role::kLayoutTableRow:
      return kExposeLayoutTableAsDataTable;
    default:
      return false;
  }
}

bool IsTextOrLineBreak(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kLineBreak:
    case ax::mojom::Role::kStaticText:
      return true;
    default:
      return false;
  }
}

bool IsReadOnlySupported(const ax::mojom::Role role) {
  // https://www.w3.org/TR/wai-aria-1.1/#aria-readonly
  // Roles that support aria-readonly
  switch (role) {
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTreeGrid:
      return true;

    // https://www.w3.org/TR/wai-aria-1.1/#aria-readonly
    // ARIA-1.1+ 'gridcell', supports aria-readonly, but 'cell' does not
    //
    // https://www.w3.org/TR/wai-aria-1.1/#columnheader
    // https://www.w3.org/TR/wai-aria-1.1/#rowheader
    // While the [columnheader|rowheader] role can be used in both interactive
    // grids and non-interactive tables, the use of aria-readonly and
    // aria-required is only applicable to interactive elements.
    // Therefore, [...] user agents SHOULD NOT expose either property to
    // assistive technologies unless the columnheader descends from a grid.
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kRowHeader:
    case ax::mojom::Role::kColumnHeader:
      return false;
    default:
      break;
  }
  return false;
}

bool SupportsExpandCollapse(const AXNodeData& data) {
  if (data.GetHasPopup() != ax::mojom::HasPopup::kFalse ||
      data.HasState(ax::mojom::State::kExpanded) ||
      data.HasState(ax::mojom::State::kCollapsed))
    return true;

  switch (data.role) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      return false;
  }
}

bool SupportsOrientation(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTree:
      return true;
    default:
      return false;
  }
}

bool SupportsToggle(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kToggleButton:
      return true;
    default:
      return false;
  }
}

bool ShouldHaveReadonlyStateByDefault(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDescriptionListTerm:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kGraphicsDocument:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTimer:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTooltip:
    case ax::mojom::Role::kWebArea:
      return true;

    case ax::mojom::Role::kGrid:
      // TODO(aleventhal) this changed between ARIA 1.0 and 1.1,
      // need to determine whether grids/treegrids should really be readonly
      // or editable by default
      break;

    default:
      break;
  }
  return false;
}

}  // namespace ui
