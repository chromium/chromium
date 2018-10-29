// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_NODE_DATA_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class Transform;
};

namespace ui {

// Return true if |attr| should be interpreted as the id of another node
// in the same tree.
AX_EXPORT bool IsNodeIdIntAttribute(ax::mojom::IntAttribute attr);

// Return true if |attr| should be interpreted as a list of ids of
// nodes in the same tree.
AX_EXPORT bool IsNodeIdIntListAttribute(ax::mojom::IntListAttribute attr);

// A compact representation of the accessibility information for a
// single accessible object, in a form that can be serialized and sent from
// one process to another.
struct AX_EXPORT AXNodeData {
  AXNodeData();
  virtual ~AXNodeData();

  AXNodeData(const AXNodeData& other);
  AXNodeData& operator=(AXNodeData other);

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or base::string16, for convenience.

  bool HasBoolAttribute(ax::mojom::BoolAttribute attribute) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute, bool* value) const;

  bool HasFloatAttribute(ax::mojom::FloatAttribute attribute) const;
  float GetFloatAttribute(ax::mojom::FloatAttribute attribute) const;
  bool GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                         float* value) const;

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const;

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;

  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;
  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  bool HasStringListAttribute(ax::mojom::StringListAttribute attribute) const;
  const std::vector<std::string>& GetStringListAttribute(
      ax::mojom::StringListAttribute attribute) const;
  bool GetStringListAttribute(ax::mojom::StringListAttribute attribute,
                              std::vector<std::string>* value) const;

  bool GetHtmlAttribute(const char* attribute, base::string16* value) const;
  bool GetHtmlAttribute(const char* attribute, std::string* value) const;

  // Setting accessibility attributes.
  void AddStringAttribute(ax::mojom::StringAttribute attribute,
                          const std::string& value);
  void AddIntAttribute(ax::mojom::IntAttribute attribute, int32_t value);
  void RemoveIntAttribute(ax::mojom::IntAttribute attribute);
  void AddFloatAttribute(ax::mojom::FloatAttribute attribute, float value);
  void AddBoolAttribute(ax::mojom::BoolAttribute attribute, bool value);
  void AddIntListAttribute(ax::mojom::IntListAttribute attribute,
                           const std::vector<int32_t>& value);
  void AddStringListAttribute(ax::mojom::StringListAttribute attribute,
                              const std::vector<std::string>& value);

  //
  // Convenience functions.
  //

  // Adds the name attribute or replaces it if already present.
  void SetName(const std::string& name);
  void SetName(const base::string16& name);

  // Allows nameless objects to pass accessibility checks.
  void SetNameExplicitlyEmpty();

  // Adds the description attribute or replaces it if already present.
  void SetDescription(const std::string& description);
  void SetDescription(const base::string16& description);

  // Adds the value attribute or replaces it if already present.
  void SetValue(const std::string& value);
  void SetValue(const base::string16& value);

  // Returns true if the given enum bit is 1.
  bool HasState(ax::mojom::State state) const;
  bool HasAction(ax::mojom::Action action) const;

  // Set or remove bits in the given enum's corresponding bitfield.
  ax::mojom::State AddState(ax::mojom::State state);
  ax::mojom::State RemoveState(ax::mojom::State state);
  ax::mojom::Action AddAction(ax::mojom::Action action);

  // Helper functions to get or set some common int attributes with some
  // specific enum types. To remove an attribute, set it to None.
  //
  // Please keep in alphabetic order.
  ax::mojom::CheckedState GetCheckedState() const;
  void SetCheckedState(ax::mojom::CheckedState checked_state);
  ax::mojom::DefaultActionVerb GetDefaultActionVerb() const;
  void SetDefaultActionVerb(ax::mojom::DefaultActionVerb default_action_verb);
  ax::mojom::HasPopup GetHasPopup() const;
  void SetHasPopup(ax::mojom::HasPopup has_popup);
  ax::mojom::InvalidState GetInvalidState() const;
  void SetInvalidState(ax::mojom::InvalidState invalid_state);
  ax::mojom::NameFrom GetNameFrom() const;
  void SetNameFrom(ax::mojom::NameFrom name_from);
  ax::mojom::TextPosition GetTextPosition() const;
  void SetTextPosition(ax::mojom::TextPosition text_position);
  ax::mojom::Restriction GetRestriction() const;
  void SetRestriction(ax::mojom::Restriction restriction);
  ax::mojom::TextDirection GetTextDirection() const;
  void SetTextDirection(ax::mojom::TextDirection text_direction);

  // Return a string representation of this data, for debugging.
  virtual std::string ToString() const;

  // As much as possible this should behave as a simple, serializable,
  // copyable struct.
  int32_t id = -1;
  ax::mojom::Role role = ax::mojom::Role::kUnknown;
  uint32_t state = static_cast<uint32_t>(ax::mojom::State::kNone);
  uint32_t actions = static_cast<uint32_t>(ax::mojom::Action::kNone);
  std::vector<std::pair<ax::mojom::StringAttribute, std::string>>
      string_attributes;
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>> int_attributes;
  std::vector<std::pair<ax::mojom::FloatAttribute, float>> float_attributes;
  std::vector<std::pair<ax::mojom::BoolAttribute, bool>> bool_attributes;
  std::vector<std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>
      intlist_attributes;
  std::vector<
      std::pair<ax::mojom::StringListAttribute, std::vector<std::string>>>
      stringlist_attributes;
  base::StringPairs html_attributes;
  std::vector<int32_t> child_ids;

  // TODO(dmazzoni): replace the following three members with a single
  // instance of AXRelativeBounds.

  // The id of an ancestor node in the same AXTree that this object's
  // bounding box is relative to, or -1 if there's no offset container.
  int32_t offset_container_id = -1;

  // The relative bounding box of this node.
  gfx::RectF location;

  // An additional transform to apply to position this object and its subtree.
  // NOTE: this member is a std::unique_ptr because it's rare and gfx::Transform
  // takes up a fair amount of space. The assignment operator and copy
  // constructor both make a duplicate of the owned pointer, so it acts more
  // like a member than a pointer.
  std::unique_ptr<gfx::Transform> transform;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_DATA_H_
