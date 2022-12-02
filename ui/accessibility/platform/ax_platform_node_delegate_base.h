// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

// Base implementation of AXPlatformNodeDelegate where all functions
// return a default value. Useful for classes that want to implement
// AXPlatformNodeDelegate but don't need to override much of its
// behavior.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeDelegateBase
    : public AXPlatformNodeDelegate {
 public:
  AXPlatformNodeDelegateBase();

  AXPlatformNodeDelegateBase(const AXPlatformNodeDelegateBase&) = delete;
  AXPlatformNodeDelegateBase& operator=(const AXPlatformNodeDelegateBase&) =
      delete;

  ~AXPlatformNodeDelegateBase() override;

  class ChildIteratorBase : public ChildIterator {
   public:
    ChildIteratorBase(AXPlatformNodeDelegateBase* parent, size_t index);
    ChildIteratorBase(const ChildIteratorBase& it);
    ~ChildIteratorBase() override = default;
    ChildIteratorBase& operator++() override;
    ChildIteratorBase& operator++(int) override;
    ChildIteratorBase& operator--() override;
    ChildIteratorBase& operator--(int) override;
    gfx::NativeViewAccessible GetNativeViewAccessible() const override;
    absl::optional<size_t> GetIndexInParent() const override;
    AXPlatformNodeDelegate& operator*() const override;
    AXPlatformNodeDelegate* operator->() const override;

   private:
    size_t index_;
    raw_ptr<AXPlatformNodeDelegateBase> parent_;
  };
  std::unique_ptr<AXPlatformNodeDelegate::ChildIterator> ChildrenBegin()
      override;
  std::unique_ptr<AXPlatformNodeDelegate::ChildIterator> ChildrenEnd() override;

  // Get another node from this same tree.
  AXPlatformNode* GetFromNodeID(int32_t id) override;

  // Get a node from a different tree using a tree ID and node ID.
  // Note that this is only guaranteed to work if the other tree is of the
  // same type, i.e. it won't work between web and views or vice-versa.
  AXPlatformNode* GetFromTreeIDAndNodeID(const ui::AXTreeID& ax_tree_id,
                                         int32_t id) override;

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a target nodes for which this delegate's node has that relationship
  // attribute or NULL if there is no such relationship.
  AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr) override;

  // Given a node ID attribute (one where IsNodeIdIntListAttribute is true),
  // return a vector of all target nodes for which this delegate's node has that
  // relationship attribute.
  std::vector<AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr) override;

  const std::vector<gfx::NativeViewAccessible> GetUIADirectChildrenInRange(
      ui::AXPlatformNodeDelegate* start,
      ui::AXPlatformNodeDelegate* end) override;

  //
  // Tables. All of these should be called on a node that's a table-like
  // role, otherwise they return nullopt.
  //
  absl::optional<int> GetTableAriaColCount() const override;
  absl::optional<int> GetTableAriaRowCount() const override;
  AXPlatformNode* GetTableCaption() const override;

  // Table cell-like nodes.
  bool IsRootWebAreaForPresentationalIframe() const override;

  // Ordered-set-like and item-like nodes.
  absl::optional<int> GetPosInSet() const override;
  absl::optional<int> GetSetSize() const override;

  //
  // Events.
  //

  // Return the platform-native GUI object that should be used as a target
  // for accessibility events.
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  //
  // Actions.
  //

  // Perform an accessibility action, switching on the ax::mojom::Action
  // provided in |data|.
  bool AccessibilityPerformAction(const AXActionData& data) override;

  //
  // Localized strings.
  //

  std::u16string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  std::u16string GetLocalizedRoleDescriptionForUnlabeledImage() const override;
  std::u16string GetLocalizedStringForLandmarkType() const override;
  std::u16string GetLocalizedStringForRoleDescription() const override;
  std::u16string GetStyleNameAttributeAsLocalizedString() const override;

  //
  // Testing.
  //

  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element. The default value should be falses if not in testing mode.
  bool ShouldIgnoreHoveredStateForTesting() override;

 protected:
  std::string SubtreeToStringHelper(size_t level) override;

  // Given a list of node ids, return the nodes in this delegate's tree to
  // which they correspond.
  std::set<ui::AXPlatformNode*> GetNodesForNodeIds(
      const std::set<int32_t>& ids);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
