// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_url_info.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "url/gurl.h"

namespace ui {

namespace {

template <typename StringType>
PlatformClipboard::Data ToClipboardData(const StringType& data_string) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      base::as_byte_span(data_string));
}
}  // namespace

class WaylandExchangeDataProviderTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// Regression test for https://crbug.com/1284996.
TEST_F(WaylandExchangeDataProviderTest, ExtractPickledData) {
  WaylandExchangeDataProvider provider;
  std::string extracted;

  EXPECT_FALSE(provider.ExtractData(kMimeTypePlainText, &extracted));
  EXPECT_FALSE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));

  extracted.clear();
  provider.SetString(u"dnd-string");
  EXPECT_TRUE(provider.ExtractData(kMimeTypePlainText, &extracted));
  EXPECT_EQ("dnd-string", extracted);

  extracted.clear();
  base::Pickle pickle;
  pickle.WriteString("pickled-str");
  provider.SetPickledData(ClipboardFormatType::DataTransferCustomType(),
                          pickle);
  EXPECT_TRUE(
      provider.ExtractData(kMimeTypeDataTransferCustomData, &extracted));

  // Ensure Pickle "reconstruction" works as expected.
  std::string read_pickled_str;
  base::Pickle read_pickle =
      base::Pickle::WithData(base::as_byte_span(extracted));
  base::PickleIterator iter(read_pickle);
  ASSERT_TRUE(read_pickle.data());
  EXPECT_FALSE(iter.ReachedEnd());
  EXPECT_TRUE(iter.ReadString(&read_pickled_str));
  EXPECT_EQ("pickled-str", read_pickled_str);
}

TEST_F(WaylandExchangeDataProviderTest, FileNameAsUriList) {
  WaylandExchangeDataProvider provider;
  std::string extracted;
  EXPECT_FALSE(provider.ExtractData(kMimeTypeUriList, &extracted));

  extracted.clear();
  provider.AddData(ToClipboardData("file:///dev/null"), kMimeTypeUriList);
  EXPECT_TRUE(provider.ExtractData(kMimeTypeUriList, &extracted));
  EXPECT_EQ("file:///dev/null", extracted);
}

// text/uri-list holds CRLF-separated URIs only, without titles. File-only
// providers offer text/uri-list and not text/x-moz-url.
TEST_F(WaylandExchangeDataProviderTest, ExtractUriList) {
  WaylandExchangeDataProvider provider;
  provider.SetFilenames({FileInfo(base::FilePath("/tmp/a"), base::FilePath()),
                         FileInfo(base::FilePath("/tmp/b"), base::FilePath())});
  EXPECT_THAT(provider.BuildMimeTypesList(),
              ::testing::AllOf(
                  ::testing::Contains(kMimeTypeUriList),
                  ::testing::Not(::testing::Contains(kMimeTypeMozillaUrl))));
  std::string extracted;
  EXPECT_FALSE(provider.ExtractData(kMimeTypeMozillaUrl, &extracted));
  EXPECT_TRUE(provider.ExtractData(kMimeTypeUriList, &extracted));
  EXPECT_EQ("file:///tmp/a\r\nfile:///tmp/b", extracted);

  WaylandExchangeDataProvider url_provider;
  const ClipboardUrlInfo url_info(GURL("https://example.com/"), u"Title");
  url_provider.SetURLs(base::span_from_ref(url_info));
  extracted.clear();
  EXPECT_TRUE(url_provider.ExtractData(kMimeTypeUriList, &extracted));
  EXPECT_EQ("https://example.com/", extracted);
}

// text/x-moz-url is UTF-16 encoded "URL\nTitle", which is what other
// applications (and AddData()) expect.  Regression test for
// https://crbug.com/572987944.
TEST_F(WaylandExchangeDataProviderTest, ExtractMozUrl) {
  WaylandExchangeDataProvider provider;
  std::string extracted;
  EXPECT_FALSE(provider.ExtractData(kMimeTypeMozillaUrl, &extracted));

  const ClipboardUrlInfo url_info(GURL("https://example.com/"),
                                  u"Title \u00e9");
  provider.SetURLs(base::span_from_ref(url_info));
  EXPECT_THAT(provider.BuildMimeTypesList(),
              ::testing::Contains(kMimeTypeMozillaUrl));
  EXPECT_TRUE(provider.ExtractData(kMimeTypeMozillaUrl, &extracted));
  const std::u16string kExpected = u"https://example.com/\nTitle \u00e9";
  EXPECT_EQ(base::as_string_view(base::as_byte_span(kExpected)), extracted);

  // Round trip through the drop side.
  WaylandExchangeDataProvider dropped;
  dropped.AddData(ToClipboardData(extracted), kMimeTypeMozillaUrl);
  const std::vector<ClipboardUrlInfo> url_infos =
      dropped.GetURLs(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  ASSERT_EQ(1U, url_infos.size());
  EXPECT_EQ(GURL("https://example.com/"), url_infos[0].url);
  EXPECT_EQ(u"Title \u00e9", url_infos[0].title);

  // An empty title produces "URL\n", which also round-trips through AddData().
  WaylandExchangeDataProvider untitled_provider;
  const ClipboardUrlInfo untitled_url_info(GURL("https://example.com/"), u"");
  untitled_provider.SetURLs(base::span_from_ref(untitled_url_info));
  extracted.clear();
  EXPECT_TRUE(untitled_provider.ExtractData(kMimeTypeMozillaUrl, &extracted));
  const std::u16string kExpectedUntitled = u"https://example.com/\n";
  EXPECT_EQ(base::as_string_view(base::as_byte_span(kExpectedUntitled)),
            extracted);

  WaylandExchangeDataProvider untitled_dropped;
  untitled_dropped.AddData(ToClipboardData(extracted), kMimeTypeMozillaUrl);
  const std::vector<ClipboardUrlInfo> untitled_url_infos =
      untitled_dropped.GetURLs(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  ASSERT_EQ(1U, untitled_url_infos.size());
  EXPECT_EQ(GURL("https://example.com/"), untitled_url_infos[0].url);
  EXPECT_EQ(u"", untitled_url_infos[0].title);
}

TEST_F(WaylandExchangeDataProviderTest, FileContents) {
  constexpr std::string kName("filename");
  const base::span<const uint8_t> kContents =
      base::byte_span_from_cstring("contents");
  const std::string kMimeType("application/octet-stream;name=\"filename\"");

  WaylandExchangeDataProvider provider;
  provider.AddData(ToClipboardData(kContents), kMimeType);

  std::vector<std::string> mime_types = provider.BuildMimeTypesList();
  EXPECT_THAT(mime_types, ::testing::Contains(kMimeType));

  std::optional<OSExchangeDataProvider::FileContentsInfo> file_contents =
      provider.GetFileContents();
  EXPECT_TRUE(file_contents.has_value());
  EXPECT_EQ(kName, file_contents->filename.value());
  EXPECT_EQ(kContents, file_contents->file_contents);

  std::string extracted;
  EXPECT_TRUE(provider.ExtractData(kMimeType, &extracted));
  EXPECT_EQ(base::as_string_view(kContents), extracted);
}

}  // namespace ui
