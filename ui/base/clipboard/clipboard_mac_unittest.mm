// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/clipboard/clipboard_mac.h"

#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>

#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/mac/mac_util.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace ui {

namespace {

// CGDataProviderReleaseDataCallback that releases the CreateImage buffer.
void CreateImageBufferReleaser(void* info, const void* data, size_t size) {
  DCHECK_EQ(info, data);
  free(info);
}

class TestClipboardObserver : public ClipboardObserver {
 public:
  TestClipboardObserver() {
    ClipboardMonitor::GetInstance()->AddObserver(this);
  }

  ~TestClipboardObserver() override {
    ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void OnClipboardDataChanged() override { ++data_changed_count_; }

  int data_changed_count() const { return data_changed_count_; }

 private:
  int data_changed_count_ = 0;
};

// Uses a random UUID pasteboard name instead of `UniquePasteboard` because
// `+[NSPasteboard pasteboardWithUniqueName]` can return the same name to
// parallel test processes started in the same microsecond.
class ScopedTestPasteboard {
 public:
  ScopedTestPasteboard()
      : pasteboard_(
            [NSPasteboard pasteboardWithName:[NSUUID UUID].UUIDString]) {}
  ScopedTestPasteboard(const ScopedTestPasteboard&) = delete;
  ScopedTestPasteboard& operator=(const ScopedTestPasteboard&) = delete;
  ~ScopedTestPasteboard() { [pasteboard_ releaseGlobally]; }

  NSPasteboard* get() const { return pasteboard_; }

 private:
  NSPasteboard* __strong pasteboard_;
};

}  // namespace

class ClipboardMacTest : public PlatformTest {
 public:
  ClipboardMacTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  NSImage* CreateImage(int32_t width, int32_t height, bool retina) {
    int32_t pixel_width = retina ? width * 2 : width;
    int32_t pixel_height = retina ? height * 2 : height;

    // It seems more natural to create an NSBitmapImageRep and set it as the
    // representation for an NSImage. This doesn't work, because when the
    // result is written, and then read from an NSPasteboard, the new NSImage
    // loses its "retina-ness".
    uint8_t* buffer =
        static_cast<uint8_t*>(calloc(pixel_width * pixel_height, 4));
    base::apple::ScopedCFTypeRef<CGDataProviderRef> provider(
        CGDataProviderCreateWithData(buffer, buffer,
                                     (pixel_width * pixel_height * 4),
                                     &CreateImageBufferReleaser));
    base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
        CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    base::apple::ScopedCFTypeRef<CGImageRef> image_ref(
        CGImageCreate(pixel_width, pixel_height, 8, 32, 4 * pixel_width,
                      color_space.get(), kCGImageAlphaLast, provider.get(),
                      nullptr, NO, kCGRenderingIntentDefault));
    return [[NSImage alloc] initWithCGImage:image_ref.get()
                                       size:NSMakeSize(width, height)];
  }

  std::vector<uint8_t> ReadPngSync(ClipboardMac* clipboard_mac,
                                   NSPasteboard* pasteboard) {
    base::test::TestFuture<std::vector<uint8_t>> future;
    clipboard_mac->ReadPngInternal(
        ClipboardBuffer::kCopyPaste, pasteboard,
        future.GetCallback<const std::vector<uint8_t>&>());
    return future.Get();
  }

  void WriteURLs(ClipboardMac* clipboard_mac,
                 base::span<const ClipboardUrlInfo> url_infos,
                 NSPasteboard* pasteboard) {
    clipboard_mac->WriteURLsInternal(url_infos, pasteboard);
  }

  void WriteBitmap(ClipboardMac* clipboard_mac,
                   const SkBitmap& bitmap,
                   NSPasteboard* pasteboard) {
    clipboard_mac->WriteBitmapInternal(bitmap, pasteboard);
  }

  std::optional<DataTransferEndpoint> GetSource(
      const ClipboardMac* clipboard_mac,
      NSPasteboard* pasteboard) {
    base::test::TestFuture<std::optional<DataTransferEndpoint>> future;
    clipboard_mac->GetSourceInternal(ClipboardBuffer::kCopyPaste, pasteboard,
                                     future.GetCallback());
    return future.Get();
  }

  void Clear(ClipboardMac* clipboard_mac, NSPasteboard* pasteboard) {
    clipboard_mac->ClearInternal(ClipboardBuffer::kCopyPaste, pasteboard);
  }

  void WritePortableAndPlatformRepresentations(
      ClipboardMac* clipboard_mac,
      std::unique_ptr<DataTransferEndpoint> data_src,
      NSPasteboard* pasteboard) {
    clipboard_mac->WritePortableAndPlatformRepresentationsInternal(
        ClipboardBuffer::kCopyPaste, /*objects=*/{}, /*raw_objects=*/{},
        /*platform_representations=*/{}, std::move(data_src), pasteboard,
        /*privacy_types=*/0);
  }

  ClipboardSequenceNumberToken GetSequenceNumber(
      const ClipboardMac* clipboard_mac,
      NSPasteboard* pasteboard) {
    return clipboard_mac->GetSequenceNumber(pasteboard);
  }

  void ClipboardChanged(ClipboardMac* clipboard_mac, NSPasteboard* pasteboard) {
    clipboard_mac->ClipboardChanged(pasteboard);
  }

  // Helper method to run assertion with given count value
  void AssertClipboardObserverCount(TestClipboardObserver& observer,
                                    int expected_count) {
    ASSERT_TRUE(base::test::RunUntil([&observer, expected_count]() {
      return observer.data_changed_count() == expected_count;
    })) << "Timeout waiting for the clipboardMonitor to be notified. Expected "
           "count: "
        << expected_count << " but got " << observer.data_changed_count();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ClipboardMacTest, ReadImageRetina) {
  int32_t width = 99;
  int32_t height = 101;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ CreateImage(width, height, true) ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap = gfx::PNGCodec::Decode(png_data);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(2 * width, bitmap.width());
  EXPECT_EQ(2 * height, bitmap.height());
}

TEST_F(ClipboardMacTest, ReadImageNonRetina) {
  int32_t width = 99;
  int32_t height = 101;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ CreateImage(width, height, false) ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap = gfx::PNGCodec::Decode(png_data);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(width, bitmap.width());
  EXPECT_EQ(height, bitmap.height());
}

TEST_F(ClipboardMacTest, EmptyImage) {
  NSImage* image = [[NSImage alloc] init];
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ image ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap = gfx::PNGCodec::Decode(png_data);
  ASSERT_TRUE(bitmap.isNull());
}

TEST_F(ClipboardMacTest, PDFImage) {
  int32_t width = 99;
  int32_t height = 101;
  PDFPage* page = [[PDFPage alloc] init];
  [page setBounds:NSMakeRect(0, 0, width, height)
           forBox:kPDFDisplayBoxMediaBox];
  PDFDocument* pdf_document = [[PDFDocument alloc] init];
  [pdf_document insertPage:page atIndex:0];
  NSData* pdf_data = [pdf_document dataRepresentation];

  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() setData:pdf_data forType:NSPasteboardTypePDF];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap = gfx::PNGCodec::Decode(png_data);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(width, bitmap.width());
  EXPECT_EQ(height, bitmap.height());
}

TEST_F(ClipboardMacTest, WriteBitmapAddsPNGToClipboard) {
  int32_t width = 99;
  int32_t height = 101;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  WriteBitmap(clipboard_mac, bitmap, pasteboard->get());

  NSData* data = [pasteboard->get() dataForType:NSPasteboardTypePNG];
  ASSERT_TRUE(data);
  const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> png_data =
      UNSAFE_TODO(std::vector<uint8_t>(bytes, bytes + data.length));

  SkBitmap result_bitmap = gfx::PNGCodec::Decode(png_data);
  ASSERT_FALSE(result_bitmap.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, result_bitmap));
}

TEST_F(ClipboardMacTest, SourceTracking) {
  TestClipboardObserver observer;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  GURL google_url = GURL("https://www.google.com");
  WritePortableAndPlatformRepresentations(
      clipboard_mac, std::make_unique<DataTransferEndpoint>(google_url),
      pasteboard->get());

  AssertClipboardObserverCount(observer, 1);

  auto source = GetSource(clipboard_mac, pasteboard->get());
  ASSERT_TRUE(source);
  ASSERT_TRUE(source->IsUrlType());
  ASSERT_EQ(*source->GetURL(), google_url);

  GURL chromium_url = GURL("https://chromium.org");
  WritePortableAndPlatformRepresentations(
      clipboard_mac, std::make_unique<DataTransferEndpoint>(chromium_url),
      pasteboard->get());

  source = GetSource(clipboard_mac, pasteboard->get());
  ASSERT_TRUE(source);
  ASSERT_TRUE(source->IsUrlType());
  ASSERT_EQ(*source->GetURL(), chromium_url);

  Clear(clipboard_mac, pasteboard->get());
  ASSERT_FALSE(GetSource(clipboard_mac, pasteboard->get()));
}

TEST_F(ClipboardMacTest, ClipboardChangeAPI_BrowserTriggered) {
  TestClipboardObserver observer;

  // Note that general pasteboard is used since the clipboard monitoring
  // is performed on the general pasteboard in ClipboardMac.
  NSPasteboard* nspasteboard = NSPasteboard.generalPasteboard;

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  ASSERT_EQ(observer.data_changed_count(), 0);

  GURL google_url = GURL("https://www.google.com");
  WritePortableAndPlatformRepresentations(
      clipboard_mac, std::make_unique<DataTransferEndpoint>(google_url),
      nspasteboard);

  AssertClipboardObserverCount(observer, 1);

  Clear(clipboard_mac, nspasteboard);
}

TEST_F(ClipboardMacTest, ClipboardChangeAPI_ExternallyTriggered) {
  TestClipboardObserver observer;

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  ASSERT_EQ(observer.data_changed_count(), 0);

  NSPasteboard* nspasteboard = NSPasteboard.generalPasteboard;
  NSString* text = @"Hello, Clipboard!";
  [nspasteboard clearContents];
  [nspasteboard setString:text forType:NSPasteboardTypeString];

  AssertClipboardObserverCount(observer, 1);

  Clear(clipboard_mac, nspasteboard);
}

// Regression test for crbug.com/568453825: an external app writes to the
// pasteboard and immediately triggers a paste. Blink reads the sequence number
// at the start of the paste (minting a token for the new changeCount) before
// the asynchronous pasteboard-changed notification has been delivered. When
// that notification arrives for the same changeCount, it must not mint a
// second token, or Blink will conclude the clipboard changed mid-paste and
// drop the paste.
//
// A private pasteboard is used, and the change notification is delivered
// directly, so that test processes running in parallel on the same machine
// (which share the general pasteboard) cannot perturb the changeCount.
TEST_F(ClipboardMacTest, SequenceNumberStableAcrossChangeNotification) {
  TestClipboardObserver observer;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  // Simulate an external application writing to the pasteboard.
  [pasteboard->get() clearContents];
  [pasteboard->get() setString:@"Hello from external app"
                       forType:NSPasteboardTypeString];

  // Simulate Blink reading the sequence number at the start of a paste, before
  // the change notification has been processed.
  const ClipboardSequenceNumberToken token_before =
      GetSequenceNumber(clipboard_mac, pasteboard->get());

  // Deliver the change notification for the same changeCount. Observers must
  // be told, but the token must not change.
  const int count_before = observer.data_changed_count();
  ClipboardChanged(clipboard_mac, pasteboard->get());
  EXPECT_EQ(observer.data_changed_count(), count_before + 1);
  EXPECT_EQ(token_before, GetSequenceNumber(clipboard_mac, pasteboard->get()));

  // Re-delivering a notification for an unchanged changeCount is a no-op.
  ClipboardChanged(clipboard_mac, pasteboard->get());
  EXPECT_EQ(observer.data_changed_count(), count_before + 1);
  EXPECT_EQ(token_before, GetSequenceNumber(clipboard_mac, pasteboard->get()));

  // A real change must still produce a new token and a notification.
  [pasteboard->get() clearContents];
  [pasteboard->get() setString:@"Something else"
                       forType:NSPasteboardTypeString];
  ClipboardChanged(clipboard_mac, pasteboard->get());
  EXPECT_EQ(observer.data_changed_count(), count_before + 2);
  EXPECT_NE(token_before, GetSequenceNumber(clipboard_mac, pasteboard->get()));
}

TEST_F(ClipboardMacTest, WriteURLsWritesMultiplePasteboardItems) {
  ScopedTestPasteboard pasteboard;
  [pasteboard.get() clearContents];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  const std::vector<ClipboardUrlInfo> url_infos = {
      {.url = GURL("https://a.example/1"), .title = u"First"},
      {.url = GURL("https://b.example/2"), .title = u"Second"},
  };
  WriteURLs(clipboard_mac, url_infos, pasteboard.get());

  ASSERT_EQ(2u, pasteboard.get().pasteboardItems.count);

  NSArray<URLAndTitle*>* urls_and_titles =
      clipboard_util::URLsAndTitlesFromPasteboard(pasteboard.get(),
                                                  /*include_files=*/false);
  ASSERT_EQ(2u, urls_and_titles.count);
  EXPECT_NSEQ(@"https://a.example/1", urls_and_titles[0].URL);
  EXPECT_NSEQ(@"First", urls_and_titles[0].title);
  EXPECT_NSEQ(@"https://b.example/2", urls_and_titles[1].URL);
  EXPECT_NSEQ(@"Second", urls_and_titles[1].title);

  // NSPasteboard joins plain-text items with a newline on demand.
  EXPECT_NSEQ(@"https://a.example/1\nhttps://b.example/2",
              [pasteboard.get() stringForType:NSPasteboardTypeString]);
  EXPECT_NSEQ(@"https://a.example/1",
              [pasteboard.get() stringForType:NSPasteboardTypeURL]);
}

TEST_F(ClipboardMacTest, WriteURLsSingleItemSharesFirstPasteboardItem) {
  ScopedTestPasteboard pasteboard;
  [pasteboard.get() clearContents];
  [pasteboard.get() setString:@"Custom text" forType:NSPasteboardTypeString];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  const std::vector<ClipboardUrlInfo> url_infos = {
      {.url = GURL("https://a.example/"), .title = u"Example"},
  };
  WriteURLs(clipboard_mac, url_infos, pasteboard.get());

  EXPECT_EQ(1u, pasteboard.get().pasteboardItems.count);
  EXPECT_NSEQ(@"Custom text",
              [pasteboard.get() stringForType:NSPasteboardTypeString]);
  EXPECT_NSEQ(@"https://a.example/",
              [pasteboard.get() stringForType:NSPasteboardTypeURL]);
}

}  // namespace ui
