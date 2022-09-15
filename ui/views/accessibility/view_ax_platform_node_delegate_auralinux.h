// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_AURALINUX_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_AURALINUX_H_

#include "base/scoped_observation.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/view_observer.h"

namespace views {

class View;

class ViewAXPlatformNodeDelegateAuraLinux : public ViewAXPlatformNodeDelegate,
                                            public ViewObserver {
 public:
  explicit ViewAXPlatformNodeDelegateAuraLinux(View* view);
  ViewAXPlatformNodeDelegateAuraLinux(
      const ViewAXPlatformNodeDelegateAuraLinux&) = delete;
  ViewAXPlatformNodeDelegateAuraLinux& operator=(
      const ViewAXPlatformNodeDelegateAuraLinux&) = delete;
  ~ViewAXPlatformNodeDelegateAuraLinux() override;

  void Init() override;

  // |ViewAXPlatformNodeDelegate| overrides:
  gfx::NativeViewAccessible GetParent() const override;
  bool IsChildOfLeaf() const override;

 private:
  void OnViewHierarchyChanged(
      View* observed_view,
      const ViewHierarchyChangedDetails& details) override;

  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_AX_PLATFORM_NODE_DELEGATE_AURALINUX_H_
