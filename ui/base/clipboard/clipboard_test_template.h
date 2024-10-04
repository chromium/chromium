// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header doesn't use REGISTER_TYPED_TEST_SUITE_P like most
// type-parameterized gtests. There are lot of test cases in here that are only
// enabled on certain platforms. However, preprocessor directives in macro
// arguments result in undefined behavior (and don't work on MSVC). Instead,
// 'parameterized' tests should typedef TypesToTest (which is used to
// instantiate the tests using the TYPED_TEST_SUITE macro) and then #include
// this header.
// TODO(dcheng): This is really horrible. In general, all tests should run on
// all platforms, to avoid this mess.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/half_float.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/clipboard/clipboard_util_win.h"
#endif

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;

using testing::_;
using testing::Contains;
using testing::Property;

namespace ui {

template <typename ClipboardTraits>
class ClipboardTest : public PlatformTest {
 public:
#if BUILDFLAG(IS_ANDROID)
  ClipboardTest() {
    feature_list_.InitAndEnableFeature(features::kClipboardFiles);
  }
#else
  ClipboardTest() = default;
#endif
  ~ClipboardTest() override = default;

  // PlatformTest:
  void SetUp() override {
    PlatformTest::SetUp();
    clipboard_ = ClipboardTraits::Create();
  }

  void TearDown() override {
    ClipboardTraits::Destroy(clipboard_.ExtractAsDangling());
    PlatformTest::TearDown();
  }

 protected:
  Clipboard& clipboard() { return *clipboard_; }

  std::vector<std::u16string> GetAvailableTypes(ClipboardBuffer buffer) {
    std::vector<std::u16string> types;
    clipboard().ReadAvailableTypes(buffer, /* data_dst = */ nullptr, &types);
    return types;
  }

 private:
  // Clipboard has a protected destructor, so scoped_ptr doesn't work here.
  raw_ptr<Clipboard> clipboard_ = nullptr;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif
};

// A mock delegate for testing.
class MockPolicyController : public DataTransferPolicyController {
 public:
  MockPolicyController();
  ~MockPolicyController() override;

  MOCK_METHOD3(IsClipboardReadAllowed,
               bool(base::optional_ref<const DataTransferEndpoint> data_src,
                    base::optional_ref<const DataTransferEndpoint> data_dst,
                    const std::optional<size_t> size));
  MOCK_METHOD5(
      PasteIfAllowed,
      void(base::optional_ref<const DataTransferEndpoint> data_src,
           base::optional_ref<const DataTransferEndpoint> data_dst,
           absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
           content::RenderFrameHost* rfh,
           base::OnceCallback<void(bool)> callback));
  MOCK_METHOD4(DropIfAllowed,
               void(std::optional<ui::DataTransferEndpoint> data_src,
                    std::optional<ui::DataTransferEndpoint> data_dst,
                    std::optional<std::vector<ui::FileInfo>> filenames,
                    base::OnceClosure drop_cb));
};

MockPolicyController::MockPolicyController() = default;

MockPolicyController::~MockPolicyController() = default;

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
    clipboard_writer.WriteText(u"clear me");
  }
  this->clipboard().Clear(ClipboardBuffer::kCopyPaste);

  EXPECT_TRUE(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste).empty());
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextAType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#endif
}

TYPED_TEST(ClipboardTest, TextTest) {
  std::u16string text(u"This is a std::u16string!#$"), text_result;
  std::string ascii_text;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(text);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextAType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#endif

  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &text_result);

  EXPECT_EQ(text, text_result);
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste,
                                  /* data_dst = */ nullptr, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TYPED_TEST(ClipboardTest, HTMLTest) {
  std::u16string markup(u"<string>Hi!</string>"), markup_result;
  std::u16string plain(u"Hi!"), plain_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if BUILDFLAG(IS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // BUILDFLAG(IS_WIN)
}

TYPED_TEST(ClipboardTest, SvgTest) {
  std::u16string markup(u"<svg> <circle r=\"40\" /> </svg>");

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteSvg(markup);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeSvg)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::SvgType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));

  std::u16string markup_result;
  this->clipboard().ReadSvg(ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &markup_result);

  EXPECT_EQ(markup, markup_result);
  // On Windows, the SVG data is written as UTF-8.
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kUseUtf8EncodingForSvgImage)) {
    std::string result;
    this->clipboard().ReadData(ClipboardFormatType::SvgType(),
                               /*data_dst=*/nullptr, &result);
    // On Windows, after calling `GetClipboardData`, some extra null characters
    // are added at the end. Use the C-string for comparison that ignores the
    // null characters after the first one.
    EXPECT_EQ(base::UTF16ToUTF8(markup), result.c_str());
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40681589): This test fails with ClipboardAndroid, but passes
// with the TestClipboard as RTF isn't implemented in ClipboardAndroid.
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
      ClipboardFormatType::RtfType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  std::string result;
  this->clipboard().ReadRTF(ClipboardBuffer::kCopyPaste,
                            /* data_dst = */ nullptr, &result);
  EXPECT_EQ(rtf, result);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// MultipleBufferTest only ran on Linux because Linux is the only platform that
// supports the selection buffer by default.
#if BUILDFLAG(IS_LINUX)
TYPED_TEST(ClipboardTest, MultipleBufferTest) {
  if (!ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    return;
  }

  std::u16string text(u"Standard"), text_result;
  std::u16string markup(u"<string>Selection</string>");
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
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kSelection,
      /* data_dst = */ nullptr));

  EXPECT_FALSE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kSelection,
      /* data_dst = */ nullptr));

  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &text_result);
  EXPECT_EQ(text, text_result);

  std::u16string markup_result;
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kSelection,
                             /* data_dst = */ nullptr, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
}
#endif  // BUILDFLAG(IS_LINUX)

TYPED_TEST(ClipboardTest, TrickyHTMLTest) {
  std::u16string markup(u"<em>Bye!<!--EndFragment --></em>"), markup_result;
  std::string url, url_result;
  std::u16string plain(u"Bye!"), plain_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(plain);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if BUILDFLAG(IS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // BUILDFLAG(IS_WIN)
}

// Some platforms store HTML as UTF-8 internally. Make sure fragment indices are
// adjusted appropriately when converting back to UTF-16.
TYPED_TEST(ClipboardTest, UnicodeHTMLTest) {
  std::u16string markup(u"<div>A ø 水</div>"), markup_result;
  std::string url, url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHTML(markup, url);
#if BUILDFLAG(IS_ANDROID)
    // Android requires HTML and plain text representations to be written.
    clipboard_writer.WriteText(markup);
#endif
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(url, url_result);
#endif
}

// TODO(estade): Port the following test (decide what target we use for urls)
#if !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
TYPED_TEST(ClipboardTest, BookmarkTest) {
  std::u16string title(u"The Example Company"), title_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteBookmark(title, url);
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::UrlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  this->clipboard().ReadBookmark(/* data_dst = */ nullptr, &title_result,
                                 &url_result);
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(title, title_result);
#else
  // On Windows the title should be empty when CFSTR_INETURLW is queried.
  EXPECT_EQ(std::string(), UTF16ToUTF8(title_result));
#endif
  EXPECT_EQ(url, url_result);
}
#endif  // !BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_APPLE)

TYPED_TEST(ClipboardTest, FilenamesTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file));

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteFilenames(
        ui::FileInfosToURIList({ui::FileInfo(file, base::FilePath())}));
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::FilenamesType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));

  std::vector<std::u16string> types;
  this->clipboard().ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                       /* data_dst = */ nullptr, &types);
  EXPECT_EQ(1u, types.size());
  EXPECT_EQ(u"text/uri-list", types[0]);

  std::vector<ui::FileInfo> filenames;
  this->clipboard().ReadFilenames(ClipboardBuffer::kCopyPaste,
                                  /* data_dst = */ nullptr, &filenames);
  EXPECT_EQ(1u, filenames.size());
  EXPECT_EQ(file, filenames[0].path);
}

TYPED_TEST(ClipboardTest, MultiFormatTest) {
  std::u16string text(u"Hi!"), text_result;
  std::u16string markup(u"<strong>Hi!</string>"), markup_result;
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
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextAType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#endif
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &markup_result,
                             &url_result, &fragment_start, &fragment_end);
  EXPECT_LE(markup.size(), fragment_end - fragment_start);
  EXPECT_EQ(markup,
            markup_result.substr(fragment_end - markup.size(), markup.size()));
#if BUILDFLAG(IS_WIN)
  // TODO(playmobil): It's not clear that non windows clipboards need to support
  // this.
  EXPECT_EQ(url, url_result);
#endif  // BUILDFLAG(IS_WIN)
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &text_result);
  EXPECT_EQ(text, text_result);
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste,
                                  /* data_dst = */ nullptr, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(text), ascii_text);
}

TYPED_TEST(ClipboardTest, URLTest) {
  std::u16string url(u"http://www.google.com/");

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeText)));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::PlainTextAType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
#endif
  std::u16string text_result;
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &text_result);

  EXPECT_EQ(text_result, url);

  std::string ascii_text;
  this->clipboard().ReadAsciiText(ClipboardBuffer::kCopyPaste,
                                  /* data_dst = */ nullptr, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);

// TODO(tonikitoo, msisov): enable back for ClipboardOzone implements
// selection support. https://crbug.com/911992
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_OZONE)
  ascii_text.clear();
  this->clipboard().ReadAsciiText(ClipboardBuffer::kSelection,
                                  /* data_dst = */ nullptr, &ascii_text);
  EXPECT_EQ(UTF16ToUTF8(url), ascii_text);
#endif
}

#if BUILDFLAG(IS_WIN)
// See crbug.com/1477344 for more details on the issue.
TYPED_TEST(ClipboardTest, ChromiumCustomFormatTest) {
  std::u16string markup(u"<strong>Hi!</string>"), markup_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHTML(markup, url);
  }

  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeHTML)));
  EXPECT_THAT(
      this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
      testing::Not(Contains(ASCIIToUTF16(kMimeTypeDataTransferCustomData))));
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    base::flat_map<std::u16string, std::u16string> custom_data;
    custom_data[ASCIIToUTF16(kMimeTypeDataTransferCustomData)] = u"data";
    base::Pickle pickle;
    WriteCustomDataToPickle(custom_data, &pickle);
    clipboard_writer.WritePickledData(
        pickle, ClipboardFormatType::DataTransferCustomType());
  }
  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              testing::Not(Contains(ASCIIToUTF16(kMimeTypeHTML))));
  EXPECT_THAT(this->GetAvailableTypes(ClipboardBuffer::kCopyPaste),
              Contains(ASCIIToUTF16(kMimeTypeDataTransferCustomData)));
}
#endif  // BUILDFLAG(IS_WIN)

namespace {

using U8x4 = std::array<uint8_t, 4>;
using F16x4 = std::array<gfx::HalfFloat, 4>;

void WriteBitmap(Clipboard* clipboard,
                 const SkImageInfo& info,
                 const void* bitmap_data) {
  {
    ScopedClipboardWriter clipboard_writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(GURL("https://google.com/")));
    SkBitmap bitmap;
    ASSERT_TRUE(bitmap.setInfo(info));
    bitmap.setPixels(const_cast<void*>(bitmap_data));
    clipboard_writer.WriteImage(bitmap);
  }
}

void AssertBitmapMatchesExpected(const SkBitmap& image,
                                 const SkImageInfo& info,
                                 const U8x4* expect_data) {
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

template <typename T>
static void TestBitmapWriteAndPngRead(Clipboard* clipboard,
                                      const SkImageInfo& info,
                                      const T* bitmap_data,
                                      const U8x4* expect_data) {
  WriteBitmap(clipboard, info, reinterpret_cast<const void*>(bitmap_data));

  // Expect to be able to read images as either bitmaps or PNGs.
  EXPECT_TRUE(clipboard->IsFormatAvailable(ClipboardFormatType::BitmapType(),
                                           ClipboardBuffer::kCopyPaste,
                                           /* data_dst = */ nullptr));
  EXPECT_TRUE(clipboard->IsFormatAvailable(ClipboardFormatType::PngType(),
                                           ClipboardBuffer::kCopyPaste,
                                           /* data_dst = */ nullptr));
  std::vector<uint8_t> result = clipboard_test_util::ReadPng(clipboard);
  SkBitmap image;
  gfx::PNGCodec::Decode(result.data(), result.size(), &image);
  AssertBitmapMatchesExpected(image, info, expect_data);
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/41372437): Re-enable this test once death tests work on
// Android.

// Only kN32_SkColorType bitmaps are allowed into the clipboard to prevent
// surprising buffer overflows due to bits-per-pixel assumptions.
TYPED_TEST(ClipboardTest, BitmapWriteAndPngRead_F16_Premul) {
  constexpr F16x4 kRGBAF16Premul = {0x30c5, 0x2d86, 0x2606, 0x3464};
  constexpr U8x4 kRGBAPremul = {0x26, 0x16, 0x06, 0x46};
  EXPECT_DEATH_IF_SUPPORTED(
      TestBitmapWriteAndPngRead(
          &this->clipboard(),
          SkImageInfo::Make(1, 1, kRGBA_F16_SkColorType, kPremul_SkAlphaType),
          &kRGBAF16Premul, &kRGBAPremul),
      "");
}
#endif  // !BUILDFLAG(IS_ANDROID)

TYPED_TEST(ClipboardTest, BitmapWriteAndPngRead_N32_Premul) {
  constexpr U8x4 b[4 * 3] = {
      {0x26, 0x16, 0x06, 0x46}, {0x88, 0x59, 0x9f, 0xf6},
      {0x37, 0x29, 0x3f, 0x79}, {0x86, 0xb9, 0x55, 0xfa},
      {0x52, 0x21, 0x77, 0x78}, {0x30, 0x2a, 0x69, 0x87},
      {0x25, 0x2a, 0x32, 0x36}, {0x1b, 0x40, 0x20, 0x43},
      {0x21, 0x8c, 0x84, 0x91}, {0x3c, 0x7b, 0x17, 0xc3},
      {0x5c, 0x15, 0x46, 0x69}, {0x52, 0x19, 0x17, 0x64},
  };
  TestBitmapWriteAndPngRead(&this->clipboard(),
                            SkImageInfo::MakeN32Premul(4, 3), b, b);
}

TYPED_TEST(ClipboardTest, BitmapWriteAndPngRead_N32_Premul_2x7) {
  constexpr U8x4 b[2 * 7] = {
      {0x26, 0x16, 0x06, 0x46}, {0x88, 0x59, 0x9f, 0xf6},
      {0x37, 0x29, 0x3f, 0x79}, {0x86, 0xb9, 0x55, 0xfa},
      {0x52, 0x21, 0x77, 0x78}, {0x30, 0x2a, 0x69, 0x87},
      {0x25, 0x2a, 0x32, 0x36}, {0x1b, 0x40, 0x20, 0x43},
      {0x21, 0x8c, 0x84, 0x91}, {0x3c, 0x7b, 0x17, 0xc3},
      {0x5c, 0x15, 0x46, 0x69}, {0x52, 0x19, 0x17, 0x64},
      {0x13, 0x03, 0x91, 0xa6}, {0x3e, 0x32, 0x02, 0x83},
  };
  TestBitmapWriteAndPngRead(&this->clipboard(),
                            SkImageInfo::MakeN32Premul(2, 7), b, b);
}

}  // namespace

TYPED_TEST(ClipboardTest, PickleTest) {
  const ClipboardFormatType kFormat =
      ClipboardFormatType::GetType("chromium/x-test-format");
  std::string payload("test string");
  base::Pickle write_pickle;
  write_pickle.WriteString(payload);

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WritePickledData(write_pickle, kFormat);
  }

  ASSERT_TRUE(this->clipboard().IsFormatAvailable(
      kFormat, ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr));
  std::string output;
  this->clipboard().ReadData(kFormat, /* data_dst = */ nullptr, &output);
  ASSERT_FALSE(output.empty());

  base::Pickle read_pickle = base::Pickle::WithData(base::as_byte_span(output));
  base::PickleIterator iter(read_pickle);
  std::string unpickled_string;
  ASSERT_TRUE(iter.ReadString(&unpickled_string));
  EXPECT_EQ(payload, unpickled_string);
}

TYPED_TEST(ClipboardTest, MultiplePickleTest) {
  const ClipboardFormatType kFormat1 =
      ClipboardFormatType::GetType("chromium/x-test-format1");
  std::string payload1("test string1");
  base::Pickle write_pickle1;
  write_pickle1.WriteString(payload1);

  const ClipboardFormatType kFormat2 =
      ClipboardFormatType::GetType("chromium/x-test-format2");
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
      kFormat1, ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr));
  ASSERT_TRUE(this->clipboard().IsFormatAvailable(
      kFormat2, ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr));

  // Check string 2.
  std::string output2;
  this->clipboard().ReadData(kFormat2, /* data_dst = */ nullptr, &output2);
  ASSERT_FALSE(output2.empty());

  base::Pickle read_pickle2 =
      base::Pickle::WithData(base::as_byte_span(output2));
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

  ASSERT_TRUE(this->clipboard().IsFormatAvailable(
      kFormat1, ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr));
  ASSERT_FALSE(this->clipboard().IsFormatAvailable(
      kFormat2, ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr));

  // Check string 1.
  std::string output1;
  this->clipboard().ReadData(kFormat1, /* data_dst = */ nullptr, &output1);
  ASSERT_FALSE(output1.empty());

  base::Pickle read_pickle1 =
      base::Pickle::WithData(base::as_byte_span(output1));
  base::PickleIterator iter1(read_pickle1);
  std::string unpickled_string1;
  ASSERT_TRUE(iter1.ReadString(&unpickled_string1));
  EXPECT_EQ(payload1, unpickled_string1);
}

#if !(BUILDFLAG(IS_IOS) && BUILDFLAG(USE_BLINK))
TYPED_TEST(ClipboardTest, DataTest) {
  const std::string kFormatString = "web chromium/x-test-format";
  const std::u16string kFormatString16 = u"chromium/x-test-format";
  const std::string payload = "test string";
  base::span<const uint8_t> payload_span(
      reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteData(kFormatString16,
                               mojo_base::BigBuffer(payload_span));
  }

  std::map<std::string, std::string> custom_format_names =
      this->clipboard().ExtractCustomPlatformNames(ClipboardBuffer::kCopyPaste,
                                                   /* data_dst = */ nullptr);
  EXPECT_TRUE(custom_format_names.find(kFormatString) !=
              custom_format_names.end());
  std::string output;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString]),
                             /* data_dst = */ nullptr, &output);

  EXPECT_EQ(payload, output);
}

TYPED_TEST(ClipboardTest, MultipleDataTest) {
  const std::string kFormatString1 = "web chromium/x-test-format1";
  const std::u16string kFormatString116 = u"chromium/x-test-format1";
  const std::string payload1("test string1");
  base::span<const uint8_t> payload_span1(
      reinterpret_cast<const uint8_t*>(payload1.data()), payload1.size());

  const std::string kFormatString2 = "web chromium/x-test-format2";
  const std::u16string kFormatString216 = u"chromium/x-test-format2";
  const std::string payload2("test string2");
  base::span<const uint8_t> payload_span2(
      reinterpret_cast<const uint8_t*>(payload2.data()), payload2.size());

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    // Both payloads should write successfully and not overwrite one another.
    clipboard_writer.WriteData(kFormatString116,
                               mojo_base::BigBuffer(payload_span1));
    clipboard_writer.WriteData(kFormatString216,
                               mojo_base::BigBuffer(payload_span2));
  }

  // Check format 1.
  EXPECT_THAT(this->clipboard().ReadAvailableStandardAndCustomFormatNames(
                  ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr),
              Contains(u"web chromium/x-test-format1"));
  std::string custom_format_json;
  this->clipboard().ReadData(ClipboardFormatType::WebCustomFormatMap(),
                             /* data_dst = */ nullptr, &custom_format_json);
  std::map<std::string, std::string> custom_format_names =
      this->clipboard().ExtractCustomPlatformNames(ClipboardBuffer::kCopyPaste,
                                                   /* data_dst = */ nullptr);
  EXPECT_TRUE(custom_format_names.find(kFormatString1) !=
              custom_format_names.end());
  std::string output1;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString1]),
                             /* data_dst = */ nullptr, &output1);
  EXPECT_EQ(payload1, output1);

  // Check format 2.
  EXPECT_THAT(this->clipboard().ReadAvailableStandardAndCustomFormatNames(
                  ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr),
              Contains(u"web chromium/x-test-format2"));
  EXPECT_TRUE(custom_format_names.find(kFormatString2) !=
              custom_format_names.end());
  std::string output2;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString2]),
                             /* data_dst = */ nullptr, &output2);
  EXPECT_EQ(payload2, output2);
}

TYPED_TEST(ClipboardTest, DataAndPortableFormatTest) {
  const std::string kFormatString1 = "web chromium/x-test-format1";
  const std::u16string kFormatString116 = u"chromium/x-test-format1";
  const std::string payload1("test string1");
  base::span<const uint8_t> payload_span1(
      reinterpret_cast<const uint8_t*>(payload1.data()), payload1.size());

  const std::string kFormatString2 = "web text/plain";
  const std::u16string kFormatString216 = u"text/plain";
  const std::string payload2("test string2");
  base::span<const uint8_t> payload_span2(
      reinterpret_cast<const uint8_t*>(payload2.data()), payload2.size());

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    // Both payloads should write successfully and not overwrite one another.
    clipboard_writer.WriteData(kFormatString116,
                               mojo_base::BigBuffer(payload_span1));
    clipboard_writer.WriteData(kFormatString216,
                               mojo_base::BigBuffer(payload_span2));
  }

  // Check format 1.
  EXPECT_THAT(this->clipboard().ReadAvailableStandardAndCustomFormatNames(
                  ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr),
              Contains(u"web chromium/x-test-format1"));
  std::string custom_format_json;
  this->clipboard().ReadData(ClipboardFormatType::WebCustomFormatMap(),
                             /* data_dst = */ nullptr, &custom_format_json);
  std::map<std::string, std::string> custom_format_names =
      this->clipboard().ExtractCustomPlatformNames(ClipboardBuffer::kCopyPaste,
                                                   /* data_dst = */ nullptr);
  EXPECT_TRUE(custom_format_names.find(kFormatString1) !=
              custom_format_names.end());
  std::string output1;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString1]),
                             /* data_dst = */ nullptr, &output1);
  EXPECT_EQ(payload1, output1);

  // Check format 2.
  EXPECT_THAT(this->clipboard().ReadAvailableStandardAndCustomFormatNames(
                  ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr),
              Contains(u"web text/plain"));
  EXPECT_TRUE(custom_format_names.find(kFormatString2) !=
              custom_format_names.end());
  std::string output2;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString2]),
                             /* data_dst = */ nullptr, &output2);
  EXPECT_EQ(payload2, output2);
}

// Test that platform-specific functionality works, with a predefined format in
// On X11 Linux, this test uses a simple MIME type, text/plain.
// On Windows, this test uses a pre-defined ANSI format, CF_TEXT, and tests that
// the Windows implicitly converts this to UNICODE as expected.
TYPED_TEST(ClipboardTest, PlatformSpecificDataTest) {
  // We're testing platform-specific behavior, so use PlatformClipboardTest.
  // TODO(crbug.com/40692232): The template shouldn't know about its
  // instantiations. Move this information up using a flag, virtual method, or
  // creating separate test files for different platforms.
  std::string test_suite_name = ::testing::UnitTest::GetInstance()
                                    ->current_test_info()
                                    ->test_suite_name();
  if (test_suite_name != std::string("ClipboardTest/PlatformClipboardTest"))
    return;

  const std::string text = "test string";
  const std::string kFormatString = "web text/plain";
  const std::u16string kFormatString16 = u"text/plain";
#if BUILDFLAG(IS_WIN)
  // Windows requires an extra '\0' at the end for a raw write.
  const std::string kPlatformSpecificText = text + '\0';
#else
  const std::string kPlatformSpecificText = text;
#endif
  base::span<const uint8_t> text_span(
      reinterpret_cast<const uint8_t*>(kPlatformSpecificText.data()),
      kPlatformSpecificText.size());
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteData(kFormatString16,
                               mojo_base::BigBuffer(text_span));
  }

  std::string custom_format_json;
  this->clipboard().ReadData(ClipboardFormatType::WebCustomFormatMap(),
                             /* data_dst = */ nullptr, &custom_format_json);
  std::map<std::string, std::string> custom_format_names =
      this->clipboard().ExtractCustomPlatformNames(ClipboardBuffer::kCopyPaste,
                                                   /* data_dst = */ nullptr);

  EXPECT_TRUE(custom_format_names.find(kFormatString) !=
              custom_format_names.end());
  std::string platform_specific_result;
  this->clipboard().ReadData(ClipboardFormatType::CustomPlatformType(
                                 custom_format_names[kFormatString]),
                             /* data_dst = */ nullptr,
                             &platform_specific_result);
  EXPECT_EQ(platform_specific_result, kPlatformSpecificText);
}
#endif

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
TYPED_TEST(ClipboardTest, HyperlinkTest) {
  const std::string kTitle("The <Example> Company's \"home page\"");
  const std::string kUrl("http://www.example.com?x=3&lt=3#\"'<>");
  const std::u16string kExpectedHtml(
      u"<a href=\"http://www.example.com?x=3&amp;lt=3#&quot;&#39;&lt;&gt;\">"
      u"The &lt;Example&gt; Company&#39;s &quot;home page&quot;</a>");

  std::string url_result;
  std::u16string html_result;
  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteHyperlink(ASCIIToUTF16(kTitle), kUrl);
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::HtmlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  uint32_t fragment_start;
  uint32_t fragment_end;
  this->clipboard().ReadHTML(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &html_result,
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
      ClipboardFormatType::WebKitSmartPasteType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
}

#if BUILDFLAG(IS_WIN)  // Windows only tests.
void HtmlTestHelper(const std::string& cf_html,
                    const std::string& expected_html) {
  std::string html;
  clipboard_util::CFHtmlToHtml(cf_html, &html, nullptr);
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

TYPED_TEST(ClipboardTest, PrivacyMetadataTest) {
  // We're testing platform-specific behavior, so use PlatformClipboardTest.
  std::string test_suite_name = ::testing::UnitTest::GetInstance()
                                    ->current_test_info()
                                    ->test_suite_name();
  if (test_suite_name != std::string("ClipboardTest/PlatformClipboardTest")) {
    return;
  }

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"foo");
    clipboard_writer.MarkAsOffTheRecord();
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::ClipboardHistoryType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::UploadCloudClipboardType(),
      ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));
  std::string result;
  this->clipboard().ReadData(ClipboardFormatType::ClipboardHistoryType(),
                             /* data_dst = */ nullptr, &result);
  DWORD history_data = std::strtoul(result.c_str(), nullptr, 16);
  EXPECT_EQ(0ul, history_data);
  this->clipboard().ReadData(ClipboardFormatType::UploadCloudClipboardType(),
                             /* data_dst = */ nullptr, &result);
  DWORD cloud_data = std::strtoul(result.c_str(), nullptr, 16);
  EXPECT_EQ(0ul, cloud_data);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
TYPED_TEST(ClipboardTest, PasswordTest) {
  // We're testing platform-specific behavior, so use PlatformClipboardTest.
  std::string test_suite_name = ::testing::UnitTest::GetInstance()
                                    ->current_test_info()
                                    ->test_suite_name();
  if (test_suite_name != std::string("ClipboardTest/PlatformClipboardTest")) {
    return;
  }

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteText(u"password");
    clipboard_writer.MarkAsConfidential();
  }

  EXPECT_TRUE(this->clipboard().IsMarkedByOriginatorAsConfidential());
}
#endif  // BUILDFLAG(IS_MAC)

// Test writing all formats we have simultaneously.
TYPED_TEST(ClipboardTest, WriteEverything) {
  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"foo");
    writer.WriteHTML(u"foo", "bar");
    writer.WriteBookmark(u"foo", "bar");
    writer.WriteHyperlink(u"foo", "bar");
    writer.WriteWebSmartPaste();
    // Left out: WriteFile, WriteFiles, WriteBitmapFromPixels, WritePickledData.
  }

  // Passes if we don't crash.
}

// TODO(dcheng): Fix this test for Android. It's rather involved, since the
// clipboard change listener is posted to the Java message loop, and spinning
// that loop from C++ to trigger the callback in the test requires a non-trivial
// amount of additional work.
#if !BUILDFLAG(IS_ANDROID)
// Simple test that the sequence number appears to change when the clipboard is
// written to.
// TODO(dcheng): Add a version to test ClipboardBuffer::kSelection.
TYPED_TEST(ClipboardTest, GetSequenceNumber) {
  const ClipboardSequenceNumberToken first_sequence_number =
      this->clipboard().GetSequenceNumber(ClipboardBuffer::kCopyPaste);

  {
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"World");
  }

  // On some platforms, the sequence number is updated by a UI callback so pump
  // the message loop to make sure we get the notification.
  base::RunLoop().RunUntilIdle();

  const ClipboardSequenceNumberToken second_sequence_number =
      this->clipboard().GetSequenceNumber(ClipboardBuffer::kCopyPaste);

  EXPECT_NE(first_sequence_number, second_sequence_number);
}
#endif

// Test that writing empty parameters doesn't try to dereference an empty data
// vector. Not crashing = passing.
TYPED_TEST(ClipboardTest, WriteTextEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteText(std::u16string());
}

TYPED_TEST(ClipboardTest, WriteHTMLEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteHTML(std::u16string(), std::string());
}

TYPED_TEST(ClipboardTest, EmptySvgTest) {
  ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteSvg(std::u16string());
}

TYPED_TEST(ClipboardTest, WriteRTFEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteRTF(std::string());
}

TYPED_TEST(ClipboardTest, WriteBookmarkEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteBookmark(std::u16string(), std::string());
}

TYPED_TEST(ClipboardTest, WriteHyperlinkEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteHyperlink(std::u16string(), std::string());
}

TYPED_TEST(ClipboardTest, WritePickledData) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WritePickledData(base::Pickle(), ClipboardFormatType::PlainTextType());
}

TYPED_TEST(ClipboardTest, WriteImageEmptyParams) {
  ScopedClipboardWriter scw(ClipboardBuffer::kCopyPaste);
  scw.WriteImage(SkBitmap());
}

#if (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))
TYPED_TEST(ClipboardTest, BookmarkTestWithoutTitle) {
  // We're testing platform-specific behavior, so use PlatformClipboardTest.
  std::string test_suite_name = ::testing::UnitTest::GetInstance()
                                    ->current_test_info()
                                    ->test_suite_name();
  if (test_suite_name != std::string("ClipboardTest/PlatformClipboardTest")) {
    return;
  }

  std::u16string title_result;
  std::string url("http://www.example.com/"), url_result;

  {
    ScopedClipboardWriter clipboard_writer(ClipboardBuffer::kCopyPaste);
    clipboard_writer.WriteBookmark(std::u16string(), url);
  }

  EXPECT_TRUE(this->clipboard().IsFormatAvailable(
      ClipboardFormatType::UrlType(), ClipboardBuffer::kCopyPaste,
      /* data_dst = */ nullptr));

  this->clipboard().ReadBookmark(/* data_dst = */ nullptr, &title_result,
                                 &url_result);
  EXPECT_EQ(url, url_result);
}
#endif  //(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID))

// Policy controller is only intended to be used in Chrome OS, so the following
// policy related tests are only run on Chrome OS.
#if BUILDFLAG(IS_CHROMEOS)
// Test that copy/paste would work normally if the policy controller didn't
// restrict the clipboard data.
TYPED_TEST(ClipboardTest, PolicyAllowDataRead) {
  auto policy_controller = std::make_unique<MockPolicyController>();
  const std::u16string kTestText(u"World");
  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(GURL("https://www.google.com")));
    writer.WriteText(kTestText);
  }
  EXPECT_CALL(*policy_controller, IsClipboardReadAllowed)
      .WillRepeatedly(testing::Return(true));
  std::u16string read_result;
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &read_result);
  EXPECT_EQ(kTestText, read_result);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Checks that the clipboard source metadata is encoded in the
  // DataTransferEndpoint mime type.
  std::string actual_json;
  this->clipboard().ReadData(
      ui::ClipboardFormatType::DataTransferEndpointDataType(),
      /* data_dst = */ nullptr, &actual_json);

  EXPECT_EQ(
      "{\"endpoint_type\":\"url\","
      "\"off_the_record\":false,"
      "\"url\":\"https://www.google.com/\"}",
      actual_json);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  ::testing::Mock::VerifyAndClearExpectations(policy_controller.get());
}

// Test that pasting clipboard data would not work if the policy controller
// restricted it.
TYPED_TEST(ClipboardTest, PolicyDisallow_ReadText) {
  auto policy_controller = std::make_unique<MockPolicyController>();
  const std::u16string kTestText(u"World");
  {
    ScopedClipboardWriter writer(
        ClipboardBuffer::kCopyPaste,
        std::make_unique<DataTransferEndpoint>(GURL("https://google.com/")));
    writer.WriteText(kTestText);
  }
  EXPECT_CALL(*policy_controller, IsClipboardReadAllowed)
      .WillRepeatedly(testing::Return(false));
  std::u16string read_result;
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst = */ nullptr, &read_result);
  ::testing::Mock::VerifyAndClearExpectations(policy_controller.get());
  EXPECT_EQ(std::u16string(), read_result);
}

TYPED_TEST(ClipboardTest, PolicyDisallow_ReadPng) {
  auto policy_controller = std::make_unique<MockPolicyController>();
  constexpr U8x4 kBitMapData[4 * 3] = {
      {0x26, 0x16, 0x06, 0x46}, {0x88, 0x59, 0x9f, 0xf6},
      {0x37, 0x29, 0x3f, 0x79}, {0x86, 0xb9, 0x55, 0xfa},
      {0x52, 0x21, 0x77, 0x78}, {0x30, 0x2a, 0x69, 0x87},
      {0x25, 0x2a, 0x32, 0x36}, {0x1b, 0x40, 0x20, 0x43},
      {0x21, 0x8c, 0x84, 0x91}, {0x3c, 0x7b, 0x17, 0xc3},
      {0x5c, 0x15, 0x46, 0x69}, {0x52, 0x19, 0x17, 0x64},
  };
  WriteBitmap(&this->clipboard(), SkImageInfo::MakeN32Premul(4, 3),
              reinterpret_cast<const void*>(kBitMapData));
  EXPECT_CALL(*policy_controller, IsClipboardReadAllowed)
      .WillRepeatedly(testing::Return(false));
  std::vector<uint8_t> image = clipboard_test_util::ReadPng(&this->clipboard());
  ::testing::Mock::VerifyAndClearExpectations(policy_controller.get());
  EXPECT_EQ(true, image.empty());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Checks that source DTEs provided through the custom MIME type can be parsed.
TYPED_TEST(ClipboardTest, ClipboardSourceDteCanBeRetrievedByLacros) {
  auto policy_controller = std::make_unique<MockPolicyController>();
  const std::u16string kTestText(u"World");
  const std::string kDteJson(
      R"({"endpoint_type":"url","url":"https://www.google.com"})");
  {
    // No source DTE provided directly to the Lacros clipboard.
    ScopedClipboardWriter writer(ClipboardBuffer::kCopyPaste);
    writer.WriteText(kTestText);
    // Encoded source DTE provided in custom MIME type.
    writer.WriteEncodedDataTransferEndpointForTesting(kDteJson);
  }

  EXPECT_CALL(
      *policy_controller,
      IsClipboardReadAllowed(
          Property(
              &base::optional_ref<const DataTransferEndpoint>::value,
              AllOf(Property(&DataTransferEndpoint::IsUrlType, true),
                    Property(&DataTransferEndpoint::GetURL,
                             Pointee(Property(&GURL::spec,
                                              "https://www.google.com/"))))),
          _, _))
      .WillRepeatedly(testing::Return(true));

  std::u16string read_result;
  this->clipboard().ReadText(ClipboardBuffer::kCopyPaste,
                             /* data_dst= */ nullptr, &read_result);
  EXPECT_EQ(kTestText, read_result);

  ::testing::Mock::VerifyAndClearExpectations(policy_controller.get());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_TEST_TEMPLATE_H_
