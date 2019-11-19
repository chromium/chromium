// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note: This header doesn't use REGISTER_TYPED_TEST_SUITE_P like most
// type-parameterized gtests. There are lot of test cases in here that are only
// enabled on certain platforms. However, preprocessor directives in macro
// arguments result in undefined behavior (and don't work on MSVC). Instead,
// 'parameterized' tests should typedef TypesToTest (which is used to
// instantiate the tests using the TYPED_TEST_SUITE macro) and then #include
// this header.
// TODO(dcheng): This is really horrible. In general, all tests should run on
// all platforms, to avoid this mess.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <string>

#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/half_float.h"

#if defined(OS_WIN)
#include "ui/base/clipboard/clipboard_util_win.h"
#endif

#if defined(USE_AURA)
#include "ui/events/platform/platform_event_source.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

using testing::Contains;

namespace ui {

template <typename ClipboardTraits>
class ClipboardTest : public PlatformTest {
 public:
  ClipboardTest() = default;
  ~ClipboardTest() override = default;

  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
#if defined(USE_AURA)
    event_source_ = ClipboardTraits::GetEventSource();
#endif
    clipboard_ = ClipboardTraits::Create();
  }

  void TearDown() override {
    ClipboardTraits::Destroy(clipboard_);
    PlatformTest::TearDown();
  }

 protected:
  Clipboard& clipboard() { return *clipboard_; }

  std::vector<base::string16> GetAvailableTypes(ClipboardBuffer buffer) {
    bool contains_filenames;
    std::vector<base::string16> types;
    clipboard().ReadAvailableTypes(buffer, &types, &contains_filenames);
    return types;
  }

 private:
#if defined(USE_AURA)
  std::unique_ptr<PlatformEventSource> event_source_;
#endif
  // ui::Clipboard has a protected destructor, so scoped_ptr doesn't work here.
  Clipboard* clipboard_ = nullptr;
};

// Hack for tests that need to call static methods of ClipboardTest.
struct NullClipboardTraits {
  static Clipboard* Create() { return nullptr; }
  static void Destroy(Clipboard*) {}
};

// |NamesOfTypesToTest| provides a way to differentiate between different
// clipboard tests that include this file. See docs in gtest-typed-test.h
TYPED_TEST_SUITE(ClipboardTest, TypesToTest, NamesOfTypesToTest);

TYPED_TEST(ClipboardTest, ClearTest) {
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(ASCIIToUTF16("clear me"));
  }
  this->clipboard().Clear(ClipboardBuffer::kCopyPaste);

  EXPECT_TRUE(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste).empty());
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextWType(), ClipboardBuffer::kCopyPaste));
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kCopyPaste));
}

TYPED_TEST(ClipboardTest, TextTest) {
  base::string16 text(ASCIIToUTF16("This is a base::string16!#$")), text_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(text);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextWType(), ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kCopyPaste));
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste, &text_result);

  EXPECT_EQ(text, text_result);
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TYPED_TEST(ClipboardTest, HTMLTest) {
  base::string16 markup(ASCIIToUTF16("<string>Hi!</string>")), markup_result;
  base::string16 plain(ASCIIToUTF16("Hi!")), plain_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

TYPED_TEST(ClipboardTest, RTFTest) {
  std::string rtf =
      "{\\rtf1\\ansi{\\fonttbl\\f0\\fswiss Helvetica;}\\f0\\pard\n"
      "This is some {\\b bold} text.\\par\n"
      "}";

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteRTF(rtf);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeRTF)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetRtfType(), ClipboardBuffer::kCopyPaste));
  std::string result;
  this->clipboard().ReadRTF(ClipboardBuffer::kCopyPaste, &result);
  EXPECT_EQ(rtf, result);
}

// TODO(msisov, tonikitoo): Enable test once ClipboardOzone implements
// selection support. https://crbug.com/911992
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(USE_OZONE)
TYPED_TEST(ClipboardTest, MultipleBufferTest) {
  base::string16 text(ASCIIToUTF16("Standard")), text_result;
  base::string16 markup(ASCIIToUTF16("<string>Selection</string>"));
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(text);
  }

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kSelection);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kSelection),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kCopyPaste));
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kSelection));

  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kSelection));

  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste, &text_result);
  EXPECT_EQ(text, text_result);

  base::string16 markup_result;
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kSelection, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
}
#endif

TYPED_TEST(ClipboardTest, TrickyHTMLTest) {
  base::string16 markup(ASCIIToUTF16("<em>Bye!<!--EndFragment --></em>")),
      markup_result;
  std::string url, url_result;
  base::string16 plain(ASCIIToUTF16("Bye!")), plain_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
}

// Some platforms store HTML as UTF-8 internally. Make sure fragment indices are
// adjusted appropriately when converting back to UTF-16.
TYPED_TEST(ClipboardTest, UnicodeHTMLTest) {
  base::string16 markup(UTF8ToUTF16("<div>A \xc3\xb8 \xe6\xb0\xb4</div>")),
      markup_result;
  std::string url, url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if defined(OS_WIN)
  EXPECT_EQ(url, url_result);
#endif
}

// TODO(estade): Port the following test (decide what target we use for urls)
#if !defined(OS_POSIX) || defined(OS_MACOSX)
TYPED_TEST(ClipboardTest, BookmarkTest) {
  base::string16 title(ASCIIToUTF16("The Example Company")), title_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteBookmark(title, url);
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetUrlWType(), ClipboardBuffer::kCopyPaste));
  this->clipboard().ReadBookmark(&title_result, &url_result);
  EXPECT_EQ(title, title_result);
  EXPECT_EQ(url, url_result);
}
#endif  // !defined(OS_POSIX) || defined(OS_MACOSX)

TYPED_TEST(ClipboardTest, MultiFormatTest) {
  base::string16 text(ASCIIToUTF16("Hi!")), text_result;
  base::string16 markup(ASCIIToUTF16("<strong>Hi!</string>")), markup_result;
  std::string url("http://www.example.com/"), url_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHTML(markup, url);
    clipboard_writer.WriteText(text);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextWType(), ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kCopyPaste));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if defined(OS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // defined(OS_WIN)
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste, &text_result);
  EXPECT_EQ(text, text_result);
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TYPED_TEST(ClipboardTest, URLTest) {
  base::string16 url(ASCIIToUTF16("http://www.google.com/"));

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextWType(), ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetPlainTextType(), ClipboardBuffer::kCopyPaste));
  base::string16 text_result;
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste, &text_result);

  EXPECT_EQ(text_result, url);

  std::string ascii_text;
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);

// TODO(tonikitoo, msisov): enable back for ClipboardOzone implements
// selection support. https://crbug.com/911992
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
    !defined(OS_CHROMEOS) && !defined(USE_OZONE)
  ascii_text.clear();
  this->clipboard().ReadAsciiText(ClipboardBuffer::kSelection, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);
#endif
}

namespace {

using U8x4 = std::array<uint8_t, 4>;
using F16x4 = std::array<gfx::HalfFloat, 4>;

template <typename T>
static void TestBitmapWrite(Clipboard* clipboard,
                            const SkImageInfo& info,
                            const T* bitmap_data,
                            const U8x4* expect_data) {
  {
    ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
    SkBitmap bitmap;
    ASSERT_TRUE(bitmap.setInfo(info));
    bitmap.setPixels(
        const_cast<void*>(reinterpret_cast<const void*>(bitmap_data)));
    scw.WriteImage(bitmap);
  }

  EXPECT_TRUE(clipboard->IsFormatAvailable(ClipboardFormatType::GetBitmapType(),
                                           ClipboardBuffer::kCopyPaste));
  const SkBitmap& image = clipboard->ReadImage(ClipboardBuffer::kCopyPaste);
  ASSERT_EQ(image.info().colorType(), kN32_SkColorType);
  ASSERT_NE(image.info().alphaType(), kUnpremul_SkAlphaType);
  EXPECT_EQ(gfx::Size(info.width(), info.height()),
            gfx::Size(image.width(), image.height()));
  for (int y = 0; y < image.height(); ++y) {
    const U8x4* actual_row =
        reinterpret_cast<const U8x4*>(image.getAddr32(0, y));
    const U8x4* expect_row = &expect_data[y * info.width()];
    for (int x = 0; x < image.width(); ++x) {
      EXPECT_EQ(expect_row[x], actual_row[x]) << "x = " << x << ", y = " << y;
    }
  }
}

constexpr U8x4 kRGBAUnpremul = {0x8a, 0x50, 0x15, 0x46};
constexpr U8x4 kRGBAPremul = {0x26, 0x16, 0x06, 0x46};
constexpr U8x4 kRGBAOpaque = {0x26, 0x16, 0x06, 0xff};
constexpr U8x4 kBGRAUnpremul = {0x15, 0x50, 0x8a, 0x46};
constexpr U8x4 kBGRAPremul = {0x06, 0x16, 0x26, 0x46};
constexpr U8x4 kBGRAOpaque = {0x06, 0x16, 0x26, 0xff};
constexpr F16x4 kRGBAF16Unpremul = {0x3854, 0x3505, 0x2d45, 0x3464};
constexpr F16x4 kRGBAF16Premul = {0x30c5, 0x2d86, 0x2606, 0x3464};
constexpr F16x4 kRGBAF16Opaque = {0x30c5, 0x2d86, 0x2606, 0x3c00};

constexpr U8x4 kN32 =
    (kN32_SkColorType == kRGBA_8888_SkColorType) ? kRGBAPremul : kBGRAPremul;
constexpr U8x4 kN32Opaque =
    (kN32_SkColorType == kRGBA_8888_SkColorType) ? kRGBAOpaque : kBGRAOpaque;

// Either RGBA_8888 or BGRA_8888 will be equivalent to N32, but the other
// won't be.
TYPED_TEST(ClipboardTest, Bitmap_RGBA_Premul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &kRGBAPremul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_RGBA_Unpremul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType),
      &kRGBAUnpremul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_RGBA_Opaque) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kOpaque_SkAlphaType),
      &kRGBAOpaque, &kN32Opaque);
}
TYPED_TEST(ClipboardTest, Bitmap_BGRA_Premul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kPremul_SkAlphaType),
      &kBGRAPremul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_BGRA_Unpremul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kUnpremul_SkAlphaType),
      &kBGRAUnpremul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_BGRA_Opaque) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kBGRA_8888_SkColorType, kOpaque_SkAlphaType),
      &kBGRAOpaque, &kN32Opaque);
}

// Used by HTMLCanvasElement.
TYPED_TEST(ClipboardTest, Bitmap_F16_Premul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_F16_SkColorType, kPremul_SkAlphaType),
      &kRGBAF16Premul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_F16_Unpremul) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_F16_SkColorType, kUnpremul_SkAlphaType),
      &kRGBAF16Unpremul, &kN32);
}
TYPED_TEST(ClipboardTest, Bitmap_F16_Opaque) {
  TestBitmapWrite(
      &this->clipboard(),
      SkImageInfo::Make(1, 1, kRGBA_F16_SkColorType, kOpaque_SkAlphaType),
      &kRGBAF16Opaque, &kN32Opaque);
}

TYPED_TEST(ClipboardTest, Bitmap_N32_Premul) {
  constexpr U8x4 b[4 * 3] = {
      {0x26, 0x16, 0x06, 0x46}, {0x88, 0x59, 0x9f, 0xf6},
      {0x37, 0x29, 0x3f, 0x79}, {0x86, 0xb9, 0x55, 0xfa},
      {0x52, 0x21, 0x77, 0x78}, {0x30, 0x2a, 0x69, 0x87},
      {0x25, 0x2a, 0x32, 0x36}, {0x1b, 0x40, 0x20, 0x43},
      {0x21, 0x8c, 0x84, 0x91}, {0x3c, 0x7b, 0x17, 0xc3},
      {0x5c, 0x15, 0x46, 0x69}, {0x52, 0x19, 0x17, 0x64},
  };
  TestBitmapWrite(&this->clipboard(), SkImageInfo::MakeN32Premul(4, 3), b, b);
}
TYPED_TEST(ClipboardTest, Bitmap_N32_Premul_2x7) {
  constexpr U8x4 b[2 * 7] = {
      {0x26, 0x16, 0x06, 0x46}, {0x88, 0x59, 0x9f, 0xf6},
      {0x37, 0x29, 0x3f, 0x79}, {0x86, 0xb9, 0x55, 0xfa},
      {0x52, 0x21, 0x77, 0x78}, {0x30, 0x2a, 0x69, 0x87},
      {0x25, 0x2a, 0x32, 0x36}, {0x1b, 0x40, 0x20, 0x43},
      {0x21, 0x8c, 0x84, 0x91}, {0x3c, 0x7b, 0x17, 0xc3},
      {0x5c, 0x15, 0x46, 0x69}, {0x52, 0x19, 0x17, 0x64},
      {0x13, 0x03, 0x91, 0xa6}, {0x3e, 0x32, 0x02, 0x83},
  };
  TestBitmapWrite(&this->clipboard(), SkImageInfo::MakeN32Premul(2, 7), b, b);
}

}  // namespace

TYPED_TEST(ClipboardTest, PickleTest) {
  const ui::ClipboardFormatType kFormat =
      ui::ClipboardFormatType::GetType("chromium/x-test-format");
  std::string payload("test string");
  base::Pickle write_pickle;
  write_pickle.WriteString(payload);

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WritePickledData(write_pickle, kFormat);
  }

  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat,
                                                  ClipboardBuffer::kCopyPaste));
  std::string output;
  this->clipboard().ReadData(kFormat, &output);
  ASSERT_FALSE(output.empty());

  base::Pickle read_pickle(output.data(), static_cast<int>(output.size()));
  base::PickleIterator iter(read_pickle);
  std::string unpickled_string;
  ASSERT_TRUE(iter.ReadString(&unpickled_string));
  EXPECT_EQ(payload, unpickled_string);
}

TYPED_TEST(ClipboardTest, MultiplePickleTest) {
  const ui::ClipboardFormatType kFormat1 =
      ui::ClipboardFormatType::GetType("chromium/x-test-format1");
  std::string payload1("test string1");
  base::Pickle write_pickle1;
  write_pickle1.WriteString(payload1);

  const ui::ClipboardFormatType kFormat2 =
      ui::ClipboardFormatType::GetType("chromium/x-test-format2");
  std::string payload2("test string2");
  base::Pickle write_pickle2;
  write_pickle2.WriteString(payload2);

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WritePickledData(write_pickle1, kFormat1);
    // overwrite the previous pickle for fun
    clipboard_writer.WritePickledData(write_pickle2, kFormat2);
  }

  ASSERT_FALSE(this->clipboard().IsFormatAvailable(
      kFormat1, ClipboardBuffer::kCopyPaste));
  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat2,
                                                  ClipboardBuffer::kCopyPaste));

  // Check string 2.
  std::string output2;
  this->clipboard().ReadData(kFormat2, &output2);
  ASSERT_FALSE(output2.empty());

  base::Pickle read_pickle2(output2.data(), static_cast<int>(output2.size()));
  base::PickleIterator iter2(read_pickle2);
  std::string unpickled_string2;
  ASSERT_TRUE(iter2.ReadString(&unpickled_string2));
  EXPECT_EQ(payload2, unpickled_string2);

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WritePickledData(write_pickle2, kFormat2);
    // overwrite the previous pickle for fun
    clipboard_writer.WritePickledData(write_pickle1, kFormat1);
  }

  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat1,
                                                  ClipboardBuffer::kCopyPaste));
  ASSERT_FALSE(this->clipboard().IsFormatAvailable(
      kFormat2, ClipboardBuffer::kCopyPaste));

  // Check string 1.
  std::string output1;
  this->clipboard().ReadData(kFormat1, &output1);
  ASSERT_FALSE(output1.empty());

  base::Pickle read_pickle1(output1.data(), static_cast<int>(output1.size()));
  base::PickleIterator iter1(read_pickle1);
  std::string unpickled_string1;
  ASSERT_TRUE(iter1.ReadString(&unpickled_string1));
  EXPECT_EQ(payload1, unpickled_string1);
}

TYPED_TEST(ClipboardTest, DataTest) {
  const std::string kFormatString = "chromium/x-test-format";
  const ui::ClipboardFormatType kFormat =
      ui::ClipboardFormatType::GetType(kFormatString);
  const std::string payload = "test string";
  base::span<const uint8_t> payload_span(
      reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteData(UTF8ToUTF16(kFormatString),
                               mojo_base::BigBuffer(payload_span));
  }

  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat,
                                                  ClipboardBuffer::kCopyPaste));
  std::string output;
  this->clipboard().ReadData(kFormat, &output);

  EXPECT_EQ(payload, output);
}

// TODO(huangdarwin): Implement multiple raw types for AuraClipboard. This test
// currently doesn't run on AuraClipboard because AuraClipboard only supports
// one raw type.
#if (!defined(USE_AURA) || defined(OS_WIN) || defined(USE_OZONE) || \
     defined(USE_X11)) &&                                           \
    !defined(OS_CHROMEOS)
TYPED_TEST(ClipboardTest, MultipleDataTest) {
  const std::string kFormatString1 = "chromium/x-test-format1";
  const ui::ClipboardFormatType kFormat1 =
      ui::ClipboardFormatType::GetType(kFormatString1);
  const std::string payload1("test string1");
  base::span<const uint8_t> payload_span1(
      reinterpret_cast<const uint8_t*>(payload1.data()), payload1.size());

  const std::string kFormatString2 = "chromium/x-test-format2";
  const ui::ClipboardFormatType kFormat2 =
      ui::ClipboardFormatType::GetType(kFormatString2);
  const std::string payload2("test string2");
  base::span<const uint8_t> payload_span2(
      reinterpret_cast<const uint8_t*>(payload2.data()), payload2.size());

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    // Both payloads should write successfully and not overwrite one another.
    clipboard_writer.WriteData(UTF8ToUTF16(kFormatString1),
                               mojo_base::BigBuffer(payload_span1));
    clipboard_writer.WriteData(UTF8ToUTF16(kFormatString2),
                               mojo_base::BigBuffer(payload_span2));
  }

  // Check format 1.
  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat1,
                                                  ClipboardBuffer::kCopyPaste));
  std::string output1;
  this->clipboard().ReadData(kFormat1, &output1);
  EXPECT_EQ(payload1, output1);

  // Check format 2.
  ASSERT_TRUE(this->clipboard().IsFormatAvailable(kFormat2,
                                                  ClipboardBuffer::kCopyPaste));
  std::string output2;
  this->clipboard().ReadData(kFormat2, &output2);
  EXPECT_EQ(payload2, output2);
}
#endif

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
TYPED_TEST(ClipboardTest, HyperlinkTest) {
  const std::string kTitle("The <Example> Company's \"home page\"");
  const std::string kUrl("http://www.example.com?x=3&lt=3#\"'<>");
  const base::string16 kExpectedHtml(UTF8ToUTF16(
      "<a href=\"http://www.example.com?x=3&amp;lt=3#&quot;&#39;&lt;&gt;\">"
      "The &lt;Example&gt; Company&#39;s &quot;home page&quot;</a>"));

  std::string url_result;
  base::string16 html_result;
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHyperlink(ASCIIToUTF16(kTitle), kUrl);
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetHtmlType(), ClipboardBuffer::kCopyPaste));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste, &html_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_EQ(kExpectedHtml,
            html_result.substr(fragment_end - kExpectedHtml.size(),
                               kExpectedHtml.size()));
}
#endif

TYPED_TEST(ClipboardTest, WebSmartPasteTest) {
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteWebSmartPaste();
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::GetWebKitSmartPasteType(),
      ClipboardBuffer::kCopyPaste));
}

#if defined(OS_WIN)  // Windows only tests.
void HtmlTestHelper(const std::string& cf_html,
                    const std::string& expected_html) {
  std::string html;
  ClipboardUtil::CFHtmlToHtml(cf_html, &html, nullptr);
  EXPECT_EQ(html, expected_html);
}

TYPED_TEST(ClipboardTest, HtmlTest) {
  // Test converting from CF_HTML format data with <!--StartFragment--> and
  // <!--EndFragment--> comments, like from MS Word.
  HtmlTestHelper(
      "Version:1.0\r\n"
      "StartHTML:0000000105\r\n"
      "EndHTML:0000000199\r\n"
      "StartFragment:0000000123\r\n"
      "EndFragment:0000000161\r\n"
      "\r\n"
      "<html>\r\n"
      "<body>\r\n"
      "<!--StartFragment-->\r\n"
      "\r\n"
      "<p>Foo</p>\r\n"
      "\r\n"
      "<!--EndFragment-->\r\n"
      "</body>\r\n"
      "</html>\r\n\r\n",
      "<p>Foo</p>");

  // Test converting from CF_HTML format data without <!--StartFragment--> and
  // <!--EndFragment--> comments, like from OpenOffice Writer.
  HtmlTestHelper(
      "Version:1.0\r\n"
      "StartHTML:0000000105\r\n"
      "EndHTML:0000000151\r\n"
      "StartFragment:0000000121\r\n"
      "EndFragment:0000000131\r\n"
      "<html>\r\n"
      "<body>\r\n"
      "<p>Foo</p>\r\n"
      "</body>\r\n"
      "</html>\r\n\r\n",
      "<p>Foo</p>");
}
#endif  // defined(OS_WIN)

// Test writing all formats we have simultaneously.
TYPED_TEST(ClipboardTest, WriteEverything) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(UTF8ToUTF16("foo"));
    writer.WriteHTML(UTF8ToUTF16("foo"), "bar");
    writer.WriteBookmark(UTF8ToUTF16("foo"), "bar");
    writer.WriteHyperlink(ASCIIToUTF16("foo"), "bar");
    writer.WriteWebSmartPaste();
    // Left out: WriteFile, WriteFiles, WriteBitmapFromPixels, WritePickledData.
  }

  // Passes if we don't crash.
}

// TODO(dcheng): Fix this test for Android. It's rather involved, since the
// clipboard change listener is posted to the Java message loop, and spinning
// that loop from C++ to trigger the callback in the test requires a non-trivial
// amount of additional work.
#if !defined(OS_ANDROID)
// Simple test that the sequence number appears to change when the clipboard is
// written to.
// TODO(dcheng): Add a version to test ClipboardBuffer::kSelection.
TYPED_TEST(ClipboardTest, GetSequenceNumber) {
  const uint64_t first_sequence_number =
      this->clipboard().GetSequenceNumber(ClipboardBuffer::kCopyPaste);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(UTF8ToUTF16("World"));
  }

  // On some platforms, the sequence number is updated by a UI callback so pump
  // the message loop to make sure we get the notification.
  base::RunLoop().RunUntilIdle();

  const uint64_t second_sequence_number =
      this->clipboard().GetSequenceNumber(ClipboardBuffer::kCopyPaste);

  EXPECT_NE(first_sequence_number, second_sequence_number);
}
#endif

// Test that writing empty parameters doesn't try to dereference an empty data
// vector. Not crashing = passing.
TYPED_TEST(ClipboardTest, WriteTextEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::string16());
}

TYPED_TEST(ClipboardTest, WriteHTMLEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteHTML(base::string16(), std::string());
}

TYPED_TEST(ClipboardTest, WriteRTFEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteRTF(std::string());
}

TYPED_TEST(ClipboardTest, WriteBookmarkEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteBookmark(base::string16(), std::string());
}

TYPED_TEST(ClipboardTest, WriteHyperlinkEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteHyperlink(base::string16(), std::string());
}

TYPED_TEST(ClipboardTest, WritePickledData) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WritePickledData(base::Pickle(), ClipboardFormatType::GetPlainTextType());
}

TYPED_TEST(ClipboardTest, WriteImageEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteImage(SkBitmap());
}

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_
