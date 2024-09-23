// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/resource/resource_bundle.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/byte_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/data_pack_literal.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

using ::testing::_;
using ::testing::Between;
using ::testing::DoAll;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnArg;
using ::testing::SetArgPointee;

namespace ui {
namespace {

const unsigned char kPngMagic[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
const size_t kPngChunkMetadataSize = 12;
const unsigned char kPngIHDRChunkType[4] = { 'I', 'H', 'D', 'R' };

// Custom chunk that GRIT adds to PNG to indicate that it could not find a
// bitmap at the requested scale factor and fell back to 1x.
const unsigned char kPngScaleChunk[12] = { 0x00, 0x00, 0x00, 0x00,
                                           'c', 's', 'C', 'l',
                                           0xc1, 0x30, 0x60, 0x4d };

// A string with the "LOTTIE" prefix that GRIT adds to Lottie assets.
constexpr char kLottieData[] = "LOTTIEtest";
// The contents after the prefix has been removed.
constexpr uint8_t kLottieExpected[] = {'t', 'e', 's', 't'};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Mock of |lottie::ParseLottieAsStillImage|. Checks that |kLottieData| is
// properly stripped of the "LOTTIE" prefix.
gfx::ImageSkia ParseLottieAsStillImageForTesting(std::vector<uint8_t> data) {
  CHECK(base::ranges::equal(data, kLottieExpected));

  constexpr int kDimension = 16;
  return gfx::ImageSkia(
      gfx::ImageSkiaRep(gfx::Size(kDimension, kDimension), 0.f));
}
#endif

// Returns |bitmap_data| with |custom_chunk| inserted after the IHDR chunk.
void AddCustomChunk(std::string_view custom_chunk,
                    std::vector<unsigned char>* bitmap_data) {
  size_t chunk_offset = 0u;

  // Expect the magic signature first.
  auto magic_span =
      base::as_byte_span(*bitmap_data).first<std::size(kPngMagic)>();
  EXPECT_TRUE(magic_span == kPngMagic);
  chunk_offset += magic_span.size();

  // Expect an IHDR chunk next. It starts with a length.
  auto ihdr_chunk = base::as_byte_span(*bitmap_data).subspan(chunk_offset);
  uint32_t ihdr_chunk_length =
      base::numerics::U32FromBigEndian(ihdr_chunk.first<sizeof(uint32_t)>());
  auto ihdr_type =
      ihdr_chunk.subspan<sizeof(uint32_t), std::size(kPngIHDRChunkType)>();
  EXPECT_TRUE(ihdr_type == kPngIHDRChunkType);
  chunk_offset += ihdr_chunk_length;

  // Expect a PNG Metadata chunk next.
  chunk_offset += kPngChunkMetadataSize;

  // Then insert custom chunk.
  ASSERT_LE(chunk_offset, bitmap_data->size());
  bitmap_data->insert(bitmap_data->begin() + chunk_offset, custom_chunk.begin(),
                      custom_chunk.end());
}

// Creates datapack at |path| with a single bitmap at resource ID 3
// which is |edge_size|x|edge_size| pixels.
// If |custom_chunk| is non empty, adds it after the IHDR chunk
// in the encoded bitmap data.
void CreateDataPackWithSingleBitmap(const base::FilePath& path,
                                    int edge_size,
                                    std::string_view custom_chunk) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(edge_size, edge_size);
  bitmap.eraseColor(SK_ColorWHITE);
  std::vector<unsigned char> bitmap_data;
  EXPECT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data));

  if (custom_chunk.size() > 0)
    AddCustomChunk(custom_chunk, &bitmap_data);

  std::map<uint16_t, std::string_view> resources;
  resources[3u] = std::string_view(
      reinterpret_cast<const char*>(&bitmap_data[0]), bitmap_data.size());
  DataPack::WritePack(path, resources, ui::DataPack::BINARY);
}

}  // namespace

class ResourceBundleTest : public testing::Test {
 public:
  ResourceBundleTest() : resource_bundle_(nullptr) {}

  ResourceBundleTest(const ResourceBundleTest&) = delete;
  ResourceBundleTest& operator=(const ResourceBundleTest&) = delete;

  ~ResourceBundleTest() override {}

  // Overridden from testing::Test:
  void TearDown() override {
    resource_bundle_.reset();
    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

  // Returns new ResoureBundle with the specified |delegate|. The
  // ResourceBundleTest class manages the lifetime of the returned
  // ResourceBundle.
  ResourceBundle* CreateResourceBundle(ResourceBundle::Delegate* delegate) {
    DCHECK(!resource_bundle_);
    resource_bundle_ = std::make_unique<ResourceBundle>(delegate);
    return resource_bundle_.get();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  MockResourceBundleDelegate delegate_;
  std::unique_ptr<ResourceBundle> resource_bundle_;
};

TEST_F(ResourceBundleTest, DelegateGetPathForResourcePack) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);
  base::FilePath pack_path(FILE_PATH_LITERAL("/path/to/test_path.pak"));
  ResourceScaleFactor pack_scale_factor = ui::k200Percent;

  EXPECT_CALL(delegate_, GetPathForResourcePack(Property(&base::FilePath::value,
                                                         pack_path.value()),
                                                pack_scale_factor))
      .Times(1)
      .WillOnce(Return(pack_path));

  resource_bundle->AddDataPackFromPath(pack_path, pack_scale_factor);
}

TEST_F(ResourceBundleTest, DelegateGetPathForLocalePack) {
  ResourceBundle* orig_instance =
      ResourceBundle::SwapSharedInstanceForTesting(nullptr);
  ResourceBundle::InitSharedInstance(&delegate_);

  std::string locale = "en-US";

  // Cancel the load.
  EXPECT_CALL(delegate_, GetPathForLocalePack(_, _))
      .WillRepeatedly(Return(base::FilePath()))
      .RetiresOnSaturation();

  EXPECT_FALSE(ResourceBundle::LocaleDataPakExists(locale));
  EXPECT_EQ("", ResourceBundle::GetSharedInstance().LoadLocaleResources(
                    locale, /*crash_on_failure=*/false));

  // Allow the load to proceed.
  EXPECT_CALL(delegate_, GetPathForLocalePack(_, _))
      .WillRepeatedly(ReturnArg<0>());

  EXPECT_TRUE(ResourceBundle::LocaleDataPakExists(locale));
  EXPECT_EQ(locale, ResourceBundle::GetSharedInstance().LoadLocaleResources(
                        locale, /*crash_on_failure=*/false));

  ResourceBundle::CleanupSharedInstance();
  ResourceBundle::SwapSharedInstanceForTesting(orig_instance);
}

TEST_F(ResourceBundleTest, DelegateGetImageNamed) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);
  gfx::Image empty_image = resource_bundle->GetEmptyImage();
  int resource_id = 5;

  EXPECT_CALL(delegate_, GetImageNamed(resource_id))
      .Times(1)
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle->GetImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST_F(ResourceBundleTest, DelegateGetNativeImageNamed) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);

  gfx::Image empty_image = resource_bundle->GetEmptyImage();
  int resource_id = 5;

  // Some platforms delegate GetNativeImageNamed calls to GetImageNamed.
  EXPECT_CALL(delegate_, GetImageNamed(resource_id))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));
  EXPECT_CALL(delegate_, GetNativeImageNamed(resource_id))
      .Times(Between(0, 1))
      .WillOnce(Return(empty_image));

  gfx::Image result = resource_bundle->GetNativeImageNamed(resource_id);
  EXPECT_EQ(empty_image.ToSkBitmap(), result.ToSkBitmap());
}

TEST_F(ResourceBundleTest, DelegateLoadDataResourceBytes) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);

  // Create the data resource for testing purposes.
  const unsigned char data[] = "My test data";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(data));

  int resource_id = 5;
  ResourceScaleFactor scale_factor = ui::kScaleFactorNone;

  EXPECT_CALL(delegate_, LoadDataResourceBytes(resource_id, scale_factor))
      .Times(1)
      .WillOnce(Return(static_memory.get()));

  scoped_refptr<base::RefCountedMemory> result =
      resource_bundle->LoadDataResourceBytesForScale(resource_id, scale_factor);
  EXPECT_EQ(static_memory, result);
}

TEST_F(ResourceBundleTest, DelegateGetRawDataResource) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);

  // Create the string piece for testing purposes.
  char data[] = "My test data";
  std::string_view string_piece(data);

  int resource_id = 5;

  EXPECT_CALL(delegate_,
              GetRawDataResource(resource_id, ui::kScaleFactorNone, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<2>(string_piece), Return(true)));

  std::string_view result = resource_bundle->GetRawDataResource(resource_id);
  EXPECT_EQ(string_piece.data(), result.data());
}

TEST_F(ResourceBundleTest, IsGzipped) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath data_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));
  // Dump contents into a pak file and load it.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));
  ResourceBundle* resource_bundle = CreateResourceBundle(nullptr);
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);

  ASSERT_FALSE(resource_bundle->IsGzipped(1));
  ASSERT_FALSE(resource_bundle->IsGzipped(4));
  ASSERT_FALSE(resource_bundle->IsGzipped(6));
  ASSERT_TRUE(resource_bundle->IsGzipped(8));
  // Ask for a non-existent resource ID.
  ASSERT_FALSE(resource_bundle->IsGzipped(200));
}

TEST_F(ResourceBundleTest, IsBrotli) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath data_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("sample.pak"));
  // Dump contents into a pak file and load it.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));
  ResourceBundle* resource_bundle = CreateResourceBundle(nullptr);
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);

  ASSERT_FALSE(resource_bundle->IsBrotli(1));
  ASSERT_FALSE(resource_bundle->IsBrotli(4));
  ASSERT_TRUE(resource_bundle->IsBrotli(6));
  ASSERT_FALSE(resource_bundle->IsGzipped(6));
  ASSERT_FALSE(resource_bundle->IsBrotli(8));
  // Ask for non-existent resource ID.
  ASSERT_FALSE(resource_bundle->IsBrotli(200));
}

TEST_F(ResourceBundleTest, DelegateGetLocalizedString) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);
  std::u16string data = u"My test data";
  int resource_id = 5;

  EXPECT_CALL(delegate_, GetLocalizedString(resource_id, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(data), Return(true)));

  std::u16string result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(data, result);
}

TEST_F(ResourceBundleTest, OverrideStringResource) {
  ResourceBundle* resource_bundle = CreateResourceBundle(nullptr);

  std::u16string data = u"My test data";
  int resource_id = 5;

  std::u16string result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(std::u16string(), result);

  resource_bundle->OverrideLocaleStringResource(resource_id, data);

  result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(data, result);
}

#if DCHECK_IS_ON()
TEST_F(ResourceBundleTest, CanOverrideStringResources) {
  ResourceBundle* resource_bundle = CreateResourceBundle(nullptr);
  std::u16string data = u"My test data";
  int resource_id = 5;

  EXPECT_TRUE(
      resource_bundle->get_can_override_locale_string_resources_for_test());
  resource_bundle->GetLocalizedString(resource_id);
  EXPECT_FALSE(
      resource_bundle->get_can_override_locale_string_resources_for_test());
}
#endif

TEST_F(ResourceBundleTest, DelegateGetLocalizedStringWithOverride) {
  ResourceBundle* resource_bundle = CreateResourceBundle(&delegate_);
  std::u16string delegate_data = u"My delegate data";
  int resource_id = 5;

  EXPECT_CALL(delegate_, GetLocalizedString(resource_id, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<1>(delegate_data), Return(true)));

  std::u16string override_data = u"My override data";

  std::u16string result = resource_bundle->GetLocalizedString(resource_id);
  EXPECT_EQ(delegate_data, result);
}

TEST_F(ResourceBundleTest, LocaleDataPakExists) {
  // Check that ResourceBundle::LocaleDataPakExists returns the correct results.
  EXPECT_TRUE(ResourceBundle::LocaleDataPakExists("en-US"));
  EXPECT_FALSE(ResourceBundle::LocaleDataPakExists("not_a_real_locale"));
}

class ResourceBundleImageTest : public ResourceBundleTest {
 public:
  ResourceBundleImageTest() {}

  ResourceBundleImageTest(const ResourceBundleImageTest&) = delete;
  ResourceBundleImageTest& operator=(const ResourceBundleImageTest&) = delete;

  ~ResourceBundleImageTest() override {}

  void SetUp() override {
    ResourceBundleTest::SetUp();
    // Create a temporary directory to write test resource bundles to.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  // Returns resource bundle which uses an empty data pak for locale data.
  ResourceBundle* CreateResourceBundleWithEmptyLocalePak() {
    // Write an empty data pak for locale data.
    const base::FilePath& locale_path = dir_path().Append(
        FILE_PATH_LITERAL("locale.pak"));
    EXPECT_TRUE(
        base::WriteFile(locale_path, {kEmptyPakContents, kEmptyPakSize}));

    ui::ResourceBundle* resource_bundle = CreateResourceBundle(nullptr);

    // Load the empty locale data pak.
    resource_bundle->LoadTestResources(base::FilePath(), locale_path);
    return resource_bundle;
  }

  // Returns the path of temporary directory to write test data packs into.
  const base::FilePath& dir_path() { return temp_dir_.GetPath(); }

  // Returns the number of DataPacks managed by |resource_bundle|.
  size_t NumDataPacksInResourceBundle(ResourceBundle* resource_bundle) {
    DCHECK(resource_bundle);
    return resource_bundle->resource_handles_.size();
  }

 private:
  std::unique_ptr<DataPack> locale_pack_;
};

TEST_F(ResourceBundleImageTest, LoadDataResourceBytes) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak files.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));

  // Load pak file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, kScaleFactorNone);

  // Test normal uncompressed data.
  scoped_refptr<base::RefCountedMemory> resource =
      resource_bundle->LoadDataResourceBytes(4);
  EXPECT_EQ("this is id 4", base::as_string_view(*resource));

  // Test the brotli data.
  scoped_refptr<base::RefCountedMemory> brotli_resource =
      resource_bundle->LoadDataResourceBytes(6);
  EXPECT_EQ("this is id 6", base::as_string_view(*brotli_resource));

  // Test the gzipped data.
  scoped_refptr<base::RefCountedMemory> gzip_resource =
      resource_bundle->LoadDataResourceBytes(8);
  EXPECT_EQ("this is id 8", base::as_string_view(*gzip_resource));
}

// Verify that we don't crash when trying to load a resource that is not found.
// In some cases, we fail to mmap resources.pak, but try to keep going anyway.
TEST_F(ResourceBundleImageTest, LoadDataResourceBytesNotFound) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));

  // Dump contents into the pak files.
  ASSERT_TRUE(base::WriteFile(data_path, {kEmptyPakContents, kEmptyPakSize}));

  // Create a resource bundle from the file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);

  const int kUnfoundResourceId = 10000;
  EXPECT_EQ(nullptr,
            resource_bundle->LoadDataResourceBytes(kUnfoundResourceId));

  // Give a .pak file that doesn't exist so we will fail to load it.
  resource_bundle->AddDataPackFromPath(
      base::FilePath(FILE_PATH_LITERAL("non-existant-file.pak")),
      ui::kScaleFactorNone);
  EXPECT_EQ(nullptr,
            resource_bundle->LoadDataResourceBytes(kUnfoundResourceId));
}

TEST_F(ResourceBundleImageTest, LoadDataResourceStringForScale) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));
  base::FilePath data_2x_path =
      dir_path().Append(FILE_PATH_LITERAL("sample_2x.pak"));

  // Dump content into pak files.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));
  ASSERT_TRUE(base::WriteFile(data_2x_path, {kSampleCompressScaledPakContents,
                                             kSampleCompressScaledPakSize}));

  // Load pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);
  resource_bundle->AddDataPackFromPath(data_2x_path, k200Percent);

  // Resource ID 6 is brotlied and exists in both 1x and 2x paks, so we expect a
  // different result when requesting the 2x scale.
  EXPECT_EQ("this is id 6",
            resource_bundle->LoadDataResourceStringForScale(6, k100Percent));
  EXPECT_EQ("this is id 6 x2",
            resource_bundle->LoadDataResourceStringForScale(6, k200Percent));
}

TEST_F(ResourceBundleImageTest, LoadLocalizedResourceString) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));
  // Dump content into pak file.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));
  // Load pak file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, kScaleFactorNone);
  resource_bundle->OverrideLocalePakForTest(data_path);

  EXPECT_EQ("this is id 6", resource_bundle->LoadLocalizedResourceString(6));
  EXPECT_EQ("this is id 8", resource_bundle->LoadLocalizedResourceString(8));
}

TEST_F(ResourceBundleImageTest, LoadDataResourceString) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));
  // Dump content into pak file.
  ASSERT_TRUE(base::WriteFile(
      data_path, {kSampleCompressPakContentsV5, kSampleCompressPakSizeV5}));
  // Load pak file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, kScaleFactorNone);

  // Resource ID 6 is Brotli compressed, expect it to be uncompressed.
  EXPECT_EQ("this is id 6", resource_bundle->LoadDataResourceString(6));

  // Resource ID 8 is Gzip compressed, expect it to be uncompressed.
  EXPECT_EQ("this is id 8", resource_bundle->LoadDataResourceString(8));

  // Resource ID 4 is plain text (not compressed), expect to return as-is.
  EXPECT_EQ("this is id 4", resource_bundle->LoadDataResourceString(4));
}

TEST_F(ResourceBundleImageTest, GetRawDataResource) {
  base::FilePath data_path = dir_path().Append(FILE_PATH_LITERAL("sample.pak"));
  base::FilePath data_2x_path =
      dir_path().Append(FILE_PATH_LITERAL("sample_2x.pak"));

  // Dump contents into the pak files.
  ASSERT_TRUE(
      base::WriteFile(data_path, {kSamplePakContentsV4, kSamplePakSizeV4}));
  ASSERT_TRUE(
      base::WriteFile(data_2x_path, {kSamplePakContents2x, kSamplePakSize2x}));

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);
  resource_bundle->AddDataPackFromPath(data_2x_path, k200Percent);

  // Resource ID 4 exists in both 1x and 2x paks, so we expect a different
  // result when requesting the 2x scale.
  EXPECT_EQ("this is id 4",
            resource_bundle->GetRawDataResourceForScale(4, k100Percent));
  EXPECT_EQ("this is id 4 2x",
            resource_bundle->GetRawDataResourceForScale(4, k200Percent));

  // Resource ID 6 only exists in the 1x pak so we expect the same resource
  // for both scale factor requests.
  EXPECT_EQ("this is id 6",
            resource_bundle->GetRawDataResourceForScale(6, k100Percent));
  EXPECT_EQ("this is id 6",
            resource_bundle->GetRawDataResourceForScale(6, k200Percent));
}

// Test requesting image reps at various scale factors from the image returned
// via ResourceBundle::GetImageNamed().
TEST_F(ResourceBundleImageTest, GetImageNamed) {
#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(2.0);
#endif
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {k100Percent, k200Percent});
  base::FilePath data_1x_path = dir_path().AppendASCII("sample_1x.pak");
  base::FilePath data_2x_path = dir_path().AppendASCII("sample_2x.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_1x_path, 10, std::string_view());
  CreateDataPackWithSingleBitmap(data_2x_path, 20, std::string_view());

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_1x_path, k100Percent);
  resource_bundle->AddDataPackFromPath(data_2x_path, k200Percent);

  EXPECT_EQ(k200Percent, resource_bundle->GetMaxResourceScaleFactor());

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
  // ChromeOS/Windows load highest scale factor first.
  EXPECT_EQ(ui::k200Percent, GetSupportedResourceScaleFactor(
                                 image_skia->image_reps()[0].scale()));
#else
  EXPECT_EQ(ui::k100Percent, GetSupportedResourceScaleFactor(
                                 image_skia->image_reps()[0].scale()));
#endif

  // Resource ID 3 exists in both 1x and 2x paks. Image reps should be
  // available for both scale factors in |image_skia|.
  gfx::ImageSkiaRep image_rep = image_skia->GetRepresentation(
      GetScaleForResourceScaleFactor(ui::k100Percent));
  EXPECT_EQ(ui::k100Percent,
            GetSupportedResourceScaleFactor(image_rep.scale()));
  image_rep = image_skia->GetRepresentation(
      GetScaleForResourceScaleFactor(ui::k200Percent));
  EXPECT_EQ(ui::k200Percent,
            GetSupportedResourceScaleFactor(image_rep.scale()));

  // Requesting the 1.4x resource should return either the 1x or the 2x
  // resource.
  image_rep = image_skia->GetRepresentation(1.4f);
  ResourceScaleFactor scale_factor =
      GetSupportedResourceScaleFactor(image_rep.scale());
  EXPECT_TRUE(scale_factor == ui::k100Percent ||
              scale_factor == ui::k200Percent);

  // ImageSkia scales image if the one for the requested scale factor is not
  // available.
  EXPECT_EQ(1.4f, image_skia->GetRepresentation(1.4f).scale());
}

// Test that GetImageNamed() behaves properly for images which GRIT has
// annotated as having fallen back to 1x.
TEST_F(ResourceBundleImageTest, GetImageNamedFallback1x) {
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {k100Percent, k200Percent});
  base::FilePath data_path = dir_path().AppendASCII("sample.pak");
  base::FilePath data_2x_path = dir_path().AppendASCII("sample_2x.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_path, 10, std::string_view());
  // 2x data pack bitmap has custom chunk to indicate that the 2x bitmap is not
  // available and that GRIT fell back to 1x.
  CreateDataPackWithSingleBitmap(
      data_2x_path, 10,
      std::string_view(reinterpret_cast<const char*>(kPngScaleChunk),
                       std::size(kPngScaleChunk)));

  // Load the regular and 2x pak files.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_path, k100Percent);
  resource_bundle->AddDataPackFromPath(data_2x_path, k200Percent);

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);

  // The image rep for 2x should be available. It should be resized to the
  // proper 2x size.
  gfx::ImageSkiaRep image_rep = image_skia->GetRepresentation(
      GetScaleForResourceScaleFactor(ui::k200Percent));
  EXPECT_EQ(ui::k200Percent,
            GetSupportedResourceScaleFactor(image_rep.scale()));
  EXPECT_EQ(20, image_rep.pixel_width());
  EXPECT_EQ(20, image_rep.pixel_height());
}

TEST_F(ResourceBundleImageTest, FallbackToNone) {
  // Presents a consistent set of supported scale factors for all platforms.
  // iOS does not include k100Percent, which breaks the test below.
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {k100Percent, k200Percent, k300Percent});

  base::FilePath data_default_path = dir_path().AppendASCII("sample.pak");

  // Create the pak files.
  CreateDataPackWithSingleBitmap(data_default_path, 10, std::string_view());

  // Load the regular pak files only.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_default_path, kScaleFactorNone);

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);
  EXPECT_EQ(1u, image_skia->image_reps().size());
  EXPECT_TRUE(image_skia->image_reps()[0].unscaled());
  EXPECT_EQ(ui::k100Percent, GetSupportedResourceScaleFactor(
                                 image_skia->image_reps()[0].scale()));
}

TEST_F(ResourceBundleImageTest, Lottie) {
  // Create the pak files.
  const base::FilePath data_unscaled_path =
      dir_path().AppendASCII("sample.pak");
  const std::map<uint16_t, std::string_view> resources = {
      std::make_pair(3u, kLottieData)};
  DataPack::WritePack(data_unscaled_path, resources, ui::DataPack::BINARY);

  // Load the unscaled pack file.
  ResourceBundle* resource_bundle = CreateResourceBundleWithEmptyLocalePak();
  resource_bundle->AddDataPackFromPath(data_unscaled_path, kScaleFactorNone);

  std::optional<std::vector<uint8_t>> data = resource_bundle->GetLottieData(3);
  ASSERT_TRUE(data.has_value());
  EXPECT_TRUE(base::ranges::equal(*data, kLottieExpected));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::ResourceBundle::SetLottieParsingFunctions(
      &ParseLottieAsStillImageForTesting,
      /*parse_lottie_as_themed_still_image=*/nullptr);
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {k100Percent, k200Percent});

  gfx::ImageSkia* image_skia = resource_bundle->GetImageSkiaNamed(3);

  // Unscaled image should always return scale=1.
  EXPECT_EQ(1.f, image_skia->GetRepresentation(2.f).scale());
  EXPECT_EQ(1.f, image_skia->GetRepresentation(1.f).scale());
  EXPECT_EQ(1.f, image_skia->GetRepresentation(1.4f).scale());

  // Lottie resource should be 'unscaled'.
  EXPECT_TRUE(image_skia->image_reps()[0].unscaled());
#endif
}

}  // namespace ui
