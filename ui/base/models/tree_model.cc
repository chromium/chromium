// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/tree_model.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "base/check_op.h"

namespace ui {

TreeModelNode::TreeModelNode() = default;

TreeModelNode::TreeModelNode(const std::u16string& title) : title_(title) {}

TreeModelNode::~TreeModelNode() = default;

std::unique_ptr<TreeModelNode> TreeModelNode::Remove(size_t index) {
  CHECK_LT(index, children_.size());
  children_[index]->parent_ = nullptr;
  std::unique_ptr<TreeModelNode> ptr = std::move(children_[index]);
  children_.erase(children_.begin() + static_cast<ptrdiff_t>(index));
  return ptr;
}

void TreeModelNode::DeleteAll() {
  children_.clear();
}

size_t TreeModelNode::GetTotalNodeCount() const {
  size_t count = 1;
  for (const auto& child : children_) {
    count += child->GetTotalNodeCount();
  }
  return count;
}

std::optional<size_t> TreeModelNode::GetIndexOf(
    const TreeModelNode* node) const {
  CHECK(node);
  const auto i =
      std::ranges::find(children_, node, &std::unique_ptr<TreeModelNode>::get);
  return i != children_.end()
             ? std::make_optional(static_cast<size_t>(i - children_.begin()))
             : std::nullopt;
}

bool TreeModelNode::HasAncestor(const TreeModelNode* ancestor) const {
  if (ancestor == this) {
    return true;
  }
  if (!ancestor) {
    return false;
  }
  return parent_ ? parent_->HasAncestor(ancestor) : false;
}

void TreeModelNode::SetTitle(const std::u16string& title) {
  title_ = title;
}

const std::u16string& TreeModelNode::GetTitle() const {
  return title_;
}

const std::u16string& TreeModelNode::GetAccessibleTitle() const {
  return title_.empty() ? placeholder_accessible_title_ : title_;
}

void TreeModelNode::SetPlaceholderAccessibleTitle(
    std::u16string placeholder_accessible_title) {
  placeholder_accessible_title_ = std::move(placeholder_accessible_title);
}

void TreeModelNode::AddImpl(std::unique_ptr<TreeModelNode> node, size_t index) {
  CHECK(node);
  CHECK_LE(index, children_.size());
  CHECK(!node->parent_);
  node->parent_ = this;
  children_.insert(children_.begin() + static_cast<ptrdiff_t>(index),
                   std::move(node));
}

TreeModel::TreeModel(std::unique_ptr<TreeModelNode> root)
    : root_(std::move(root)) {}

TreeModel::~TreeModel() = default;

TreeModelNode* TreeModel::GetRoot() {
  return root_.get();
}

const TreeModelNode* TreeModel::GetRoot() const {
  return root_.get();
}

std::unique_ptr<TreeModelNode> TreeModel::Remove(TreeModelNode* parent,
                                                 size_t index) {
  CHECK(parent);
  std::unique_ptr<TreeModelNode> owned_node = parent->Remove(index);
  NotifyObserverTreeNodeRemoved(parent, index);
  return owned_node;
}

std::unique_ptr<TreeModelNode> TreeModel::Remove(TreeModelNode* parent,
                                                 TreeModelNode* node) {
  CHECK(parent);
  return Remove(parent, parent->GetIndexOf(node).value());
}

void TreeModel::NotifyObserverTreeNodeAdded(TreeModelNode* parent,
                                            size_t index) {
  observer_list_.Notify(&TreeModelObserver::TreeNodeAdded, this, parent, index);
}

void TreeModel::NotifyObserverTreeNodeRemoved(TreeModelNode* parent,
                                              size_t index) {
  observer_list_.Notify(&TreeModelObserver::TreeNodeRemoved, this, parent,
                        index);
}

void TreeModel::NotifyObserverTreeNodeChanged(TreeModelNode* node) {
  observer_list_.Notify(&TreeModelObserver::TreeNodeChanged, this, node);
}

TreeModel::Nodes TreeModel::GetChildren(const TreeModelNode* parent) const {
  CHECK(parent);
  const auto& children = parent->children();
  Nodes nodes;
  nodes.reserve(children.size());
  std::ranges::transform(children, std::back_inserter(nodes),
                         &TreeModelNode::TreeNodes::value_type::get);
  return nodes;
}

std::optional<size_t> TreeModel::GetIndexOf(TreeModelNode* parent,
                                            TreeModelNode* child) const {
  CHECK(parent);
  return parent->GetIndexOf(child);
}

TreeModelNode* TreeModel::GetParent(TreeModelNode* node) const {
  CHECK(node);
  return node->parent();
}

void TreeModel::AddObserver(TreeModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TreeModel::RemoveObserver(TreeModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void TreeModel::SetTitle(TreeModelNode* node, const std::u16string& title) {
  CHECK(node);
  node->SetTitle(title);
  NotifyObserverTreeNodeChanged(node);
}

void TreeModel::AddImpl(TreeModelNode* parent,
                        std::unique_ptr<TreeModelNode> node,
                        size_t index) {
  CHECK(parent);
  CHECK(node);
  parent->Add(std::move(node), index);
  NotifyObserverTreeNodeAdded(parent, index);
}

}  // namespace ui
