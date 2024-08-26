// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTPROVIDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTPROVIDER_WIN_H_

#include <wrl/client.h>

#include "base/component_export.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/sequence_affine_com_object_root_win.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) __declspec(
    uuid("3e1c192b-4348-45ac-8eb6-4b58eeb3dcca")) AXPlatformNodeTextProviderWin
    : public SequenceAffineComObjectRoot,
      public ITextEditProvider {
 public:
  BEGIN_COM_MAP(AXPlatformNodeTextProviderWin)
  COM_INTERFACE_ENTRY(ITextProvider)
  COM_INTERFACE_ENTRY(ITextEditProvider)
  COM_INTERFACE_ENTRY(AXPlatformNodeTextProviderWin)
  END_COM_MAP()

  AXPlatformNodeTextProviderWin();
  ~AXPlatformNodeTextProviderWin();

  static AXPlatformNodeTextProviderWin* Create(AXPlatformNodeWin* owner);
  static void CreateIUnknown(AXPlatformNodeWin* owner, IUnknown** unknown);

  //
  // ITextProvider methods.
  //

  IFACEMETHODIMP GetSelection(SAFEARRAY** selection) override;

  IFACEMETHODIMP GetVisibleRanges(SAFEARRAY** visible_ranges) override;

  IFACEMETHODIMP RangeFromChild(IRawElementProviderSimple* child,
                                ITextRangeProvider** range) override;

  IFACEMETHODIMP RangeFromPoint(UiaPoint point,
                                ITextRangeProvider** range) override;

  IFACEMETHODIMP get_DocumentRange(ITextRangeProvider** range) override;

  IFACEMETHODIMP get_SupportedTextSelection(
      enum SupportedTextSelection* text_selection) override;

  //
  // ITextEditProvider methods.
  //

  IFACEMETHODIMP GetActiveComposition(ITextRangeProvider** range) override;

  IFACEMETHODIMP GetConversionTarget(ITextRangeProvider** range) override;

  // ITextProvider supporting methods.

  static void GetRangeFromChild(AXPlatformNodeWin* ancestor,
                                AXPlatformNodeWin* descendant,
                                ITextRangeProvider** range);

  // Create a dengerate text range at the start of the specified node.
  static void CreateDegenerateRangeAtStart(AXPlatformNodeWin* node,
                                           ITextRangeProvider** range);

 private:
  friend class AXPlatformNodeTextProviderTest;
  AXPlatformNodeWin* owner() const;
  HRESULT GetTextRangeProviderFromActiveComposition(ITextRangeProvider** range);

  Microsoft::WRL::ComPtr<AXPlatformNodeWin> owner_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTPROVIDER_WIN_H_
