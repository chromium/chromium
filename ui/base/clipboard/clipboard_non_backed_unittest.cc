// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <memory>
#include <string>
#include <vector>

#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

#if BUILDFLAG(IS_OZONE)
#include "base/command_line.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace ui {
namespace {

std::vector<std::string> UTF8Types(std::vector<std::u16string> types) {
  std::vector<std::string> result;
  for (const std::u16string& type : types)
    result.push_back(base::UTF16ToUTF8(type));
  return result;
}

}  // namespace

// Base class for tests of `ClipboardNonBacked`.
class ClipboardNonBackedTestBase : public testing::Test {
 public:
  explicit ClipboardNonBackedTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          time_source) {}
  void SetUp() override {
#if BUILDFLAG(IS_OZONE)
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(::switches::kOzonePlatform,
                                    switches::kHeadless);
    ui::OzonePlatform::PreEarlyInitialization();
#endif  // BUILDFLAG(IS_OZONE)

    // Clipboard needs to be instantiated after Ozone is initialized so that
    // ui::Clipboard::Create() can call ui::OzonePlatform::GetInstance().
    std::unique_ptr<Clipboard> clipboard(new ClipboardNonBacked());
    Clipboard::SetClipboardForCurrentThread(std::move(clipboard));
  }

  void TearDown() override { Clipboard::DestroyClipboardForCurrentThread(); }

  ClipboardNonBacked* clipboard() {
    return ClipboardNonBacked::GetForCurrentThread();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Base class for tests of `ClipboardNonBacked` which use system time.
class ClipboardNonBackedTest : public ClipboardNonBackedTestBase {
 public:
  ClipboardNonBackedTest()
      : ClipboardNonBackedTestBase(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
};

// Verifies that GetClipboardData() returns the same instance of ClipboardData
// as was written via WriteClipboardData().
TEST_F(ClipboardNonBackedTest, WriteAndGetClipboardData) {
  auto clipboard_data = std::make_unique<ClipboardData>();

  auto* expected_clipboard_data_ptr = clipboard_data.get();
  clipboard()->WriteClipboardData(std::move(clipboard_data));
  auto* actual_clipboard_data_ptr = clipboard()->GetClipboardData(nullptr);

  EXPECT_EQ(expected_clipboard_data_ptr, actual_clipboard_data_ptr);
}

// Verifies that WriteClipboardData() writes a ClipboardData instance to the
// clipboard and returns the previous instance.
TEST_F(ClipboardNonBackedTest, WriteClipboardData) {
  auto first_data = std::make_unique<ClipboardData>();
  auto second_data = std::make_unique<ClipboardData>();

  auto* first_data_ptr = first_data.get();
  auto* second_data_ptr = second_data.get();

  auto previous_data = clipboard()->WriteClipboardData(std::move(first_data));
  EXPECT_EQ(previous_data.get(), nullptr);

  previous_data = clipboard()->WriteClipboardData(std::move(second_data));

  EXPECT_EQ(first_data_ptr, previous_data.get());
  EXPECT_EQ(second_data_ptr, clipboard()->GetClipboardData(nullptr));
}

// Verifies that directly writing to ClipboardInternal does not result in
// histograms being logged. This is used by ClipboardHistoryController to
// manipulate the clipboard in order to facilitate pasting from clipboard
// history.
TEST_F(ClipboardNonBackedTest, AdminWriteDoesNotRecordHistograms) {
  base::HistogramTester histogram_tester;
  auto data = std::make_unique<ClipboardData>();
  data->set_text("test");

  auto* data_ptr = data.get();

  // Write the data to the clipboard, no histograms should be recorded.
  clipboard()->WriteClipboardData(std::move(data));
  EXPECT_EQ(data_ptr, clipboard()->GetClipboardData(/*data_dst=*/nullptr));

  histogram_tester.ExpectTotalCount("Clipboard.Read", 0);
  histogram_tester.ExpectTotalCount("Clipboard.Write", 0);
}

// Tests that text data uses 'text/plain' mime type.
TEST_F(ClipboardNonBackedTest, PlainText) {
  auto data = std::make_unique<ClipboardData>();
  data->set_text("hello");
  clipboard()->WriteClipboardData(std::move(data));
  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);

  // Text data uses mime type 'text/plain'.
  EXPECT_EQ(std::vector<std::string>({"text/plain"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  // Validate reading back the text.
  std::u16string text;
  clipboard()->ReadText(ClipboardBuffer::kCopyPaste, /*dat_dist=*/nullptr,
                        &text);
  EXPECT_EQ(u"hello", text);
}

// Tests that site bookmark URLs are accessed as text, and
// IsFormatAvailable('text/uri-list') is only true for files.
TEST_F(ClipboardNonBackedTest, BookmarkURL) {
  auto data = std::make_unique<ClipboardData>();
  data->set_bookmark_title("Example Page");
  data->set_bookmark_url("http://example.com");
  clipboard()->WriteClipboardData(std::move(data));
  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);

  // Bookmark data returns available type 'text/plain'.
  EXPECT_EQ(std::vector<std::string>({"text/plain"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(ClipboardFormatType::UrlType(),
                                             ClipboardBuffer::kCopyPaste,
                                             /*data_dst=*/nullptr));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::PlainTextType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::FilenamesType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  // Validate reading back the bookmark.
  std::u16string title;
  std::string url;
  clipboard()->ReadBookmark(/*dat_dist=*/nullptr, &title, &url);
  EXPECT_EQ(u"Example Page", title);
  EXPECT_EQ("http://example.com", url);
  std::u16string text;
  clipboard()->ReadText(ClipboardBuffer::kCopyPaste, /*dat_dist=*/nullptr,
                        &text);
  EXPECT_EQ(u"http://example.com", text);
}

// Filenames data uses mime type 'text/uri-list'.
TEST_F(ClipboardNonBackedTest, TextURIList) {
  auto data = std::make_unique<ClipboardData>();
  data->set_filenames(
      {FileInfo(base::FilePath(FILE_PATH_LITERAL("/path")), base::FilePath())});
  clipboard()->WriteClipboardData(std::move(data));
  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"text/uri-list"}), UTF8Types(types));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(ClipboardFormatType::UrlType(),
                                              ClipboardBuffer::kCopyPaste,
                                              /*data_dst=*/nullptr));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::FilenamesType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  // Filenames data uses mime type 'text/uri-list', but clients can also set
  // 'text/uri-list' as one of the custom data types. When it is set as a custom
  // type, it will be returned from ReadAvailableTypes(), but
  // IsFormatAvailable(FilenamesType()) is false.
  data = std::make_unique<ClipboardData>();
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/uri-list"] = u"data";
  base::Pickle pickle;
  ui::WriteCustomDataToPickle(custom_data, &pickle);
  data->SetCustomData(ui::ClipboardFormatType::DataTransferCustomType(),
                      std::string(pickle.data_as_char(), pickle.size()));
  clipboard()->WriteClipboardData(std::move(data));
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"text/uri-list"}), UTF8Types(types));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ClipboardFormatType::FilenamesType(), ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
}

// Tests that bitmaps written to the clipboard are read out encoded as a PNG.
TEST_F(ClipboardNonBackedTest, ImageEncoding) {
  auto data = std::make_unique<ClipboardData>();
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  data->SetBitmapData(test_bitmap);
  clipboard()->WriteClipboardData(std::move(data));

  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"image/png"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(ClipboardFormatType::PngType(),
                                             ClipboardBuffer::kCopyPaste,
                                             /*data_dst=*/nullptr));

  // Asynchronously read out the image as a PNG. It should be the encoded
  // version of the bitmap we wrote above.
  std::vector<uint8_t> png;
  base::RunLoop loop;
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        png = png_data;
        loop.Quit();
      }));
  loop.Run();

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(png.data(), png.size(), &bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, test_bitmap));
}

// Tests that consecutive calls to read an image from the clipboard only results
// in the image being encoded once.
TEST_F(ClipboardNonBackedTest, EncodeImageOnce) {
  auto data = std::make_unique<ClipboardData>();
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  data->SetBitmapData(test_bitmap);
  clipboard()->WriteClipboardData(std::move(data));

  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"image/png"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(ClipboardFormatType::PngType(),
                                             ClipboardBuffer::kCopyPaste,
                                             /*data_dst=*/nullptr));

  std::vector<std::vector<uint8_t>> pngs;
  base::RunLoop loop;
  // Read from the clipboard many times in a row.
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
      }));
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
      }));
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
        // Read operations should be ordered. This callback will be called last.
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(pngs.size(), 3u);
  EXPECT_EQ(pngs[0], pngs[1]);
  EXPECT_EQ(pngs[0], pngs[2]);

  // The bitmap should only have been encoded once.
  EXPECT_EQ(clipboard()->NumImagesEncodedForTesting(), 1);

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(pngs[0].data(), pngs[0].size(), &bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, test_bitmap));
}

// Tests that consecutive calls to read an image from the clipboard only results
// in the image being encoded once, but if another image is placed on the
// clipboard, this image is appropriately encoded.
TEST_F(ClipboardNonBackedTest, EncodeMultipleImages) {
  auto data = std::make_unique<ClipboardData>();
  SkBitmap test_bitmap = gfx::test::CreateBitmap(3, 2);
  data->SetBitmapData(test_bitmap);

  auto data2 = std::make_unique<ClipboardData>();
  SkBitmap test_bitmap2 = gfx::test::CreateBitmap(4, 3);
  data2->SetBitmapData(test_bitmap2);

  clipboard()->WriteClipboardData(std::move(data));

  std::vector<std::u16string> types;
  clipboard()->ReadAvailableTypes(ClipboardBuffer::kCopyPaste,
                                  /*data_dst=*/nullptr, &types);
  EXPECT_EQ(std::vector<std::string>({"image/png"}), UTF8Types(types));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(ClipboardFormatType::PngType(),
                                             ClipboardBuffer::kCopyPaste,
                                             /*data_dst=*/nullptr));

  std::vector<std::vector<uint8_t>> pngs;
  base::RunLoop loop;
  // Read from the clipboard many times in a row.
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
      }));
  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
      }));

  // Write a different image to the clipboard.
  clipboard()->WriteClipboardData(std::move(data2));

  clipboard()->ReadPng(
      ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr,
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& png_data) {
        pngs.emplace_back(png_data);
        // Read operations should be ordered. This callback will be called last.
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(pngs.size(), 3u);
  EXPECT_EQ(pngs[0], pngs[1]);
  EXPECT_NE(pngs[0], pngs[2]);

  // The first bitmap should only have been encoded once, but the second bitmap
  // should have been encoded separately.
  EXPECT_EQ(clipboard()->NumImagesEncodedForTesting(), 2);

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(pngs[0].data(), pngs[0].size(), &bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, test_bitmap));
  gfx::PNGCodec::Decode(pngs[2].data(), pngs[2].size(), &bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, test_bitmap2));
}

// Tests that different clipboard buffers have independent data.
TEST_F(ClipboardNonBackedTest, ClipboardBufferTypes) {
  static const struct ClipboardBufferInfo {
    ui::ClipboardBuffer buffer;
    std::u16string paste_text;
  } clipboard_buffers[] = {
      {ui::ClipboardBuffer::kCopyPaste, u"kCopyPaste"},
      {ui::ClipboardBuffer::kSelection, u"kSelection"},
      {ui::ClipboardBuffer::kDrag, u"kDrag"},
  };

  // Check basic write/read ops into each buffer type.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (ui::Clipboard::IsSupportedClipboardBuffer(buffer)) {
      ScopedClipboardWriter(buffer).WriteText(paste_text);

      std::u16string copy_text;
      clipboard()->ReadText(buffer, /*data_dst=*/nullptr, &copy_text);
      EXPECT_EQ(paste_text, copy_text);
    }
  }

  // Verify that different clipboard buffer data is independent.
  for (const auto& [buffer, paste_text] : clipboard_buffers) {
    if (ui::Clipboard::IsSupportedClipboardBuffer(buffer)) {
      std::u16string copy_text;
      clipboard()->ReadText(buffer, /*data_dst=*/nullptr, &copy_text);
      EXPECT_EQ(paste_text, copy_text);
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// Base class for tests of `ClipboardNonBacked` which use mock time.
class ClipboardNonBackedMockTimeTest : public ClipboardNonBackedTestBase {
 public:
  ClipboardNonBackedMockTimeTest()
      : ClipboardNonBackedTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Verifies that `ClipboardNonBacked` records the time interval between commit
// and read of the same `ClipboardData`.
TEST_F(ClipboardNonBackedMockTimeTest,
       RecordsTimeIntervalBetweenCommitAndRead) {
  // Cache clipboard for the current thread.
  auto* clipboard = ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);

  // Write clipboard data to the clipboard.
  ScopedClipboardWriter(ClipboardBuffer::kCopyPaste).WriteText(u"");
  auto* clipboard_data = clipboard->GetClipboardData(/*data_dst=*/nullptr);
  ASSERT_TRUE(clipboard_data);
  ASSERT_TRUE(clipboard_data->commit_time().has_value());
  EXPECT_EQ(clipboard_data->commit_time().value(), base::Time::Now());

  // This test will verify expectations for every kind of clipboard data read.
  std::vector<base::RepeatingClosure> test_cases = {{
      base::BindLambdaForTesting([&]() {
        clipboard->ReadAsciiText(
            ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
            /*result=*/std::make_unique<std::string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadBookmark(
            /*data_dst=*/nullptr,
            /*title=*/std::make_unique<std::u16string>().get(),
            /*url=*/std::make_unique<std::string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadDataTransferCustomData(
            ClipboardBuffer::kCopyPaste,
            /*type=*/std::u16string(), /*data_dst=*/nullptr,
            /*result=*/std::make_unique<std::u16string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadData(ui::ClipboardFormatType(),
                            /*data_dst=*/nullptr,
                            /*result=*/std::make_unique<std::string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadFilenames(
            ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
            /*result=*/std::make_unique<std::vector<ui::FileInfo>>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadHTML(
            ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
            /*markup=*/std::make_unique<std::u16string>().get(),
            /*src_url=*/std::make_unique<std::string>().get(),
            /*fragment_start=*/std::make_unique<uint32_t>().get(),
            /*fragment_end=*/std::make_unique<uint32_t>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadPng(ClipboardBuffer::kCopyPaste,
                           /*data_dst=*/nullptr,
                           /*callback=*/base::DoNothing());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadRTF(ClipboardBuffer::kCopyPaste,
                           /*data_dst=*/nullptr,
                           /*result=*/std::make_unique<std::string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadSvg(ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
                           /*result=*/std::make_unique<std::u16string>().get());
      }),
      base::BindLambdaForTesting([&]() {
        clipboard->ReadText(
            ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
            /*result=*/std::make_unique<std::u16string>().get());
      }),
  }};

  // Read clipboard data and verify histogram expectations.
  constexpr base::TimeDelta kTimeDelta = base::Seconds(10);
  constexpr char kHistogram[] = "Clipboard.TimeIntervalBetweenCommitAndRead";
  for (size_t i = 1u; i <= test_cases.size(); ++i) {
    base::HistogramTester histogram_tester;
    histogram_tester.ExpectUniqueTimeSample(kHistogram, i * kTimeDelta, 0u);
    task_environment_.FastForwardBy(kTimeDelta);
    test_cases.at(i - 1u).Run();
    histogram_tester.ExpectUniqueTimeSample(kHistogram, i * kTimeDelta, 1u);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ui
