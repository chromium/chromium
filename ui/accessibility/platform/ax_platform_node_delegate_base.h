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

  // `AXPlatformNodeDelegate` implementation.
  const AXNodeData& GetData() const override;
  std::u16string GetTextContentUTF16() const override;
  std::u16string GetValueForControl() const override;

  AXNodePosition::AXPositionInstance CreatePositionAt(
      int offset,
      ax::mojom::TextAffinity affinity) const override;

  AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset,
      ax::mojom::TextAffinity affinity) const override;

  // See comments in AXPlatformNodeDelegate.
  gfx::NativeViewAccessible GetNSWindow() override;

  // Get the node for this delegate, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // Get the parent of the node, which may be an AXPlatformNode or it may
  // be a native accessible object implemented by another class.
  gfx::NativeViewAccessible GetParent() const override;

  // Get the index in parent. Typically this is the AXNode's index_in_parent_.
  absl::optional<size_t> GetIndexInParent() override;

  // Get the number of children of this node.
  size_t GetChildCount() const override;

  // Get the child of a node given a 0-based index.
  gfx::NativeViewAccessible ChildAtIndex(size_t index) override;

  // Returns true if it has a modal dialog.
  bool HasModalDialog() const override;

  gfx::NativeViewAccessible GetFirstChild() override;
  gfx::NativeViewAccessible GetLastChild() override;
  gfx::NativeViewAccessible GetNextSibling() override;
  gfx::NativeViewAccessible GetPreviousSibling() override;

  bool IsChildOfLeaf() const override;
  bool IsDescendantOfAtomicTextField() const override;
  bool IsPlatformDocument() const override;
  bool IsFocused() const override;
  bool IsToplevelBrowserWindow() override;
  gfx::NativeViewAccessible GetLowestPlatformAncestor() const override;
  gfx::NativeViewAccessible GetTextFieldAncestor() const override;
  gfx::NativeViewAccessible GetSelectionContainer() const override;
  gfx::NativeViewAccessible GetTableAncestor() const override;

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

  const std::string& GetDescription() const override;
  std::u16string GetHypertext() const override;
  const std::map<int, int>& GetHypertextOffsetToHyperlinkChildIndex()
      const override;
  bool SetHypertextSelection(int start_offset, int end_offset) override;
  TextAttributeMap ComputeTextAttributeMap(
      const TextAttributeList& default_attributes) const override;
  std::string GetInheritedFontFamilyName() const override;

  gfx::Rect GetBoundsRect(const AXCoordinateSystem coordinate_system,
                          const AXClippingBehavior clipping_behavior,
                          AXOffscreenResult* offscreen_result) const override;

  gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;

  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const override;

  // Do a *synchronous* hit test of the given location in global screen physical
  // pixel coordinates, and the node within this node's subtree (inclusive)
  // that's hit, if any.
  //
  // If the result is anything other than this object or NULL, it will be
  // hit tested again recursively - that allows hit testing to work across
  // implementation classes. It's okay to take advantage of this and return
  // only an immediate child and not the deepest descendant.
  //
  // This function is mainly used by accessibility debugging software.
  // Platforms with touch accessibility use a different asynchronous interface.
  gfx::NativeViewAccessible HitTestSync(
      int screen_physical_pixel_x,
      int screen_physical_pixel_y) const override;

  // Return the node within this node's subtree (inclusive) that currently
  // has focus.
  gfx::NativeViewAccessible GetFocus() const override;

  // Get whether this node is offscreen.
  bool IsOffscreen() const override;

  // Returns true if this node is ignored.
  bool IsIgnored() const override;

  // Get whether this node is a minimized window.
  bool IsMinimized() const override;
  bool IsText() const override;

  // Get whether this node is in web content.
  bool IsWebContent() const override;

  // Returns true if the caret or selection is visible on this object.
  bool HasVisibleCaretOrSelection() const override;

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

  // Given a node ID attribute (one where IsNodeIdIntAttribute is true), return
  // a set of all source nodes that have that relationship attribute between
  // them and this delegate's node.
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) override;

  // Given a node ID list attribute (one where IsNodeIdIntListAttribute is
  // true) return a set of all source nodes that have that relationship
  // attribute between them and this delegate's node.
  std::set<AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) override;

  std::u16string GetAuthorUniqueId() const override;

  const AXUniqueId& GetUniqueId() const override;

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

  AXPlatformNodeDelegate* GetParentDelegate() const;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_DELEGATE_BASE_H_
