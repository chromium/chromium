// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/fallback_linux_ui.h"

#include "base/time/time.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/platform_font.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ui {

FallbackLinuxUi::FallbackLinuxUi() = default;

FallbackLinuxUi::~FallbackLinuxUi() = default;

std::unique_ptr<ui::LinuxInputMethodContext>
FallbackLinuxUi::CreateInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate) const {
  // Fallback to FakeInputMethodContext.
  return nullptr;
}

gfx::FontRenderParams FallbackLinuxUi::GetDefaultFontRenderParams() {
  if (!default_font_render_params_.has_value()) {
    InitializeFontSettings();
  }
  return *default_font_render_params_;
}

ui::SelectFileDialog* FallbackLinuxUi::CreateSelectFileDialog(
    void* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) const {
  // The toolkit select file dialog is a last choice used only when
  // the freedesktop portal interface isn't available and the desktop
  // isn't KDE.  If neither condition is met, a dialog won't be created.
  NOTIMPLEMENTED();
  return nullptr;
}

bool FallbackLinuxUi::Initialize() {
  return true;
}

void FallbackLinuxUi::InitializeFontSettings() {
  gfx::FontRenderParamsQuery query;
  query.pixel_size = gfx::PlatformFont::kDefaultBaseFontSize;
  query.style = gfx::Font::NORMAL;
  query.weight = gfx::Font::Weight::NORMAL;
  query.device_scale_factor = display_config().primary_scale;
  std::string default_font_family;
  // gfx::GetFontRenderParams() calls GetDefaultFontRenderParams(), so
  // initialize `default_font_render_params_`.  This is intended to use
  // the FontConfig settings when there's no toolkit to obtain font
  // settings from.
  default_font_render_params_ = gfx::FontRenderParams();
  default_font_render_params_ =
      gfx::GetFontRenderParams(query, &default_font_family);
  set_default_font_settings(FontSettings{
      .family = std::move(default_font_family),
      .size_pixels = query.pixel_size,
      .style = query.style,
      .weight = static_cast<int>(query.weight),
  });
}

bool FallbackLinuxUi::GetColor(int id,
                               SkColor* color,
                               bool use_custom_frame) const {
  return false;
}

bool FallbackLinuxUi::GetDisplayProperty(int id, int* result) const {
  return false;
}

void FallbackLinuxUi::GetFocusRingColor(SkColor* color) const {}

void FallbackLinuxUi::GetActiveSelectionBgColor(SkColor* color) const {}

void FallbackLinuxUi::GetActiveSelectionFgColor(SkColor* color) const {}

void FallbackLinuxUi::GetInactiveSelectionBgColor(SkColor* color) const {}

void FallbackLinuxUi::GetInactiveSelectionFgColor(SkColor* color) const {}

base::TimeDelta FallbackLinuxUi::GetCursorBlinkInterval() const {
  return views::Textfield::GetCaretBlinkInterval();
}

gfx::Image FallbackLinuxUi::GetIconForContentType(
    const std::string& content_type,
    int size,
    float scale) const {
  // Fallback to using generic icons.
  return gfx::Image();
}

LinuxUi::WindowFrameAction FallbackLinuxUi::GetWindowFrameAction(
    WindowFrameActionSource source) {
  switch (source) {
    case WindowFrameActionSource::kDoubleClick:
      return WindowFrameAction::kToggleMaximize;
    case WindowFrameActionSource::kMiddleClick:
      return WindowFrameAction::kNone;
    case WindowFrameActionSource::kRightClick:
      return WindowFrameAction::kMenu;
  }
}

bool FallbackLinuxUi::PreferDarkTheme() const {
  return theme_is_dark_;
}

void FallbackLinuxUi::SetDarkTheme(bool dark) {
  theme_is_dark_ = dark;
}

void FallbackLinuxUi::SetAccentColor(std::optional<SkColor> accent_color) {}

bool FallbackLinuxUi::AnimationsEnabled() const {
  return true;
}

void FallbackLinuxUi::AddWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {}

void FallbackLinuxUi::RemoveWindowButtonOrderObserver(
    ui::WindowButtonOrderObserver* observer) {}

std::unique_ptr<ui::NavButtonProvider>
FallbackLinuxUi::CreateNavButtonProvider() {
  return nullptr;
}

ui::WindowFrameProvider* FallbackLinuxUi::GetWindowFrameProvider(
    bool solid_frame,
    bool tiled) {
  return nullptr;
}

base::flat_map<std::string, std::string>
FallbackLinuxUi::GetKeyboardLayoutMap() {
  return ui::GenerateDomKeyboardLayoutMap();
}

std::string FallbackLinuxUi::GetCursorThemeName() {
  // This is only used on X11 where QT obtains the cursor theme from XSettings.
  // However, ui/base/x/x11_cursor_loader.cc already handles this.
  return std::string();
}

int FallbackLinuxUi::GetCursorThemeSize() {
  // This is only used on X11 where QT obtains the cursor size from XSettings.
  // However, ui/base/x/x11_cursor_loader.cc already handles this.
  return 0;
}

ui::NativeTheme* FallbackLinuxUi::GetNativeTheme() const {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

bool FallbackLinuxUi::GetTextEditCommandsForEvent(
    const ui::Event& event,
    int text_flags,
    std::vector<ui::TextEditCommandAuraLinux>* commands) {
  return false;
}

#if BUILDFLAG(ENABLE_PRINTING)
printing::PrintDialogLinuxInterface* FallbackLinuxUi::CreatePrintDialog(
    printing::PrintingContextLinux* context) {
  // A print dialog won't be created.  Chrome's print dialog (Ctrl-P)
  // should be used instead of the system (Ctrl-Shift-P) dialog.
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Size FallbackLinuxUi::GetPdfPaperSize(
    printing::PrintingContextLinux* context) {
  return gfx::Size();
}
#endif

}  // namespace ui
