// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_role_properties.h"

#include "build/build_config.h"

namespace ui {

namespace {

#if defined(OS_WIN) || defined(OS_CHROMEOS)
constexpr bool kExposeLayoutTableAsDataTable = true;
#else
constexpr bool kExposeLayoutTableAsDataTable = false;
#endif  // defined(OS_WIN)

}  // namespace

bool IsClickable(const ax::mojom::Role role) {
  switch (role) {
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
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
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
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return true;
    default:
      return false;
  }
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

bool IsImage(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kVideo:
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

bool IsRowContainer(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kGrid:
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

bool SupportsExpandCollapse(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kTextFieldWithComboBox:
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

bool IsUIASelectable(const ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      return false;
  }
}

}  // namespace ui
