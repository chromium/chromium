// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_skia.h"

#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_names_testing.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/system_fonts_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include <fontconfig/fontconfig.h>

#include "ui/gfx/linux/fontconfig_util.h"
#include "ui/linux/fake_linux_ui.h"
#endif

namespace gfx {

#if BUILDFLAG(IS_LINUX)
// Implementation of LinuxUi used to control the default font description.
class TestFontDelegate : public ui::FakeLinuxUi {
 public:
  TestFontDelegate() {
    set_default_font_settings(FontSettings{
        // Default values to be returned.
        .family = "",
        .size_pixels = 0,
        .style = Font::NORMAL,
        .weight = static_cast<int>(Font::Weight::NORMAL),
    });
  }

  TestFontDelegate(const TestFontDelegate&) = delete;
  TestFontDelegate& operator=(const TestFontDelegate&) = delete;

  ~TestFontDelegate() override = default;

  void SetFontSettings(const FontSettings& font_settings,
                       const FontRenderParams& params) {
    set_default_font_settings(font_settings);
    params_ = params;
  }

  FontRenderParams GetDefaultFontRenderParams() override {
    return params_;
    NOTIMPLEMENTED();
    return FontRenderParams();
  }

 private:
  FontRenderParams params_;
};

class PlatformFontSkiaTest : public testing::Test {
 public:
  PlatformFontSkiaTest() = default;

  PlatformFontSkiaTest(const PlatformFontSkiaTest&) = delete;
  PlatformFontSkiaTest& operator=(const PlatformFontSkiaTest&) = delete;

  ~PlatformFontSkiaTest() override = default;

  void SetUp() override {
    DCHECK_EQ(ui::LinuxUi::instance(), nullptr);
    old_linux_ui_ = ui::LinuxUi::SetInstance(&test_font_delegate_);
    ClearFontRenderParamsCache();

    // Use a fresh Fontconfig configuration with the test fonts, so that tests
    // can add rules to it without affecting other tests.
    override_config_ = FcConfigCreate();
    CHECK(FcConfigParseAndLoad(override_config_, nullptr, FcTrue));
    CHECK(FcConfigBuildFonts(override_config_));
    original_config_ = GetGlobalFontConfig();
    OverrideGlobalFontConfigForTesting(override_config_);

    PlatformFontSkia::ReloadDefaultFont();
  }

  void TearDown() override {
    DCHECK_EQ(&test_font_delegate_, ui::LinuxUi::instance());
    OverrideGlobalFontConfigForTesting(original_config_);
    FcConfigDestroy(override_config_.ExtractAsDangling());
    ClearFontRenderParamsCache();
    ui::LinuxUi::SetInstance(old_linux_ui_);
    PlatformFontSkia::ReloadDefaultFont();
  }

 protected:
  // Loads XML-formatted `data` into the current Fontconfig configuration.
  bool LoadConfigDataIntoFontconfig(const std::string& data) {
    return FcConfigParseAndLoadFromMemory(
        override_config_, reinterpret_cast<const FcChar8*>(data.c_str()),
        FcTrue);
  }

  TestFontDelegate test_font_delegate_;
  raw_ptr<ui::LinuxUi> old_linux_ui_ = nullptr;
  raw_ptr<FcConfig> override_config_ = nullptr;
  raw_ptr<FcConfig> original_config_ = nullptr;
};

// Test that PlatformFontSkia's default constructor initializes the instance
// with the correct parameters.
TEST_F(PlatformFontSkiaTest, DefaultFont) {
  FontRenderParams params;
  params.antialiasing = false;
  params.hinting = FontRenderParams::HINTING_FULL;
  test_font_delegate_.SetFontSettings(
      {
          .family = kTestFontName,
          .size_pixels = 13,
          .style = Font::NORMAL,
          .weight = static_cast<int>(gfx::Font::Weight::NORMAL),
      },
      params);
  scoped_refptr<gfx::PlatformFontSkia> font(new gfx::PlatformFontSkia());
  EXPECT_EQ(kTestFontName, font->GetFontName());
  EXPECT_EQ(13, font->GetFontSize());
  EXPECT_EQ(gfx::Font::NORMAL, font->GetStyle());

  EXPECT_EQ(params.antialiasing, font->GetFontRenderParams().antialiasing);
  EXPECT_EQ(params.hinting, font->GetFontRenderParams().hinting);

  // Drop the old default font and check that new settings are loaded.
  test_font_delegate_.SetFontSettings(
      {
          .family = kSymbolFontName,
          .size_pixels = 15,
          .style = Font::ITALIC,
          .weight = static_cast<int>(gfx::Font::Weight::BOLD),
      },
      params);
  PlatformFontSkia::ReloadDefaultFont();
  scoped_refptr<gfx::PlatformFontSkia> font2(new gfx::PlatformFontSkia());
  EXPECT_EQ(kSymbolFontName, font2->GetFontName());
  EXPECT_EQ(15, font2->GetFontSize());
  EXPECT_NE(font2->GetStyle() & Font::ITALIC, 0);
  EXPECT_EQ(gfx::Font::Weight::BOLD, font2->GetWeight());
}

// Test that Fontconfig has the final say over the default font's render
// params, as it does for all other fonts.
TEST_F(PlatformFontSkiaTest, DefaultFontUsesFontconfig) {
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(
      "<?xml version=\"1.0\"?>\n"
      "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
      "<fontconfig>\n"
      "  <match target=\"font\">\n"
      "    <edit name=\"antialias\" mode=\"assign\"><bool>false</bool></edit>\n"
      "    <edit name=\"hinting\" mode=\"assign\"><bool>true</bool></edit>\n"
      "    <edit name=\"hintstyle\" mode=\"assign\">"
      "<const>hintfull</const></edit>\n"
      "  </match>\n"
      "</fontconfig>\n"));

  // The toolkit reports antialiasing, but Fontconfig disables it.
  FontRenderParams toolkit_params;
  toolkit_params.antialiasing = true;
  toolkit_params.hinting = FontRenderParams::HINTING_SLIGHT;
  const ui::LinuxUi::FontSettings font_settings = {
      .family = kTestFontName,
      .size_pixels = 13,
      .style = Font::NORMAL,
      .weight = static_cast<int>(gfx::Font::Weight::NORMAL),
  };
  test_font_delegate_.SetFontSettings(font_settings, toolkit_params);

  scoped_refptr<PlatformFontSkia> default_font(new PlatformFontSkia());
  EXPECT_FALSE(default_font->GetFontRenderParams().antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL,
            default_font->GetFontRenderParams().hinting);

  // A font derived with a different size must get the same params.
  Font derived =
      Font(default_font.get()).Derive(1, Font::NORMAL, Font::Weight::NORMAL);
  EXPECT_EQ(default_font->GetFontRenderParams(), derived.GetFontRenderParams());
}
#endif  // BUILDFLAG(IS_LINUX)

TEST(PlatformFontSkiaRenderParamsTest, DefaultFontRenderParams) {
  scoped_refptr<PlatformFontSkia> default_font(new PlatformFontSkia());
  scoped_refptr<PlatformFontSkia> named_font(new PlatformFontSkia(
      default_font->GetFontName(), default_font->GetFontSize()));

  // Ensures that both constructors are producing fonts with the same render
  // params.
  EXPECT_EQ(default_font->GetFontRenderParams(),
            named_font->GetFontRenderParams());
}

#if BUILDFLAG(IS_WIN)
TEST(PlatformFontSkiaOnWindowsTest, SystemFont) {
  // Ensures that the font styles are kept while creating the default font.
  gfx::Font system_font = win::GetDefaultSystemFont();
  gfx::Font default_font;

  EXPECT_EQ(system_font.GetFontName(), default_font.GetFontName());
  EXPECT_EQ(system_font.GetFontSize(), default_font.GetFontSize());
  EXPECT_EQ(system_font.GetStyle(), default_font.GetStyle());
  EXPECT_EQ(system_font.GetWeight(), default_font.GetWeight());
  EXPECT_EQ(system_font.GetHeight(), default_font.GetHeight());
  EXPECT_EQ(system_font.GetBaseline(), default_font.GetBaseline());
  EXPECT_EQ(system_font.GetBaseline(), default_font.GetBaseline());
  EXPECT_EQ(system_font.GetFontRenderParams(),
            default_font.GetFontRenderParams());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gfx
