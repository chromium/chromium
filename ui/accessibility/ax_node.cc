// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/transform.h"

namespace ui {

AXNode::AXNode(AXNode::OwnerTree* tree,
               AXNode* parent,
               int32_t id,
               int32_t index_in_parent)
    : tree_(tree), index_in_parent_(index_in_parent), parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() {}

int AXNode::GetUnignoredChildCount() const {
  int count = 0;
  for (int i = 0; i < child_count(); i++) {
    AXNode* child = children_[i];
    if (child->data().HasState(ax::mojom::State::kIgnored))
      count += child->GetUnignoredChildCount();
    else
      count++;
  }
  return count;
}

AXNode* AXNode::GetUnignoredChildAtIndex(int index) const {
  int count = 0;
  for (int i = 0; i < child_count(); i++) {
    AXNode* child = children_[i];
    if (child->data().HasState(ax::mojom::State::kIgnored)) {
      int nested_child_count = child->GetUnignoredChildCount();
      if (index < count + nested_child_count)
        return child->GetUnignoredChildAtIndex(index - count);
      else
        count += nested_child_count;
    } else {
      if (count == index)
        return child;
      else
        count++;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  AXNode* result = parent();
  while (result && result->data().HasState(ax::mojom::State::kIgnored))
    result = result->parent();
  return result;
}

int AXNode::GetUnignoredIndexInParent() const {
  AXNode* parent = GetUnignoredParent();
  if (parent) {
    for (int i = 0; i < parent->GetUnignoredChildCount(); ++i) {
      if (parent->GetUnignoredChildAtIndex(i) == this)
        return i;
    }
  }

  return 0;
}

bool AXNode::IsTextNode() const {
  return data().role == ax::mojom::Role::kStaticText ||
         data().role == ax::mojom::Role::kLineBreak ||
         data().role == ax::mojom::Role::kInlineTextBox;
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int32_t offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.offset_container_id = offset_container_id;
  data_.location = location;
  if (transform)
    data_.transform.reset(new gfx::Transform(*transform));
  else
    data_.transform.reset(nullptr);
}

void AXNode::SetIndexInParent(int index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(AXNode* ancestor) {
  if (this == ancestor)
    return true;
  else if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  std::vector<int> line_offsets;
  if (data().GetIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                                 &line_offsets))
    return line_offsets;

  int start_offset = 0;
  ComputeLineStartOffsets(&line_offsets, &start_offset);
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                            line_offsets);
  return line_offsets;
}

void AXNode::ComputeLineStartOffsets(std::vector<int>* line_offsets,
                                     int* start_offset) const {
  DCHECK(line_offsets);
  DCHECK(start_offset);
  for (const AXNode* child : children()) {
    DCHECK(child);
    if (child->child_count()) {
      child->ComputeLineStartOffsets(line_offsets, start_offset);
      continue;
    }

    // Don't report if the first piece of text starts a new line or not.
    if (*start_offset && !child->data().HasIntAttribute(
                             ax::mojom::IntAttribute::kPreviousOnLineId)) {
      // If there are multiple objects with an empty accessible label at the
      // start of a line, only include a single line start offset.
      if (line_offsets->empty() || line_offsets->back() != *start_offset)
        line_offsets->push_back(*start_offset);
    }

    base::string16 text =
        child->data().GetString16Attribute(ax::mojom::StringAttribute::kName);
    *start_offset += static_cast<int>(text.length());
  }
}

const std::string& AXNode::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXNode* current_node = this;
  do {
    if (current_node->data().HasStringAttribute(attribute))
      return current_node->data().GetStringAttribute(attribute);
    current_node = current_node->parent();
  } while (current_node);
  return base::EmptyString();
}

base::string16 AXNode::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

std::ostream& operator<<(std::ostream& stream, const AXNode& node) {
  return stream << node.data().ToString();
}

bool AXNode::IsTable() const {
  return IsTableLike(data().role);
}

int32_t AXNode::GetTableColCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->col_count;
}

int32_t AXNode::GetTableRowCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return table_info->row_count;
}

int32_t AXNode::GetTableCellCount() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return 0;

  return static_cast<int32_t>(table_info->unique_cell_ids.size());
}

AXNode* AXNode::GetTableCellFromIndex(int32_t index) const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  if (index < 0 ||
      index >= static_cast<int32_t>(table_info->unique_cell_ids.size()))
    return nullptr;

  return tree_->GetFromId(table_info->unique_cell_ids[index]);
}

AXNode* AXNode::GetTableCellFromCoords(int32_t row_index,
                                       int32_t col_index) const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  if (row_index < 0 || row_index >= table_info->row_count || col_index < 0 ||
      col_index >= table_info->col_count)
    return nullptr;

  return tree_->GetFromId(table_info->cell_ids[row_index][col_index]);
}

void AXNode::GetTableColHeaderNodeIds(
    int32_t col_index,
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  if (col_index < 0 || col_index >= table_info->col_count)
    return;

  for (size_t i = 0; i < table_info->col_headers[col_index].size(); i++)
    col_header_ids->push_back(table_info->col_headers[col_index][i]);
}

void AXNode::GetTableRowHeaderNodeIds(
    int32_t row_index,
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  if (row_index < 0 || row_index >= table_info->row_count)
    return;

  for (size_t i = 0; i < table_info->row_headers[row_index].size(); i++)
    row_header_ids->push_back(table_info->row_headers[row_index][i]);
}

void AXNode::GetTableUniqueCellIds(std::vector<int32_t>* cell_ids) const {
  DCHECK(cell_ids);
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return;

  cell_ids->assign(table_info->unique_cell_ids.begin(),
                   table_info->unique_cell_ids.end());
}

std::vector<AXNode*>* AXNode::GetExtraMacNodes() const {
  AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  return &table_info->extra_mac_nodes;
}

//
// Table row-like nodes.
//

bool AXNode::IsTableRow() const {
  return data().role == ax::mojom::Role::kRow;
}

int32_t AXNode::GetTableRowRowIndex() const {
  // TODO(dmazzoni): Compute from AXTableInfo. http://crbug.com/832289
  int32_t row_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, &row_index);
  return row_index;
}

//
// Table cell-like nodes.
//

bool AXNode::IsTableCellOrHeader() const {
  return IsCellOrTableHeader(data().role);
}

int32_t AXNode::GetTableCellIndex() const {
  if (!IsTableCellOrHeader())
    return -1;

  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return -1;

  const auto& iter = table_info->cell_id_to_index.find(id());
  if (iter != table_info->cell_id_to_index.end())
    return iter->second;

  return -1;
}

int32_t AXNode::GetTableCellColIndex() const {
  // TODO(dmazzoni): Compute from AXTableInfo. http://crbug.com/832289
  int32_t col_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex, &col_index);
  return col_index;
}

int32_t AXNode::GetTableCellRowIndex() const {
  // TODO(dmazzoni): Compute from AXTableInfo. http://crbug.com/832289
  int32_t row_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, &row_index);
  return row_index;
}

int32_t AXNode::GetTableCellColSpan() const {
  // If it's not a table cell, don't return a col span.
  if (!IsTableCellOrHeader())
    return 0;

  // Otherwise, try to return a colspan, with 1 as the default if it's not
  // specified.
  int32_t col_span = 1;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, &col_span))
    return col_span;

  return 1;
}

int32_t AXNode::GetTableCellRowSpan() const {
  // If it's not a table cell, don't return a row span.
  if (!IsTableCellOrHeader())
    return 0;

  // Otherwise, try to return a row span, with 1 as the default if it's not
  // specified.
  int32_t row_span = 1;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, &row_span))
    return row_span;
  return 1;
}

int32_t AXNode::GetTableCellAriaColIndex() const {
  int32_t col_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex, &col_index);
  return col_index;
}

int32_t AXNode::GetTableCellAriaRowIndex() const {
  int32_t row_index = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex, &row_index);
  return row_index;
}

void AXNode::GetTableCellColHeaderNodeIds(
    std::vector<int32_t>* col_header_ids) const {
  DCHECK(col_header_ids);
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  int32_t col_index = GetTableCellColIndex();
  if (col_index < 0 || col_index >= table_info->col_count)
    return;

  for (size_t i = 0; i < table_info->col_headers[col_index].size(); i++)
    col_header_ids->push_back(table_info->col_headers[col_index][i]);
}

void AXNode::GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const {
  DCHECK(col_headers);

  std::vector<int32_t> col_header_ids;
  GetTableCellColHeaderNodeIds(&col_header_ids);
  IdVectorToNodeVector(col_header_ids, col_headers);
}

void AXNode::GetTableCellRowHeaderNodeIds(
    std::vector<int32_t>* row_header_ids) const {
  DCHECK(row_header_ids);
  AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return;

  int32_t row_index = GetTableCellRowIndex();
  if (row_index < 0 || row_index >= table_info->row_count)
    return;

  for (size_t i = 0; i < table_info->row_headers[row_index].size(); i++)
    row_header_ids->push_back(table_info->row_headers[row_index][i]);
}

void AXNode::GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const {
  DCHECK(row_headers);

  std::vector<int32_t> row_header_ids;
  GetTableCellRowHeaderNodeIds(&row_header_ids);
  IdVectorToNodeVector(row_header_ids, row_headers);
}

AXTableInfo* AXNode::GetAncestorTableInfo() const {
  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->parent();
  if (node)
    return tree_->GetTableInfo(node);
  return nullptr;
}

void AXNode::IdVectorToNodeVector(std::vector<int32_t>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (int32_t id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

}  // namespace ui
