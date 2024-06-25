// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "os_exchange_data_provider_non_backed.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {
const char16_t kTestString[] = u"Hello World!";
const char kUrl[] = "https://example.com";
const char16_t kUrlTitle[] = u"example";
const char kFileName[] = "file.pdf";
const base::FilePath::CharType kFileContentsFileName[] =
    FILE_PATH_LITERAL("file.jpg");
const char kFileContents[] = "test data";
const char16_t kHtml[] = u"<h1>Random Title</h1>";
const char kBaseUrl[] = "www.example2.com";
}  // namespace

// Tests that cloning OsExchangeDataProviderNonBacked object will clone all of
// its data members.
TEST(OSExchangeDataProviderNonBackedTest, CloneTest) {
  OSExchangeDataProviderNonBacked original;

  original.SetString(kTestString);
  original.SetURL(GURL(kUrl), kUrlTitle);

  base::Pickle original_pickle;
  original_pickle.WriteString16(kTestString);
  original.SetPickledData(ClipboardFormatType::PlainTextType(),
                          original_pickle);
  original.SetFileContents(base::FilePath(kFileContentsFileName),
                           std::string(kFileContents));
  original.SetHtml(kHtml, GURL(kBaseUrl));
  original.MarkRendererTaintedFromOrigin(url::Origin());
  GURL url("https://www.example.com");
  original.SetSource(std::make_unique<DataTransferEndpoint>(url));

  std::unique_ptr<OSExchangeDataProvider> copy = original.Clone();
  std::optional<std::u16string> copy_string = copy->GetString();
  EXPECT_EQ(kTestString, copy_string);

  std::optional<OSExchangeDataProvider::UrlInfo> url_info =
      copy->GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  EXPECT_TRUE(url_info.has_value());
  EXPECT_EQ(GURL(kUrl), url_info->url);
  EXPECT_EQ(kUrlTitle, url_info->title);

  std::optional<base::Pickle> copy_pickle =
      copy->GetPickledData(ClipboardFormatType::PlainTextType());
  base::PickleIterator pickle_itr(copy_pickle.value());
  std::u16string copy_pickle_string;
  EXPECT_TRUE(pickle_itr.ReadString16(&copy_pickle_string));
  EXPECT_EQ(kTestString, copy_pickle_string);

  std::optional<OSExchangeDataProvider::FileContentsInfo> copy_file_contents =
      copy->GetFileContents();
  ASSERT_TRUE(copy_file_contents.has_value());
  EXPECT_EQ(base::FilePath(kFileContentsFileName),
            copy_file_contents->filename);
  EXPECT_EQ(std::string(kFileContents), copy_file_contents->file_contents);

  std::optional<OSExchangeDataProvider::HtmlInfo> html_content =
      copy->GetHtml();
  ASSERT_TRUE(html_content.has_value());
  EXPECT_EQ(kHtml, html_content->html);
  EXPECT_EQ(GURL(kBaseUrl), html_content->base_url);

  EXPECT_TRUE(copy->IsRendererTainted());

  DataTransferEndpoint* data_endpoint = copy->GetSource();
  EXPECT_TRUE(data_endpoint);
  EXPECT_TRUE(data_endpoint->IsUrlType());
  EXPECT_EQ(url, *data_endpoint->GetURL());
}

TEST(OSExchangeDataProviderNonBackedTest, FileNameCloneTest) {
  OSExchangeDataProviderNonBacked original;
  original.SetFilename(base::FilePath(kFileName));

  std::unique_ptr<OSExchangeDataProvider> copy = original.Clone();
  std::optional<std::vector<FileInfo>> filenames = copy->GetFilenames();
  ASSERT_TRUE(filenames.has_value());
  EXPECT_EQ(base::FilePath(kFileName), filenames.value()[0].path);
}

}  // namespace ui
