// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTPROVIDER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_TEXTPROVIDER_WIN_H_

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/component_export.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeTextProviderWin
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::ChainInterfaces<ITextEditProvider, ITextProvider>> {
 public:
  explicit AXPlatformNodeTextProviderWin(AXPlatformNodeWin* owner);
  ~AXPlatformNodeTextProviderWin() override;

  static Microsoft::WRL::ComPtr<ITextEditProvider> Create(
      AXPlatformNodeWin* owner);
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
