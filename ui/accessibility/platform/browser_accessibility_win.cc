// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_win.h"

#include "base/memory/ptr_util.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_win.h"
#include "ui/base/win/atl_module.h"

namespace ui {

// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    AXNode* node) {
  return base::WrapUnique(new BrowserAccessibilityWin(manager, node));
}

BrowserAccessibilityWin::BrowserAccessibilityWin(
    BrowserAccessibilityManager* manager,
    AXNode* node)
    : BrowserAccessibility(manager, node) {
  win::CreateATLModuleIfNeeded();
  HRESULT hr = CComObject<BrowserAccessibilityComWin>::CreateInstance(
      &browser_accessibility_com_.AsEphemeralRawAddr());
  DCHECK(SUCCEEDED(hr));
  browser_accessibility_com_->AddRef();
  browser_accessibility_com_->Init(this);
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  if (browser_accessibility_com_) {
    browser_accessibility_com_.ExtractAsDangling()->Destroy();
  }
}

void BrowserAccessibilityWin::UpdatePlatformAttributes() {
  GetCOM()->UpdateStep1ComputeWinAttributes();
  GetCOM()->UpdateStep2ComputeHypertext();
  GetCOM()->UpdateStep3FireEvents();
}

std::wstring BrowserAccessibilityWin::ComputeListItemNameFromContent() const {
  DCHECK_EQ(GetRole(), ax::mojom::Role::kListItem);
  DCHECK(!HasStringAttribute(ax::mojom::StringAttribute::kName));

  std::wstring str;
  int offset = 0;
  if (node()->GetFirstChild() &&
      node()->GetFirstChild()->GetRole() == ax::mojom::Role::kListMarker) {
    offset = 1;
  }
  auto start_position = AXNodePosition::CreatePosition(*node(), offset);
  auto end_position = start_position->CreatePositionAtEndOfAnchor();
  auto range = AXRange(std::move(start_position), std::move(end_position));
  // TODO(accessibility): We're aware that there is an issue with no space being
  // generated between descendants' names in some cases when appending their
  // names. For instance if we have a <li> with a child <ul> which has <li> as
  // children.
  str = base::UTF16ToWide(
      range.GetText(AXTextConcatenationBehavior::kWithoutParagraphBreaks,
                    AXEmbeddedObjectBehavior::kSuppressCharacter));

  return str;
}

bool BrowserAccessibilityWin::CanFireEvents() const {
  // On Windows, we want to hide the subtree of a collapsed <select> element but
  // we still need to fire events on those hidden nodes.
  if (!IsIgnored() && GetCollapsedMenuListSelectAncestor())
    return true;

  // If the node changed its ignored state this frame then some events should be
  // allowed, such as hide/show/structure events. If a node with no siblings
  // changes aria-hidden value, this would affect whether it would be considered
  // a "child of leaf" node which affects BrowserAccessibility::CanFireEvents.
  if (manager()->ToBrowserAccessibilityManagerWin()->IsIgnoredChangedNode(this))
    return true;

  return BrowserAccessibility::CanFireEvents();
}

AXPlatformNode* BrowserAccessibilityWin::GetAXPlatformNode() const {
  return GetCOM();
}

void BrowserAccessibilityWin::OnLocationChanged() {
  GetCOM()->FireNativeEvent(EVENT_OBJECT_LOCATIONCHANGE);
}

std::u16string BrowserAccessibilityWin::GetHypertext() const {
  return GetCOM()->AXPlatformNodeWin::GetHypertext();
}

const std::vector<gfx::NativeViewAccessible>
BrowserAccessibilityWin::GetUIADirectChildrenInRange(
    AXPlatformNodeDelegate* start,
    AXPlatformNodeDelegate* end) {
  std::vector<gfx::NativeViewAccessible> descendants;

  if (!IsIgnored() && !ShouldHideChildrenForUIA() && PlatformChildCount() > 0) {
    BrowserAccessibility* start_wrapper = FromAXPlatformNodeDelegate(start);
    DCHECK(start_wrapper);
    BrowserAccessibility* end_wrapper = FromAXPlatformNodeDelegate(end);
    DCHECK(end_wrapper);

    // When either (or both) of the start/end node is the same as the common
    // anchor, make them null. A null start node means that all UIA embedded
    // objects from the start will be added, and a null end node means that all
    // UIA embedded objects past the start of the range will be included. When
    // both are null, all UIA embedded objects will be included.
    if (this == start_wrapper)
      start_wrapper = nullptr;
    if (this == end_wrapper)
      end_wrapper = nullptr;

    // Don't include nodes that are before the start node - they are not in the
    // range. If the start node is the one we're on right now (ie. the common
    // anchor is the start anchor), include all nodes from the start.
    bool in_range = !start_wrapper;

    for (auto it = PlatformChildrenBegin(); it != PlatformChildrenEnd(); ++it) {
      BrowserAccessibility* child = it.get();
      DCHECK(child);

      if (!in_range &&
          (start_wrapper &&
           (child == start_wrapper || start_wrapper->IsDescendantOf(child)))) {
        in_range = true;
      }

      // The only children that should be returned are the ones that are
      // unignored UIA embedded objects.
      if (in_range && IsUIAEmbeddedObject(child->GetRole())) {
        descendants.emplace_back(child->GetNativeViewAccessible());
      }

      // Don't include the nodes that follow the end of the range.
      if (end_wrapper &&
          (child == end_wrapper || end_wrapper->IsDescendantOf(child))) {
        break;
      }
    }
  }

  return descendants;
}

gfx::NativeViewAccessible BrowserAccessibilityWin::GetNativeViewAccessible() {
  return GetCOM();
}

BrowserAccessibilityComWin* BrowserAccessibilityWin::GetCOM() const {
  DCHECK(browser_accessibility_com_);
  return browser_accessibility_com_;
}

BrowserAccessibilityWin* ToBrowserAccessibilityWin(BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityWin*>(obj);
}

const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj) {
  return static_cast<const BrowserAccessibilityWin*>(obj);
}

TextAttributeList BrowserAccessibilityWin::ComputeTextAttributes() const {
  return GetCOM()->AXPlatformNodeWin::ComputeTextAttributes();
}

bool BrowserAccessibilityWin::ShouldHideChildrenForUIA() const {
  return GetCOM()->AXPlatformNodeWin::ShouldHideChildrenForUIA();
}

}  // namespace ui
