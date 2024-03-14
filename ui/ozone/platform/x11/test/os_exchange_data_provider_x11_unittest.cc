// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/os_exchange_data_provider_x11.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "url/gurl.h"

const char kFileURL[] = "file:///home/user/file.txt";
const char16_t kFileURL16[] = u"file:///home/user/file.txt";
const char kFileName[] = "/home/user/file.txt";
const char16_t kGoogleTitle[] = u"Google";
const char kGoogleURL[] = "http://www.google.com/";

namespace ui {

class OSExchangeDataProviderX11Test : public testing::Test {
 public:
  OSExchangeDataProviderX11Test()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        event_source(x11::Connection::Get()) {}

  void AddURLList(const std::string& list_contents) {
    scoped_refptr<base::RefCountedMemory> mem(
        base::MakeRefCounted<base::RefCountedString>(list_contents));

    provider.format_map_.Insert(x11::GetAtom(kMimeTypeURIList), mem);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  X11EventSource event_source;
  OSExchangeDataProviderX11 provider;
};

TEST_F(OSExchangeDataProviderX11Test, MozillaURL) {
  // Check that we can get titled entries.
  provider.SetURL(GURL(kGoogleURL), kGoogleTitle);
  {
    std::optional<OSExchangeDataProvider::UrlInfo> url_info =
        provider.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
    ASSERT_TRUE(url_info.has_value());
    EXPECT_EQ(kGoogleTitle, url_info->title);
    EXPECT_EQ(kGoogleURL, url_info->url);
  }

  // Check that we can get non-titled entries.
  provider.SetURL(GURL(kGoogleURL), std::u16string());
  {
    std::optional<OSExchangeDataProvider::UrlInfo> url_info =
        provider.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
    ASSERT_TRUE(url_info.has_value());
    EXPECT_EQ(std::u16string(), url_info->title);
    EXPECT_EQ(kGoogleURL, url_info->url);
  }
}

TEST_F(OSExchangeDataProviderX11Test, FilesArentURLs) {
  AddURLList(kFileURL);

  EXPECT_TRUE(provider.HasFile());
  EXPECT_TRUE(provider.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  EXPECT_FALSE(provider.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
}

TEST_F(OSExchangeDataProviderX11Test, HTTPURLsArentFiles) {
  AddURLList(kGoogleURL);

  EXPECT_FALSE(provider.HasFile());
  EXPECT_TRUE(provider.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  EXPECT_TRUE(provider.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
}

TEST_F(OSExchangeDataProviderX11Test, URIListWithBoth) {
  AddURLList("file:///home/user/file.txt\nhttp://www.google.com");

  EXPECT_TRUE(provider.HasFile());
  EXPECT_TRUE(provider.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES));
  EXPECT_TRUE(provider.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));

  // We should only receive the file from GetFilenames().
  std::optional<std::vector<FileInfo>> filenames = provider.GetFilenames();
  ASSERT_TRUE(filenames.has_value());
  ASSERT_EQ(1u, filenames.value().size());
  EXPECT_EQ(kFileName, filenames.value()[0].path.value());

  // We should only receive the URL here.
  std::optional<OSExchangeDataProvider::UrlInfo> url_info =
      provider.GetURLAndTitle(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
  ASSERT_TRUE(url_info.has_value());
  EXPECT_EQ(std::u16string(), url_info->title);
  EXPECT_EQ(kGoogleURL, url_info->url);
}

TEST_F(OSExchangeDataProviderX11Test, OnlyStringURLIsUnfiltered) {
  const std::u16string file_url = kFileURL16;
  provider.SetString(file_url);

  EXPECT_TRUE(provider.HasString());
  EXPECT_FALSE(provider.HasURL(FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES));
}

TEST_F(OSExchangeDataProviderX11Test, StringAndURIListFilterString) {
  const std::u16string file_url = kFileURL16;
  provider.SetString(file_url);
  AddURLList(kFileURL);

  EXPECT_FALSE(provider.HasString());
  EXPECT_FALSE(provider.GetString().has_value());

  EXPECT_TRUE(provider.HasFile());
}

}  // namespace ui
