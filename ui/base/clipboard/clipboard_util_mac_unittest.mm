// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
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

  NSArray* urls = nil;
  NSArray* titles = nil;
  clipboard_util::URLsAndTitlesFromPasteboard(
      pasteboard->get(), /*include_files=*/false, &urls, &titles);

  ASSERT_EQ(2u, urls.count);
  EXPECT_NSEQ(url_string_1, urls[0]);
  EXPECT_NSEQ(url_string_2, urls[1]);
  ASSERT_EQ(2u, titles.count);
  EXPECT_NSEQ(@"", titles[0]);
  EXPECT_NSEQ(title_2, titles[1]);

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

  NSArray* urls = nil;
  NSArray* titles = nil;
  clipboard_util::URLsAndTitlesFromPasteboard(
      pasteboard->get(), /*include_files=*/false, &urls, &titles);

  ASSERT_EQ(1u, urls.count);
  EXPECT_NSEQ(@"https://www.google.com/", urls[0]);
  ASSERT_EQ(1u, titles.count);
  EXPECT_NSEQ(@"www.google.com", titles[0]);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemWithFilePath) {
  NSURL* url = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
  ASSERT_TRUE(url);
  NSString* url_string = url.absoluteString;

  NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];
  [item setString:url_string forType:NSPasteboardTypeFileURL];

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  // Read without translating file URLs, expect to not find it.

  NSArray* urls = nil;
  NSArray* titles = nil;
  clipboard_util::URLsAndTitlesFromPasteboard(
      pasteboard->get(), /*include_files=*/false, &urls, &titles);

  ASSERT_EQ(0u, urls.count);
  ASSERT_EQ(0u, titles.count);

  // Read with translating file URLs, expect to find it.

  clipboard_util::URLsAndTitlesFromPasteboard(
      pasteboard->get(), /*include_files=*/true, &urls, &titles);

  ASSERT_EQ(1u, urls.count);
  EXPECT_NSEQ(url_string, urls[0]);
  ASSERT_EQ(1u, titles.count);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemDisplayName) {
  NSURL* tempdir_url = [NSURL fileURLWithPath:NSTemporaryDirectory()
                                  isDirectory:YES];
  ASSERT_TRUE(tempdir_url);
  NSURL* file_url = [tempdir_url URLByAppendingPathComponent:@"a:b.txt"
                                                 isDirectory:NO];
  ASSERT_TRUE(tempdir_url);
  NSString* url_string = file_url.absoluteString;

  NSPasteboardItem* item = [[[NSPasteboardItem alloc] init] autorelease];
  [item setString:url_string forType:NSPasteboardTypeFileURL];

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  NSArray* urls = nil;
  NSArray* titles = nil;
  clipboard_util::URLsAndTitlesFromPasteboard(
      pasteboard->get(), /*include_files=*/true, &urls, &titles);

  ASSERT_EQ(1u, urls.count);
  EXPECT_NSEQ(url_string, urls[0]);
  ASSERT_EQ(1u, titles.count);
  EXPECT_NSEQ(@"a/b.txt", titles[0]);
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
