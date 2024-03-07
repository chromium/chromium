// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/mac_util.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {
namespace {

using ClipboardUtilMacTest = PlatformTest;

TEST_F(ClipboardUtilMacTest, PasteboardItemsFromUrlsRoundtrip) {
  NSString* url_string_1 =
      @"https://www.google.com/"
      @"search?q=test&oq=test&aqs=chrome..69i57l2j69i60l4.278j0j7&"
      @"sourceid=chrome&ie=UTF-8";

  NSString* url_string_2 = @"https://www.google.com/";
  NSString* title_2 = @"Burrowing Yams";

  NSArray<NSPasteboardItem*>* items = clipboard_util::PasteboardItemsFromUrls(
      @[ url_string_1, url_string_2 ], @[ @"", title_2 ]);

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:items];

  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(pasteboard->get(),
                                                  /*include_files=*/false);

  ASSERT_EQ(2u, urls_and_titles.count);
  EXPECT_NSEQ(url_string_1, urls_and_titles[0].URL);
  EXPECT_NSEQ(url_string_2, urls_and_titles[1].URL);
  EXPECT_NSEQ(@"", urls_and_titles[0].title);
  EXPECT_NSEQ(title_2, urls_and_titles[1].title);

  NSURL* url = [NSURL URLFromPasteboard:pasteboard->get()];
  EXPECT_NSEQ(url.absoluteString, url_string_1);

  // Only the first item should have the "web urls and titles" data.
  EXPECT_TRUE([items[0].types containsObject:kUTTypeWebKitWebURLsWithTitles]);
  EXPECT_FALSE([items[1].types containsObject:kUTTypeWebKitWebURLsWithTitles]);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemsFromString) {
  NSString* url_string = @"    https://www.google.com/   ";

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ url_string ]];

  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(pasteboard->get(),
                                                  /*include_files=*/false);

  ASSERT_EQ(1u, urls_and_titles.count);
  EXPECT_NSEQ(@"https://www.google.com/", urls_and_titles[0].URL);
  EXPECT_NSEQ(@"www.google.com", urls_and_titles[0].title);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemWithFilePath) {
  NSURL* url = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
  ASSERT_TRUE(url);
  NSString* url_string = url.absoluteString;

  NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
  [item setString:url_string forType:NSPasteboardTypeFileURL];

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  // Read without translating file URLs, expect to not find it.

  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(pasteboard->get(),
                                                  /*include_files=*/false);

  ASSERT_EQ(0u, urls_and_titles.count);

  // Read with translating file URLs, expect to find it.

  urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(pasteboard->get(),
                                                  /*include_files=*/true);

  ASSERT_EQ(1u, urls_and_titles.count);
  EXPECT_NSEQ(url_string, urls_and_titles[0].URL);
}

TEST_F(ClipboardUtilMacTest, CheckForLeak) {
  for (int i = 0; i < 10000; ++i) {
    @autoreleasepool {
      scoped_refptr<UniquePasteboard> pboard = new UniquePasteboard;
      EXPECT_TRUE(pboard->get());
    }
  }
}

}  // namespace
}  // namespace ui
