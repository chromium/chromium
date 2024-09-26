// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_ACCESSIBILITY_AX_POSITION_H_
#define UI_ACCESSIBILITY_AX_POSITION_H_

#include <math.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/stack.h"
#include "base/export_template.h"
#include "base/i18n/break_iterator.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_text_attributes.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/gfx/utf16_indexing.h"

namespace ui {

class AXNodePosition;
class AXNode;

// Defines the type of position in the accessibility tree.
// A tree position is used when referring to a specific child of a node in the
// accessibility tree.
// A text position is used when referring to a specific character of text inside
// a particular node.
// A null position is used to signify that the provided data is invalid or that
// a boundary has been reached.
enum class AXPositionKind { NULL_POSITION, TREE_POSITION, TEXT_POSITION };

// Defines how creating the next or previous position should behave whenever we
// are at or are crossing a text boundary, (such as the start of a word or the
// end of a sentence), or whenever we are crossing the initial position's
// anchor. Note that the "anchor" is the node to which an AXPosition is attached
// to. It is provided when a position is created.
enum class AXBoundaryBehavior {
  // Crosses all boundaries. If the bounds of the current window-like container,
  // such as the current webpage, have been reached, returns a null position.
  kCrossBoundary,
  // Stops if the current anchor is crossed, regardless of how the resulting
  // position has been computed. For example, even though in order to find the
  // next or previous word start in a text field we need to descend to the leaf
  // equivalent position, this behavior will only stop when the bounds of the
  // original anchor, i.e. the text field, have been crossed.
  kStopAtAnchorBoundary,
  // Stops if we have reached the start or the end of of a window-like
  // container, such as a webpage, a PDF, a dialog, the browser's UI (AKA
  // Views), or the whole desktop.
  kStopAtLastAnchorBoundary
};

// Defines whether moving to the next or previous position should consider the
// initial position before testing for the given boundary/behavior.
// kCheckInitialPosition should be used if the current position should be
// maintained if it meets the boundary criteria. Otherwise,
// kDontCheckInitialPosition will move to the next/previous position before
// testing for the specified boundary.
enum class AXBoundaryDetection {
  kCheckInitialPosition,
  kDontCheckInitialPosition,
};

struct AXMovementOptions {
  AXMovementOptions(AXBoundaryBehavior boundary, AXBoundaryDetection detection)
      : boundary_behavior(boundary), boundary_detection(detection) {}

  AXBoundaryBehavior boundary_behavior;
  AXBoundaryDetection boundary_detection;

  // If true, indicates that an upstream position should not be crossed when
  // moving forward and should skip its initial check when moving backward. This
  // primarily applies to getting a pair of positions around a line from an
  // upstream caret.
  bool upstream_bounded = false;
};

// Describes in further detail what type of boundary a current position is on.
//
// For complex boundaries such as format boundaries, it can be useful to know
// why a particular boundary was chosen.
enum class AXBoundaryType {
  // Not at a unit boundary.
  kNone,
  // At a unit boundary (e.g. a format boundary).
  kUnitBoundary,
  // At the start of the whole content, possibly spanning multiple accessibility
  // trees.
  kContentStart,
  // At the end of the whole content, possibly spanning multiple accessibility
  // trees.
  kContentEnd
};

// When converting to an unignored position, determines how to adjust the new
// position in order to make it valid, either moving backward or forward in
// the accessibility tree.
enum class AXPositionAdjustmentBehavior { kMoveBackward, kMoveForward };

// Specifies how AXPosition::ExpandToEnclosingTextBoundary behaves.
//
// As an example, imagine we have the text "hello world" and a position before
// the space character. We want to expand to the surrounding word boundary.
// Since we are right at the end of the first word, we could either expand to
// the left first, find the start of the first word and then use that to find
// the corresponding word end, resulting in the word "Hello". Another
// possibility is to expand to the right first, find the end of the next word
// and use that as our starting point to find the previous word start, resulting
// in the word "world".
enum class AXRangeExpandBehavior {
  // Expands to the left boundary first and then uses that position as the
  // starting point to find the boundary to the right.
  kLeftFirst,
  // Expands to the right boundary first and then uses that position as the
  // starting point to find the boundary to the left.
  kRightFirst
};

// Some platforms require most objects, including empty objects, to be
// represented by an "embedded object character" in order for text navigation to
// work correctly. This enum controls whether a replacement character will be
// exposed for such objects.
//
// When an embedded object is replaced by this special character, the
// expectations are the same with this character as with other ordinary
// characters.
//
// For example, with UIA on Windows, we need to be able to navigate inside and
// outside of this character as if it was an ordinary character, using the
// `AXPlatformNodeTextRangeProvider` methods. Since an "embedded object
// character" is the only character in a node, we also treat this character as a
// word.
//
// However, there is a special case for UIA. kExposeCharacterForHypertext is
// used mainly to enable the hypertext logic and calculation for cases where the
// embedded object character is not needed. This logic is IA2 and ATK specific,
// and should not be used for UIA relevant calls and calculations. As a result,
// we have the kUIAExposeCharacterForTextContent which avoids the IA2/ATK
// specific logic for the text calculation but also keeps the same embedded
// object character behavior for cases when it is needed.
enum class AXEmbeddedObjectBehavior {
  kExposeCharacterForHypertext,
  kSuppressCharacter,
  kUIAExposeCharacterForTextContent,
};

// Controls whether embedded objects are represented by a replacement
// character. This is initialized to a per-platform default but can be
// overridden for testing.
//
// On some platforms, most objects are represented in the text of their parents
// with a special "embedded object character" and not with their actual text
// contents. Also on the same platforms, if a node has only ignored descendants,
// i.e., it appears to be empty to assistive software, we need to treat it as a
// character and a word boundary. For example, an empty text field should act as
// a character and a word boundary when a screen reader user tries to navigate
// through it, otherwise the text field would be missed by the user.
//
// Tests should use ScopedAXEmbeddedObjectBehaviorSetter to change this.
// TODO(crbug.com/40764129) Don't export this so tests can't change it.
extern AX_EXPORT AXEmbeddedObjectBehavior g_ax_embedded_object_behavior;

class AX_EXPORT ScopedAXEmbeddedObjectBehaviorSetter {
 public:
  explicit ScopedAXEmbeddedObjectBehaviorSetter(
      AXEmbeddedObjectBehavior behavior);
  ~ScopedAXEmbeddedObjectBehaviorSetter();

 private:
  AXEmbeddedObjectBehavior prev_behavior_;
};

// Forward declarations.
template <class AXPositionType, class AXNodeType>
class AXPosition;
template <class AXPositionType>
class AXRange;
template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);
template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second);

// A position in the accessibility tree.
//
// This class could either represent a tree position or a text position.
// Tree positions point to either a child of a specific node or at the end of a
// node (i.e. an "after children" position).
// Text positions point to either a character offset in the text inside a
// particular node including text from all its children, or to the end of the
// node's text, (i.e. an "after text" position).
// On tree positions that have a leaf node as their anchor, we also need to
// distinguish between "before text" and "after text" positions. To do this, if
// the child index is 0 and the anchor is a leaf node, then it's an "after text"
// position. If the child index is |BEFORE_TEXT| and the anchor is a leaf node,
// then this is a "before text" position.
// It doesn't make sense to have a "before text" position on a text position,
// because it is identical to setting its offset to the first character.
//
// To avoid re-computing either the text offset or the child index when
// converting between the two types of positions, both values are saved after
// the first conversion.
//
// This class template uses static polymorphism in order to allow sub-classes to
// be created from the base class without the base class knowing the type of the
// sub-class in advance.
// The template argument |AXPositionType| should always be set to the type of
// any class that inherits from this template, making this a
// "curiously recursive template".
//
// This class can be copied using the |Clone| method. It is designed to be
// immutable.
template <class AXPositionType, class AXNodeType>
class AXPosition {
 public:
  using AXPositionInstance =
      std::unique_ptr<AXPosition<AXPositionType, AXNodeType>>;

  using AXRangeType = AXRange<AXPosition<AXPositionType, AXNodeType>>;

  using BoundaryConditionPredicate =
      base::RepeatingCallback<bool(const AXPositionInstance&)>;

  using BoundaryTextOffsetsFunc =
      base::RepeatingCallback<const std::vector<int32_t>&(
          const AXPositionInstance&)>;

  static const int BEFORE_TEXT = -1;
  static const int INVALID_INDEX = -2;
  static const int INVALID_OFFSET = -1;

  static AXPositionInstance CreateNullPosition() {
    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::NULL_POSITION, AXTreeIDUnknown(),
                             kInvalidAXNodeID, INVALID_INDEX, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTreePosition(const AXNode& anchor,
                                               int child_index) {
    DCHECK(anchor.tree());
    DCHECK_NE(anchor.tree()->GetAXTreeID(), AXTreeIDUnknown());
    DCHECK_NE(anchor.id(), kInvalidAXNodeID);

    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TREE_POSITION,
                             anchor.tree()->GetAXTreeID(), anchor.id(),
                             child_index, INVALID_OFFSET,
                             ax::mojom::TextAffinity::kDownstream);
    return new_position;
  }

  static AXPositionInstance CreateTreePositionAtStartOfAnchor(
      const AXNode& anchor) {
    // Initialize the child index:
    // - For a leaf, the child index will be BEFORE_TEXT.
    // - Otherwise the child index will be 0.
    int child_index = IsLeafNodeForTreePosition(anchor) ? BEFORE_TEXT : 0;
    return CreateTreePosition(anchor, child_index);
  }

  static AXPositionInstance CreateTreePositionAtEndOfAnchor(
      const AXNode& anchor) {
    // Initialize the child index to the anchor's child count.
    return CreateTreePosition(anchor,
                              anchor.GetChildCountCrossingTreeBoundary());
  }

  static AXPositionInstance CreateTextPosition(
      const AXNode& anchor,
      int text_offset,
      ax::mojom::TextAffinity affinity) {
    DCHECK(anchor.tree());
    DCHECK_NE(anchor.tree()->GetAXTreeID(), AXTreeIDUnknown());
    DCHECK_NE(anchor.id(), kInvalidAXNodeID);

    AXPositionInstance new_position(new AXPositionType());
    new_position->Initialize(AXPositionKind::TEXT_POSITION,
                             anchor.tree()->GetAXTreeID(), anchor.id(),
                             INVALID_INDEX, text_offset, affinity);
    return new_position;
  }

  virtual ~AXPosition() = default;

  // Implemented based on the copy and swap idiom.
  AXPosition& operator=(const AXPosition& other) {
    AXPositionInstance clone = other.Clone();
    swap(*clone);
    return *this;
  }

  virtual AXPositionInstance Clone() const = 0;

  AXPositionInstance CloneWithDownstreamAffinity() const {
    if (!IsTextPosition()) {
      NOTREACHED_IN_MIGRATION() << "Only text positions have affinity.";
      return CreateNullPosition();
    }

    AXPositionInstance clone_with_downstream_affinity = Clone();
    clone_with_downstream_affinity->affinity_ =
        ax::mojom::TextAffinity::kDownstream;
    return clone_with_downstream_affinity;
  }

  AXPositionInstance CloneWithUpstreamAffinity() const {
    if (!IsTextPosition()) {
      NOTREACHED_IN_MIGRATION() << "Only text positions have affinity.";
      return CreateNullPosition();
    }

    AXPositionInstance clone_with_upstream_affinity = Clone();
    clone_with_upstream_affinity->affinity_ =
        ax::mojom::TextAffinity::kUpstream;
    return clone_with_upstream_affinity;
  }

  // A serialization of a position as POD. Not for sharing on disk or sharing
  // across thread or process boundaries, just for passing a position to an
  // API that works with positions as opaque objects.
  struct SerializedPosition {
    AXPositionKind kind;
    AXNodeID anchor_id;
    int child_index;
    int text_offset;
    ax::mojom::TextAffinity affinity;
    char tree_id[33];
  };

  static_assert(std::is_trivially_copyable<SerializedPosition>::value,
                "SerializedPosition must be POD");

  SerializedPosition Serialize() {
    SerializedPosition result;
    result.kind = kind_;

    // A tree ID can be serialized as a 32-byte string.
    std::string tree_id_string = tree_id_.ToString();
    DCHECK_LE(tree_id_string.size(), 32U);
    strncpy(result.tree_id, tree_id_string.c_str(), 32);
    result.tree_id[32] = 0;

    result.anchor_id = anchor_id_;
    result.child_index = child_index_;
    result.text_offset = text_offset_;
    result.affinity = affinity_;
    return result;
  }

  static AXPositionInstance Unserialize(
      const SerializedPosition& serialization) {
    AXPositionInstance new_position(new AXPositionType());
    // Use initialize without validation because this is used by ATs that
    // used outdated information to generated a selection request.
    new_position->InitializeWithoutValidation(
        serialization.kind, AXTreeID::FromString(serialization.tree_id),
        serialization.anchor_id, serialization.child_index,
        serialization.text_offset, serialization.affinity);
    return new_position;
  }

  std::string ToString() const {
    std::string str;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return "NullPosition";
      case AXPositionKind::TREE_POSITION: {
        std::string str_child_index;
        if (child_index_ == BEFORE_TEXT) {
          str_child_index = "before_text";
        } else if (child_index_ == INVALID_INDEX) {
          str_child_index = "invalid";
        } else {
          str_child_index = base::NumberToString(child_index_);
        }
        str = "TreePosition tree_id=" + tree_id_.ToString() +
              " anchor_id=" + base::NumberToString(anchor_id_) +
              " child_index=" + str_child_index;
        break;
      }
      case AXPositionKind::TEXT_POSITION: {
        std::string str_text_offset;
        if (text_offset_ == INVALID_OFFSET) {
          str_text_offset = "invalid";
        } else {
          str_text_offset = base::NumberToString(text_offset_);
        }
        str = "TextPosition anchor_id=" + base::NumberToString(anchor_id_) +
              " text_offset=" + str_text_offset + " affinity=" +
              ui::ToString(static_cast<ax::mojom::TextAffinity>(affinity_));
        break;
      }
    }

    if (!IsTextPosition() || text_offset_ < 0 || text_offset_ > MaxTextOffset())
      return str;

    const std::u16string& text = GetText();
    DCHECK_GE(text_offset_, 0);
    const size_t max_text_offset = text.size();
    DCHECK_LE(text_offset_, static_cast<int>(max_text_offset)) << text;
    std::u16string annotated_text;
    if (text_offset_ == static_cast<int>(max_text_offset)) {
      annotated_text = text + u"<>";
    } else {
      // TODO(aleventhal) This extra casting is only necessary to satisfy a
      // compiler error that strangely occurs only when Initialize() contains
      // SnapToMaxTextOffsetIfBeyond().
      size_t unsigned_text_offset = static_cast<size_t>(text_offset_);
      annotated_text = text.substr(0, unsigned_text_offset) + u"<" +
                       text[unsigned_text_offset] + u">" +
                       text.substr(unsigned_text_offset + 1);
    }

    return str + " annotated_text=" + base::UTF16ToUTF8(annotated_text);
  }

  // Helper for logging the position, the AXTreeManager and the anchor node.
  std::string ToDebugString() const {
    std::ostringstream str;
    str << "* Position: " << ToString();
    if (GetAnchor()) {
      str << "\n* Anchor node: " << *GetAnchor();
      if (IsTreePosition()) {
        str << "\n* AnchorChildCount(): " << AnchorChildCount()
            << "\n* IsLeaf(): " << IsLeaf();
      } else {
        str << "\n* TextOffset: " << text_offset()
            << "\n* MaxTextOffset: " << MaxTextOffset();
      }
    }
    if (GetManager())
      str << "\n* Tree: " << GetManager()->ax_tree()->data().ToString();
    return str.str();
  }

  AXPositionKind kind() const { return kind_; }
  AXTreeID tree_id() const { return tree_id_; }
  AXNodeID anchor_id() const { return anchor_id_; }

  AXTreeManager* GetManager() const { return AXTreeManager::FromID(tree_id()); }

  // Returns true if this position is within an "empty object", i.e. within a
  // node that should contribute no text to the accessibility tree's text
  // representation. For example, returns true if this position is within an
  // empty control, such as an empty text field or (on Windows) a collapsed
  // popup menu. On some platforms, such nodes need to be represented by an
  // "object replacement character". This character is inserted purely for
  // navigational purposes. This is because empty controls still need to act as
  // a word and character boundary on those platforms.
  static bool IsEmptyObject(const AXNode& node) {
    // A collapsed popup button that contains a menu list popup (i.e, the exact
    // subtree representation we get from a collapsed <select> element on
    // Windows) should not expose its descendants even though they are not
    // ignored.
    if (node.IsCollapsedMenuListSelect())
      return true;

    // All anchor nodes that are empty leaf nodes should be treated as empty
    // objects. Empty leaf nodes are defined as nodes whose descendants are (A)
    // not exposed to any platform accessibility APIs and (B) do not contribute
    // any text to the tree's text representation. They may have unignored
    // descendants however. They do not have any text content, hence they are
    // empty from our perspective. For example, an empty text field may still
    // have an unignored generic container inside it.
    if (!node.IsEmptyLeaf())
      return false;

    // While atomic text fields from web content have a text node descendant,
    // atomic text fields from Views don't. Their text value is set in the value
    // attribute of the text field node directly.
    if (node.IsView() && node.data().IsAtomicTextField() &&
        !node.GetValueForControl().empty()) {
      return false;
    }

    // One exception to the above rule that all empty leaf nodes are empty
    // objects in AXPosition are <embed> and <object> elements that have
    // children. They should not be treated as empty objects even when their
    // descendants are all ignored so that text navigation won't stop on such
    // nodes.
    ax::mojom::Role role = node.GetRole();
    if ((role == ax::mojom::Role::kEmbeddedObject ||
         role == ax::mojom::Role::kPluginObject) &&
        node.GetChildCountCrossingTreeBoundary()) {
      return false;
    }

    // Nodes that are skipped during text navigation should also be "empty
    // objects".
    //
    // Note that nodes that are skipped during text navigation could still have
    // positions anchored to them, e.g. for determining if a paragraph boundary
    // should be reported before or after such a node. Descending into the
    // children of such objects could add unnecessary extra text boundaries.
    if (node.IsIgnoredForTextNavigation())
      return true;

    // Another exception to the rule that all leaf nodes in the accessibility
    // tree should be "empty objects" are kRootWebArea, kPdfRoot, kIframe,
    // kIframePresentational, and text nodes. We don't want text navigation to
    // stop on any of the above roles. On the other hand, nodes that only have
    // ignored children (e.g., a button that contains only an empty ignored div)
    // need to be treated as leaf nodes.
    //
    // Note that we have already determined that the anchor at this position
    // doesn't have an unignored child, making this a leaf tree or text
    // position, or a leaf's descendant.
    return (!IsPlatformDocument(role) && !IsIframe(role) && !node.IsText());
  }

  // Return true if the node is a leaf, or has no selectable text content.
  static bool IsLeafNodeForTreePosition(const AXNode& node) {
    // Unignored text list markers expose text on their own, and all their
    // descendants are ignored. Make sure they are treated as leaves, not empty
    // containers.
    if (node.GetRole() == ax::mojom::Role::kListMarker && !node.IsIgnored() &&
        !node.GetUnignoredChildCountCrossingTreeBoundary()) {
      return true;
    }
    return !node.GetChildCountCrossingTreeBoundary() || IsEmptyObject(node);
  }

  AXNode* GetAnchor() const {
    if (tree_id_ == AXTreeIDUnknown() || anchor_id_ == kInvalidAXNodeID)
      return nullptr;

    const AXTreeManager* manager = GetManager();
    if (manager)
      return manager->GetNode(anchor_id());

    return nullptr;
  }

  int GetAnchorSiblingCount() const {
    if (IsNullPosition())
      return 0;

    AXPositionInstance parent_position = AsTreePosition()->CreateParentPosition(
        ax::mojom::MoveDirection::kBackward);
    if (!parent_position->IsNullPosition())
      return parent_position->AnchorChildCount();

    return 0;
  }

  int child_index() const { return child_index_; }
  int text_offset() const { return text_offset_; }
  ax::mojom::TextAffinity affinity() const { return affinity_; }

  bool IsIgnored() const {
    if (IsNullPosition())
      return false;

    DCHECK(GetAnchor());
    // If this position is anchored to an ignored node, then consider this
    // position to be ignored.
    if (GetAnchor()->IsIgnored())
      return true;

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TREE_POSITION: {
        // If this is a "before text" or an "after text" tree position, it's
        // pointing to the anchor itself, which we've determined to be
        // unignored.
        DCHECK(child_index_ == BEFORE_TEXT ||
               child_index_ == AnchorChildCount() || !IsLeaf())
            << "Leaf nodes can only have a position before or after, so they "
               "must have a child index of BEFORE_TEXT or AnchorChildCount(): "
            << ToString() << "  AnchorChildCount(): " << AnchorChildCount();
        DCHECK(child_index_ != BEFORE_TEXT || IsLeaf())
            << "Non-leaf nodes cannot have a position of BEFORE_TEXT: "
            << *GetAnchor();
        if (child_index_ == BEFORE_TEXT || IsLeaf())
          return false;

        // If this position is an "after children" position, consider the
        // position to be ignored if the last child is ignored. This is because
        // the last child will not be visible in the unignored tree.
        //
        // For example, in the following tree if the position is not adjusted,
        // the resulting position would erroneously point before the second
        // child in the unignored subtree rooted at the last child.
        //
        // 1 kRootWebArea
        // ++2 kGenericContainer ignored
        // ++++3 kStaticText "Line 1."
        // ++++4 kStaticText "Line 2."
        //
        // Tree position anchor=kGenericContainer, child_index=1.
        //
        // Alternatively, if there is a node at the position pointed to by
        // "child_index_", i.e. this position is neither a leaf position nor an
        // "after children" position, consider this tree position to be ignored
        // if the child node is ignored.
        int adjusted_child_index = child_index_ != AnchorChildCount()
                                       ? child_index_
                                       : child_index_ - 1;
        AXPositionInstance child_position =
            CreateChildPositionAt(adjusted_child_index);
        DCHECK(child_position && !child_position->IsNullPosition());
        return child_position->IsNullPosition() ||
               child_position->GetAnchor()->IsIgnored();
      }
      case AXPositionKind::TEXT_POSITION:
        // If the corresponding leaf position is ignored, the current text
        // offset will point to ignored text. Therefore, consider this position
        // to be ignored.
        if (!IsLeaf())
          return AsLeafTreePosition()->IsIgnored();
        return false;
    }
  }

  bool IsNullPosition() const {
    return kind_ == AXPositionKind::NULL_POSITION || !GetAnchor();
  }

  bool IsTreePosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TREE_POSITION;
  }

  bool IsLeafTreePosition() const { return IsTreePosition() && IsLeaf(); }

  bool IsTextPosition() const {
    return GetAnchor() && kind_ == AXPositionKind::TEXT_POSITION;
  }

  bool IsLeafTextPosition() const { return IsTextPosition() && IsLeaf(); }

  bool IsLeaf() const {
    if (IsNullPosition())
      return false;

    AXNode* anchor = GetAnchor();
    DCHECK(anchor);

    return IsLeafNodeForTreePosition(*anchor);
  }

  // Returns true if this is a valid position, e.g. the child_index_ or
  // text_offset_ is within a valid range.
  //
  // A position is always valid at creation time, but could become invalid after
  // a tree update. For performance reasons, we don't check for validity every
  // time a position is used, expecting clients to use this method instead.
  bool IsValid() const {
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return tree_id_ == AXTreeIDUnknown() &&
               anchor_id_ == kInvalidAXNodeID &&
               child_index_ == INVALID_INDEX &&
               text_offset_ == INVALID_OFFSET &&
               affinity_ == ax::mojom::TextAffinity::kDownstream;
      case AXPositionKind::TREE_POSITION:
        if (!GetAnchor())
          return false;

        // The `BEFORE_TEXT` constant is only needed on leaf positions because
        // on any other position a `child_index_` of 0 could be used. On leaf
        // positions, however, often there are no child nodes and so a
        // `child_index_` of 0 would confusingly indicate both a "before text"
        // as well as an "after text" position. Note that some leaf positions,
        // e.g. positions in empty objects, do have children.
        if (IsLeaf()) {
          // Leaf nodes can only have a position before or after, so they must
          // have a child index of BEFORE_TEXT or AnchorChildCount().
          return child_index_ == BEFORE_TEXT ||
                 child_index_ == AnchorChildCount();
        }

        return child_index_ >= 0 && child_index_ <= AnchorChildCount();
      case AXPositionKind::TEXT_POSITION:
        if (!GetAnchor())
          return false;

        // For performance reasons we skip any validation of the text offset
        // that involves retrieving the anchor's text, if the offset is set to
        // 0, because 0 is frequently used and always valid regardless of the
        // actual text.
        return text_offset_ == 0 ||
               (text_offset_ > 0 && text_offset_ <= MaxTextOffset());
    }
  }

  bool AtStartOfAnchor() const {
    if (!GetAnchor())
      return false;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        if (IsLeaf())
          return child_index_ == BEFORE_TEXT;
        return child_index_ == 0;
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == 0;
    }
  }

  bool AtEndOfAnchor() const {
    if (!GetAnchor())
      return false;
    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        // A few positions are anchored to nodes that have children but we want
        // to treat them as leaf positions. An example is an empty text field;
        // it often has empty unignored divs coming from Blink inside it.
        return child_index_ == AnchorChildCount();
      case AXPositionKind::TEXT_POSITION:
        return text_offset_ == MaxTextOffset();
    }
  }

  bool AtStartOfWord() const {
    AXPositionInstance text_position;
    if (!AtEndOfAnchor()) {
      // We could get a leaf text position at the end of its anchor, where word
      // start offsets would surely not be present. In such cases, we need to
      // normalize to the start of the next leaf anchor. We avoid making this
      // change when we are at the end of our anchor because this could
      // effectively shift the position forward.
      text_position = AsLeafTextPositionBeforeCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t>& word_starts =
            text_position->GetWordStartOffsets();
        return base::Contains(word_starts,
                              int32_t{text_position->text_offset_});
      }
    }
  }

  bool AtEndOfWord() const {
    AXPositionInstance text_position;
    if (!AtStartOfAnchor()) {
      // We could get a leaf text position at the start of its anchor, where
      // word end offsets would surely not be present. In such cases, we need to
      // normalize to the end of the previous leaf anchor. We avoid making this
      // change when we are at the start of our anchor because this could
      // effectively shift the position backward.
      text_position = AsLeafTextPositionAfterCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t>& word_ends =
            text_position->GetWordEndOffsets();
        return base::Contains(word_ends, int32_t{text_position->text_offset_});
      }
    }
  }

  bool AtStartOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // We treat a position after some white space that is not connected to
        // any node after it via "next on line ID", to be equivalent to a
        // position before the next line, and therefore as being at start of
        // line.
        //
        // We assume that white space, including but not limited to hard line
        // breaks, might be used to separate lines. For example, an inline text
        // box with just a single space character inside it can be used to
        // represent a soft line break. If an inline text box containing white
        // space separates two lines, it should always be connected to the first
        // line via "kPreviousOnLineId". This is guaranteed by the renderer. If
        // there are multiple line breaks separating the two lines, then only
        // the first line break is connected to the first line via
        // "kPreviousOnLineId".
        //
        // Sometimes there might be an inline text box with a single space in it
        // at the end of a text field. We should not mark positions that are at
        // the end of text fields, or in general at the end of their anchor, as
        // being at the start of line, except when that anchor is an inline text
        // box that is in the middle of a text span. Note that in most but not
        // all cases, the parent of an inline text box is a static text object,
        // whose end signifies the end of the text span. One exception is line
        // breaks.
        if (text_position->AtEndOfAnchor() &&
            !text_position->AtEndOfTextSpan() &&
            text_position->IsInWhiteSpace() &&
            text_position->GetNextOnLineID() == kInvalidAXNodeID) {
          return true;
        }

        // If the anchor is ignored, then by default it will not have a
        // PreviousOnLineID set since we only set this on unignored nodes.
        // However, it could still have something previous to it on the same
        // line, like for example if we have some text on the same line, and a
        // text node in the middle is set to aria-hidden.
        return text_position->GetPreviousOnLineID() == kInvalidAXNodeID &&
               text_position->AtStartOfAnchor() &&
               !text_position->GetAnchor()->IsIgnored();
    }
  }

  bool AtEndOfLine() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION:
        // Text positions on objects with no text should not be considered at
        // end of line because the empty position may share a text offset with
        // a non-empty text position in which case the end of line iterators
        // must move to the line end of the non-empty content. Specified next
        // line IDs are ignored.
        if (text_position->MaxTextOffset() == 0)
          return false;

        // If affinity has been used to specify whether the caret is at the end
        // of a line or at the start of the next one, this should have been
        // reflected in the leaf text position we got via "AsLeafTextPosition".
        // If affinity had been set to upstream, the leaf text position should
        // be pointing to the end of the inline text box that ends the first
        // line. If it had been set to downstream, the leaf text position should
        // be pointing to the start of the inline text box that starts the
        // second line.
        //
        // In other cases, we assume that white space, including but not limited
        // to hard line breaks, might be used to separate lines. For example, an
        // inline text box with just a single space character inside it can be
        // used to represent a soft line break. If an inline text box containing
        // white space separates two lines, it should always be connected to the
        // first line via "kPreviousOnLineId". This is guaranteed by the
        // renderer. If there are multiple line breaks separating the two lines,
        // then only the first line break is connected to the first line via
        // "kPreviousOnLineId".
        //
        // We don't treat a position that is at the start of white space that is
        // on a line by itself as being at the end of the line. This is in order
        // to enable screen readers to recognize and announce blank lines
        // correctly. However, we do treat positions at the start of white space
        // that end a line of text as being at the end of that line. We also
        // treat positions at the end of white space that is on a line by
        // itself, i.e. on a blank line, as being at the end of that line.
        //
        // Sometimes there might be an inline text box with a single space in it
        // at the end of a text field. We should mark positions that are at the
        // end of text fields, or in general at the end of an anchor with no
        // "kNextOnLineId", as being at end of line, except when that anchor is
        // an inline text box that is in the middle of a text span. Note that
        // in most but not all cases, the parent of an inline text box is a
        // static text object, whose end signifies the end of the text span. One
        // exception is line breaks.
        if (text_position->GetNextOnLineID() == kInvalidAXNodeID) {
          return (!text_position->AtEndOfTextSpan() &&
                  text_position->IsInWhiteSpace() &&
                  text_position->GetPreviousOnLineID() != kInvalidAXNodeID)
                     ? text_position->AtStartOfAnchor()
                     : text_position->AtEndOfAnchor();
        }

        // The current anchor might be followed by a soft line break.
        return text_position->AtEndOfAnchor() &&
               text_position->CreateNextLeafTextPosition()->AtEndOfLine();
    }
  }

  AXBoundaryType GetFormatStartBoundaryType() const {
    // Since formats are stored on text anchors, the start of a format boundary
    // must be at the start of an anchor.
    if (IsNullPosition() || !AtStartOfAnchor())
      return AXBoundaryType::kNone;

    // Treat the first iterable node as a format boundary.
    if (CreatePreviousLeafTreePosition(
            base::BindRepeating(&AbortMoveAtRootBoundary))
            ->IsNullPosition()) {
      return AXBoundaryType::kContentStart;
    }

    // Ignored positions cannot be format boundaries.
    if (IsIgnored())
      return AXBoundaryType::kNone;

    // Iterate over anchors until a format boundary is found. This will return a
    // null position upon crossing a boundary. Make sure the previous position
    // is not on an ignored node.
    AXPositionInstance previous_position = Clone();
    do {
      previous_position = previous_position->CreatePreviousLeafTreePosition(
          base::BindRepeating(&AbortMoveAtFormatBoundary));
    } while (previous_position->IsIgnored());

    if (previous_position->IsNullPosition())
      return AXBoundaryType::kUnitBoundary;

    return AXBoundaryType::kNone;
  }

  bool AtStartOfFormat() const {
    return GetFormatStartBoundaryType() != AXBoundaryType::kNone;
  }

  AXBoundaryType GetFormatEndBoundaryType() const {
    // Since formats are stored on text anchors, the end of a format break must
    // be at the end of an anchor.
    if (IsNullPosition() || !AtEndOfAnchor())
      return AXBoundaryType::kNone;

    // Treat the last iterable node as a format boundary
    if (CreateNextLeafTreePosition(
            base::BindRepeating(&AbortMoveAtRootBoundary))
            ->IsNullPosition())
      return AXBoundaryType::kContentEnd;

    // Ignored positions cannot be format boundaries.
    if (IsIgnored())
      return AXBoundaryType::kNone;

    // Iterate over anchors until a format boundary is found. This will return a
    // null position upon crossing a boundary. Make sure the next position is
    // not on an ignored node.
    AXPositionInstance next_position = Clone();
    do {
      next_position = next_position->CreateNextLeafTreePosition(
          base::BindRepeating(&AbortMoveAtFormatBoundary));
    } while (next_position->IsIgnored());

    if (next_position->IsNullPosition())
      return AXBoundaryType::kUnitBoundary;

    return AXBoundaryType::kNone;
  }

  bool AtEndOfFormat() const {
    return GetFormatEndBoundaryType() != AXBoundaryType::kNone;
  }

  bool AtStartOfSentence() const {
    AXPositionInstance text_position;
    if (!AtEndOfAnchor()) {
      // We could get a leaf text position at the end of its anchor, where
      // sentence start offsets would surely not be present. In such cases, we
      // need to normalize to the start of the next leaf anchor. We avoid making
      // this change when we are at the end of our anchor because this could
      // effectively shift the position forward.
      text_position = AsLeafTextPositionBeforeCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t>& sentence_starts =
            text_position->GetAnchor()->GetIntListAttribute(
                ax::mojom::IntListAttribute::kSentenceStarts);
        return base::Contains(sentence_starts,
                              int32_t{text_position->text_offset_});
      }
    }
  }

  bool AtEndOfSentence() const {
    AXPositionInstance text_position;
    if (!AtStartOfAnchor()) {
      // We could get a leaf text position at the start of its anchor, where
      // sentence end offsets would surely not be present. In such cases, we
      // need to normalize to the end of the previous leaf anchor. We avoid
      // making this change when we are at the start of our anchor because this
      // could effectively shift the position backward.
      text_position = AsLeafTextPositionAfterCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        const std::vector<int32_t>& sentence_ends =
            text_position->GetAnchor()->GetIntListAttribute(
                ax::mojom::IntListAttribute::kSentenceEnds);
        return base::Contains(sentence_ends,
                              int32_t{text_position->text_offset_});
      }
    }
  }

  // `AtStartOfParagraph` is asymmetric from `AtEndOfParagraph` because line
  // breaks could be present between paragraphs. The end of the paragraph is
  // always before all such breaks, whilst the start of paragraph is always
  // after.
  //
  // The start of a paragraph should be a leaf text position (or equivalent),
  // either at the start of the whole content, or at the start of a leaf text
  // position which is right after the one representing the end of the previous
  // paragraph, or the one representing one or more line breaks that separate
  // the two paragraphs.
  //
  // In other words, a position `AsLeafTextPosition` is the start of a paragraph
  // if one of the following is true :
  // 1. The current leaf text position must be at the start of an anchor, or
  // after a '\n' character if white space is preserved (e.g. when using
  // <pre>...</pre>, or when in an ARIA label), but not before a '\n' character
  // in a <br> element unless multiple consecutive <br> elements are present and
  // so empty paragraphs have been created.
  // 2. Either (a) the current leaf text position is the first leaf text
  // position in the whole content, or (b) there is a line breaking object
  // between it and the previous leaf text position including any <br> element.
  bool AtStartOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        // 1. The current leaf text position must be at the start of an anchor,
        // or after a '\n' character if white space is preserved (e.g. when
        // using <pre>...</pre>, or when in an ARIA label), but not before a
        // '\n' character in a <br> element unless multiple consecutive <br>
        // elements are present and so empty paragraphs have been created.
        //
        // Note that, in theory, `!AtStartOfAnchor()` implies that
        // `MaxTextOffset()` > 0 and `text_offset()` > 0. Therefore,
        // `text_position->GetText().at(text_position->text_offset_ - 1)` should
        // always be valid. However, as reported by https://crbug.com/1379716,
        // this logic appears to have flaws.
        //
        // TODO(accessibility): Investigate what are these edge cases that lead
        // to have a `text_offset_` greater than or equal to the `text` length.
        if (!text_position->AtStartOfAnchor()) {
          if (!text_position->IsPointingToLineBreak()) {
            const std::u16string text = text_position->GetText();
            if (static_cast<size_t>(text_position->text_offset_) <
                    text.length() &&
                text.at(text_position->text_offset_) == '\n') {
              return true;
            }
          }
          return false;
        }

        // 2. Either (a) the current leaf text position is the first leaf text
        // position in the whole content, or (b) there is a line breaking object
        // between it and the previous leaf text position including any <br>
        // element.
        //
        // Search for the previous text position within the current paragraph,
        // using the paragraph boundary abort predicate. If a valid position was
        // found, then this position cannot be the start of a paragraph. The
        // predicate will return a null position when an anchor movement would
        // cross a paragraph boundary, or the start of content has been reached.
        const AbortMovePredicate abort_move_predicate =
            base::BindRepeating(&AbortMoveAtParagraphBoundary,
                                ax::mojom::TextBoundary::kParagraphStart);
        return text_position
            ->CreatePreviousLeafTextPosition(abort_move_predicate)
            ->IsNullPosition();
      }
    }
  }

  // `AtEndOfParagraph` is asymmetric from `AtStartOfParagraph` because line
  // breaks could be present between paragraphs. The end of the paragraph is
  // always before all such breaks, whilst the start of paragraph is always
  // after.
  //
  // The end of a paragraph should be a leaf text position (or equivalent),
  // either at the end of the whole content, or at the end of a leaf text
  // position which is right before the one representing the start of the next
  // paragraph, or the one representing one or more line breaks that separate
  // the two paragraphs.
  //
  // In other words, a position `AsLeafTextPosition` is the end of a paragraph
  // if one of the following is true :
  // 1. The current leaf text position must be at the end of an anchor, or
  // before a '\n' character if white space is preserved (e.g. when using
  // <pre>...</pre>, or when in an ARIA label), but not after a '\n' character
  // in a <br> element unless multiple consecutive <br> elements are present and
  // so empty paragraphs have been created.
  // 2. Either (a) the current leaf text position is the last leaf text position
  // in the whole content, or (b) there is a line breaking object between it and
  // the next leaf text position, including any <br> element.
  bool AtEndOfParagraph() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        // 1. The current leaf text position must be at the end of an anchor, or
        // before a '\n' character if white space is preserved (e.g. when using
        // <pre>...</pre>, or when in an ARIA label), but not after a '\n'
        // character in a <br> element unless multiple consecutive <br> elements
        // are present and so empty paragraphs have been created.
        //
        // Note that, in theory, `!AtEndOfAnchor()` implies
        // `AtStartOfAnchor()` != `AtEndOfAnchor()` which in turn implies that
        // `MaxTextOffset()` > 0 and `text_offset()` < `MaxTextOffset()`.
        // Therefore, `text_position->GetText().at(text_position->text_offset_)`
        // should always be valid. However, as reported by
        // https://crbug.com/1379716, this logic appears to have flaws.
        //
        // TODO(accessibility): Investigate what are these edge cases that lead
        // to have a `text_offset_` greater than or equal to the `text` length.
        if (!text_position->AtEndOfAnchor()) {
          if (!text_position->IsPointingToLineBreak()) {
            const std::u16string text = text_position->GetText();
            if (static_cast<size_t>(text_position->text_offset_) <
                    text.length() &&
                text.at(text_position->text_offset_) == '\n') {
              return true;
            }
          }
          return false;
        }

        // 2. Either (a) the current leaf text position is the last leaf text
        // position in the whole content, or (b) there is a line breaking object
        // between it and the next leaf text position, including any <br>
        // element.
        //
        // Search for the next text position within the current paragraph, using
        // the paragraph boundary abort predicate. If a valid position was
        // found, then this position cannot be the end of a paragraph. The
        // predicate will return a null position when an anchor movement would
        // cross a paragraph boundary, or the end of content has been reached.
        const AbortMovePredicate abort_move_predicate =
            base::BindRepeating(&AbortMoveAtParagraphBoundary,
                                ax::mojom::TextBoundary::kParagraphEnd);
        return text_position->CreateNextLeafTextPosition(abort_move_predicate)
            ->IsNullPosition();
      }
    }
  }

  // Returns true if this position is at the start or right before content that
  // is laid out using "display: inline-block".
  bool AtStartOfInlineBlock() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (text_position->AtStartOfAnchor()) {
          AXPositionInstance previous_position =
              text_position->CreatePreviousLeafTreePosition();

          // Check that this position is not the start of the first anchor.
          if (!previous_position->IsNullPosition()) {
            previous_position = text_position->CreatePreviousLeafTreePosition(
                base::BindRepeating(&AbortMoveAtStartOfInlineBlock));

            // If we get a null position here it means we have crossed an inline
            // block's start, thus this position is located at such start.
            if (previous_position->IsNullPosition())
              return true;
          }
        }
        if (text_position->AtEndOfAnchor()) {
          AXPositionInstance next_position =
              text_position->CreateNextLeafTreePosition();

          // Check that this position is not the end of the last anchor.
          if (!next_position->IsNullPosition()) {
            next_position = text_position->CreateNextLeafTreePosition(
                base::BindRepeating(&AbortMoveAtStartOfInlineBlock));

            // If we get a null position here it means we have crossed an inline
            // block's start, thus this position is located at such start.
            if (next_position->IsNullPosition())
              return true;
          }
        }
        return false;
      }
    }
  }

  // Page boundaries are only supported in certain content types, e.g. PDF
  // documents.
  bool AtStartOfPage() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtStartOfAnchor())
          return false;

        // Search for the previous text position within the current page,
        // using the page boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the start of a page.
        // This will return a null position when an anchor movement would
        // cross a page boundary, or the start of content was reached.
        AXPositionInstance previous_text_position =
            text_position->CreatePreviousLeafTextPosition(
                base::BindRepeating(&AbortMoveAtPageBoundary));
        return previous_text_position->IsNullPosition();
      }
    }
  }

  // Page boundaries are only supported in certain content types, e.g. PDF
  // documents.
  bool AtEndOfPage() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    switch (text_position->kind_) {
      case AXPositionKind::NULL_POSITION:
        return false;
      case AXPositionKind::TREE_POSITION:
        NOTREACHED_IN_MIGRATION();
        return false;
      case AXPositionKind::TEXT_POSITION: {
        if (!text_position->AtEndOfAnchor())
          return false;

        // Search for the next text position within the current page,
        // using the page boundary abort predicate.
        // If a valid position was found, then this position cannot be
        // the end of a page.
        // This will return a null position when an anchor movement would
        // cross a page boundary, or the end of content was reached.
        AXPositionInstance next_text_position =
            text_position->CreateNextLeafTextPosition(
                base::BindRepeating(&AbortMoveAtPageBoundary));
        return next_text_position->IsNullPosition();
      }
    }
  }

  // Returns true if this position is at the start of the current accessibility
  // tree, such as the current iframe, webpage, PDF document, dialog or window.
  // Note that the current webpage could be made up of multiple accessibility
  // trees stitched together, e.g. an out-of-process iframe will be in its own
  // accessibility tree. For the purposes of this method, we don't distinguish
  // between out-of-process and in-process iframes, treating them both as tree
  // boundaries.
  bool AtStartOfAXTree() const {
    if (IsNullPosition() || !AtStartOfAnchor())
      return false;

    AXPositionInstance previous_anchor = CreatePreviousAnchorPosition();
    // The start of the whole content should also be the start of an AXTree.
    if (previous_anchor->IsNullPosition())
      return true;

    return previous_anchor->tree_id() != tree_id();
  }

  // Returns true if this position is at the end of the current accessibility
  // tree, such as the current iframe, webpage, PDF document, dialog or window.
  // Note that the current webpage could be made up of multiple accessibility
  // trees stitched together, e.g. an out-of-process iframe will be in its own
  // accessibility tree. For the purposes of this method, we don't distinguish
  // between out-of-process and in-process iframes, treating them both as tree
  // boundaries.
  bool AtEndOfAXTree() const {
    if (IsNullPosition() || !IsLeaf() || !AtEndOfAnchor())
      return false;

    return *CreatePositionAtEndOfAXTree() == *this;
  }

  // Returns true if this position is at the start of all content. This might
  // refer to e.g. a single webpage (made up of multiple iframes), or a PDF
  // document. Note that the current webpage could be made up of multiple
  // accessibility trees stitched together, so even though a position could be
  // at the start of a specific accessibility tree, it might not be at the start
  // of the whole content.
  bool AtStartOfContent() const {
    if (IsNullPosition() || !AtStartOfAnchor())
      return false;

    return *CreatePositionAtStartOfContent() == *this;
  }

  // Returns true if this position is at the end of all content. This might
  // refer to e.g. a single webpage (made up of multiple iframes), or a PDF
  // document. Note that the current webpage could be made up of multiple
  // accessibility trees stitched together, so even though a position could be
  // at the end of a specific accessibility tree, it might not be at the end of
  // the whole content.
  bool AtEndOfContent() const {
    if (IsNullPosition() || !AtEndOfAnchor())
      return false;

    return *CreatePositionAtEndOfContent() == *this;
  }

  // This method finds the lowest common ancestor node in the accessibility tree
  // of this and |other| positions' anchor nodes.
  AXNode* LowestCommonAnchor(const AXPosition& other) const {
    if (IsNullPosition() || other.IsNullPosition())
      return nullptr;
    if (GetAnchor() == other.GetAnchor())
      return GetAnchor();

    base::stack<AXNode*> our_ancestors = GetAncestorAnchors();
    base::stack<AXNode*> other_ancestors = other.GetAncestorAnchors();

    AXNode* common_anchor = nullptr;
    while (!our_ancestors.empty() && !other_ancestors.empty() &&
           our_ancestors.top() == other_ancestors.top()) {
      common_anchor = our_ancestors.top();
      our_ancestors.pop();
      other_ancestors.pop();
    }
    return common_anchor;
  }

  // This method returns a position instead of a node because this allows us to
  // return the corresponding text offset or child index in the ancestor that
  // relates to the current position.
  // Also, this method uses position instead of tree logic to traverse the tree,
  // because positions can handle moving across multiple trees, while trees
  // cannot.
  AXPositionInstance LowestCommonAncestorPosition(
      const AXPosition& other,
      ax::mojom::MoveDirection move_direction) const {
    return CreateAncestorPosition(LowestCommonAnchor(other), move_direction);
  }

  // See "CreateParentPosition" for an explanation of the use of
  // |move_direction|.
  AXPositionInstance CreateAncestorPosition(
      const AXNode* ancestor_anchor,
      ax::mojom::MoveDirection move_direction) const {
    if (!ancestor_anchor)
      return CreateNullPosition();

    AXPositionInstance ancestor_position = Clone();
    while (!ancestor_position->IsNullPosition() &&
           ancestor_position->GetAnchor() != ancestor_anchor) {
      ancestor_position =
          ancestor_position->CreateParentPosition(move_direction);
    }
    return ancestor_position;
  }

  // If the position is not valid, we return a new valid position that is
  // closest to the original position if possible, or a null position otherwise.
  AXPositionInstance AsValidPosition() const {
    AXPositionInstance position = Clone();
    switch (position->kind_) {
      case AXPositionKind::NULL_POSITION:
        // We avoid cloning to ensure that all fields will be valid.
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION: {
        if (!position->GetAnchor())
          return CreateNullPosition();

        const AXNode* leaf_node = GetEmptyObjectAncestorNode();
        if (!leaf_node && position->IsLeaf()) {
          // If there is no empty object ancestor, but the current position's
          // anchor is a leaf, then use the same anchor, as it will be valid
          // as long as a valid offset is used.
          leaf_node = position->GetAnchor();
        }
        if (leaf_node) {
          // In this class, we define the empty node as a leaf node (see
          // `AXNode::IsLeaf()`) that doesn't have any content. On certain
          // platforms, and so that such nodes will act as a character and a
          // word boundary, we insert an "embedded object replacement character"
          // in their text contents. This character is a string of length
          // `AXNode::kEmbeddedObjectCharacterLengthUTF16`. For example, an
          // empty text field should act as a character and a word boundary when
          // a screen reader user tries to navigate through it, otherwise the
          // text field would be missed by the user.
          //
          // Since we just explained that on certain platforms empty leaf nodes
          // expose the "embedded object replacement character" in their text
          // contents, and since we assume that all text is found only on leaf
          // nodes, we should hide any descendants. Thus, a position on a
          // descendant of an empty object is defined as invalid. To make it
          // valid we move the position from the descendant to the empty leaf
          // node itself. Otherwise, character and word navigation won't work
          // properly.
          AXPositionInstance new_position =
              position->child_index() == BEFORE_TEXT
                  ? CreateTreePositionAtStartOfAnchor(*leaf_node)
                  : CreateTreePositionAtEndOfAnchor(*leaf_node);
          DCHECK(new_position->IsLeaf());
          return new_position;
        }

        DCHECK(!position->IsLeaf());
        // Not a leaf: use a child index from 0 to AnchorChilkdCount().
        if (position->child_index_ == BEFORE_TEXT) {
          position->child_index_ = 0;
          return position;
        }

        DCHECK_GE(position->child_index_, 0);
        if (position->child_index_ > position->AnchorChildCount())
          position->child_index_ = position->AnchorChildCount();
        break;
      }
      case AXPositionKind::TEXT_POSITION: {
        if (!position->GetAnchor())
          return CreateNullPosition();

        if (const AXNode* empty_object_node = GetEmptyObjectAncestorNode()) {
          // This is needed because an empty object as defined in this class can
          // have descendants that should not be exposed. See comment above in
          // similar implementation for AXPositionKind::TREE_POSITION.
          //
          // We set the |text_offset_| to either 0 or (on certain platforms) the
          // length of the embedded object character here because the
          // `MaxTextOffset()` of an empty object on those platforms is
          // `AXNode::kEmbeddedObjectCharacterLengthUTF16`. If the invalid
          // position was already at the start of the node, we set it to 0.
          AXPositionInstance valid_position = CreateTextPosition(
              *empty_object_node,
              /* text_offset */ 0, ax::mojom::TextAffinity::kDownstream);
          if (position->text_offset() > 0)
            return valid_position->CreatePositionAtEndOfAnchor();
          return std::move(valid_position);
        }

        if (position->text_offset_ <= 0) {
          // 0 is always a valid offset, so skip calling MaxTextOffset in that
          // case.
          position->text_offset_ = 0;
          position->affinity_ = ax::mojom::TextAffinity::kDownstream;
        } else {
          int max_text_offset = position->MaxTextOffset();
          if (position->text_offset_ > max_text_offset) {
            position->text_offset_ = max_text_offset;
            position->affinity_ = ax::mojom::TextAffinity::kDownstream;
          }
        }
        break;
      }
    }
    DCHECK(position->IsValid()) << *position;
    return position;
  }

  AXPositionInstance AsTreePosition() const {
    if (IsNullPosition() || IsTreePosition())
      return Clone();

    AXPositionInstance copy = Clone();
    DCHECK_GE(copy->text_offset_, 0);
    // Note that by design, `AXPosition::IsLeaf()` excludes the text found in
    // ignored subtrees from the accessibility tree's text representation. (See
    // `AXNode::IsEmptyLeaf()`.)
    if (copy->IsLeaf()) {
      // Even though leaf positions are generally not anchored to a node with a
      // lot of descendants, still, there is the possibility that the leaf node
      // is a text field with a large amount of text. We avoid computing
      // `MaxTextOffset()` unless it is really necessary.
      if (copy->text_offset_ == 0) {
        copy->child_index_ = BEFORE_TEXT;
      } else {
        const int max_text_offset = copy->MaxTextOffset();
        copy->child_index_ = copy->text_offset_ != max_text_offset
                                 ? BEFORE_TEXT
                                 : AnchorChildCount();
      }

      copy->kind_ = AXPositionKind::TREE_POSITION;
      DCHECK(copy->IsValid());
      return copy;
    }

    // We stop at the first child that we can reach with the current text
    // offset. We do not attempt to validate `MaxTextOffset()` in case it
    // doesn't match the total length of all our children. This may happen if,
    // for example, there is a bug in the internal accessibility tree we get
    // from the renderer. In contrast, the current offset could not be greater
    // than the length of all our children because the position would have been
    // invalid.
    //
    // Note that even though ignored children should not contribute any text
    // content or hypertext to the tree's text representation, we have to
    // include them because they might contain unignored descendants. We only
    // exclude them if they are both ignored and contain no text content or
    // hypertext. The latter is to avoid, as much as we can, the possibility
    // that an unignored position will turn into an ignored one after calling
    // this method.

    int child_index = 0;
    for (int current_offset = 0; child_index < copy->AnchorChildCount();
         ++child_index) {
      AXPositionInstance child = copy->CreateChildPositionAt(child_index);
      DCHECK(!child->IsNullPosition());

      // If the text offset falls on the boundary between two adjacent children,
      // we look at the affinity to decide whether to place the tree position on
      // the first child vs. the second child. Upstream affinity would always
      // choose the first child, whilst downstream affinity the second. This
      // also has implications when converting the resulting tree position back
      // to a text position. In that case, maintaining an upstream affinity
      // would place the text position at the end of the first child, whilst
      // maintaining a downstream affinity will place the text position at the
      // beginning of the second child. This is vital for text positions on soft
      // line breaks, as well as text positions before and after character, to
      // work properly.
      //
      // Note that in this context "adjacent children" excludes ignored
      // children. Note also that children with no text content or no hypertext
      // are not skipped, otherwise the following situation will produce an
      // erroneous tree position:
      // ++kTextField contenteditable=true "" (empty)
      // ++++kStaticText "\n" ignored
      // ++++++kInlineTextBox "\n" ignored
      // ++++kStaticText "" (empty)
      // ++++++kInlineTextOffset "" (empty)
      // TextPosition anchor=kTextField text_offset=0 affinity=downstream
      // AsTreePosition should produce:
      // TreePosition anchor=kTextField child_index=1, and not child_index=0 or
      // child_index=2
      //
      // See also `CreateLeafTextPositionBeforeCharacter` and
      // `CreateLeafTextPositionAfterCharacter`.

      const int child_length = child->MaxTextOffsetInParent();
      const bool contributes_no_text_in_parent = !child_length;
      const bool is_anchor_unignored = !child->GetAnchor()->IsIgnored();
      if (copy->text_offset_ >= current_offset &&
          (copy->text_offset_ < (current_offset + child_length) ||
           ((copy->affinity_ == ax::mojom::TextAffinity::kUpstream ||
             (contributes_no_text_in_parent && is_anchor_unignored)) &&
            copy->text_offset_ == (current_offset + child_length)))) {
        break;
      }

      current_offset += child_length;
    }

    copy->child_index_ = child_index;
    copy->kind_ = AXPositionKind::TREE_POSITION;
    return copy;
  }

  // This is an optimization over "AsLeafTextPosition", in cases when computing
  // the corresponding text offset on the leaf node is not needed. If this
  // method is called on a text position, it will conservatively fall back to
  // the non-optimized "AsLeafTextPosition", if the current text offset is
  // greater than 0, or the affinity is upstream, since converting to a tree
  // position at any point before reaching the leaf node could potentially lose
  // information.
  AXPositionInstance AsLeafTreePosition() const {
    if (IsNullPosition() || IsLeaf())
      return AsTreePosition();

    // If our text offset is greater than 0, or if our affinity is set to
    // upstream, we need to ensure that text offset and affinity will be taken
    // into consideration during our descend to the leaves. Switching to a tree
    // position early in this case will potentially lose information, so we
    // descend using a text position instead.
    //
    // We purposely don't check whether this position is a text position, to
    // allow for the possibility that this position has recently been converted
    // from a text to a tree position and text offset or affinity information
    // has been left intact.
    if (text_offset_ > 0 || affinity_ == ax::mojom::TextAffinity::kUpstream)
      return AsLeafTextPosition()->AsTreePosition();

    AXPositionInstance tree_position = AsTreePosition();
    do {
      if (tree_position->AtEndOfAnchor()) {
        tree_position =
            tree_position
                ->CreateChildPositionAt(tree_position->child_index_ - 1)
                ->CreatePositionAtEndOfAnchor();
      } else {
        tree_position =
            tree_position->CreateChildPositionAt(tree_position->child_index_);
      }
      DCHECK(!tree_position->IsNullPosition());
    } while (!tree_position->IsLeaf());

    DCHECK(tree_position->IsLeafTreePosition());
    return tree_position;
  }

  AXPositionInstance AsTextPosition() const {
    if (IsNullPosition() || IsTextPosition())
      return Clone();

    AXPositionInstance copy = Clone();
    // Check if it is a "before text" position.
    if (copy->child_index_ == BEFORE_TEXT) {
      DCHECK(copy->IsLeaf())
          << "Before text positions can only appear on leaf nodes.";
      // If the current text offset is valid, we don't touch it to potentially
      // allow converting from a text position to a tree position and back
      // without losing information.
      //
      // We test for INVALID_OFFSET and greater than 0 first, due to the
      // possible performance cost of calling `MaxTextOffset()`. Also, if the
      // text offset is already 0, we don't need to touch it, and if it is less
      // than `MaxTextOffset()` we don't modify it as explained above.
      DCHECK_GE(copy->text_offset_, INVALID_OFFSET)
          << "Unrecognized text offset.";
      if (copy->text_offset_ == INVALID_OFFSET ||
          (copy->text_offset_ > 0 &&
           copy->text_offset_ >= copy->MaxTextOffset())) {
        copy->text_offset_ = 0;
      }

      copy->kind_ = AXPositionKind::TEXT_POSITION;
      return copy;
    }

    // Leaf nodes might have descendants that should be hidden for text
    // navigation purposes, thus we can't rely solely on `AnchorChildCount()`.
    // Any child index that is not `BEFORE_TEXT` should be treated as indicating
    // an "after text" position. (See `IsInEmptyObject()` for more information.)
    // ++kButton "<embedded_object_character>" (empty)
    // ++++kGenericContainer ignored (Might sometimes be added by Blink.)
    if (copy->IsLeaf() || copy->child_index_ == copy->AnchorChildCount()) {
      copy->text_offset_ = copy->MaxTextOffset();
      copy->kind_ = AXPositionKind::TEXT_POSITION;
      return copy;
    }

      DCHECK_GE(copy->child_index_, 0);
      DCHECK_LT(copy->child_index_, copy->AnchorChildCount());
      int new_offset = 0;
      for (int i = 0; i <= child_index_; ++i) {
        AXPositionInstance child = copy->CreateChildPositionAt(i);
        DCHECK(!child->IsNullPosition());
        // If the current text offset is valid, we don't touch it to
        // potentially allow converting from a text position to a tree
        // position and back without losing information. Otherwise, if the
        // text_offset is invalid, equals to 0 or is smaller than
        // |new_offset|, we reset it to the beginning of the current child.
        if (i == child_index_ && copy->text_offset_ <= new_offset) {
          copy->text_offset_ = new_offset;
          break;
        }

        int child_length = child->MaxTextOffsetInParent();
        // Same comment as above: we don't touch the text offset if it's
        // already valid.
        if (i == child_index_ &&
            (copy->text_offset_ > (new_offset + child_length) ||
             // When the text offset is equal to the text's length but this is
             // not an "after text" position.
             (!copy->AtEndOfAnchor() &&
              copy->text_offset_ == (new_offset + child_length)))) {
          copy->text_offset_ = new_offset;
          break;
        }

        new_offset += child_length;
      }

      // Affinity should always be left as downstream. The only case when the
      // resulting text position is at the end of the line is when we get an
      // "after text" leaf position, but even in this case downstream is
      // appropriate because there is no ambiguity whether the position is at
      // the end of the current line vs. the start of the next line. It would
      // always be the former.
      copy->kind_ = AXPositionKind::TEXT_POSITION;
      return copy;
  }

  AXPositionInstance AsLeafTextPosition() const {
    if (IsNullPosition() || IsLeaf())
      return AsTextPosition();

    AXPositionInstance text_position = Clone();
    if (IsTreePosition()) {
      DCHECK_NE(child_index(), BEFORE_TEXT)
          << "Before text positions should only be present on leaf anchor "
             "nodes.";
      DCHECK_GT(AnchorChildCount(), 0)
          << "Non-leaf positions should be anchored to nodes that have "
             "children.";

      // We can't go directly to a text position if we are initially dealing
      // with a tree position, because empty child objects contribute no text to
      // the tree's text representation and thus the existing child index
      // information would be lost.
      //
      // ++kRootWebArea
      // ++++kGenericContainer (empty object)
      // ++++kGenericContainer (empty object)
      // TreePosition anchor=kRootWebArea child_index=1 would turn into a text
      // position on the same anchor but with a text offset of 0 if we call
      // `AsTextPosition()` immediately before first anchoring ourselves to the
      // selected child node.
      if (child_index() > 0 && child_index() == AnchorChildCount()) {
        text_position = CreateChildPositionAt(child_index() - 1)
                            ->CreatePositionAtEndOfAnchor();
      } else {
        text_position = CreateChildPositionAt(child_index());
      }
    }
    text_position = text_position->AsTextPosition();
    DCHECK(!text_position->IsNullPosition());

    int offset_in_parent = text_position->text_offset_;
    // Determine the anchor and text offset of the leaf equivalent position by
    // counting characters that are previous in tree order than
    // `offset_in_parent`.
    while (!text_position->IsLeaf()) {
      AXPositionInstance child = text_position->CreateChildPositionAt(0);
      DCHECK(!child->IsNullPosition());

      // Note that even though ignored children should not contribute any text
      // content or hypertext to the tree's text representation, we have to
      // include them because they might contain unignored descendants. We only
      // exclude them if they are both ignored and contain no text content or
      // hypertext. The latter is to avoid, as much as we can, the possibility
      // that an unignored position will turn into an ignored one after calling
      // this method.
      for (int i = 1;
           i < text_position->AnchorChildCount() && offset_in_parent >= 0;
           ++i) {
        const int child_length_in_parent = child->MaxTextOffsetInParent();
        const bool contributes_no_text_in_parent =
            (child_length_in_parent == 0);
        const bool is_anchor_unignored = !child->GetAnchor()->IsIgnored();
        if (offset_in_parent == 0 && contributes_no_text_in_parent &&
            is_anchor_unignored) {
          // If the text offset corresponds to multiple child positions because
          // some of the children have no text content or hypertext, the above
          // condition ensures that the first child will be chosen; unless it is
          // ignored as explained before.
          break;
        }

        if (offset_in_parent < child_length_in_parent)
          break;

        if (affinity_ == ax::mojom::TextAffinity::kUpstream &&
            offset_in_parent == child_length_in_parent) {
          // Maintain upstream affinity so that we'll be able to choose the
          // correct leaf anchor if the text offset is right on the boundary
          // between two leaves.
          child->affinity_ = ax::mojom::TextAffinity::kUpstream;
          break;
        }

        child = text_position->CreateChildPositionAt(i);
        offset_in_parent -= child_length_in_parent;
      }

      // The text offset provided by our parent position might need to be
      // adjusted, if this is an "after text" position and our anchor node is an
      // embedded object (as determined by `IsEmbeddedObjectInParent()`).
      // ++kRootWebArea "<embedded_object>"
      // ++++kParagraph "Hello"
      // TextPosition anchor=kRootWebArea text_offset=1
      // should be translated into the following text position
      // TextPosition anchor=kParagraph text_offset=5 annotated_text=Hello<>
      // and not into the following one
      // TextPosition anchor=kParagraph text_offset=1 annotated_text=<H>ello
      if (child->IsEmbeddedObjectInParent() &&
          offset_in_parent == child->MaxTextOffsetInParent()) {
        offset_in_parent -= child->MaxTextOffsetInParent();
        offset_in_parent += child->MaxTextOffset();
      }

      text_position = std::move(child);
    }

    DCHECK(text_position->IsLeafTextPosition());
    text_position->text_offset_ = offset_in_parent;
    // A leaf Text position is always downstream since there is no ambiguity as
    // to whether it refers to the end of the current or the start of the next
    // line.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    return text_position;
  }

  // Converts to a text position that is suitable for passing into the renderer
  // as a selection endpoint. In other words, converts to a position that is
  // suitable for setting as a DOM selection range endpoint.
  //
  // When blink is asked to set selection, it expects a text position to be
  // anchored to the text node (otherwise a generic tree position is assumed
  // and the offset is interpreted as a child index).
  AXPositionInstance AsDomSelectionPosition() const {
    if (IsNullPosition() || GetAnchor()->data().IsAtomicTextField())
      return Clone();

    AXPositionInstance text_position = AsLeafTextPosition();
    if (text_position->GetAnchor() && text_position->GetAnchor()->GetRole() ==
                                          ax::mojom::Role::kInlineTextBox) {
      return text_position->CreateParentPosition();
    }
    return text_position;
  }

  // We deploy three strategies in order to find the best match for an ignored
  // position in the accessibility tree:
  //
  // 1. In the case of a text position, we move up the parent positions until we
  // find the next unignored equivalent parent position. We don't do this for
  // tree positions because, unlike text positions which maintain the
  // corresponding text offset in the text content of the parent node, tree
  // positions would lose some information every time a parent position is
  // computed. In other words, the parent position of a tree position is, in
  // most cases, non-equivalent to the child position.
  // 2. If no equivalent and unignored parent position can be computed, we try
  // computing the leaf equivalent position. If this is unignored, we return it.
  // This can happen both for tree and text positions, provided that the leaf
  // node and its text content is visible to platform APIs, i.e. it's unignored.
  // 3. As a last resort, we move either to the next or previous unignored
  // position in the accessibility tree, based on the "adjustment_behavior".
  AXPositionInstance AsUnignoredPosition(
      AXPositionAdjustmentBehavior adjustment_behavior) const {
    if (IsNullPosition() || !IsIgnored())
      return Clone();

    AXPositionInstance leaf_tree_position = AsLeafTreePosition();

    // If this is a text position, first try moving up to a parent equivalent
    // position and check if the resulting position is still ignored. This
    // won't result in the loss of any information. We can't do that in the
    // case of tree positions, because we would be better off to move to the
    // next or previous position within the same anchor, as this would lose
    // less information than moving to a parent equivalent position.
    //
    // Text positions are considered ignored if either the current anchor is
    // ignored, or if the equivalent leaf tree position is ignored.
    // If this position is a leaf text position, or the equivalent leaf tree
    // position is ignored, then it's not possible to create an ancestor text
    // position that is unignored.
    if (IsTextPosition() && !IsLeafTextPosition() &&
        !leaf_tree_position->IsIgnored()) {
      AXPositionInstance unignored_position = CreateParentPosition();
      while (!unignored_position->IsNullPosition()) {
        // Since the equivalent leaf tree position is unignored, search for the
        // first unignored ancestor anchor and return that text position.
        if (!unignored_position->GetAnchor()->IsIgnored()) {
          DCHECK(!unignored_position->IsIgnored());
          return unignored_position;
        }
        unignored_position = unignored_position->CreateParentPosition();
      }
    }

    // There is a possibility that the position became unignored by moving to a
    // leaf equivalent position. Otherwise, we have no choice but to move to the
    // next or previous position and lose some information in the process.
    while (leaf_tree_position->IsIgnored()) {
      switch (adjustment_behavior) {
        case AXPositionAdjustmentBehavior::kMoveForward:
          leaf_tree_position = leaf_tree_position->CreateNextLeafTreePosition();
          break;
        case AXPositionAdjustmentBehavior::kMoveBackward:
          leaf_tree_position =
              leaf_tree_position->CreatePreviousLeafTreePosition();
          // in case the unignored leaf node contains some text, ensure that the
          // resulting position is an "after text" position, as such a position
          // would be the closest to the ignored one, given the fact that we are
          // moving backwards through the tree.
          leaf_tree_position =
              leaf_tree_position->CreatePositionAtEndOfAnchor();
          break;
      }
    }

    if (IsTextPosition())
      return leaf_tree_position->AsTextPosition();
    return leaf_tree_position;
  }

  // Searches backward and forward from this position until it finds the given
  // text boundary, and creates an AXRange that spans from the former to the
  // latter. The resulting AXRange is always a forward range: its anchor always
  // comes before its focus in document order. The resulting AXRange is bounded
  // by the anchor of this position and the requested boundary type, i.e. the
  // AXMovementOptions is set to `AXBoundaryBehavior::kStopAtAnchorBoundary` and
  // `AXBoundaryDetection::kCheckInitialPosition`. The
  // exception is `ax::mojom::TextBoundary::kWebPage`, where this behavior won't
  // make sense. This behavior is based on current platform needs and might be
  // relaxed if necessary in the future.
  //
  // Observe that `expand_behavior` has an effect only when this position is
  // between text units, e.g. between words, lines, paragraphs, etc. Also,
  // please note that `expand_behavior` should have no effect for
  // `ax::mojom::TextBoundary::kObject` and `ax::mojom::TextBoundary::kWebPage`
  // because the range should be the same regardless if we first move left or
  // right.
  AXRangeType ExpandToEnclosingTextBoundary(
      ax::mojom::TextBoundary boundary,
      AXRangeExpandBehavior expand_behavior) const {
    AXMovementOptions left_options{AXBoundaryBehavior::kStopAtAnchorBoundary,
                                   AXBoundaryDetection::kCheckInitialPosition};
    AXMovementOptions right_options{
        AXBoundaryBehavior::kStopAtAnchorBoundary,
        AXBoundaryDetection::kDontCheckInitialPosition};
    if (boundary == ax::mojom::TextBoundary::kWebPage) {
      left_options =
          right_options = {AXBoundaryBehavior::kCrossBoundary,
                           AXBoundaryDetection::kDontCheckInitialPosition};
    }

    switch (expand_behavior) {
      case AXRangeExpandBehavior::kLeftFirst: {
        AXPositionInstance left_position = CreatePositionAtTextBoundary(
            boundary, ax::mojom::MoveDirection::kBackward, left_options);
        AXPositionInstance right_position =
            left_position->CreatePositionAtTextBoundary(
                boundary, ax::mojom::MoveDirection::kForward, right_options);
        return AXRangeType(std::move(left_position), std::move(right_position));
      }
      case AXRangeExpandBehavior::kRightFirst: {
        AXPositionInstance right_position = CreatePositionAtTextBoundary(
            boundary, ax::mojom::MoveDirection::kForward, left_options);
        AXPositionInstance left_position =
            right_position->CreatePositionAtTextBoundary(
                boundary, ax::mojom::MoveDirection::kBackward, right_options);
        return AXRangeType(std::move(left_position), std::move(right_position));
      }
    }
  }

  // Starting from this position, moves in the given direction until it finds
  // the given text boundary, and creates a new position at that location.
  //
  // When a boundary has the "StartOrEnd" suffix, it means that this method will
  // find the start boundary when moving in the backward direction, and the end
  // boundary when moving in the forward direction.
  AXPositionInstance CreatePositionAtTextBoundary(
      ax::mojom::TextBoundary boundary,
      ax::mojom::MoveDirection direction,
      AXMovementOptions options) const {
    AXPositionInstance resulting_position = CreateNullPosition();
    switch (boundary) {
      case ax::mojom::TextBoundary::kNone:
        NOTREACHED_IN_MIGRATION();
        break;

      case ax::mojom::TextBoundary::kCharacter:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousCharacterPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextCharacterPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kFormatEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousFormatEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextFormatEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kFormatStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousFormatStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextFormatStartPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kFormatStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousFormatStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextFormatEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kLineEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousLineEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextLineEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kLineStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousLineStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextLineStartPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kLineStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousLineStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextLineEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kObject:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePositionAtStartOfAnchor();
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreatePositionAtEndOfAnchor();
            break;
        }
        break;

      case ax::mojom::TextBoundary::kPageEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousPageEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextPageEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kPageStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousPageStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextPageStartPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kPageStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousPageStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextPageEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kParagraphEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousParagraphEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextParagraphEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kParagraphStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousParagraphStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextParagraphStartPosition(options);
            break;
        }
        break;

      // For UI Automation, empty lines after a paragraph should be merged into
      // the preceding paragraph.
      //
      // See
      // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationtextunits#paragraph
      case ax::mojom::TextBoundary::kParagraphStartSkippingEmptyParagraphs:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position =
                CreatePreviousParagraphStartPositionSkippingEmptyParagraphs(
                    options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position =
                CreateNextParagraphStartPositionSkippingEmptyParagraphs(
                    options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kParagraphStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousParagraphStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextParagraphEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kSentenceEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousSentenceEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextSentenceEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kSentenceStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousSentenceStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextSentenceStartPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kSentenceStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousSentenceStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextSentenceEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kWebPage:
        DCHECK_EQ(options.boundary_behavior, AXBoundaryBehavior::kCrossBoundary)
            << "We can't reach the start of the whole contents if we are "
               "disallowed from crossing boundaries.";
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePositionAtStartOfContent();
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreatePositionAtEndOfContent();
            break;
        }
        break;

      case ax::mojom::TextBoundary::kWordEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousWordEndPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextWordEndPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kWordStart:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousWordStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextWordStartPosition(options);
            break;
        }
        break;

      case ax::mojom::TextBoundary::kWordStartOrEnd:
        switch (direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            break;
          case ax::mojom::MoveDirection::kBackward:
            resulting_position = CreatePreviousWordStartPosition(options);
            break;
          case ax::mojom::MoveDirection::kForward:
            resulting_position = CreateNextWordEndPosition(options);
            break;
        }
        break;
    }

    return resulting_position;
  }

  AXPositionInstance CreatePositionAtStartOfAnchor() const {
    const AXNode* anchor = GetAnchor();
    if (!anchor)
      return CreateNullPosition();

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePositionAtStartOfAnchor(*anchor);
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(*anchor, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }
  }

  AXPositionInstance CreatePositionAtEndOfAnchor() const {
    const AXNode* anchor = GetAnchor();
    if (!anchor)
      return CreateNullPosition();

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePositionAtEndOfAnchor(*anchor);
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(*anchor, MaxTextOffset(),
                                  ax::mojom::TextAffinity::kDownstream);
    }
  }

  // Creates a position at the start of this position's accessibility tree, e.g.
  // at the start of the current iframe, PDF plugin, Views tree, dialog, etc. We
  // don't distinguish between out-of-process and in-process iframes, treating
  // them both as tree boundaries.
  //
  // For a similar method that does not stop at iframe boundaries, see
  // `CreatePositionAtStartOfContent()`.
  AXPositionInstance CreatePositionAtStartOfAXTree() const {
    AXPositionInstance root_position =
        AsTreePosition()
            ->CreateAXTreeRootAncestorPosition(
                ax::mojom::MoveDirection::kBackward)
            ->CreatePositionAtStartOfAnchor();
    if (IsTextPosition())
      root_position = root_position->AsTextPosition();
    DCHECK_EQ(root_position->tree_id_, tree_id_)
        << "`CreatePositionAtStartOfAXTree` should not cross any tree "
           "boundaries, neither return the null position.";
    return root_position;
  }

  // Creates a position at the end of this position's accessibility tree, e.g.
  // at the end of the current iframe, PDF plugin, Views tree, dialog, etc. We
  // don't distinguish between out-of-process and in-process iframes, treating
  // them both as tree boundaries.
  //
  // For a similar method that does not stop at iframe boundaries, see
  // `CreatePositionAtEndOfContent()`.
  AXPositionInstance CreatePositionAtEndOfAXTree() const {
    AXPositionInstance root_position =
        AsTreePosition()->CreateAXTreeRootAncestorPosition(
            ax::mojom::MoveDirection::kBackward);
    AXPositionInstance last_position =
        root_position->CreatePositionAtEndOfAnchor()->AsLeafTreePosition();
    if (IsTextPosition())
      last_position = last_position->AsTextPosition();
    return last_position;
  }

  // Creates a position at the start of all content, e.g. at the start of the
  // whole webpage, PDF plugin, Views tree, dialog (native, ARIA or HTML),
  // window, or the whole desktop.
  //
  // Note that this method will break out of an out-of-process iframe and return
  // a position at the start of the top-level document, but it will not break
  // into the Views tree if present. For a similar method that stops at all
  // iframe boundaries, see `CreatePositionAtStartOfAXTree()`.
  AXPositionInstance CreatePositionAtStartOfContent() const {
    AXPositionInstance root_position =
        AsTreePosition()
            ->CreateRootAncestorPosition(ax::mojom::MoveDirection::kBackward)
            ->CreatePositionAtStartOfAnchor();
    if (IsTextPosition())
      root_position = root_position->AsTextPosition();
    return root_position;
  }

  // Creates a position at the end of all content, e.g. at the end of the whole
  // webpage, PDF plugin, Views tree, dialog (native, ARIA or HTML), window, or
  // the whole desktop.
  //
  // Note that this method will break out of an out-of-process iframe and return
  // a position at the end of the top-level document, but it will not break into
  // the Views tree if present. For a similar method that stops at all iframe
  // boundaries, see `CreatePositionAtEndOfAXTree()`.
  AXPositionInstance CreatePositionAtEndOfContent() const {
    AXPositionInstance root_position =
        AsTreePosition()->CreateRootAncestorPosition(
            ax::mojom::MoveDirection::kBackward);
    AXPositionInstance last_position =
        root_position->CreatePositionAtEndOfAnchor()->AsLeafTreePosition();
    if (IsTextPosition())
      last_position = last_position->AsTextPosition();
    return last_position;
  }

  AXPositionInstance CreateChildPositionAt(int child_index) const {
    if (IsNullPosition() || IsLeaf())
      return CreateNullPosition();

    if (child_index < 0 || child_index >= AnchorChildCount())
      return CreateNullPosition();

    const AXNode* child_anchor =
        GetAnchor()->GetChildAtIndexCrossingTreeBoundary(child_index);
    if (!child_anchor)
      return CreateNullPosition();

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED_IN_MIGRATION();
        return CreateNullPosition();
      case AXPositionKind::TREE_POSITION:
        return CreateTreePositionAtStartOfAnchor(*child_anchor);
      case AXPositionKind::TEXT_POSITION:
        return CreateTextPosition(*child_anchor, 0 /* text_offset */,
                                  ax::mojom::TextAffinity::kDownstream);
    }

    return CreateNullPosition();
  }

  // Creates a parent equivalent position.
  //
  // Note that "move_direction" is only taken into consideration when all of
  // these three conditions apply: This is a text position, we are in the
  // process of searching for a text boundary, and this is a platform where
  // child nodes are represented by "object replacement characters". On such
  // platforms, the `IsEmbeddedObjectInParent` method returns true. We need to
  // decide whether to create a parent equivalent position that is before or
  // after the child node, since moving to a parent position would always cause
  // us to lose some information. We can't simply re-use the text offset of the
  // child position because by definition the parent node doesn't include all
  // the text of the child node, but only a single "object replacement
  // character".
  //
  // staticText name='Line one' IA2-hypertext='<embedded_object>'
  // ++inlineTextBox name='Line one'
  //
  // If we are given a text position pointing to somewhere inside the
  // inlineTextBox, and we move to the parent equivalent position, we need to
  // decide whether the parent position would be set to point to before the
  // object replacement character or after it. Both are valid, depending on the
  // direction on motion, e.g. if we are trying to find the start of the line
  // vs. the end of the line.
  AXPositionInstance CreateParentPosition(
      ax::mojom::MoveDirection move_direction =
          ax::mojom::MoveDirection::kForward) const {
    if (IsNullPosition())
      return CreateNullPosition();

    const AXNode* parent_anchor = GetAnchor()->GetParentCrossingTreeBoundary();
    if (!parent_anchor)
      return CreateNullPosition();

    const AXTree* tree = parent_anchor->tree();
    DCHECK(tree);

    switch (kind_) {
      case AXPositionKind::NULL_POSITION:
        NOTREACHED_IN_MIGRATION();
        return CreateNullPosition();

      case AXPositionKind::TREE_POSITION: {
        if (IsLeafNodeForTreePosition(*parent_anchor)) {
          if (AtEndOfAnchor() ||
              move_direction == ax::mojom::MoveDirection::kForward) {
            // If this position is an "after children" or an "after text"
            // position inside of a leaf, or we are seeking a parent position
            // for a forward movement operation with a parent leaf anchor,
            // return a position at the end of the parent anchor.
            return CreateTreePositionAtEndOfAnchor(*parent_anchor);
          }
          // If we are seeking a parent position for a backward movement
          // operation, return a position at the start of the parent anchor.
          return CreateTreePositionAtStartOfAnchor(*parent_anchor);
        }

        // If this position is an "after children" or an "after text" position,
        // return either an "after children" position on the parent anchor, or a
        // position anchored at the next child, depending on whether this is the
        // last child in its parent anchor.
        int child_index = AnchorIndexInParent();
        if (AtEndOfAnchor())
          return CreateTreePosition(*parent_anchor, (child_index + 1));

        switch (move_direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            return CreateNullPosition();
          case ax::mojom::MoveDirection::kBackward:
            // "move_direction" is only important when this position is an
            // "embedded object in parent", i.e., when this position's anchor is
            // represented by an "object replacement character" in the text of
            // its parent anchor. In this case we need to keep the child index
            // to be right before the "object replacement character". If this is
            // not an "embedded object in parent", then we simply need to use
            // the "AnchorIndexInParent" for the child index. However, since
            // "AnchorIndexInParent" always returns a child index that is before
            // any "object replacement character" in our parent, we use that for
            // both situations.
            return CreateTreePosition(*parent_anchor, child_index);
          case ax::mojom::MoveDirection::kForward:
            // "move_direction" is only important when this position is an
            // "embedded object in parent", i.e., when this position's anchor is
            // represented by an "object replacement character" in the text of
            // its parent anchor. In this case we need to move the child index
            // to be after the "object replacement character" when this position
            // is not at the start of its anchor. If this is not an "embedded
            // object in parent", then we simply need to use the
            // "AnchorIndexInParent" for the child index.
            if (!AtStartOfAnchor() && IsEmbeddedObjectInParent())
              ++child_index;
            return CreateTreePosition(*parent_anchor, child_index);
        }
      }

      case AXPositionKind::TEXT_POSITION: {
        // On some platforms, such as Android, Mac and Chrome OS, the text
        // content of a node is made up by concatenating the text of child
        // nodes. On other platforms, such as Windows IAccessible2 and Linux
        // ATK, child nodes are represented by a single "object replacement
        // character".
        //
        // If our parent's text content is a concatenation of all its children's
        // text, we need to maintain the affinity and compute the corresponding
        // text offset. Otherwise, we have no choice but to return a position
        // that is either before or after this child, losing some information in
        // the process. Regardless to whether our parent contains all our text,
        // we always recompute the affinity when the position is after the
        // child.
        //
        // Recomputing the affinity in the latter situation is important because
        // even though a text position might unambiguously be at the end of a
        // line, its parent position might be the same as the parent position of
        // a position that represents the start of the next line. For example:
        //
        // staticText name='Line oneLine two'
        // ++inlineTextBox name='Line one'
        // ++inlineTextBox name='Line two'
        //
        // If the original position is at the end of the inline text box for
        // "Line one", then the resulting parent equivalent position would be
        // the same as the one that would have been computed if the original
        // position were at the start of the inline text box for "Line two".

        const int max_text_offset = MaxTextOffset();

        // TODO(crbug.com/40885940): temporary disabled until ax position
        // autocorrection issue is fixed.
        // DCHECK_LE(text_offset_, max_text_offset);

        const int max_text_offset_in_parent =
            IsEmbeddedObjectInParent()
                ? AXNode::kEmbeddedObjectCharacterLengthUTF16
                : max_text_offset;
        int parent_offset = AnchorTextOffsetInParent();
        ax::mojom::TextAffinity parent_affinity = affinity_;

        // "max_text_offset > 0" is required to filter out anchor nodes that are
        // either ignored or empty, i.e. those that contribute no text content
        // or hypertext to their parent's text representation. (See example in
        // the "else" block.)
        if (max_text_offset > 0 &&
            max_text_offset == max_text_offset_in_parent) {
          // Our parent contains all our text. No information would be lost when
          // moving to a parent equivalent position. It turns out, that even in
          // the unusual case where there is a single character in our anchor's
          // text content but our anchor is represented in our parent by an
          // "embedded object replacement character" and not by our text
          // content, the outcome is still correct.
          parent_offset += text_offset_;
        } else {
          // Our parent represents our anchor node using an "object replacement"
          // character in its text representation. Or, our anchor is a text node
          // that is ignored or empty, and so contributes no text in its
          // parent's text representation. For example:
          // ++kTextField "Before after."
          // ++++kStaticText "Before "
          // ++++kStaticText "Ignored text" ignored
          // ++++kStaticText "after."
          // TextPosition anchor=kStaticText (ignored) text_offset=2
          // annotated_text="Ig<n>ored text"

          if (text_offset_ > 0 && text_offset_ < max_text_offset) {
            // If this is a "before text" or an "after text" position, i.e. if
            // "text_offset_" == 0 or "max_text_offset", then the child position
            // is clearly before or clearly after any "object replacement
            // character". No information would be lost when moving to a parent
            // equivalent position, including affinity which can easily be
            // computed. Otherwise, we should decide whether to set the parent
            // position to be before or after the child, based on the direction
            // of motion, and also reset the affinity.
            switch (move_direction) {
              case ax::mojom::MoveDirection::kNone:
                NOTREACHED_IN_MIGRATION();
                return CreateNullPosition();
              case ax::mojom::MoveDirection::kBackward:
                // Keep the offset to be right before the embedded object
                // character.
                break;
              case ax::mojom::MoveDirection::kForward:
                // Set the offset to be after the embedded object character.
                parent_offset += max_text_offset_in_parent;
                break;
            }
          } else if (text_offset_ == max_text_offset) {
            // Clearly, this is an "after text" position. The text offset should
            // be after the "object replacement character". No information would
            // be lost when moving to a parent equivalent position, including
            // affinity which can easily be computed.
            parent_offset += max_text_offset_in_parent;
          }

          // The original affinity doesn't apply any more. In most cases, it
          // should be downstream, unless there is an ambiguity as to whether
          // the parent position is between the end of one line and the start of
          // the next. We perform this check below.
          parent_affinity = ax::mojom::TextAffinity::kDownstream;
        }

        // There are two cases for which we need to set an upstream affinity on
        // the parent position:
        //
        // Case 1:
        // If the current position is pointing at the end of its anchor, we need
        // to check if the parent position has introduced ambiguity as to
        // whether it refers to the end of a line or the start of the next.
        // Ambiguity is only present when the parent position points to a text
        // offset that is neither at the start nor at the end of its anchor. We
        // check for ambiguity by creating the parent position and testing if it
        // is erroneously at the start of the next line. Given that the current
        // position, by the nature of being at the end of its anchor, could only
        // be at end of line, the fact that the parent position is also
        // determined to be at start of line demonstrates the presence of
        // ambiguity which is resolved by setting its affinity to upstream.
        //
        // We could not have checked if the child was at the end of the line,
        // because our "AtEndOfLine" predicate takes into account trailing line
        // breaks, which would create false positives.
        //
        // Case 2:
        // If the current position is followed by a generated newline character,
        // which is a character that is actually not represented in the text
        // content of the nodes but should still act as a stop when navigating
        // to the previous/next character.
        //
        // When this is the case, we almost always want to set an upstream
        // affinity on the `parent_position`. The only exception is when our
        // current position is contained on a descendant of an empty object,
        // because an empty object will hide the textual representation of its
        // descendants, including the generated newline characters, by exposing
        // a only the empty object character.
        //
        // Example:
        // ++1 kLink "<embedded_object>"
        // ++++2 kStaticText "hello" IsLineBreakingObject=true
        // ++++++3 kInlineTextBox "hello"
        // ++++4 kStaticText "world"
        // ++++++5 kInlineTextBox "world"
        //
        // While there should be a generated newline character at the end of a
        // position created on node 2, there won't be one represented to the
        // user because node 1 simply exposes the empty object character and not
        // its children's text.

        AXPositionInstance parent_position =
            CreateTextPosition(*parent_anchor, parent_offset, parent_affinity);
        if ((AtEndOfAnchor() && !parent_position->AtStartOfAnchor() &&
             !parent_position->AtEndOfAnchor() &&
             parent_position->AtStartOfLine()) ||
            (!IsEmbeddedObjectInParent() && IsFollowedByGeneratedNewline())) {
          parent_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
        }
        return parent_position;
      }
    }
  }

  // Creates the next tree position that is anchored at a leaf node of the
  // AXTree.
  AXPositionInstance CreateNextLeafTreePosition() const {
    return CreateNextLeafTreePosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates the previous tree position that is anchored at a leaf node of the
  // AXTree.
  AXPositionInstance CreatePreviousLeafTreePosition() const {
    return CreatePreviousLeafTreePosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates the next text position that is anchored at a leaf node of the
  // AXTree.
  AXPositionInstance CreateNextLeafTextPosition() const {
    return CreateNextLeafTextPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Creates the previous text position that is anchored at a leaf node of the
  // AXTree.
  AXPositionInstance CreatePreviousLeafTextPosition() const {
    return CreatePreviousLeafTextPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  AXPositionInstance CreateNextPositionAtAnchorWithText() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    do {
      text_position = text_position->CreateNextLeafTextPosition(
          base::BindRepeating(&AbortMoveAtRootBoundary));
    } while (!text_position->IsNullPosition() &&
             (text_position->IsIgnored() || !text_position->MaxTextOffset()));

    return text_position;
  }

  AXPositionInstance CreatePreviousPositionAtAnchorWithText() const {
    AXPositionInstance text_position = AsLeafTextPosition();
    do {
      text_position = text_position->CreatePreviousLeafTextPosition(
          base::BindRepeating(&AbortMoveAtRootBoundary));
    } while (!text_position->IsNullPosition() &&
             (text_position->IsIgnored() || !text_position->MaxTextOffset()));

    return text_position->CreatePositionAtEndOfAnchor();
  }

  // Generated newline characters are not part of any AXNode in the AXTree. They
  // are appended to the accessible textual representation exposed to ATs in
  // AXRange::GetText. They are necessary to expose the implicit newlines
  // created from the layout breaks to screen reader users. For example, the
  // following HTML will create an implicit line break after "hello":
  //
  // <div contenteditable>
  //     <div>hello</div>
  //     <div>world</div>
  // </div>
  //
  // Even though there is not explicit line break in this template, the text
  // returned for this contenteditable is "hello\nworld". In order to allow
  // screen reader users to navigate (either using the caret or the controls
  // built-in the AT), we need to create character stops around these
  // generated characters.
  //
  // We can only create character stops around generated newline characters
  // when empty objects are represented in the accessible text (ie. when the
  // behavior is set to
  // `AXEmbeddedObjectBehavior::kExposeCharacterForHypertext`). Otherwise,
  // there's a risk that `CreateParentPosition` will create a position that
  // doesn't point to the same character. This is because a position located
  // right before a generated newline character will be represented in the
  // parent ancestor with an upstream affinity.
  //
  // Let's consider this AXTree:
  // 1 root
  // ++2 button
  // ++3 checkbox
  // ++4 static text
  // ++++5 inline text box "abc"
  //
  // The text representation for the entire document, including the generated
  // newlines, will be "\n\nabc" if the empty objects do not expose the empty
  // object character. If we were to allow character stops at generated
  // newline characters, it would be possible to create a next and previous
  // position located before/after a generated newline character. However,
  // creating an equivalent position in an ancestor would potentially lead to
  // an incorrect position.
  //
  // Example:
  // leaf_position_1: anchor=2, text_offset=0, affinity=downstream
  // leaf_position_2: anchor=3, text_offset=0, affinity=downstream
  //
  // `leaf_position_1` and `leaf_position_2` should both return true for
  // `AtStartOfParagraph` and `AtEndOfParagraph`. Calling
  // `CreateParentPosition` on each of those will respectively create:
  // parent_position_1: anchor=1, text_offset=0, affinity=upstream
  // parent_position_1: anchor=1, text_offset=0, affinity=upstream
  //
  // ...which are both the same. `CreateParentPosition` is relatively important
  // when it comes to moving the position by character because
  // `CreatePreviousCharacterPosition` uses it in many cases to create the
  // previous position on the same anchor as the original position.
  //
  // This is a quirk of the current implementation which cannot easily be fixed,
  // because when object replacement characters are missing from empty objects
  // (sucha as a checkbox without a label, etc.) any leaf equivalent position
  // from one of the objects' ancestors would skip the empty object and create
  // the child position at the first non-empty object. Consequently,
  // CreateParentPosition cannot easily determine the correct affinity when
  // computing parent equivalent positions from positions on empty objects, i.e.
  // like the example positions given here. Skipping empty objects when creating
  // leaf equivalent positions had to be done, because on platforms where they
  // are not represented by an object replacement character, the AT does not
  // even know they are there.
  bool AllowsCharacterStopsOnGeneratedNewline() const {
    return g_ax_embedded_object_behavior ==
               AXEmbeddedObjectBehavior::kExposeCharacterForHypertext ||
           g_ax_embedded_object_behavior ==
               AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent ||
           !IsInUnignoredEmptyObject();
  }

  bool IsFollowedByGeneratedNewline() const {
    // Hard line breaks (such as <br> in HTML) are discounted because generated
    // newlines are only inserted between neighboring block elements (such as
    // <p>Hello</p><p>world</p>). Generated newlines are always a product of
    // layout and have no corresponding AXNode to it. Hard line breaks have
    // a matching AXNode and thus do not require to be treated differently.
    AXPositionInstance leaf_text_position = AsLeafTextPosition();
    if (!leaf_text_position->AllowsCharacterStopsOnGeneratedNewline() ||
        leaf_text_position->affinity_ != ax::mojom::TextAffinity::kDownstream ||
        leaf_text_position->GetAnchor()->IsLineBreak() ||
        !leaf_text_position->AtEndOfParagraph()) {
      return false;
    }

    AXPositionInstance next_position =
        leaf_text_position->CreateNextPositionAtAnchorWithText();
    return next_position->AllowsCharacterStopsOnGeneratedNewline() &&
           !next_position->IsNullPosition() &&
           !next_position->GetAnchor()->IsLineBreak() &&
           next_position->AtStartOfParagraph();
  }

  bool IsPrecededByGeneratedNewline() const {
    // Hard line breaks (such as <br> in HTML) are discounted because generated
    // newlines are only inserted between neighboring block elements (such as
    // <p>Hello</p><p>world</p>). Generated newlines are always a product of
    // layout and have no corresponding AXNode to it. Hard line breaks have
    // a matching AXNode and thus do not require to be treated differently.
    AXPositionInstance leaf_text_position = AsLeafTextPosition();
    if (!leaf_text_position->AllowsCharacterStopsOnGeneratedNewline() ||
        leaf_text_position->GetAnchor()->IsLineBreak() ||
        !leaf_text_position->AtStartOfParagraph()) {
      return false;
    }

    AXPositionInstance previous_position =
        leaf_text_position->CreatePreviousPositionAtAnchorWithText();
    if (previous_position->IsNullPosition()) {
      // When it's null, it's because we've reached the beginning of the tree.
      // We need to make sure we didn't skip any generated newlines that could
      // have been before the start of the page and our current position.
      AXPositionInstance start_of_content =
          CreatePositionAtStartOfContent()->AsLeafTextPosition();
      return *start_of_content < *this &&
             start_of_content->IsFollowedByGeneratedNewline();
    }

    return previous_position->AllowsCharacterStopsOnGeneratedNewline() &&
           !previous_position->GetAnchor()->IsLineBreak() &&
           previous_position->AtEndOfParagraph();
  }

  // Returns a text position located right before the next character (from this
  // position) in the tree's text representation, following these conditions:
  //
  //   - If this position is at the end of its anchor, normalize it to the start
  //   of the next text anchor, regardless of the position's affinity.
  //   Both text positions are equal when compared, but we consider the start of
  //   an anchor to be a position BEFORE its first character and the end of the
  //   previous to be AFTER its last character.
  //
  //   - Skip any empty text anchors; they're "invisible" to the text
  //   representation and the next character could be ahead.
  //
  //   - Return a null position if there is no next character forward.
  //
  // If possible, return a position anchored at the current position's anchor;
  // this is necessary because we don't want to return any position that might
  // be located in the shadow DOM or in a position anchored at a node that is
  // not visible to a specific platform's APIs.
  //
  // Also, |text_offset| is adjusted to point to a valid character offset, i.e.
  // it cannot be pointing to a low surrogate pair or to the middle of a
  // grapheme cluster.
  AXPositionInstance AsLeafTextPositionBeforeCharacter() const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance leaf_text_position = AsLeafTextPosition();
    if (leaf_text_position->IsFollowedByGeneratedNewline()) {
      return leaf_text_position;
    }

    AXPositionInstance text_position = AsTextPosition();

    // In case the input affinity is upstream, reset it to downstream.
    //
    // This is to ensure that when we find the equivalent leaf text position, it
    // will be at the start of anchor if the original position is anchored to a
    // node higher up in the tree and pointing to a text offset that falls on
    // the boundary between two leaf nodes. In other words, the returned
    // position will always be "before character".
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
    text_position = text_position->AsLeafTextPosition();
    DCHECK(!text_position->IsNullPosition())
        << "Adjusting to a leaf position should never turn a non-null position "
           "into a null one.";

    if (!text_position->IsIgnored() && !text_position->AtEndOfAnchor()) {
      std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
          text_position->GetGraphemeIterator();
      // The following situation should not be possible but there are existing
      // crashes in the field.
      //
      // TODO(nektar): Remove this workaround as soon as the source of the bug
      // is identified.
      if (text_position->text_offset_ < 0 ||
          text_position->text_offset_ > text_position->MaxTextOffset()) {
        SANITIZER_NOTREACHED() << "Offset range error:\n" << ToDebugString();
        return CreateNullPosition();
      }
      DCHECK_GE(text_position->text_offset_, 0);
      DCHECK_LE(text_position->text_offset_, text_position->MaxTextOffset());
      while (!text_position->AtStartOfAnchor() &&
             (!gfx::IsValidCodePointIndex(
                  text_position->GetText(),
                  static_cast<size_t>(text_position->text_offset_)) ||
              (grapheme_iterator &&
               !grapheme_iterator->IsGraphemeBoundary(
                   static_cast<size_t>(text_position->text_offset_))))) {
        --text_position->text_offset_;
      }
      return text_position;
    }

    return text_position->CreateNextPositionAtAnchorWithText();
  }

  // Returns a text position located right after the previous character (from
  // this position) in the tree's text representation.
  //
  // See `AsLeafTextPositionBeforeCharacter`, as this is its "reversed" version.
  AXPositionInstance AsLeafTextPositionAfterCharacter() const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance leaf_text_position = AsLeafTextPosition();
    if (leaf_text_position->IsPrecededByGeneratedNewline())
      return leaf_text_position;

    AXPositionInstance text_position = AsTextPosition();
    // Temporarily set the affinity to upstream.
    //
    // This is to ensure that when we find the equivalent leaf text position, it
    // will be at the end of anchor if the original position is anchored to a
    // node higher up in the tree and pointing to a text offset that falls on
    // the boundary between two leaf nodes. In other words, the returned
    // position will always be "after character".
    text_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
    text_position = text_position->AsLeafTextPosition();
    DCHECK(!text_position->IsNullPosition())
        << "Adjusting to a leaf position should never turn a non-null position "
           "into a null one.";

    if (!text_position->IsIgnored() && !text_position->AtStartOfAnchor()) {
      std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
          text_position->GetGraphemeIterator();
      // The following situation should not be possible but there are existing
      // crashes in the field.
      //
      // TODO(nektar): Remove this workaround as soon as the source of the bug
      // is identified.
      if (text_position->text_offset_ < 0 ||
          text_position->text_offset_ > text_position->MaxTextOffset()) {
        SANITIZER_NOTREACHED() << "Offset range error:\n" << ToDebugString();
        return CreateNullPosition();
      }
      DCHECK_GE(text_position->text_offset_, 0);
      DCHECK_LE(text_position->text_offset_, text_position->MaxTextOffset());
      while (!text_position->AtEndOfAnchor() &&
             (!gfx::IsValidCodePointIndex(
                  text_position->GetText(),
                  static_cast<size_t>(text_position->text_offset_)) ||
              (grapheme_iterator &&
               !grapheme_iterator->IsGraphemeBoundary(
                   static_cast<size_t>(text_position->text_offset_))))) {
        ++text_position->text_offset_;
      }

      // Reset the affinity to downstream, because an upstream affinity doesn't
      // make sense on a leaf anchor.
      text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      return text_position;
    }

    return text_position->CreatePreviousPositionAtAnchorWithText();
  }

  // Creates a position pointing to before the next character, which is defined
  // as the start of the next grapheme cluster. Also, ensures that the created
  // position will not point to a low surrogate pair.
  //
  // A grapheme cluster is what an end-user would consider a character and it
  // could include a letter with additional diacritics. It could be more than
  // one Unicode code unit in length.
  //
  // See also http://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
  AXPositionInstance CreateNextCharacterPosition(
      AXMovementOptions options) const {
    if (options.boundary_behavior ==
            AXBoundaryBehavior::kStopAtAnchorBoundary &&
        AtEndOfAnchor()) {
      return Clone();
    }

    AXPositionInstance text_position = AsLeafTextPositionBeforeCharacter();
    if (text_position->IsNullPosition()) {
      if (options.boundary_behavior != AXBoundaryBehavior::kCrossBoundary)
        text_position = Clone();

      return text_position;
    }

    if (text_position->IsFollowedByGeneratedNewline()) {
      return CreateNextPositionAtAnchorWithText();
    }

    // Calling "AsLeafTextPositionBeforeCharacter" should have created a text
    // position that is either at a grapheme boundary, or a null position. If
    // our text offset is pointing to a position that is in the middle of a
    // grapheme cluster, we should not erroneously assume that we are at a
    // character boundary and stop because we had been asked to "stop if already
    // at boundary". However, we should not modify our position if
    // `AsLeafTextPositionBeforeCharacter` has simply moved us to the start of
    // the next leaf anchor because we originally happened to be at the end of
    // our current anchor. We also need to ensure that we are comparing two
    // positions that have the same affinity, since
    // `AsLeafTextPositionBeforeCharacter` resets the affinity to downstream,
    // while the original affinity might have been upstream.
    if (options.boundary_behavior ==
            AXBoundaryBehavior::kStopAtAnchorBoundary &&
        options.boundary_detection ==
            AXBoundaryDetection::kCheckInitialPosition &&
        (AtEndOfAnchor() || *text_position == *CloneWithDownstreamAffinity())) {
      return Clone();
    }

    int max_text_offset = text_position->MaxTextOffset();
    DCHECK_LT(text_position->text_offset_, max_text_offset);
    std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
        text_position->GetGraphemeIterator();
    do {
      ++text_position->text_offset_;
    } while (text_position->text_offset_ < max_text_offset &&
             grapheme_iterator &&
             !grapheme_iterator->IsGraphemeBoundary(
                 static_cast<size_t>(text_position->text_offset_)));
    DCHECK_GT(text_position->text_offset_, 0);
    DCHECK_LE(text_position->text_offset_, text_position->MaxTextOffset());

    // If the character boundary is in the same subtree, return a position
    // rooted at this position's anchor. This is necessary because we don't want
    // to return a position that might be in the shadow DOM when this position
    // is not.
    const AXNode* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(
          common_anchor, ax::mojom::MoveDirection::kForward);
    } else if (options.boundary_behavior ==
               AXBoundaryBehavior::kStopAtAnchorBoundary) {
      // If the next character position crosses the current anchor boundary
      // with kStopAtAnchorBoundary, snap to the end of the current anchor.
      return CreatePositionAtEndOfAnchor();
    }

    // Even if the resulting position is right on a soft line break, affinity is
    // defaulted to downstream so that this method will always produce the same
    // result regardless of the direction of motion or the input affinity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (IsTreePosition())
      return text_position->AsTreePosition();
    return text_position;
  }

  // Creates a position pointing to before the previous character, which is
  // defined as the start of the previous grapheme cluster. Also, ensures that
  // the created position will not point to a low surrogate pair.
  //
  // See the comment above `CreateNextCharacterPosition` for the definition of a
  // grapheme cluster.
  AXPositionInstance CreatePreviousCharacterPosition(
      AXMovementOptions options) const {
    if (options.boundary_behavior ==
            AXBoundaryBehavior::kStopAtAnchorBoundary &&
        AtStartOfAnchor()) {
      return Clone();
    }

    AXPositionInstance text_position = AsLeafTextPositionAfterCharacter();
    if (text_position->IsNullPosition()) {
      if (options.boundary_behavior != AXBoundaryBehavior::kCrossBoundary)
        text_position = Clone();

      return text_position;
    }

    // Calling "AsLeafTextPositionAfterCharacter" should have created a text
    // position that is either at the start of an anchor that is preceded by a
    // generated newline, at a grapheme boundary or a null position.
    if (text_position->IsPrecededByGeneratedNewline()) {
      // When `text_position` is right after a generated newline character, we
      // should create a position located at the end of the previous anchor.
      text_position = CreatePreviousPositionAtAnchorWithText();
      DCHECK(!text_position->IsNullPosition());
    } else {
      // If our text offset is pointing to a position that is in the middle of a
      // grapheme cluster, we should not erroneously assume that we are at a
      // character boundary and stop because we had been asked to "stop if
      // already at boundary". However, we should not modify our position if
      // `AsLeafTextPositionAfterCharacter` has simply moved us to the end of
      // the previous leaf anchor because we originally happened to be at the
      // start of our current anchor. We also need to ignore any differences
      // that might be due to the affinity, because that should not be a
      // determining factor as to whether we would stop if we are already at
      // boundary or not.
      if (options.boundary_behavior ==
              AXBoundaryBehavior::kStopAtAnchorBoundary &&
          options.boundary_detection ==
              AXBoundaryDetection::kCheckInitialPosition &&
          (AtStartOfAnchor() ||
           *text_position == *CloneWithUpstreamAffinity() ||
           *text_position == *CloneWithDownstreamAffinity())) {
        return Clone();
      }

      DCHECK_GT(text_position->text_offset_, 0);
      std::unique_ptr<base::i18n::BreakIterator> grapheme_iterator =
          text_position->GetGraphemeIterator();
      do {
        --text_position->text_offset_;
      } while (!text_position->AtStartOfAnchor() && grapheme_iterator &&
               !grapheme_iterator->IsGraphemeBoundary(
                   static_cast<size_t>(text_position->text_offset_)));
      DCHECK_GE(text_position->text_offset_, 0);
      DCHECK_LT(text_position->text_offset_, text_position->MaxTextOffset());
    }

    // The character boundary should be in the same subtree. Return a position
    // rooted at this position's anchor. This is necessary because we don't want
    // to return a position that might be in the shadow DOM when this position
    // is not.
    const AXNode* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position = text_position->CreateAncestorPosition(
          common_anchor, ax::mojom::MoveDirection::kBackward);
    } else if (options.boundary_behavior ==
               AXBoundaryBehavior::kStopAtAnchorBoundary) {
      // If the previous character position crosses the current anchor boundary
      // with StopAtAnchorBoundary, snap to the start of the current anchor.
      return CreatePositionAtStartOfAnchor();
    }

    // Even if the resulting position is right on a soft line break, affinity is
    // defaulted to downstream so that this method will always produce the same
    // result regardless of the direction of motion or the input affinity.
    text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;

    if (IsTreePosition())
      return text_position->AsTreePosition();
    return text_position;
  }

  AXPositionInstance CreateNextWordStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordStartOffsetsFunc));
  }

  AXPositionInstance CreatePreviousWordStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordStartOffsetsFunc));
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreateNextWordEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordEndOffsetsFunc));
  }

  // Word end positions are one past the last character of the word.
  AXPositionInstance CreatePreviousWordEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfWordPredicate),
        base::BindRepeating(&AtEndOfWordPredicate),
        base::BindRepeating(&GetWordEndOffsetsFunc));
  }

  AXPositionInstance CreateNextLineStartPosition(
      AXMovementOptions options) const {
    options.upstream_bounded = true;
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  AXPositionInstance CreatePreviousLineStartPosition(
      AXMovementOptions options) const {
    options.upstream_bounded = true;
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters that separate the lines.
  AXPositionInstance CreateNextLineEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  // Line end positions are one past the last character of the line, excluding
  // any white space or newline characters separating the lines.
  AXPositionInstance CreatePreviousLineEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfLinePredicate),
        base::BindRepeating(&AtEndOfLinePredicate));
  }

  AXPositionInstance CreateNextFormatStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfFormatPredicate),
        base::BindRepeating(&AtEndOfFormatPredicate));
  }

  AXPositionInstance CreatePreviousFormatStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfFormatPredicate),
        base::BindRepeating(&AtEndOfFormatPredicate));
  }

  AXPositionInstance CreateNextFormatEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfFormatPredicate),
        base::BindRepeating(&AtEndOfFormatPredicate));
  }

  AXPositionInstance CreatePreviousFormatEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfFormatPredicate),
        base::BindRepeating(&AtEndOfFormatPredicate));
  }

  AXPositionInstance CreateNextSentenceStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfSentencePredicate),
        base::BindRepeating(&AtEndOfSentencePredicate),
        base::BindRepeating(&GetSentenceStartOffsetsFunc));
  }

  AXPositionInstance CreatePreviousSentenceStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfSentencePredicate),
        base::BindRepeating(&AtEndOfSentencePredicate),
        base::BindRepeating(&GetSentenceStartOffsetsFunc));
  }

  AXPositionInstance CreateNextSentenceEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfSentencePredicate),
        base::BindRepeating(&AtEndOfSentencePredicate),
        base::BindRepeating(&GetSentenceEndOffsetsFunc));
  }

  AXPositionInstance CreatePreviousSentenceEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfSentencePredicate),
        base::BindRepeating(&AtEndOfSentencePredicate),
        base::BindRepeating(&GetSentenceEndOffsetsFunc));
  }

  AXPositionInstance CreateNextParagraphStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreateNextParagraphStartPositionSkippingEmptyParagraphs(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(
            &AtStartOfParagraphExcludingEmptyParagraphsPredicate),
        base::BindRepeating(
            &AtStartOfParagraphExcludingEmptyParagraphsPredicate));
  }

  AXPositionInstance CreatePreviousParagraphStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance
  CreatePreviousParagraphStartPositionSkippingEmptyParagraphs(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(
            &AtStartOfParagraphExcludingEmptyParagraphsPredicate),
        base::BindRepeating(
            &AtStartOfParagraphExcludingEmptyParagraphsPredicate));
  }

  AXPositionInstance CreateNextParagraphEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreatePreviousParagraphEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfParagraphPredicate),
        base::BindRepeating(&AtEndOfParagraphPredicate));
  }

  AXPositionInstance CreateNextPageStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreatePreviousPageStartPosition(
      AXMovementOptions options) const {
    return CreateBoundaryStartPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreateNextPageEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kForward,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreatePreviousPageEndPosition(
      AXMovementOptions options) const {
    return CreateBoundaryEndPosition(
        options, ax::mojom::MoveDirection::kBackward,
        base::BindRepeating(&AtStartOfPagePredicate),
        base::BindRepeating(&AtEndOfPagePredicate));
  }

  AXPositionInstance CreateBoundaryStartPosition(
      AXMovementOptions options,
      ax::mojom::MoveDirection move_direction,
      BoundaryConditionPredicate at_start_condition,
      BoundaryConditionPredicate at_end_condition,
      BoundaryTextOffsetsFunc get_start_offsets =
          BoundaryTextOffsetsFunc()) const {
    AXPositionInstance text_position;
    if (!AtEndOfAnchor()) {
      // We could get a leaf text position at the end of its anchor, where
      // boundary start offsets would surely not be present. In such cases, we
      // need to normalize to the start of the next leaf anchor. We avoid making
      // this change when we are at the end of our anchor because this could
      // effectively shift the position forward.
      text_position = AsLeafTextPositionBeforeCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    if (text_position->IsNullPosition()) {
      return text_position;
    }

    // If true, we should not move the position any further.
    bool forward_upstream =
        options.upstream_bounded &&
        move_direction == ax::mojom::MoveDirection::kForward &&
        affinity() == ax::mojom::TextAffinity::kUpstream;

    // If true, we should skip the initial position and move at least once.
    bool backward_upstream =
        options.upstream_bounded &&
        move_direction == ax::mojom::MoveDirection::kBackward &&
        affinity() == ax::mojom::TextAffinity::kUpstream;

    if (backward_upstream ||
        (options.boundary_detection ==
             AXBoundaryDetection::kDontCheckInitialPosition &&
         !forward_upstream)) {
      text_position =
          text_position->CreateAdjacentLeafTextPosition(move_direction);
      if (text_position->IsNullPosition()) {
        // There is no adjacent position to move to; in such case, CrossBoundary
        // behavior shall return a null position, while any other behavior shall
        // fallback to return the initial position.
        if (options.boundary_behavior == AXBoundaryBehavior::kCrossBoundary) {
          return text_position;
        }

        return Clone();
      }
    }

    if (!forward_upstream && !at_start_condition.Run(text_position)) {
      text_position = text_position->CreatePositionAtNextOffsetBoundary(
          move_direction, get_start_offsets);

      while (!at_start_condition.Run(text_position)) {
        AXPositionInstance next_position;
        switch (move_direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            return CreateNullPosition();
          case ax::mojom::MoveDirection::kBackward:
            if (text_position->AtStartOfAnchor()) {
              next_position = text_position->CreatePreviousLeafTextPosition(
                  base::BindRepeating(&AbortMoveAtRootBoundary));
            } else {
              text_position = text_position->CreatePositionAtStartOfAnchor();
              DCHECK(!text_position->IsNullPosition());
              continue;
            }
            break;
          case ax::mojom::MoveDirection::kForward:
            next_position = text_position->CreateNextLeafTextPosition(
                base::BindRepeating(&AbortMoveAtRootBoundary));
            break;
        }

        if (next_position->IsNullPosition()) {
          if (options.boundary_behavior ==
              AXBoundaryBehavior::kStopAtAnchorBoundary) {
            switch (move_direction) {
              case ax::mojom::MoveDirection::kNone:
                NOTREACHED_IN_MIGRATION();
                return CreateNullPosition();
              case ax::mojom::MoveDirection::kBackward:
                return CreatePositionAtStartOfAnchor()->AsUnignoredPosition(
                    AXPositionAdjustmentBehavior::kMoveBackward);
              case ax::mojom::MoveDirection::kForward:
                return CreatePositionAtEndOfAnchor()->AsUnignoredPosition(
                    AXPositionAdjustmentBehavior::kMoveForward);
            }
          }

          if (options.boundary_behavior ==
              AXBoundaryBehavior::kStopAtLastAnchorBoundary) {
            // We can't simply return the following position; break and after
            // this loop we'll try to do some adjustments to text_position.
            switch (move_direction) {
              case ax::mojom::MoveDirection::kNone:
                NOTREACHED_IN_MIGRATION();
                return CreateNullPosition();
              case ax::mojom::MoveDirection::kBackward:
                text_position = text_position->CreatePositionAtStartOfAnchor();
                break;
              case ax::mojom::MoveDirection::kForward:
                text_position = text_position->CreatePositionAtEndOfAnchor();
                break;
            }

            break;
          }

          return next_position->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveForward);
        }

        // Continue searching for the next boundary start in the specified
        // direction until the next logical text position is reached.
        text_position = next_position->CreatePositionAtFirstOffsetBoundary(
            move_direction, get_start_offsets);
      }
    }

    // If the boundary is in the same subtree, return a position rooted at this
    // position's anchor. This is necessary because we don't want to return a
    // position that might be in the shadow DOM when this position is not.
    const AXNode* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position =
          text_position->CreateAncestorPosition(common_anchor, move_direction);
    } else if (options.boundary_behavior ==
               AXBoundaryBehavior::kStopAtAnchorBoundary) {
      switch (move_direction) {
        case ax::mojom::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return CreateNullPosition();
        case ax::mojom::MoveDirection::kBackward:
          text_position = CreatePositionAtStartOfAnchor()->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveBackward);
          break;
        case ax::mojom::MoveDirection::kForward:
          text_position = CreatePositionAtEndOfAnchor()->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveForward);
      }

      // Preserve affinity for forward upstream positions.
      if (forward_upstream) {
        text_position->affinity_ = ax::mojom::TextAffinity::kUpstream;
      }

      return text_position;
    }

    if (IsTreePosition()) {
      text_position = text_position->AsTreePosition();
    }

    AXPositionInstance unignored_position = text_position->AsUnignoredPosition(
        AXPositionAdjustmentBehavior::kMoveForward);

    // If there are no unignored positions then `text_position` is anchored in
    // ignored content at the end of the whole content. For
    // `kStopAtLastAnchorBoundary`, try to adjust in the opposite direction to
    // return a position within the whole content just before crossing into the
    // ignored content. This will be the last unignored anchor boundary.
    if (unignored_position->IsNullPosition() &&
        options.boundary_behavior ==
            AXBoundaryBehavior::kStopAtLastAnchorBoundary) {
      unignored_position = text_position->AsUnignoredPosition(
          AXPositionAdjustmentBehavior::kMoveBackward);
    }

    unignored_position->affinity_ = forward_upstream
                                        ? ax::mojom::TextAffinity::kUpstream
                                        : ax::mojom::TextAffinity::kDownstream;

    return unignored_position;
  }

  AXPositionInstance CreateBoundaryEndPosition(
      AXMovementOptions options,
      ax::mojom::MoveDirection move_direction,
      BoundaryConditionPredicate at_start_condition,
      BoundaryConditionPredicate at_end_condition,
      BoundaryTextOffsetsFunc get_end_offsets =
          BoundaryTextOffsetsFunc()) const {
    AXPositionInstance text_position;
    if (!AtStartOfAnchor()) {
      // We could get a leaf text position at the start of its anchor, where
      // boundary end offsets would surely not be present. In such cases, we
      // need to normalize to the end of the previous leaf anchor. We avoid
      // making this change when we are at the start of our anchor because this
      // could effectively shift the position backward.
      text_position = AsLeafTextPositionAfterCharacter();
    } else {
      text_position = AsLeafTextPosition();
    }

    if (text_position->IsNullPosition())
      return text_position;

    if (options.boundary_detection ==
        AXBoundaryDetection::kDontCheckInitialPosition) {
      text_position =
          text_position->CreateAdjacentLeafTextPosition(move_direction);
      if (text_position->IsNullPosition()) {
        // There is no adjacent position to move to; in such case, CrossBoundary
        // behavior shall return a null position, while any other behavior shall
        // fallback to return the initial position.
        if (options.boundary_behavior == AXBoundaryBehavior::kCrossBoundary)
          return text_position;
        return Clone();
      }
    }

    if (!at_end_condition.Run(text_position)) {
      text_position = text_position->CreatePositionAtNextOffsetBoundary(
          move_direction, get_end_offsets);

      while (!at_end_condition.Run(text_position)) {
        AXPositionInstance next_position;
        switch (move_direction) {
          case ax::mojom::MoveDirection::kNone:
            NOTREACHED_IN_MIGRATION();
            return CreateNullPosition();
          case ax::mojom::MoveDirection::kBackward:
            next_position =
                text_position
                    ->CreatePreviousLeafTextPosition(
                        base::BindRepeating(&AbortMoveAtRootBoundary))
                    ->CreatePositionAtEndOfAnchor();
            break;
          case ax::mojom::MoveDirection::kForward:
            if (text_position->AtEndOfAnchor()) {
              next_position = text_position->CreateNextLeafTextPosition(
                  base::BindRepeating(&AbortMoveAtRootBoundary));
            } else {
              text_position = text_position->CreatePositionAtEndOfAnchor();
              DCHECK(!text_position->IsNullPosition());
              continue;
            }
            break;
        }

        if (next_position->IsNullPosition()) {
          if (options.boundary_behavior ==
              AXBoundaryBehavior::kStopAtAnchorBoundary) {
            switch (move_direction) {
              case ax::mojom::MoveDirection::kNone:
                NOTREACHED_IN_MIGRATION();
                return CreateNullPosition();
              case ax::mojom::MoveDirection::kBackward:
                return CreatePositionAtStartOfAnchor()->AsUnignoredPosition(
                    AXPositionAdjustmentBehavior::kMoveBackward);
              case ax::mojom::MoveDirection::kForward:
                return CreatePositionAtEndOfAnchor()->AsUnignoredPosition(
                    AXPositionAdjustmentBehavior::kMoveForward);
            }
          }

          if (options.boundary_behavior ==
              AXBoundaryBehavior::kStopAtLastAnchorBoundary) {
            // We can't simply return the following position; break and after
            // this loop we'll try to do some adjustments to text_position.
            switch (move_direction) {
              case ax::mojom::MoveDirection::kNone:
                NOTREACHED_IN_MIGRATION();
                return CreateNullPosition();
              case ax::mojom::MoveDirection::kBackward:
                text_position = text_position->CreatePositionAtStartOfAnchor();
                break;
              case ax::mojom::MoveDirection::kForward:
                text_position = text_position->CreatePositionAtEndOfAnchor();
                break;
            }

            break;
          }

          return next_position->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveBackward);
        }

        // Continue searching for the next boundary end in the specified
        // direction until the next logical text position is reached.
        text_position = next_position->CreatePositionAtFirstOffsetBoundary(
            move_direction, get_end_offsets);
      }
    }

    // If the boundary is in the same subtree, return a position rooted at this
    // position's anchor. This is necessary because we don't want to return a
    // position that might be in the shadow DOM when this position is not.
    const AXNode* common_anchor = text_position->LowestCommonAnchor(*this);
    if (GetAnchor() == common_anchor) {
      text_position =
          text_position->CreateAncestorPosition(common_anchor, move_direction);
    } else if (options.boundary_behavior ==
               AXBoundaryBehavior::kStopAtAnchorBoundary) {
      switch (move_direction) {
        case ax::mojom::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return CreateNullPosition();
        case ax::mojom::MoveDirection::kBackward:
          return CreatePositionAtStartOfAnchor()->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveBackward);
        case ax::mojom::MoveDirection::kForward:
          return CreatePositionAtEndOfAnchor()->AsUnignoredPosition(
              AXPositionAdjustmentBehavior::kMoveForward);
      }
    }

    // If there is no ambiguity as to whether the position is at the end of
    // the current boundary or the start of the next boundary, an upstream
    // affinity should be reset to downstream in order to get consistent output
    // from this method, regardless of input affinity.
    //
    // Note that there could be no ambiguity if the boundary is either at the
    // start or the end of the current anchor, so we should always reset to
    // downstream affinity in those cases.
    if (text_position->affinity_ == ax::mojom::TextAffinity::kUpstream) {
      AXPositionInstance downstream_position =
          text_position->CloneWithDownstreamAffinity();
      if (downstream_position->AtStartOfAnchor() ||
          downstream_position->AtEndOfAnchor() ||
          !downstream_position->AtStartOfLine()) {
        text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
      }
    }

    if (IsTreePosition())
      text_position = text_position->AsTreePosition();
    AXPositionInstance unignored_position = text_position->AsUnignoredPosition(
        AXPositionAdjustmentBehavior::kMoveBackward);
    // If there are no unignored positions then `text_position` is anchored in
    // ignored content at the start or end of the whole content. For
    // `kStopAtLastAnchorBoundary`, try to adjust in the opposite direction to
    // return a position within the whole content just before crossing into the
    // ignored content. This will be the last unignored anchor boundary.
    if (unignored_position->IsNullPosition() &&
        options.boundary_behavior ==
            AXBoundaryBehavior::kStopAtLastAnchorBoundary) {
      unignored_position = text_position->AsUnignoredPosition(
          AXPositionAdjustmentBehavior::kMoveForward);
    }
    return unignored_position;
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition() const {
    return CreateNextAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition() const {
    return CreatePreviousAnchorPosition(
        base::BindRepeating(&DefaultAbortMovePredicate));
  }

  // Returns an optional integer indicating the logical order of this position
  // compared to another position or returns an empty optional if the positions
  // are not comparable. Any text position at the same character location is
  // logically equivalent although they may be on different anchors or have
  // different text offsets. Positions are not comparable when one position is
  // null and the other is not or if the positions do not have any common
  // ancestor.
  //
  //    0: if this position is logically equivalent to the other position
  //   <0: if this position is logically less than the other position
  //   >0: if this position is logically greater than the other position
  std::optional<int> CompareTo(const AXPosition& other) const {
    if (IsNullPosition() || other.IsNullPosition()) {
      if (IsNullPosition() && other.IsNullPosition())
        return 0;
      return std::nullopt;
    }
    // Valid positions are required for comparison. Use `AsValidPosition`
    // or `SnapToMaxTextOffsetIfBeyond` before calling `CompareTo` or making
    // comparisons.
    DCHECK(IsValid());
    DCHECK(other.IsValid());

    if (GetAnchor() == other.GetAnchor())
      return SlowCompareTo(other);  // No optimization is necessary.

    // Ancestor positions are expensive to compute. If possible, we will avoid
    // doing so by computing the ancestor chain of the two positions' anchors.
    // If the lowest common ancestor is neither position's anchor, we can use
    // the order of the first uncommon ancestors as a proxy for the order of the
    // positions. Obviously, this heuristic cannot be used if one position is
    // the ancestor of the other.
    //
    // In order to do that, we need to normalize text positions at the end of an
    // anchor to equivalent positions at the start of the next anchor. Ignored
    // positions are a special case in that they need to be shifted to the
    // nearest unignored position in order to be normalized. That shifting can
    // change the comparison result, so if we have an ignored position, we must
    // use a different, slower method which does away with many of our
    // optimizations.
    if (IsIgnored() || other.IsIgnored())
      return SlowCompareTo(other);

    // Normalize any text positions at the end of an anchor to equivalent
    // positions at the start of the next anchor. This will potentially make the
    // two positions not be ancestors of one another, if they originally were.
    AXPositionInstance normalized_this_position = Clone();
    if (normalized_this_position->IsTextPosition()) {
      normalized_this_position =
          normalized_this_position->AsLeafTextPositionBeforeCharacter();
    }

    AXPositionInstance normalized_other_position = other.Clone();
    if (normalized_other_position->IsTextPosition()) {
      normalized_other_position =
          normalized_other_position->AsLeafTextPositionBeforeCharacter();
    }

    if (normalized_this_position->IsNullPosition()) {
      if (normalized_other_position->IsNullPosition()) {
        // Both positions normalized to a position past the end of the whole
        // content. There is no way that they could be ancestors of one another,
        // so using the slow path is not required.
        DCHECK_EQ(SlowCompareTo(other).value(), 0);
        return 0;
      }
      // |this| normalized to a position past the end of the whole content.
      // Since we don't know if one position is the ancestor of the other, we
      // need to use the slow path.
      return SlowCompareTo(other);
    }
    if (normalized_other_position->IsNullPosition()) {
      // |other| normalized to a position past the end of the whole content.
      // Since we don't know if one position is the ancestor of the other, we
      // need to use the slow path.
      return SlowCompareTo(other);
    }

    // Compute the ancestor stacks of both positions and walk them ourselves
    // rather than calling `LowestCommonAnchor`. That way, we can discover the
    // first uncommon ancestors which we need to use in order to compare the two
    // positions.
    const AXNode* common_anchor = nullptr;
    base::stack<AXNode*> our_ancestors =
        normalized_this_position->GetAncestorAnchors();
    base::stack<AXNode*> other_ancestors =
        normalized_other_position->GetAncestorAnchors();
    while (!our_ancestors.empty() && !other_ancestors.empty() &&
           our_ancestors.top() == other_ancestors.top()) {
      common_anchor = our_ancestors.top();
      our_ancestors.pop();
      other_ancestors.pop();
    }

    if (!common_anchor)
      return std::nullopt;

    // If each position has an uncommon ancestor node, we can compare those
    // instead of needing to compute ancestor positions. Otherwise we need to
    // use "SlowCompareTo". Also, if the two positions became equivalent after
    // being normalized above, we can't compare using this optimized method. We
    // need to use "SlowCompareTo", because affinity information would have been
    // lost during the normalization process. See comments in "SlowCompareTo"
    // for an explanation of how affinity could affect the comparison. If one
    // position is the ancestor of the other, we need to use "SlowCompareTo",
    // especially if either or both positions are text positions, because the
    // conversion to tree positions below would lose information that could
    // affect the comparison. In the case where the positions are ancestors of
    // one another, but they are both tree positions, using the "SlowCompareTo"
    // method will not affect performance, so we still opt for that. Note that
    // determining whether two positions are ancestors of one another could
    // easily be accomplished by checking if there are any ancestors left after
    // removing the common ancestor anchor from either position's ancestor
    // stack.
    if (our_ancestors.empty() || other_ancestors.empty())
      return SlowCompareTo(other);

    AXPositionInstance this_uncommon_tree_position =
        CreateTreePositionAtStartOfAnchor(*our_ancestors.top());
    int this_uncommon_ancestor_index =
        this_uncommon_tree_position->AnchorIndexInParent();
    AXPositionInstance other_uncommon_tree_position =
        CreateTreePositionAtStartOfAnchor(*other_ancestors.top());
    int other_uncommon_ancestor_index =
        other_uncommon_tree_position->AnchorIndexInParent();
    DCHECK_NE(this_uncommon_ancestor_index, other_uncommon_ancestor_index)
        << "Deepest uncommon ancestors should truly be uncommon, i.e. not "
           "the same.";
    int result = this_uncommon_ancestor_index - other_uncommon_ancestor_index;

    // On platforms that support embedded objects, if a text position is within
    // an embedded object and if it is not at the start of that object, the
    // resulting ancestor position should be adjusted to point after the
    // embedded object. Otherwise, assistive software will not be able to get
    // out of the embedded object if its text is not editable when navigating by
    // character or by word. The "SlowCompareTo" method can handle such corner
    // cases. For some reproduction steps see https://crbug.com/1057831.
    //
    // For example, look at the following accessibility tree and the two example
    // text positions together with their equivalent ancestor positions.
    // ++1 kRootWebArea
    // ++++2 kTextField "Before<embedded_object>after"
    // ++++++3 kStaticText "Before"
    // ++++++++4 kInlineTextBox "Before"
    // ++++++5 kImage "Test image"
    // ++++++6 kStaticText "after"
    // ++++++++7 kInlineTextBox "after"
    //
    // Note that the alt text of an image cannot be navigated with cursor
    // left/right, even when the rest of the contents are in a contenteditable.
    //
    // 1. Ancestor position should not be adjusted:
    // TextPosition anchor_id=kImage text_offset=0 affinity=downstream
    // annotated_text=<T>est image
    //
    // AncestorTextPosition anchor_id=kTextField text_offset=6
    // affinity=downstream annotated_text=Before<embedded_object>after
    //
    // 2. Ancestor position should be adjusted:
    // TextPosition anchor_id=kImage text_offset=1 affinity=downstream
    // annotated_text=T<e>st image
    //
    // AncestorTextPosition anchor_id=kTextField text_offset=7
    // affinity=downstream annotated_text=Beforeembedded_object<a>fter
    //
    // Note that since the adjustment to the distance between the ancestor
    // positions could at most be by one, we skip doing this check if the
    // ancestor positions have a distance of more than one since it can never
    // change the outcome of the comparison. We also don't need to perform an
    // adjustment if one of the positions is not right after the "object
    // replacement character" representing the object inside which the other
    // position is located, hence the `AtStartOfAnchor()` and
    // `IsEmbeddedObjectInParent()` checks.
    if (abs(result) == 1 &&
        ((IsTextPosition() && !AtStartOfAnchor() &&
          this_uncommon_tree_position->IsEmbeddedObjectInParent()) ||
         (other.IsTextPosition() && !other.AtStartOfAnchor() &&
          other_uncommon_tree_position->IsEmbeddedObjectInParent()))) {
      return SlowCompareTo(other);
    }

#if DCHECK_IS_ON()
    // Validate the optimization against the non-optimized version of the
    // method.
    int slow_result = SlowCompareTo(other).value();
    DCHECK((result == 0 && slow_result == 0) ||
           (result < 0 && slow_result < 0) || (result > 0 && slow_result > 0))
        << result << " vs. " << slow_result;
#endif  // DCHECK_IS_ON()

    return result;
  }

  // A less optimized, but much slower version of "CompareTo". Should only be
  // used when optimizations cannot be applied, e.g. when comparing ignored
  // positions. See "CompareTo" for an explanation of the return values.
  std::optional<int> SlowCompareTo(const AXPosition& other) const {
    if (IsNullPosition() && other.IsNullPosition())
      return 0;
    if (IsNullPosition() || other.IsNullPosition())
      return std::nullopt;

    // If both positions share an anchor and either one is a text position, or
    // both are tree positions, we can do a straight comparison of text offsets
    // or child indices.
    if (GetAnchor() == other.GetAnchor()) {
      std::optional<int> optional_result;
      ax::mojom::TextAffinity this_affinity;
      ax::mojom::TextAffinity other_affinity;

      if (IsTextPosition()) {
        AXPositionInstance other_text_position = other.AsTextPosition();
        optional_result = text_offset_ - other_text_position->text_offset_;
        this_affinity = affinity();
        other_affinity = other_text_position->affinity();
      } else if (other.IsTextPosition()) {
        AXPositionInstance this_text_position = AsTextPosition();
        optional_result = this_text_position->text_offset_ - other.text_offset_;
        this_affinity = this_text_position->affinity();
        other_affinity = other.affinity();
      }

      if (optional_result) {
        // Only when the two positions are otherwise equivalent will affinity
        // play a role.
        if (*optional_result != 0)
          return optional_result;

        if (this_affinity == ax::mojom::TextAffinity::kUpstream &&
            other_affinity == ax::mojom::TextAffinity::kDownstream) {
          return -1;
        }
        if (this_affinity == ax::mojom::TextAffinity::kDownstream &&
            other_affinity == ax::mojom::TextAffinity::kUpstream) {
          return 1;
        }

        return optional_result;
      }

      return child_index_ - other.child_index_;
    }

    // It is potentially costly to compute the parent position of a text
    // position, whilst computing the parent position of a tree position is
    // really inexpensive. In order to find the lowest common ancestor position,
    // especially if that ancestor is all the way up to the root of the tree,
    // computing the parent position will need to be done repeatedly. We avoid
    // the performance hit by converting both positions to tree positions and
    // only falling back to computing ancestor text positions if at least one
    // position is a text position and they don't have the same anchor.
    //
    // Essentially, the question we need to answer is: "When are two non
    // equivalent positions going to erroneously have the same lowest common
    // ancestor position when converted to tree positions as the ones they had
    // before the conversion?" In other words, when will
    // "this->AsTreePosition()->LowestCommonAncestorPosition(*other.AsTreePosition())
    // ==
    // other.AsTreePosition()->LowestCommonAncestorPosition(*this->AsTreePosition())"?
    // The answer is either when they have the same anchor and at least one is a
    // text position, (a case that was dealt with in the previous block), or
    // when at least one is a text position and one is an ancestor position of
    // the other. In all other cases, no information will be lost when
    // converting to tree positions.

    const AXNode* common_anchor = this->LowestCommonAnchor(other);
    if (!common_anchor)
      return std::nullopt;

    // If either of the two positions is a text position, and if one position is
    // an ancestor of the other, we need to compare using text positions,
    // because converting to tree positions will potentially lose information if
    // the text offset is anything other than 0 or `MaxTextOffset()`.
    if (IsTextPosition() || other.IsTextPosition()) {
      std::optional<int> optional_result;
      ax::mojom::TextAffinity this_affinity;
      ax::mojom::TextAffinity other_affinity;

      // The following two "if" blocks deal with comparisons between two
      // positions (one of which is a text position) that are ancestors of one
      // another. The third "if" block deals with comparisons between two text
      // positions that may or may not be ancestors of one another. Obviously,
      // in the case of two text positions, affinity could always play a role
      // (see comment in the relevant "if" block for an example). For the first
      // two cases, affinity still needs to be taken into consideration because
      // an "object replacement character" could be used to represent child
      // nodes in the text of their parents. Here is an example of how affinity
      // can influence a text/tree position comparison.
      //
      // 1 kRootWebArea
      // ++2 kGenericContainer
      // "<embedded_object_character><embedded_object_character>"
      // ++3 kButton "Line 1"
      // ++++++4 kStaticText "Line 1"
      // ++++++++5 kInlineTextBox "Line 1"
      // ++++6 kImage "<embedded_object_character>" kIsLineBreakingObject
      //
      // TextPosition anchor_id=5 text_offset=2 affinity=downstream
      // annotated_text=Li<n>e 1
      //
      // TreePosition anchor_id=6 child_index=BEFORE_TEXT
      //
      // The `LowestCommonAncestor` for both will differ in its affinity:
      // TextPosition anchor_id=2 text_offset=1 affinity=...
      // annotated_text=embedded_object_character<embedded_object_character>
      //
      // The text position would create a kUpstream position, while the tree
      // position would create a kDownstream position.

      if (GetAnchor() == common_anchor) {
        DCHECK_EQ(AsTextPosition()->GetAnchor(), common_anchor)
            << "AsTextPosition() should never modify the position's anchor.";
        // This text position's anchor is the common ancestor of the other text
        // position's anchor. We don't need to compute the ancestor position of
        // this position at the common anchor, since we already have it.
        //
        // Note that we convert the other position to an ancestor text position
        // using a forward direction, so that if there are any "object
        // replacement characters", two positions one inside the character and
        // one after it would compare as equivalent. Otherwise, screen readers
        // might get stuck inside embedded objects while navigating by character
        // or word. For some reproduction steps see https://crbug.com/1057831.
        // Per the IAccessible2 Spec, any selection that partially selects text
        // inside an embedded object, should select the entire "object
        // replacement character" in the parent object where the character
        // appears.

        AXPositionInstance other_text_position =
            other.AsTextPosition()->CreateAncestorPosition(
                common_anchor, ax::mojom::MoveDirection::kForward);
        DCHECK_EQ(other_text_position->GetAnchor(), common_anchor);
        other_affinity = other_text_position->affinity();
        AXPositionInstance this_text_position = AsTextPosition();
        this_affinity = this_text_position->affinity();
        optional_result = this_text_position->text_offset() -
                          other_text_position->text_offset();
      } else if (other.GetAnchor() == common_anchor) {
        DCHECK_EQ(other.AsTextPosition()->GetAnchor(), common_anchor)
            << "AsTextPosition() should never modify the position's anchor.";
        // The other text position's anchor is the common ancestor of this text
        // position's anchor. We don't need to compute the ancestor position of
        // the other position at the common anchor, since we already have it.
        //
        // Note that we convert this position to an ancestor text position using
        // a forward direction, so that if there are any "object replacement
        // characters", two positions one inside the character and one after it
        // would compare as equivalent. Otherwise, screen readers might get
        // stuck inside embedded objects while navigating by character or word.
        // For some reproduction steps see https://crbug.com/1057831.
        // Per the IAccessible2 Spec, any selection that partially selects text
        // inside an embedded object, should select the entire "object
        // replacement character" in the parent object where the character
        // appears.

        AXPositionInstance this_text_position =
            AsTextPosition()->CreateAncestorPosition(
                common_anchor, ax::mojom::MoveDirection::kForward);
        DCHECK_EQ(this_text_position->GetAnchor(), common_anchor);
        this_affinity = this_text_position->affinity();
        AXPositionInstance other_text_position = other.AsTextPosition();
        other_affinity = other_text_position->affinity();
        optional_result = this_text_position->text_offset() -
                          other_text_position->text_offset();
      } else if (IsTextPosition() && other.IsTextPosition()) {
        // We should compute and compare using the common ancestor text
        // position. Computing an ancestor text position will automatically take
        // affinity into consideration. It will also normalize text positions at
        // the end of their anchors to equivalent positions at the start of the
        // next anchor. Additionally, it would normalize positions within
        // "object replacement characters" to before the character, because the
        // two positions are not ancestors of one another and thus the special
        // case (see previous block) defined in the IAccessible2 Spec doesn't
        // apply. This process would maintain the characteristics of text
        // position comparisons, since a particular offset in the tree's text
        // representation could refer to multiple equivalent positions which are
        // anchored to different nodes in the tree, i.e. nodes which are
        // adjacent, or nodes that are at different levels of the tree.
        //
        // Here is an example of how affinity can influence a text position
        // comparison when at a line boundary:
        //
        // 1 kRootWebArea
        // ++2 kTextField "Line 1Line 2"
        // ++++3 kStaticText "Line 1"
        // ++++++4 kInlineTextBox "Line 1"
        // ++++5 kGenericContainer kIsLineBreakingObject
        // ++++++6 kStaticText "Line 2"
        // ++++++++7 kInlineTextBox "Line 2"
        //
        // TextPosition anchor_id=4 text_offset=6 affinity=downstream
        // annotated_text=Line 1<>
        //
        // TextPosition anchor_id=7 text_offset=0 affinity=downstream
        // annotated_text=<L>ine 2
        //
        // The `LowestCommonAncestor` for both will differ only in its affinity:
        // TextPosition anchor_id=2 text_offset=6 affinity=...
        // annotated_text=Line 1<L>ine 2
        //
        // anchor_id=4 would create a kUpstream position, while anchor_id=7
        // would create a kDownstream position.

        AXPositionInstance this_text_position_ancestor =
            LowestCommonAncestorPosition(other,
                                         ax::mojom::MoveDirection::kBackward);
        AXPositionInstance other_text_position_ancestor =
            other.LowestCommonAncestorPosition(
                *this, ax::mojom::MoveDirection::kBackward);
        DCHECK(this_text_position_ancestor->IsTextPosition());
        DCHECK(other_text_position_ancestor->IsTextPosition());

        this_affinity = this_text_position_ancestor->affinity();
        other_affinity = other_text_position_ancestor->affinity();
        optional_result = this_text_position_ancestor->text_offset() -
                          other_text_position_ancestor->text_offset();
      }

      if (optional_result) {
        // Only when the two positions are otherwise equivalent will affinity
        // play a role.
        if (*optional_result != 0)
          return optional_result;

        if (this_affinity == ax::mojom::TextAffinity::kUpstream &&
            other_affinity == ax::mojom::TextAffinity::kDownstream) {
          return -1;
        }
        if (this_affinity == ax::mojom::TextAffinity::kDownstream &&
            other_affinity == ax::mojom::TextAffinity::kUpstream) {
          return 1;
        }

        return optional_result;
      }
    }

    // Both positions are tree positions. We should normalize all tree positions
    // to the beginning of their anchors, unless one of the positions is the
    // ancestor of the other. In the latter case, such a normalization would
    // potentially lose information if performed on any of the two positions.
    //
    // ++kRootWebArea "<embedded_object><embedded_object>"
    // ++++kParagraph "Paragraph1"
    // ++++kParagraph "paragraph2"
    // A tree position at the end of the root web area and a tree position at
    // the end of the second paragraph should compare as equal. Normalizing any
    // of the two positions to the start of their respective anchors would make
    // the two positions unequal.
    //
    // Unlike text positions, two tree positions on two adjacent anchors, (the
    // first position at the end of its anchor, (i.e. an "after children"
    // position), and the other at its beginning), should not compare as equal.
    // This is because each position in the tree is unique, unlike an offset in
    // the tree's text representation which can refer to more than one tree
    // position. Meanwhile, affinity does not play any role in this case, since
    // except for "after children" positions, tree positions are collapsed to
    // the beginning of their parent node when computing their parent position.

    AXPositionInstance this_normalized_tree_position = AsTreePosition();
    AXPositionInstance other_normalized_tree_position = other.AsTreePosition();
    if (GetAnchor() != common_anchor &&
        other_normalized_tree_position->GetAnchor() != common_anchor) {
      // None of the positions is the ancestor of the other, so normalization
      // could go ahead.
      this_normalized_tree_position =
          this_normalized_tree_position->CreatePositionAtStartOfAnchor();
      other_normalized_tree_position =
          other_normalized_tree_position->CreatePositionAtStartOfAnchor();
    }

    AXPositionInstance this_tree_position_ancestor =
        this_normalized_tree_position->CreateAncestorPosition(
            common_anchor, ax::mojom::MoveDirection::kBackward);
    AXPositionInstance other_tree_position_ancestor =
        other_normalized_tree_position->CreateAncestorPosition(
            common_anchor, ax::mojom::MoveDirection::kBackward);
    DCHECK(this_tree_position_ancestor->IsTreePosition());
    DCHECK(other_tree_position_ancestor->IsTreePosition());
    return this_tree_position_ancestor->child_index_ -
           other_tree_position_ancestor->child_index_;
  }

  // A valid position can become invalid if the underlying tree structure
  // changes. This is expected behavior, but it is sometimes necessary to
  // maintain valid positions. This method modifies an invalid position that is
  // beyond MaxTextOffset to snap to MaxTextOffset.
  void SnapToMaxTextOffsetIfBeyond() {
    int max_text_offset = MaxTextOffset();
    if (text_offset_ > max_text_offset)
      text_offset_ = max_text_offset;
  }

  bool IsInEmptyObject() const {
    if (IsNullPosition())
      return false;

    return IsEmptyObject(*GetAnchor());
  }

  bool IsInUnignoredEmptyObject() const {
    return GetAnchor() && !GetAnchor()->IsIgnored() && IsInEmptyObject();
  }

  // Returns whether the position is anchored in an unignored and empty object,
  // has an author specified name that is not empty, and it is not anchored in
  // an image. This is because in UIA we want to expose embedded object
  // characters for image elements, even if they have an author specified name.
  // Only used for UIA.
  bool EmptyObjectShouldProvideNameFromAttribute() const {
    DCHECK(IsInUnignoredEmptyObject());
    return GetAnchor()->GetNameFrom() == ax::mojom::NameFrom::kAttribute &&
           !IsImage(GetAnchor()->GetRole()) &&
           !GetAnchor()->GetNameUTF16().empty();
  }

  AXNode* GetEmptyObjectAncestorNode() const {
    if (!GetAnchor())
      return nullptr;

    if (!GetAnchor()->IsIgnored()) {
      // The only cases where a descendant of an empty object can be unignored
      // is when on Windows we are inside of a collapsed popup button which is
      // the parent of a menu list popup, or on all platforms inside a generic
      // container that is the child of an empty text field.
      if (AXNode* popup_button =
              GetAnchor()->GetCollapsedMenuListSelectAncestor()) {
        return popup_button;
      }

      if (GetAnchorRole() == ax::mojom::Role::kGenericContainer &&
          !AnchorUnignoredChildCount()) {
        return GetAnchor()->GetTextFieldAncestor();
      }

      return nullptr;
    }

    // The first unignored ancestor is necessarily the empty object if this node
    // is the descendant of an empty object.
    AXNode* ancestor_node = GetLowestUnignoredAncestor();
    if (!ancestor_node)
      return nullptr;

    AXPositionInstance position =
        CreateTextPosition(*ancestor_node, 0 /* text_offset */,
                           ax::mojom::TextAffinity::kDownstream);
    if (position->IsInUnignoredEmptyObject())
      return ancestor_node;

    return nullptr;
  }

  void swap(AXPosition& other) {
    std::swap(kind_, other.kind_);
    std::swap(tree_id_, other.tree_id_);
    std::swap(anchor_id_, other.anchor_id_);
    std::swap(child_index_, other.child_index_);
    std::swap(text_offset_, other.text_offset_);
    std::swap(affinity_, other.affinity_);
    // We explicitly don't swap any cached members.
    name_ = std::u16string();
    other.name_ = std::u16string();
  }

  // Returns the text (in UTF16 format) that is present inside the anchor node,
  // including any text found in descendant text nodes, based on the platform's
  // text representation. Some platforms use an embedded object replacement
  // character that replaces the text coming from most child nodes and empty
  // objects.
  std::u16string GetText(
      const AXEmbeddedObjectBehavior embedded_object_behavior =
          g_ax_embedded_object_behavior) const {
    if (IsNullPosition()) {
      return std::u16string();
    }

    static const base::NoDestructor<std::u16string> embedded_character_str(
        AXNode::kEmbeddedObjectCharacterUTF16);
    switch (embedded_object_behavior) {
      case AXEmbeddedObjectBehavior::kSuppressCharacter:
        return GetAnchor()->GetTextContentUTF16();
      case AXEmbeddedObjectBehavior::kExposeCharacterForHypertext:
        // Special case, if a position's anchor node has only ignored
        // descendants, i.e., it appears to be empty to assistive software, on
        // some platforms we need to still treat it as a character and a word
        // boundary. We achieve this by adding an embedded object character in
        // the text representation used by this class, but we don't expose that
        // character to assistive software that tries to retrieve the node's
        // text content.
        if (IsInUnignoredEmptyObject()) {
          return *embedded_character_str;
        }
        return GetAnchor()->GetHypertext();
      case AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent:
        // For UIA, we still have the notion of embedded object characters for
        // text navigation purposes. I.e. when AT's need to navigate around
        // nodes and elements which are empty and should then be exposed as
        // embedded object characters.
        //
        // According to the spec, we should favor author supplied names over
        // names from content. However, trying to fulfill this in every case
        // leads to bugs in the UIA implementation in the TextRangeProvider
        // since we create leaf text positions, which means that they will
        // always have name from content. As such, for now we are
        // implementing this special case where we will only return the author
        // specified name if NameFrom is kAttribute and the name is not empty.
        // Even though a case like:
        // <button aria-label="label">hello</button>
        // Should have its name exposed as "label" according to the spec
        // but we will expose "hello" instead.
        // Exposing the aria label here would make us expose text that isn't on
        // a leaf position, and throughout our UIA implementation, we always
        // assume and expect to be on leaf positions. Exposing the label
        // when it has text from content would effectively hide the subtree
        // from UIA ATs
        // https://www.w3.org/TR/accname-1.1/#mapping_additional_nd_te

        if (IsInUnignoredEmptyObject()) {
          if (EmptyObjectShouldProvideNameFromAttribute()) {
            return GetAnchor()->GetNameUTF16();
          }
          return *embedded_character_str;
        }
        // However, for UIA, we don't want to expose the Hypertext like the
        // kExposeCharacterForHypertext case does, since that computation for
        // Hypertext is IA2-specific. Instead, UIA needs the text contents of
        // the node, which is what GetTextContentUTF16() returns.
        return GetAnchor()->GetTextContentUTF16();
    }
  }

  // Determines if this position is pointing to text inside a node that causes a
  // line break. For example, a tree position pointing to a <br> element or a
  // text node whose only content is the
  // '\n' character, or a text position pointing to a '\n' character in its
  // anchor's text representation.
  bool IsPointingToLineBreak() const {
    if (IsNullPosition())
      return false;

    // The position might be an ancestor position that does not currently point
    // to a line break node, but once resolved to a leaf position, it might do
    // so. This could only occur when we have a text position, because tree
    // positions do not point to text unless they are anchored directly to a
    // text node.
    if (IsTextPosition()) {
      AXPositionInstance leaf_text_position = AsLeafTextPosition();
      DCHECK(leaf_text_position->GetAnchor());
      if (leaf_text_position->GetAnchor()->IsLineBreak())
        return true;
      std::u16string text = leaf_text_position->GetText();
      if (text.empty() ||
          static_cast<size_t>(leaf_text_position->text_offset()) >=
              text.length()) {
        return false;
      }
      return text[leaf_text_position->text_offset()] == '\n';
    }

    // Tree position.
    return GetAnchor()->IsLineBreak();
  }

  // Determines if the anchor containing this position is a text object.
  bool IsInTextObject() const {
    if (IsNullPosition())
      return false;
    return GetAnchor()->IsText();
  }

  // Determines if the anchor containing this position is a text field object.
  bool IsInTextField() const {
    if (IsNullPosition()) {
      return false;
    }
    return GetAnchor()->data().IsTextField();
  }

  // Determines if the text representation of this position's anchor contains
  // only whitespace characters; <br> objects span a single '\n' character, so
  // positions inside line breaks are also considered "in whitespace". Note that
  // by the above definition, if a position is pointing to a whitespace
  // character, but not all of the text inside the position's anchor is
  // whitespace, this method returns false.
  bool IsInWhiteSpace() const {
    if (IsNullPosition())
      return false;
    if (GetAnchor()->IsLineBreak())
      return true;  // A <br> or a text node whose contents is a single '\n'.
    std::u16string text = GetText();
    // `base::ContainsOnlyChars` returns true if the text is empty, which is not
    // what we want here because the empty text is not the same as text with
    // only whitespace characters. So, we explicitly exclude that possibility.
    return !text.empty() &&
           base::ContainsOnlyChars(text, base::kWhitespaceUTF16);
  }

  // Returns the length of the text that is present inside the anchor node,
  // including any text found in descendant text nodes. This is based on the
  // platform's text representation. Some platforms use an embedded object
  // character that replaces the text coming from most child nodes and empty
  // objects.
  //
  // Similar to "text_offset_", the length of the text is in UTF16 code units,
  // not in grapheme clusters.
  int MaxTextOffset(const AXEmbeddedObjectBehavior embedded_object_behavior =
                        g_ax_embedded_object_behavior) const {
    if (IsNullPosition())
      return INVALID_OFFSET;

    switch (embedded_object_behavior) {
      case AXEmbeddedObjectBehavior::kSuppressCharacter:
        // TODO(nektar): Switch to anchor->GetTextContentLengthUTF8() after
        // AXPosition switches to using UTF8.
        return GetAnchor()->GetTextContentLengthUTF16();
      case AXEmbeddedObjectBehavior::kExposeCharacterForHypertext:
        // Special case: If a node has only ignored descendants, i.e., it
        // appears to be empty to assistive software, on some platforms we need
        // to still treat it as a character and a word boundary. We achieve this
        // by adding an "object replacement character" in the accessibility
        // tree's text representation, but we don't expose that character to
        // assistive software that tries to retrieve the node's text content or
        // hypertext.
        if (IsInUnignoredEmptyObject())
          return AXNode::kEmbeddedObjectCharacterLengthUTF16;
        return static_cast<int>(GetAnchor()->GetHypertext().length());
      case AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent:
        // For UIA, we still have the notion of embedded object characters for
        // text navigation purposes. I.e. when AT's need to navigate around
        // nodes and elements which are empty and should then be exposed as
        // embedded object characters, and as such we need to return the length
        // of the embedded object character when calculating the `MaxTextOffset`
        // for these nodes.
        //
        // According to the spec, we should favor author supplied names over
        // names from content. However, trying to fulfill this in every case
        // leads to bugs in the UIA implementation in the TextRangeProvider
        // since we create leaf text positions, which means that they will
        // always have name from content. As such, for now we are
        // implementing this special case where we will only return the author
        // specified name if NameFrom is kAttribute and the name is not empty.
        if (IsInUnignoredEmptyObject()) {
          if (EmptyObjectShouldProvideNameFromAttribute()) {
            return (int)GetAnchor()->GetNameUTF16().length();
          }
          return AXNode::kEmbeddedObjectCharacterLengthUTF16;
        }
        // However, for UIA, we don't want to expose the Hypertext like the
        // kExposeCharacterForHypertext case does, since that computation for
        // Hypertext is IA2-specific. Instead, UIA needs the text contents of
        // the node, so for `MaxTextOffset()` we should return the length of the
        // text content.
        return GetAnchor()->GetTextContentLengthUTF16();
    }
  }

  // Returns the accessibility role of this position's anchor node. If this is a
  // "null position", returns `ax::mojom::Role::kUnknown`.
  ax::mojom::Role GetRole() const {
    if (IsNullPosition())
      return ax::mojom::Role::kUnknown;
    return GetAnchor()->GetRole();
  }

  AXTextAttributes GetTextAttributes() const {
    // Check either the current anchor or its parent for text attributes.
    AXTextAttributes current_anchor_text_attributes =
        !IsNullPosition() ? GetAnchor()->GetTextAttributes()
                          : AXTextAttributes();
    if (current_anchor_text_attributes.IsUnset()) {
      AXPositionInstance parent_position =
          AsTreePosition()->CreateParentPosition(
              ax::mojom::MoveDirection::kBackward);
      if (!parent_position->IsNullPosition()) {
        return parent_position->GetAnchor()->GetTextAttributes();
      }
    }
    return current_anchor_text_attributes;
  }

 protected:
  AXPosition()
      : tree_id_(AXTreeIDUnknown()),
        anchor_id_(kInvalidAXNodeID),
        child_index_(INVALID_INDEX),
        text_offset_(INVALID_OFFSET) {}

  // We explicitly don't copy any cached members.
  AXPosition(const AXPosition& other)
      : kind_(other.kind_),
        tree_id_(other.tree_id_),
        anchor_id_(other.anchor_id_),
        child_index_(other.child_index_),
        text_offset_(other.text_offset_),
        affinity_(other.affinity_) {}

  // Returns the character offset inside our anchor's parent at which our text
  // starts.
  int AnchorTextOffsetInParent() const {
    if (IsNullPosition())
      return INVALID_OFFSET;

    // Calculate how much text there is to the left of this anchor.
    //
    // Work with a tree position so as not to incur any performance hit for
    // calculating the corresponding text offset in the parent anchor on
    // platforms that do not use an "object replacement character" to represent
    // child nodes.
    //
    // Ignored positions are not visible to platform APIs. As a result, their
    // text content or hypertext does not appear in their parent node, but the
    // text of their unignored children does. (See `AXNode::GetHypertext()` for
    // the meaning of "hypertext" in this context.
    AXPositionInstance tree_position =
        CreatePositionAtStartOfAnchor()->AsTreePosition();
    DCHECK(!tree_position->IsNullPosition());
    AXPositionInstance parent_position = tree_position->CreateParentPosition(
        ax::mojom::MoveDirection::kBackward);
    if (parent_position->IsNullPosition())
      return 0;  // There is only a single root node.

    int offset_in_parent = 0;
    for (int i = 0; i < parent_position->child_index(); ++i) {
      AXPositionInstance child = parent_position->CreateChildPositionAt(i);
      DCHECK(!child->IsNullPosition());
      offset_in_parent += child->MaxTextOffsetInParent();
    }
    return offset_in_parent;
  }

  // In the case of a text position, lazily initializes or returns the existing
  // grapheme iterator for the position's text. The grapheme iterator breaks at
  // every grapheme cluster boundary.
  //
  // We only allow creating this iterator on leaf nodes. We currently don't need
  // to move by grapheme boundaries on non-leaf nodes and computing plus caching
  // the text content for all nodes is costly.
  std::unique_ptr<base::i18n::BreakIterator> GetGraphemeIterator() const {
    if (!IsLeafTextPosition())
      return {};

    // TODO(nektar): Remove member variable `name_` once hypertext has been
    // migrated to AXNode. Currently, hypertext in AXNode gets updated every
    // time the `AXNode::GetHypertext()` method is called which erroniously
    // invalidates this AXPosition.
    name_ = GetText();
    auto grapheme_iterator = std::make_unique<base::i18n::BreakIterator>(
        name_, base::i18n::BreakIterator::BREAK_CHARACTER);
    if (!grapheme_iterator->Init())
      return {};
    return grapheme_iterator;
  }

  void InitializeWithoutValidation(AXPositionKind kind,
                                   AXTreeID tree_id,
                                   AXNodeID anchor_id,
                                   int child_index,
                                   int text_offset,
                                   ax::mojom::TextAffinity affinity) {
    kind_ = kind;
    tree_id_ = tree_id;
    anchor_id_ = anchor_id;
    child_index_ = child_index;
    text_offset_ = text_offset;
    affinity_ = affinity;

    if (!IsValid()) {
      // Reset to the null position.
      kind_ = AXPositionKind::NULL_POSITION;
      tree_id_ = AXTreeIDUnknown();
      anchor_id_ = kInvalidAXNodeID;
      child_index_ = INVALID_INDEX;
      text_offset_ = INVALID_OFFSET;
      affinity_ = ax::mojom::TextAffinity::kDownstream;
    }
  }

  void Initialize(AXPositionKind kind,
                  AXTreeID tree_id,
                  AXNodeID anchor_id,
                  int child_index,
                  int text_offset,
                  ax::mojom::TextAffinity affinity) {
    kind_ = kind;
    tree_id_ = tree_id;
    anchor_id_ = anchor_id;
    child_index_ = child_index;
    text_offset_ = text_offset;
    affinity_ = affinity;

    // TODO(accessibility) Consider using WeakPtr<AXTree> instead of an
    // AXTreeID, which would be both faster and easier to use in combination
    // with AXTreeSnapshotter, which does not use AXTreeManager to cache
    // AXTreeIDs in a map.
    SANITIZER_CHECK(GetManager() || kind_ == AXPositionKind::NULL_POSITION)
        << "Tree manager required, tree_id = " << tree_id.ToString()
        << "  is unknown = " << (tree_id == AXTreeIDUnknown());
    SANITIZER_CHECK(GetAnchor() || kind_ == AXPositionKind::NULL_POSITION)
        << "Creating a position without an anchor is disallowed:\n"
        << ToDebugString();

    // TODO(crbug.com/40885940) Remove this line and let the below IsValid()
    // assertion get triggered instead. We shouldn't be creating test positions
    // with offsets that are too large. This seems to occur when the anchor node
    // is ignored, and leads to a number of failing tests.
    // Comment this line out as a known performance culprit (also see
    // crbug.com/1401591).
    // SnapToMaxTextOffsetIfBeyond();

#if defined(AX_EXTRA_MAC_NODES)
    // Temporary hack to constrain child index when extra mac nodes are present.
    // TODO(accessibility) Remove this hack that works around the fact that Mac
    // can set a selection on extra mac nodes, which looks invalid because the
    // child index is larger than AnchorChildCount(), which does not account
    // for them. We need to get a child count that includes extra mac nodes,
    // similar to how BrowserAccessibility::PlatformChildCount() does.
    if (!IsValid() && IsTreePosition() && IsTableLike(GetAnchor()->GetRole()) &&
        child_index > AnchorChildCount()) {
      child_index_ = AnchorChildCount();
    }
#endif

    // TODO(crbug.com/40885940) see TODO above.
    // Also look for the failures in
    // AXPositionTest.AsLeafTextPositionBeforeCharacterIncludingGeneratedNewlines,
    // AXPlatformNodeTextRangeProviderTest.TestNormalizeTextRangeForceSameAnchorOnDegenerateRange.
    // SANITIZER_CHECK(IsValid()) << "Creating invalid positions is
    // disallowed:\n"
    //                            << ToDebugString();
  }

  int AnchorChildCount() const {
    if (!GetAnchor())
      return 0;
    return static_cast<int>(GetAnchor()->GetChildCountCrossingTreeBoundary());
  }

  // When a child is ignored, it looks for unignored nodes of that child's
  // children until there are no more descendants.
  //
  // For example:
  // ++TextField
  // ++++GenericContainer ignored
  // ++++++StaticText "Hello"
  // When we call the following method on TextField, it would return 1.
  int AnchorUnignoredChildCount() const {
    if (!GetAnchor())
      return 0;
    return static_cast<int>(
        GetAnchor()->GetUnignoredChildCountCrossingTreeBoundary());
  }

  int AnchorIndexInParent() const {
    // If this is the root tree, the index in parent will be 0.
    return GetAnchor() ? static_cast<int>(GetAnchor()->GetIndexInParent())
                       : INVALID_INDEX;
  }

  base::stack<AXNode*> GetAncestorAnchors() const {
    if (!GetAnchor())
      return base::stack<AXNode*>();
    return GetAnchor()->GetAncestorsCrossingTreeBoundaryAsStack();
  }

  AXNode* GetLowestUnignoredAncestor() const {
    if (!GetAnchor())
      return nullptr;
    return GetAnchor()->GetLowestPlatformAncestor();
  }

  // Returns the length of text (in UTF16 code points) that this anchor node
  // takes up in its parent.
  //
  // On some platforms, embedded objects are represented in their parent with a
  // single "embedded object character".
  int MaxTextOffsetInParent() const {
    if (IsNullPosition())
      return 0;

    // Ignored anchors are not visible to platform APIs. As a result, their
    // text content or hypertext does not appear in their parent node, but the
    // text of their unignored children does, if any. (See
    // `AXNode::GetHypertext()` for the meaning of "hypertext" in this context.
    if (!GetAnchor()->IsIgnored()) {
      if (IsEmbeddedObjectInParent())
        return AXNode::kEmbeddedObjectCharacterLengthUTF16;
    } else {
      // Ignored leaf (text) nodes might contain text content or hypertext, but
      // it should not be exposed in their parent.
      if (!AnchorUnignoredChildCount())
        return 0;
    }
    return MaxTextOffset();
  }

  // Returns whether or not this anchor is represented in their parent with a
  // single "object replacement character".
  bool IsEmbeddedObjectInParent() const {
    switch (g_ax_embedded_object_behavior) {
      case AXEmbeddedObjectBehavior::kSuppressCharacter:
        return false;
      case AXEmbeddedObjectBehavior::kExposeCharacterForHypertext:
        // We expose an "object replacement character" for all nodes except:
        // A) Textual nodes, such as static text, inline text boxes and line
        // breaks, and B) Nodes that are invisible to platform APIs.
        //
        // In the first case, textual nodes cannot be represented by an "object
        // replacement character" in the hypertext of their unignored parents,
        // because we want to maintain compatibility with how Firefox exposes
        // text in IAccessibleText. In the second case, ignored nodes and nodes
        // that are descendants of platform leaves should maintain the actual
        // text of all their static text descendants, otherwise there would be
        // loss of information while traversing the accessibility tree upwards.
        // An example of a platform leaf is an <input> text field, because all
        // of the accessibility subtree inside the text field is hidden from
        // platform APIs. An example of how an ignored node can affect the
        // hypertext of an unignored ancestor is shown below:
        // ++kTextField "Hello"
        // ++++kGenericContainer ignored "Hello"
        // ++++++kStaticText "Hello"
        // ++++++++kInlineTextBox "Hello"
        // The generic container, even though it is ignored, should nevertheless
        // maintain the text of its static text child and not use an "object
        // replacement character". Otherwise, the value of the text field would
        // be wrong.
        //
        // Please note that there is one more method that controls whether an
        // "object replacement character" would be exposed. See
        // `AXPosition::IsInUnignoredEmptyObject()`.
        return !IsNullPosition() && !GetAnchor()->IsIgnored() &&
               !GetAnchor()->IsText() && !GetAnchor()->IsChildOfLeaf();
      case AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent:
        return !IsNullPosition() && !GetAnchor()->IsIgnored() &&
               GetAnchor()->IsLeaf() && IsInUnignoredEmptyObject();
    }
  }

  // Determines if the anchor containing this position produces a hard line
  // break in the text representation, e.g. the anchor is a block level element
  // or a <br>.
  bool IsInLineBreakingObject() const {
    if (IsNullPosition())
      return false;
    return GetAnchor()->GetBoolAttribute(
        ax::mojom::BoolAttribute::kIsLineBreakingObject);
  }

  ax::mojom::Role GetAnchorRole() const {
    if (IsNullPosition())
      return ax::mojom::Role::kUnknown;
    return GetRole(GetAnchor());
  }

  ax::mojom::Role GetRole(AXNode* node) const { return node->GetRole(); }

  const std::vector<int32_t>& GetWordStartOffsets() const {
    if (IsNullPosition()) {
      static const base::NoDestructor<std::vector<int32_t>> empty_word_starts;
      return *empty_word_starts;
    }
    DCHECK(GetAnchor());

    // An embedded object replacement character is exposed in a node's text
    // representation when a control, such as a text field, is empty. Since the
    // control has no text, no word start offsets are present in the
    // `ax::mojom::IntListAttribute::kWordStarts` attribute, so we need to
    // special case them here.
    //
    // For the kUIAExposeCharacterForHypertext case, we only want to return a
    // vector with {0} if the empty object does not have an author specified
    // name that we are exposing.
    if (IsInUnignoredEmptyObject() &&
        (g_ax_embedded_object_behavior ==
             AXEmbeddedObjectBehavior::kExposeCharacterForHypertext ||
         (g_ax_embedded_object_behavior ==
              AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent &&
          !EmptyObjectShouldProvideNameFromAttribute()))) {
      // Using braces ensures that the vector will contain the given value, and
      // not create a vector of size 0.
      static const base::NoDestructor<std::vector<int32_t>>
          embedded_word_starts{{0}};
      return *embedded_word_starts;
    }

    return GetAnchor()->GetIntListAttribute(
        ax::mojom::IntListAttribute::kWordStarts);
  }

  const std::vector<int32_t>& GetWordEndOffsets() const {
    if (IsNullPosition()) {
      static const base::NoDestructor<std::vector<int32_t>> empty_word_ends;
      return *empty_word_ends;
    }
    DCHECK(GetAnchor());

    // An embedded object replacement character is exposed in a node's text
    // representation when a control, such as a text field, is empty. Since the
    // control has no text, no word end offsets are present in the
    // `ax::mojom::IntListAttribute::kWordEnds` attribute, so we need to special
    // case them here.
    //
    // Since the whole text exposed inside of an embedded object is of
    // length 1 (the embedded object replacement character), the word end offset
    // is positioned at 1. Because we want to treat embedded object replacement
    // characters as ordinary characters, it wouldn't be consistent to assume
    // they have no length and return 0 instead of 1.
    if (IsInUnignoredEmptyObject() &&
        (g_ax_embedded_object_behavior ==
             AXEmbeddedObjectBehavior::kExposeCharacterForHypertext ||
         (g_ax_embedded_object_behavior ==
              AXEmbeddedObjectBehavior::kUIAExposeCharacterForTextContent &&
          !EmptyObjectShouldProvideNameFromAttribute()))) {
      // Using braces ensures that the vector will contain the given value, and
      // not create a vector of size 1.
      static const base::NoDestructor<std::vector<int32_t>> embedded_word_ends{
          {1}};
      return *embedded_word_ends;
    }

    return GetAnchor()->GetIntListAttribute(
        ax::mojom::IntListAttribute::kWordEnds);
  }

  AXNodeID GetNextOnLineID() const {
    if (IsNullPosition())
      return kInvalidAXNodeID;
    DCHECK(GetAnchor());

    if (GetAnchor()->HasIntAttribute(ax::mojom::IntAttribute::kNextOnLineId)) {
      return static_cast<AXNodeID>(
          GetAnchor()->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId));
    }
    AXNode* parent = GetAnchor()->GetUnignoredParent();

    if (!parent) {
      return kInvalidAXNodeID;
    }

    // We should not need to bubble up to find the NextOnLine if we are not
    // in an InlineTextBox, because the only cases where the relevant NextOnLine
    // information is stored in the parent is in cases where we have text inside
    // inline-block elements.
    //
    // We only want to bubble up to the parent to find the nextOnLine
    // if we are in a leaf that is a last child.
    // This is because if we have a structure where there are multiple
    // InlineTextBox children that are in different lines, and the parent's
    // NextOnLine only applies to the last child.
    if (GetAnchor()->GetRole() != ax::mojom::Role::kInlineTextBox ||
        parent->GetLastUnignoredChild() != GetAnchor()) {
      return kInvalidAXNodeID;
    }

    while (parent &&
           !parent->HasIntAttribute(ax::mojom::IntAttribute::kNextOnLineId)) {
      parent = parent->GetUnignoredParent();
    }

    if (parent) {
      return static_cast<AXNodeID>(
          parent->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId));
    }
    return kInvalidAXNodeID;
  }

  AXNodeID GetPreviousOnLineID() const {
    if (IsNullPosition())
      return kInvalidAXNodeID;
    DCHECK(GetAnchor());

    if (GetAnchor()->HasIntAttribute(
            ax::mojom::IntAttribute::kPreviousOnLineId)) {
      return static_cast<AXNodeID>(GetAnchor()->GetIntAttribute(
          ax::mojom::IntAttribute::kPreviousOnLineId));
    }
    AXNode* parent = GetAnchor()->GetUnignoredParent();

    if (!parent) {
      return kInvalidAXNodeID;
    }

    // We should not need to bubble up to find the PreviousOnLine if we are not
    // in an InlineTextBox, because the only cases where the relevant
    // PreviousOnLine information is stored in the parent is in cases where we
    // have text inside inline-block elements.
    //
    // We have some expectations that
    // line break elements are not expected to have a previous on line element.
    //
    // We only want to bubble up to the parent to find the previousOnLine
    // if we are in a leaf that is a first child.
    // This is because if we have a structure where there are multiple
    // InlineTextBox children that are in different lines, and the parent's
    // PreviousOnLine only applies to the first child.
    if (GetAnchor()->GetRole() != ax::mojom::Role::kInlineTextBox ||
        parent->GetRole() == ax::mojom::Role::kLineBreak ||
        parent->GetFirstUnignoredChild() != GetAnchor()) {
      return kInvalidAXNodeID;
    }

    while (parent && !parent->HasIntAttribute(
                         ax::mojom::IntAttribute::kPreviousOnLineId)) {
      parent = parent->GetUnignoredParent();
    }

    if (parent) {
      return static_cast<AXNodeID>(
          parent->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId));
    }
    return kInvalidAXNodeID;
  }

 private:
  // Defines the relationship between positions during traversal.
  // For example, moving from a descendant to an ancestor, is a kAncestor move.
  enum class AXMoveType {
    kAncestor,
    kDescendant,
    kSibling,
  };

  // Defines the direction of position movement, either next / previous in tree.
  enum class AXMoveDirection {
    kNextInTree,
    kPreviousInTree,
  };

  // Type of predicate function called during anchor navigation.
  // When the predicate returns |true|, the navigation stops and returns a
  // null position object.
  using AbortMovePredicate =
      base::RepeatingCallback<bool(const AXPosition& move_from,
                                   const AXPosition& move_to,
                                   const AXMoveType type,
                                   const AXMoveDirection direction)>;

  // A text span is defined by a series of inline text boxes that make up a
  // single static text object.
  bool AtEndOfTextSpan() const {
    if (GetAnchorRole() != ax::mojom::Role::kInlineTextBox || !AtEndOfAnchor())
      return false;

    // We are at the end of text span if |this| position has
    // role::kInlineTextBox, the parent of |this| has role::kStaticText, and the
    // anchor node of |this| is the last child of its parent's children.
    const bool is_last_child =
        AnchorIndexInParent() == (GetAnchorSiblingCount() - 1);

    DCHECK(GetAnchor());
    return is_last_child &&
           GetRole(GetAnchor()->GetParentCrossingTreeBoundary()) ==
               ax::mojom::Role::kStaticText;
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreateNextAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance current_position = AsTreePosition();
    DCHECK(!current_position->IsNullPosition());

    if (!IsLeaf()) {
      const int child_index = current_position->child_index_;
      if (child_index < current_position->AnchorChildCount()) {
        AXPositionInstance child_position =
            current_position->CreateChildPositionAt(child_index);

        if (abort_predicate.Run(*current_position, *child_position,
                                AXMoveType::kDescendant,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return child_position;
      }
    }

    AXPositionInstance parent_position =
        current_position->CreateParentPosition();

    // Get the next sibling if it exists, otherwise move up the AXTree to the
    // lowest next sibling of this position's ancestors.
    while (!parent_position->IsNullPosition()) {
      const int index_in_parent = current_position->AnchorIndexInParent();
      if (index_in_parent + 1 < parent_position->AnchorChildCount()) {
        AXPositionInstance next_sibling =
            parent_position->CreateChildPositionAt(index_in_parent + 1);
        DCHECK(!next_sibling->IsNullPosition());

        if (abort_predicate.Run(*current_position, *next_sibling,
                                AXMoveType::kSibling,
                                AXMoveDirection::kNextInTree)) {
          return CreateNullPosition();
        }
        return next_sibling;
      }

      if (abort_predicate.Run(*current_position, *parent_position,
                              AXMoveType::kAncestor,
                              AXMoveDirection::kNextInTree)) {
        return CreateNullPosition();
      }

      current_position = std::move(parent_position);
      parent_position = current_position->CreateParentPosition();
    }
    return CreateNullPosition();
  }

  // Uses depth-first pre-order traversal.
  AXPositionInstance CreatePreviousAnchorPosition(
      const AbortMovePredicate& abort_predicate) const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance current_position = AsTreePosition();
    DCHECK(!current_position->IsNullPosition());

    AXPositionInstance parent_position =
        current_position->CreateParentPosition();
    if (parent_position->IsNullPosition())
      return parent_position;

    // If there is no previous sibling, or the parent itself is a leaf, move up
    // to the parent. The parent can be a leaf if we start with a tree position
    // that is a descendant of a node that is an empty control represented by an
    // "object replacement character" (see `IsInUnignoredEmptyObject()`).
    const int index_in_parent = current_position->AnchorIndexInParent();
    if (index_in_parent <= 0 || parent_position->IsLeaf()) {
      if (abort_predicate.Run(*current_position, *parent_position,
                              AXMoveType::kAncestor,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }
      return parent_position;
    }

    // Get the previous sibling's deepest last child.
    AXPositionInstance rightmost_leaf =
        parent_position->CreateChildPositionAt(index_in_parent - 1);
    DCHECK(!rightmost_leaf->IsNullPosition());

    if (abort_predicate.Run(*current_position, *rightmost_leaf,
                            AXMoveType::kSibling,
                            AXMoveDirection::kPreviousInTree)) {
      return CreateNullPosition();
    }

    CHECK(!rightmost_leaf->IsNullPosition());
    while (!rightmost_leaf->IsLeaf()) {
      parent_position = std::move(rightmost_leaf);
      rightmost_leaf = parent_position->CreateChildPositionAt(
          parent_position->AnchorChildCount() - 1);
      DCHECK(!rightmost_leaf->IsNullPosition());

      if (abort_predicate.Run(*parent_position, *rightmost_leaf,
                              AXMoveType::kDescendant,
                              AXMoveDirection::kPreviousInTree)) {
        return CreateNullPosition();
      }
      CHECK(!rightmost_leaf->IsNullPosition());
    }
    return rightmost_leaf;
  }

  // Creates a text position using the next leaf node as its anchor.
  // Nearly all of the text in the accessibility tree is contained in leaf
  // nodes, so this method is mostly used to move through text nodes.
  AXPositionInstance CreateNextLeafTextPosition(
      const AbortMovePredicate& abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && !IsLeaf())
      return AsLeafTextPosition();

    AXPositionInstance next_leaf = CreateNextAnchorPosition(abort_predicate);
    while (!next_leaf->IsNullPosition() && !next_leaf->IsLeaf())
      next_leaf = next_leaf->CreateNextAnchorPosition(abort_predicate);

    DCHECK(next_leaf);
    return next_leaf->AsLeafTextPosition();
  }

  // Creates a text position using the previous leaf node as its anchor.
  // Nearly all of the text in the accessibility tree is contained in leaf
  // nodes, so this method is mostly used to move through text nodes.
  AXPositionInstance CreatePreviousLeafTextPosition(
      const AbortMovePredicate& abort_predicate) const {
    // If this is an ancestor text position, resolve to its leaf text position.
    if (IsTextPosition() && !IsLeaf())
      return AsLeafTextPosition();

    AXPositionInstance previous_leaf =
        CreatePreviousAnchorPosition(abort_predicate);
    while (!previous_leaf->IsNullPosition() && !previous_leaf->IsLeaf()) {
      previous_leaf =
          previous_leaf->CreatePreviousAnchorPosition(abort_predicate);
    }

    DCHECK(previous_leaf);
    return previous_leaf->AsLeafTextPosition();
  }

  // Creates a tree position using the next leaf node as its anchor.
  // Nearly all of the text in the accessibility tree is contained in leaf
  // nodes, so this method is mostly used to move through text nodes.
  AXPositionInstance CreateNextLeafTreePosition(
      const AbortMovePredicate& abort_predicate) const {
    AXPositionInstance next_leaf =
        AsTreePosition()->CreateNextAnchorPosition(abort_predicate);
    while (!next_leaf->IsNullPosition() && !next_leaf->IsLeaf())
      next_leaf = next_leaf->CreateNextAnchorPosition(abort_predicate);

    DCHECK(next_leaf);
    return next_leaf;
  }

  // Creates a tree position using the previous leaf node as its anchor.
  // Nearly all of the text in the accessibility tree is contained in leaf
  // nodes, so this method is mostly used to move through text nodes.
  AXPositionInstance CreatePreviousLeafTreePosition(
      const AbortMovePredicate& abort_predicate) const {
    AXPositionInstance previous_leaf =
        AsTreePosition()->CreatePreviousAnchorPosition(abort_predicate);
    while (!previous_leaf->IsNullPosition() && !previous_leaf->IsLeaf()) {
      previous_leaf =
          previous_leaf->CreatePreviousAnchorPosition(abort_predicate);
    }

    DCHECK(previous_leaf);
    return previous_leaf;
  }

  //
  // Static helpers for lambda usage.
  //

  static bool AtStartOfPagePredicate(const AXPositionInstance& position) {
    // If a page boundary is ignored, then it should not be exposed to assistive
    // software.
    return !position->IsIgnored() && position->AtStartOfPage();
  }

  static bool AtEndOfPagePredicate(const AXPositionInstance& position) {
    // If a page boundary is ignored, then it should not be exposed to assistive
    // software.
    return !position->IsIgnored() && position->AtEndOfPage();
  }

  static bool AtStartOfParagraphPredicate(const AXPositionInstance& position) {
    // Sometimes, nodes that are used to signify paragraph boundaries are
    // ignored, e.g. <div aria-hidden="true"></div>". We make the design
    // decision to expose such boundaries to assistive software. Their
    // associated ignored nodes are still not exposed. This ensures that
    // navigation keys in text fields, such as Ctrl+Up/Down, will behave the
    // same way as related screen reader commands.
    return position->AtStartOfParagraph();
  }

  static bool AtStartOfParagraphExcludingEmptyParagraphsPredicate(
      const AXPositionInstance& position) {
    // For UI Automation, empty lines after a paragraph should be merged into
    // the preceding paragraph.
    //
    // See
    // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-uiautomationtextunits#paragraph
    const bool is_empty_paragraph =
        position->IsPointingToLineBreak() ||
        (position->IsInLineBreakingObject() &&
         (position->GetAnchor()->IsEmptyLeaf() || position->GetText().empty()));
    return !is_empty_paragraph && AtStartOfParagraphPredicate(position);
  }

  static bool AtEndOfParagraphPredicate(const AXPositionInstance& position) {
    // Sometimes, nodes that are used to signify paragraph boundaries are
    // ignored, e.g. <div aria-hidden="true"></div>". We make the design
    // decision to expose such boundaries to assistive software. Their
    // associated ignored nodes are still not exposed. This ensures that
    // navigation keys in text fields, such as Ctrl+Up/Down, will behave the
    // same way as related screen reader commands.
    return position->AtEndOfParagraph();
  }

  static bool AtStartOfLinePredicate(const AXPositionInstance& position) {
    // Sometimes, nodes that are used to signify line boundaries are ignored,
    // e.g. <span contenteditable="false"> <br role="presentation"></span> which
    // is used to make a hard line break appear as a soft one. We make the
    // design decision to expose such boundaries to assistive software. Their
    // associated ignored nodes are still not exposed.
    return position->AtStartOfLine();
  }

  static bool AtEndOfLinePredicate(const AXPositionInstance& position) {
    // Sometimes, nodes that are used to signify line boundaries are ignored,
    // e.g. <span contenteditable="false"> <br role="presentation"></span> which
    // is used to make a hard line break appear as a soft one. We make the
    // design decision to expose such boundaries to assistive software. Their
    // associated ignored nodes are still not exposed.
    return position->AtEndOfLine();
  }

  static bool AtStartOfSentencePredicate(const AXPositionInstance& position) {
    // Sentence boundaries should be at specific text offsets that are "visible"
    // to assistive software, hence not ignored. Ignored nodes are often used
    // for additional layout information, such as line and paragraph boundaries.
    // Their text is not currently processed.
    return !position->IsIgnored() && position->AtStartOfSentence();
  }

  static bool AtEndOfSentencePredicate(const AXPositionInstance& position) {
    // Sentence boundaries should be at specific text offsets that are "visible"
    // to assistive software, hence not ignored. Ignored nodes are often used
    // for additional layout information, such as line and paragraph boundaries.
    // Their text is not currently processed.
    return !position->IsIgnored() && position->AtEndOfSentence();
  }

  static bool AtStartOfFormatPredicate(const AXPositionInstance& position) {
    return position->AtStartOfFormat();
  }

  static bool AtEndOfFormatPredicate(const AXPositionInstance& position) {
    return position->AtEndOfFormat();
  }

  static bool AtStartOfWordPredicate(const AXPositionInstance& position) {
    // Word boundaries should be at specific text offsets that are "visible" to
    // assistive software, hence not ignored. Ignored nodes are often used for
    // additional layout information, such as line and paragraph boundaries.
    // Their text is not currently processed.
    return !position->IsIgnored() && position->AtStartOfWord();
  }

  static bool AtEndOfWordPredicate(const AXPositionInstance& position) {
    // Word boundaries should be at specific text offsets that are "visible" to
    // assistive software, hence not ignored. Ignored nodes are often used for
    // additional layout information, such as line and paragraph boundaries.
    // Their text is not currently processed.
    return !position->IsIgnored() && position->AtEndOfWord();
  }

  static bool DefaultAbortMovePredicate(const AXPosition& move_from,
                                        const AXPosition& move_to,
                                        const AXMoveType move_type,
                                        const AXMoveDirection direction) {
    // Default behavior is to never abort.
    return false;
  }

  // AbortMovePredicate function used to detect format boundaries.
  static bool AbortMoveAtFormatBoundary(const AXPosition& move_from,
                                        const AXPosition& move_to,
                                        const AXMoveType move_type,
                                        const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition() ||
        move_from.IsInUnignoredEmptyObject() ||
        move_to.IsInUnignoredEmptyObject()) {
      return true;
    }

    // Treat moving into or out of nodes with certain roles as a format break.
    ax::mojom::Role from_role = move_from.GetAnchorRole();
    ax::mojom::Role to_role = move_to.GetAnchorRole();
    if (from_role != to_role) {
      if (IsFormatBoundary(from_role) || IsFormatBoundary(to_role))
        return true;
    }

    // Stop moving when text attributes differ.
    return move_from.AsLeafTreePosition()->GetTextAttributes() !=
           move_to.AsLeafTreePosition()->GetTextAttributes();
  }

  static bool MoveCrossesLineBreakingObject(
      const ax::mojom::TextBoundary paragraph_boundary,
      const AXPosition& move_from,
      const AXPosition& move_to,
      const AXMoveType move_type,
      const AXMoveDirection direction) {
    const AXPosition* proceeding_position = &move_from;
    const AXPosition* trailing_position = &move_to;
    switch (direction) {
      case AXMoveDirection::kNextInTree:
        break;
      case AXMoveDirection::kPreviousInTree:
        std::swap(proceeding_position, trailing_position);
        break;
    }

    switch (paragraph_boundary) {
      case ax::mojom::TextBoundary::kParagraphEnd: {
        const bool trailing_block = trailing_position->IsInLineBreakingObject();
        const bool trailing_line_break =
            trailing_position->IsPointingToLineBreak();
        return trailing_block || trailing_line_break;
      }
      case ax::mojom::TextBoundary::kParagraphStart: {
        // The trailing object does not need to be a block or a line break for
        // it to represent a start of a new paragraph.
        //
        // 1. Preceding block before "world" creates a paragraph start:
        // <div><p>hello</p>world</div>
        // 2. Preceding line break before "world" creates a paragraph start:
        // <div>Hello<br>world</div>
        const bool preceding_block =
            proceeding_position->IsInLineBreakingObject();
        const bool preceding_line_break =
            proceeding_position->IsPointingToLineBreak();
        return preceding_block || preceding_line_break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }

  // AbortMovePredicate function used to detect paragraph boundaries.
  static bool AbortMoveAtParagraphBoundary(
      const ax::mojom::TextBoundary paragraph_boundary,
      const AXPosition& move_from,
      const AXPosition& move_to,
      const AXMoveType move_type,
      const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition() ||
        move_from.IsInUnignoredEmptyObject() ||
        move_to.IsInUnignoredEmptyObject()) {
      // We deliberately put empty objects, such as empty text fields, in their
      // own paragraph for easier navigation. Otherwise, they could easily be
      // missed by screen reader users.
      return true;
    }

    return MoveCrossesLineBreakingObject(paragraph_boundary, move_from, move_to,
                                         move_type, direction);
  }

  // AbortMovePredicate function used to detect page boundaries.
  //
  // Depending on the type of content, it might be separated into a number of
  // pages. For example, a PDF document may expose multiple pages.
  static bool AbortMoveAtPageBoundary(const AXPosition& move_from,
                                      const AXPosition& move_to,
                                      const AXMoveType move_type,
                                      const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    const bool move_from_break = move_from.GetAnchor()->GetBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject);
    const bool move_to_break = move_to.GetAnchor()->GetBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject);

    switch (move_type) {
      case AXMoveType::kAncestor:
        // For Ancestor moves, only abort when exiting a page break.
        // We don't care if the ancestor is a page break or not, since the
        // descendant is contained by it.
        return move_from_break;
      case AXMoveType::kDescendant:
        // For Descendant moves, only abort when entering a page break
        // descendant. We don't care if the ancestor is a page break  or not,
        // since the descendant is contained by it.
        return move_to_break;
      case AXMoveType::kSibling:
        // For Sibling moves, abort if both of the siblings are a page break,
        // because that would mean exiting and/or entering a page break.
        return move_from_break && move_to_break;
    }
  }

  // AbortMovePredicate function used to detect crossing through the boundaries
  // of a window-like container, such as a webpage, a PDF, a dialog, the
  // browser's UI (AKA Views), or the whole desktop. Window-like containers
  // that are ignored should not cause us to abort. For example, a hidden dialog
  // should not cause a break.
  static bool AbortMoveAtRootBoundary(const AXPosition& move_from,
                                      const AXPosition& move_to,
                                      const AXMoveType move_type,
                                      const AXMoveDirection direction) {
    // Positions are null when moving past the whole content, therefore the root
    // of a window-like container has certainly been crossed.
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    const ax::mojom::Role move_from_role = move_from.GetAnchorRole();
    const ax::mojom::Role move_to_role = move_to.GetAnchorRole();
    switch (move_type) {
      case AXMoveType::kAncestor:
        // For Ancestor moves, only abort when exiting an unignored window-like
        // container. We don't care if the ancestor is the root of a window-like
        // container or not, since the descendant is contained by it. However,
        // we do care if the ancestor is an iframe because a webpage should be
        // navigated as a single document together with all its iframes,
        // (out-of-process or otherwise).
        return IsRootLike(move_from_role) && !IsIframe(move_to_role) &&
               !move_from.IsIgnored();
      case AXMoveType::kDescendant:
        // For Descendant moves, only abort when entering an unignored
        // window-like container. We don't care if the ancestor is the root of a
        // window-like container or not, since the descendant is contained by
        // it. However, we do care if the ancestor is an iframe because a
        // webpage should be navigated as a single document together with all
        // its iframes, (out-of-process or otherwise).
        return IsRootLike(move_to_role) && !IsIframe(move_from_role) &&
               !move_to.IsIgnored();
      case AXMoveType::kSibling:
        // For Sibling moves, abort if both of the siblings are at the root of
        // unignored window-like containers because that would mean exiting
        // and/or entering a new window-like container. Iframes should not be
        // present in this case because an iframe should never contain more than
        // one kRootWebArea as its immediate child.
        return IsRootLike(move_from_role) && IsRootLike(move_to_role) &&
               !move_from.IsIgnored() && !move_to.IsIgnored();
    }
  }

  static bool AbortMoveAtStartOfInlineBlock(const AXPosition& move_from,
                                            const AXPosition& move_to,
                                            const AXMoveType move_type,
                                            const AXMoveDirection direction) {
    if (move_from.IsNullPosition() || move_to.IsNullPosition())
      return true;

    // These will only be available if AXMode has kHTML set.
    const bool move_from_is_inline_block =
        move_from.GetAnchor()->GetStringAttribute(
            ax::mojom::StringAttribute::kDisplay) == "inline-block";
    const bool move_to_is_inline_block =
        move_to.GetAnchor()->GetStringAttribute(
            ax::mojom::StringAttribute::kDisplay) == "inline-block";

    switch (direction) {
      case AXMoveDirection::kNextInTree:
        // When moving forward, break if we enter an inline block.
        return move_to_is_inline_block &&
               (move_type == AXMoveType::kDescendant ||
                move_type == AXMoveType::kSibling);
      case AXMoveDirection::kPreviousInTree:
        // When moving backward, break if we exit an inline block.
        return move_from_is_inline_block &&
               (move_type == AXMoveType::kAncestor ||
                move_type == AXMoveType::kSibling);
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  static const std::vector<int32_t>& GetSentenceStartOffsetsFunc(
      const AXPositionInstance& position) {
    if (position->IsNullPosition()) {
      static const base::NoDestructor<std::vector<int32_t>>
          empty_sentence_starts;
      return *empty_sentence_starts;
    }
    DCHECK(position->GetAnchor());
    return position->GetAnchor()->GetIntListAttribute(
        ax::mojom::IntListAttribute::kSentenceStarts);
  }

  static const std::vector<int32_t>& GetSentenceEndOffsetsFunc(
      const AXPositionInstance& position) {
    if (position->IsNullPosition()) {
      static const base::NoDestructor<std::vector<int32_t>> empty_sentence_ends;
      return *empty_sentence_ends;
    }
    DCHECK(position->GetAnchor());
    return position->GetAnchor()->GetIntListAttribute(
        ax::mojom::IntListAttribute::kSentenceEnds);
  }

  static const std::vector<int32_t>& GetWordStartOffsetsFunc(
      const AXPositionInstance& position) {
    return position->GetWordStartOffsets();
  }

  static const std::vector<int32_t>& GetWordEndOffsetsFunc(
      const AXPositionInstance& position) {
    return position->GetWordEndOffsets();
  }

  // Creates an ancestor equivalent position at the root node of this position's
  // accessibility tree, e.g. at the root of the current iframe (out-of-process
  // or not), PDF plugin, Views tree, dialog (native, ARIA or HTML), window, or
  // the whole desktop.
  //
  // For a similar method that does not stop at all iframe boundaries, see
  // `CreateRootAncestorPosition`.
  //
  // See `CreateParentPosition` for an explanation of the use of
  // |move_direction|.
  AXPositionInstance CreateAXTreeRootAncestorPosition(
      ax::mojom::MoveDirection move_direction) const {
    if (IsNullPosition())
      return Clone();

    AXPositionInstance root_position = Clone();
    while (!IsRootLike(root_position->GetAnchorRole())) {
      AXPositionInstance parent_position =
          root_position->CreateParentPosition(move_direction);
      if (parent_position->IsNullPosition())
        break;
      root_position = std::move(parent_position);
    }

    return root_position;
  }

  // Creates an ancestor equivalent position at the root node of all content,
  // e.g. at the root of the whole webpage, PDF plugin, Views tree, dialog
  // (native, ARIA or HTML), window, or the whole desktop.
  //
  // Note that this method will break out of an out-of-process iframe and return
  // a position at the root of the top-level document, but it will not break
  // into the Views tree if present. For a similar method that stops at all
  // iframe boundaries, see `CreateAXTreeRootAncestorPosition`.
  //
  // See `CreateParentPosition` for an explanation of the use of
  // |move_direction|.
  AXPositionInstance CreateRootAncestorPosition(
      ax::mojom::MoveDirection move_direction) const {
    AXPositionInstance root_position =
        CreateAXTreeRootAncestorPosition(move_direction);
    AXPositionInstance web_root_position = CreateNullPosition();
    for (; !root_position->IsNullPosition();
         root_position =
             root_position->CreateAXTreeRootAncestorPosition(move_direction)) {
      // An "ax::mojom::Role::kRootWebArea" could also be present at the root of
      // iframes or embedded objects, so we need to check that for that specific
      // role the position is also at the top of the forest of accessibility
      // trees making up the webpage. Note that the forest of accessibility
      // trees would include Views and on Chrome OS the whole desktop, so in the
      // case of a web root, checking if the parent position is the null
      // position will not work.
      if (root_position->GetAnchorRole() != ax::mojom::Role::kRootWebArea) {
        if (web_root_position->IsNullPosition())
          return root_position;  // Original position is not in web contents.

        // The previously saved web root is the shallowest in the forest of
        // accessibility trees.
        return web_root_position;
      }

      // Save this web root position and check if it is the shallowest in the
      // forest of accessibility trees.
      web_root_position = root_position->Clone();
      root_position = root_position->CreateParentPosition(move_direction);
    }
    return web_root_position;
  }

  // Creates a text position that is in the same anchor as the current
  // position, but starting from the current text offset, adjusts to the next
  // or the previous boundary offset depending on the boundary direction. If
  // there is no next / previous offset, the current text offset is unchanged.
  AXPositionInstance CreatePositionAtNextOffsetBoundary(
      ax::mojom::MoveDirection move_direction,
      BoundaryTextOffsetsFunc get_offsets) const {
    if (IsNullPosition() || get_offsets.is_null())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    const std::vector<int32_t>& boundary_offsets =
        get_offsets.Run(text_position);
    if (boundary_offsets.empty())
      return text_position;

    switch (move_direction) {
      case ax::mojom::MoveDirection::kNone:
        NOTREACHED_IN_MIGRATION();
        return CreateNullPosition();
      case ax::mojom::MoveDirection::kBackward: {
        auto offsets_iterator =
            std::lower_bound(boundary_offsets.begin(), boundary_offsets.end(),
                             int32_t{text_position->text_offset_});
        // If there is no previous offset, the current offset should be
        // unchanged.
        if (offsets_iterator > boundary_offsets.begin()) {
          // Since we already checked if "boundary_offsets" are non-empty, we
          // can safely move the iterator one position back, even if it's
          // currently at the vector's end.
          --offsets_iterator;
          auto offsets_iterator_ref = *offsets_iterator;
          text_position->text_offset_ = static_cast<int>(offsets_iterator_ref);
          text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
        }
        break;
      }
      case ax::mojom::MoveDirection::kForward: {
        const auto offsets_iterator =
            std::upper_bound(boundary_offsets.begin(), boundary_offsets.end(),
                             int32_t{text_position->text_offset_});
        // If there is no next offset, the current offset should be unchanged.
        if (offsets_iterator < boundary_offsets.end()) {
          auto offsets_iterator_ref = *offsets_iterator;
          text_position->text_offset_ = static_cast<int>(offsets_iterator_ref);
          text_position->affinity_ = ax::mojom::TextAffinity::kDownstream;
        }
        break;
      }
    }

    return text_position;
  }

  // Creates a text position that is in the same anchor as the current
  // position, but adjusts its text offset to be either at the first or last
  // offset boundary, based on the boundary direction. When moving forward,
  // the text position is adjusted to point to the first offset boundary, or
  // to the end of its anchor if there are no offset boundaries. When moving
  // backward, it is adjusted to point to the last offset boundary, or to the
  // start of its anchor if there are no offset boundaries.
  AXPositionInstance CreatePositionAtFirstOffsetBoundary(
      ax::mojom::MoveDirection move_direction,
      BoundaryTextOffsetsFunc get_offsets) const {
    if (IsNullPosition() || get_offsets.is_null())
      return Clone();

    AXPositionInstance text_position = AsTextPosition();
    const std::vector<int32_t>& boundary_offsets =
        get_offsets.Run(text_position);
    switch (move_direction) {
      case ax::mojom::MoveDirection::kNone:
        NOTREACHED_IN_MIGRATION();
        return CreateNullPosition();
      case ax::mojom::MoveDirection::kBackward:
        if (boundary_offsets.empty()) {
          return text_position->CreatePositionAtStartOfAnchor();
        } else {
          text_position->text_offset_ =
              int{boundary_offsets[boundary_offsets.size() - 1]};
          return text_position;
        }
        break;
      case ax::mojom::MoveDirection::kForward:
        if (boundary_offsets.empty()) {
          return text_position->CreatePositionAtEndOfAnchor();
        } else {
          text_position->text_offset_ = int{boundary_offsets[0]};
          return text_position;
        }
        break;
    }
  }

  // Returns the next unignored leaf text position in the specified direction,
  // also ensuring that *AsLeafTextPosition() !=
  // *CreateAdjacentLeafTextPosition() is true; returns a null position if no
  // adjacent position exists.
  //
  // This method is the first step for CreateBoundary[Start|End]Position to
  // guarantee that the resulting position when using a boundary behavior other
  // than `AXBoundaryBehavior::kStopAtAnchorBoundaryOrIfAlreadyAtBoundary` is
  // not equivalent to the initial position. That's why ignored positions are
  // also skipped. Otherwise, if a boundary is present on an ignored position,
  // the search for the next or previous boundary would stop prematurely. Note
  // that if there are multiple adjacent ignored positions and all of them
  // create a boundary, we'll skip them all on purpose. For example, adjacent
  // ignored paragraph boundaries could be created by using multiple aria-hidden
  // divs next to one another. These should not contribute more than one
  // paragraph boundary to the tree's text representation, otherwise this will
  // create user confusion.
  //
  // Note that using the `CompareTo` method with text positions does not take
  // into account position affinity or the order of their anchors in the tree:
  // two text positions are considered equivalent if their offsets in the text
  // representation of the entire AXTree are the same. As such, using
  // Create[Next|Previous]LeafTextPosition is not enough to create adjacent
  // positions, e.g. the end of an anchor and the start of the next one are
  // equivalent; furthermore, there could be nodes with no text between them,
  // all of them being equivalent too.
  //
  // IMPORTANT! This method basically moves the given position one character
  // forward/backward, but it could end up at the middle of a grapheme cluster,
  // so it shouldn't be used to move by ax::mojom::TextBoundary::kCharacter (for
  // such a purpose use Create[Next|Previous]CharacterPosition instead).
  AXPositionInstance CreateAdjacentLeafTextPosition(
      ax::mojom::MoveDirection move_direction) const {
    AXPositionInstance text_position = AsLeafTextPosition();

    switch (move_direction) {
      case ax::mojom::MoveDirection::kNone:
        NOTREACHED_IN_MIGRATION();
        return CreateNullPosition();
      case ax::mojom::MoveDirection::kBackward:
        // If we are at a text offset greater than 0, we will simply decrease
        // the offset by one; otherwise, we will create a position at the end of
        // the previous unignored leaf node with non-empty text and decrease its
        // offset.
        //
        // Note that a position located at offset 0 of an empty text node is
        // considered both at the start and at the end of its anchor, so the
        // following loop skips over empty text leaf nodes, which is expected
        // since those positions are equivalent to both the previous non-empty
        // leaf node's end and the next non-empty leaf node's start.
        while (text_position->AtStartOfAnchor() || text_position->IsIgnored()) {
          text_position = text_position
                              ->CreatePreviousLeafTextPosition(
                                  base::BindRepeating(&AbortMoveAtRootBoundary))
                              ->CreatePositionAtEndOfAnchor();
        }
        if (!text_position->IsNullPosition())
          --text_position->text_offset_;
        break;
      case ax::mojom::MoveDirection::kForward:
        // If we are at a text offset less than MaxTextOffset, we will simply
        // increase the offset by one; otherwise, we will create a position at
        // the start of the next unignored leaf node with non-empty text and
        // increase its offset.
        //
        // Same as the comment above: using AtEndOfAnchor is enough to skip
        // empty text nodes that are equivalent to the initial position.
        while (text_position->AtEndOfAnchor() || text_position->IsIgnored()) {
          text_position = text_position->CreateNextLeafTextPosition(
              base::BindRepeating(&AbortMoveAtRootBoundary));
        }
        if (!text_position->IsNullPosition())
          ++text_position->text_offset_;
        break;
    }

    DCHECK(text_position->IsValid());
    return text_position;
  }

  AXPositionKind kind_ = AXPositionKind::NULL_POSITION;
  // TODO(crbug.com/40864560): use weak pointers for the AXTree, so that
  // AXPosition can be used without AXTreeManager support (and also faster than
  // the slow AXTreeID).
  AXTreeID tree_id_;
  AXNodeID anchor_id_;

  // For text positions, |child_index_| is initially set to |-1| and only
  // computed on demand. The same with tree positions and |text_offset_|.
  int child_index_;
  // "text_offset_" represents the number of UTF16 code units before this
  // position. It doesn't count grapheme clusters.
  int text_offset_;

  // Affinity is used to distinguish between two text positions that point to
  // the same text offset, but which happens to fall on a soft line break. A
  // soft line break doesn't insert any white space in the accessibility tree,
  // so without affinity there would be no way to determine whether a text
  // position is before or after the soft line break. An upstream affinity
  // means that the position is before the soft line break, whilst a
  // downstream affinity means that the position is after the soft line break.
  //
  // Please note that affinity could only be set to upstream for positions
  // that are anchored to non-leaf nodes. When on a leaf node, there could
  // never be an ambiguity as to which line a position points to because Blink
  // creates separate inline text boxes for each line of text. Therefore, a
  // leaf text position before the soft line break would be pointing to the
  // end of its anchor node, whilst a leaf text position after the soft line
  // break would be pointing to the start of the next node.
  ax::mojom::TextAffinity affinity_ = ax::mojom::TextAffinity::kDownstream;

  //
  // Cached members that should be lazily created on first use.
  //

  // In the case of a leaf position, its text content (in UTF16 format). Used
  // for initializing a grapheme break iterator.
  mutable std::u16string name_;
};

template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::BEFORE_TEXT;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_INDEX;
template <class AXPositionType, class AXNodeType>
const int AXPosition<AXPositionType, AXNodeType>::INVALID_OFFSET;

template <class AXPositionType, class AXNodeType>
bool operator==(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() == 0;
}

template <class AXPositionType, class AXNodeType>
bool operator!=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  // It makes sense to also return false if the positions are not comparable,
  // because by definition non-comparable positions are uniqual. Positions are
  // not comparable when one position is null and the other is not or if the
  // positions do not have any common ancestor.
  return !compare_to_optional.has_value() || compare_to_optional.value() != 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() < 0;
}

template <class AXPositionType, class AXNodeType>
bool operator<=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() <= 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>(const AXPosition<AXPositionType, AXNodeType>& first,
               const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() > 0;
}

template <class AXPositionType, class AXNodeType>
bool operator>=(const AXPosition<AXPositionType, AXNodeType>& first,
                const AXPosition<AXPositionType, AXNodeType>& second) {
  const std::optional<int> compare_to_optional = first.CompareTo(second);
  return compare_to_optional.has_value() && compare_to_optional.value() >= 0;
}

template <class AXPositionType, class AXNodeType>
void swap(AXPosition<AXPositionType, AXNodeType>& first,
          AXPosition<AXPositionType, AXNodeType>& second) {
  first.swap(second);
}

template <class AXPositionType, class AXNodeType>
std::ostream& operator<<(
    std::ostream& stream,
    const AXPosition<AXPositionType, AXNodeType>& position) {
  return stream << position.ToString();
}

extern template class EXPORT_TEMPLATE_DECLARE(AX_EXPORT)
    AXPosition<AXNodePosition, AXNode>;

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_POSITION_H_
