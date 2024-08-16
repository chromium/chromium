// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/icon_util.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util_unittests_resource.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

static const char kSmallIconName[] = "16_X_16_icon.ico";
static const char kLargeIconName[] = "128_X_128_icon.ico";
static const char kTempIconFilename[] = "temp_test_icon.ico";

}  // namespace

class IconUtilTest : public testing::Test {
 public:
  using ScopedHICON = base::win::ScopedHICON;

  void SetUp() override {
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir_));
    test_data_dir_ = test_data_dir_.Append(FILE_PATH_LITERAL("ui"))
                         .Append(FILE_PATH_LITERAL("gfx"))
                         .Append(FILE_PATH_LITERAL("test"))
                         .Append(FILE_PATH_LITERAL("data"))
                         .Append(FILE_PATH_LITERAL("icon_util"));
    ASSERT_TRUE(base::PathExists(test_data_dir_));

    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  }

  static const int kSmallIconWidth = 16;
  static const int kSmallIconHeight = 16;
  static const int kLargeIconWidth = 128;
  static const int kLargeIconHeight = 128;

  // Given a file name for an .ico file and an image dimensions, this
  // function loads the icon and returns an HICON handle.
  ScopedHICON LoadIconFromFile(const base::FilePath& filename,
                               int width,
                               int height) {
    HICON icon = static_cast<HICON>(LoadImage(NULL,
                                    filename.value().c_str(),
                                    IMAGE_ICON,
                                    width,
                                    height,
                                    LR_LOADTRANSPARENT | LR_LOADFROMFILE));
    return ScopedHICON(icon);
  }

  SkBitmap CreateBlackSkBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    // Setting the pixels to transparent-black.
    memset(bitmap.getPixels(), 0, width * height * 4);
    return bitmap;
  }

  // Loads an .ico file from |icon_filename| and asserts that it contains all of
  // the expected icon sizes up to and including |max_icon_size|, and no other
  // icons. If |max_icon_size| >= 256, this tests for a 256x256 PNG icon entry.
  void CheckAllIconSizes(const base::FilePath& icon_filename,
                         int max_icon_size);

 protected:
  // The root directory for test files. This should be treated as read-only.
  base::FilePath test_data_dir_;

  // Directory for creating files by this test.
  base::ScopedTempDir temp_directory_;
};

void IconUtilTest::CheckAllIconSizes(const base::FilePath& icon_filename,
                                     int max_icon_size) {
  ASSERT_TRUE(base::PathExists(icon_filename));

  // Determine how many icons to expect, based on |max_icon_size|.
  int expected_num_icons = 0;
  for (size_t i = 0; i < IconUtil::kNumIconDimensions; ++i) {
    if (IconUtil::kIconDimensions[i] > max_icon_size)
      break;
    ++expected_num_icons;
  }

  // First, use the Windows API to load the icon, a basic validity test.
  EXPECT_TRUE(LoadIconFromFile(icon_filename, kSmallIconWidth, kSmallIconHeight)
                  .is_valid());

  // Read the file completely into memory.
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_filename, &icon_data));
  ASSERT_GE(icon_data.length(), sizeof(IconUtil::ICONDIR));

  // Ensure that it has exactly the expected number and sizes of icons, in the
  // expected order. This matches each entry of the loaded file's icon directory
  // with the corresponding element of kIconDimensions.
  // Also extracts the 256x256 entry as png_entry.
  const IconUtil::ICONDIR* icon_dir =
      reinterpret_cast<const IconUtil::ICONDIR*>(icon_data.data());
  EXPECT_EQ(expected_num_icons, icon_dir->idCount);
  ASSERT_GE(IconUtil::kNumIconDimensions, icon_dir->idCount);
  ASSERT_GE(icon_data.length(),
            sizeof(IconUtil::ICONDIR) +
                icon_dir->idCount * sizeof(IconUtil::ICONDIRENTRY));
  const IconUtil::ICONDIRENTRY* png_entry = NULL;
  for (size_t i = 0; i < icon_dir->idCount; ++i) {
    const IconUtil::ICONDIRENTRY* entry = &icon_dir->idEntries[i];
    // Mod 256 because as a special case in ICONDIRENTRY, the value 0 represents
    // a width or height of 256.
    int expected_size = IconUtil::kIconDimensions[i] % 256;
    EXPECT_EQ(expected_size, static_cast<int>(entry->bWidth));
    EXPECT_EQ(expected_size, static_cast<int>(entry->bHeight));
    if (entry->bWidth == 0 && entry->bHeight == 0) {
      EXPECT_EQ(NULL, png_entry);
      png_entry = entry;
    }
  }

  if (max_icon_size >= 256) {
    ASSERT_TRUE(png_entry);

    // Convert the PNG entry data back to a SkBitmap to ensure it's valid.
    ASSERT_GE(icon_data.length(),
              png_entry->dwImageOffset + png_entry->dwBytesInRes);
    gfx::Image image =
        gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(icon_data).subspan(
            png_entry->dwImageOffset, png_entry->dwBytesInRes));
    SkBitmap bitmap = image.AsBitmap();
    EXPECT_EQ(256, bitmap.width());
    EXPECT_EQ(256, bitmap.height());
  }
}

// The following test case makes sure IconUtil::SkBitmapFromHICON fails
// gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestIconToBitmapInvalidParameters) {
  base::FilePath icon_filename = test_data_dir_.AppendASCII(kSmallIconName);
  gfx::Size icon_size(kSmallIconWidth, kSmallIconHeight);
  ScopedHICON icon(
      LoadIconFromFile(icon_filename, icon_size.width(), icon_size.height()));
  ASSERT_TRUE(icon.is_valid());

  // Invalid size parameter.
  gfx::Size invalid_icon_size(kSmallIconHeight, 0);
  EXPECT_TRUE(IconUtil::CreateSkBitmapFromHICON(icon.get(), invalid_icon_size)
                  .isNull());

  // Invalid icon.
  EXPECT_TRUE(IconUtil::CreateSkBitmapFromHICON(nullptr, icon_size).isNull());

  // The following code should succeed.
  EXPECT_FALSE(
      IconUtil::CreateSkBitmapFromHICON(icon.get(), icon_size).drawsNothing());
}

// The following test case makes sure IconUtil::CreateHICONFromSkBitmap fails
// gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestBitmapToIconInvalidParameters) {
  ScopedHICON icon;
  std::unique_ptr<SkBitmap> bitmap;

  // Wrong bitmap format.
  bitmap = std::make_unique<SkBitmap>();
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setInfo(SkImageInfo::MakeA8(kSmallIconWidth, kSmallIconHeight));
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_FALSE(icon.is_valid());

  // Invalid bitmap size.
  bitmap = std::make_unique<SkBitmap>();
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setInfo(SkImageInfo::MakeN32Premul(0, 0));
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_FALSE(icon.is_valid());

  // Valid bitmap configuration but no pixels allocated.
  bitmap = std::make_unique<SkBitmap>();
  ASSERT_NE(bitmap.get(), static_cast<SkBitmap*>(NULL));
  bitmap->setInfo(SkImageInfo::MakeN32Premul(kSmallIconWidth,
                                             kSmallIconHeight));
  icon = IconUtil::CreateHICONFromSkBitmap(*bitmap);
  EXPECT_FALSE(icon.is_valid());
}

// The following test case makes sure IconUtil::CreateIconFileFromImageFamily
// fails gracefully when called with invalid input parameters.
TEST_F(IconUtilTest, TestCreateIconFileInvalidParameters) {
  gfx::ImageFamily image_family;
  base::FilePath valid_icon_filename =
      temp_directory_.GetPath().AppendASCII(kTempIconFilename);
  base::FilePath invalid_icon_filename =
      temp_directory_.GetPath().AppendASCII("<>?.ico");

  // Invalid file name.
  image_family.clear();
  image_family.Add(gfx::test::CreateImage(/*size=*/1));
  EXPECT_FALSE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                       invalid_icon_filename));
  EXPECT_FALSE(base::PathExists(invalid_icon_filename));
}

// This test case makes sure IconUtil::CreateIconFileFromImageFamily fails if
// the image family is empty or invalid.
TEST_F(IconUtilTest, TestCreateIconFileEmptyImageFamily) {
  base::FilePath icon_filename =
      temp_directory_.GetPath().AppendASCII(kTempIconFilename);

  // Empty image family.
  EXPECT_FALSE(IconUtil::CreateIconFileFromImageFamily(gfx::ImageFamily(),
                                                       icon_filename));
  EXPECT_FALSE(base::PathExists(icon_filename));

  // Image family with only an empty image.
  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image());
  EXPECT_FALSE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                       icon_filename));
  EXPECT_FALSE(base::PathExists(icon_filename));
}

// This test case makes sure that when we load an icon from disk and convert
// the HICON into a bitmap, the bitmap has the expected format and dimensions.
TEST_F(IconUtilTest, TestCreateSkBitmapFromHICON) {
  base::FilePath small_icon_filename =
      test_data_dir_.AppendASCII(kSmallIconName);
  gfx::Size small_icon_size(kSmallIconWidth, kSmallIconHeight);
  ScopedHICON small_icon(LoadIconFromFile(
      small_icon_filename, small_icon_size.width(), small_icon_size.height()));
  ASSERT_TRUE(small_icon.is_valid());
  SkBitmap bitmap =
      IconUtil::CreateSkBitmapFromHICON(small_icon.get(), small_icon_size);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(bitmap.width(), small_icon_size.width());
  EXPECT_EQ(bitmap.height(), small_icon_size.height());
  EXPECT_EQ(bitmap.colorType(), kN32_SkColorType);

  base::FilePath large_icon_filename =
      test_data_dir_.AppendASCII(kLargeIconName);
  gfx::Size large_icon_size(kLargeIconWidth, kLargeIconHeight);
  ScopedHICON large_icon(LoadIconFromFile(
      large_icon_filename, large_icon_size.width(), large_icon_size.height()));
  ASSERT_TRUE(large_icon.is_valid());
  bitmap = IconUtil::CreateSkBitmapFromHICON(large_icon.get(), large_icon_size);
  ASSERT_FALSE(bitmap.isNull());
  EXPECT_EQ(bitmap.width(), large_icon_size.width());
  EXPECT_EQ(bitmap.height(), large_icon_size.height());
  EXPECT_EQ(bitmap.colorType(), kN32_SkColorType);
}

// This test case makes sure that when an HICON is created from an SkBitmap,
// the returned handle is valid and refers to an icon with the expected
// dimensions color depth etc.
TEST_F(IconUtilTest, TestBasicCreateHICONFromSkBitmap) {
  SkBitmap bitmap = CreateBlackSkBitmap(kSmallIconWidth, kSmallIconHeight);
  ScopedHICON icon(IconUtil::CreateHICONFromSkBitmap(bitmap));
  EXPECT_TRUE(icon.is_valid());
  ICONINFO icon_info;
  ASSERT_TRUE(GetIconInfo(icon.get(), &icon_info));
  EXPECT_TRUE(icon_info.fIcon);

  // Now that have the icon information, we should obtain the specification of
  // the icon's bitmap and make sure it matches the specification of the
  // SkBitmap we started with.
  //
  // The bitmap handle contained in the icon information is a handle to a
  // compatible bitmap so we need to call ::GetDIBits() in order to retrieve
  // the bitmap's header information.
  BITMAPINFO bitmap_info;
  ::ZeroMemory(&bitmap_info, sizeof(BITMAPINFO));
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFO);
  HDC hdc = ::GetDC(NULL);
  int result = ::GetDIBits(hdc,
                           icon_info.hbmColor,
                           0,
                           kSmallIconWidth,
                           NULL,
                           &bitmap_info,
                           DIB_RGB_COLORS);
  ASSERT_GT(result, 0);
  EXPECT_EQ(bitmap_info.bmiHeader.biWidth, kSmallIconWidth);
  EXPECT_EQ(bitmap_info.bmiHeader.biHeight, kSmallIconHeight);
  EXPECT_EQ(bitmap_info.bmiHeader.biPlanes, 1);
  EXPECT_EQ(bitmap_info.bmiHeader.biBitCount, 32);
  ::ReleaseDC(NULL, hdc);
}

// This test case makes sure that CreateIconFileFromImageFamily creates a
// valid .ico file given an ImageFamily, and appropriately creates all icon
// sizes from the given input.
TEST_F(IconUtilTest, TestCreateIconFileFromImageFamily) {
  gfx::ImageFamily image_family;
  base::FilePath icon_filename =
      temp_directory_.GetPath().AppendASCII(kTempIconFilename);

  // Test with only a 16x16 icon. Should only scale up to 48x48.
  image_family.Add(gfx::Image::CreateFrom1xBitmap(
      CreateBlackSkBitmap(kSmallIconWidth, kSmallIconHeight)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 48);

  // Test with a 48x48 icon. Should only scale down.
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(48, 48)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 48);

  // Test with a 64x64 icon. Should scale up to 256x256.
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(64, 64)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 256);

  // Test with a 256x256 icon. Should include the 256x256 in the output.
  image_family.Add(gfx::Image::CreateFrom1xBitmap(
      CreateBlackSkBitmap(256, 256)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 256);

  // Test with a 49x49 icon. Should scale up to 256x256, but exclude the
  // original 49x49 representation from the output.
  image_family.clear();
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(49, 49)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 256);

  // Test with a non-square 16x32 icon. Should scale up to 48, but exclude the
  // original 16x32 representation from the output.
  image_family.clear();
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(16, 32)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 48);

  // Test with a non-square 32x49 icon. Should scale up to 256, but exclude the
  // original 32x49 representation from the output.
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(32, 49)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                      icon_filename));
  CheckAllIconSizes(icon_filename, 256);

  // Test with an empty and non-empty image.
  // The empty image should be ignored.
  image_family.clear();
  image_family.Add(gfx::Image());
  image_family.Add(gfx::Image::CreateFrom1xBitmap(CreateBlackSkBitmap(16, 16)));
  ASSERT_TRUE(IconUtil::CreateIconFileFromImageFamily(image_family,
                                                       icon_filename));
  CheckAllIconSizes(icon_filename, 48);
}

TEST_F(IconUtilTest, TestCreateImageFamilyFromIconResource) {
  HMODULE module = GetModuleHandle(NULL);
  std::unique_ptr<gfx::ImageFamily> family(
      IconUtil::CreateImageFamilyFromIconResource(module, IDR_MAINFRAME));
  ASSERT_TRUE(family.get());
  EXPECT_FALSE(family->empty());
  std::vector<gfx::Image> images;
  for (const auto& image : *family)
    images.push_back(image);

  // Assert that the family contains all of the images from the icon resource.
  EXPECT_EQ(5u, images.size());
  EXPECT_EQ(16, images[0].Width());
  EXPECT_EQ(24, images[1].Width());
  EXPECT_EQ(32, images[2].Width());
  EXPECT_EQ(48, images[3].Width());
  EXPECT_EQ(256, images[4].Width());
}

// This tests that kNumIconDimensionsUpToMediumSize has the correct value.
TEST_F(IconUtilTest, TestNumIconDimensionsUpToMediumSize) {
  ASSERT_LE(IconUtil::kNumIconDimensionsUpToMediumSize,
            IconUtil::kNumIconDimensions);
  EXPECT_EQ(IconUtil::kMediumIconSize,
            IconUtil::kIconDimensions[
                IconUtil::kNumIconDimensionsUpToMediumSize - 1]);
}

TEST_F(IconUtilTest, TestTransparentIcon) {
  base::FilePath icon_filename =
      temp_directory_.GetPath().AppendASCII(kTempIconFilename);
  int size = 48;
  auto semi_transparent_red = SkColorSetARGB(0x77, 0xFF, 0x00, 0x00);

  // Create a bitmap with a semi transparent red dot.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size, false);
  EXPECT_EQ(bitmap.alphaType(), kPremul_SkAlphaType);
  {
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    canvas.drawColor(SK_ColorWHITE);
    SkPaint paint;
    paint.setColor(semi_transparent_red);
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas.drawPoint(1, 1, paint);
  }

  // Create icon from that bitmap.
  gfx::ImageFamily image_family;
  image_family.Add(gfx::Image::CreateFrom1xBitmap(bitmap));
  ASSERT_TRUE(
      IconUtil::CreateIconFileFromImageFamily(image_family, icon_filename));

  // Load icon and check that dot has same color.
  ScopedHICON icon(LoadIconFromFile(icon_filename, size, size));
  ASSERT_TRUE(icon.is_valid());
  SkBitmap bitmap_loaded =
      IconUtil::CreateSkBitmapFromHICON(icon.get(), gfx::Size(size, size));
  EXPECT_EQ(bitmap_loaded.getColor(1, 1), semi_transparent_red);
}
