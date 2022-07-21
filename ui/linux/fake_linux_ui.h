// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_FAKE_LINUX_UI_H_
#define UI_LINUX_FAKE_LINUX_UI_H_

#include "ui/linux/linux_ui_base.h"

namespace ui {

// This class is meant to be overridden by tests.  It's provided as a
// convenience so that tests don't have to stub lots of methods just to override
// a single one.
class FakeLinuxUi : public LinuxUiBase {
 public:
  FakeLinuxUi();
  ~FakeLinuxUi() override;

  std::unique_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate) const override;
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      int* weight_out,
      gfx::FontRenderParams* params_out) const override;
  ui::SelectFileDialog* CreateSelectFileDialog(
      void* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) const override;
  bool Initialize() override;
  bool GetColor(int id, SkColor* color, bool use_custom_frame) const override;
  bool GetDisplayProperty(int id, int* result) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  base::TimeDelta GetCursorBlinkInterval() const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size,
                                   float scale) const override;
  WindowFrameAction GetWindowFrameAction(
      WindowFrameActionSource source) override;
  float GetDeviceScaleFactor() const override;
  bool PreferDarkTheme() const override;
  bool AnimationsEnabled() const override;
  std::unique_ptr<ui::NavButtonProvider> CreateNavButtonProvider() override;
  ui::WindowFrameProvider* GetWindowFrameProvider(bool solid_frame) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  std::string GetCursorThemeName() override;
  int GetCursorThemeSize() override;
  ui::NativeTheme* GetNativeThemeImpl() const override;
  bool GetTextEditCommandsForEvent(
      const ui::Event& event,
      std::vector<ui::TextEditCommandAuraLinux>* commands) override;
#if BUILDFLAG(ENABLE_PRINTING)
  printing::PrintDialogLinuxInterface* CreatePrintDialog(
      printing::PrintingContextLinux* context) override;
  gfx::Size GetPdfPaperSize(printing::PrintingContextLinux* context) override;
#endif
};

}  // namespace ui

#endif  // UI_LINUX_FAKE_LINUX_UI_H_
