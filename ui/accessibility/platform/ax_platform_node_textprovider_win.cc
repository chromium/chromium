// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"

#include <utility>

#include "base/win/scoped_safearray.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"

#define UIA_VALIDATE_TEXTPROVIDER_CALL() \
  if (!owner()->GetDelegate())           \
    return UIA_E_ELEMENTNOTAVAILABLE;
#define UIA_VALIDATE_TEXTPROVIDER_CALL_1_ARG(arg) \
  if (!owner()->GetDelegate())                    \
    return UIA_E_ELEMENTNOTAVAILABLE;             \
  if (!arg)                                       \
    return E_INVALIDARG;

namespace ui {

AXPlatformNodeTextProviderWin::AXPlatformNodeTextProviderWin() {
  DVLOG(1) << __func__;
}

AXPlatformNodeTextProviderWin::~AXPlatformNodeTextProviderWin() {}

// static
AXPlatformNodeTextProviderWin* AXPlatformNodeTextProviderWin::Create(
    AXPlatformNodeWin* owner) {
  CComObject<AXPlatformNodeTextProviderWin>* text_provider = nullptr;
  if (SUCCEEDED(CComObject<AXPlatformNodeTextProviderWin>::CreateInstance(
          &text_provider))) {
    DCHECK(text_provider);
    text_provider->owner_ = owner;
    text_provider->AddRef();
    return text_provider;
  }

  return nullptr;
}

// static
void AXPlatformNodeTextProviderWin::CreateIUnknown(AXPlatformNodeWin* owner,
                                                   IUnknown** unknown) {
  Microsoft::WRL::ComPtr<AXPlatformNodeTextProviderWin> text_provider(
      Create(owner));
  if (text_provider)
    *unknown = text_provider.Detach();
}

//
// ITextProvider methods.
//

STDMETHODIMP AXPlatformNodeTextProviderWin::GetSelection(
    SAFEARRAY** selection) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GETSELECTION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *selection = nullptr;

  AXPlatformNodeDelegate* delegate = owner()->GetDelegate();

  ui::AXTree::Selection unignored_selection = delegate->GetUnignoredSelection();

  AXPlatformNode* anchor_object =
      delegate->GetFromNodeID(unignored_selection.anchor_object_id);
  AXPlatformNode* focus_object =
      delegate->GetFromNodeID(unignored_selection.focus_object_id);

  // If there's no selected object (or the selected object is not in the
  // subtree), return success and don't fill the SAFEARRAY
  //
  // Note that if a selection spans multiple elements, this will report
  // that no selection took place. This is expected for this API, rather
  // than returning the subset of the selection within this node, because
  // subsequently expanding the ITextRange wouldn't  expand to the full
  // selection.
  if (!anchor_object || !focus_object || (anchor_object != focus_object) ||
      (!anchor_object->IsDescendantOf(owner())))
    return S_OK;

  // anchor_offset corresponds to the selection start index
  // and focus_offset is where the selection ends.
  // If they are equal, that indicates a caret on editable text,
  // which should return a degenerate (empty) text range.
  auto start_offset = unignored_selection.anchor_offset;
  auto end_offset = unignored_selection.focus_offset;

  // Reverse start and end if the selection goes backwards
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  AXNodePosition::AXPositionInstance start =
      anchor_object->GetDelegate()->CreateTextPositionAt(start_offset);
  AXNodePosition::AXPositionInstance end =
      anchor_object->GetDelegate()->CreateTextPositionAt(end_offset);

  DCHECK(!start->IsNullPosition());
  DCHECK(!end->IsNullPosition());

  // At this point, if there is no selection, the start and end endpoints will
  // create a degenerate range. According to UIA's documentation, we should only
  // fill the SAFEARRAY with a degenerate range if the degenerate range is on an
  // editable node. Otherwise, the expectations are that the SAFEARRAY is set to
  // nullptr. Here, we are explicitly not allocating an empty SAFEARRAY.
  if (!anchor_object->GetDelegate()->HasVisibleCaretOrSelection())
    return S_OK;

  Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider =
      AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
          owner_.Get(), std::move(start), std::move(end));
  if (&text_range_provider == nullptr)
    return E_OUTOFMEMORY;

  // Since we don't support disjoint text ranges, the SAFEARRAY returned
  // will always have one element
  base::win::ScopedSafearray selections_to_return(
      SafeArrayCreateVector(VT_UNKNOWN /* element type */, 0 /* lower bound */,
                            1 /* number of elements */));

  if (!selections_to_return.Get())
    return E_OUTOFMEMORY;

  LONG index = 0;
  HRESULT hr = SafeArrayPutElement(selections_to_return.Get(), &index,
                                   text_range_provider.Get());
  DCHECK(SUCCEEDED(hr));

  // Since DCHECK only happens in debug builds, return immediately to ensure
  // that we're not leaking the SAFEARRAY on release builds
  if (FAILED(hr))
    return E_FAIL;

  *selection = selections_to_return.Release();

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetVisibleRanges(
    SAFEARRAY** visible_ranges) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GETVISIBLERANGES);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  const AXPlatformNodeDelegate* delegate = owner()->GetDelegate();

  // Get the Clipped Frame Bounds of the current node, not from the root,
  // so if this node is wrapped with overflow styles it will have the
  // correct bounds
  const gfx::Rect frame_rect = delegate->GetBoundsRect(
      AXCoordinateSystem::kFrame, AXClippingBehavior::kClipped);

  const auto start = delegate->CreateTextPositionAt(0);
  const auto end = start->CreatePositionAtEndOfAnchor();
  DCHECK(start->GetAnchor() == end->GetAnchor());

  // SAFEARRAYs are not dynamic, so fill the visible ranges in a vector
  // and then transfer to an appropriately-sized SAFEARRAY
  std::vector<Microsoft::WRL::ComPtr<ITextRangeProvider>> ranges;

  auto current_line_start = start->Clone();
  while (!current_line_start->IsNullPosition() && *current_line_start < *end) {
    auto current_line_end = current_line_start->CreateNextLineEndPosition(
        AXBoundaryBehavior::CrossBoundary);
    if (current_line_end->IsNullPosition() || *current_line_end > *end)
      current_line_end = end->Clone();

    gfx::Rect current_rect = delegate->GetInnerTextRangeBoundsRect(
        current_line_start->text_offset(), current_line_end->text_offset(),
        AXCoordinateSystem::kFrame, AXClippingBehavior::kUnclipped);

    if (frame_rect.Contains(current_rect)) {
      Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider =
          AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
              owner_.Get(), current_line_start->Clone(),
              current_line_end->Clone());

      ranges.emplace_back(text_range_provider);
    }

    current_line_start = current_line_start->CreateNextLineStartPosition(
        AXBoundaryBehavior::CrossBoundary);
  }

  base::win::ScopedSafearray scoped_visible_ranges(
      SafeArrayCreateVector(VT_UNKNOWN /* element type */, 0 /* lower bound */,
                            ranges.size() /* number of elements */));

  if (!scoped_visible_ranges.Get())
    return E_OUTOFMEMORY;

  LONG index = 0;
  for (Microsoft::WRL::ComPtr<ITextRangeProvider>& current_provider : ranges) {
    HRESULT hr = SafeArrayPutElement(scoped_visible_ranges.Get(), &index,
                                     current_provider.Get());
    DCHECK(SUCCEEDED(hr));

    // Since DCHECK only happens in debug builds, return immediately to ensure
    // that we're not leaking the SAFEARRAY on release builds
    if (FAILED(hr))
      return E_FAIL;

    ++index;
  }

  *visible_ranges = scoped_visible_ranges.Release();

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromChild(
    IRawElementProviderSimple* child,
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_RANGEFROMCHILD);
  UIA_VALIDATE_TEXTPROVIDER_CALL_1_ARG(child);

  *range = nullptr;

  Microsoft::WRL::ComPtr<ui::AXPlatformNodeWin> child_platform_node;
  if (!SUCCEEDED(child->QueryInterface(IID_PPV_ARGS(&child_platform_node))))
    return UIA_E_INVALIDOPERATION;

  if (!owner()->IsDescendant(child_platform_node.Get()))
    return E_INVALIDARG;

  *range = GetRangeFromChild(owner(), child_platform_node.Get());

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::RangeFromPoint(
    UiaPoint uia_point,
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_RANGEFROMPOINT);
  UIA_VALIDATE_TEXTPROVIDER_CALL();
  *range = nullptr;

  // Retrieve the closest accessibility node via hit testing the point. No
  // coordinate unit conversion is needed, hit testing input is also in screen
  // coordinates.
  gfx::NativeViewAccessible nearest_native_view_accessible =
      owner()->GetDelegate()->HitTestSync(uia_point.x, uia_point.y);
  DCHECK(nearest_native_view_accessible);

  AXPlatformNodeWin* nearest_node = static_cast<AXPlatformNodeWin*>(
      AXPlatformNode::FromNativeViewAccessible(nearest_native_view_accessible));
  DCHECK(nearest_node);

  AXNodePosition::AXPositionInstance start, end;
  start = nearest_node->GetDelegate()->CreateTextPositionAt(
      nearest_node->NearestTextIndexToPoint(
          gfx::Point(uia_point.x, uia_point.y)));
  DCHECK(!start->IsNullPosition());
  end = start->Clone();

  *range = AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
      nearest_node, std::move(start), std::move(end));
  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::get_DocumentRange(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GET_DOCUMENTRANGE);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  // Start and end should be leaf text positions that span the beginning
  // and end of text content within a node for get_DocumentRange. The start
  // position should be the directly first child and the end position should
  // be the deepest last child node.
  AXNodePosition::AXPositionInstance start =
      owner()->GetDelegate()->CreateTextPositionAt(0)->AsLeafTextPosition();

  AXNodePosition::AXPositionInstance end;
  if (ui::IsDocument(owner()->GetData().role)) {
    // Fast path for getting the range of the web root.
    end = start->CreatePositionAtEndOfDocument();
  } else if (owner()->GetChildCount() == 0) {
    end = owner()
              ->GetDelegate()
              ->CreateTextPositionAt(0)
              ->CreatePositionAtEndOfAnchor()
              ->AsLeafTextPosition();
  } else {
    AXPlatformNode* deepest_last_child =
        AXPlatformNode::FromNativeViewAccessible(
            owner()->ChildAtIndex(owner()->GetChildCount() - 1));

    while (deepest_last_child &&
           deepest_last_child->GetDelegate()->GetChildCount() > 0) {
      deepest_last_child = AXPlatformNode::FromNativeViewAccessible(
          deepest_last_child->GetDelegate()->ChildAtIndex(
              deepest_last_child->GetDelegate()->GetChildCount() - 1));
    }
    end = deepest_last_child->GetDelegate()
              ->CreateTextPositionAt(0)
              ->CreatePositionAtEndOfAnchor()
              ->AsLeafTextPosition();
  }

  *range = AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
      owner_.Get(), std::move(start), std::move(end));

  return S_OK;
}

STDMETHODIMP AXPlatformNodeTextProviderWin::get_SupportedTextSelection(
    enum SupportedTextSelection* text_selection) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXT_GET_SUPPORTEDTEXTSELECTION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *text_selection = SupportedTextSelection_Single;
  return S_OK;
}

//
// ITextEditProvider methods.
//

STDMETHODIMP AXPlatformNodeTextProviderWin::GetActiveComposition(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTEDIT_GETACTIVECOMPOSITION);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *range = nullptr;
  return GetTextRangeProviderFromActiveComposition(range);
}

STDMETHODIMP AXPlatformNodeTextProviderWin::GetConversionTarget(
    ITextRangeProvider** range) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TEXTEDIT_GETCONVERSIONTARGET);
  UIA_VALIDATE_TEXTPROVIDER_CALL();

  *range = nullptr;
  return GetTextRangeProviderFromActiveComposition(range);
}

ITextRangeProvider* AXPlatformNodeTextProviderWin::GetRangeFromChild(
    ui::AXPlatformNodeWin* ancestor,
    ui::AXPlatformNodeWin* descendant) {

  DCHECK(ancestor);
  DCHECK(descendant);
  DCHECK(descendant->GetDelegate());
  DCHECK(ancestor->IsDescendant(descendant));

  // Start and end should be leaf text positions.
  AXNodePosition::AXPositionInstance start =
      descendant->GetDelegate()->CreateTextPositionAt(0)->AsLeafTextPosition();

  AXNodePosition::AXPositionInstance end =
      descendant->GetDelegate()
          ->CreateTextPositionAt(start->MaxTextOffset())
          ->AsLeafTextPosition()
          ->CreatePositionAtEndOfAnchor();

  return AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
      ancestor, std::move(start), std::move(end));
}

ui::AXPlatformNodeWin* AXPlatformNodeTextProviderWin::owner() const {
  return owner_.Get();
}

HRESULT
AXPlatformNodeTextProviderWin::GetTextRangeProviderFromActiveComposition(
    ITextRangeProvider** range) {
  *range = nullptr;
  // We fetch the start and end offset of an active composition only if
  // this object has focus and TSF is in composition mode.
  // The offsets here refer to the character positions in a plain text
  // view of the DOM tree. Ex: if the active composition in an element
  // has "abc" then the range will be (0,3) in both TSF and accessibility
  if ((AXPlatformNode::FromNativeViewAccessible(
           owner()->GetDelegate()->GetFocus()) ==
       static_cast<AXPlatformNode*>(owner())) &&
      owner()->HasActiveComposition()) {
    gfx::Range active_composition_offset =
        owner()->GetActiveCompositionOffsets();
    AXNodePosition::AXPositionInstance start =
        owner()->GetDelegate()->CreateTextPositionAt(
            /*offset*/ active_composition_offset.start());
    AXNodePosition::AXPositionInstance end =
        owner()->GetDelegate()->CreateTextPositionAt(
            /*offset*/ active_composition_offset.end());

    *range = AXPlatformNodeTextRangeProviderWin::CreateTextRangeProvider(
        owner_.Get(), std::move(start), std::move(end));
  }

  return S_OK;
}

}  // namespace ui
