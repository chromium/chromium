// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/compute_attributes.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

const base::char16 AXPlatformNodeBase::kEmbeddedCharacter = L'\xfffc';

// Map from each AXPlatformNode's unique id to its instance.
using UniqueIdMap = std::unordered_map<int32_t, AXPlatformNode*>;
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase* node = new AXPlatformNodeBase();
  node->Init(delegate);
  return node;
}
#endif

// static
AXPlatformNode* AXPlatformNodeBase::GetFromUniqueId(int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
}

// static
size_t AXPlatformNodeBase::GetInstanceCountForTesting() {
  return g_unique_id_map.Get().size();
}

AXPlatformNodeBase::AXPlatformNodeBase() = default;

AXPlatformNodeBase::~AXPlatformNodeBase() = default;

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;

  // This must be called after assigning our delegate.
  g_unique_id_map.Get()[GetUniqueId()] = this;
}

const AXNodeData& AXPlatformNodeBase::GetData() const {
  static const base::NoDestructor<AXNodeData> empty_data;
  if (delegate_)
    return delegate_->GetData();
  return *empty_data;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetFocus() {
  if (delegate_)
    return delegate_->GetFocus();
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() const {
  if (delegate_)
    return delegate_->GetParent();
  return nullptr;
}

int AXPlatformNodeBase::GetChildCount() const {
  if (delegate_)
    return delegate_->GetChildCount();
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  if (delegate_)
    return delegate_->ChildAtIndex(index);
  return nullptr;
}

int AXPlatformNodeBase::GetIndexInParent() {
  if (delegate_)
    return delegate_->GetIndexInParent();
  return -1;
}

// AXPlatformNode overrides.

void AXPlatformNodeBase::Destroy() {
  g_unique_id_map.Get().erase(GetUniqueId());

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

void AXPlatformNodeBase::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
}

#if defined(OS_MACOSX)
void AXPlatformNodeBase::AnnounceText(const base::string16& text) {}
#endif

AXPlatformNodeDelegate* AXPlatformNodeBase::GetDelegate() const {
  return delegate_;
}

bool AXPlatformNodeBase::IsDescendantOf(AXPlatformNode* ancestor) const {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return false;

  return parent->IsDescendantOf(ancestor);
}

// Helpers.

AXPlatformNodeBase* AXPlatformNodeBase::GetPreviousSibling() {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetPreviousSibling());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetNextSibling() {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetNextSibling());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetFirstChild() {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetFirstChild());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetLastChild() {
  if (!delegate_)
    return nullptr;
  return FromNativeViewAccessible(delegate_->GetLastChild());
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
    return 0;
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

bool AXPlatformNodeBase::HasInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return false;
    }

    if (current_node->GetData().HasStringAttribute(attribute)) {
      return true;
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return false;
}

const std::string& AXPlatformNodeBase::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return base::EmptyString();
    }

    if (current_node->GetData().HasStringAttribute(attribute)) {
      return current_node->GetData().GetStringAttribute(attribute);
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return base::EmptyString();
}

base::string16 AXPlatformNodeBase::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

bool AXPlatformNodeBase::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  const AXPlatformNodeBase* current_node = this;

  do {
    if (!current_node->delegate_) {
      return false;
    }

    if (current_node->GetData().GetStringAttribute(attribute, value)) {
      return true;
    }

    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return false;
}

bool AXPlatformNodeBase::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute,
    base::string16* value) const {
  std::string value_utf8;
  if (!GetInheritedStringAttribute(attribute, &value_utf8))
    return false;
  *value = base::UTF8ToUTF16(value_utf8);
  return true;
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

// static
AXPlatformNodeBase* AXPlatformNodeBase::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return static_cast<AXPlatformNodeBase*>(
      AXPlatformNode::FromNativeViewAccessible(accessible));
}

bool AXPlatformNodeBase::SetHypertextSelection(int start_offset,
                                               int end_offset) {
  if (!delegate_)
    return false;
  return delegate_->SetHypertextSelection(start_offset, end_offset);
}

bool AXPlatformNodeBase::IsDocument() const {
  return ui::IsDocument(GetData().role);
}

bool AXPlatformNodeBase::IsTextOnlyObject() const {
  return GetData().role == ax::mojom::Role::kStaticText ||
         GetData().role == ax::mojom::Role::kLineBreak ||
         GetData().role == ax::mojom::Role::kInlineTextBox;
}

bool AXPlatformNodeBase::IsPlainTextField() const {
  // We need to check both the role and editable state, because some ARIA text
  // fields may in fact not be editable, whilst some editable fields might not
  // have the role.
  return ui::IsPlainTextField(GetData());
}

bool AXPlatformNodeBase::IsRichTextField() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot) &&
         GetData().HasState(ax::mojom::State::kRichlyEditable);
}

base::string16 AXPlatformNodeBase::GetHypertext() const {
  return base::string16();
}

base::string16 AXPlatformNodeBase::GetInnerText() const {
  if (IsTextOnlyObject())
    return GetString16Attribute(ax::mojom::StringAttribute::kName);

  base::string16 text;
  for (int i = 0; i < GetChildCount(); ++i) {
    gfx::NativeViewAccessible child_accessible =
        const_cast<AXPlatformNodeBase*>(this)->ChildAtIndex(i);
    AXPlatformNodeBase* child = FromNativeViewAccessible(child_accessible);
    if (!child)
      continue;

    text += child->GetInnerText();
  }
  return text;
}

bool AXPlatformNodeBase::IsSelectionItemSupported() const {
  switch (GetData().role) {
    // An ARIA 1.1+ role of "cell", or a role of "row" inside
    // an ARIA 1.1 role of "table", should not be selectable.
    // ARIA "table" is not interactable, ARIA "grid" is.
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kRowHeader: {
      AXPlatformNodeBase* table = GetTable();
      if (!table)
        return false;

      return table->GetData().role == ax::mojom::Role::kGrid ||
             table->GetData().role == ax::mojom::Role::kTreeGrid;
    }
    // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
    // SelectionItem.IsSelected is exposed when aria-checked is True or False,
    // for 'radio' and 'menuitemradio' roles.
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kMenuItemRadio: {
      if (GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue ||
          GetData().GetCheckedState() == ax::mojom::CheckedState::kFalse)
        return true;
      return false;
    }
    // https://www.w3.org/TR/wai-aria-1.1/#aria-selected
    // SelectionItem.IsSelected is exposed when aria-select is True or False.
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTreeItem:
      return HasBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    default:
      return false;
  }
}

base::string16 AXPlatformNodeBase::GetRangeValueText() const {
  float fval;
  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);

  if (value.empty() &&
      GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &fval)) {
    value = base::NumberToString16(fval);
  }
  return value;
}

base::string16
AXPlatformNodeBase::GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute()
    const {
  if (GetData().role == ax::mojom::Role::kImage &&
      (GetData().GetImageAnnotationStatus() ==
           ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation ||
       GetData().GetImageAnnotationStatus() ==
           ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation)) {
    return GetDelegate()->GetLocalizedRoleDescriptionForUnlabeledImage();
  }

  return GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription);
}

base::string16 AXPlatformNodeBase::GetRoleDescription() const {
  base::string16 role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();

  if (!role_description.empty()) {
    return role_description;
  }

  return GetDelegate()->GetLocalizedStringForRoleDescription();
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

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCaption() const {
  if (!delegate_)
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;

  DCHECK(table->delegate_);
  return static_cast<AXPlatformNodeBase*>(table->delegate_->GetTableCaption());
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int index) const {
  if (!delegate_)
    return nullptr;
  if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return nullptr;

  DCHECK(table->delegate_);
  base::Optional<int32_t> cell_id = table->delegate_->CellIndexToId(index);
  if (!cell_id)
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(*cell_id));
}

AXPlatformNodeBase* AXPlatformNodeBase::GetTableCell(int row,
                                                     int column) const {
  if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
    return nullptr;

  AXPlatformNodeBase* table = GetTable();
  if (!table || !GetTableRowCount() || !GetTableColumnCount())
    return nullptr;

  if (row < 0 || row >= *GetTableRowCount() || column < 0 ||
      column >= *GetTableColumnCount()) {
    return nullptr;
  }

  DCHECK(table->delegate_);
  base::Optional<int32_t> cell_id = table->delegate_->GetCellId(row, column);
  if (!cell_id)
    return nullptr;

  return static_cast<AXPlatformNodeBase*>(
      table->delegate_->GetFromNodeID(*cell_id));
}

base::Optional<int> AXPlatformNodeBase::GetTableCellIndex() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetTableCellIndex();
}

base::Optional<int> AXPlatformNodeBase::GetTableColumn() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetTableCellColIndex();
}

base::Optional<int> AXPlatformNodeBase::GetTableColumnCount() const {
  if (!delegate_)
    return base::nullopt;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return base::nullopt;

  DCHECK(table->delegate_);
  return table->delegate_->GetTableColCount();
}

base::Optional<int> AXPlatformNodeBase::GetTableAriaColumnCount() const {
  if (!delegate_)
    return base::nullopt;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return base::nullopt;

  DCHECK(table->delegate_);
  return table->delegate_->GetTableAriaColCount();
}

base::Optional<int> AXPlatformNodeBase::GetTableColumnSpan() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetTableCellColSpan();
}

base::Optional<int> AXPlatformNodeBase::GetTableRow() const {
  if (!delegate_)
    return base::nullopt;
  if (delegate_->IsTableRow())
    return delegate_->GetTableRowRowIndex();
  if (delegate_->IsTableCellOrHeader())
    return delegate_->GetTableCellRowIndex();
  return base::nullopt;
}

base::Optional<int> AXPlatformNodeBase::GetTableRowCount() const {
  if (!delegate_)
    return base::nullopt;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return base::nullopt;

  DCHECK(table->delegate_);
  return table->delegate_->GetTableRowCount();
}

base::Optional<int> AXPlatformNodeBase::GetTableAriaRowCount() const {
  if (!delegate_)
    return base::nullopt;

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return base::nullopt;

  DCHECK(table->delegate_);
  return table->delegate_->GetTableAriaRowCount();
}

base::Optional<int> AXPlatformNodeBase::GetTableRowSpan() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetTableCellRowSpan();
}

bool AXPlatformNodeBase::HasCaret() {
  if (IsInvisibleOrIgnored())
    return false;

  if (IsPlainTextField() &&
      HasIntAttribute(ax::mojom::IntAttribute::kTextSelStart) &&
      HasIntAttribute(ax::mojom::IntAttribute::kTextSelEnd)) {
    return true;
  }

  // The caret is always at the focus of the selection.
  int32_t focus_id = delegate_->GetUnignoredSelection().focus_object_id;
  AXPlatformNodeBase* focus_object =
      static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(focus_id));

  if (!focus_object)
    return false;

  return focus_object->IsDescendantOf(this);
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

bool AXPlatformNodeBase::IsChildOfLeaf() const {
  AXPlatformNodeBase* ancestor = FromNativeViewAccessible(GetParent());

  while (ancestor) {
    if (ancestor->IsLeaf())
      return true;
    ancestor = FromNativeViewAccessible(ancestor->GetParent());
  }

  return false;
}

bool AXPlatformNodeBase::IsInvisibleOrIgnored() const {
  const AXNodeData& data = GetData();
  return data.HasState(ax::mojom::State::kInvisible) || ui::IsIgnored(data);
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

base::string16 AXPlatformNodeBase::GetValue() const {
  // Expose slider value.
  if (IsRangeValueSupported(GetData()))
    return GetRangeValueText();

  // On Windows, the value of a document should be its URL.
  if (ui::IsDocument(GetData().role))
    return base::UTF8ToUTF16(delegate_->GetTreeData().url);

  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);

  // Some screen readers like Jaws and VoiceOver require a
  // value to be set in text fields with rich content, even though the same
  // information is available on the children.
  if (value.empty() && IsRichTextField())
    return GetInnerText();

  return value;
}

void AXPlatformNodeBase::ComputeAttributes(PlatformAttributeList* attributes) {
  DCHECK(delegate_) << "Many attributes need to be retrieved from our "
                       "AXPlatformNodeDelegate.";
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
      GetData().HasState(ax::mojom::State::kAutofillAvailable)) {
    AddAttributeToList("autocomplete", "list", attributes);
  }

  base::string16 role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();
  if (!role_description.empty() ||
      HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription)) {
    AddAttributeToList("roledescription", base::UTF16ToUTF8(role_description),
                       attributes);
  }

  AddAttributeToList(ax::mojom::StringAttribute::kKeyShortcuts, "keyshortcuts",
                     attributes);

  AddAttributeToList(ax::mojom::IntAttribute::kHierarchicalLevel, "level",
                     attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kSetSize, "setsize", attributes);
  AddAttributeToList(ax::mojom::IntAttribute::kPosInSet, "posinset",
                     attributes);

  if (HasIntAttribute(ax::mojom::IntAttribute::kCheckedState))
    AddAttributeToList("checkable", "true", attributes);

  if (IsInvisibleOrIgnored())  // Note: NVDA prefers this over INVISIBLE state.
    AddAttributeToList("hidden", "true", attributes);

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
      case ax::mojom::HasPopup::kDialog:
        AddAttributeToList("haspopup", "dialog", attributes);
        break;
    }
  } else if (GetData().HasState(ax::mojom::State::kAutofillAvailable)) {
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
    base::Optional<int> index = delegate_->GetTableCellIndex();
    if (index) {
      std::string str_index(base::NumberToString(*index));
      AddAttributeToList("table-cell-index", str_index, attributes);
    }
  }
  if (GetData().role == ax::mojom::Role::kLayoutTable)
    AddAttributeToList("layout-guess", "true", attributes);

  // Expose aria-colcount and aria-rowcount in a table, grid or treegrid if they
  // are different from its physical dimensions.
  if (IsTableLike(GetData().role) &&
      (delegate_->GetTableAriaRowCount() != delegate_->GetTableRowCount() ||
       delegate_->GetTableAriaColCount() != delegate_->GetTableColCount())) {
    AddAttributeToList(ax::mojom::IntAttribute::kAriaColumnCount, "colcount",
                       attributes);
    AddAttributeToList(ax::mojom::IntAttribute::kAriaRowCount, "rowcount",
                       attributes);
  }

  if (IsCellOrTableHeader(GetData().role) || IsTableRow(GetData().role)) {
    // Expose aria-colindex and aria-rowindex in a cell or row only if they are
    // different from the table's physical coordinates.
    if (delegate_->GetTableCellAriaRowIndex() !=
            delegate_->GetTableCellRowIndex() ||
        delegate_->GetTableCellAriaColIndex() !=
            delegate_->GetTableCellColIndex()) {
      if (!IsTableRow(GetData().role)) {
        AddAttributeToList(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                           "colindex", attributes);
      }
      AddAttributeToList(ax::mojom::IntAttribute::kAriaCellRowIndex, "rowindex",
                         attributes);
    }

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
  if (IsTableHeader(GetData().role) &&
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
  if (IsRangeValueSupported(GetData())) {
    std::string value = base::UTF16ToUTF8(GetRangeValueText());
    if (!value.empty())
      AddAttributeToList("valuetext", value, attributes);
  }

  // Expose dropeffect attribute.
  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  if (GetData().HasIntAttribute(ax::mojom::IntAttribute::kDropeffect)) {
    std::string dropeffect = GetData().DropeffectBitfieldToString();
    AddAttributeToList("dropeffect", dropeffect, attributes);
  }

  // Expose grabbed attribute.
  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  AddAttributeToList(ax::mojom::BoolAttribute::kGrabbed, "grabbed", attributes);

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

  auto maybe_value = ComputeAttribute(delegate_, attribute);
  if (maybe_value.has_value()) {
    std::string str_value = base::NumberToString(maybe_value.value());
    AddAttributeToList(name, str_value, attributes);
  }
}

void AXPlatformNodeBase::AddAttributeToList(const char* name,
                                            const std::string& value,
                                            PlatformAttributeList* attributes) {
  AddAttributeToList(name, value.c_str(), attributes);
}

AXHypertext::AXHypertext() = default;
AXHypertext::AXHypertext(const AXHypertext& other) = default;
AXHypertext::~AXHypertext() = default;

void AXPlatformNodeBase::UpdateComputedHypertext() {
  hypertext_ = AXHypertext();

  if (IsPlainTextField()) {
    hypertext_.hypertext = GetValue();
    return;
  }

  if (!GetChildCount()) {
    if (IsRichTextField()) {
      // We don't want to expose any associated label in IA2 Hypertext.
      return;
    }
    hypertext_.hypertext =
        GetString16Attribute(ax::mojom::StringAttribute::kName);
    return;
  }

  // Construct the hypertext for this node, which contains the concatenation
  // of all of the static text and widespace of this node's children and an
  // embedded object character for all the other children. Build up a map from
  // the character index of each embedded object character to the id of the
  // child object it points to.
  base::string16 hypertext;
  for (AXPlatformNodeBase* child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    // Similar to Firefox, we don't expose text-only objects in IA2 hypertext.
    if (child->IsTextOnlyObject()) {
      hypertext_.hypertext +=
          child->GetString16Attribute(ax::mojom::StringAttribute::kName);
    } else {
      int32_t char_offset = static_cast<int32_t>(hypertext_.hypertext.size());
      int32_t child_unique_id = child->GetUniqueId();
      int32_t index = static_cast<int32_t>(hypertext_.hyperlinks.size());
      hypertext_.hyperlink_offset_to_index[char_offset] = index;
      hypertext_.hyperlinks.push_back(child_unique_id);
      hypertext_.hypertext += kEmbeddedCharacter;
    }
  }
}

void AXPlatformNodeBase::AddAttributeToList(const char* name,
                                            const char* value,
                                            PlatformAttributeList* attributes) {
}

base::Optional<int> AXPlatformNodeBase::GetPosInSet() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetPosInSet();
}

base::Optional<int> AXPlatformNodeBase::GetSetSize() const {
  if (!delegate_)
    return base::nullopt;
  return delegate_->GetSetSize();
}

bool AXPlatformNodeBase::ScrollToNode(ScrollType scroll_type) {
  // ax::mojom::Action::kScrollToMakeVisible wants a target rect in *local*
  // coords.
  gfx::Rect r = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  r -= r.OffsetFromOrigin();
  switch (scroll_type) {
    case ScrollType::TopLeft:
      r = gfx::Rect(r.x(), r.y(), 0, 0);
      break;
    case ScrollType::BottomRight:
      r = gfx::Rect(r.right(), r.bottom(), 0, 0);
      break;
    case ScrollType::TopEdge:
      r = gfx::Rect(r.x(), r.y(), r.width(), 0);
      break;
    case ScrollType::BottomEdge:
      r = gfx::Rect(r.x(), r.bottom(), r.width(), 0);
      break;
    case ScrollType::LeftEdge:
      r = gfx::Rect(r.x(), r.y(), 0, r.height());
      break;
    case ScrollType::RightEdge:
      r = gfx::Rect(r.right(), r.y(), 0, r.height());
      break;
    case ScrollType::Anywhere:
      break;
  }

  ui::AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.horizontal_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.vertical_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.target_rect = r;
  GetDelegate()->AccessibilityPerformAction(action_data);
  return true;
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

AXPlatformNodeBase* AXPlatformNodeBase::GetHyperlinkFromHypertextOffset(
    int offset) {
  std::map<int32_t, int32_t>::iterator iterator =
      hypertext_.hyperlink_offset_to_index.find(offset);
  if (iterator == hypertext_.hyperlink_offset_to_index.end())
    return nullptr;

  int32_t index = iterator->second;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int32_t>(hypertext_.hyperlinks.size()));
  int32_t id = hypertext_.hyperlinks[index];
  auto* hyperlink =
      static_cast<AXPlatformNodeBase*>(AXPlatformNodeBase::GetFromUniqueId(id));
  if (!hyperlink)
    return nullptr;
  return hyperlink;
}

int32_t AXPlatformNodeBase::GetHyperlinkIndexFromChild(
    AXPlatformNodeBase* child) {
  if (hypertext_.hyperlinks.empty())
    return -1;

  auto iterator = std::find(hypertext_.hyperlinks.begin(),
                            hypertext_.hyperlinks.end(), child->GetUniqueId());
  if (iterator == hypertext_.hyperlinks.end())
    return -1;

  return static_cast<int32_t>(iterator - hypertext_.hyperlinks.begin());
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromHyperlinkIndex(
    int32_t hyperlink_index) {
  for (auto& offset_index : hypertext_.hyperlink_offset_to_index) {
    if (offset_index.second == hyperlink_index)
      return offset_index.first;
  }
  return -1;
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromChild(
    AXPlatformNodeBase* child) {
  // TODO(dougt) DCHECK(child.owner()->PlatformGetParent() == owner());

  if (IsLeaf())
    return -1;

  // Handle the case when we are dealing with a text-only child.
  // Text-only children should not be present at tree roots and so no
  // cross-tree traversal is necessary.
  if (child->IsTextOnlyObject()) {
    int32_t hypertext_offset = 0;
    int32_t index_in_parent = child->GetIndexInParent();
    DCHECK_GE(index_in_parent, 0);
    DCHECK_LT(index_in_parent, static_cast<int32_t>(GetChildCount()));
    for (uint32_t i = 0; i < static_cast<uint32_t>(index_in_parent); ++i) {
      auto* sibling = static_cast<AXPlatformNodeBase*>(
          FromNativeViewAccessible(ChildAtIndex(i)));
      DCHECK(sibling);
      if (sibling->IsTextOnlyObject()) {
        hypertext_offset += (int32_t)sibling->GetHypertext().size();
      } else {
        ++hypertext_offset;
      }
    }
    return hypertext_offset;
  }

  int32_t hyperlink_index = GetHyperlinkIndexFromChild(child);
  if (hyperlink_index < 0)
    return -1;

  return GetHypertextOffsetFromHyperlinkIndex(hyperlink_index);
}

int32_t AXPlatformNodeBase::GetHypertextOffsetFromDescendant(
    AXPlatformNodeBase* descendant) {
  auto* parent_object = static_cast<AXPlatformNodeBase*>(
      FromNativeViewAccessible(descendant->GetDelegate()->GetParent()));
  while (parent_object && parent_object != this) {
    descendant = parent_object;
    parent_object = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(descendant->GetParent()));
  }
  if (!parent_object)
    return -1;

  return parent_object->GetHypertextOffsetFromChild(descendant);
}

int AXPlatformNodeBase::GetHypertextOffsetFromEndpoint(
    AXPlatformNodeBase* endpoint_object,
    int endpoint_offset) {
  // There are three cases:
  // 1. The selection endpoint is inside this object but not one of its
  // descendants, or is in an ancestor of this object. endpoint_offset should be
  // returned, possibly adjusted from a child offset to a hypertext offset.
  // 2. The selection endpoint is a descendant of this object. The offset of the
  // character in this object's hypertext corresponding to the subtree in which
  // the endpoint is located should be returned.
  // 3. The selection endpoint is in a completely different part of the tree.
  // Either 0 or hypertext length should be returned depending on the direction
  // that one needs to travel to find the endpoint.
  //
  // TODO(nektar): Replace all this logic with the use of AXNodePosition.

  // Case 1. Is the endpoint object equal to this object or an ancestor of this
  // object?
  //
  // IsDescendantOf includes the case when endpoint_object == this.
  if (IsDescendantOf(endpoint_object)) {
    if (endpoint_object->IsLeaf()) {
      DCHECK_EQ(endpoint_object, this) << "Text objects cannot have children.";
      return endpoint_offset;
    } else {
      DCHECK_GE(endpoint_offset, 0);
      DCHECK_LE(endpoint_offset,
                endpoint_object->GetDelegate()->GetChildCount());

      // Adjust the |endpoint_offset| because the selection endpoint is a tree
      // position, i.e. it represents a child index and not a text offset.
      if (endpoint_offset >= endpoint_object->GetChildCount()) {
        return static_cast<int>(endpoint_object->GetHypertext().size());
      } else {
        auto* child = static_cast<AXPlatformNodeBase*>(FromNativeViewAccessible(
            endpoint_object->ChildAtIndex(endpoint_offset)));
        DCHECK(child);
        return endpoint_object->GetHypertextOffsetFromChild(child);
      }
    }
  }

  AXPlatformNodeBase* common_parent = this;
  int32_t index_in_common_parent = GetIndexInParent();
  while (common_parent && !endpoint_object->IsDescendantOf(common_parent)) {
    index_in_common_parent = common_parent->GetIndexInParent();
    common_parent = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(common_parent->GetParent()));
  }
  if (!common_parent)
    return -1;

  DCHECK_GE(index_in_common_parent, 0);
  DCHECK(!(common_parent->IsTextOnlyObject()));

  // Case 2. Is the selection endpoint inside a descendant of this object?
  //
  // We already checked in case 1 if our endpoint object is equal to this
  // object. We can safely assume that it is a descendant or in a completely
  // different part of the tree.
  if (common_parent == this) {
    int32_t hypertext_offset =
        GetHypertextOffsetFromDescendant(endpoint_object);
    auto* parent = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(endpoint_object->GetParent()));
    if (parent == this && endpoint_object->IsTextOnlyObject()) {
      // Due to a historical design decision, the hypertext of the immediate
      // parents of text objects includes all their text. We therefore need to
      // adjust the hypertext offset in the parent by adding any text offset.
      hypertext_offset += endpoint_offset;
    }

    return hypertext_offset;
  }

  // Case 3. Is the selection endpoint in a completely different part of the
  // tree?
  //
  // We can safely assume that the endpoint is in another part of the tree or
  // at common parent, and that this object is a descendant of common parent.
  int32_t endpoint_index_in_common_parent = -1;
  for (int i = 0; i < common_parent->GetDelegate()->GetChildCount(); ++i) {
    auto* child = static_cast<AXPlatformNodeBase*>(FromNativeViewAccessible(
        common_parent->GetDelegate()->ChildAtIndex(i)));
    DCHECK(child);
    if (endpoint_object->IsDescendantOf(child)) {
      endpoint_index_in_common_parent = child->GetIndexInParent();
      break;
    }
  }
  DCHECK_GE(endpoint_index_in_common_parent, 0);

  if (endpoint_index_in_common_parent < index_in_common_parent)
    return 0;
  if (endpoint_index_in_common_parent > index_in_common_parent)
    return static_cast<int32_t>(GetHypertext().size());

  NOTREACHED();
  return -1;
}

int AXPlatformNodeBase::GetUnignoredSelectionAnchor() {
  ui::AXTree::Selection unignored_selection =
      delegate_->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  AXPlatformNodeBase* anchor_object =
      static_cast<AXPlatformNodeBase*>(delegate_->GetFromNodeID(anchor_id));

  if (!anchor_object)
    return -1;

  int anchor_offset = int{unignored_selection.anchor_offset};
  return GetHypertextOffsetFromEndpoint(anchor_object, anchor_offset);
}

int AXPlatformNodeBase::GetUnignoredSelectionFocus() {
  ui::AXTree::Selection unignored_selection =
      delegate_->GetUnignoredSelection();
  int32_t focus_id = unignored_selection.focus_object_id;
  AXPlatformNodeBase* focus_object =
      static_cast<AXPlatformNodeBase*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_object)
    return -1;

  int focus_offset = int{unignored_selection.focus_offset};
  return GetHypertextOffsetFromEndpoint(focus_object, focus_offset);
}

void AXPlatformNodeBase::GetSelectionOffsets(int* selection_start,
                                             int* selection_end) {
  DCHECK(selection_start && selection_end);

  if (IsPlainTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                      selection_start) &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, selection_end)) {
    return;
  }

  GetSelectionOffsetsFromTree(selection_start, selection_end);
}

void AXPlatformNodeBase::GetSelectionOffsetsFromTree(int* selection_start,
                                                     int* selection_end) {
  DCHECK(selection_start && selection_end);

  *selection_start = GetUnignoredSelectionAnchor();
  *selection_end = GetUnignoredSelectionFocus();
  if (*selection_start < 0 || *selection_end < 0)
    return;

  // There are three cases when a selection would start and end on the same
  // character:
  // 1. Anchor and focus are both in a subtree that is to the right of this
  // object.
  // 2. Anchor and focus are both in a subtree that is to the left of this
  // object.
  // 3. Anchor and focus are in a subtree represented by a single embedded
  // object character.
  // Only case 3 refers to a valid selection because cases 1 and 2 fall
  // outside this object in their entirety.
  // Selections that span more than one character are by definition inside
  // this object, so checking them is not necessary.
  if (*selection_start == *selection_end && !HasCaret()) {
    *selection_start = -1;
    *selection_end = -1;
    return;
  }

  // The IA2 Spec says that if the largest of the two offsets falls on an
  // embedded object character and if there is a selection in that embedded
  // object, it should be incremented by one so that it points after the
  // embedded object character.
  // This is a signal to AT software that the embedded object is also part of
  // the selection.
  int* largest_offset =
      (*selection_start <= *selection_end) ? selection_end : selection_start;
  AXPlatformNodeBase* hyperlink =
      GetHyperlinkFromHypertextOffset(*largest_offset);
  if (!hyperlink)
    return;

  int hyperlink_selection_start, hyperlink_selection_end;
  hyperlink->GetSelectionOffsets(&hyperlink_selection_start,
                                 &hyperlink_selection_end);
  if (hyperlink_selection_start >= 0 && hyperlink_selection_end >= 0 &&
      hyperlink_selection_start != hyperlink_selection_end) {
    ++(*largest_offset);
  }
}

bool AXPlatformNodeBase::IsSameHypertextCharacter(
    const AXHypertext& old_hypertext,
    size_t old_char_index,
    size_t new_char_index) {
  if (old_char_index >= old_hypertext.hypertext.size() ||
      new_char_index >= hypertext_.hypertext.size()) {
    return false;
  }

  // For anything other than the "embedded character", we just compare the
  // characters directly.
  base::char16 old_ch = old_hypertext.hypertext[old_char_index];
  base::char16 new_ch = hypertext_.hypertext[new_char_index];
  if (old_ch != new_ch)
    return false;
  if (new_ch != kEmbeddedCharacter)
    return true;

  // If it's an embedded character, they're only identical if the child id
  // the hyperlink points to is the same.
  const std::map<int32_t, int32_t>& old_offset_to_index =
      old_hypertext.hyperlink_offset_to_index;
  const std::vector<int32_t>& old_hyperlinks = old_hypertext.hyperlinks;
  int32_t old_hyperlinkscount = static_cast<int32_t>(old_hyperlinks.size());
  auto iter = old_offset_to_index.find(static_cast<int32_t>(old_char_index));
  int old_index = (iter != old_offset_to_index.end()) ? iter->second : -1;
  int old_child_id = (old_index >= 0 && old_index < old_hyperlinkscount)
                         ? old_hyperlinks[old_index]
                         : -1;

  const std::map<int32_t, int32_t>& new_offset_to_index =
      hypertext_.hyperlink_offset_to_index;
  const std::vector<int32_t>& new_hyperlinks = hypertext_.hyperlinks;
  int32_t new_hyperlinkscount = static_cast<int32_t>(new_hyperlinks.size());
  iter = new_offset_to_index.find(static_cast<int32_t>(new_char_index));
  int new_index = (iter != new_offset_to_index.end()) ? iter->second : -1;
  int new_child_id = (new_index >= 0 && new_index < new_hyperlinkscount)
                         ? new_hyperlinks[new_index]
                         : -1;

  return old_child_id == new_child_id;
}

// Return true if the index represents a text character.
bool AXPlatformNodeBase::IsText(const base::string16& text,
                                size_t index,
                                bool is_indexed_from_end) {
  size_t text_len = text.size();
  if (index == text_len)
    return false;
  auto ch = text[is_indexed_from_end ? text_len - index - 1 : index];
  return ch != kEmbeddedCharacter;
}

void AXPlatformNodeBase::ComputeHypertextRemovedAndInserted(
    const AXHypertext& old_hypertext,
    size_t* start,
    size_t* old_len,
    size_t* new_len) {
  *start = 0;
  *old_len = 0;
  *new_len = 0;

  // Do not compute for static text objects, otherwise redundant text change
  // announcements will occur in live regions, as the parent hypertext also
  // changes.
  if (GetData().role == ax::mojom::Role::kStaticText)
    return;

  const base::string16& old_text = old_hypertext.hypertext;
  const base::string16& new_text = hypertext_.hypertext;

  // TODO(accessibility) Plumb through which part of text changed so we don't
  // have to guess what changed based on character differences. This can be
  // wrong in some cases as follows:
  // -- EDITABLE --
  // If editable: when part of the text node changes, assume only that part
  // changed, and not the entire thing. For example, if "car" changes to
  // "cat", assume only 1 letter changed. This code compares common characters
  // to guess what has changed.
  // -- NOT EDITABLE --
  // When part of the text changes, assume the entire node's text changed. For
  // example, if "car" changes to "cat" then assume all 3 letters changed.
  // Note, it is possible (though rare) that CharacterData methods are used to
  // remove, insert, replace or append a substring.
  bool allow_partial_text_node_changes =
      GetData().HasState(ax::mojom::State::kEditable);
  size_t prefix_index = 0;
  size_t common_prefix = 0;
  while (prefix_index < old_text.size() && prefix_index < new_text.size() &&
         IsSameHypertextCharacter(old_hypertext, prefix_index, prefix_index)) {
    ++prefix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, prefix_index) && !IsText(new_text, prefix_index))) {
      common_prefix = prefix_index;
    }
  }

  size_t suffix_index = 0;
  size_t common_suffix = 0;
  while (common_prefix + suffix_index < old_text.size() &&
         common_prefix + suffix_index < new_text.size() &&
         IsSameHypertextCharacter(old_hypertext,
                                  old_text.size() - suffix_index - 1,
                                  new_text.size() - suffix_index - 1)) {
    ++suffix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, suffix_index, true) &&
         !IsText(new_text, suffix_index, true))) {
      common_suffix = suffix_index;
    }
  }

  *start = common_prefix;
  *old_len = old_text.size() - common_prefix - common_suffix;
  *new_len = new_text.size() - common_prefix - common_suffix;
}

int AXPlatformNodeBase::FindTextBoundary(
    AXTextBoundary boundary,
    int offset,
    AXTextBoundaryDirection direction,
    ax::mojom::TextAffinity affinity) const {
  if (boundary != AXTextBoundary::kSentenceStart) {
    base::Optional<int> boundary_offset =
        GetDelegate()->FindTextBoundary(boundary, offset, direction, affinity);
    if (boundary_offset.has_value())
      return *boundary_offset;
  }

  std::vector<int32_t> unused_line_start_offsets;
  return static_cast<int>(
      FindAccessibleTextBoundary(GetHypertext(), unused_line_start_offsets,
                                 boundary, offset, direction, affinity));
}

int AXPlatformNodeBase::NearestTextIndexToPoint(gfx::Point point) {
  // For text objects, find the text position nearest to the point.The nearest
  // index of a non-text object is implicitly 0. Text fields such as textarea
  // have an embedded div inside them that holds all the text,
  // GetRangeBoundsRect will correctly handle these nodes
  int nearest_index = 0;
  const AXCoordinateSystem coordinate_system = AXCoordinateSystem::kScreen;
  const AXClippingBehavior clipping_behavior = AXClippingBehavior::kUnclipped;

  // Manhattan Distance  is used to provide faster distance estimates.
  // get the distance from the point to the bounds of each character.
  float shortest_distance = GetDelegate()
                                ->GetInnerTextRangeBoundsRect(
                                    0, 1, coordinate_system, clipping_behavior)
                                .ManhattanDistanceToPoint(point);
  for (int i = 1, text_length = GetInnerText().length(); i < text_length; ++i) {
    float current_distance =
        GetDelegate()
            ->GetInnerTextRangeBoundsRect(i, i + 1, coordinate_system,
                                          clipping_behavior)
            .ManhattanDistanceToPoint(point);
    if (current_distance < shortest_distance) {
      shortest_distance = current_distance;
      nearest_index = i;
    }
  }
  return nearest_index;
}

std::string AXPlatformNodeBase::GetInvalidValue() const {
  const AXPlatformNodeBase* target = this;
  // The aria-invalid=spelling/grammar need to be exposed as text attributes for
  // a range matching the visual underline representing the error.
  if (static_cast<ax::mojom::InvalidState>(
          target->GetIntAttribute(ax::mojom::IntAttribute::kInvalidState)) ==
          ax::mojom::InvalidState::kNone &&
      target->IsTextOnlyObject() && target->GetParent()) {
    // Text nodes need to reflect the invalid state of their parent object,
    // otherwise spelling and grammar errors communicated through aria-invalid
    // won't be reflected in text attributes.
    target = static_cast<AXPlatformNodeBase*>(
        FromNativeViewAccessible(target->GetParent()));
  }

  std::string invalid_value("");
  // Note: spelling+grammar errors case is disallowed and not supported. It
  // could possibly arise with aria-invalid on the ancestor of a spelling error,
  // but this is not currently described in any spec and no real-world use cases
  // have been found.
  switch (static_cast<ax::mojom::InvalidState>(
      target->GetIntAttribute(ax::mojom::IntAttribute::kInvalidState))) {
    case ax::mojom::InvalidState::kNone:
    case ax::mojom::InvalidState::kFalse:
      break;
    case ax::mojom::InvalidState::kTrue:
      invalid_value = "true";
      break;
    case ax::mojom::InvalidState::kOther: {
      if (!target->GetStringAttribute(
              ax::mojom::StringAttribute::kAriaInvalidValue, &invalid_value)) {
        // Set the attribute to "true", since we cannot be more specific.
        invalid_value = "true";
      }
      break;
    }
  }
  return invalid_value;
}

ui::TextAttributeList AXPlatformNodeBase::ComputeTextAttributes() const {
  ui::TextAttributeList attributes;

  // We include list markers for now, but there might be other objects that are
  // auto generated.
  // TODO(nektar): Compute what objects are auto-generated in Blink.
  if (GetData().role == ax::mojom::Role::kListMarker)
    attributes.push_back(std::make_pair("auto-generated", "true"));

  int color;
  if (GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor, &color)) {
    unsigned int alpha = SkColorGetA(color);
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // Don't expose default value of pure white.
    if (alpha && (red != 255 || green != 255 || blue != 255)) {
      std::string color_value = "rgb(" + base::NumberToString(red) + ',' +
                                base::NumberToString(green) + ',' +
                                base::NumberToString(blue) + ')';
      SanitizeTextAttributeValue(color_value, &color_value);
      attributes.push_back(std::make_pair("background-color", color_value));
    }
  }

  if (GetIntAttribute(ax::mojom::IntAttribute::kColor, &color)) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // Don't expose default value of black.
    if (red || green || blue) {
      std::string color_value = "rgb(" + base::NumberToString(red) + ',' +
                                base::NumberToString(green) + ',' +
                                base::NumberToString(blue) + ')';
      SanitizeTextAttributeValue(color_value, &color_value);
      attributes.push_back(std::make_pair("color", color_value));
    }
  }

  // First try to get the inherited font family name from the delegate. If we
  // cannot find any name, fall back to looking the hierarchy of this node's
  // AXNodeData instead.
  std::string font_family(GetDelegate()->GetInheritedFontFamilyName());
  if (font_family.empty()) {
    font_family =
        GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  }

  // Attribute has no default value.
  if (!font_family.empty()) {
    SanitizeTextAttributeValue(font_family, &font_family);
    attributes.push_back(std::make_pair("font-family", font_family));
  }

  float font_size;
  // Attribute has no default value.
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize, &font_size)) {
    // The IA2 Spec requires the value to be in pt, not in pixels.
    // There are 72 points per inch.
    // We assume that there are 96 pixels per inch on a standard display.
    // TODO(nektar): Figure out the current value of pixels per inch.
    float points = font_size * 72.0 / 96.0;

    // Round to the nearest 0.5 points.
    points = std::round(points * 2.0) / 2.0;

    attributes.push_back(
        std::make_pair("font-size", base::NumberToString(points) + "pt"));
  }

  // TODO(nektar): Add Blink support for the following attributes:
  // text-line-through-mode, text-line-through-width, text-outline:false,
  // text-position:baseline, text-shadow:none, text-underline-mode:continuous.

  int32_t text_style = GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  if (text_style) {
    if (GetData().HasTextStyle(ax::mojom::TextStyle::kBold))
      attributes.push_back(std::make_pair("font-weight", "bold"));
    if (GetData().HasTextStyle(ax::mojom::TextStyle::kItalic))
      attributes.push_back(std::make_pair("font-style", "italic"));
    if (GetData().HasTextStyle(ax::mojom::TextStyle::kLineThrough)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.push_back(std::make_pair("text-line-through-style", "solid"));
    }
    if (GetData().HasTextStyle(ax::mojom::TextStyle::kUnderline)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.push_back(std::make_pair("text-underline-style", "solid"));
    }
  }

  // Screen readers look at the text attributes to determine if something is
  // misspelled, so we need to propagate any spelling attributes from immediate
  // parents of text-only objects.
  std::string invalid_value = GetInvalidValue();
  if (!invalid_value.empty())
    attributes.push_back(std::make_pair("invalid", invalid_value));

  std::string language = GetDelegate()->GetLanguage();
  if (!language.empty()) {
    SanitizeTextAttributeValue(language, &language);
    attributes.push_back(std::make_pair("language", language));
  }

  auto text_direction = static_cast<ax::mojom::TextDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
      break;
    case ax::mojom::TextDirection::kLtr:
      attributes.push_back(std::make_pair("writing-mode", "lr"));
      break;
    case ax::mojom::TextDirection::kRtl:
      attributes.push_back(std::make_pair("writing-mode", "rl"));
      break;
    case ax::mojom::TextDirection::kTtb:
      attributes.push_back(std::make_pair("writing-mode", "tb"));
      break;
    case ax::mojom::TextDirection::kBtt:
      // Not listed in the IA2 Spec.
      attributes.push_back(std::make_pair("writing-mode", "bt"));
      break;
  }

  auto text_position = static_cast<ax::mojom::TextPosition>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextPosition));
  switch (text_position) {
    case ax::mojom::TextPosition::kNone:
      break;
    case ax::mojom::TextPosition::kSubscript:
      attributes.push_back(std::make_pair("text-position", "sub"));
      break;
    case ax::mojom::TextPosition::kSuperscript:
      attributes.push_back(std::make_pair("text-position", "super"));
      break;
  }

  return attributes;
}

void AXPlatformNodeBase::SanitizeTextAttributeValue(const std::string& input,
                                                    std::string* output) const {
  DCHECK(output);
}

}  // namespace ui
