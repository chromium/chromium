// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/system_fonts_win.h"

#include <windows.h>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "ui/gfx/platform_font.h"

namespace gfx {
namespace win {

namespace {

class SystemFonts {
 public:
  const gfx::Font& GetFont(SystemFont system_font) {
    if (!IsInitialized())
      Initialize();

    auto it = system_fonts_.find(system_font);
    CHECK(it != system_fonts_.end(), base::NotFatalUntil::M130)
        << "System font #" << static_cast<int>(system_font) << " not found!";
    return it->second;
  }

  static SystemFonts* Instance() {
    static base::NoDestructor<SystemFonts> instance;
    return instance.get();
  }

  SystemFonts(const SystemFonts&) = delete;
  SystemFonts& operator=(const SystemFonts&) = delete;

  void ResetForTesting() {
    SystemFonts::is_initialized_ = false;
    SystemFonts::adjust_font_callback_ = nullptr;
    SystemFonts::get_minimum_font_size_callback_ = nullptr;
    system_fonts_.clear();
  }

  static int AdjustFontSize(int lf_height, int size_delta) {
    // Extract out the sign of |lf_height| - we'll add it back later.
    const int lf_sign = lf_height < 0 ? -1 : 1;
    lf_height = std::abs(lf_height);

    // Apply the size adjustment.
    lf_height += size_delta;

    // Make sure |lf_height| is not smaller than allowed min allowed font size.
    int min_font_size = 0;
    if (get_minimum_font_size_callback_) {
      min_font_size = get_minimum_font_size_callback_();
      DCHECK_GE(min_font_size, 0);
    }
    lf_height = std::max(min_font_size, lf_height);

    // Add back the sign.
    return lf_sign * lf_height;
  }

  static void AdjustLOGFONT(const FontAdjustment& font_adjustment,
                            LOGFONT* logfont) {
    DCHECK_GT(font_adjustment.font_scale, 0.0);
    LONG new_height =
        base::ClampRound<LONG>(logfont->lfHeight * font_adjustment.font_scale);
    if (logfont->lfHeight && !new_height)
      new_height = logfont->lfHeight > 0 ? 1 : -1;
    logfont->lfHeight = new_height;
    if (!font_adjustment.font_family_override.empty()) {
      auto result = wcscpy_s(logfont->lfFaceName,
                             font_adjustment.font_family_override.c_str());
      DCHECK_EQ(0, result) << "Font name "
                           << font_adjustment.font_family_override
                           << " cannot be copied into LOGFONT structure.";
    }
  }

  static Font GetFontFromLOGFONT(const LOGFONT& logfont) {
    // Finds a matching font by triggering font mapping. The font mapper finds
    // the closest physical font for a given logical font.
    base::win::ScopedHFONT font(::CreateFontIndirect(&logfont));
    base::win::ScopedGetDC screen_dc(NULL);
    base::win::ScopedSelectObject scoped_font(screen_dc, font.get());

    DCHECK(font.get()) << "Font for '"
                       << base::SysWideToUTF8(logfont.lfFaceName)
                       << "' has an invalid handle.";

    // Retrieve the name and height of the mapped font (physical font).
    LOGFONT mapped_font_info;
    GetObject(font.get(), sizeof(mapped_font_info), &mapped_font_info);
    std::string font_name = base::SysWideToUTF8(mapped_font_info.lfFaceName);

    TEXTMETRIC mapped_font_metrics;
    GetTextMetrics(screen_dc, &mapped_font_metrics);
    const int font_size =
        std::max<int>(1, mapped_font_metrics.tmHeight -
                             mapped_font_metrics.tmInternalLeading);

    Font system_font =
        Font(PlatformFont::CreateFromNameAndSize(font_name, font_size));

    // System fonts may have different styles when they are manually changed by
    // the users (see crbug.com/989476).
    Font::FontStyle style = logfont.lfItalic == 0 ? Font::FontStyle::NORMAL
                                                  : Font::FontStyle::ITALIC;
    Font::Weight weight = logfont.lfWeight == 0
                              ? Font::Weight::NORMAL
                              : static_cast<Font::Weight>(logfont.lfWeight);
    if (style != Font::FontStyle::NORMAL || weight != Font::Weight::NORMAL)
      system_font = system_font.Derive(0, style, weight);

    return system_font;
  }

  static void SetGetMinimumFontSizeCallback(
      GetMinimumFontSizeCallback callback) {
    DCHECK(!SystemFonts::IsInitialized());
    get_minimum_font_size_callback_ = callback;
  }

  static void SetAdjustFontCallback(AdjustFontCallback callback) {
    DCHECK(!SystemFonts::IsInitialized());
    adjust_font_callback_ = callback;
  }

 private:
  friend base::NoDestructor<SystemFonts>;

  SystemFonts() {}

  void Initialize() {
    TRACE_EVENT0("fonts", "gfx::SystemFonts::Initialize");

    NONCLIENTMETRICS metrics = {};
    metrics.cbSize = sizeof(metrics);
    const bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                                metrics.cbSize, &metrics, 0);
    DCHECK(success);

    // NOTE(dfried): When rendering Chrome, we do all of our own font scaling
    // based on a number of factors, but what Windows reports to us has some
    // (but not all) of these factors baked in, and not in a way that is
    // display-consistent.
    //
    // For example, if your system DPI is 192 (200%) but you connect a monitor
    // with a standard DPI (100%) then even if Chrome starts on the second
    // monitor, we will be told the system font is 24pt instead of 12pt.
    // Conversely, if the system DPI is set to 96 (100%) but all of our monitors
    // are currently at 150%, Windows will still report 12pt fonts.
    //
    // The same is true with Text Zoom (a new accessibility feature). If zoom is
    // set to 150%, then Windows will report a font size of 18pt. But again, we
    // already take Text Zoom into account when rendering, so we want to account
    // for that.
    //
    // Our system fonts are in DIPs, so we must always take what Windows gives
    // us, figure out which adjustments it's making (and undo them), make our
    // own adjustments for localization (for example, we always render Hindi 25%
    // larger for readability), and only then can we store (and report) the
    // system fonts.

    // Factor in/out scale adjustment that fall outside what we can access here.
    // This includes l10n adjustments and those we have to ask UWP or other COM
    // interfaces for (since we don't have dependencies on that code from this
    // module, and don't want to implicitly invoke COM for testing purposes if
    // we don't have to).
    FontAdjustment font_adjustment;
    if (adjust_font_callback_) {
      adjust_font_callback_(font_adjustment);
    }

    // Factor out system DPI scale that Windows will include in reported font
    // sizes. Note that these are (sadly) system-wide and do not reflect
    // specific displays' DPI.
    double system_scale = GetSystemScale();
    font_adjustment.font_scale /= system_scale;

    // Grab each of the fonts from the NONCLIENTMETRICS block, adjust it
    // appropriately, and store it in the font table.
    AddFont(SystemFont::kCaption, font_adjustment, &metrics.lfCaptionFont);
    AddFont(SystemFont::kSmallCaption, font_adjustment,
            &metrics.lfSmCaptionFont);
    AddFont(SystemFont::kMenu, font_adjustment, &metrics.lfMenuFont);
    AddFont(SystemFont::kMessage, font_adjustment, &metrics.lfMessageFont);
    AddFont(SystemFont::kStatus, font_adjustment, &metrics.lfStatusFont);

    is_initialized_ = true;
  }

  static bool IsInitialized() { return is_initialized_; }

  void AddFont(SystemFont system_font,
               const FontAdjustment& font_adjustment,
               LOGFONT* logfont) {
    TRACE_EVENT0("fonts", "gfx::SystemFonts::AddFont");

    // Make adjustments to the font as necessary.
    AdjustLOGFONT(font_adjustment, logfont);

    // Cap at minimum font size.
    logfont->lfHeight = AdjustFontSize(logfont->lfHeight, 0);

    system_fonts_.emplace(system_font, GetFontFromLOGFONT(*logfont));
  }

  // Returns the system DPI scale (standard DPI being 1.0).
  // TODO(dfried): move dpi.[h|cc] somewhere in base/win so we can share this
  // logic. However, note that the similar function in dpi.h is used many places
  // it ought not to be.
  static double GetSystemScale() {
    constexpr double kDefaultDPI = 96.0;
    base::win::ScopedGetDC screen_dc(nullptr);
    return ::GetDeviceCaps(screen_dc, LOGPIXELSY) / kDefaultDPI;
  }

  // Use a flat map for faster lookups.
  base::flat_map<SystemFont, gfx::Font> system_fonts_;

  static bool is_initialized_;

  // Font adjustment callback.
  static AdjustFontCallback adjust_font_callback_;

  // Minimum size callback.
  static GetMinimumFontSizeCallback get_minimum_font_size_callback_;
};

// static
bool SystemFonts::is_initialized_ = false;

// static
AdjustFontCallback SystemFonts::adjust_font_callback_ = nullptr;

// static
GetMinimumFontSizeCallback SystemFonts::get_minimum_font_size_callback_ =
    nullptr;

}  // namespace

void SetGetMinimumFontSizeCallback(GetMinimumFontSizeCallback callback) {
  SystemFonts::SetGetMinimumFontSizeCallback(callback);
}

void SetAdjustFontCallback(AdjustFontCallback callback) {
  SystemFonts::SetAdjustFontCallback(callback);
}

const Font& GetDefaultSystemFont() {
  // The message font is the closest font for a default system font from the
  // structure NONCLIENTMETRICS. The lfMessageFont field contains information
  // about the logical font used to display text in message boxes.
  return GetSystemFont(SystemFont::kMessage);
}

const Font& GetSystemFont(SystemFont system_font) {
  return SystemFonts::Instance()->GetFont(system_font);
}

int AdjustFontSize(int lf_height, int size_delta) {
  return SystemFonts::AdjustFontSize(lf_height, size_delta);
}

void AdjustLOGFONTForTesting(const FontAdjustment& font_adjustment,
                             LOGFONT* logfont) {
  SystemFonts::AdjustLOGFONT(font_adjustment, logfont);
}

// Retrieve a FONT from a LOGFONT structure.
Font GetFontFromLOGFONTForTesting(const LOGFONT& logfont) {
  return SystemFonts::GetFontFromLOGFONT(logfont);
}

void ResetSystemFontsForTesting() {
  SystemFonts::Instance()->ResetForTesting();
}

}  // namespace win
}  // namespace gfx
