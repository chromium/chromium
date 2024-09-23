// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/events/platform/platform_event_source.h"
#include "url/gurl.h"

namespace ui {

class OSExchangeDataTest : public PlatformTest {
 public:
  OSExchangeDataTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        event_source_(PlatformEventSource::CreateDefault()) {}

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PlatformEventSource> event_source_;
};

TEST_F(OSExchangeDataTest, StringDataGetAndSet) {
  const OSExchangeData copy([] {
    OSExchangeData data;
    EXPECT_FALSE(data.HasString());
    data.SetString(u"I can has cheezburger?");
    EXPECT_TRUE(data.HasString());
    return data.provider().Clone();
  }());

  EXPECT_TRUE(copy.HasString());
  std::optional<std::u16string> string = copy.GetString();
  EXPECT_EQ(u"I can has cheezburger?", string);
  std::optional<OSExchangeData::UrlInfo> url_info =
      copy.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  // No URLs in `data` so no URLs should be read out.
  EXPECT_FALSE(url_info.has_value());
}

TEST_F(OSExchangeDataTest, TestURLExchangeFormats) {
  const OSExchangeData copy([] {
    OSExchangeData data;
    EXPECT_FALSE(data.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
    data.SetURL(GURL("https://www.google.com/"), u"www.google.com");
    EXPECT_TRUE(data.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
    return data.provider().Clone();
  }());

  // URL spec and title should match
  EXPECT_TRUE(copy.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
  std::optional<OSExchangeData::UrlInfo> url_info =
      copy.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  EXPECT_TRUE(url_info.has_value());
  EXPECT_EQ("https://www.google.com/", url_info->url.spec());
  EXPECT_EQ(u"www.google.com", url_info->title);

  // URL should be the raw text response, even though no text was explicitly
  // set.
  std::optional<std::u16string> string = copy.GetString();
  EXPECT_EQ(u"https://www.google.com/", string);
}

// Test that setting the URL does not overwrite a previously set custom string
// and that the synthesized URL shortcut file is ignored by GetFileContents().
TEST_F(OSExchangeDataTest, URLStringFileContents) {
  const OSExchangeData copy([] {
    OSExchangeData data;
    data.SetString(u"I can has cheezburger?");
    data.SetURL(GURL("https://www.google.com/"), u"www.google.com");
    return data.provider().Clone();
  }());

  std::optional<std::u16string> string = copy.GetString();
  EXPECT_EQ(u"I can has cheezburger?", string);

  std::optional<OSExchangeData::UrlInfo> url_info =
      copy.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  EXPECT_TRUE(url_info.has_value());
  EXPECT_EQ("https://www.google.com/", url_info->url.spec());
  EXPECT_EQ(u"www.google.com", url_info->title);

  // HasFileContents() should be false, and GetFileContents() should be empty
  // (https://crbug.com/1274395).
  EXPECT_FALSE(copy.HasFileContents());
  std::optional<OSExchangeData::FileContentsInfo> file_contents =
      copy.GetFileContents();
  EXPECT_FALSE(file_contents.has_value());
}

TEST_F(OSExchangeDataTest, TestFileToURLConversion) {
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&file_path));

  const OSExchangeData copy([&] {
    OSExchangeData data;
    EXPECT_FALSE(data.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
    EXPECT_FALSE(data.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
    EXPECT_FALSE(data.HasFile());

    data.SetFilename(file_path);

    return data.provider().Clone();
  }());

  {
    EXPECT_FALSE(copy.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
    std::optional<OSExchangeData::UrlInfo> no_converted_filenames_url_info =
        copy.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
    EXPECT_FALSE(no_converted_filenames_url_info.has_value());
  }

  {
    EXPECT_TRUE(copy.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
    std::optional<OSExchangeData::UrlInfo> converted_url_info =
        copy.GetURLAndTitle(FilenameToURLPolicy::CONVERT_FILENAMES);
    ASSERT_TRUE(converted_url_info.has_value());
    EXPECT_EQ(net::FilePathToFileURL(file_path), converted_url_info->url);
    EXPECT_EQ(std::u16string(), converted_url_info->title);
  }
  EXPECT_TRUE(copy.HasFile());
  std::optional<std::vector<FileInfo>> actual_files = copy.GetFilenames();
  ASSERT_TRUE(actual_files.has_value());
  EXPECT_EQ(1u, actual_files.value().size());
  EXPECT_EQ(file_path, actual_files.value()[0].path);
}

TEST_F(OSExchangeDataTest, TestPickledData) {
  const ClipboardFormatType kTestFormat =
      ClipboardFormatType::GetType("application/vnd.chromium.test");

  const OSExchangeData copy([&] {
    OSExchangeData data;
    base::Pickle saved_pickle;
    saved_pickle.WriteInt(1);
    saved_pickle.WriteInt(2);
    data.SetPickledData(kTestFormat, saved_pickle);
    return data.provider().Clone();
  }());

  EXPECT_TRUE(copy.HasCustomFormat(kTestFormat));

  std::optional<base::Pickle> restored_pickle =
      copy.GetPickledData(kTestFormat);
  ASSERT_TRUE(restored_pickle.has_value());
  base::PickleIterator iterator(restored_pickle.value());
  int value;
  EXPECT_TRUE(iterator.ReadInt(&value));
  EXPECT_EQ(1, value);
  EXPECT_TRUE(iterator.ReadInt(&value));
  EXPECT_EQ(2, value);
}

TEST_F(OSExchangeDataTest, TestFilenames) {
#if BUILDFLAG(IS_WIN)
  const std::vector<FileInfo> kTestFilenames = {
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file1")),
       base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("C:\\tmp\\test_file2")),
       base::FilePath()},
  };
#else
  const std::vector<FileInfo> kTestFilenames = {
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file1")), base::FilePath()},
      {base::FilePath(FILE_PATH_LITERAL("/tmp/test_file2")), base::FilePath()},
  };
#endif

  const OSExchangeData copy([&] {
    OSExchangeData data;
    data.SetFilenames(kTestFilenames);
    return data.provider().Clone();
  }());

  std::optional<std::vector<FileInfo>> dropped_filenames = copy.GetFilenames();
  ASSERT_TRUE(dropped_filenames.has_value());
  // Only check the paths, not the display names, as those might be synthesized
  // during the clone while reading from the clipboard.
  ASSERT_EQ(kTestFilenames.size(), dropped_filenames.value().size());
  for (size_t i = 0; i < kTestFilenames.size(); ++i) {
    EXPECT_EQ(kTestFilenames[i].path, dropped_filenames.value()[i].path);
  }
}

#if defined(USE_AURA)
TEST_F(OSExchangeDataTest, TestHTML) {
  const GURL url("http://www.google.com/");
  const std::u16string html =
      u"<HTML>\n<BODY>\n"
      u"<b>bold.</b> <i><b>This is bold italic.</b></i>\n"
      u"</BODY>\n</HTML>";

  const OSExchangeData copy([&] {
    OSExchangeData data;
    data.SetHtml(html, url);
    return data.provider().Clone();
  }());

  EXPECT_TRUE(copy.HasHtml());
  std::optional<ui::OSExchangeData::HtmlInfo> html_content = copy.GetHtml();
  ASSERT_TRUE(html_content.has_value());
  EXPECT_EQ(html, html_content->html);
  EXPECT_EQ(url, html_content->base_url);
}
#endif

TEST_F(OSExchangeDataTest, NotRendererTainted) {
  const OSExchangeData copy([&] {
    OSExchangeData data;
    return data.provider().Clone();
  }());

  EXPECT_FALSE(copy.IsRendererTainted());
  EXPECT_EQ(std::nullopt, copy.GetRendererTaintedOrigin());
}

TEST_F(OSExchangeDataTest, RendererTaintedOpaqueOrigin) {
  const url::Origin tuple_origin =
      url::Origin::Create(GURL("https://www.google.com/"));
  const url::Origin opaque_origin = tuple_origin.DeriveNewOpaqueOrigin();
  ASSERT_TRUE(opaque_origin.opaque());

  const OSExchangeData copy([&] {
    OSExchangeData data;
    data.MarkRendererTaintedFromOrigin(opaque_origin);
    return data.provider().Clone();
  }());

  EXPECT_TRUE(copy.IsRendererTainted());
  std::optional<url::Origin> origin = copy.GetRendererTaintedOrigin();
  EXPECT_TRUE(origin.has_value());
  EXPECT_TRUE(origin->opaque());
  // Currently, the actual value of an opaque origin is not actually serialized
  // into OSExchangeData, so expect a random opaque origin to be read out.
  EXPECT_NE(opaque_origin, origin);
  // And there should be no precursor tuple.
  EXPECT_FALSE(origin->GetTupleOrPrecursorTupleIfOpaque().IsValid());
}

TEST_F(OSExchangeDataTest, RendererTaintedTupleOrigin) {
  const url::Origin tuple_origin =
      url::Origin::Create(GURL("https://www.google.com/"));

  const OSExchangeData copy([&] {
    OSExchangeData data;
    data.MarkRendererTaintedFromOrigin(tuple_origin);
    return data.provider().Clone();
  }());

  EXPECT_TRUE(copy.IsRendererTainted());
  std::optional<url::Origin> origin = copy.GetRendererTaintedOrigin();
  EXPECT_TRUE(origin.has_value());
  EXPECT_EQ(tuple_origin, origin);
}

}  // namespace ui
