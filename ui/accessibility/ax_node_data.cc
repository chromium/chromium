// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_data.h"

#include <stddef.h>

#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

namespace {

bool IsFlagSet(uint32_t bitfield, uint32_t flag) {
  return (bitfield & (1U << flag)) != 0;
}

bool IsFlagSet(uint64_t bitfield, uint32_t flag) {
  return (bitfield & (1ULL << flag)) != 0;
}

uint32_t ModifyFlag(uint32_t bitfield, uint32_t flag, bool set) {
  return set ? (bitfield |= (1U << flag)) : (bitfield &= ~(1U << flag));
}

uint64_t ModifyFlag(uint64_t bitfield, uint32_t flag, bool set) {
  return set ? (bitfield |= (1ULL << flag)) : (bitfield &= ~(1ULL << flag));
}

std::string StateBitfieldToString(uint32_t state_enum) {
  std::string str;
  for (uint32_t i = static_cast<uint32_t>(ax::mojom::State::kNone) + 1;
       i <= static_cast<uint32_t>(ax::mojom::State::kMaxValue); ++i) {
    if (IsFlagSet(state_enum, i))
      str += " " +
             base::ToUpperASCII(ui::ToString(static_cast<ax::mojom::State>(i)));
  }
  return str;
}

std::string ActionsBitfieldToString(uint64_t actions) {
  std::string str;
  for (uint32_t i = static_cast<uint32_t>(ax::mojom::Action::kNone) + 1;
       i <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue); ++i) {
    if (IsFlagSet(actions, i)) {
      str += ui::ToString(static_cast<ax::mojom::Action>(i));
      actions = ModifyFlag(actions, i, false);
      str += actions ? "," : "";
    }
  }
  return str;
}

template <typename ItemType, typename ItemToStringFunction>
std::string VectorToString(const std::vector<ItemType>& items,
                           ItemToStringFunction itemToStringFunction) {
  std::string str;
  for (size_t i = 0; i < items.size(); ++i) {
    std::string item_str = itemToStringFunction(items[i]);
    if (item_str.empty())
      continue;
    if (i > 0)
      str += ",";
    str += itemToStringFunction(items[i]);
  }
  return str;
}

std::string IntVectorToString(const std::vector<int32_t>& items) {
  return VectorToString(
      items, [](const int32_t item) { return base::NumberToString(item); });
}

// Helper function that finds a key in a vector of pairs by matching on the
// first value, and returns an iterator.
template <typename FirstType, typename SecondType>
typename std::vector<std::pair<FirstType, SecondType>>::const_iterator
FindInVectorOfPairs(
    FirstType first,
    const std::vector<std::pair<FirstType, SecondType>>& vector) {
  return base::ranges::find(vector, first,
                            &std::pair<FirstType, SecondType>::first);
}

}  // namespace

// Return true if |attr| is a node ID that would need to be mapped when
// renumbering the ids in a combined tree.
bool IsNodeIdIntAttribute(ax::mojom::IntAttribute attr) {
  switch (attr) {
    case ax::mojom::IntAttribute::kActivedescendantId:
    case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
    case ax::mojom::IntAttribute::kMemberOfId:
    case ax::mojom::IntAttribute::kNextOnLineId:
    case ax::mojom::IntAttribute::kPopupForId:
    case ax::mojom::IntAttribute::kPreviousOnLineId:
    case ax::mojom::IntAttribute::kTableHeaderId:
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
    case ax::mojom::IntAttribute::kTableRowHeaderId:
    case ax::mojom::IntAttribute::kNextFocusId:
    case ax::mojom::IntAttribute::kPreviousFocusId:
    case ax::mojom::IntAttribute::kNextWindowFocusId:
    case ax::mojom::IntAttribute::kPreviousWindowFocusId:
      return true;

    // Note: all of the attributes are included here explicitly,
    // rather than using "default:", so that it's a compiler error to
    // add a new attribute without explicitly considering whether it's
    // a node id attribute or not.
    case ax::mojom::IntAttribute::kNone:
    case ax::mojom::IntAttribute::kDefaultActionVerb:
    case ax::mojom::IntAttribute::kScrollX:
    case ax::mojom::IntAttribute::kScrollXMin:
    case ax::mojom::IntAttribute::kScrollXMax:
    case ax::mojom::IntAttribute::kScrollY:
    case ax::mojom::IntAttribute::kScrollYMin:
    case ax::mojom::IntAttribute::kScrollYMax:
    case ax::mojom::IntAttribute::kTextSelStart:
    case ax::mojom::IntAttribute::kTextSelEnd:
    case ax::mojom::IntAttribute::kTableRowCount:
    case ax::mojom::IntAttribute::kTableColumnCount:
    case ax::mojom::IntAttribute::kTableRowIndex:
    case ax::mojom::IntAttribute::kTableColumnIndex:
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
    case ax::mojom::IntAttribute::kTableCellRowIndex:
    case ax::mojom::IntAttribute::kTableCellRowSpan:
    case ax::mojom::IntAttribute::kSortDirection:
    case ax::mojom::IntAttribute::kHierarchicalLevel:
    case ax::mojom::IntAttribute::kNameFrom:
    case ax::mojom::IntAttribute::kDescriptionFrom:
    case ax::mojom::IntAttribute::kDetailsFrom:
    case ax::mojom::IntAttribute::kSetSize:
    case ax::mojom::IntAttribute::kPosInSet:
    case ax::mojom::IntAttribute::kColorValue:
    case ax::mojom::IntAttribute::kAriaCurrentState:
    case ax::mojom::IntAttribute::kHasPopup:
    case ax::mojom::IntAttribute::kIsPopup:
    case ax::mojom::IntAttribute::kBackgroundColor:
    case ax::mojom::IntAttribute::kColor:
    case ax::mojom::IntAttribute::kInvalidState:
    case ax::mojom::IntAttribute::kCheckedState:
    case ax::mojom::IntAttribute::kRestriction:
    case ax::mojom::IntAttribute::kListStyle:
    case ax::mojom::IntAttribute::kTextAlign:
    case ax::mojom::IntAttribute::kTextDirection:
    case ax::mojom::IntAttribute::kTextPosition:
    case ax::mojom::IntAttribute::kTextStyle:
    case ax::mojom::IntAttribute::kTextOverlineStyle:
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
    case ax::mojom::IntAttribute::kAriaColumnCount:
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
    case ax::mojom::IntAttribute::kAriaCellColumnSpan:
    case ax::mojom::IntAttribute::kAriaRowCount:
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
    case ax::mojom::IntAttribute::kAriaCellRowSpan:
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
    case ax::mojom::IntAttribute::kDropeffectDeprecated:
    case ax::mojom::IntAttribute::kDOMNodeIdDeprecated:
    case ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated:
    case ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// Returns true if |attr| contains a vector of node ids that would need
// to be mapped when renumbering the ids in a combined tree.
bool IsNodeIdIntListAttribute(ax::mojom::IntListAttribute attr) {
  switch (attr) {
    case ax::mojom::IntListAttribute::kIndirectChildIds:
    case ax::mojom::IntListAttribute::kControlsIds:
    case ax::mojom::IntListAttribute::kDetailsIds:
    case ax::mojom::IntListAttribute::kDescribedbyIds:
    case ax::mojom::IntListAttribute::kErrormessageIds:
    case ax::mojom::IntListAttribute::kFlowtoIds:
    case ax::mojom::IntListAttribute::kLabelledbyIds:
    case ax::mojom::IntListAttribute::kRadioGroupIds:
      return true;

    // Note: all of the attributes are included here explicitly,
    // rather than using "default:", so that it's a compiler error to
    // add a new attribute without explicitly considering whether it's
    // a node id attribute or not.
    case ax::mojom::IntListAttribute::kNone:
    case ax::mojom::IntListAttribute::kMarkerTypes:
    case ax::mojom::IntListAttribute::kMarkerStarts:
    case ax::mojom::IntListAttribute::kMarkerEnds:
    case ax::mojom::IntListAttribute::kHighlightTypes:
    case ax::mojom::IntListAttribute::kCaretBounds:
    case ax::mojom::IntListAttribute::kCharacterOffsets:
    case ax::mojom::IntListAttribute::kLineStarts:
    case ax::mojom::IntListAttribute::kLineEnds:
    case ax::mojom::IntListAttribute::kSentenceStarts:
    case ax::mojom::IntListAttribute::kSentenceEnds:
    case ax::mojom::IntListAttribute::kWordStarts:
    case ax::mojom::IntListAttribute::kWordEnds:
    case ax::mojom::IntListAttribute::kCustomActionIds:
    case ax::mojom::IntListAttribute::kTextOperationStartOffsets:
    case ax::mojom::IntListAttribute::kTextOperationEndOffsets:
    case ax::mojom::IntListAttribute::kTextOperationEndAnchorIds:
    case ax::mojom::IntListAttribute::kTextOperationStartAnchorIds:
    case ax::mojom::IntListAttribute::kTextOperations:
    case ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties:
    case ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties:
      return false;
  }
}

AXNodeData::AXNodeData() : role(ax::mojom::Role::kUnknown) {}

AXNodeData::~AXNodeData() = default;

AXNodeData::AXNodeData(const AXNodeData& other) {
  id = other.id;
  role = other.role;
  state = other.state;
  actions = other.actions;
  string_attributes = other.string_attributes;
  int_attributes = other.int_attributes;
  float_attributes = other.float_attributes;
  bool_attributes = other.bool_attributes;
  intlist_attributes = other.intlist_attributes;
  stringlist_attributes = other.stringlist_attributes;
  html_attributes = other.html_attributes;
  child_ids = other.child_ids;
  relative_bounds = other.relative_bounds;
}

AXNodeData::AXNodeData(AXNodeData&& other) {
  id = other.id;
  role = other.role;
  state = other.state;
  actions = other.actions;
  string_attributes.swap(other.string_attributes);
  int_attributes.swap(other.int_attributes);
  float_attributes.swap(other.float_attributes);
  bool_attributes.swap(other.bool_attributes);
  intlist_attributes.swap(other.intlist_attributes);
  stringlist_attributes.swap(other.stringlist_attributes);
  html_attributes.swap(other.html_attributes);
  child_ids.swap(other.child_ids);
  relative_bounds = other.relative_bounds;

  other.id = kInvalidAXNodeID;
  other.role = ax::mojom::Role::kUnknown;
  other.state = 0U;
  other.actions = 0ULL;
}

AXNodeData& AXNodeData::operator=(const AXNodeData& other) {
  id = other.id;
  role = other.role;
  state = other.state;
  actions = other.actions;
  string_attributes = other.string_attributes;
  int_attributes = other.int_attributes;
  float_attributes = other.float_attributes;
  bool_attributes = other.bool_attributes;
  intlist_attributes = other.intlist_attributes;
  stringlist_attributes = other.stringlist_attributes;
  html_attributes = other.html_attributes;
  child_ids = other.child_ids;
  relative_bounds = other.relative_bounds;
  return *this;
}

bool AXNodeData::HasBoolAttribute(ax::mojom::BoolAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, bool_attributes);
  return iter != bool_attributes.end();
}

bool AXNodeData::GetBoolAttribute(ax::mojom::BoolAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, bool_attributes);
  if (iter != bool_attributes.end()) {
    return iter->second;
  }
  return kDefaultBoolValue;
}

bool AXNodeData::HasFloatAttribute(ax::mojom::FloatAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, float_attributes);
  return iter != float_attributes.end();
}

float AXNodeData::GetFloatAttribute(ax::mojom::FloatAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, float_attributes);
  if (iter != float_attributes.end()) {
    return iter->second;
  }
  return kDefaultFloatValue;
}

bool AXNodeData::HasIntAttribute(ax::mojom::IntAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, int_attributes);
  return iter != int_attributes.end();
}

int AXNodeData::GetIntAttribute(ax::mojom::IntAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, int_attributes);
  if (iter != int_attributes.end()) {
    return int{iter->second};
  }
  return kDefaultIntValue;
}

bool AXNodeData::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, string_attributes);
  return iter != string_attributes.end();
}

const std::string& AXNodeData::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, string_attributes);
  return iter != string_attributes.end() ? iter->second : base::EmptyString();
}

std::u16string AXNodeData::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  if (!HasStringAttribute(attribute)) {
    return std::u16string();
  }
  const std::string& value_utf8 = GetStringAttribute(attribute);
  return base::UTF8ToUTF16(value_utf8);
}

bool AXNodeData::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, intlist_attributes);
  return iter != intlist_attributes.end();
}

const std::vector<int32_t>& AXNodeData::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  static const base::NoDestructor<std::vector<int32_t>> empty_vector;
  auto iter = FindInVectorOfPairs(attribute, intlist_attributes);
  if (iter != intlist_attributes.end())
    return iter->second;
  return *empty_vector;
}

bool AXNodeData::HasStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  auto iter = FindInVectorOfPairs(attribute, stringlist_attributes);
  return iter != stringlist_attributes.end();
}

const std::vector<std::string>& AXNodeData::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  static const base::NoDestructor<std::vector<std::string>> empty_vector;
  auto iter = FindInVectorOfPairs(attribute, stringlist_attributes);
  if (iter != stringlist_attributes.end())
    return iter->second;
  return *empty_vector;
}

bool AXNodeData::HasHtmlAttribute(const char* attribute) const {
  const std::string* value = FindHtmlAttribute(attribute);
  return value != nullptr;
}

const std::string& AXNodeData::GetHtmlAttribute(const char* attribute) const {
  const std::string* value = FindHtmlAttribute(attribute);
  if (value) {
    return *value;
  }
  return base::EmptyString();
}

const std::string* AXNodeData::FindHtmlAttribute(const char* attribute) const {
  for (const std::pair<std::string, std::string>& html_attribute :
       html_attributes) {
    const std::string& attr = html_attribute.first;
    if (base::EqualsCaseInsensitiveASCII(attr, attribute)) {
      return &html_attribute.second;
    }
  }
  return nullptr;
}

std::u16string AXNodeData::GetHtmlAttributeUTF16(const char* attribute) const {
  return base::UTF8ToUTF16(GetHtmlAttribute(attribute));
}

void AXNodeData::AddChildTreeId(const AXTreeID& tree_id) {
  if (HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    RemoveStringAttribute(ax::mojom::StringAttribute::kChildTreeId);
  }
  if (tree_id.type() == ax::mojom::AXTreeIDType::kUnknown) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }
  std::string tree_id_str = tree_id.ToString();
  DCHECK(!tree_id_str.empty());
  string_attributes.emplace_back(ax::mojom::StringAttribute::kChildTreeId,
                                 tree_id_str);
}

bool AXNodeData::HasChildTreeID() const {
  return HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId);
}

std::optional<AXTreeID> AXNodeData::GetChildTreeID() const {
  if (!HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    return std::nullopt;
  }

  const std::string& child_tree_id_str =
      GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId);
  DCHECK(!child_tree_id_str.empty());
  return AXTreeID::FromString(child_tree_id_str);
}

void AXNodeData::AddBoolAttribute(ax::mojom::BoolAttribute attribute,
                                  bool value) {
  DCHECK_NE(attribute, ax::mojom::BoolAttribute::kNone);
  if (HasBoolAttribute(attribute))
    RemoveBoolAttribute(attribute);
  bool_attributes.emplace_back(attribute, value);
}

void AXNodeData::AddIntAttribute(ax::mojom::IntAttribute attribute, int value) {
  DCHECK_NE(attribute, ax::mojom::IntAttribute::kNone);
  if (HasIntAttribute(attribute))
    RemoveIntAttribute(attribute);
  int_attributes.emplace_back(attribute, value);
}

void AXNodeData::AddFloatAttribute(ax::mojom::FloatAttribute attribute,
                                   float value) {
  DCHECK_NE(attribute, ax::mojom::FloatAttribute::kNone);
  if (HasFloatAttribute(attribute))
    RemoveFloatAttribute(attribute);
  float_attributes.emplace_back(attribute, value);
}

void AXNodeData::AddStringAttribute(ax::mojom::StringAttribute attribute,
                                    const std::string& value) {
  DCHECK_NE(attribute, ax::mojom::StringAttribute::kNone);
  DCHECK_NE(attribute, ax::mojom::StringAttribute::kChildTreeId)
      << "Use AddChildTreeId.";
  if (HasStringAttribute(attribute))
    RemoveStringAttribute(attribute);
  string_attributes.emplace_back(attribute, value);
}

void AXNodeData::AddIntListAttribute(ax::mojom::IntListAttribute attribute,
                                     const std::vector<int32_t>& value) {
  DCHECK_NE(attribute, ax::mojom::IntListAttribute::kNone);
  if (HasIntListAttribute(attribute))
    RemoveIntListAttribute(attribute);
  intlist_attributes.emplace_back(attribute, value);
}

void AXNodeData::AddStringListAttribute(
    ax::mojom::StringListAttribute attribute,
    const std::vector<std::string>& value) {
  DCHECK_NE(attribute, ax::mojom::StringListAttribute::kNone);
  if (HasStringListAttribute(attribute))
    RemoveStringListAttribute(attribute);
  stringlist_attributes.emplace_back(attribute, value);
}

void AXNodeData::RemoveBoolAttribute(ax::mojom::BoolAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::BoolAttribute::kNone);
  std::erase_if(bool_attributes, [attribute](const auto& bool_attribute) {
    return bool_attribute.first == attribute;
  });
}

void AXNodeData::RemoveIntAttribute(ax::mojom::IntAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::IntAttribute::kNone);
  std::erase_if(int_attributes, [attribute](const auto& int_attribute) {
    return int_attribute.first == attribute;
  });
}

void AXNodeData::RemoveFloatAttribute(ax::mojom::FloatAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::FloatAttribute::kNone);
  std::erase_if(float_attributes, [attribute](const auto& float_attribute) {
    return float_attribute.first == attribute;
  });
}

void AXNodeData::RemoveStringAttribute(ax::mojom::StringAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::StringAttribute::kNone);
  std::erase_if(string_attributes, [attribute](const auto& string_attribute) {
    return string_attribute.first == attribute;
  });
}

void AXNodeData::RemoveIntListAttribute(ax::mojom::IntListAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::IntListAttribute::kNone);
  std::erase_if(intlist_attributes, [attribute](const auto& intlist_attribute) {
    return intlist_attribute.first == attribute;
  });
}

void AXNodeData::RemoveStringListAttribute(
    ax::mojom::StringListAttribute attribute) {
  DCHECK_NE(attribute, ax::mojom::StringListAttribute::kNone);
  std::erase_if(stringlist_attributes,
                [attribute](const auto& stringlist_attribute) {
                  return stringlist_attribute.first == attribute;
                });
}

AXTextAttributes AXNodeData::GetTextAttributes() const {
  return AXTextAttributes(*this);
}

int AXNodeData::GetDOMNodeId() const {
  return id > 0 ? id : 0;
}

void AXNodeData::SetName(const std::string& name) {
  // Elements with role='presentation' have Role::kNone. They should not be
  // named. Objects with Role::kUnknown were never given a role. This check
  // is only relevant if the name is not empty.
  // TODO(crbug.com/40863978): It would be nice to have a means to set the name
  // and role at the same time to avoid this ordering requirement.
  DCHECK(name.empty() ||
         (role != ax::mojom::Role::kNone && role != ax::mojom::Role::kUnknown))
      << "Cannot set name to '" << name << "' on class: '"
      << GetStringAttribute(ax::mojom::StringAttribute::kClassName)
      << "' because a valid role is needed to set the default NameFrom "
         "attribute. Set the role first.";

  auto iter = base::ranges::find(
      string_attributes, ax::mojom::StringAttribute::kName,
      &std::pair<ax::mojom::StringAttribute, std::string>::first);

  if (iter == string_attributes.end()) {
    string_attributes.emplace_back(ax::mojom::StringAttribute::kName, name);
  } else {
    iter->second = name;
  }

  // It is possible for `SetName`/`SetNameChecked` to be called after
  // `SetNameExplicitlyEmpty`.
  if (!name.empty() &&
      GetNameFrom() == ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
    RemoveIntAttribute(ax::mojom::IntAttribute::kNameFrom);
  }

  if (HasIntAttribute(ax::mojom::IntAttribute::kNameFrom))
    return;
  // Since this method is mostly used by tests which don't always set the
  // "NameFrom" attribute, we need to set it here to the most likely value if
  // not set, otherwise code that tries to calculate the node's inner text, its
  // hypertext, or even its value, might not know whether to include the name in
  // the result or not.
  //
  // For example, if there is a text field, but it is empty, i.e. it has no
  // value, its value could be its name if "NameFrom" is set to "kPlaceholder"
  // or to "kContents" but not if it's set to "kAttribute". Similarly, if there
  // is a button without any unignored children, it's name can only be
  // equivalent to its inner text if "NameFrom" is set to "kContents" or to
  // "kValue", but not if it is set to "kAttribute".
  if (IsText(role)) {
    SetNameFrom(ax::mojom::NameFrom::kContents);
  } else {
    SetNameFrom(ax::mojom::NameFrom::kAttribute);
  }
}

void AXNodeData::SetName(const std::u16string& name) {
  SetName(base::UTF16ToUTF8(name));
}

void AXNodeData::SetNameChecked(const std::string& name) {
  SetName(name);

  // We do this check after calling `SetName` because `SetName` handles the
  // case where it is called after `SetNameExplicitlyEmpty` by removing the
  // existing `NameFrom::kAttributeExplicitlyEmpty`.
  DCHECK_EQ(name.empty(),
            GetNameFrom() == ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
      << "If the accessible name of Role::" << role << " class: '"
      << GetStringAttribute(ax::mojom::StringAttribute::kClassName)
      << "' is being set to an empty string to improve the "
         "user experience, call `SetNameExplicitlyEmpty` instead of `SetName`.";
}

void AXNodeData::SetNameChecked(const std::u16string& name) {
  SetNameChecked(base::UTF16ToUTF8(name));
}

void AXNodeData::SetNameExplicitlyEmpty() {
  SetNameFrom(ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  SetName(std::string());
}

void AXNodeData::SetDescription(const std::string& description) {
  AddStringAttribute(ax::mojom::StringAttribute::kDescription, description);

  // It is possible for `SetDescription` to be called after
  // `SetDescriptionExplicitlyEmpty`.
  if (!description.empty() &&
      GetDescriptionFrom() ==
          ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty) {
    RemoveIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom);
  }

  DCHECK_EQ(description.empty(),
            GetDescriptionFrom() ==
                ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty)
      << "If the accessible description of Role::" << role << " class: '"
      << GetStringAttribute(ax::mojom::StringAttribute::kClassName)
      << "' is being set to an empty string to improve the user experience, "
         "call SetDescriptionExplicitlyEmpty instead of SetDescription.";

  if (HasIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom))
    return;

  SetDescriptionFrom(ax::mojom::DescriptionFrom::kAriaDescription);
}

void AXNodeData::SetDescription(const std::u16string& description) {
  SetDescription(base::UTF16ToUTF8(description));
}

void AXNodeData::SetDescriptionExplicitlyEmpty() {
  SetDescriptionFrom(ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  SetDescription(std::string());
}

void AXNodeData::SetValue(const std::string& value) {
  AddStringAttribute(ax::mojom::StringAttribute::kValue, value);
}

void AXNodeData::SetValue(const std::u16string& value) {
  SetValue(base::UTF16ToUTF8(value));
}

bool AXNodeData::HasState(ax::mojom::State state_enum) const {
  return IsFlagSet(state, static_cast<uint32_t>(state_enum));
}

bool AXNodeData::HasAction(ax::mojom::Action action) const {
  return IsFlagSet(actions, static_cast<uint32_t>(action));
}

bool AXNodeData::HasTextStyle(ax::mojom::TextStyle text_style_enum) const {
  int32_t style = GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  return IsFlagSet(static_cast<uint32_t>(style),
                   static_cast<uint32_t>(text_style_enum));
}

void AXNodeData::AddState(ax::mojom::State state_enum) {
  DCHECK_GT(static_cast<int>(state_enum),
            static_cast<int>(ax::mojom::State::kNone));
  DCHECK_LE(static_cast<int>(state_enum),
            static_cast<int>(ax::mojom::State::kMaxValue));
  state = ModifyFlag(state, static_cast<uint32_t>(state_enum), true);
}

void AXNodeData::RemoveState(ax::mojom::State state_enum) {
  DCHECK_GT(static_cast<int>(state_enum),
            static_cast<int>(ax::mojom::State::kNone));
  DCHECK_LE(static_cast<int>(state_enum),
            static_cast<int>(ax::mojom::State::kMaxValue));
  state = ModifyFlag(state, static_cast<uint32_t>(state_enum), false);
}

void AXNodeData::AddAction(ax::mojom::Action action_enum) {
  switch (action_enum) {
    case ax::mojom::Action::kNone:
      NOTREACHED_IN_MIGRATION();
      break;

    // Note: all of the attributes are included here explicitly, rather than
    // using "default:", so that it's a compiler error to add a new action
    // without explicitly considering whether there are mutually exclusive
    // actions that can be performed on a UI control at the same time.
    case ax::mojom::Action::kBlur:
    case ax::mojom::Action::kFocus: {
      ax::mojom::Action excluded_action =
          (action_enum == ax::mojom::Action::kBlur) ? ax::mojom::Action::kFocus
                                                    : ax::mojom::Action::kBlur;
      DCHECK(!HasAction(excluded_action)) << excluded_action;
      break;
    }

    case ax::mojom::Action::kClearAccessibilityFocus:
    case ax::mojom::Action::kCollapse:
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kDecrement:
    case ax::mojom::Action::kDoDefault:
    case ax::mojom::Action::kExpand:
    case ax::mojom::Action::kGetImageData:
    case ax::mojom::Action::kHitTest:
    case ax::mojom::Action::kIncrement:
    case ax::mojom::Action::kInternalInvalidateTree:
    case ax::mojom::Action::kLoadInlineTextBoxes:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kScrollToMakeVisible:
    case ax::mojom::Action::kScrollToPoint:
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
    case ax::mojom::Action::kSetAccessibilityFocus:
    case ax::mojom::Action::kSetScrollOffset:
    case ax::mojom::Action::kSetSelection:
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
    case ax::mojom::Action::kSetValue:
    case ax::mojom::Action::kShowContextMenu:
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
    case ax::mojom::Action::kGetTextLocation:
    case ax::mojom::Action::kAnnotatePageImages:
    case ax::mojom::Action::kSignalEndOfTest:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kStitchChildTree:
    case ax::mojom::Action::kResumeMedia:
    case ax::mojom::Action::kStartDuckingMedia:
    case ax::mojom::Action::kStopDuckingMedia:
    case ax::mojom::Action::kSuspendMedia:
    case ax::mojom::Action::kLongClick:
      break;
  }

  actions = ModifyFlag(actions, static_cast<uint32_t>(action_enum), true);
}

void AXNodeData::RemoveAction(ax::mojom::Action action_enum) {
  DCHECK_GT(static_cast<int>(action_enum),
            static_cast<int>(ax::mojom::Action::kNone));
  DCHECK_LE(static_cast<int>(action_enum),
            static_cast<int>(ax::mojom::Action::kMaxValue));
  actions = ModifyFlag(actions, static_cast<uint64_t>(action_enum), false);
}

void AXNodeData::AddTextStyle(ax::mojom::TextStyle text_style_enum) {
  DCHECK_GE(static_cast<int>(text_style_enum),
            static_cast<int>(ax::mojom::TextStyle::kMinValue));
  DCHECK_LE(static_cast<int>(text_style_enum),
            static_cast<int>(ax::mojom::TextStyle::kMaxValue));
  int32_t style = GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  style = ModifyFlag(static_cast<uint32_t>(style),
                     static_cast<uint32_t>(text_style_enum), true);
  RemoveIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  AddIntAttribute(ax::mojom::IntAttribute::kTextStyle, style);
}

ax::mojom::CheckedState AXNodeData::GetCheckedState() const {
  return static_cast<ax::mojom::CheckedState>(
      GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
}

void AXNodeData::SetCheckedState(ax::mojom::CheckedState checked_state) {
  if (HasCheckedState())
    RemoveIntAttribute(ax::mojom::IntAttribute::kCheckedState);
  if (checked_state != ax::mojom::CheckedState::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                    static_cast<int32_t>(checked_state));
  }
}

bool AXNodeData::HasCheckedState() const {
  return HasIntAttribute(ax::mojom::IntAttribute::kCheckedState);
}

ax::mojom::DefaultActionVerb AXNodeData::GetDefaultActionVerb() const {
  return static_cast<ax::mojom::DefaultActionVerb>(
      GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));
}

void AXNodeData::SetDefaultActionVerb(
    ax::mojom::DefaultActionVerb default_action_verb) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb))
    RemoveIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb);
  if (default_action_verb != ax::mojom::DefaultActionVerb::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                    static_cast<int32_t>(default_action_verb));
  }
}

ax::mojom::HasPopup AXNodeData::GetHasPopup() const {
  return static_cast<ax::mojom::HasPopup>(
      GetIntAttribute(ax::mojom::IntAttribute::kHasPopup));
}

void AXNodeData::SetHasPopup(ax::mojom::HasPopup has_popup) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kHasPopup))
    RemoveIntAttribute(ax::mojom::IntAttribute::kHasPopup);
  if (has_popup != ax::mojom::HasPopup::kFalse) {
    AddIntAttribute(ax::mojom::IntAttribute::kHasPopup,
                    static_cast<int32_t>(has_popup));
  }
}

ax::mojom::IsPopup AXNodeData::GetIsPopup() const {
  return static_cast<ax::mojom::IsPopup>(
      GetIntAttribute(ax::mojom::IntAttribute::kIsPopup));
}

void AXNodeData::SetIsPopup(ax::mojom::IsPopup is_popup) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kIsPopup)) {
    RemoveIntAttribute(ax::mojom::IntAttribute::kIsPopup);
  }
  if (is_popup != ax::mojom::IsPopup::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kIsPopup,
                    static_cast<int32_t>(is_popup));
  }
}

ax::mojom::InvalidState AXNodeData::GetInvalidState() const {
  return static_cast<ax::mojom::InvalidState>(
      GetIntAttribute(ax::mojom::IntAttribute::kInvalidState));
}

void AXNodeData::SetInvalidState(ax::mojom::InvalidState invalid_state) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState))
    RemoveIntAttribute(ax::mojom::IntAttribute::kInvalidState);
  if (invalid_state != ax::mojom::InvalidState::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kInvalidState,
                    static_cast<int32_t>(invalid_state));
  }
}

ax::mojom::NameFrom AXNodeData::GetNameFrom() const {
  return static_cast<ax::mojom::NameFrom>(
      GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
}

void AXNodeData::SetNameFrom(ax::mojom::NameFrom name_from) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kNameFrom))
    RemoveIntAttribute(ax::mojom::IntAttribute::kNameFrom);
  if (name_from != ax::mojom::NameFrom::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kNameFrom,
                    static_cast<int32_t>(name_from));
  }
}

ax::mojom::DescriptionFrom AXNodeData::GetDescriptionFrom() const {
  return static_cast<ax::mojom::DescriptionFrom>(
      GetIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom));
}

void AXNodeData::SetDescriptionFrom(
    ax::mojom::DescriptionFrom description_from) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom))
    RemoveIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom);
  if (description_from != ax::mojom::DescriptionFrom::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom,
                    static_cast<int32_t>(description_from));
  }
}

ax::mojom::DetailsFrom AXNodeData::GetDetailsFrom() const {
  return static_cast<ax::mojom::DetailsFrom>(
      GetIntAttribute(ax::mojom::IntAttribute::kDetailsFrom));
}

void AXNodeData::SetDetailsFrom(ax::mojom::DetailsFrom details_from) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kDetailsFrom)) {
    RemoveIntAttribute(ax::mojom::IntAttribute::kDetailsFrom);
  }
  AddIntAttribute(ax::mojom::IntAttribute::kDetailsFrom,
                  static_cast<int32_t>(details_from));
}

ax::mojom::TextPosition AXNodeData::GetTextPosition() const {
  return static_cast<ax::mojom::TextPosition>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextPosition));
}

void AXNodeData::SetTextPosition(ax::mojom::TextPosition text_position) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kTextPosition))
    RemoveIntAttribute(ax::mojom::IntAttribute::kTextPosition);
  if (text_position != ax::mojom::TextPosition::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kTextPosition,
                    static_cast<int32_t>(text_position));
  }
}

ax::mojom::ImageAnnotationStatus AXNodeData::GetImageAnnotationStatus() const {
  return static_cast<ax::mojom::ImageAnnotationStatus>(
      GetIntAttribute(ax::mojom::IntAttribute::kImageAnnotationStatus));
}

void AXNodeData::SetImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kImageAnnotationStatus))
    RemoveIntAttribute(ax::mojom::IntAttribute::kImageAnnotationStatus);
  if (status != ax::mojom::ImageAnnotationStatus::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kImageAnnotationStatus,
                    static_cast<int32_t>(status));
  }
}

ax::mojom::Restriction AXNodeData::GetRestriction() const {
  return static_cast<ax::mojom::Restriction>(
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction));
}

void AXNodeData::SetRestriction(ax::mojom::Restriction restriction) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kRestriction))
    RemoveIntAttribute(ax::mojom::IntAttribute::kRestriction);
  if (restriction != ax::mojom::Restriction::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kRestriction,
                    static_cast<int32_t>(restriction));
  }
}

ax::mojom::ListStyle AXNodeData::GetListStyle() const {
  return static_cast<ax::mojom::ListStyle>(
      GetIntAttribute(ax::mojom::IntAttribute::kListStyle));
}

void AXNodeData::SetListStyle(ax::mojom::ListStyle list_style) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kListStyle))
    RemoveIntAttribute(ax::mojom::IntAttribute::kListStyle);
  if (list_style != ax::mojom::ListStyle::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kListStyle,
                    static_cast<int32_t>(list_style));
  }
}

ax::mojom::TextAlign AXNodeData::GetTextAlign() const {
  return static_cast<ax::mojom::TextAlign>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextAlign));
}

void AXNodeData::SetTextAlign(ax::mojom::TextAlign text_align) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kTextAlign))
    RemoveIntAttribute(ax::mojom::IntAttribute::kTextAlign);
  AddIntAttribute(ax::mojom::IntAttribute::kTextAlign,
                  static_cast<int32_t>(text_align));
}

ax::mojom::WritingDirection AXNodeData::GetTextDirection() const {
  return static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
}

void AXNodeData::SetTextDirection(ax::mojom::WritingDirection text_direction) {
  if (HasIntAttribute(ax::mojom::IntAttribute::kTextDirection))
    RemoveIntAttribute(ax::mojom::IntAttribute::kTextDirection);
  if (text_direction != ax::mojom::WritingDirection::kNone) {
    AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                    static_cast<int32_t>(text_direction));
  }
}

bool AXNodeData::IsActivatable() const {
  return IsTextField() || role == ax::mojom::Role::kListBox;
}

bool AXNodeData::IsActiveLiveRegionRoot() const {
  if (!HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus)) {
    return false;
  }
  const std::string& aria_live_status =
      GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  return aria_live_status != "off";
}

bool AXNodeData::IsButtonPressed() const {
  // Currently there is no internal representation for |aria-pressed|, and
  // we map |aria-pressed="true"| to ax::mojom::CheckedState::kTrue for a native
  // button or role="button".
  // https://www.w3.org/TR/wai-aria-1.1/#aria-pressed
  if (IsButton(role) && GetCheckedState() == ax::mojom::CheckedState::kTrue)
    return true;
  return false;
}

bool AXNodeData::IsClickable() const {
  // If it has a custom default action verb except for
  // ax::mojom::DefaultActionVerb::kClickAncestor, it's definitely clickable.
  // ax::mojom::DefaultActionVerb::kClickAncestor is used when an element with a
  // click listener is present in its ancestry chain.
  if (HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb) &&
      (GetDefaultActionVerb() != ax::mojom::DefaultActionVerb::kClickAncestor))
    return true;

  return ui::IsClickable(role);
}

bool AXNodeData::IsContainedInActiveLiveRegion() const {
  if (!HasStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus)) {
    return false;
  }

  const std::string& aria_container_live_status =
      GetStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);

  return aria_container_live_status != "off" &&
         HasStringAttribute(ax::mojom::StringAttribute::kName);
}

bool AXNodeData::IsSelectable() const {
  // It's selectable if it has the attribute, whether it's true or false.
  return HasBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
         GetRestriction() != ax::mojom::Restriction::kDisabled;
}

bool AXNodeData::IsIgnored() const {
  return HasState(ax::mojom::State::kIgnored) || role == ax::mojom::Role::kNone;
}

bool AXNodeData::IsInvisible() const {
  return HasState(ax::mojom::State::kInvisible);
}

bool AXNodeData::IsInvisibleOrIgnored() const {
  return IsIgnored() || IsInvisible();
}

bool AXNodeData::IsInvocable() const {
  // A control is "invocable" if it initiates an action when activated but
  // does not maintain any state. A control that maintains state when activated
  // would be considered a toggle or expand-collapse element - these elements
  // are "clickable" but not "invocable". Similarly, if the action only involves
  // activating the control, such as when clicking a text field, the control is
  // not considered "invocable". However, all links are always invocable.
  return IsLink(role) || (IsClickable() && !IsActivatable() &&
                          !SupportsExpandCollapse() && !SupportsToggle(role));
}

bool AXNodeData::IsMenuButton() const {
  // According to the WAI-ARIA spec, a menu button is a native button or an ARIA
  // role="button" that opens a menu. Although ARIA does not include a role
  // specifically for menu buttons, screen readers identify buttons that have
  // aria-haspopup="true" or aria-haspopup="menu" as menu buttons, and Blink
  // maps both to HasPopup::kMenu.
  // https://www.w3.org/TR/wai-aria-practices/#menubutton
  // https://www.w3.org/TR/wai-aria-1.1/#aria-haspopup
  if (IsButton(role) && GetHasPopup() == ax::mojom::HasPopup::kMenu)
    return true;

  return false;
}

bool AXNodeData::IsTextField() const {
  if (HasState(ax::mojom::State::kIgnored)) {
    return false;
  }
  return IsAtomicTextField() || IsNonAtomicTextField();
}

bool AXNodeData::IsPasswordField() const {
  return IsTextField() && HasState(ax::mojom::State::kProtected);
}

bool AXNodeData::IsAtomicTextField() const {
  // The ARIA spec suggests a textbox or a searchbox is a simple text field,
  // like an <input> or <textarea> depending on aria-multiline. However there is
  // nothing to stop an author from adding the textbox role to a
  // non-contenteditable element, or from adding or removing non-plain-text
  // nodes. If we treat the textbox role as atomic when contenteditable is not
  // set, it can break accessibility by pruning interactive elements from the
  // accessibility tree. Therefore, until we have a reliable means to identify
  // truly atomic ARIA textboxes, we treat them as non-atomic in Blink.
  return (ui::IsTextField(role) || IsSpinnerTextField()) &&
         !GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot);
}

bool AXNodeData::IsNonAtomicTextField() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot);
}

bool AXNodeData::IsSpinnerTextField() const {
  // All root editable nodes that have the role `spinbutton` should be treated
  // as spinner text fields, this is so that the node does not lose
  // TextField characteristics that are exposed so that ATs can interact with
  // the node appropriately. Like for example, using left and right arrow keys
  // to scan and read out the characters in the text field.
  if (role != ax::mojom::Role::kSpinButton)
    return false;
  if (!HasState(ax::mojom::State::kEditable))
    return false;
  // Nodes that inherited their editable state should not be included.
  return !(
      HasState(ax::mojom::State::kRichlyEditable) &&
      !GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot));
}

bool AXNodeData::IsRangeValueSupported() const {
  if (role == ax::mojom::Role::kSplitter) {
    // According to the ARIA spec, role="separator" acts as a splitter only
    // when focusable, and supports a range only in that case.
    return HasState(ax::mojom::State::kFocusable);
  }
  return ui::IsRangeValueSupported(role);
}

bool AXNodeData::SupportsExpandCollapse() const {
  if (GetHasPopup() != ax::mojom::HasPopup::kFalse ||
      HasState(ax::mojom::State::kExpanded) ||
      HasState(ax::mojom::State::kCollapsed))
    return true;

  return ui::SupportsExpandCollapse(role);
}

// TODO(accessibility) Consider reusing code from AXTreeFormatterBlink, where
// the |verbose| parameter alters the property filter. Would remove ~800 lines.
std::string AXNodeData::ToString(bool verbose) const {
  std::string result;

  // Most important properties are provided first.
  result += "id=" + base::NumberToString(id);
  result += " ";
  result += ui::ToString(role);

  result += StateBitfieldToString(state);

  if (HasStringAttribute(ax::mojom::StringAttribute::kHtmlTag)) {
    result += base::StringPrintf(
        " <%s",
        GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag).c_str());
    if (HasStringAttribute(ax::mojom::StringAttribute::kClassName)) {
      result += base::StringPrintf(
          ".%s",
          GetStringAttribute(ax::mojom::StringAttribute::kClassName).c_str());
    }
    if (HasStringAttribute(ax::mojom::StringAttribute::kHtmlId)) {
      const std::string& id_attr =
          GetStringAttribute(ax::mojom::StringAttribute::kHtmlId);
      result += base::StringPrintf("#%s", id_attr.c_str());
    }
    result += ">";
  } else if (HasStringAttribute(ax::mojom::StringAttribute::kClassName)) {
    result += " class_name=" +
              GetStringAttribute(ax::mojom::StringAttribute::kClassName);
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kRole)) {
    result += " aria_role=";
    result += GetStringAttribute(ax::mojom::StringAttribute::kRole);
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    result += " name=";
    result += GetStringAttribute(ax::mojom::StringAttribute::kName);
  }

  // TODO(accessibility) Blink a11y shouldn't serialize name_from field for
  // text, because it's always contents, and is just adding noise.
  if (!ui::IsText(role) &&
      HasIntAttribute(ax::mojom::IntAttribute::kNameFrom)) {
    result += " name_from=";
    result += ui::ToString(static_cast<ax::mojom::NameFrom>(
        GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)));
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kUrl)) {
    result += " url=";
    result += GetStringAttribute(ax::mojom::StringAttribute::kUrl);
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    result += " has_child_tree";
  }

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren)) {
    result += " clips_children";
  }

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kBusy)) {
    result += " busy";
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kDisplay)) {
    std::string str = GetStringAttribute(ax::mojom::StringAttribute::kDisplay);
    // Show CSS display type if it is interesting.
    if (str != "block") {
      result += " display=" + str;
    }
  }

  if (!child_ids.empty()) {
    result += " child_ids=" + IntVectorToString(child_ids);
  }

  if (!verbose) {
    return result;
  }

  // Properties of lesser importance are provided if verbose is set to true.

  result += " " + relative_bounds.ToString();

  for (const std::pair<ax::mojom::IntAttribute, int32_t>& int_attribute :
       int_attributes) {
    std::string value = base::NumberToString(int_attribute.second);
    switch (int_attribute.first) {
      case ax::mojom::IntAttribute::kDefaultActionVerb:
        result += std::string(" action=") +
                  ui::ToString(static_cast<ax::mojom::DefaultActionVerb>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kScrollX:
        result += " scroll_x=" + value;
        break;
      case ax::mojom::IntAttribute::kScrollXMin:
        result += " scroll_x_min=" + value;
        break;
      case ax::mojom::IntAttribute::kScrollXMax:
        result += " scroll_x_max=" + value;
        break;
      case ax::mojom::IntAttribute::kScrollY:
        result += " scroll_y=" + value;
        break;
      case ax::mojom::IntAttribute::kScrollYMin:
        result += " scroll_y_min=" + value;
        break;
      case ax::mojom::IntAttribute::kScrollYMax:
        result += " scroll_y_max=" + value;
        break;
      case ax::mojom::IntAttribute::kHierarchicalLevel:
        result += " level=" + value;
        break;
      case ax::mojom::IntAttribute::kTextSelStart:
        result += " sel_start=" + value;
        break;
      case ax::mojom::IntAttribute::kTextSelEnd:
        result += " sel_end=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaColumnCount:
        result += " aria_column_count=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaCellColumnIndex:
        result += " aria_cell_column_index=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaCellColumnSpan:
        result += " aria_cell_column_span=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaRowCount:
        result += " aria_row_count=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaCellRowIndex:
        result += " aria_cell_row_index=" + value;
        break;
      case ax::mojom::IntAttribute::kAriaCellRowSpan:
        result += " aria_cell_row_span=" + value;
        break;
      case ax::mojom::IntAttribute::kTableRowCount:
        result += " rows=" + value;
        break;
      case ax::mojom::IntAttribute::kTableColumnCount:
        result += " cols=" + value;
        break;
      case ax::mojom::IntAttribute::kTableCellColumnIndex:
        result += " col=" + value;
        break;
      case ax::mojom::IntAttribute::kTableCellRowIndex:
        result += " row=" + value;
        break;
      case ax::mojom::IntAttribute::kTableCellColumnSpan:
        result += " colspan=" + value;
        break;
      case ax::mojom::IntAttribute::kTableCellRowSpan:
        result += " rowspan=" + value;
        break;
      case ax::mojom::IntAttribute::kTableColumnHeaderId:
        result += " column_header_id=" + value;
        break;
      case ax::mojom::IntAttribute::kTableColumnIndex:
        result += " column_index=" + value;
        break;
      case ax::mojom::IntAttribute::kTableHeaderId:
        result += " header_id=" + value;
        break;
      case ax::mojom::IntAttribute::kTableRowHeaderId:
        result += " row_header_id=" + value;
        break;
      case ax::mojom::IntAttribute::kTableRowIndex:
        result += " row_index=" + value;
        break;
      case ax::mojom::IntAttribute::kSortDirection:
        switch (static_cast<ax::mojom::SortDirection>(int_attribute.second)) {
          case ax::mojom::SortDirection::kUnsorted:
            result += " sort_direction=none";
            break;
          case ax::mojom::SortDirection::kAscending:
            result += " sort_direction=ascending";
            break;
          case ax::mojom::SortDirection::kDescending:
            result += " sort_direction=descending";
            break;
          case ax::mojom::SortDirection::kOther:
            result += " sort_direction=other";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kNameFrom:
        // Already provided in default (non-verbose) section above.
        break;
      case ax::mojom::IntAttribute::kDescriptionFrom:
        result += " description_from=";
        result += ui::ToString(
            static_cast<ax::mojom::DescriptionFrom>(int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kDetailsFrom:
        result += " details_from=";
        result += ui::ToString(
            static_cast<ax::mojom::DetailsFrom>(int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kActivedescendantId:
        result += " activedescendant=" + value;
        break;
      case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
        result += " errormessage=" + value;
        break;
      case ax::mojom::IntAttribute::kInPageLinkTargetId:
        result += " in_page_link_target_id=" + value;
        break;
      case ax::mojom::IntAttribute::kMemberOfId:
        result += " member_of_id=" + value;
        break;
      case ax::mojom::IntAttribute::kNextOnLineId:
        result += " next_on_line_id=" + value;
        break;
      case ax::mojom::IntAttribute::kPopupForId:
        result += " popup_for_id=" + value;
        break;
      case ax::mojom::IntAttribute::kPreviousOnLineId:
        result += " previous_on_line_id=" + value;
        break;
      case ax::mojom::IntAttribute::kColorValue:
        result += base::StringPrintf(" color_value=&%X", int_attribute.second);
        break;
      case ax::mojom::IntAttribute::kAriaCurrentState:
        switch (
            static_cast<ax::mojom::AriaCurrentState>(int_attribute.second)) {
          case ax::mojom::AriaCurrentState::kFalse:
            result += " aria_current_state=false";
            break;
          case ax::mojom::AriaCurrentState::kTrue:
            result += " aria_current_state=true";
            break;
          case ax::mojom::AriaCurrentState::kPage:
            result += " aria_current_state=page";
            break;
          case ax::mojom::AriaCurrentState::kStep:
            result += " aria_current_state=step";
            break;
          case ax::mojom::AriaCurrentState::kLocation:
            result += " aria_current_state=location";
            break;
          case ax::mojom::AriaCurrentState::kDate:
            result += " aria_current_state=date";
            break;
          case ax::mojom::AriaCurrentState::kTime:
            result += " aria_current_state=time";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kBackgroundColor:
        result +=
            base::StringPrintf(" background_color=&%X", int_attribute.second);
        break;
      case ax::mojom::IntAttribute::kColor:
        result += base::StringPrintf(" color=&%X", int_attribute.second);
        break;
      case ax::mojom::IntAttribute::kListStyle:
        switch (static_cast<ax::mojom::ListStyle>(int_attribute.second)) {
          case ax::mojom::ListStyle::kCircle:
            result += " list_style=circle";
            break;
          case ax::mojom::ListStyle::kDisc:
            result += " list_style=disc";
            break;
          case ax::mojom::ListStyle::kImage:
            result += " list_style=image";
            break;
          case ax::mojom::ListStyle::kNumeric:
            result += " list_style=numeric";
            break;
          case ax::mojom::ListStyle::kOther:
            result += " list_style=other";
            break;
          case ax::mojom::ListStyle::kSquare:
            result += " list_style=square";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kTextAlign:
        result += " text_align=";
        result += ui::ToString(
            static_cast<ax::mojom::TextAlign>(int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kTextDirection:
        switch (
            static_cast<ax::mojom::WritingDirection>(int_attribute.second)) {
          case ax::mojom::WritingDirection::kLtr:
            result += " text_direction=ltr";
            break;
          case ax::mojom::WritingDirection::kRtl:
            result += " text_direction=rtl";
            break;
          case ax::mojom::WritingDirection::kTtb:
            result += " text_direction=ttb";
            break;
          case ax::mojom::WritingDirection::kBtt:
            result += " text_direction=btt";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kTextPosition:
        switch (static_cast<ax::mojom::TextPosition>(int_attribute.second)) {
          case ax::mojom::TextPosition::kNone:
            result += " text_position=none";
            break;
          case ax::mojom::TextPosition::kSubscript:
            result += " text_position=subscript";
            break;
          case ax::mojom::TextPosition::kSuperscript:
            result += " text_position=superscript";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kTextStyle: {
        std::string text_style_value;
        if (HasTextStyle(ax::mojom::TextStyle::kBold))
          text_style_value += "bold,";
        if (HasTextStyle(ax::mojom::TextStyle::kItalic))
          text_style_value += "italic,";
        if (HasTextStyle(ax::mojom::TextStyle::kUnderline))
          text_style_value += "underline,";
        if (HasTextStyle(ax::mojom::TextStyle::kLineThrough))
          text_style_value += "line-through,";
        if (HasTextStyle(ax::mojom::TextStyle::kOverline))
          text_style_value += "overline,";
        result += text_style_value.substr(0, text_style_value.size() - 1);
        break;
      }
      case ax::mojom::IntAttribute::kTextOverlineStyle:
        result += std::string(" text_overline_style=") +
                  ui::ToString(static_cast<ax::mojom::TextDecorationStyle>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kTextStrikethroughStyle:
        result += std::string(" text_strikethrough_style=") +
                  ui::ToString(static_cast<ax::mojom::TextDecorationStyle>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kTextUnderlineStyle:
        result += std::string(" text_underline_style=") +
                  ui::ToString(static_cast<ax::mojom::TextDecorationStyle>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kSetSize:
        result += " setsize=" + value;
        break;
      case ax::mojom::IntAttribute::kPosInSet:
        result += " posinset=" + value;
        break;
      case ax::mojom::IntAttribute::kHasPopup:
        switch (static_cast<ax::mojom::HasPopup>(int_attribute.second)) {
          case ax::mojom::HasPopup::kTrue:
            result += " haspopup=true";
            break;
          case ax::mojom::HasPopup::kMenu:
            result += " haspopup=menu";
            break;
          case ax::mojom::HasPopup::kListbox:
            result += " haspopup=listbox";
            break;
          case ax::mojom::HasPopup::kTree:
            result += " haspopup=tree";
            break;
          case ax::mojom::HasPopup::kGrid:
            result += " haspopup=grid";
            break;
          case ax::mojom::HasPopup::kDialog:
            result += " haspopup=dialog";
            break;
          case ax::mojom::HasPopup::kFalse:
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kIsPopup:
        switch (static_cast<ax::mojom::IsPopup>(int_attribute.second)) {
          case ax::mojom::IsPopup::kNone:
            break;
          case ax::mojom::IsPopup::kAuto:
            result += " ispopup=auto";
            break;
          case ax::mojom::IsPopup::kHint:
            result += " ispopup=hint";
            break;
          case ax::mojom::IsPopup::kManual:
            result += " ispopup=manual";
            break;
        }
        break;
      case ax::mojom::IntAttribute::kInvalidState:
        switch (static_cast<ax::mojom::InvalidState>(int_attribute.second)) {
          case ax::mojom::InvalidState::kFalse:
            result += " invalid_state=false";
            break;
          case ax::mojom::InvalidState::kTrue:
            result += " invalid_state=true";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kCheckedState:
        switch (static_cast<ax::mojom::CheckedState>(int_attribute.second)) {
          case ax::mojom::CheckedState::kFalse:
            result += " checked_state=false";
            break;
          case ax::mojom::CheckedState::kTrue:
            result += " checked_state=true";
            break;
          case ax::mojom::CheckedState::kMixed:
            result += " checked_state=mixed";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kRestriction:
        switch (static_cast<ax::mojom::Restriction>(int_attribute.second)) {
          case ax::mojom::Restriction::kReadOnly:
            result += " restriction=readonly";
            break;
          case ax::mojom::Restriction::kDisabled:
            result += " restriction=disabled";
            break;
          default:
            break;
        }
        break;
      case ax::mojom::IntAttribute::kNextFocusId:
        result += " next_focus_id=" + value;
        break;
      case ax::mojom::IntAttribute::kPreviousFocusId:
        result += " previous_focus_id=" + value;
        break;
      case ax::mojom::IntAttribute::kNextWindowFocusId:
        result += " next_window_focus_id=" + value;
        break;
      case ax::mojom::IntAttribute::kPreviousWindowFocusId:
        result += " previous_window_focus_id=" + value;
        break;
      case ax::mojom::IntAttribute::kImageAnnotationStatus:
        result += std::string(" image_annotation_status=") +
                  ui::ToString(static_cast<ax::mojom::ImageAnnotationStatus>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kDropeffectDeprecated:
        result += " dropeffect=" + value;
        break;
      case ax::mojom::IntAttribute::kDOMNodeIdDeprecated:
        break;
      case ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated:
        result +=
            std::string(" aria_notification_interrupt=") +
            ui::ToString(static_cast<ax::mojom::AriaNotificationInterrupt>(
                int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated:
        result += std::string(" aria_notification_priority=") +
                  ui::ToString(static_cast<ax::mojom::AriaNotificationPriority>(
                      int_attribute.second));
        break;
      case ax::mojom::IntAttribute::kNone:
        break;
    }
  }

  for (const std::pair<ax::mojom::StringAttribute, std::string>&
           string_attribute : string_attributes) {
    std::string value = string_attribute.second;
    switch (string_attribute.first) {
      case ax::mojom::StringAttribute::kAccessKey:
        result += " access_key=" + value;
        break;
      case ax::mojom::StringAttribute::kAppId:
        result += " app_id=" + value.substr(0, 8);
        break;
      case ax::mojom::StringAttribute::kAriaCellColumnIndexText:
        result += " aria_cell_column_index_text=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaCellRowIndexText:
        result += " aria_cell_row_index_text=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaInvalidValueDeprecated:
        result += " aria_invalid_value=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaBrailleLabel:
        result += " aria_braille_label=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaBrailleRoleDescription:
        result += " aria_braille_role_description=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaNotificationAnnouncementDeprecated:
        result += " aria_notification_announcement=" + value;
        break;
      case ax::mojom::StringAttribute::kAriaNotificationIdDeprecated:
        result += " aria_notification_id=" + value;
        break;
      case ax::mojom::StringAttribute::kCheckedStateDescription:
        result += " checked_state_description=" + value;
        break;
      case ax::mojom::StringAttribute::kAutoComplete:
        result += " autocomplete=" + value;
        break;
      case ax::mojom::StringAttribute::kChildTreeId:
        // This is covered by has_child_tree above. The exact value of the
        // child tree is not added to the string as it varies, and adding it
        // would cause tesrt failures.
        break;
      case ax::mojom::StringAttribute::kChildTreeNodeAppId:
        result += " child_tree_node_app_id=" + value.substr(0, 8);
        break;
      case ax::mojom::StringAttribute::kDescription:
        result += " description=" + value;
        break;
      case ax::mojom::StringAttribute::kDisplay:
        break;
      case ax::mojom::StringAttribute::kDoDefaultLabel:
        result += " doDefaultLabel=" + value;
        break;
      case ax::mojom::StringAttribute::kFontFamily:
        result += " font-family=" + value;
        break;
      case ax::mojom::StringAttribute::kImageAnnotation:
        result += " image_annotation=" + value;
        break;
      case ax::mojom::StringAttribute::kImageDataUrl:
        result += " image_data_url=(" +
                  base::NumberToString(static_cast<int>(value.size())) +
                  " bytes)";
        break;
      case ax::mojom::StringAttribute::kInputType:
        result += " input_type=" + value;
        break;
      case ax::mojom::StringAttribute::kKeyShortcuts:
        result += " key_shortcuts=" + value;
        break;
      case ax::mojom::StringAttribute::kLanguage:
        result += " language=" + value;
        break;
      case ax::mojom::StringAttribute::kLinkTarget:
        result += " link_target=" + value;
        break;
      case ax::mojom::StringAttribute::kLiveRelevant:
        result += " relevant=" + value;
        break;
      case ax::mojom::StringAttribute::kLiveStatus:
        result += " live=" + value;
        break;
      case ax::mojom::StringAttribute::kContainerLiveRelevant:
        result += " container_relevant=" + value;
        break;
      case ax::mojom::StringAttribute::kContainerLiveStatus:
        result += " container_live=" + value;
        break;
      case ax::mojom::StringAttribute::kMathContent:
        result += " math_content=" + value;
        break;
      case ax::mojom::StringAttribute::kPlaceholder:
        result += " placeholder=" + value;
        break;
      case ax::mojom::StringAttribute::kRoleDescription:
        result += " role_description=" + value;
        break;
      case ax::mojom::StringAttribute::kLongClickLabel:
        result += " longClickLabel=" + value;
        break;
      case ax::mojom::StringAttribute::kTooltip:
        result += " tooltip=" + value;
        break;
      case ax::mojom::StringAttribute::kValue:
        result += " value=" + value;
        break;
      case ax::mojom::StringAttribute::kVirtualContent:
        result += " virtual_content=" + value;
        break;
      case ax::mojom::StringAttribute::kClassName:
      case ax::mojom::StringAttribute::kHtmlId:
      case ax::mojom::StringAttribute::kHtmlTag:
      case ax::mojom::StringAttribute::kRole:
      case ax::mojom::StringAttribute::kUrl:
      case ax::mojom::StringAttribute::kName:
        // Already provided in default (non-verbose) section above.
        break;
      case ax::mojom::StringAttribute::kNone:
        break;
    }
  }

  for (const std::pair<ax::mojom::FloatAttribute, float>& float_attribute :
       float_attributes) {
    std::string value = base::NumberToString(float_attribute.second);
    switch (float_attribute.first) {
      case ax::mojom::FloatAttribute::kValueForRange:
        result += " value_for_range=" + value;
        break;
      case ax::mojom::FloatAttribute::kMaxValueForRange:
        result += " max_value=" + value;
        break;
      case ax::mojom::FloatAttribute::kMinValueForRange:
        result += " min_value=" + value;
        break;
      case ax::mojom::FloatAttribute::kStepValueForRange:
        result += " step_value=" + value;
        break;
      case ax::mojom::FloatAttribute::kFontSize:
        result += " font_size=" + value;
        break;
      case ax::mojom::FloatAttribute::kFontWeight:
        result += " font_weight=" + value;
        break;
      case ax::mojom::FloatAttribute::kTextIndent:
        result += " text_indent=" + value;
        break;
      case ax::mojom::FloatAttribute::kChildTreeScale:
        result += " child_tree_scale=" + value;
        break;
      case ax::mojom::FloatAttribute::kNone:
        break;
    }
  }

  for (const std::pair<ax::mojom::BoolAttribute, bool>& bool_attribute :
       bool_attributes) {
    std::string value = bool_attribute.second ? "true" : "false";
    switch (bool_attribute.first) {
      case ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot:
        result += " non_atomic_text_field_root=" + value;
        break;
      case ax::mojom::BoolAttribute::kLiveAtomic:
        result += " atomic=" + value;
        break;
      case ax::mojom::BoolAttribute::kBusy:
        // Already provided in default (non-verbose) section above.
        break;
      case ax::mojom::BoolAttribute::kContainerLiveAtomic:
        result += " container_atomic=" + value;
        break;
      case ax::mojom::BoolAttribute::kContainerLiveBusy:
        result += " container_busy=" + value;
        break;
      case ax::mojom::BoolAttribute::kUpdateLocationOnly:
        result += " update_location_only=" + value;
        break;
      case ax::mojom::BoolAttribute::kCanvasHasFallback:
        result += " has_fallback=" + value;
        break;
      case ax::mojom::BoolAttribute::kModal:
        result += " modal=" + value;
        break;
      case ax::mojom::BoolAttribute::kScrollable:
        result += " scrollable=" + value;
        break;
      case ax::mojom::BoolAttribute::kClickable:
        result += " clickable=" + value;
        break;
      case ax::mojom::BoolAttribute::kClipsChildren:
        // Already provided in default (non-verbose) section above.
        break;
      case ax::mojom::BoolAttribute::kNotUserSelectableStyle:
        result += " not_user_selectable=" + value;
        break;
      case ax::mojom::BoolAttribute::kSelected:
        result += " selected=" + value;
        break;
      case ax::mojom::BoolAttribute::kSelectedFromFocus:
        result += " selected_from_focus=" + value;
        break;
      case ax::mojom::BoolAttribute::kSupportsTextLocation:
        result += " supports_text_location=" + value;
        break;
      case ax::mojom::BoolAttribute::kGrabbedDeprecated:
        result += " grabbed=" + value;
        break;
      case ax::mojom::BoolAttribute::kIsLineBreakingObject:
        result += " is_line_breaking_object=" + value;
        break;
      case ax::mojom::BoolAttribute::kIsPageBreakingObject:
        result += " is_page_breaking_object=" + value;
        break;
      case ax::mojom::BoolAttribute::kHasAriaAttribute:
        result += " has_aria_attribute=" + value;
        break;
      case ax::mojom::BoolAttribute::kTouchPassthroughDeprecated:
        result += " touch_passthrough=" + value;
        break;
      case ax::mojom::BoolAttribute::kLongClickable:
        result += " long_clickable=" + value;
        break;
      case ax::mojom::BoolAttribute::kHasHiddenOffscreenNodes:
        result += " has_hidden_nodes=" + value;
        break;
      case ax::mojom::BoolAttribute::kNone:
        break;
    }
  }

  for (const std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>&
           intlist_attribute : intlist_attributes) {
    const std::vector<int32_t>& values = intlist_attribute.second;
    switch (intlist_attribute.first) {
      case ax::mojom::IntListAttribute::kNone:
        break;
      case ax::mojom::IntListAttribute::kIndirectChildIds:
        result += " indirect_child_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kControlsIds:
        result += " controls_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kDetailsIds:
        result += " details_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kDescribedbyIds:
        result += " describedby_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kErrormessageIds:
        result += " errormessage_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kFlowtoIds:
        result += " flowto_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kLabelledbyIds:
        result += " labelledby_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kRadioGroupIds:
        result += " radio_group_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kMarkerTypes: {
        std::string types_str = VectorToString(values, [](const int32_t type) {
          std::string type_str;
          if (type == static_cast<int32_t>(ax::mojom::MarkerType::kNone))
            return type_str;

          if (type & static_cast<int32_t>(ax::mojom::MarkerType::kSpelling))
            type_str += "spelling&";
          if (type & static_cast<int32_t>(ax::mojom::MarkerType::kGrammar))
            type_str += "grammar&";
          if (type & static_cast<int32_t>(ax::mojom::MarkerType::kHighlight))
            type_str += "highlight&";
          if (type & static_cast<int32_t>(ax::mojom::MarkerType::kTextMatch))
            type_str += "text_match&";
          if (type &
              static_cast<int32_t>(ax::mojom::MarkerType::kActiveSuggestion))
            type_str += "active_suggestion&";
          if (type & static_cast<int32_t>(ax::mojom::MarkerType::kSuggestion))
            type_str += "suggestion&";

          return type_str;
        });

        if (!types_str.empty()) {
          types_str = types_str.substr(0, types_str.size() - 1);
          result += " marker_types=" + types_str;
        }

        break;
      }
      case ax::mojom::IntListAttribute::kMarkerStarts:
        result += " marker_starts=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kMarkerEnds:
        result += " marker_ends=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kHighlightTypes: {
        std::string highlight_types_str =
            VectorToString(values, [](const int32_t highlight_type) {
              if (static_cast<ax::mojom::HighlightType>(highlight_type) ==
                  ax::mojom::HighlightType::kNone)
                return "";
              return ui::ToString(
                  static_cast<ax::mojom::HighlightType>(highlight_type));
            });

        if (!highlight_types_str.empty())
          result += " highlight_types=" + highlight_types_str;
        break;
      }
      case ax::mojom::IntListAttribute::kCaretBounds:
        result += " caret_bounds=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kCharacterOffsets:
        result += " character_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kLineStarts:
        result += " line_start_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kLineEnds:
        result += " line_end_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kSentenceStarts:
        result += " sentence_start_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kSentenceEnds:
        result += " sentence_end_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kWordStarts:
        result += " word_starts=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kWordEnds:
        result += " word_ends=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kCustomActionIds:
        result += " custom_action_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kTextOperationStartOffsets:
        result += " text_operation_start_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kTextOperationEndOffsets:
        result += " text_operation_end_offsets=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kTextOperationStartAnchorIds:
        result +=
            " text_operation_start_anchor_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kTextOperationEndAnchorIds:
        result += " text_operation_end_anchor_ids=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kTextOperations:
        result += " text_operations=" + IntVectorToString(values);
        break;
      case ax::mojom::IntListAttribute::kAriaNotificationInterruptProperties:
        result +=
            " aria_notification_interrupt_properties=" +
            VectorToString(values, [](int32_t interrupt) {
              return ui::ToString(
                  static_cast<ax::mojom::AriaNotificationInterrupt>(interrupt));
            });
        break;
      case ax::mojom::IntListAttribute::kAriaNotificationPriorityProperties:
        result +=
            " aria_notification_priority_properties=" +
            VectorToString(values, [](int32_t priority) {
              return ui::ToString(
                  static_cast<ax::mojom::AriaNotificationPriority>(priority));
            });
        break;
    }
  }

  for (const std::pair<ax::mojom::StringListAttribute,
                       std::vector<std::string>>& stringlist_attribute :
       stringlist_attributes) {
    const std::vector<std::string>& values = stringlist_attribute.second;
    switch (stringlist_attribute.first) {
      case ax::mojom::StringListAttribute::kAriaNotificationAnnouncements:
        result +=
            " aria_notification_announcements=" + base::JoinString(values, ",");
        break;
      case ax::mojom::StringListAttribute::kAriaNotificationIds:
        result += " aria_notification_ids=" + base::JoinString(values, ",");
        break;
      case ax::mojom::StringListAttribute::kCustomActionDescriptions:
        result +=
            " custom_action_descriptions=" + base::JoinString(values, ",");
        break;
      case ax::mojom::StringListAttribute::kNone:
        break;
    }
  }

  for (const std::pair<std::string, std::string>& string_pair :
       html_attributes) {
    result += " " + string_pair.first + "=" + string_pair.second;
  }

  if (actions)
    result += " actions=" + ActionsBitfieldToString(actions);

  return result;
}

size_t AXNodeData::ByteSize() const {
  // Simple fields.
  size_t total_size = sizeof(id) + sizeof(role) + sizeof(state) +
                      sizeof(actions) + sizeof(relative_bounds);

  AXNodeDataSize node_data_size;
  AccumulateSize(node_data_size);
  total_size += node_data_size.ByteSize();
  return total_size;
}

void AXNodeData::AccumulateSize(
    AXNodeData::AXNodeDataSize& node_data_size) const {
  node_data_size.int_attribute_size +=
      int_attributes.size() *
      (sizeof(ax::mojom::IntAttribute) + sizeof(int32_t));
  node_data_size.float_attribute_size +=
      float_attributes.size() *
      (sizeof(ax::mojom::FloatAttribute) + sizeof(float));
  node_data_size.bool_attribute_size +=
      bool_attributes.size() *
      (sizeof(ax::mojom::BoolAttribute) + sizeof(bool));
  node_data_size.child_ids_size = child_ids.size() * sizeof(int32_t);

  for (const auto& pair : string_attributes) {
    node_data_size.string_attribute_size +=
        sizeof(ax::mojom::StringAttribute) + pair.second.size() * sizeof(char);
  }

  for (const auto& pair : intlist_attributes) {
    node_data_size.int_list_attribhute_size +=
        sizeof(ax::mojom::IntListAttribute) +
        pair.second.size() * sizeof(int32_t);
  }

  for (const auto& pair : stringlist_attributes) {
    node_data_size.string_list_attribute_size +=
        sizeof(ax::mojom::StringListAttribute);
    for (const auto& value : pair.second) {
      node_data_size.string_list_attribute_size += value.size() * sizeof(char);
    }
  }

  for (const auto& pair : html_attributes) {
    node_data_size.html_attribute_size +=
        pair.first.size() * sizeof(char) + pair.second.size() * sizeof(char);
  }
}

size_t AXNodeData::AXNodeDataSize::ByteSize() const {
  return int_attribute_size + float_attribute_size + bool_attribute_size +
         string_attribute_size + int_list_attribhute_size +
         string_list_attribute_size + html_attribute_size + child_ids_size;
}

}  // namespace ui
