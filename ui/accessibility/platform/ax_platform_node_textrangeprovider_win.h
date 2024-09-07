// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_

#include <wrl/client.h>

#include <string>
#include <tuple>
#include <vector>

#include "base/component_export.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/sequence_affine_com_object_root_win.h"

namespace ui {
class COMPONENT_EXPORT(AX_PLATFORM) __declspec(uuid(
    "3071e40d-a10d-45ff-a59f-6e8e1138e2c1")) AXPlatformNodeTextRangeProviderWin
    : public SequenceAffineComObjectRoot,
      public ITextRangeProvider {
 public:
  BEGIN_COM_MAP(AXPlatformNodeTextRangeProviderWin)
  COM_INTERFACE_ENTRY(ITextRangeProvider)
  COM_INTERFACE_ENTRY(AXPlatformNodeTextRangeProviderWin)
  END_COM_MAP()

  AXPlatformNodeTextRangeProviderWin();
  ~AXPlatformNodeTextRangeProviderWin();

  // Creates an instance of the class.
  static void CreateTextRangeProvider(AXNodePosition::AXPositionInstance start,
                                      AXNodePosition::AXPositionInstance end,
                                      ITextRangeProvider** text_range_provider);

  // Creates an instance of the class for unit tests, where AXPlatformNodes
  // cannot be queried automatically from endpoints.
  static void CreateTextRangeProviderForTesting(
      AXPlatformNodeWin* owner,
      AXNodePosition::AXPositionInstance start,
      AXNodePosition::AXPositionInstance end,
      ITextRangeProvider** text_range_provider_out);

  //
  // ITextRangeProvider methods.
  //

  IFACEMETHODIMP Clone(ITextRangeProvider** clone) override;
  IFACEMETHODIMP Compare(ITextRangeProvider* other, BOOL* result) override;
  IFACEMETHODIMP
  CompareEndpoints(TextPatternRangeEndpoint this_endpoint,
                   ITextRangeProvider* other,
                   TextPatternRangeEndpoint other_endpoint,
                   int* result) override;
  IFACEMETHODIMP ExpandToEnclosingUnit(TextUnit unit) override;
  IFACEMETHODIMP
  FindAttribute(TEXTATTRIBUTEID attribute_id,
                VARIANT attribute_val,
                BOOL is_backward,
                ITextRangeProvider** result) override;
  IFACEMETHODIMP
  FindText(BSTR string,
           BOOL backwards,
           BOOL ignore_case,
           ITextRangeProvider** result) override;
  IFACEMETHODIMP GetAttributeValue(TEXTATTRIBUTEID attribute_id,
                                   VARIANT* value) override;
  IFACEMETHODIMP
  GetBoundingRectangles(SAFEARRAY** screen_physical_pixel_rectangles) override;
  IFACEMETHODIMP
  GetEnclosingElement(IRawElementProviderSimple** element) override;
  IFACEMETHODIMP GetText(int max_count, BSTR* text) override;
  IFACEMETHODIMP Move(TextUnit unit, int count, int* units_moved) override;
  IFACEMETHODIMP
  MoveEndpointByUnit(TextPatternRangeEndpoint endpoint,
                     TextUnit unit,
                     int count,
                     int* units_moved) override;
  IFACEMETHODIMP
  MoveEndpointByRange(TextPatternRangeEndpoint this_endpoint,
                      ITextRangeProvider* other,
                      TextPatternRangeEndpoint other_endpoint) override;
  IFACEMETHODIMP Select() override;
  IFACEMETHODIMP AddToSelection() override;
  IFACEMETHODIMP RemoveFromSelection() override;
  IFACEMETHODIMP ScrollIntoView(BOOL align_to_top) override;
  IFACEMETHODIMP GetChildren(SAFEARRAY** children) override;

  AXPlatformNodeWin* GetOwner() const;
  void SetOwnerForTesting(AXPlatformNodeWin* owner);

 private:
  using AXPositionInstance = AXNodePosition::AXPositionInstance;
  using AXPositionInstanceType = typename AXPositionInstance::element_type;
  using AXNodeRange = AXRange<AXPositionInstanceType>;

  friend class AXPlatformNodeTextRangeProviderTest;
  friend class AXPlatformNodeTextProviderTest;
  friend class AXRangePhysicalPixelRectDelegate;

  static bool AtStartOfLinePredicate(const AXPositionInstance& position);
  static bool AtEndOfLinePredicate(const AXPositionInstance& position);

  static AXPositionInstance GetNextTextBoundaryPosition(
      const AXPositionInstance& position,
      ax::mojom::TextBoundary boundary_type,
      AXMovementOptions options,
      ax::mojom::MoveDirection boundary_direction);

  // Prefer these *Impl methods when functionality is needed internally. We
  // should avoid calling external APIs internally as it will cause the
  // histograms to become innaccurate.
  HRESULT MoveEndpointByUnitImpl(TextPatternRangeEndpoint endpoint,
                                 TextUnit unit,
                                 int count,
                                 int* units_moved);

  IFACEMETHODIMP ExpandToEnclosingUnitImpl(TextUnit unit);

  std::u16string GetString(
      int max_count,
      std::vector<size_t>* appended_newlines_indices = nullptr);

  static size_t GetAppendedNewLinesCountInRange(
      size_t find_start,
      size_t find_length,
      const std::vector<size_t>& appended_newlines_indices);
  const AXPositionInstance& start() const { return endpoints_.GetStart(); }
  const AXPositionInstance& end() const { return endpoints_.GetEnd(); }
  AXPlatformNodeDelegate* GetDelegate(
      const AXPositionInstanceType* position) const;
  AXPlatformNodeDelegate* GetDelegate(const AXTreeID tree_id,
                                      const AXNodeID node_id) const;

  template <typename AnchorIterator, typename ExpandMatchLambda>
  HRESULT FindAttributeRange(const TEXTATTRIBUTEID text_attribute_id,
                             VARIANT attribute_val,
                             const AnchorIterator first,
                             const AnchorIterator last,
                             ExpandMatchLambda expand_match);

  AXPositionInstance MoveEndpointByCharacter(const AXPositionInstance& endpoint,
                                             const int count,
                                             int* units_moved);
  AXPositionInstance MoveEndpointByWord(const AXPositionInstance& endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByLine(const AXPositionInstance& endpoint,
                                        bool is_start_endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByParagraph(const AXPositionInstance& endpoint,
                                             const bool is_start_endpoint,
                                             const int count,
                                             int* units_moved);
  AXPositionInstance MoveEndpointByPage(const AXPositionInstance& endpoint,
                                        const bool is_start_endpoint,
                                        const int count,
                                        int* units_moved);
  AXPositionInstance MoveEndpointByFormat(const AXPositionInstance& endpoint,
                                          const bool is_start_endpoint,
                                          const int count,
                                          int* units_moved);
  AXPositionInstance MoveEndpointByDocument(const AXPositionInstance& endpoint,
                                            const int count,
                                            int* units_moved);

  AXPositionInstance MoveEndpointByUnitHelper(
      const AXPositionInstance& endpoint,
      const ax::mojom::TextBoundary boundary_type,
      const int count,
      int* units_moved);

  // A text range normalization is necessary to prevent a |start_| endpoint to
  // be positioned at the end of an anchor when it can be at the start of the
  // next anchor. After normalization, it is guaranteed that:
  // * both endpoints passed by parameter are always positioned on unignored
  //   anchors;
  // * both endpoints passed by parameter are never between a grapheme cluster;
  // * if the endpoints passed by parameter create a degenerate range, both
  //   endpoints are on the same anchor.
  // Normalization never updates the internal endpoints directly. Instead, it
  // normalizes the endpoints passed by parameter.
  void NormalizeTextRange(AXPositionInstance& start, AXPositionInstance& end);
  static void NormalizeAsUnignoredPosition(AXPositionInstance& position);
  static void NormalizeAsUnignoredTextRange(AXPositionInstance& start,
                                            AXPositionInstance& end);

  AXPlatformNodeDelegate* GetRootDelegate(const AXTreeID tree_id);
  AXNode* GetSelectionCommonAnchor();
  void RemoveFocusFromPreviousSelectionIfNeeded(
      const AXNodeRange& new_selection);
  AXPlatformNodeWin* GetPlatformNodeFromAXNode(const AXNode* node) const;
  AXPlatformNodeWin* GetLowestAccessibleCommonPlatformNode() const;
  bool HasTextRangeOrSelectionInAtomicTextField(
      const AXPositionInstance& start_position,
      const AXPositionInstance& end_position) const;

  void SetStart(AXPositionInstance start);
  void SetEnd(AXPositionInstance end);

  void SnapStartAndEndToMaxTextOffsetIfBeyond();

  static bool TextAttributeIsArrayType(TEXTATTRIBUTEID attribute_id);
  static bool TextAttributeIsUiaReservedValue(
      const base::win::VariantVector& vector);
  static bool ShouldReleaseTextAttributeAsSafearray(
      TEXTATTRIBUTEID attribute_id,
      const base::win::VariantVector& vector);

  Microsoft::WRL::ComPtr<AXPlatformNodeWin> owner_for_test_;

  // The TextRangeEndpoints class has the responsibility of keeping the
  // endpoints of the range valid or nullify them when it can't find a valid
  // alternative.
  //
  // An endpoint can become invalid when
  //   A. the node it's on gets deleted,
  //   B. when an ancestor node gets deleted, deleting the subtree our endpoint
  //      is on, or
  //   C. when a descendant node gets deleted, potentially rendering the
  //      position invalid due to a smaller MaxTextOffset value (for a text
  //      position) or fewer child nodes (for a tree position).
  //
  // In all cases, our approach to resolve the endpoints to valid positions
  // takes two steps:
  //   1. Move the endpoint to an equivalent ancestor position before the node
  //      gets deleted - we can't move the position once the node it's on is
  //      deleted since this position would already be considered invalid.
  //   2. Call AsValidPosition on that new position once the node is deleted -
  //      calling this function before the node gets deleted wouldn't do much
  //      since our position would still be considered valid at this point.
  //
  // Because AsValidPosition can potentially be expensive, we only want to run
  // it when necessary. For this reason, we store the node ID and tree ID that
  // causes the first step to happen and only run the second step in
  // OnNodeDeleted for the corresponding node deletion. When OnNodeDeleted is
  // called, the |start_| and |end_| endpoints have already been moved up to an
  // ancestor that is still part of the tree. This is to ensure that we don't
  // have to read the node/tree structure of the deleted node in that function -
  // which would likely result in a crash.
  //
  // Both scenarios A and B are fixed by this approach (by the implementation of
  // OnSubtreeWillBeDeleted), but we still have work to do to fix scenario C.
  // This case, in theory, would only require the second step to ensure that the
  // position is always valid but computing whether node is part of the subtree
  // of the endpoint we're on would be very expensive. Furthermore, because the
  // endpoints are generally on leaf nodes, the scenario is unlikely - we
  // haven't heard of issues caused by this scenario yet. Eventually, we might
  // be able to scope the fix to specific use cases, like when the range is on
  // UIA embedded object (e.g. button, select, etc.)
  //
  // ***
  //
  // Why we can't use a ScopedObserver here:
  // We tried using a ScopedObserver instead of a simple observer in this case,
  // but there appears to be a problem with the lifetime of the referenced
  // AXTreeManager in the ScopedObserver. The AXTreeManager can get deleted
  // before the TextRangeEndpoints does, so when the destructor of the
  // ScopedObserver calls ScopedObserver::RemoveAll on an already deleted
  // AXTreeManager, it crashes.
  class COMPONENT_EXPORT(AX_PLATFORM) TextRangeEndpoints
      : public AXTreeObserver {
   public:
    TextRangeEndpoints();
    ~TextRangeEndpoints() override;
    const AXPositionInstance& GetStart();
    const AXPositionInstance& GetEnd();
    void SetStart(AXPositionInstance new_start);
    void SetEnd(AXPositionInstance new_end);

    void AddObserver(const AXTreeID tree_id);
    void RemoveObserver(const AXTreeID tree_id);
    void OnStringAttributeChanged(AXTree* tree,
                                  AXNode* node,
                                  ax::mojom::StringAttribute attr,
                                  const std::string& old_value,
                                  const std::string& new_value) override;
    void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override;
    void OnNodeDeleted(AXTree* tree, AXNodeID node_id) override;
    void OnTreeManagerWillBeRemoved(AXTreeID previous_tree_id) override;

    // This function is in charge of modifying the text offset when it changes
    // by deletion of text. The renderer fires an even notifying that the text
    // offset changed via a deletion, we listen to that here and adjust
    // accordingly. This is needed so that the text offset that the renderer has
    // and the text offset that we have here is synched in case a deletion
    // happens. Otherwise, there are scenarios where an AT may perform an
    // operation, such as a selection, on a range that is no longer in sync with
    // what the renderer has which can lead to wrong behavior.
    void OnTextDeletionOrInsertion(const AXNode& node,
                                   const AXNodeData& new_data) override;

   private:
    struct DeletionOfInterest {
      AXTreeID tree_id;
      AXNodeID node_id;
      // Needed to defer validation from OnNodeDeleted to
      // ValidateEndpointsAfterNodeDeletionIfNeeded.
      bool validation_needed;
    };

    void AdjustEndpointForSubtreeDeletion(AXTree* tree,
                                          const AXNode* const node,
                                          bool is_start_endpoint);
    // TODO(accessibility): Re-evaluate if we want to continue deferring
    // validation after the BrowserAccessibilityManager-specific nodes have been
    // moved to a single unified tree. At this point, deferring will no longer
    // be necessary as there would be no danger in accessing the tree during
    // OnNodeDeleted. However, it may still be preferable to defer the
    // validation to keep work out of unserialize.
    void ValidateEndpointsAfterNodeDeletionIfNeeded();

    void AdjustEndpointForTextFieldEdit(
        const AXNode& text_field_node,
        const AXPositionInstance& current_position,
        AXNode* edit_start_anchor,
        AXNode* edit_end_anchor,
        int edit_start,
        int edit_end,
        bool is_start,
        ax::mojom::Command op);

    AXPositionInstance start_;
    AXPositionInstance end_;

    std::optional<DeletionOfInterest> validation_necessary_for_start_;
    std::optional<DeletionOfInterest> validation_necessary_for_end_;
  };
  // This is marked as mutable since endpoints will lazily validate their
  // positions after a deletion of interest was actually deleted.
  mutable TextRangeEndpoints endpoints_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTRANGEPROVIDER_WIN_H_
