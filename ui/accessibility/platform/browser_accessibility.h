// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_H_

#include <stdint.h>

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node_id.h"
#include "ui/accessibility/platform/child_iterator.h"
#include "ui/base/buildflags.h"

#if BUILDFLAG(IS_MAC) && __OBJC__
@class BrowserAccessibilityCocoa;
#endif
namespace content {
class DumpAccessibilityTestBase;
}
namespace ui {
class AXPlatformNode;
class BrowserAccessibilityManager;
// A `BrowserAccessibility` object represents one node in the accessibility tree
// on the browser side. It wraps an `AXNode` and assists in exposing
// web-specific information from the node. It's owned by a
// `BrowserAccessibilityManager`.
//
// There are subclasses of BrowserAccessibility for each platform where we
// implement some of the native accessibility APIs that are only specific to the
// Web.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibility
    : public AXPlatformNodeDelegate {
 public:
  // Creates a platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  static std::unique_ptr<BrowserAccessibility> Create(
      BrowserAccessibilityManager* manager,
      AXNode* node);

  // Returns |delegate| as a BrowserAccessibility object, if |delegate| is
  // non-null and an object in the BrowserAccessibility class hierarchy.
  static BrowserAccessibility* FromAXPlatformNodeDelegate(
      AXPlatformNodeDelegate* delegate);

  ~BrowserAccessibility() override;
  BrowserAccessibility(const BrowserAccessibility&) = delete;
  BrowserAccessibility& operator=(const BrowserAccessibility&) = delete;

  // Called after the object is first initialized and again every time
  // its data changes.
  virtual void OnDataChanged();

  // Called when the location changed.
  virtual void OnLocationChanged() {}

  // This is called when the platform-specific attributes for a node need
  // to be recomputed, which may involve firing native events, due to a
  // change other than an update from OnAccessibilityEvents.
  virtual void UpdatePlatformAttributes() {}

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(const BrowserAccessibility* ancestor) const;

  bool IsIgnoredForTextNavigation() const;

  bool IsLineBreakObject() const;

  // Returns true if this object can fire events.
  virtual bool CanFireEvents() const;

  // Return the AXPlatformNode corresponding to this node, if applicable
  // on this platform.
  virtual AXPlatformNode* GetAXPlatformNode() const;

  // Returns the number of children of this object, or 0 if PlatformIsLeaf()
  // returns true.
  virtual size_t PlatformChildCount() const;

  // Return a pointer to the child at the given index, or NULL for an
  // invalid index. Returns nullptr if PlatformIsLeaf() returns true.
  virtual BrowserAccessibility* PlatformGetChild(size_t child_index) const;

  BrowserAccessibility* PlatformGetParent() const;

  // The following methods are virtual so that they can be overridden on Mac to
  // take into account the "extra Mac nodes".
  //
  // TODO(nektar): Refactor `AXNode` so that it can handle "extra Mac nodes"
  // itself when using any of its tree traversal methods.
  virtual BrowserAccessibility* PlatformGetFirstChild() const;
  virtual BrowserAccessibility* PlatformGetLastChild() const;
  virtual BrowserAccessibility* PlatformGetNextSibling() const;
  virtual BrowserAccessibility* PlatformGetPreviousSibling() const;

  // Iterator over platform children.
  class COMPONENT_EXPORT(AX_PLATFORM) PlatformChildIterator
      : public ChildIterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = int;
    using value_type = BrowserAccessibility;
    using pointer = BrowserAccessibility*;
    using reference = BrowserAccessibility&;

    PlatformChildIterator(const BrowserAccessibility* parent,
                          BrowserAccessibility* child);
    PlatformChildIterator(const PlatformChildIterator& it);
    ~PlatformChildIterator() override;
    PlatformChildIterator& operator++() override;
    // Postfix increment/decrement can't be overrides. See comment in
    // child_iterator.h
    PlatformChildIterator operator++(int);
    PlatformChildIterator& operator--() override;
    PlatformChildIterator operator--(int);
    gfx::NativeViewAccessible GetNativeViewAccessible() const override;
    BrowserAccessibility* get() const override;
    std::optional<size_t> GetIndexInParent() const override;
    BrowserAccessibility& operator*() const override;
    BrowserAccessibility* operator->() const override;

   private:
    raw_ptr<const BrowserAccessibility> parent_;
    AXNode::ChildIteratorBase<BrowserAccessibility,
                              &BrowserAccessibility::PlatformGetNextSibling,
                              &BrowserAccessibility::PlatformGetPreviousSibling,
                              &BrowserAccessibility::PlatformGetFirstChild,
                              &BrowserAccessibility::PlatformGetLastChild>
        platform_iterator;
  };

  // C++ range implementation for platform children, see PlatformChildren().
  class PlatformChildrenRange {
   public:
    explicit PlatformChildrenRange(const BrowserAccessibility* parent)
        : parent_(parent) {}
    PlatformChildrenRange(const PlatformChildrenRange&) = default;

    PlatformChildIterator begin() { return parent_->PlatformChildrenBegin(); }
    PlatformChildIterator end() { return parent_->PlatformChildrenEnd(); }

    std::reverse_iterator<PlatformChildIterator> rbegin() {
      return std::reverse_iterator(parent_->PlatformChildrenEnd());
    }
    std::reverse_iterator<PlatformChildIterator> rend() {
      return std::reverse_iterator(parent_->PlatformChildrenBegin());
    }

   private:
    const raw_ptr<const BrowserAccessibility> parent_;
  };

  // Returns a range for platform children which can be used in range-based for
  // loops, for example, for (const auto& child : PlatformChildren()) {}.
  PlatformChildrenRange PlatformChildren() const {
    return PlatformChildrenRange(this);
  }

  PlatformChildIterator PlatformChildrenBegin() const;
  PlatformChildIterator PlatformChildrenEnd() const;

  // If this object is exposed to the platform's accessibility layer, returns
  // this object. Otherwise, returns the lowest ancestor that is exposed to the
  // platform.
  virtual BrowserAccessibility* PlatformGetLowestPlatformAncestor() const;

  // If this node is within an editable region, such as a content editable,
  // returns the node that is at the root of that editable region, otherwise
  // returns nullptr. In accessibility, an editable region includes all types of
  // text fields, (see `AXNodeData::IsTextField()`).
  BrowserAccessibility* PlatformGetTextFieldAncestor() const;

  // If this node is within a container (or widget) that supports either single
  // or multiple selection, returns the node that represents the container.
  BrowserAccessibility* PlatformGetSelectionContainer() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestLastChild() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestLastChild() const;

  // Range implementation for all children traversal see AllChildren().
  class AllChildrenRange final {
   public:
    explicit AllChildrenRange(const BrowserAccessibility* parent)
        : parent_(parent),
          child_tree_root_(parent->PlatformGetRootOfChildTree()) {}
    AllChildrenRange(const AllChildrenRange&) = default;

    class COMPONENT_EXPORT(AX_PLATFORM) Iterator final {
     public:
      using iterator_category = std::input_iterator_tag;
      using value_type = BrowserAccessibility*;
      using difference_type = std::ptrdiff_t;
      using pointer = BrowserAccessibility**;
      using reference = BrowserAccessibility*&;

      Iterator(const BrowserAccessibility* parent,
               const BrowserAccessibility* child_tree_root,
               unsigned int index = 0U)
          : parent_(parent), child_tree_root_(child_tree_root), index_(index) {}
      Iterator(const Iterator&) = default;
      ~Iterator() = default;

      Iterator& operator++() {
        ++index_;
        return *this;
      }
      Iterator operator++(int) {
        Iterator tmp(*this);
        operator++();
        return tmp;
      }
      bool operator==(const Iterator& rhs) const {
        return parent_ == rhs.parent_ && index_ == rhs.index_;
      }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      const BrowserAccessibility* operator*();

     private:
      const raw_ptr<const BrowserAccessibility> parent_;
      const raw_ptr<const BrowserAccessibility> child_tree_root_;
      unsigned int index_;
    };

    Iterator begin() { return {parent_, child_tree_root_}; }
    Iterator end() {
      unsigned int count =
          child_tree_root_ ? 1U : parent_->node()->children().size();
      return {parent_, child_tree_root_, count};
    }

   private:
    const raw_ptr<const BrowserAccessibility, DanglingUntriaged> parent_;
    const raw_ptr<const BrowserAccessibility, DanglingUntriaged>
        child_tree_root_;
  };

  // Returns a range for all children including ignored children, which can be
  // used in range-based for loops, for example,
  // for (const auto& child : AllChildren()) {}.
  AllChildrenRange AllChildren() const { return AllChildrenRange(this); }

  // Derivative utils for AXPlatformNodeDelegate::GetInnerTextRangeBoundsRect
  gfx::Rect GetUnclippedRootFrameInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // DEPRECATED: Prefer using the interfaces provided by AXPlatformNodeDelegate
  // when writing new code.
  gfx::Rect GetScreenHypertextRangeBoundsRect(
      int start,
      int len,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // Returns the bounds of the given range in coordinates relative to the
  // top-left corner of the overall web area. Only valid when the role is
  // WebAXRoleStaticText.
  // DEPRECATED (for public use): Prefer using the interfaces provided by
  // AXPlatformNodeDelegate when writing new non-private code.
  gfx::Rect GetRootFrameHypertextRangeBoundsRect(
      int start,
      int len,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // This is an approximate hit test that only uses the information in
  // the browser process to compute the correct result. It will not return
  // correct results in many cases of z-index, overflow, and absolute
  // positioning, so BrowserAccessibilityManager::CachingAsyncHitTest
  // should be used instead, which falls back on calling ApproximateHitTest
  // automatically.
  //
  // Note that unlike BrowserAccessibilityManager::CachingAsyncHitTest, this
  // method takes a parameter in Blink's definition of screen coordinates.
  // This is so that the scale factor is consistent with what we receive from
  // Blink and store in the AX tree.
  // Blink screen coordinates are 1:1 with physical pixels if use-zoom-for-dsf
  // is disabled; they're physical pixels divided by device scale factor if
  // use-zoom-for-dsf is disabled. For more information see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  BrowserAccessibility* ApproximateHitTest(
      const gfx::Point& blink_screen_point);

  //
  // Accessors and simple setters.
  //

  BrowserAccessibilityManager* manager() const { return manager_; }

  // These access the internal unignored accessibility tree, which doesn't
  // necessarily reflect the accessibility tree that should be exposed on each
  // platform. Use PlatformChildCount and PlatformGetChild to implement platform
  // accessibility APIs.
  size_t InternalChildCount() const;
  BrowserAccessibility* InternalGetChild(size_t child_index) const;
  BrowserAccessibility* InternalGetParent() const;
  BrowserAccessibility* InternalGetFirstChild() const;
  BrowserAccessibility* InternalGetLastChild() const;
  BrowserAccessibility* InternalGetNextSibling() const;
  BrowserAccessibility* InternalGetPreviousSibling() const;
  using InternalChildIterator = AXNode::ChildIteratorBase<
      BrowserAccessibility,
      &BrowserAccessibility::InternalGetNextSibling,
      &BrowserAccessibility::InternalGetPreviousSibling,
      &BrowserAccessibility::InternalGetFirstChild,
      &BrowserAccessibility::InternalGetLastChild>;
  InternalChildIterator InternalChildrenBegin() const;
  InternalChildIterator InternalChildrenEnd() const;

  gfx::RectF GetLocation() const;

  // See AXNodeData::IsClickable().
  virtual bool IsClickable() const;

  // See AXNodeData::IsTextField().
  bool IsTextField() const;

  // See AXNodeData::IsPasswordField().
  bool IsPasswordField() const;

  // See AXNodeData::IsAtomicTextField().
  bool IsAtomicTextField() const;

  // See AXNodeData::IsNonAtomicTextField().
  bool IsNonAtomicTextField() const;

  // Returns true if the accessible name was explicitly set to "" by the author
  bool HasExplicitlyEmptyName() const;

  // |offset| could only be a character offset. Depending on the platform, the
  // character offset could be either in the object's text content (Android and
  // Mac), or an offset in the object's hypertext (Linux ATK and Windows IA2).
  // Converts to a leaf text position if you pass a character offset on a
  // non-leaf node.
  AXPosition CreatePositionForSelectionAt(int offset) const;

  std::u16string GetNameAsString16() const;

  // `AXPlatformNodeDelegate` implementation.
  gfx::NativeViewAccessible GetParent() const override;
  size_t GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(size_t index) const override;
  gfx::NativeViewAccessible GetFirstChild() const override;
  gfx::NativeViewAccessible GetLastChild() const override;
  gfx::NativeViewAccessible GetNextSibling() const override;
  gfx::NativeViewAccessible GetPreviousSibling() const override;
  bool IsPlatformDocument() const override;
  bool IsLeaf() const override;
  bool IsFocused() const override;
  gfx::NativeViewAccessible GetLowestPlatformAncestor() const override;
  gfx::NativeViewAccessible GetTextFieldAncestor() const override;
  gfx::NativeViewAccessible GetSelectionContainer() const override;
  gfx::NativeViewAccessible GetTableAncestor() const override;

  std::unique_ptr<ChildIterator> ChildrenBegin() const override;
  std::unique_ptr<ChildIterator> ChildrenEnd() const override;

  bool SetHypertextSelection(int start_offset, int end_offset) override;
  gfx::Rect GetBoundsRect(
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::NativeViewAccessible HitTestSync(int physical_pixel_x,
                                        int physical_pixel_y) const override;
  gfx::NativeViewAccessible GetFocus() const override;
  AXPlatformNode* GetFromNodeID(int32_t id) override;
  AXPlatformNode* GetFromTreeIDAndNodeID(const AXTreeID& ax_tree_id,
                                         int32_t id) override;
  std::optional<size_t> GetIndexInParent() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  const std::vector<gfx::NativeViewAccessible> GetUIADirectChildrenInRange(
      AXPlatformNodeDelegate* start,
      AXPlatformNodeDelegate* end) override;

  AXPlatformNode* GetTableCaption() const override;

  bool AccessibilityPerformAction(const AXActionData& data) override;

// TODO(https://crbug.com/358567091): Move this logic outside of
// BrowserAccessibility to avoid platform-specific code in the base class.
#if !BUILDFLAG(IS_FUCHSIA)
  virtual std::u16string GetLocalizedString(int message_id) const;
  std::u16string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  std::u16string GetLocalizedRoleDescriptionForUnlabeledImage() const override;
  std::u16string GetLocalizedStringForLandmarkType() const override;
  std::u16string GetLocalizedStringForRoleDescription() const override;
  std::u16string GetStyleNameAttributeAsLocalizedString() const override;
#endif  // !BUILDFLAG(IS_FUCHSIA)

  TextAttributeMap ComputeTextAttributeMap(
      const TextAttributeList& default_attributes) const override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  bool IsWebContent() const override;
  bool HasVisibleCaretOrSelection() const override;
  std::vector<AXPlatformNode*> GetSourceNodesForReverseRelations(
      ax::mojom::IntAttribute attr) override;
  std::vector<AXPlatformNode*> GetSourceNodesForReverseRelations(
      ax::mojom::IntListAttribute attr) override;
  std::optional<int> GetPosInSet() const override;
  std::optional<int> GetSetSize() const override;

  // Returns true if this node is a list marker or if it's a descendant
  // of a list marker node. Returns false otherwise.
  bool IsInListMarker() const;

  // Returns the popup button ancestor of this current node if any. The popup
  // button needs to be the parent of a menu list popup and needs to be
  // collapsed.
  BrowserAccessibility* GetCollapsedMenuListSelectAncestor() const;

  // Returns true if:
  // 1. This node is a list, AND
  // 2. This node has a list ancestor or a list descendant.
  bool IsHierarchicalList() const;

  // Returns a string representation of this object for debugging purposes.
  std::string ToString() const;

 protected:
  BrowserAccessibility(BrowserAccessibilityManager* manager, AXNode* node);

  virtual TextAttributeList ComputeTextAttributes() const;

  // The manager of this tree of accessibility objects. Weak, owns us.
  const raw_ptr<BrowserAccessibilityManager> manager_;

  // A unique ID, needed by some platform APIs, since node IDs are frame-local.
  // Protected so that it can't be called directly on a BrowserAccessibility
  // where it could be confused with an id that comes from the node data,
  // which is only unique to the Blink process.
  // Does need to be called by subclasses such as BrowserAccessibilityAndroid.
  AXPlatformNodeId GetUniqueId() const override;

  // Returns a text attribute map indicating the offsets in the text of a leaf
  // object, such as a text field or static text, where spelling and grammar
  // errors are present.
  TextAttributeMap GetSpellingAndGrammarAttributes() const;

  std::string SubtreeToStringHelper(size_t level) override;

  void NotifyAccessibilityApiUsage() const override;

  // The UIA tree formatter needs access to GetUniqueId() to identify the
  // starting point for tree dumps.
  friend class AccessibilityTreeFormatterUia;

  // DumpAccessibilityTestBase needs to be able to set
  // ignore_hovered_state_for_testing_ to avoid flaky tests.
  friend class content::DumpAccessibilityTestBase;

 private:
  // Return the bounds after converting from this node's coordinate system
  // (which is relative to its nearest scrollable ancestor) to the coordinate
  // system specified. If the clipping behavior is set to clipped, clipping is
  // applied to all bounding boxes so that the resulting rect is within the
  // window. If the clipping behavior is unclipped, the resulting rect may be
  // outside of the window or offscreen. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  gfx::Rect RelativeToAbsoluteBounds(gfx::RectF bounds,
                                     const AXCoordinateSystem coordinate_system,
                                     const AXClippingBehavior clipping_behavior,
                                     AXOffscreenResult* offscreen_result) const;

  // Return a rect for a 1-width character past the end of text. This is what
  // ATs expect when getting the character extents past the last character in
  // a line, and equals what the caret bounds would be when past the end of
  // the text.
  gfx::Rect GetRootFrameHypertextBoundsPastEndOfText(
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result = nullptr) const;

  // See `AXNode::GetTextContentRangeBoundsUTF16`.
  gfx::RectF GetTextContentRangeBoundsUTF16(int start_offset,
                                            int end_offset) const;

  // Recursive helper function for GetInnerTextRangeBounds.
  gfx::Rect GetInnerTextRangeBoundsRectInSubtree(
      const int start_offset,
      const int end_offset,
      const AXCoordinateSystem coordinate_system,
      const AXClippingBehavior clipping_behavior,
      AXOffscreenResult* offscreen_result) const;

  // If the node has a child tree, get the root node.
  BrowserAccessibility* PlatformGetRootOfChildTree() const;

  // Determines whether this object is valid.
  bool IsValid() const;

  // Given a set of map of spelling text attributes and a start offset, merge
  // them into the given map of existing text attributes. Merges the given
  // spelling attributes, i.e. document marker information, into the given
  // text attributes starting at the given character offset. This is required
  // because document markers that are present on text leaves need to be
  // propagated to their parent object for compatibility with Firefox.
  static void MergeSpellingAndGrammarIntoTextAttributes(
      const TextAttributeMap& spelling_attributes,
      int start_offset,
      TextAttributeMap* text_attributes);

  // Return true is the list of text attributes already includes an invalid
  // attribute originating from ARIA.
  static bool HasInvalidAttribute(const TextAttributeList& attributes);

  // Disable hover state - needed just to avoid flaky tests.
  // Accessibility objects can have the "hot tracked" state set when
  // the mouse is hovering over them, but this makes tests flaky because
  // the test behaves differently when the mouse happens to be over an
  // element.  This is a global switch to not use the "hot tracked" state
  // in a test.
  static bool ignore_hovered_state_for_testing_;

  // The node's unique identifier as chosen by the node's manager. The value is
  // computed on first use.
  mutable AXPlatformNodeId unique_id_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_H_
