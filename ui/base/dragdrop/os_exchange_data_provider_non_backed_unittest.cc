// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "os_exchange_data_provider_non_backed.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace ui {

namespace {
const char kTestString[] = "Hello World!";
const char kUrl[] = "https://example.com";
const char kUrlTitle[] = "example";
const char kFileName[] = "file.pdf";
const char kHtml[] = "<h1>Random Title</h1>";
const char kBaseUrl[] = "www.example2.com";
}  // namespace

// Tests that cloning OsExchangeDataProviderNonBacked object will clone all of
// its data members.
TEST(OSExchangeDataProviderNonBackedTest, CloneTest) {
  OSExchangeDataProviderNonBacked original;

  original.SetString(base::UTF8ToUTF16(kTestString));
  original.SetURL(GURL(kUrl), base::UTF8ToUTF16(kUrlTitle));

  base::Pickle original_pickle;
  original_pickle.WriteString16(base::UTF8ToUTF16(kTestString));
  original.SetPickledData(ClipboardFormatType::GetPlainTextType(),
                          original_pickle);
  original.SetHtml(base::UTF8ToUTF16(kHtml), GURL(kBaseUrl));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  original.MarkOriginatedFromRenderer();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  url::Origin origin(url::Origin::Create(GURL("www.example.com")));
  original.SetSource(std::make_unique<DataTransferEndpoint>(origin));

  std::unique_ptr<OSExchangeDataProvider> copy = original.Clone();
  base::string16 copy_string;
  EXPECT_TRUE(copy->GetString(&copy_string));
  EXPECT_EQ(base::UTF8ToUTF16(kTestString), copy_string);

  GURL copy_url;
  base::string16 copy_title;
  EXPECT_TRUE(copy->GetURLAndTitle(
      FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES, &copy_url, &copy_title));
  EXPECT_EQ(GURL(kUrl), copy_url);
  EXPECT_EQ(base::UTF8ToUTF16(kUrlTitle), copy_title);

  base::Pickle copy_pickle;
  copy->GetPickledData(ClipboardFormatType::GetPlainTextType(), &copy_pickle);
  base::PickleIterator pickle_itr(copy_pickle);
  base::string16 copy_pickle_string;
  EXPECT_TRUE(pickle_itr.ReadString16(&copy_pickle_string));
  EXPECT_EQ(base::UTF8ToUTF16(kTestString), copy_pickle_string);

  base::string16 copy_html;
  GURL copy_base_url;
  EXPECT_TRUE(copy->GetHtml(&copy_html, &copy_base_url));
  EXPECT_EQ(base::UTF8ToUTF16(kHtml), copy_html);
  EXPECT_EQ(GURL(kBaseUrl), copy_base_url);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(copy->DidOriginateFromRenderer());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  DataTransferEndpoint* data_endpoint = copy->GetSource();
  EXPECT_TRUE(data_endpoint);
  EXPECT_TRUE(data_endpoint->IsUrlType());
  EXPECT_EQ(origin, *data_endpoint->origin());
}

TEST(OSExchangeDataProviderNonBackedTest, FileNameCloneTest) {
  OSExchangeDataProviderNonBacked original;
  original.SetFilename(base::FilePath(kFileName));

  std::unique_ptr<OSExchangeDataProvider> copy = original.Clone();
  base::FilePath copy_file_path;
  EXPECT_TRUE(copy->GetFilename(&copy_file_path));
  EXPECT_EQ(base::FilePath(kFileName), copy_file_path);
}

}  // namespace ui