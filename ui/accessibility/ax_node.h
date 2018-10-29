// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_H_
#define UI_ACCESSIBILITY_AX_NODE_H_

#include <stdint.h>

#include <ostream>
#include <vector>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

class AXTableInfo;

// One node in an AXTree.
class AX_EXPORT AXNode final {
 public:
  // Interface to the tree class that owns an AXNode. We use this instead
  // of letting AXNode have a pointer to its AXTree directly so that we're
  // forced to think twice before calling an AXTree interface that might not
  // be necessary.
  class OwnerTree {
   public:
    // See AXTree.
    virtual AXTableInfo* GetTableInfo(const AXNode* table_node) const = 0;
    // See AXTree.
    virtual AXNode* GetFromId(int32_t id) const = 0;
  };

  // The constructor requires a parent, id, and index in parent, but
  // the data is not required. After initialization, only index_in_parent
  // is allowed to change, the others are guaranteed to never change.
  AXNode(OwnerTree* tree, AXNode* parent, int32_t id, int32_t index_in_parent);
  virtual ~AXNode();

  // Accessors.
  OwnerTree* tree() const { return tree_; }
  int32_t id() const { return data_.id; }
  AXNode* parent() const { return parent_; }
  int child_count() const { return static_cast<int>(children_.size()); }
  const AXNodeData& data() const { return data_; }
  const std::vector<AXNode*>& children() const { return children_; }
  int index_in_parent() const { return index_in_parent_; }

  // Get the child at the given index.
  AXNode* ChildAtIndex(int index) const { return children_[index]; }

  // Walking the tree skipping ignored nodes.
  int GetUnignoredChildCount() const;
  AXNode* GetUnignoredChildAtIndex(int index) const;
  AXNode* GetUnignoredParent() const;
  int GetUnignoredIndexInParent() const;

  // Returns true if the node has any of the text related roles.
  bool IsTextNode() const;

  // Set the node's accessibility data. This may be done during initialization
  // or later when the node data changes.
  void SetData(const AXNodeData& src);

  // Update this node's location. This is separate from |SetData| just because
  // changing only the location is common and should be more efficient than
  // re-copying all of the data.
  //
  // The node's location is stored as a relative bounding box, the ID of
  // the element it's relative to, and an optional transformation matrix.
  // See ax_node_data.h for details.
  void SetLocation(int32_t offset_container_id,
                   const gfx::RectF& location,
                   gfx::Transform* transform);

  // Set the index in parent, for example if siblings were inserted or deleted.
  void SetIndexInParent(int index_in_parent);

  // Swap the internal children vector with |children|. This instance
  // now owns all of the passed children.
  void SwapChildren(std::vector<AXNode*>& children);

  // This is called when the AXTree no longer includes this node in the
  // tree. Reference counting is used on some platforms because the
  // operating system may hold onto a reference to an AXNode
  // object even after we're through with it, so this may decrement the
  // reference count and clear out the object's data.
  void Destroy();

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(AXNode* ancestor);

  // Gets the text offsets where new lines start either from the node's data or
  // by computing them and caching the result.
  std::vector<int> GetOrComputeLineStartOffsets();

  // Accessing accessibility attributes.
  // See |AXNodeData| for more information.

  bool HasBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().HasBoolAttribute(attribute);
  }
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute) const {
    return data().GetBoolAttribute(attribute);
  }
  bool GetBoolAttribute(ax::mojom::BoolAttribute attribute, bool* value) const {
    return data().GetBoolAttribute(attribute, value);
  }

  bool HasFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().HasFloatAttribute(attribute);
  }
  float GetFloatAttribute(ax::mojom::FloatAttribute attribute) const {
    return data().GetFloatAttribute(attribute);
  }
  bool GetFloatAttribute(ax::mojom::FloatAttribute attribute,
                         float* value) const {
    return data().GetFloatAttribute(attribute, value);
  }

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const {
    return data().HasIntAttribute(attribute);
  }
  int32_t GetIntAttribute(ax::mojom::IntAttribute attribute) const {
    return data().GetIntAttribute(attribute);
  }
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const {
    return data().GetIntAttribute(attribute, value);
  }

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const {
    return data().HasStringAttribute(attribute);
  }
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const {
    return data().GetStringAttribute(attribute);
  }
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const {
    return data().GetStringAttribute(attribute, value);
  }

  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            base::string16* value) const {
    return data().GetString16Attribute(attribute, value);
  }
  base::string16 GetString16Attribute(
      ax::mojom::StringAttribute attribute) const {
    return data().GetString16Attribute(attribute);
  }

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const {
    return data().HasIntListAttribute(attribute);
  }
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const {
    return data().GetIntListAttribute(attribute);
  }
  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const {
    return data().GetIntListAttribute(attribute, value);
  }

  bool HasStringListAttribute(ax::mojom::StringListAttribute attribute) const {
    return data().HasStringListAttribute(attribute);
  }
  const std::vector<std::string>& GetStringListAttribute(
      ax::mojom::StringListAttribute attribute) const {
    return data().GetStringListAttribute(attribute);
  }
  bool GetStringListAttribute(ax::mojom::StringListAttribute attribute,
                              std::vector<std::string>* value) const {
    return data().GetStringListAttribute(attribute, value);
  }

  bool GetHtmlAttribute(const char* attribute, base::string16* value) const {
    return data().GetHtmlAttribute(attribute, value);
  }
  bool GetHtmlAttribute(const char* attribute, std::string* value) const {
    return data().GetHtmlAttribute(attribute, value);
  }

  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  base::string16 GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  //
  // Helper functions for tables, table rows, and table cells.
  // Most of these functions construct and cache an AXTableInfo behind
  // the scenes to infer many properties of tables.
  //
  // TODO(dmazzoni): Make these const - not trivial because AXTableInfo
  // does need to modify the AXTree.
  //

  // Table-like nodes (including grids).
  bool IsTable() const;
  int32_t GetTableColCount() const;
  int32_t GetTableRowCount() const;
  int32_t GetTableCellCount() const;
  AXNode* GetTableCellFromIndex(int32_t index) const;
  AXNode* GetTableCellFromCoords(int32_t row_index, int32_t col_index) const;
  void GetTableColHeaderNodeIds(int32_t col_index,
                                std::vector<int32_t>* col_header_ids) const;
  void GetTableRowHeaderNodeIds(int32_t row_index,
                                std::vector<int32_t>* row_header_ids) const;
  void GetTableUniqueCellIds(std::vector<int32_t>* row_header_ids) const;
  // Extra computed nodes for the accessibility tree for macOS:
  // one column node for each table column, followed by one
  // table header container node, or nullptr if not applicable.
  std::vector<AXNode*>* GetExtraMacNodes() const;

  // Table row-like nodes.
  bool IsTableRow() const;
  int32_t GetTableRowRowIndex() const;

  // Table cell-like nodes.
  bool IsTableCellOrHeader() const;
  int32_t GetTableCellIndex() const;
  int32_t GetTableCellColIndex() const;
  int32_t GetTableCellRowIndex() const;
  int32_t GetTableCellColSpan() const;
  int32_t GetTableCellRowSpan() const;
  int32_t GetTableCellAriaColIndex() const;
  int32_t GetTableCellAriaRowIndex() const;
  void GetTableCellColHeaderNodeIds(std::vector<int32_t>* col_header_ids) const;
  void GetTableCellRowHeaderNodeIds(std::vector<int32_t>* row_header_ids) const;
  void GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const;
  void GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const;

 private:
  // Computes the text offset where each line starts by traversing all child
  // leaf nodes.
  void ComputeLineStartOffsets(std::vector<int>* line_offsets,
                               int* start_offset) const;
  AXTableInfo* GetAncestorTableInfo() const;
  void IdVectorToNodeVector(std::vector<int32_t>& ids,
                            std::vector<AXNode*>* nodes) const;

  OwnerTree* tree_;  // Owns this.
  int index_in_parent_;
  AXNode* parent_;
  std::vector<AXNode*> children_;
  AXNodeData data_;
};

AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXNode& node);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_H_
