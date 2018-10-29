// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/clipboard/clipboard_util_mac.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/mozilla/NSPasteboard+Utils.h"

namespace {

class ClipboardUtilMacTest : public PlatformTest {
 public:
  ClipboardUtilMacTest() { }

  NSDictionary* DictionaryFromPasteboard(NSPasteboard* pboard) {
    NSArray* types = [pboard types];
    NSMutableDictionary* data = [NSMutableDictionary dictionary];
    for (NSString* type in types) {
      data[type] = [pboard dataForType:type];
    }
    return data;
  }
};

TEST_F(ClipboardUtilMacTest, PasteboardItemFromUrl) {
  NSString* urlString =
      @"https://www.google.com/"
      @"search?q=test&oq=test&aqs=chrome..69i57l2j69i60l4.278j0j7&"
      @"sourceid=chrome&ie=UTF-8";

  base::scoped_nsobject<NSPasteboardItem> item(
      ui::ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pasteboard->get() getURLs:&urls
                   andTitles:&titles
         convertingFilenames:NO
         convertingTextToURL:NO];

  ASSERT_EQ(1u, [urls count]);
  EXPECT_NSEQ(urlString, urls[0]);
  ASSERT_EQ(1u, [titles count]);
  EXPECT_NSEQ(urlString, titles[0]);

  NSURL* url = [NSURL URLFromPasteboard:pasteboard->get()];
  EXPECT_NSEQ([url absoluteString], urlString);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemWithTitle) {
  NSString* urlString = @"https://www.google.com/";
  NSString* title = @"Burrowing Yams";

  base::scoped_nsobject<NSPasteboardItem> item(
      ui::ClipboardUtil::PasteboardItemFromUrl(urlString, title));
  scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pasteboard->get() getURLs:&urls
                   andTitles:&titles
         convertingFilenames:NO
         convertingTextToURL:NO];

  ASSERT_EQ(1u, [urls count]);
  EXPECT_NSEQ(urlString, urls[0]);
  ASSERT_EQ(1u, [titles count]);
  EXPECT_NSEQ(title, titles[0]);

  NSURL* url = [NSURL URLFromPasteboard:pasteboard->get()];
  EXPECT_NSEQ([url absoluteString], urlString);
}

TEST_F(ClipboardUtilMacTest, PasteboardItemWithFilePath) {
  NSURL* url = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
  ASSERT_TRUE(url);
  NSString* urlString = [url absoluteString];

  base::scoped_nsobject<NSPasteboardItem> item(
      ui::ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  NSArray* urls = nil;
  NSArray* titles = nil;
  [pasteboard->get() getURLs:&urls
                   andTitles:&titles
         convertingFilenames:NO
         convertingTextToURL:NO];

  ASSERT_EQ(1u, [urls count]);
  EXPECT_NSEQ(urlString, urls[0]);
  ASSERT_EQ(1u, [titles count]);
  EXPECT_NSEQ(urlString, titles[0]);

  NSURL* urlFromPasteboard = [NSURL URLFromPasteboard:pasteboard->get()];
  EXPECT_NSEQ(urlFromPasteboard, url);
}

TEST_F(ClipboardUtilMacTest, CheckForLeak) {
  for (int i = 0; i < 10000; ++i) {
    @autoreleasepool {
      scoped_refptr<ui::UniquePasteboard> pboard = new ui::UniquePasteboard;
      EXPECT_TRUE(pboard->get());
    }
  }
}

TEST_F(ClipboardUtilMacTest, CompareToWriteToPasteboard) {
  NSString* urlString = @"https://www.cnn.com/";

  base::scoped_nsobject<NSPasteboardItem> item(
      ui::ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<ui::UniquePasteboard> pasteboard = new ui::UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  scoped_refptr<ui::UniquePasteboard> pboard = new ui::UniquePasteboard;
  [pboard->get() setDataForURL:urlString title:urlString];

  NSDictionary* data1 = DictionaryFromPasteboard(pasteboard->get());
  NSDictionary* data2 = DictionaryFromPasteboard(pboard->get());
  EXPECT_NSEQ(data1, data2);
}

}  // namespace
