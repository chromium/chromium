// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/clipboard/clipboard_mac.h"

#import <AppKit/AppKit.h>
#import <PDFKit/PDFKit.h>

#include <vector>

#include "base/feature_list.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

// CGDataProviderReleaseDataCallback that releases the CreateImage buffer.
void CreateImageBufferReleaser(void* info, const void* data, size_t size) {
  DCHECK_EQ(info, data);
  free(info);
}

}  // namespace

class ClipboardMacTest : public PlatformTest,
                         public testing::WithParamInterface<bool> {
 public:
  ClipboardMacTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kMacClipboardWriteImageWithPng, ShouldWriteImageWithPng());

    PlatformTest::SetUp();
  }

  NSImage* CreateImage(int32_t width, int32_t height, bool retina) {
    int32_t pixel_width = retina ? width * 2 : width;
    int32_t pixel_height = retina ? height * 2 : height;

    // It seems more natural to create an NSBitmapImageRep and set it as the
    // representation for an NSImage. This doesn't work, because when the
    // result is written, and then read from an NSPasteboard, the new NSImage
    // loses its "retina-ness".
    uint8_t* buffer =
        static_cast<uint8_t*>(calloc(pixel_width * pixel_height, 4));
    base::ScopedCFTypeRef<CGDataProviderRef> provider(
        CGDataProviderCreateWithData(buffer, buffer,
                                     (pixel_width * pixel_height * 4),
                                     &CreateImageBufferReleaser));
    base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
        CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    base::ScopedCFTypeRef<CGImageRef> image_ref(
        CGImageCreate(pixel_width, pixel_height, 8, 32, 4 * pixel_width,
                      color_space.get(), kCGBitmapByteOrderDefault,
                      provider.get(), nullptr, NO, kCGRenderingIntentDefault));
    return [[NSImage alloc] initWithCGImage:image_ref.get()
                                       size:NSMakeSize(width, height)];
  }

  bool ShouldWriteImageWithPng() const { return GetParam(); }

  std::vector<uint8_t> ReadPngSync(ClipboardMac* clipboard_mac,
                                   NSPasteboard* pasteboard) {
    base::test::TestFuture<std::vector<uint8_t>> future;
    clipboard_mac->ReadPngInternal(
        ClipboardBuffer::kCopyPaste, pasteboard,
        future.GetCallback<const std::vector<uint8_t>&>());
    return future.Get();
  }

  void WriteBitmap(ClipboardMac* clipboard_mac,
                   const SkBitmap& bitmap,
                   NSPasteboard* pasteboard) {
    clipboard_mac->WriteBitmapInternal(bitmap, pasteboard);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
};

TEST_P(ClipboardMacTest, ReadImageRetina) {
  int32_t width = 99;
  int32_t height = 101;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ CreateImage(width, height, true) ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  EXPECT_EQ(2 * width, bitmap.width());
  EXPECT_EQ(2 * height, bitmap.height());
}

TEST_P(ClipboardMacTest, ReadImageNonRetina) {
  int32_t width = 99;
  int32_t height = 101;
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ CreateImage(width, height, false) ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  EXPECT_EQ(width, bitmap.width());
  EXPECT_EQ(height, bitmap.height());
}

TEST_P(ClipboardMacTest, EmptyImage) {
  NSImage* image = [[NSImage alloc] init];
  scoped_refptr<UniquePasteboard> pasteboard = new UniquePasteboard;
  [pasteboard->get() writeObjects:@[ image ]];

  Clipboard* clipboard = Clipboard::GetForCurrentThread();
  ClipboardMac* clipboard_mac = static_cast<ClipboardMac*>(clipboard);

  std::vector<uint8_t> png_data = ReadPngSync(clipboard_mac, pasteboard->get());
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  EXPECT_EQ(0, bitmap.width());
  EXPECT_EQ(0, bitmap.height());
}

TEST_P(ClipboardMacTest, PDFImage) {
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
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
  EXPECT_EQ(width, bitmap.width());
  EXPECT_EQ(height, bitmap.height());
}

TEST_P(ClipboardMacTest, WriteBitmapAddsPNGToClipboard) {
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

  if (!ShouldWriteImageWithPng()) {
    ASSERT_FALSE(data);
    return;
  }

  ASSERT_TRUE(data);
  const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
  std::vector<uint8_t> png_data(bytes, bytes + data.length);

  SkBitmap result_bitmap;
  gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &result_bitmap);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, result_bitmap));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardMacTest,
                         // Is the kMacClipboardWriteImageWithPng flag enabled?
                         ::testing::Bool());

}  // namespace ui
