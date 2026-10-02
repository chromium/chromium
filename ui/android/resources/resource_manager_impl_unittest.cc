// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/stub_layer_tree_host_delegate.h"
#include "cc/test/test_task_graph_runner.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/android/resources/etc1_utils.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/android/ui_android_features.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace ui {

class TestResourceManagerImpl : public ResourceManagerImpl {
 public:
  TestResourceManagerImpl(WindowAndroid* window_android)
      : ResourceManagerImpl(window_android) {}

  ~TestResourceManagerImpl() override {}

  void SetResourceAsLoaded(AndroidResourceType res_type, int res_id) {
    SkBitmap small_bitmap;
    small_bitmap.allocN32Pixels(1, 1, /*is_opaque=*/true);
    SkCanvas canvas(small_bitmap);
    canvas.drawColor(SK_ColorWHITE);
    small_bitmap.setImmutable();

    OnResourceReady(nullptr, res_type, res_id, std::move(small_bitmap), 1, 1,
                    reinterpret_cast<intptr_t>(new Resource()));
  }

 protected:
  void PreloadResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {}

  void RequestResourceFromJava(AndroidResourceType res_type,
                               int res_id) override {
    SetResourceAsLoaded(res_type, res_id);
  }
};

namespace {

const ui::SystemUIResourceType kTestResourceType = ui::OVERSCROLL_GLOW;

class MockUIResourceManager : public cc::UIResourceManager {
 public:
  MockUIResourceManager() {}

  MockUIResourceManager(const MockUIResourceManager&) = delete;
  MockUIResourceManager& operator=(const MockUIResourceManager&) = delete;

  MOCK_METHOD1(CreateUIResource, cc::UIResourceId(cc::UIResourceClient*));
  MOCK_METHOD1(DeleteUIResource, void(cc::UIResourceId));
};

}  // namespace

class ResourceManagerTest : public testing::Test {
 public:
  ResourceManagerTest()
      : window_android_(WindowAndroid::CreateForTesting()),
        resource_manager_(window_android_->get()) {
    resource_manager_.Init(&ui_resource_manager_);
  }

  void PreloadResource(ui::SystemUIResourceType type) {
    resource_manager_.PreloadResource(ui::ANDROID_RESOURCE_TYPE_SYSTEM, type);
  }

  cc::UIResourceId GetUIResourceId(ui::SystemUIResourceType type) {
    return resource_manager_.GetUIResourceId(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                             type);
  }

  void SetResourceAsLoaded(ui::SystemUIResourceType type) {
    resource_manager_.SetResourceAsLoaded(ui::ANDROID_RESOURCE_TYPE_SYSTEM,
                                          type);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<WindowAndroid::ScopedWindowAndroidForTesting> window_android_;

 protected:
  MockUIResourceManager ui_resource_manager_;
  TestResourceManagerImpl resource_manager_;
  cc::StubLayerTreeHostDelegate stub_client_;
};

TEST_F(ResourceManagerTest, GetResource) {
  const cc::UIResourceId kResourceId = 99;
  EXPECT_CALL(ui_resource_manager_, CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, PreloadEnsureResource) {
  const cc::UIResourceId kResourceId = 99;
  PreloadResource(kTestResourceType);
  EXPECT_CALL(ui_resource_manager_, CreateUIResource(_))
      .WillOnce(Return(kResourceId))
      .RetiresOnSaturation();
  SetResourceAsLoaded(kTestResourceType);
  EXPECT_EQ(kResourceId, GetUIResourceId(kTestResourceType));
}

TEST_F(ResourceManagerTest, TestOnMemoryDumpEmitsData) {
  SetResourceAsLoaded(kTestResourceType);

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump =
      std::make_unique<base::trace_event::ProcessMemoryDump>(dump_args);
  resource_manager_.OnMemoryDump(dump_args, process_memory_dump.get());
  const auto& allocator_dumps = process_memory_dump->allocator_dumps();
  const char* system_allocator_pool_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  size_t kExpectedDumpCount = 10;
  EXPECT_EQ(kExpectedDumpCount, allocator_dumps.size());
  for (const auto& dump : allocator_dumps) {
    ASSERT_TRUE(dump.first.find("ui/resource_manager") == 0 ||
                dump.first.find(system_allocator_pool_name) == 0);
  }
}

TEST_F(ResourceManagerTest, Etc1DecompressBitmapBitExactness) {
  const gfx::Size kTestSizes[] = {
      gfx::Size(1, 1),
      gfx::Size(19, 21),
      gfx::Size(32, 24),
      gfx::Size(35, 7),
  };

  for (bool supports_npot : {true, false}) {
    for (const gfx::Size& content_size : kTestSizes) {
      SkBitmap input;
      ASSERT_TRUE(input.tryAllocN32Pixels(content_size.width(),
                                          content_size.height(),
                                          /*is_opaque=*/true));
      for (int y = 0; y < content_size.height(); ++y) {
        for (int x = 0; x < content_size.width(); ++x) {
          const int block_x = x / 4;
          const int block_y = y / 4;
          uint8_t r = 0;
          uint8_t g = 0;
          uint8_t b = 0;
          if (((block_x + block_y) & 1) == 0) {
            // Smooth gradient blocks to exercise differential mode (diff = 1).
            r = static_cast<uint8_t>((x * 7 + y * 5) & 0xFF);
            g = static_cast<uint8_t>((x * 3 + y * 11 + 64) & 0xFF);
            b = static_cast<uint8_t>((x * 5 + y * 9 + 128) & 0xFF);
          } else {
            // High-contrast 2x2 subblock quadrants to force individual mode
            // (diff = 0) as well as both horizontal and vertical flip splits.
            const size_t sub = static_cast<size_t>(((x & 2) >> 1) | (y & 2));
            constexpr std::array<std::array<uint8_t, 3>, 4> kColors = {{
                {255, 0, 0},
                {0, 255, 0},
                {0, 0, 255},
                {255, 255, 255},
            }};
            r = kColors[sub][0];
            g = kColors[sub][1];
            b = kColors[sub][2];
          }
          *input.getAddr32(x, y) = (0xFFu << 24) |
                                   (static_cast<uint32_t>(b) << 16) |
                                   (static_cast<uint32_t>(g) << 8) | r;
        }
      }
      input.setImmutable();

      sk_sp<SkPixelRef> compressed = Etc1::CompressBitmap(input, supports_npot);
      ASSERT_TRUE(compressed);

      SkBitmap decoded_legacy;
      {
        base::test::ScopedFeatureList features;
        features.InitAndDisableFeature(kUseNewEtc1Decoder);
        decoded_legacy = Etc1::DecompressBitmap(content_size, compressed);
      }
      ASSERT_FALSE(decoded_legacy.empty());
      EXPECT_EQ(decoded_legacy.width(), content_size.width());
      EXPECT_EQ(decoded_legacy.height(), content_size.height());

      SkBitmap decoded_rust;
      {
        base::test::ScopedFeatureList features;
        features.InitAndEnableFeature(kUseNewEtc1Decoder);
        decoded_rust = Etc1::DecompressBitmap(content_size, compressed);
      }
      ASSERT_FALSE(decoded_rust.empty());
      EXPECT_TRUE(decoded_rust.isImmutable());
      EXPECT_EQ(decoded_rust.width(), content_size.width());
      EXPECT_EQ(decoded_rust.height(), content_size.height());
      EXPECT_EQ(decoded_rust.colorType(), decoded_legacy.colorType());
      EXPECT_EQ(decoded_rust.alphaType(), decoded_legacy.alphaType());
      EXPECT_EQ(decoded_rust.rowBytes(), decoded_legacy.rowBytes());

      for (int y = 0; y < content_size.height(); ++y) {
        for (int x = 0; x < content_size.width(); ++x) {
          EXPECT_EQ(*decoded_legacy.getAddr32(x, y),
                    *decoded_rust.getAddr32(x, y))
              << "Mismatch at (" << x << ", " << y
              << ") for size=" << content_size.ToString()
              << " supports_npot=" << supports_npot;
        }
      }
    }
  }
}

TEST_F(ResourceManagerTest, Etc1DecompressBitmapSyntheticBlocksBitExactness) {
  // Construct 32 synthetic 4x4 ETC1 blocks (8x4 blocks = 32x16 pixels) that
  // exhaustively cover:
  // - diff = 0 (individual 4-bit base colors) and diff = 1 (differential 5-bit
  //   base colors + 3-bit signed two's-complement deltas in [-4, 3])
  // - flip = 0 (2x4 subblocks) and flip = 1 (4x2 subblocks)
  // - all 8 modifier tables (0..7) for both subblock 1 and subblock 2
  // - all 16 texel positions (k = 0..15) with all 4 selector indices (0..3),
  //   including saturation/clamping at 0 and 255.
  constexpr int kBlocksX = 8;
  constexpr int kBlocksY = 4;
  constexpr int kNumBlocks = kBlocksX * kBlocksY;
  constexpr int kWidth = kBlocksX * 4;
  constexpr int kHeight = kBlocksY * 4;
  constexpr size_t kRowBytes = kWidth / 2;

  sk_sp<SkData> etc1_data = SkData::MakeUninitialized(kNumBlocks * 8);
  base::span<uint8_t> data_span = skia::as_writable_byte_span(*etc1_data);

  for (int block_idx = 0; block_idx < kNumBlocks; ++block_idx) {
    const uint64_t diff = (block_idx >= 16) ? 1u : 0u;
    const uint64_t flip = ((block_idx / 8) & 1) ? 1u : 0u;
    const uint64_t table1 = static_cast<uint64_t>(block_idx % 8);
    const uint64_t table2 = static_cast<uint64_t>((block_idx * 3 + 1) % 8);

    uint64_t high = (table1 << 5) | (table2 << 2) | (diff << 1) | flip;
    if (diff == 0) {
      // Individual mode: 4-bit R1, R2, G1, G2, B1, B2 (spanning 0..15 to test
      // both mid-range and clamping at 0 and 255).
      const uint64_t r1 = static_cast<uint64_t>(block_idx & 0xF);
      const uint64_t r2 = static_cast<uint64_t>((block_idx * 3 + 5) & 0xF);
      const uint64_t g1 = static_cast<uint64_t>((block_idx * 7 + 2) & 0xF);
      const uint64_t g2 = static_cast<uint64_t>((15 - (block_idx & 0xF)) & 0xF);
      const uint64_t b1 = static_cast<uint64_t>((block_idx * 5 + 9) & 0xF);
      const uint64_t b2 = static_cast<uint64_t>((block_idx * 11 + 1) & 0xF);
      high |= (r1 << 28) | (r2 << 24) | (g1 << 20) | (g2 << 16) | (b1 << 12) |
              (b2 << 8);
    } else {
      // Differential mode: 5-bit base in [4, 28] so adding any 3-bit signed
      // two's-complement delta in [-4, 3] (encoded as 0..7) stays within the
      // valid [0, 31] range for both C and Rust decoders.
      const uint64_t r_base = static_cast<uint64_t>(4 + (block_idx % 25));
      const uint64_t g_base = static_cast<uint64_t>(4 + ((block_idx * 5) % 25));
      const uint64_t b_base = static_cast<uint64_t>(4 + ((block_idx * 9) % 25));
      const uint64_t dr = static_cast<uint64_t>(block_idx & 0x7);
      const uint64_t dg = static_cast<uint64_t>((block_idx + 3) & 0x7);
      const uint64_t db = static_cast<uint64_t>((block_idx + 6) & 0x7);
      high |= (r_base << 27) | (dr << 24) | (g_base << 19) | (dg << 16) |
              (b_base << 11) | (db << 8);
    }

    uint64_t msb = 0;
    uint64_t lsb = 0;
    for (int k = 0; k < 16; ++k) {
      const uint64_t selector = static_cast<uint64_t>((k + block_idx) & 0b11);
      lsb |= (selector & 1u) << k;
      msb |= ((selector >> 1) & 1u) << k;
    }
    const uint64_t low = (msb << 16) | lsb;
    const uint64_t block = (high << 32) | low;

    data_span.subspan(static_cast<size_t>(block_idx) * 8u, 8u)
        .copy_from(base::U64ToBigEndian(block));
  }

  SkImageInfo info = SkImageInfo::Make(kWidth, kHeight, kUnknown_SkColorType,
                                       kUnpremul_SkAlphaType);
  sk_sp<SkPixelRef> compressed =
      SkMallocPixelRef::MakeWithData(info, kRowBytes, std::move(etc1_data));
  ASSERT_TRUE(compressed);
  compressed->setImmutable();

  // Verify both full buffer size (32x16) and cropped content size (29x13).
  for (const gfx::Size& content_size :
       {gfx::Size(kWidth, kHeight), gfx::Size(29, 13)}) {
    SkBitmap decoded_legacy;
    {
      base::test::ScopedFeatureList features;
      features.InitAndDisableFeature(kUseNewEtc1Decoder);
      decoded_legacy = Etc1::DecompressBitmap(content_size, compressed);
    }
    ASSERT_FALSE(decoded_legacy.empty());

    SkBitmap decoded_rust;
    {
      base::test::ScopedFeatureList features;
      features.InitAndEnableFeature(kUseNewEtc1Decoder);
      decoded_rust = Etc1::DecompressBitmap(content_size, compressed);
    }
    ASSERT_FALSE(decoded_rust.empty());
    EXPECT_TRUE(decoded_rust.isImmutable());
    EXPECT_EQ(decoded_rust.colorType(), decoded_legacy.colorType());
    EXPECT_EQ(decoded_rust.alphaType(), decoded_legacy.alphaType());
    EXPECT_EQ(decoded_rust.rowBytes(), decoded_legacy.rowBytes());

    for (int y = 0; y < content_size.height(); ++y) {
      for (int x = 0; x < content_size.width(); ++x) {
        EXPECT_EQ(*decoded_legacy.getAddr32(x, y),
                  *decoded_rust.getAddr32(x, y))
            << "Mismatch at (" << x << ", " << y
            << ") for content_size=" << content_size.ToString();
      }
    }
  }
}

TEST_F(ResourceManagerTest, Etc1DecompressBitmapInvalidInputs) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kUseNewEtc1Decoder);

  SkBitmap valid_input;
  ASSERT_TRUE(valid_input.tryAllocN32Pixels(8, 8, /*is_opaque=*/true));
  valid_input.eraseColor(SK_ColorBLUE);
  valid_input.setImmutable();
  sk_sp<SkPixelRef> valid_compressed =
      Etc1::CompressBitmap(valid_input, /*supports_etc_npot=*/true);
  ASSERT_TRUE(valid_compressed);

  // Empty content_size.
  EXPECT_TRUE(
      Etc1::DecompressBitmap(gfx::Size(0, 8), valid_compressed).empty());
  EXPECT_TRUE(
      Etc1::DecompressBitmap(gfx::Size(8, 0), valid_compressed).empty());

  // Null compressed_data.
  EXPECT_TRUE(Etc1::DecompressBitmap(gfx::Size(8, 8), nullptr).empty());

  // content_size larger than buffer_size (8x8).
  EXPECT_TRUE(
      Etc1::DecompressBitmap(gfx::Size(9, 8), valid_compressed).empty());
  EXPECT_TRUE(
      Etc1::DecompressBitmap(gfx::Size(8, 9), valid_compressed).empty());

  // Non-multiple-of-4 buffer_size (e.g. 6x8).
  sk_sp<SkData> bad_dim_data = SkData::MakeZeroInitialized(6 * 8 / 2);
  sk_sp<SkPixelRef> bad_dim_ref = SkMallocPixelRef::MakeWithData(
      SkImageInfo::Make(6, 8, kUnknown_SkColorType, kUnpremul_SkAlphaType),
      /*rowBytes=*/3, std::move(bad_dim_data));
  ASSERT_TRUE(bad_dim_ref);
  EXPECT_TRUE(Etc1::DecompressBitmap(gfx::Size(4, 4), bad_dim_ref).empty());

  // Wrong rowBytes (expected 8 / 2 = 4, pass 8).
  sk_sp<SkData> bad_stride_data = SkData::MakeZeroInitialized(8 * 8);
  sk_sp<SkPixelRef> bad_stride_ref = SkMallocPixelRef::MakeWithData(
      SkImageInfo::Make(8, 8, kUnknown_SkColorType, kUnpremul_SkAlphaType),
      /*rowBytes=*/8, std::move(bad_stride_data));
  ASSERT_TRUE(bad_stride_ref);
  EXPECT_TRUE(Etc1::DecompressBitmap(gfx::Size(8, 8), bad_stride_ref).empty());
}
}  // namespace ui
