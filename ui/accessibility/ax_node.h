// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_H_
#define UI_ACCESSIBILITY_AX_NODE_H_

#include <stdint.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/optional.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace ui {

class AXTableInfo;
struct AXLanguageInfo;

// One node in an AXTree.
class AX_EXPORT AXNode final {
 public:
  // Defines the type used for AXNode IDs.
  using AXID = int32_t;

  // If a node is not yet or no longer valid, its ID should have a value of
  // kInvalidAXID.
  static constexpr AXID kInvalidAXID = 0;

  // Interface to the tree class that owns an AXNode. We use this instead
  // of letting AXNode have a pointer to its AXTree directly so that we're
  // forced to think twice before calling an AXTree interface that might not
  // be necessary.
  class OwnerTree {
   public:
    struct Selection {
      bool is_backward;
      AXID anchor_object_id;
      int anchor_offset;
      ax::mojom::TextAffinity anchor_affinity;
      AXID focus_object_id;
      int focus_offset;
      ax::mojom::TextAffinity focus_affinity;
    };

    // See AXTree::GetAXTreeID.
    virtual AXTreeID GetAXTreeID() const = 0;
    // See AXTree::GetTableInfo.
    virtual AXTableInfo* GetTableInfo(const AXNode* table_node) const = 0;
    // See AXTree::GetFromId.
    virtual AXNode* GetFromId(int32_t id) const = 0;

    virtual int32_t GetPosInSet(const AXNode& node,
                                const AXNode* ordered_set) = 0;
    virtual int32_t GetSetSize(const AXNode& node,
                               const AXNode* ordered_set) = 0;
    virtual Selection GetUnignoredSelection() const = 0;
    virtual bool GetTreeUpdateInProgressState() const = 0;
    virtual bool HasPaginationSupport() const = 0;
  };

  template <typename NodeType,
            NodeType* (NodeType::*NextSibling)() const,
            NodeType* (NodeType::*PreviousSibling)() const,
            NodeType* (NodeType::*LastChild)() const>
  class ChildIteratorBase {
   public:
    ChildIteratorBase(const NodeType* parent, NodeType* child);
    ChildIteratorBase(const ChildIteratorBase& it);
    ~ChildIteratorBase() {}
    bool operator==(const ChildIteratorBase& rhs) const;
    bool operator!=(const ChildIteratorBase& rhs) const;
    ChildIteratorBase& operator++();
    ChildIteratorBase& operator--();
    NodeType* get() const;
    NodeType& operator*() const;
    NodeType* operator->() const;

   protected:
    const NodeType* parent_;
    NodeType* child_;
  };

  // The constructor requires a parent, id, and index in parent, but
  // the data is not required. After initialization, only index_in_parent
  // and unignored_index_in_parent is allowed to change, the others are
  // guaranteed to never change.
  AXNode(OwnerTree* tree,
         AXNode* parent,
         int32_t id,
         size_t index_in_parent,
         size_t unignored_index_in_parent = 0);
  virtual ~AXNode();

  // Accessors.
  OwnerTree* tree() const { return tree_; }
  int32_t id() const { return data_.id; }
  AXNode* parent() const { return parent_; }
  const AXNodeData& data() const { return data_; }
  const std::vector<AXNode*>& children() const { return children_; }
  size_t index_in_parent() const { return index_in_parent_; }

  // Returns ownership of |data_| to the caller; effectively clearing |data_|.
  AXNodeData&& TakeData();

  // Walking the tree skipping ignored nodes.
  size_t GetUnignoredChildCount() const;
  AXNode* GetUnignoredChildAtIndex(size_t index) const;
  AXNode* GetUnignoredParent() const;
  size_t GetUnignoredIndexInParent() const;
  size_t GetIndexInParent() const;
  AXNode* GetFirstUnignoredChild() const;
  AXNode* GetLastUnignoredChild() const;
  AXNode* GetDeepestFirstUnignoredChild() const;
  AXNode* GetDeepestLastUnignoredChild() const;
  AXNode* GetNextUnignoredSibling() const;
  AXNode* GetPreviousUnignoredSibling() const;
  AXNode* GetNextUnignoredInTreeOrder() const;
  AXNode* GetPreviousUnignoredInTreeOrder() const;

  using UnignoredChildIterator =
      ChildIteratorBase<AXNode,
                        &AXNode::GetNextUnignoredSibling,
                        &AXNode::GetPreviousUnignoredSibling,
                        &AXNode::GetLastUnignoredChild>;
  UnignoredChildIterator UnignoredChildrenBegin() const;
  UnignoredChildIterator UnignoredChildrenEnd() const;

  // Returns true if the node has any of the text related roles.
  bool IsText() const;

  // Returns true if the node has any line break related roles or is the child a
  // node with line break related roles.
  bool IsLineBreak() const;

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
  void SetIndexInParent(size_t index_in_parent);

  // Update the unignored index in parent for unignored children.
  void UpdateUnignoredCachedValues();

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
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const {
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

  // PosInSet and SetSize public methods.
  bool IsOrderedSetItem() const;
  bool IsOrderedSet() const;
  base::Optional<int> GetPosInSet();
  base::Optional<int> GetSetSize();

  // Helpers for GetPosInSet and GetSetSize.
  // Returns true if the role of ordered set matches the role of item.
  // Returns false otherwise.
  bool SetRoleMatchesItemRole(const AXNode* ordered_set) const;

  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  base::string16 GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  // Return a string representing the language code.
  //
  // This will consider the language declared in the DOM, and may eventually
  // attempt to automatically detect the language from the text.
  //
  // This language code will be BCP 47.
  //
  // Returns empty string if no appropriate language was found.
  std::string GetLanguage();

  //
  // Helper functions for tables, table rows, and table cells.
  // Most of these functions construct and cache an AXTableInfo behind
  // the scenes to infer many properties of tables.
  //
  // These interfaces use attributes provided by the source of the
  // AX tree where possible, but fills in missing details and ignores
  // specified attributes when they're bad or inconsistent. That way
  // you're guaranteed to get a valid, consistent table when using these
  // interfaces.
  //

  // Table-like nodes (including grids). All indices are 0-based except
  // ARIA indices are all 1-based. In other words, the top-left corner
  // of the table is row 0, column 0, cell index 0 - but that same cell
  // has a minimum ARIA row index of 1 and column index of 1.
  //
  // The below methods return base::nullopt if the AXNode they are called on is
  // not inside a table.
  bool IsTable() const;
  base::Optional<int> GetTableColCount() const;
  base::Optional<int> GetTableRowCount() const;
  base::Optional<int> GetTableAriaColCount() const;
  base::Optional<int> GetTableAriaRowCount() const;
  base::Optional<int> GetTableCellCount() const;
  AXNode* GetTableCaption() const;
  AXNode* GetTableCellFromIndex(int index) const;
  AXNode* GetTableCellFromCoords(int row_index, int col_index) const;
  void GetTableColHeaderNodeIds(int col_index,
                                std::vector<int32_t>* col_header_ids) const;
  void GetTableRowHeaderNodeIds(int row_index,
                                std::vector<int32_t>* row_header_ids) const;
  void GetTableUniqueCellIds(std::vector<int32_t>* row_header_ids) const;
  // Extra computed nodes for the accessibility tree for macOS:
  // one column node for each table column, followed by one
  // table header container node, or nullptr if not applicable.
  const std::vector<AXNode*>* GetExtraMacNodes() const;

  // Table row-like nodes.
  bool IsTableRow() const;
  base::Optional<int> GetTableRowRowIndex() const;

#if defined(OS_MACOSX)
  // Table column-like nodes. These nodes are only present on macOS.
  bool IsTableColumn() const;
  base::Optional<int> GetTableColColIndex() const;
#endif  // defined(OS_MACOSX)

  // Table cell-like nodes.
  bool IsTableCellOrHeader() const;
  base::Optional<int> GetTableCellIndex() const;
  base::Optional<int> GetTableCellColIndex() const;
  base::Optional<int> GetTableCellRowIndex() const;
  base::Optional<int> GetTableCellColSpan() const;
  base::Optional<int> GetTableCellRowSpan() const;
  base::Optional<int> GetTableCellAriaColIndex() const;
  base::Optional<int> GetTableCellAriaRowIndex() const;
  void GetTableCellColHeaderNodeIds(std::vector<int32_t>* col_header_ids) const;
  void GetTableCellRowHeaderNodeIds(std::vector<int32_t>* row_header_ids) const;
  void GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const;
  void GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const;

  // Helper methods to check if a cell is an ARIA-1.1+ 'cell' or 'gridcell'
  bool IsCellOrHeaderOfARIATable() const;
  bool IsCellOrHeaderOfARIAGrid() const;

  // Return an object containing information about the languages used.
  // Callers should not retain this pointer, instead they should request it
  // every time it is needed.
  //
  // Clients likely want to use GetLanguage instead.
  //
  // Returns nullptr if the node has no language info.
  AXLanguageInfo* GetLanguageInfo();

  // This should only be called by the LabelLanguageForSubtree and is used as
  // part of the language detection feature.
  void SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info);

  // Returns true if node has ignored state or ignored role.
  bool IsIgnored() const;

 private:
  // Computes the text offset where each line starts by traversing all child
  // leaf nodes.
  void ComputeLineStartOffsets(std::vector<int>* line_offsets,
                               int* start_offset) const;
  AXTableInfo* GetAncestorTableInfo() const;
  void IdVectorToNodeVector(std::vector<int32_t>& ids,
                            std::vector<AXNode*>* nodes) const;

  int UpdateUnignoredCachedValuesRecursive(int startIndex);
  AXNode* ComputeLastUnignoredChildRecursive() const;
  AXNode* ComputeFirstUnignoredChildRecursive() const;

  // Finds and returns a pointer to ordered set containing node.
  AXNode* GetOrderedSet() const;

  OwnerTree* tree_;  // Owns this.
  size_t index_in_parent_;
  size_t unignored_index_in_parent_;
  size_t unignored_child_count_;
  AXNode* parent_;
  std::vector<AXNode*> children_;
  AXNodeData data_;

  std::unique_ptr<AXLanguageInfo> language_info_;
};

AX_EXPORT std::ostream& operator<<(std::ostream& stream, const AXNode& node);

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    ChildIteratorBase(const NodeType* parent, NodeType* child)
    : parent_(parent), child_(child) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    ChildIteratorBase(const ChildIteratorBase& it)
    : parent_(it.parent_), child_(it.child_) {}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::
    ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    operator==(const ChildIteratorBase& rhs) const {
  return parent_ == rhs.parent_ && child_ == rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
bool AXNode::
    ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    operator!=(const ChildIteratorBase& rhs) const {
  return parent_ != rhs.parent_ || child_ != rhs.child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>&
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
operator++() {
  if (child_)
    child_ = (child_->*NextSibling)();
  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>&
AXNode::ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
operator--() {
  if (child_)
    child_ = (child_->*PreviousSibling)();
  else
    child_ = (parent_->*LastChild)();
  return *this;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::
    ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::get()
        const {
  DCHECK(child_);
  return child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType& AXNode::
    ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    operator*() const {
  DCHECK(child_);
  return *child_;
}

template <typename NodeType,
          NodeType* (NodeType::*NextSibling)() const,
          NodeType* (NodeType::*PreviousSibling)() const,
          NodeType* (NodeType::*LastChild)() const>
NodeType* AXNode::
    ChildIteratorBase<NodeType, NextSibling, PreviousSibling, LastChild>::
    operator->() const {
  DCHECK(child_);
  return child_;
}

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_H_
