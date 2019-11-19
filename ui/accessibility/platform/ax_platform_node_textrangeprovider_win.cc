// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#include <utility>
#include <vector>

#include "base/i18n/string_search.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

#define UIA_VALIDATE_TEXTRANGEPROVIDER_CALL()              \
  if (!owner() || !owner()->GetDelegate() || !start_ ||    \
      !start_->GetAnchor() || !end_ || !end_->GetAnchor()) \
    return UIA_E_ELEMENTNOTAVAILABLE;
#define UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN(in)       \
  if (!owner() || !owner()->GetDelegate() || !start_ ||    \
      !start_->GetAnchor() || !end_ || !end_->GetAnchor()) \
    return UIA_E_ELEMENTNOTAVAILABLE;                      \
  if (!in)                                                 \
    return E_POINTER;
#define UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(out)     \
  if (!owner() || !owner()->GetDelegate() || !start_ ||    \
      !start_->GetAnchor() || !end_ || !end_->GetAnchor()) \
    return UIA_E_ELEMENTNOTAVAILABLE;                      \
  if (!out)                                                \
    return E_POINTER;                                      \
  *out = {};
#define UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN_1_OUT(in, out) \
  if (!owner() || !owner()->GetDelegate() || !start_ ||         \
      !start_->GetAnchor() || !end_ || !end_->GetAnchor())      \
    return UIA_E_ELEMENTNOTAVAILABLE;                           \
  if (!in || !out)                                              \
    return E_POINTER;                                           \
  *out = {};

// Validate bounds calculated by AXPlatformNodeDelegate. Degenerate bounds
// indicate the interface is not yet supported on the platform.
#define UIA_VALIDATE_BOUNDS(bounds)                           \
  if (bounds.OffsetFromOrigin().IsZero() && bounds.IsEmpty()) \
    return UIA_E_NOTSUPPORTED;

namespace ui {

class AXRangeScreenRectDelegateImpl : public AXRangeScreenRectDelegate {
 public:
  AXRangeScreenRectDelegateImpl(AXPlatformNodeTextRangeProviderWin* host)
      : host_(host) {}

  gfx::Rect GetInnerTextRangeBoundsRect(
      AXTreeID tree_id,
      AXNode::AXID node_id,
      int start_offset,
      int end_offset,
      AXOffscreenResult* offscreen_result) override {
    AXPlatformNodeDelegate* delegate = host_->GetDelegate(tree_id, node_id);
    DCHECK(delegate);
    return delegate->GetInnerTextRangeBoundsRect(
        start_offset, end_offset, ui::AXCoordinateSystem::kScreen,
        ui::AXClippingBehavior::kClipped, offscreen_result);
  }

  gfx::Rect GetBoundsRect(AXTreeID tree_id,
                          AXNode::AXID node_id,
                          AXOffscreenResult* offscreen_result) override {
    AXPlatformNodeDelegate* delegate = host_->GetDelegate(tree_id, node_id);
    DCHECK(delegate);
    return delegate->GetBoundsRect(ui::AXCoordinateSystem::kScreen,
                                   ui::AXClippingBehavior::kClipped,
                                   offscreen_result);
  }

 private:
  AXPlatformNodeTextRangeProviderWin* host_;
};

AXPlatformNodeTextRangeProviderWin::AXPlatformNodeTextRangeProviderWin() {
  DVLOG(1) << __func__;
}

AXPlatformNodeTextRangeProviderWin::~AXPlatformNodeTextRangeProviderWin() {}

ITextRangeProvider* AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
    AXPlatformNodeWin* owner,
    AXPositionInstance start,
    AXPositionInstance end) {
  CComObject<AXPlatformNodeTextRangeProviderWin>* text_range_provider = nullptr;
  if (SUCCEEDED(CComObject<AXPlatformNodeTextRangeProviderWin>::CreateInstance(
          &text_range_provider))) {
    DCHECK(text_range_provider);
    text_range_provider->owner_ = owner;
    text_range_provider->start_ = std::move(start);
    text_range_provider->end_ = std::move(end);
    text_range_provider->AddRef();
    return text_range_provider;
  }

  return nullptr;
}

//
// ITextRangeProvider methods.
//
STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Clone(
    ITextRangeProvider** clone) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_CLONE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(clone);

  *clone =
      CreateTextRangeProvider(owner_.Get(), start_->Clone(), end_->Clone());
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Compare(
    ITextRangeProvider* other,
    BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_COMPARE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN_1_OUT(other, result);

  Microsoft::WRL::ComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(IID_PPV_ARGS(&other_provider)) != S_OK)
    return UIA_E_INVALIDOPERATION;

  if (*start_ == *(other_provider->start_) &&
      *end_ == *(other_provider->end_)) {
    *result = TRUE;
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::CompareEndpoints(
    TextPatternRangeEndpoint this_endpoint,
    ITextRangeProvider* other,
    TextPatternRangeEndpoint other_endpoint,
    int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_COMPAREENDPOINTS);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN_1_OUT(other, result);

  Microsoft::WRL::ComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(IID_PPV_ARGS(&other_provider)) != S_OK)
    return UIA_E_INVALIDOPERATION;

  const AXPositionInstance& this_provider_endpoint =
      (this_endpoint == TextPatternRangeEndpoint_Start) ? start_ : end_;
  const AXPositionInstance& other_provider_endpoint =
      (other_endpoint == TextPatternRangeEndpoint_Start)
          ? other_provider->start_
          : other_provider->end_;

  base::Optional<int> comparison =
      this_provider_endpoint->CompareTo(*other_provider_endpoint);
  if (!comparison)
    return UIA_E_INVALIDOPERATION;

  if (comparison.value() < 0)
    *result = -1;
  else if (comparison.value() > 0)
    *result = 1;
  else
    *result = 0;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ExpandToEnclosingUnit(
    TextUnit unit) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_EXPANDTOENCLOSINGUNIT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  // Determine if start is on a boundary of the specified TextUnit, if it is
  // not, move backwards until it is. Move the end forwards from start until it
  // is on the next TextUnit boundary, if one exists.
  switch (unit) {
    case TextUnit_Character: {
      // For characters, the start endpoint will always be on a TextUnit
      // boundary, thus we only need to move the end position.
      AXPositionInstance end_backup = end_->Clone();
      end_ = start_->CreateNextCharacterPosition(
          AXBoundaryBehavior::CrossBoundary);

      if (end_->IsNullPosition()) {
        // The previous could fail if the start is at the end of the last anchor
        // of the tree, try expanding to the previous character instead.
        AXPositionInstance start_backup = start_->Clone();
        start_ = start_->CreatePreviousCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);

        if (start_->IsNullPosition()) {
          // Text representation is empty, undo everything and exit.
          start_ = std::move(start_backup);
          end_ = std::move(end_backup);
          return S_OK;
        }
        end_ = start_->CreateNextCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);
        DCHECK(!end_->IsNullPosition());
      }

      NormalizeTextRange();
      break;
    }
    case TextUnit_Format:
      start_ = start_->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      end_ = start_->CreateNextFormatEndPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      break;
    case TextUnit_Word:
      start_ = start_->CreatePreviousWordStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      // Since start_ is already located at a word boundary, we need to cross it
      // in order to move to the next one (stopping at the last anchor's end).
      end_ = start_->CreateNextWordStartPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary);
      // Because Windows ATs behave undesirably when the start and end endpoints
      // are not in the same anchor (for character and word navigation), make
      // sure to bring back the end endpoint to the end of the start's anchor.
      if (start_->anchor_id() != end_->anchor_id()) {
        end_ = start_->CreatePositionAtEndOfAnchor();
      }
      break;
    case TextUnit_Line:
      start_ = start_->CreateBoundaryStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary,
          AXTextBoundaryDirection::kBackwards,
          base::BindRepeating(&AtStartOfLinePredicate),
          base::BindRepeating(&AtEndOfLinePredicate));
      end_ = start_->CreateBoundaryEndPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary,
          AXTextBoundaryDirection::kForwards,
          base::BindRepeating(&AtStartOfLinePredicate),
          base::BindRepeating(&AtEndOfLinePredicate));
      break;
    case TextUnit_Paragraph:
      start_ = start_->CreatePreviousParagraphStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      end_ = start_->CreateNextParagraphEndPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
      break;
    case TextUnit_Page: {
      // Per UIA spec, if the document containing the current range doesn't
      // support pagination, default to document navigation.
      const AXNode* common_anchor = start_->LowestCommonAnchor(*end_);
      if (common_anchor->tree()->HasPaginationSupport()) {
        start_ = start_->CreatePreviousPageStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
        end_ = start_->CreateNextPageEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
        break;
      }
    }
      FALLTHROUGH;
    case TextUnit_Document:
      start_ = start_->CreatePositionAtStartOfDocument()->AsLeafTextPosition();
      end_ = start_->CreatePositionAtEndOfDocument();
      break;
    default:
      return UIA_E_NOTSUPPORTED;
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindAttribute(
    TEXTATTRIBUTEID text_attribute_id,
    VARIANT attribute_val,
    BOOL is_backward,
    ITextRangeProvider** result) {
  // Algorithm description:
  // Performs linear search. Expand forward or backward to fetch the first
  // instance of a sub text range that matches the attribute and its value.
  // |is_backward| determines the direction of our search.
  // |is_backward=true|, we search from the end of this text range to its
  // beginning.
  // |is_backward=false|, we search from the beginning of this text range to its
  // end.
  //
  // 1. Iterate through the vector of AXRanges in this text range in the
  //    direction denoted by |is_backward|.
  // 2. The |matched_range| is initially denoted as null since no range
  //    currently matches. We initialize |matched_range| to non-null value when
  //    we encounter the first AXRange instance that matches in attribute and
  //    value. We then set the |matched_range_start| to be the start (anchor) of
  //    the current AXRange, and |matched_range_end| to be the end (focus) of
  //    the current AXRange.
  // 3. If the current AXRange we are iterating on continues to match attribute
  //    and value, we extend |matched_range| in one of the two following ways:
  //    - If |is_backward=true|, we extend the |matched_range| by moving
  //      |matched_range_start| backward. We do so by setting
  //      |matched_range_start| to the start (anchor) of the current AXRange.
  //    - If |is_backward=false|, we extend the |matched_range| by moving
  //      |matched_range_end| forward. We do so by setting |matched_range_end|
  //      to the end (focus) of the current AXRange.
  // 4. We found a match when the current AXRange we are iterating on does not
  //    match the attribute and value and there is a previously matched range.
  //    The previously matched range is the final match we found.
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_FINDATTRIBUTE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(result);
  NormalizeTextRange();

  *result = nullptr;
  AXPositionInstance matched_range_start = nullptr;
  AXPositionInstance matched_range_end = nullptr;

  std::vector<AXNodeRange> anchors;
  AXNodeRange range(start_->Clone(), end_->Clone());
  for (AXNodeRange leaf_text_range : range)
    anchors.emplace_back(std::move(leaf_text_range));

  auto expand_match = [&matched_range_start, &matched_range_end, is_backward](
                          auto& current_start, auto& current_end) {
    // The current AXRange has the attribute and its value that we are looking
    // for, we expand the matched text range if a previously matched exists,
    // otherwise initialize a newly matched text range.
    if (matched_range_start != nullptr && matched_range_end != nullptr) {
      // Continue expanding the matched text range forward/backward based on
      // the search direction.
      if (is_backward)
        matched_range_start = current_start->Clone();
      else
        matched_range_end = current_end->Clone();
    } else {
      // Initialize the matched text range. The first AXRange instance that
      // matches the attribute and its value encountered.
      matched_range_start = current_start->Clone();
      matched_range_end = current_end->Clone();
    }
  };

  HRESULT hr_result =
      is_backward
          ? FindAttributeRange(text_attribute_id, attribute_val,
                               anchors.crbegin(), anchors.crend(), expand_match)
          : FindAttributeRange(text_attribute_id, attribute_val,
                               anchors.cbegin(), anchors.cend(), expand_match);
  if (FAILED(hr_result))
    return E_FAIL;

  if (matched_range_start != nullptr && matched_range_end != nullptr)
    *result = CreateTextRangeProvider(owner(), std::move(matched_range_start),
                                      std::move(matched_range_end));
  return S_OK;
}

template <typename AnchorIterator, typename ExpandMatchLambda>
HRESULT AXPlatformNodeTextRangeProviderWin::FindAttributeRange(
    const TEXTATTRIBUTEID text_attribute_id,
    VARIANT attribute_val,
    const AnchorIterator first,
    const AnchorIterator last,
    ExpandMatchLambda expand_match) {
  AXPlatformNodeWin* current_platform_node;
  bool is_match_found = false;

  for (auto it = first; it != last; ++it) {
    const auto& current_start = it->anchor();
    const auto& current_end = it->focus();

    DCHECK(current_start->GetAnchor() == current_end->GetAnchor());

    AXPlatformNodeDelegate* delegate = GetDelegate(current_start);
    DCHECK(delegate);

    current_platform_node = static_cast<AXPlatformNodeWin*>(
        delegate->GetFromNodeID(current_start->GetAnchor()->id()));

    base::win::ScopedVariant current_attribute_value;
    if (FAILED(current_platform_node->GetTextAttributeValue(
            text_attribute_id, current_attribute_value.Receive())))
      return E_FAIL;

    if (VARCMP_EQ == VarCmp(&attribute_val, current_attribute_value.AsInput(),
                            LOCALE_USER_DEFAULT, 0)) {
      // When we encounter an AXRange instance that matches the attribute
      // and its value which we are looking for and no previously matched text
      // range exists, we expand or initialize the matched range.
      is_match_found = true;
      expand_match(current_start, current_end);
    } else if (is_match_found) {
      // When we encounter an AXRange instance that does not match the attribute
      // and its value which we are looking for and a previously matched text
      // range exists, the previously matched text range is the result we found.
      break;
    }
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::FindText(
    BSTR string,
    BOOL backwards,
    BOOL ignore_case,
    ITextRangeProvider** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_FINDTEXT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN_1_OUT(string, result);

  base::string16 search_string(string);
  if (search_string.length() <= 0)
    return E_INVALIDARG;

  size_t appended_newlines_count = 0;
  base::string16 text_range = GetString(-1, &appended_newlines_count);
  size_t find_start;
  size_t find_length;
  if (base::i18n::StringSearch(search_string, text_range, &find_start,
                               &find_length, !ignore_case, !backwards) &&
      find_length > appended_newlines_count) {
    // TODO(https://crbug.com/1023599): There is a known issue here related to
    // text searches of a |string| starting and ending with a "\n", e.g.
    // "\nsometext" or "sometext\n" if the newline is computed from a line
    // breaking object. FindText() is rarely called, and when it is, it's not to
    // look for a string starting or ending with a newline. This may change
    // someday, and if so, we'll have to address this issue.
    const AXNode* common_anchor = start_->LowestCommonAnchor(*end_);
    AXPositionInstance start_ancestor_position =
        start_->CreateAncestorPosition(common_anchor);
    DCHECK(!start_ancestor_position->IsNullPosition());
    AXPositionInstance end_ancestor_position =
        end_->CreateAncestorPosition(common_anchor);
    DCHECK(!end_ancestor_position->IsNullPosition());
    AXTreeID tree_id = start_ancestor_position->tree_id();
    AXNode::AXID anchor_id = start_ancestor_position->anchor_id();
    const int start_offset =
        start_ancestor_position->text_offset() + find_start;
    const int end_offset = start_offset + find_length - appended_newlines_count;
    const int max_end_offset = end_ancestor_position->text_offset();
    DCHECK(start_offset <= end_offset && end_offset <= max_end_offset);

    AXPositionInstance start = ui::AXNodePosition::CreateTextPosition(
                                   tree_id, anchor_id, start_offset,
                                   ax::mojom::TextAffinity::kDownstream)
                                   ->AsLeafTextPosition();
    AXPositionInstance end = ui::AXNodePosition::CreateTextPosition(
                                 tree_id, anchor_id, end_offset,
                                 ax::mojom::TextAffinity::kDownstream)
                                 ->AsLeafTextPosition();

    *result =
        CreateTextRangeProvider(owner_.Get(), start->Clone(), end->Clone());
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetAttributeValue(
    TEXTATTRIBUTEID attribute_id,
    VARIANT* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETATTRIBUTEVALUE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(value);
  NormalizeTextRange();

  base::win::ScopedVariant attribute_value_variant;

  // The range is inclusive, so advance our endpoint to the next position
  auto end = end_->AsLeafTextPosition()->CreateNextAnchorPosition();

  // Iterate over anchor positions
  for (auto it = start_->AsLeafTextPosition();
       it->anchor_id() != end->anchor_id() || it->tree_id() != end->tree_id();
       it = it->CreateNextAnchorPosition()) {
    // If the iterator creates a null position, then it has likely overrun the
    // range, return failure. This is unexpected but may happen if the range
    // became inverted.
    DCHECK(!it->IsNullPosition());
    if (it->IsNullPosition())
      return E_FAIL;

    AXPlatformNodeDelegate* delegate = GetDelegate(it.get());
    DCHECK(it && delegate);

    AXPlatformNodeWin* platform_node = static_cast<AXPlatformNodeWin*>(
        delegate->GetFromNodeID(it->anchor_id()));
    DCHECK(platform_node);

    base::win::ScopedVariant current_variant;
    HRESULT hr = platform_node->GetTextAttributeValue(
        attribute_id, current_variant.Receive());
    if (FAILED(hr))
      return E_FAIL;

    if (attribute_value_variant.type() == VT_EMPTY) {
      attribute_value_variant.Reset(current_variant);
      if (attribute_value_variant.type() == VT_UNKNOWN) {
        *value = attribute_value_variant.Release();
        return S_OK;
      }
    } else if (attribute_value_variant.Compare(current_variant)) {
      V_VT(value) = VT_UNKNOWN;
      return ::UiaGetReservedMixedAttributeValue(&V_UNKNOWN(value));
    }
  }

  *value = attribute_value_variant.Release();
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetBoundingRectangles(
    SAFEARRAY** rectangles) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETBOUNDINGRECTANGLES);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(rectangles);

  *rectangles = nullptr;
  AXNodeRange range(start_->Clone(), end_->Clone());
  AXRangeScreenRectDelegateImpl rect_delegate(this);
  std::vector<gfx::Rect> rects = range.GetScreenRects(&rect_delegate);

  // 4 array items per rect: left, top, width, height
  SAFEARRAY* safe_array = SafeArrayCreateVector(
      VT_R8 /* element type */, 0 /* lower bound */, rects.size() * 4);

  if (!safe_array)
    return E_OUTOFMEMORY;

  if (rects.size() > 0) {
    double* double_array = nullptr;
    HRESULT hr = SafeArrayAccessData(safe_array,
                                     reinterpret_cast<void**>(&double_array));

    if (SUCCEEDED(hr)) {
      for (size_t rect_index = 0; rect_index < rects.size(); rect_index++) {
        const gfx::Rect& rect = rects[rect_index];
        double_array[rect_index * 4] = rect.x();
        double_array[rect_index * 4 + 1] = rect.y();
        double_array[rect_index * 4 + 2] = rect.width();
        double_array[rect_index * 4 + 3] = rect.height();
      }
      hr = SafeArrayUnaccessData(safe_array);
    }

    if (FAILED(hr)) {
      DCHECK(safe_array);
      SafeArrayDestroy(safe_array);
      return E_FAIL;
    }
  }

  *rectangles = safe_array;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetEnclosingElement(
    IRawElementProviderSimple** element) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETENCLOSINGELEMENT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(element);

  AXNode* common_anchor = start_->LowestCommonAnchor(*end_);
  DCHECK(common_anchor);
  if (!common_anchor)
    return UIA_E_ELEMENTNOTAVAILABLE;

  const AXTreeID tree_id = common_anchor->tree()->GetAXTreeID();
  const AXNode::AXID node_id = common_anchor->id();
  AXPlatformNodeWin* enclosing_node =
      static_cast<AXPlatformNodeWin*>(AXPlatformNode::FromNativeViewAccessible(
          GetDelegate(tree_id, node_id)->GetNativeViewAccessible()));
  DCHECK(enclosing_node);
  // If this node has an ancestor that is a control type, use that as the
  // enclosing element.
  enclosing_node = enclosing_node->GetLowestAccessibleElement();
  DCHECK(enclosing_node);

  while (ui::IsIgnored(enclosing_node->GetData())) {
    AXPlatformNodeWin* parent = static_cast<AXPlatformNodeWin*>(
        AXPlatformNode::FromNativeViewAccessible(enclosing_node->GetParent()));
    DCHECK(parent);
    enclosing_node = parent;
  }

  enclosing_node->GetNativeViewAccessible()->QueryInterface(
      IID_PPV_ARGS(element));

  DCHECK(*element);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetText(int max_count,
                                                         BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETTEXT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(text);

  // -1 is a valid value that signifies that the caller wants complete text.
  // Any other negative value is an invalid argument.
  if (max_count < -1)
    return E_INVALIDARG;

  base::string16 full_text = GetString(max_count);
  if (!full_text.empty()) {
    size_t length = full_text.length();

    if (max_count != -1 && max_count < static_cast<int>(length))
      *text = SysAllocStringLen(full_text.c_str(), max_count);
    else
      *text = SysAllocStringLen(full_text.c_str(), length);
  } else {
    *text = SysAllocString(L"");
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Move(TextUnit unit,
                                                      int count,
                                                      int* units_moved) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(units_moved);

  // Per MSDN, move with zero count has no effect.
  if (count == 0)
    return S_OK;

  // Save a clone of start and end, in case one of the moves fails.
  auto start_backup = start_->Clone();
  auto end_backup = end_->Clone();
  bool is_degenerate_range = (*start_ == *end_);

  // Move the start of the text range forward or backward in the document by the
  // requested number of text unit boundaries.
  int start_units_moved = 0;
  HRESULT hr = MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, count,
                                  &start_units_moved);

  bool succeeded_move = SUCCEEDED(hr) && start_units_moved != 0;
  if (succeeded_move) {
    end_ = start_->Clone();
    if (!is_degenerate_range) {
      bool forwards = count > 0;
      if (forwards && start_->AtEndOfDocument()) {
        // The start is at the end of the document, so move the start backward
        // by one text unit to expand the text range from the degenerate range
        // state.
        int current_start_units_moved = 0;
        hr = MoveEndpointByUnit(TextPatternRangeEndpoint_Start, unit, -1,
                                &current_start_units_moved);
        start_units_moved -= 1;
        succeeded_move = SUCCEEDED(hr) && current_start_units_moved == -1 &&
                         start_units_moved > 0;
      } else {
        // The start is not at the end of the document, so move the endpoint
        // forward by one text unit to expand the text range from the degenerate
        // state.
        int end_units_moved = 0;
        hr = MoveEndpointByUnit(TextPatternRangeEndpoint_End, unit, 1,
                                &end_units_moved);
        succeeded_move = SUCCEEDED(hr) && end_units_moved == 1;
      }

      // Because Windows ATs behave undesirably when the start and end endpoints
      // are not in the same anchor (for character and word navigation), make
      // sure to bring back the end endpoint to the end of the start's anchor.
      if (start_->anchor_id() != end_->anchor_id() &&
          (unit == TextUnit_Character || unit == TextUnit_Word)) {
        ExpandToEnclosingUnit(unit);
      }
    }
  }

  if (!succeeded_move) {
    start_ = std::move(start_backup);
    end_ = std::move(end_backup);
    start_units_moved = 0;
    if (!SUCCEEDED(hr))
      return hr;
  }

  *units_moved = start_units_moved;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByUnit(
    TextPatternRangeEndpoint endpoint,
    TextUnit unit,
    int count,
    int* units_moved) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVEENDPOINTBYUNIT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(units_moved);

  // Per MSDN, MoveEndpointByUnit with zero count has no effect.
  if (count == 0) {
    *units_moved = 0;
    return S_OK;
  }

  bool is_start_endpoint = endpoint == TextPatternRangeEndpoint_Start;
  AXPositionInstance& position_to_move = is_start_endpoint ? start_ : end_;
  AXPositionInstance new_position;

  switch (unit) {
    case TextUnit_Character:
      new_position =
          MoveEndpointByCharacter(position_to_move, count, units_moved);
      break;
    case TextUnit_Format:
      new_position = MoveEndpointByFormat(position_to_move, count, units_moved);
      break;
    case TextUnit_Word:
      new_position = MoveEndpointByWord(position_to_move, count, units_moved);
      break;
    case TextUnit_Line:
      new_position = MoveEndpointByLine(position_to_move, is_start_endpoint,
                                        count, units_moved);
      break;
    case TextUnit_Paragraph:
      new_position = MoveEndpointByParagraph(
          position_to_move, is_start_endpoint, count, units_moved);
      break;
    case TextUnit_Page:
      new_position = MoveEndpointByPage(position_to_move, is_start_endpoint,
                                        count, units_moved);
      break;
    case TextUnit_Document:
      new_position =
          MoveEndpointByDocument(position_to_move, count, units_moved);
      break;
    default:
      return UIA_E_NOTSUPPORTED;
  }
  position_to_move = std::move(new_position);

  // If the start was moved past the end, create a degenerate range with the end
  // equal to the start. Do the equivalent if the end moved past the start.
  if (*end_->AsTreePosition() < *start_->AsTreePosition() || *end_ < *start_) {
    if (is_start_endpoint)
      end_ = start_->Clone();
    else
      start_ = end_->Clone();
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::MoveEndpointByRange(
    TextPatternRangeEndpoint this_endpoint,
    ITextRangeProvider* other,
    TextPatternRangeEndpoint other_endpoint) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_MOVEENPOINTBYRANGE);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_IN(other);

  Microsoft::WRL::ComPtr<AXPlatformNodeTextRangeProviderWin> other_provider;
  if (other->QueryInterface(IID_PPV_ARGS(&other_provider)) != S_OK)
    return UIA_E_INVALIDOPERATION;

  const AXPositionInstance& other_provider_endpoint =
      (other_endpoint == TextPatternRangeEndpoint_Start)
          ? other_provider->start_
          : other_provider->end_;

  if (this_endpoint == TextPatternRangeEndpoint_Start) {
    start_ = other_provider_endpoint->Clone();
    if (*start_ > *end_)
      end_ = start_->Clone();
  } else {
    end_ = other_provider_endpoint->Clone();
    if (*start_ > *end_)
      start_ = end_->Clone();
  }
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::Select() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_SELECT);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  AXNodeRange range(start_->Clone(), end_->Clone());
  AXActionData action_data;
  action_data.anchor_node_id = range.anchor()->anchor_id();
  action_data.anchor_offset = range.anchor()->text_offset();
  action_data.focus_node_id = range.focus()->anchor_id();
  action_data.focus_offset = range.focus()->text_offset();
  action_data.action = ax::mojom::Action::kSetSelection;
  owner()->GetDelegate()->AccessibilityPerformAction(action_data);
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::AddToSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_ADDTOSELECTION);
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP
AXPlatformNodeTextRangeProviderWin::RemoveFromSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_REMOVEFROMSELECTION);
  return UIA_E_INVALIDOPERATION;  // not supporting disjoint text selections
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::ScrollIntoView(
    BOOL align_to_top) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_SCROLLINTOVIEW);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL();

  const AXPositionInstance start_common_ancestor =
      start_->LowestCommonAncestor(*end_);
  const AXPositionInstance end_common_ancestor =
      end_->LowestCommonAncestor(*start_);
  if (start_common_ancestor->IsNullPosition() ||
      end_common_ancestor->IsNullPosition())
    return E_INVALIDARG;

  const AXNode* common_ancestor_anchor = start_common_ancestor->GetAnchor();
  DCHECK(common_ancestor_anchor == end_common_ancestor->GetAnchor());

  const AXTreeID common_ancestor_tree_id = start_common_ancestor->tree_id();
  const AXTreeManager* ax_tree_manager =
      AXTreeManagerMap::GetInstance().GetManager(common_ancestor_tree_id);
  DCHECK(ax_tree_manager);
  AXNode* root_node = ax_tree_manager->GetRootAsAXNode();
  const AXPlatformNode* root_platform_node =
      owner_->GetDelegate()->GetFromTreeIDAndNodeID(common_ancestor_tree_id,
                                                    root_node->id());
  DCHECK(root_platform_node);
  const AXPlatformNodeDelegate* root_delegate =
      root_platform_node->GetDelegate();
  const gfx::Rect root_frame_bounds = root_delegate->GetBoundsRect(
      AXCoordinateSystem::kFrame, AXClippingBehavior::kUnclipped);
  UIA_VALIDATE_BOUNDS(root_frame_bounds);

  const AXPlatformNode* common_ancestor_platform_node =
      owner_->GetDelegate()->GetFromTreeIDAndNodeID(
          common_ancestor_tree_id, common_ancestor_anchor->id());
  DCHECK(common_ancestor_platform_node);
  AXPlatformNodeDelegate* common_ancestor_delegate =
      common_ancestor_platform_node->GetDelegate();
  DCHECK(common_ancestor_delegate);
  const gfx::Rect text_range_container_frame_bounds =
      common_ancestor_delegate->GetBoundsRect(AXCoordinateSystem::kFrame,
                                              AXClippingBehavior::kUnclipped);
  UIA_VALIDATE_BOUNDS(text_range_container_frame_bounds);

  gfx::Point target_point;
  if (align_to_top) {
    target_point = gfx::Point(root_frame_bounds.x(), root_frame_bounds.y());
  } else {
    target_point =
        gfx::Point(root_frame_bounds.x(),
                   root_frame_bounds.y() + root_frame_bounds.height());
  }

  if ((align_to_top && start_->GetAnchor()->IsText()) ||
      (!align_to_top && end_->GetAnchor()->IsText())) {
    const gfx::Rect text_range_frame_bounds =
        common_ancestor_delegate->GetInnerTextRangeBoundsRect(
            start_common_ancestor->text_offset(),
            end_common_ancestor->text_offset(), AXCoordinateSystem::kFrame,
            AXClippingBehavior::kUnclipped);
    UIA_VALIDATE_BOUNDS(text_range_frame_bounds);

    if (align_to_top) {
      target_point.Offset(0, -(text_range_container_frame_bounds.height() -
                               text_range_frame_bounds.height()));
    } else {
      target_point.Offset(0, -text_range_frame_bounds.height());
    }
  } else {
    if (!align_to_top)
      target_point.Offset(0, -text_range_container_frame_bounds.height());
  }

  const gfx::Rect root_screen_bounds = root_delegate->GetBoundsRect(
      AXCoordinateSystem::kScreen, AXClippingBehavior::kUnclipped);
  UIA_VALIDATE_BOUNDS(root_screen_bounds);
  target_point += root_screen_bounds.OffsetFromOrigin();

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_node_id = common_ancestor_anchor->id();
  action_data.target_point = target_point;
  if (!common_ancestor_delegate->AccessibilityPerformAction(action_data))
    return E_FAIL;
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextRangeProviderWin::GetChildren(
    SAFEARRAY** children) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTRANGE_GETCHILDREN);
  UIA_VALIDATE_TEXTRANGEPROVIDER_CALL_1_OUT(children);

  std::vector<gfx::NativeViewAccessible> descendants;

  const AXNode* common_anchor = start_->LowestCommonAnchor(*end_);
  const AXTreeID tree_id = common_anchor->tree()->GetAXTreeID();
  const AXNode::AXID node_id = common_anchor->id();
  AXPlatformNodeDelegate* delegate = GetDelegate(tree_id, node_id);
  DCHECK(delegate);
  while (ui::IsIgnored(delegate->GetData())) {
    auto* node = static_cast<AXPlatformNodeWin*>(
        AXPlatformNode::FromNativeViewAccessible(delegate->GetParent()));
    DCHECK(node);
    delegate = node->GetDelegate();
  }
  if (delegate->GetChildCount())
    descendants = delegate->GetDescendants();

  SAFEARRAY* safe_array =
      SafeArrayCreateVector(VT_UNKNOWN, 0, descendants.size());

  if (!safe_array)
    return E_OUTOFMEMORY;

  if (safe_array->rgsabound->cElements != descendants.size()) {
    DCHECK(safe_array);
    SafeArrayDestroy(safe_array);
    return E_OUTOFMEMORY;
  }

  LONG i = 0;
  for (const gfx::NativeViewAccessible& descendant : descendants) {
    IRawElementProviderSimple* raw_provider;
    descendant->QueryInterface(IID_PPV_ARGS(&raw_provider));
    SafeArrayPutElement(safe_array, &i, raw_provider);
    ++i;
  }

  *children = safe_array;
  return S_OK;
}

// static
bool AXPlatformNodeTextRangeProviderWin::AtStartOfLinePredicate(
    const AXPositionInstance& position) {
  return !position->IsIgnored() &&
         (position->AtStartOfLine() || position->AtStartOfInlineBlock());
}

// static
bool AXPlatformNodeTextRangeProviderWin::AtEndOfLinePredicate(
    const AXPositionInstance& position) {
  return !position->IsIgnored() &&
         (position->AtEndOfLine() || position->AtStartOfInlineBlock());
}

base::string16 AXPlatformNodeTextRangeProviderWin::GetString(
    int max_count,
    size_t* appended_newlines_count) {
  AXNodeRange range(start_->Clone(), end_->Clone());
  return range.GetText(AXTextConcatenationBehavior::kAsInnerText, max_count,
                       false, appended_newlines_count);
}

AXPlatformNodeWin* AXPlatformNodeTextRangeProviderWin::owner() const {
  return owner_.Get();
}

AXPlatformNodeDelegate* AXPlatformNodeTextRangeProviderWin::GetDelegate(
    const AXPositionInstanceType* position) const {
  return GetDelegate(position->tree_id(), position->anchor_id());
}

AXPlatformNodeDelegate* AXPlatformNodeTextRangeProviderWin::GetDelegate(
    const AXTreeID tree_id,
    const AXNode::AXID node_id) const {
  AXPlatformNode* platform_node =
      owner_->GetDelegate()->GetFromTreeIDAndNodeID(tree_id, node_id);
  if (!platform_node)
    return nullptr;

  return platform_node->GetDelegate();
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByCharacter(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  return MoveEndpointByUnitHelper(
      std::move(endpoint), AXTextBoundary::kCharacter, count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByWord(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  return MoveEndpointByUnitHelper(
      std::move(endpoint), AXTextBoundary::kWordStart, count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByLine(
    const AXPositionInstance& endpoint,
    bool is_start_endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);
  const bool going_forward = count > 0;

  AXPositionInstance current_endpoint = endpoint->Clone();
  for (int iteration = 0; iteration < std::abs(count); ++iteration) {
    AXPositionInstance next_endpoint;
    if (is_start_endpoint) {
      next_endpoint = current_endpoint->CreateBoundaryStartPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary,
          going_forward ? AXTextBoundaryDirection::kForwards
                        : AXTextBoundaryDirection::kBackwards,
          base::BindRepeating(&AtStartOfLinePredicate),
          base::BindRepeating(&AtEndOfLinePredicate));
    } else {
      next_endpoint = current_endpoint->CreateBoundaryEndPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary,
          going_forward ? AXTextBoundaryDirection::kForwards
                        : AXTextBoundaryDirection::kBackwards,
          base::BindRepeating(&AtStartOfLinePredicate),
          base::BindRepeating(&AtEndOfLinePredicate));
    }
    DCHECK(!next_endpoint->IsNullPosition());

    // Since AXBoundaryBehavior::StopAtLastAnchorBoundary forces the next text
    // boundary position to be different than the input position, the only case
    // where these are equal is when they're already located at the last anchor
    // boundary. In such case, there is no next position to move to.
    if (*current_endpoint == *next_endpoint) {
      *units_moved = going_forward ? iteration : -iteration;
      return current_endpoint;
    }
    current_endpoint = std::move(next_endpoint);
  }

  *units_moved = count;
  return current_endpoint;
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByFormat(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  return MoveEndpointByUnitHelper(
      std::move(endpoint), AXTextBoundary::kFormatChange, count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByParagraph(
    const AXPositionInstance& endpoint,
    const bool is_start_endpoint,
    const int count,
    int* units_moved) {
  return MoveEndpointByUnitHelper(std::move(endpoint),
                                  is_start_endpoint
                                      ? AXTextBoundary::kParagraphStart
                                      : AXTextBoundary::kParagraphEnd,
                                  count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByPage(
    const AXPositionInstance& endpoint,
    const bool is_start_endpoint,
    const int count,
    int* units_moved) {
  // Per UIA spec, if the document containing the current endpoint doesn't
  // support pagination, default to document navigation.
  AXPositionInstance common_ancestor = start_->LowestCommonAncestor(*end_);
  if (!common_ancestor->GetAnchor()->tree()->HasPaginationSupport())
    return MoveEndpointByDocument(std::move(endpoint), count, units_moved);

  return MoveEndpointByUnitHelper(
      std::move(endpoint),
      is_start_endpoint ? AXTextBoundary::kPageStart : AXTextBoundary::kPageEnd,
      count, units_moved);
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByDocument(
    const AXPositionInstance& endpoint,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);

  if (count < 0) {
    *units_moved = !endpoint->AtStartOfDocument() ? -1 : 0;
    return endpoint->CreatePositionAtStartOfDocument();
  }
  *units_moved = !endpoint->AtEndOfDocument() ? 1 : 0;
  return endpoint->CreatePositionAtEndOfDocument();
}

AXPlatformNodeTextRangeProviderWin::AXPositionInstance
AXPlatformNodeTextRangeProviderWin::MoveEndpointByUnitHelper(
    const AXPositionInstance& endpoint,
    const AXTextBoundary boundary_type,
    const int count,
    int* units_moved) {
  DCHECK_NE(count, 0);
  const bool going_forward = count > 0;

  AXPositionInstance current_endpoint = endpoint->Clone();
  for (int iteration = 0; iteration < std::abs(count); ++iteration) {
    AXPositionInstance next_endpoint =
        current_endpoint->CreatePositionAtTextBoundary(
            boundary_type,
            going_forward ? AXTextBoundaryDirection::kForwards
                          : AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::StopAtLastAnchorBoundary);
    DCHECK(!next_endpoint->IsNullPosition());

    // Since AXBoundaryBehavior::StopAtLastAnchorBoundary forces the next text
    // boundary position to be different than the input position, the only case
    // where these are equal is when they're already located at the last anchor
    // boundary. In such case, there is no next position to move to.
    if (*current_endpoint == *next_endpoint) {
      *units_moved = going_forward ? iteration : -iteration;
      return current_endpoint;
    }
    current_endpoint = std::move(next_endpoint);
  }

  *units_moved = count;
  return current_endpoint;
}

void AXPlatformNodeTextRangeProviderWin::NormalizeAsUnignoredTextRange() {
  if (!start_->IsValid() || !end_->IsValid())
    return;

  if (!start_->IsIgnored() && !end_->IsIgnored())
    return;

  if (start_->IsIgnored()) {
    AXPositionInstance normalized_start = start_->AsUnignoredTextPosition(
        AXNodePosition::AdjustmentBehavior::kMoveRight);
    if (normalized_start->IsNullPosition()) {
      normalized_start = start_->AsUnignoredTextPosition(
          AXNodePosition::AdjustmentBehavior::kMoveLeft);
    }
    if (!normalized_start->IsNullPosition())
      start_ = std::move(normalized_start);
  }

  if (end_->IsIgnored()) {
    AXPositionInstance normalized_end = end_->AsUnignoredTextPosition(
        AXNodePosition::AdjustmentBehavior::kMoveRight);
    if (normalized_end->IsNullPosition()) {
      normalized_end = end_->AsUnignoredTextPosition(
          AXNodePosition::AdjustmentBehavior::kMoveLeft);
    }
    if (!normalized_end->IsNullPosition())
      end_ = std::move(normalized_end);
  }

  DCHECK_LE(*start_, *end_);
}

void AXPlatformNodeTextRangeProviderWin::NormalizeTextRange() {
  if (!start_->IsValid() || !end_->IsValid())
    return;

  // If either endpoint is anchored to an ignored node,
  // first snap them both to be unignored positions.
  NormalizeAsUnignoredTextRange();

  // Only normalize non-degenerate ranges.
  if (*start_ != *end_) {
    AXPositionInstance normalized_start =
        start_->AsLeafTextPositionBeforeCharacter();
    if (!normalized_start->IsNullPosition()) {
      DCHECK_EQ(*start_, *normalized_start);
      start_ = std::move(normalized_start);
    }

    AXPositionInstance normalized_end =
        end_->AsLeafTextPositionAfterCharacter();
    if (!normalized_end->IsNullPosition()) {
      DCHECK_EQ(*end_, *normalized_end);
      end_ = std::move(normalized_end);
    }

    DCHECK_LE(*start_, *end_);
  }
}

}  // namespace ui
