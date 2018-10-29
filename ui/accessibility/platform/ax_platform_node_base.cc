// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

const base::char16 AXPlatformNodeBase::kEmbeddedCharacter = L'\xfffc';

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  static const base::NoDestructor<AXNodeData> empty_data;
  if (delegate_)
    return delegate_->GetData();
  return *empty_data;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  if (delegate_)
    return delegate_->GetParent();
  return nullptr;
}

int AXPlatformNodeBase::GetChildCount() {
  if (delegate_)
    return delegate_->GetChildCount();
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  if (delegate_)
    return delegate_->ChildAtIndex(index);
  return nullptr;
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  AXPlatformNode::Destroy();
  delegate_ = nullptr;
  Dispose();
}

void AXPlatformNodeBase::Dispose() {
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return nullptr;
}

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() {
  if (!delegate_)
    return nullptr;
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int previous_index = GetIndexInParent() - 1;
  if (previous_index >= 0 &&
      previous_index < parent->GetChildCount()) {
    return FromNativeViewAccessible(parent->ChildAtIndex(previous_index));
  }
  return nullptr;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() {
  if (!delegate_)
    return nullptr;
  gfx::NativeViewAccessible parent_accessible = GetParent();
  AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);
  if (!parent)
    return nullptr;

  int next_index = GetIndexInParent() + 1;
  if (next_index >= 0 && next_index < parent->GetChildCount())
    return FromNativeViewAccessible(parent->ChildAtIndex(next_index));
  return nullptr;
}

bool AXPlatformNodeBase::IsDescendant(AXPlatformNodeBase* node) {
  if (!delegate_)
    return false;
  if (!node)
    return false;
  if (node == this)
    return true;
  gfx::NativeViewAccessible native_parent = node->GetParent();
  if (!native_parent)
    return false;
  AXPlatformNodeBase* parent = FromNativeViewAccessible(native_parent);
  return IsDescendant(parent);
}

bool AXPlatformNodeBase::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeBase::GetBoolAttribute(ax::mojom::BoolAttribute attribute,
                                          bool* value) const {
  if (!delegate_)
    return false;
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeBase::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeBase::GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                                           float* value) const {
  if (!delegate_)
    return false;
  return GetData().GetFloatAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeBase::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntAttribute(ax::mojom::IntAttribute attribute,
                                         int* value) const {
  if (!delegate_)
    return false;
  return GetData().GetIntAttribute(attribute, value);
}

bool AXPlatformNodeBase::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return base::EmptyString();
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  if (!delegate_)
    return false;
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 AXPlatformNodeBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  if (!delegate_)
    return base::string16();
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    base::string16* value) const {
  if (!delegate_)
    return false;
  return GetData().GetString16Attribute(attribute, value);
}

bool AXPlatformNodeBase::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  if (!delegate_)
    return false;
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  static const base::NoDestructor<std::vector<int32_t>> empty_data;
  if (!delegate_)
    return *empty_data;
  return GetData().GetIntListAttribute(attribute);
}

bool AXPlatformNodeBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  if (!delegate_)
    return false;
  return GetData().GetIntListAttribute(attribute, value);
}

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
}

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

bool AXPlatformNodeBase::SetTextSelection(int start_offset, int end_offset) {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = action_data.focus_node_id = GetData().id;
  action_data.anchor_offset = start_offset;
  action_data.focus_offset = end_offset;
  if (!delegate_)
    return false;

  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeBase::IsTextOnlyObject() const {
  return GetData().role == ax::mojom::Role::kStaticText ||
         GetData().role == ax::mojom::Role::kLineBreak ||
         GetData().role == ax::mojom::Role::kInlineTextBox;
}

// TODO(crbug.com/865101) Remove this once the autofill state works.
bool AXPlatformNodeBase::IsFocusedInputWithSuggestions() {
  return HasInputSuggestions() && IsPlainTextField() &&
         delegate_->GetFocus() == GetNativeViewAccessible();
}

bool AXPlatformNodeBase::IsPlainTextField() const {
  // We need to check both the role and editable state, because some ARIA text
  // fields may in fact not be editable, whilst some editable fields might not
  // have the role.
  return !GetData().HasState(ax::mojom::State::kRichlyEditable) &&
         (GetData().role == ax::mojom::Role::kTextField ||
          GetData().role == ax::mojom::Role::kTextFieldWithComboBox ||
          GetData().role == ax::mojom::Role::kSearchBox ||
          GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot));
}

bool AXPlatformNodeBase::IsRichTextField() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot) &&
         GetData().HasState(ax::mojom::State::kRichlyEditable);
}

std::string AXPlatformNodeBase::GetInnerText() {
  if (IsTextOnlyObject())
    return GetStringAttribute(ax::mojom::StringAttribute::kName);

  std::string text;
  for (int i = 0; i < GetChildCount(); ++i) {
    gfx::NativeViewAccessible child_accessible = ChildAtIndex(i);
    AXPlatformNodeBase* child = FromNativeViewAccessible(child_accessible);
    if (!child)
      continue;

    text += child->GetInnerText();
  }
  return text;
}

bool AXPlatformNodeBase::IsRangeValueSupported() const {
  switch (GetData().role) {
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kScrollBar:
      return true;
    case ax::mojom::Role::kSplitter:
      return GetData().HasState(ax::mojom::State::kFocusable);
    default:
      return false;
  }
}

base::string16 AXPlatformNodeBase::GetRangeValueText() {
  float fval;
  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);

  if (value.empty() &&
      GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &fval)) {
    value = base::NumberToString16(fval);
  }
  return value;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetSelectionContainer() const {
  if (!delegate_)
    return nullptr;
  AXPlatformNodeBase* container = const_cast<AXPlatformNodeBase*>(this);
  while (container &&
         !IsContainerWithSelectableChildren(container->GetData().role)) {
    gfx::NativeViewAccessible parent_accessible = container->GetParent();
    AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);

    container = parent;
  }
  return container;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTable() const {
  if (!delegate_)
    return nullptr;
  AXPlatformNodeBase* table = const_cast<AXPlatformNodeBase*>(this);
  while (table && !IsTableLike(table->GetData().role)) {
    gfx::NativeViewAccessible parent_accessible = table->GetParent();
    AXPlatformNodeBase* parent = FromNativeViewAccessible(parent_accessible);

    table = parent;
  }
  return table;
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int index) const {
  if (!delegate_)
    return nullptr;
  if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(table->delegate_->CellIndexToId(index)));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int row,
                                                     int column) const {
  if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
    return nullptr;

  if (row < 0 || row >= GetTableRowCount() || column < 0 ||
      column >= GetTableColumnCount()) {
    return nullptr;
  }

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;

  int32_t cell_id = table->delegate_->GetCellId(row, column);
  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(cell_id));
}

int AXPlatformNodeBase::GetTableCellIndex() const {
  return delegate_->GetTableCellIndex();
}

int AXPlatformNodeBase::GetTableColumn() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex);
}

int AXPlatformNodeBase::GetTableColumnCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);
}

int AXPlatformNodeBase::GetTableColumnSpan() const {
  if (!IsCellOrTableHeader(GetData().role))
    return 0;

  int column_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                      &column_span))
    return column_span;
  return 1;
}

int AXPlatformNodeBase::GetTableRow() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
}

int AXPlatformNodeBase::GetTableRowCount() const {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return 0;

  return table->GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount);
}

int AXPlatformNodeBase::GetTableRowSpan() const {
  if (!IsCellOrTableHeader(GetData().role))
    return 0;

  int row_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, &row_span))
    return row_span;
  return 1;
}

bool AXPlatformNodeBase::HasCaret() {
  if (IsPlainTextField() &&
      HasIntAttribute(ax::mojom::IntAttribute::kTextSelStart) &&
      HasIntAttribute(ax::mojom::IntAttribute::kTextSelEnd)) {
    return true;
  }

  // The caret is always at the focus of the selection.
  int32_t focus_id = delegate_->GetTreeData().sel_focus_object_id;
  AXPlatformNodeBase* focus_object =
      static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(focus_id));

  if (!focus_object)
    return false;

  return focus_object->IsDescendantOf(this);
}

bool AXPlatformNodeBase::IsDescendantOf(AXPlatformNodeBase* ancestor) {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return false;

  return parent->IsDescendantOf(ancestor);
}

bool AXPlatformNodeBase::IsLeaf() {
  if (GetChildCount() == 0)
    return true;

  // These types of objects may have children that we use as internal
  // implementation details, but we want to expose them as leaves to platform
  // accessibility APIs because screen readers might be confused if they find
  // any children.
  if (IsPlainTextField() || IsTextOnlyObject())
    return true;

  // Roles whose children are only presentational according to the ARIA and
  // HTML5 Specs should be hidden from screen readers.
  // (Note that whilst ARIA buttons can have only presentational children, HTML5
  // buttons are allowed to have content.)
  switch (GetData().role) {
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kProgressIndicator:
      return true;
    default:
      return false;
  }
}

bool AXPlatformNodeBase::IsChildOfLeaf() {
  AXPlatformNodeBase* ancestor = FromNativeViewAccessible(GetParent());

  while (ancestor) {
    if (ancestor->IsLeaf())
      return true;
    ancestor = FromNativeViewAccessible(ancestor->GetParent());
  }

  return false;
}

bool AXPlatformNodeBase::IsScrollable() const {
  return (HasIntAttribute(ax::mojom::IntAttribute::kScrollXMin) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollXMax) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollX)) ||
         (HasIntAttribute(ax::mojom::IntAttribute::kScrollYMin) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollYMax) &&
          HasIntAttribute(ax::mojom::IntAttribute::kScrollY));
}

bool AXPlatformNodeBase::IsHorizontallyScrollable() const {
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin), 0)
      << "Pixel sizes should be non-negative.";
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax), 0)
      << "Pixel sizes should be non-negative.";
  return IsScrollable() &&
         GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin) <
             GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
}

bool AXPlatformNodeBase::IsVerticallyScrollable() const {
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin), 0)
      << "Pixel sizes should be non-negative.";
  DCHECK_GE(GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax), 0)
      << "Pixel sizes should be non-negative.";
  return IsScrollable() &&
         GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin) <
             GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
}

std::string AXPlatformNodeBase::GetText() {
  return GetInnerText();
}

base::string16 AXPlatformNodeBase::GetValue() {
  // Expose slider value.
  if (IsRangeValueSupported()) {
    return GetRangeValueText();
  } else if (ui::IsDocument(GetData().role)) {
    // On Windows, the value of a document should be its URL.
    return base::UTF8ToUTF16(delegate_->GetTreeData().url);
  }
  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);

  // Some screen readers like Jaws and VoiceOver require a
  // value to be set in text fields with rich content, even though the same
  // information is available on the children.
  if (value.empty() && IsRichTextField())
    return base::UTF8ToUTF16(GetInnerText());

  return value;
}

void AXPlatformNodeBase::ComputeAttributes(PlatformAttributeList* attributes) {
  // Expose some HTML and ARIA attributes in the IAccessible2 attributes string
  // "display", "tag", and "xml-roles" have somewhat unusual names for
  // historical reasons. Aside from that virtually every ARIA attribute
  // is exposed in a really straightforward way, i.e. "aria-foo" is exposed
  // as "foo".
  AddAttributeToList(ax::mojom::StringAttribute::kDisplay, "display",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kHtmlTag, "tag", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kRole, "xml-roles",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kPlaceholder, "placeholder",
                     attributes);

  AddAttributeToList(ax::mojom::StringAttribute::kAutoComplete, "autocomplete",
                     attributes);
  if (!HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete) &&
      IsFocusedInputWithSuggestions()) {
    // TODO(crbug.com/865101) Use
    // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
    // IsFocusedInputWithSuggestions()
    AddAttributeToList("autocomplete", "list", attributes);
  }

  AddAttributeToList(ax::mojom::StringAttribute::kRoleDescription,
                     "roledescription", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kKeyShortcuts, "keyshortcuts",
                     attributes);

  AddAttributeToList(ax::mojom::IntAttribute::kHierarchicalLevel, "level",
                     attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kSetSize, "setsize", attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kPosInSet, "posinset",
                     attributes);

  if (HasIntAttribute(ax::mojom::IntAttribute::kCheckedState))
    AddAttributeToList("checkable", "true", attributes);

  // Expose live region attributes.
  AddAttributeToList(ax::mojom::StringAttribute::kLiveStatus, "live",
                     attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kLiveRelevant, "relevant",
                     attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kLiveAtomic, "atomic",
                     attributes);
  // Busy is usually associated with live regions but can occur anywhere:
  AddAttributeToList(ax::mojom::BoolAttribute::kBusy, "busy", attributes);

  // Expose container live region attributes.
  AddAttributeToList(ax::mojom::StringAttribute::kContainerLiveStatus,
                     "container-live", attributes);
  AddAttributeToList(ax::mojom::StringAttribute::kContainerLiveRelevant,
                     "container-relevant", attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                     "container-atomic", attributes);
  AddAttributeToList(ax::mojom::BoolAttribute::kContainerLiveBusy,
                     "container-busy", attributes);

  // Expose the non-standard explicit-name IA2 attribute.
  int name_from;
  if (GetIntAttribute(ax::mojom::IntAttribute::kNameFrom, &name_from) &&
      name_from != static_cast<int32_t>(ax::mojom::NameFrom::kContents)) {
    AddAttributeToList("explicit-name", "true", attributes);
  }

  // Expose the aria-haspopup attribute.
  int32_t has_popup;
  if (GetIntAttribute(ax::mojom::IntAttribute::kHasPopup, &has_popup)) {
    switch (static_cast<ax::mojom::HasPopup>(has_popup)) {
      case ax::mojom::HasPopup::kFalse:
        break;
      case ax::mojom::HasPopup::kTrue:
        AddAttributeToList("haspopup", "true", attributes);
        break;
      case ax::mojom::HasPopup::kMenu:
        AddAttributeToList("haspopup", "menu", attributes);
        break;
      case ax::mojom::HasPopup::kListbox:
        AddAttributeToList("haspopup", "listbox", attributes);
        break;
      case ax::mojom::HasPopup::kTree:
        AddAttributeToList("haspopup", "tree", attributes);
        break;
      case ax::mojom::HasPopup::kGrid:
        AddAttributeToList("haspopup", "grid", attributes);
        break;
        break;
      case ax::mojom::HasPopup::kDialog:
        AddAttributeToList("haspopup", "dialog", attributes);
        break;
    }
  } else if (IsFocusedInputWithSuggestions()) {
    // TODO(crbug.com/865101) Use
    // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
    // IsFocusedInputWithSuggestions()
    // TODO(crbug.com/865101) Remove this comment:
    // Note: suggestions are special-cased here because there is no way
    // for the browser to know when a suggestion popup is available.
    AddAttributeToList("haspopup", "menu", attributes);
  }

  // Expose the aria-current attribute.
  int32_t aria_current_state;
  if (GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                      &aria_current_state)) {
    switch (static_cast<ax::mojom::AriaCurrentState>(aria_current_state)) {
      case ax::mojom::AriaCurrentState::kNone:
        break;
      case ax::mojom::AriaCurrentState::kFalse:
        AddAttributeToList("current", "false", attributes);
        break;
      case ax::mojom::AriaCurrentState::kTrue:
        AddAttributeToList("current", "true", attributes);
        break;
      case ax::mojom::AriaCurrentState::kPage:
        AddAttributeToList("current", "page", attributes);
        break;
      case ax::mojom::AriaCurrentState::kStep:
        AddAttributeToList("current", "step", attributes);
        break;
      case ax::mojom::AriaCurrentState::kLocation:
        AddAttributeToList("current", "location", attributes);
        break;
      case ax::mojom::AriaCurrentState::kUnclippedLocation:
        AddAttributeToList("current", "unclippedLocation", attributes);
        break;
      case ax::mojom::AriaCurrentState::kDate:
        AddAttributeToList("current", "date", attributes);
        break;
      case ax::mojom::AriaCurrentState::kTime:
        AddAttributeToList("current", "time", attributes);
        break;
    }
  }

  // Expose table cell index.
  if (IsCellOrTableHeader(GetData().role)) {
    int32_t index = delegate_->GetTableCellIndex();
    if (index >= 0) {
      std::string str_index(base::IntToString(index));
      AddAttributeToList("table-cell-index", str_index, attributes);
    }
  }
  if (GetData().role == ax::mojom::Role::kLayoutTable)
    AddAttributeToList("layout-guess", "true", attributes);

  // Expose aria-colcount and aria-rowcount in a table, grid or treegrid.
  if (IsTableLike(GetData().role)) {
    AddAttributeToList(ax::mojom::IntAttribute::kAriaColumnCount, "colcount",
                       attributes);
    AddAttributeToList(ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                       attributes);
  }

  // Expose aria-colindex and aria-rowindex in a cell or row.
  if (IsCellOrTableHeader(GetData().role) ||
      GetData().role == ax::mojom::Role::kRow) {
    if (GetData().role != ax::mojom::Role::kRow)
      AddAttributeToList(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                         "colindex", attributes);
    AddAttributeToList(ax::mojom::IntAttribute::kAriaCellRowIndex, "rowindex",
                       attributes);

    // Experimental: expose aria-rowtext / aria-coltext. Not standardized
    // yet, but obscure enough that it's safe to expose.
    // http://crbug.com/791634
    for (size_t i = 0; i < GetData().html_attributes.size(); ++i) {
      const std::string& attr = GetData().html_attributes[i].first;
      const std::string& value = GetData().html_attributes[i].second;
      if (attr == "aria-coltext") {
        AddAttributeToList("coltext", value, attributes);
      }
      if (attr == "aria-rowtext") {
        AddAttributeToList("rowtext", value, attributes);
      }
    }
  }

  // Expose row or column header sort direction.
  int32_t sort_direction;
  if ((GetData().role == ax::mojom::Role::kColumnHeader ||
       GetData().role == ax::mojom::Role::kRowHeader) &&
      GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                      &sort_direction)) {
    switch (static_cast<ax::mojom::SortDirection>(sort_direction)) {
      case ax::mojom::SortDirection::kNone:
        break;
      case ax::mojom::SortDirection::kUnsorted:
        AddAttributeToList("sort", "none", attributes);
        break;
      case ax::mojom::SortDirection::kAscending:
        AddAttributeToList("sort", "ascending", attributes);
        break;
      case ax::mojom::SortDirection::kDescending:
        AddAttributeToList("sort", "descending", attributes);
        break;
      case ax::mojom::SortDirection::kOther:
        AddAttributeToList("sort", "other", attributes);
        break;
    }
  }

  if (IsCellOrTableHeader(GetData().role)) {
    // Expose colspan attribute.
    std::string colspan;
    if (GetData().GetHtmlAttribute("aria-colspan", &colspan)) {
      AddAttributeToList("colspan", colspan, attributes);
    }
    // Expose rowspan attribute.
    std::string rowspan;
    if (GetData().GetHtmlAttribute("aria-rowspan", &rowspan)) {
      AddAttributeToList("rowspan", rowspan, attributes);
    }
  }

  // Expose slider value.
  if (IsRangeValueSupported()) {
    std::string value = base::UTF16ToUTF8(GetRangeValueText());
    if (!value.empty())
      AddAttributeToList("valuetext", value, attributes);
  }

  // Expose dropeffect attribute.
  std::string drop_effect;
  if (GetData().GetHtmlAttribute("aria-dropeffect", &drop_effect)) {
    AddAttributeToList("dropeffect", drop_effect, attributes);
  }

  // Expose grabbed attribute.
  std::string grabbed;
  if (GetData().GetHtmlAttribute("aria-grabbed", &grabbed)) {
    AddAttributeToList("grabbed", grabbed, attributes);
  }

  // Expose class attribute.
  std::string class_attr;
  if (GetData().GetHtmlAttribute("class", &class_attr) ||
      GetData().GetStringAttribute(ax::mojom::StringAttribute::kClassName,
                                   &class_attr)) {
    AddAttributeToList("class", class_attr, attributes);
  }

  // Expose datetime attribute.
  std::string datetime;
  if (GetData().role == ax::mojom::Role::kTime &&
      GetData().GetHtmlAttribute("datetime", &datetime)) {
    AddAttributeToList("datetime", datetime, attributes);
  }

  // Expose id attribute.
  std::string id;
  if (GetData().GetHtmlAttribute("id", &id)) {
    AddAttributeToList("id", id, attributes);
  }

  // Expose src attribute.
  std::string src;
  if (GetData().role == ax::mojom::Role::kImage &&
      GetData().GetHtmlAttribute("src", &src)) {
    AddAttributeToList("src", src, attributes);
  }

  // Text fields need to report the attribute "text-model:a1" to instruct
  // screen readers to use IAccessible2 APIs to handle text editing in this
  // object (as opposed to treating it like a native Windows text box).
  // The text-model:a1 attribute is documented here:
  // http://www.linuxfoundation.org/collaborate/workgroups/accessibility/ia2/ia2_implementation_guide
  if (IsPlainTextField() || IsRichTextField())
    AddAttributeToList("text-model", "a1", attributes);

  // Expose input-text type attribute.
  std::string type;
  std::string html_tag =
      GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  if (IsPlainTextField() && base::LowerCaseEqualsASCII(html_tag, "input") &&
      GetData().GetHtmlAttribute("type", &type)) {
    AddAttributeToList("text-input-type", type, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::StringAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);
  std::string value;
  if (GetStringAttribute(attribute, &value)) {
    AddAttributeToList(name, value, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::BoolAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    AddAttributeToList(name, value ? "true" : "false", attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(
    const ax::mojom::IntAttribute attribute,
    const char* name,
    PlatformAttributeList* attributes) {
  DCHECK(attributes);
  int value;
  if (GetIntAttribute(attribute, &value)) {
    std::string str_value = base::IntToString(value);
    AddAttributeToList(name, str_value, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(const char* name,
                                            const std::string& value,
                                            PlatformAttributeList* attributes) {
  AddAttributeToList(name, value.c_str(), attributes);
}

AXHypertext::AXHypertext() {}
AXHypertext::AXHypertext(const AXHypertext& other) = default;
AXHypertext::~AXHypertext() {}

AXHypertext AXPlatformNodeBase::ComputeHypertext() {
  AXHypertext result;

  if (IsPlainTextField()) {
    result.hypertext = GetValue();
    return result;
  }

  int child_count = delegate_->GetChildCount();

  if (!child_count) {
    if (IsRichTextField()) {
      // We don't want to expose any associated label in IA2 Hypertext.
      return result;
    }
    result.hypertext = GetString16Attribute(ax::mojom::StringAttribute::kName);
    return result;
  }

  // Construct the hypertext for this node, which contains the concatenation
  // of all of the static text and widespace of this node's children and an
  // embedded object character for all the other children. Build up a map from
  // the character index of each embedded object character to the id of the
  // child object it points to.
  base::string16 hypertext;
  for (int i = 0; i < child_count; ++i) {
    auto* child = FromNativeViewAccessible(delegate_->ChildAtIndex(i));

    DCHECK(child);
    // Similar to Firefox, we don't expose text-only objects in IA2 hypertext.
    if (child->IsTextOnlyObject()) {
      hypertext +=
          child->GetString16Attribute(ax::mojom::StringAttribute::kName);
    } else {
      int32_t char_offset = static_cast<int32_t>(hypertext.size());
      int32_t child_unique_id = child->GetUniqueId();
      int32_t index = static_cast<int32_t>(result.hyperlinks.size());
      result.hyperlink_offset_to_index[char_offset] = index;
      result.hyperlinks.push_back(child_unique_id);
      hypertext += kEmbeddedCharacter;
    }
  }
  result.hypertext = hypertext;
  return result;
}

// static
void AXPlatformNodeBase::SanitizeStringAttribute(const std::string& input,
                                                 std::string* output) {
  DCHECK(output);
  // According to the IA2 spec and AT-SPI2, these characters need to be escaped
  // with a backslash: backslash, colon, comma, equals and semicolon.  Note
  // that backslash must be replaced first.
  base::ReplaceChars(input, "\\", "\\\\", output);
  base::ReplaceChars(*output, ":", "\\:", output);
  base::ReplaceChars(*output, ",", "\\,", output);
  base::ReplaceChars(*output, "=", "\\=", output);
  base::ReplaceChars(*output, ";", "\\;", output);
}

}  // namespace ui
