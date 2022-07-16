// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "third_party/mozilla/NSPasteboard+Utils.h"

namespace ui {
namespace {

class ClipboardUtilMacTest : public PlatformTest {
 public:
  ClipboardUtilMacTest() = default;

  // Given a pasteboard, returns a dictionary of the contents of the pasteboard
  // for use in deep comparisons. This fully unpacks any plist-encoded items.
  NSDictionary* DictionaryFromPasteboardForDeepComparisons(
      NSPasteboard* pboard) {
    NSMutableDictionary* result = [NSMutableDictionary dictionary];
    for (NSString* type in [pboard types]) {
      NSData* data = [pboard dataForType:type];
      // Try to unpack the data as a plist, and if it succeeds, use that in the
      // resulting dictionary rather than the raw NSData. This is needed because
      // plists have multiple encodings, and the comparison should be made on
      // the underlying data rather than the specific encoding used by the OS.
      NSDictionary* unpacked_data = [NSPropertyListSerialization
          propertyListWithData:data
                       options:NSPropertyListImmutable
                        format:nil
                         error:nil];
      if (unpacked_data)
        result[type] = unpacked_data;
      else
        result[type] = data;
    }
    return result;
  }
};

TEST_F(ClipboardUtilMacTest, PasteboardItemFromUrl) {
  if (base::mac::IsAtMostOS10_11()) {
    GTEST_SKIP() << "macOS 10.11 and earlier are flaky and hang in pasteboard "
                    "code. https://crbug.com/1232472";
  }

  NSString* urlString =
      @"https://www.google.com/"
      @"search?q=test&oq=test&aqs=chrome..69i57l2j69i60l4.278j0j7&"
      @"sourceid=chrome&ie=UTF-8";

  base::scoped_nsobject<NSPasteboardItem> item(
      ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
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
  if (base::mac::IsAtMostOS10_11()) {
    GTEST_SKIP() << "macOS 10.11 and earlier are flaky and hang in pasteboard "
                    "code. https://crbug.com/1232472";
  }

  NSString* urlString = @"https://www.google.com/";
  NSString* title = @"Burrowing Yams";

  base::scoped_nsobject<NSPasteboardItem> item(
      ClipboardUtil::PasteboardItemFromUrl(urlString, title));
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
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
  if (base::mac::IsAtMostOS10_11()) {
    GTEST_SKIP() << "macOS 10.11 and earlier are flaky and hang in pasteboard "
                    "code. https://crbug.com/1232472";
  }

  NSURL* url = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
  ASSERT_TRUE(url);
  NSString* urlString = [url absoluteString];

  base::scoped_nsobject<NSPasteboardItem> item(
      ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
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
  if (base::mac::IsAtMostOS10_11()) {
    GTEST_SKIP() << "macOS 10.11 and earlier are flaky and hang in pasteboard "
                    "code. https://crbug.com/1232472";
  }

  for (int i = 0; i < 10000; ++i) {
    @autoreleasepool {
      scoped_refptr<UniquePasteboard> pboard = new UniquePasteboard;
      EXPECT_TRUE(pboard->get());
    }
  }
}

TEST_F(ClipboardUtilMacTest, CompareToWriteToPasteboard) {
  if (base::mac::IsAtMostOS10_11()) {
    GTEST_SKIP() << "macOS 10.11 and earlier are flaky and hang in pasteboard "
                    "code. https://crbug.com/1232472";
  }

  NSString* urlString = @"https://www.cnn.com/";

  base::scoped_nsobject<NSPasteboardItem> item(
      ClipboardUtil::PasteboardItemFromUrl(urlString, nil));
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ item ]];

  scoped_refptr<UniquePasteboard> pboard = new UniquePasteboard;
  [pboard->get() setDataForURL:urlString title:urlString];

  NSDictionary* data1 =
      DictionaryFromPasteboardForDeepComparisons(pasteboard->get());
  NSDictionary* data2 =
      DictionaryFromPasteboardForDeepComparisons(pboard->get());
  EXPECT_NSEQ(data1, data2);
}

}  // namespace
}  // namespace ui
