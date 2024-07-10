// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/fake_linux_ui.h"

#include "base/time/time.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

FakeLinuxUi::FakeLinuxUi() = default;

FakeLinuxUi::~FakeLinuxUi() = default;

std::unique_ptr<ui::LinuxInputMethodContext>
FakeLinuxUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  return nullptr;
}

gfx::FontRenderParams FakeLinuxUi::GetDefaultFontRenderParams() {
  return gfx::FontRenderParams();
}

ui::SelectFileDialog* FakeLinuxUi::CreateSelectFileDialog(
    void* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  return nullptr;
}

bool FakeLinuxUi::Initialize() {
  return false;
}

void FakeLinuxUi::InitializeFontSettings() {
  set_default_font_settings(FontSettings());
}

bool FakeLinuxUi::GetColor(int id,
                           SkColor* color,
                           bool use_custom_frame) const {
  return false;
}

bool FakeLinuxUi::GetDisplayProperty(int id, int* result) const {
  return false;
}

void FakeLinuxUi::GetFocusRingColor(SkColor* color) const {
  *color = gfx::kPlaceholderColor;
}

void FakeLinuxUi::GetActiveSelectionBgColor(SkColor* color) const {
  *color = gfx::kPlaceholderColor;
}

void FakeLinuxUi::GetActiveSelectionFgColor(SkColor* color) const {
  *color = gfx::kPlaceholderColor;
}

void FakeLinuxUi::GetInactiveSelectionBgColor(SkColor* color) const {
  *color = gfx::kPlaceholderColor;
}

void FakeLinuxUi::GetInactiveSelectionFgColor(SkColor* color) const {
  *color = gfx::kPlaceholderColor;
}

base::TimeDelta FakeLinuxUi::GetCursorBlinkInterval() const {
  return base::TimeDelta();
}

gfx::Image FakeLinuxUi::GetIconForContentType(const std::string& content_type,
                                              int size,
                                              float scale) const {
  return gfx::Image();
}

LinuxUi::WindowFrameAction FakeLinuxUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  return WindowFrameAction::kNone;
}

bool FakeLinuxUi::PreferDarkTheme() const {
  return false;
}

void FakeLinuxUi::SetDarkTheme(bool dark) {}

void FakeLinuxUi::SetAccentColor(std::optional<SkColor> accent_color) {}

bool FakeLinuxUi::AnimationsEnabled() const {
  return true;
}

void FakeLinuxUi::AddWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {}

void FakeLinuxUi::RemoveWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {}

std::unique_ptr<ui::NavButtonProvider> FakeLinuxUi::CreateNavButtonProvider() {
  return nullptr;
}

ui::WindowFrameProvider* FakeLinuxUi::GetWindowFrameProvider(bool solid_frame,
                                                             bool tiled) {
  return nullptr;
}

base::flat_map<std::string, std::string> FakeLinuxUi::GetKeyboardLayoutMap() {
  return base::flat_map<std::string, std::string>();
}

std::string FakeLinuxUi::GetCursorThemeName() {
  return std::string();
}

int FakeLinuxUi::GetCursorThemeSize() {
  return 0;
}

ui::NativeTheme* FakeLinuxUi::GetNativeTheme() const {
  return nullptr;
}

bool FakeLinuxUi::GetTextEditCommandsForEvent(
    const ui::Event& event,
    int text_falgs,
    std::vector<ui::TextEditCommandAuraLinux>* commands) {
  return false;
}

#if BUILDFLAG(ENABLE_PRINTING)
printing::PrintDialogLinuxInterface* FakeLinuxUi::CreatePrintDialog(
    printing::PrintingContextLinux* context) {
  return nullptr;
}

gfx::Size FakeLinuxUi::GetPdfPaperSize(
    printing::PrintingContextLinux* context) {
  return gfx::Size();
}
#endif

}  // namespace ui
