// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_FRAGMENT_ROOT_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_FRAGMENT_ROOT_WIN_H_

#include <wrl/client.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"

namespace ui {

class AXFragmentRootDelegateWin;
class AXFragmentRootPlatformNodeWin;

// UI Automation on Windows requires the root of a multi-element provider to
// implement IRawElementProviderFragmentRoot. Our internal accessibility trees
// may not know their roots for right away; for example, web content may
// deserialize the document for an iframe before the host document. Because of
// this, and because COM rules require that the list of interfaces returned by
// QueryInterface remain static over the lifetime of an object instance, we
// implement IRawElementProviderFragmentRoot on its own node for each HWND, with
// the root of our internal accessibility tree for that HWND as its sole child.
//
// Since UIA derives some information from the underlying HWND hierarchy, we
// expose one fragment root per HWND. The class that owns the HWND is expected
// to own the corresponding AXFragmentRootWin.
class COMPONENT_EXPORT(AX_PLATFORM) AXFragmentRootWin
    : public AXPlatformNodeDelegate {
 public:
  AXFragmentRootWin(gfx::AcceleratedWidget widget,
                    AXFragmentRootDelegateWin* delegate);
  ~AXFragmentRootWin() override;

  // Fragment roots register themselves in a map upon creation and unregister
  // upon destruction. This method provides a lookup, which allows the internal
  // accessibility root to navigate back to the corresponding fragment root.
  static AXFragmentRootWin* GetForAcceleratedWidget(
      gfx::AcceleratedWidget widget);

  // If the given NativeViewAccessible is the direct descendant of a fragment
  // root, return the corresponding fragment root.
  static AXFragmentRootWin* GetFragmentRootParentOf(
      gfx::NativeViewAccessible accessible);

  // Returns the NativeViewAccessible for this fragment root.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // Assistive technologies will typically use UI Automation's control or
  // content view rather than the raw view.
  // Returns true if the fragment root should be included in the control and
  // content views or false if it should be excluded.
  bool IsControlElement();

  // If a child node is available, return its delegate.
  AXPlatformNodeDelegate* GetChildNodeDelegate() const;

 private:
  // AXPlatformNodeDelegate overrides.
  gfx::NativeViewAccessible GetParent() const override;
  size_t GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(size_t index) const override;
  gfx::NativeViewAccessible GetNextSibling() const override;
  gfx::NativeViewAccessible GetPreviousSibling() const override;
  gfx::NativeViewAccessible HitTestSync(int x, int y) const override;
  gfx::NativeViewAccessible GetFocus() const override;
  AXPlatformNodeId GetUniqueId() const override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;
  AXPlatformNode* GetFromTreeIDAndNodeID(const AXTreeID& ax_tree_id,
                                         int32_t id) override;

  // A fragment root does not correspond to any node in the platform neutral
  // accessibility tree. Rather, the fragment root's child is a child of the
  // fragment root's parent. This helper computes the child's index in the
  // parent's array of children.
  size_t GetIndexInParentOfChild() const;

  // If a parent node is available, return its delegate.
  AXPlatformNodeDelegate* GetParentNodeDelegate() const;

  gfx::AcceleratedWidget widget_;
  const raw_ptr<AXFragmentRootDelegateWin> delegate_;
  Microsoft::WRL::ComPtr<AXFragmentRootPlatformNodeWin> platform_node_;
  const AXUniqueId unique_id_{AXUniqueId::Create()};
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_FRAGMENT_ROOT_WIN_H_
