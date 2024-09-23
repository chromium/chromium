// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/hdr_metadata_helper_win.h"

#include "base/compiler_specific.h"
#include "media/base/win/d3d11_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
// using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
// using ::testing::SaveArg;
using ::testing::SetArgPointee;

namespace gl {

class HDRMetadataHelperWinTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_dxgi_factory_ = media::MakeComPtr<NiceMock<media::DXGIFactoryMock>>();
    ON_CALL(*mock_dxgi_factory_.Get(), EnumAdapters(_, _))
        .WillByDefault(Return(DXGI_ERROR_NOT_FOUND));

    mock_dxgi_device_ = media::MakeComPtr<NiceMock<media::DXGIDeviceMock>>();

    mock_d3d11_device_ = media::MakeComPtr<NiceMock<media::D3D11DeviceMock>>();
    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface(IID_IDXGIDevice, _))
        .WillByDefault(
            media::SetComPointeeAndReturnOk<1>(mock_dxgi_device_.Get()));
  }

  std::unique_ptr<HDRMetadataHelperWin> CreateHelper() {
    // Set the D3D11 device's adapter to the first one, somewhat arbitrarily.
    ON_CALL(*mock_dxgi_device_.Get(), GetAdapter(_))
        .WillByDefault(
            media::SetComPointeeAndReturnOk<0>(mock_dxgi_adapters_[0].Get()));

    return std::make_unique<HDRMetadataHelperWin>(mock_d3d11_device_);
  }

  // Adds an adapter that |mock_dxgi_factory_| will enumerate.
  void AddAdapter() {
    Microsoft::WRL::ComPtr<media::DXGIAdapterMock> dxgi_adapter =
        media::MakeComPtr<NiceMock<media::DXGIAdapterMock>>();
    ON_CALL(*dxgi_adapter.Get(), GetParent(_, _))
        .WillByDefault(
            media::SetComPointeeAndReturnOk<1>(mock_dxgi_factory_.Get()));

    // By default, the adapter has no outputs.
    ON_CALL(*dxgi_adapter.Get(), EnumOutputs(_, _))
        .WillByDefault(Return(DXGI_ERROR_NOT_FOUND));

    // Make the factory enumerate this adapter.
    ON_CALL(*mock_dxgi_factory_.Get(),
            EnumAdapters(mock_dxgi_adapters_.size(), _))
        .WillByDefault(media::SetComPointeeAndReturnOk<1>(dxgi_adapter.Get()));

    mock_dxgi_adapters_.push_back(std::move(dxgi_adapter));
  }

  // Add |desc1| to the most recent adapter.
  // Note that, for now, this can only be called once, since we only set
  // an expectation for output 0.
  void AddOutput(const DXGI_OUTPUT_DESC1& desc1) {
    // Create a DXGIOutput6 that can return |desc1|.
    Microsoft::WRL::ComPtr<media::DXGIOutput6Mock> output6 =
        media::MakeComPtr<media::DXGIOutput6Mock>();
    mock_dxgi_output6s_.push_back(output6);
    ON_CALL(*output6.Get(), GetDesc1(_))
        .WillByDefault(DoAll(SetArgPointee<0>(desc1), Return(S_OK)));

    // Tell the current adapter to return |output6| as its 0-th output.
    auto& dxgi_adapter = mock_dxgi_adapters_[mock_dxgi_adapters_.size() - 1];
    ON_CALL(*dxgi_adapter.Get(), EnumOutputs(0, _))
        .WillByDefault(media::SetComPointeeAndReturnOk<1>(output6.Get()));
  }

  Microsoft::WRL::ComPtr<media::D3D11DeviceMock> mock_d3d11_device_;
  Microsoft::WRL::ComPtr<media::DXGIDeviceMock> mock_dxgi_device_;
  Microsoft::WRL::ComPtr<media::DXGIFactoryMock> mock_dxgi_factory_;
  std::vector<Microsoft::WRL::ComPtr<media::DXGIAdapterMock>>
      mock_dxgi_adapters_;
  std::vector<Microsoft::WRL::ComPtr<media::DXGIOutput6Mock>>
      mock_dxgi_output6s_;
  std::vector<Microsoft::WRL::ComPtr<media::DXGIOutputMock>> mock_dxgi_outputs_;
};

TEST_F(HDRMetadataHelperWinTest, CachesMetadataIfAvailable) {
  // Set up two adapters with one monitor each.
  AddAdapter();
  DXGI_OUTPUT_DESC1 desc{};
  desc.RedPrimary[0] = 0.1;
  // SAFETY: required from Windows API.
  UNSAFE_BUFFERS(desc.RedPrimary[1]) = 0.2;
  desc.GreenPrimary[0] = 0.3;
  UNSAFE_BUFFERS(desc.GreenPrimary[1]) = 0.4;
  desc.BluePrimary[0] = 0.5;
  UNSAFE_BUFFERS(desc.BluePrimary[1]) = 0.6;
  desc.WhitePoint[0] = 0.7;
  UNSAFE_BUFFERS(desc.WhitePoint[1]) = 0.8;
  desc.MinLuminance = 0.9;
  desc.MaxLuminance = 1.0;
  desc.MaxFullFrameLuminance = 100;
  desc.Monitor = static_cast<HMONITOR>(malloc(sizeof(HMONITOR)));
  AddOutput(desc);
  AddAdapter();
  DXGI_OUTPUT_DESC1 desc_second = desc;
  // Make the second one less bright.
  desc_second.RedPrimary[0] = 0.5;
  desc_second.MaxLuminance /= 2.0;
  desc_second.Monitor = static_cast<HMONITOR>(malloc(sizeof(HMONITOR)));
  AddOutput(desc_second);
  auto helper = CreateHelper();
  auto result = helper->GetDisplayMetadata();
  EXPECT_TRUE(result);
  // From MSDN.
  static constexpr int kPrimariesFixedPoint = 50000;
  static constexpr int kMinLuminanceFixedPoint = 10000;
  EXPECT_EQ(result->RedPrimary[0],
            static_cast<int>(desc.RedPrimary[0] * kPrimariesFixedPoint));
  // SAFETY: required from Windows API.
  EXPECT_EQ(UNSAFE_BUFFERS(result->RedPrimary[1]),
            static_cast<int>(UNSAFE_BUFFERS(desc.RedPrimary[1]) *
                             kPrimariesFixedPoint));
  EXPECT_EQ(result->GreenPrimary[0],
            static_cast<int>(desc.GreenPrimary[0] * kPrimariesFixedPoint));
  EXPECT_EQ(UNSAFE_BUFFERS(result->GreenPrimary[1]),
            static_cast<int>(UNSAFE_BUFFERS(desc.GreenPrimary[1]) *
                             kPrimariesFixedPoint));
  EXPECT_EQ(result->BluePrimary[0],
            static_cast<int>(desc.BluePrimary[0] * kPrimariesFixedPoint));
  EXPECT_EQ(UNSAFE_BUFFERS(result->BluePrimary[1]),
            static_cast<int>(UNSAFE_BUFFERS(desc.BluePrimary[1]) *
                             kPrimariesFixedPoint));
  EXPECT_EQ(result->WhitePoint[0],
            static_cast<int>(desc.WhitePoint[0] * kPrimariesFixedPoint));
  EXPECT_EQ(UNSAFE_BUFFERS(result->WhitePoint[1]),
            static_cast<int>(UNSAFE_BUFFERS(desc.WhitePoint[1]) *
                             kPrimariesFixedPoint));
  EXPECT_EQ(result->MaxMasteringLuminance,
            static_cast<unsigned>(desc.MaxLuminance));
  EXPECT_EQ(result->MinMasteringLuminance,
            static_cast<unsigned>(desc.MinLuminance * kMinLuminanceFixedPoint));
  EXPECT_EQ(result->MaxContentLightLevel, desc.MaxFullFrameLuminance);
  EXPECT_EQ(result->MaxFrameAverageLightLevel, desc.MaxFullFrameLuminance);
}

TEST_F(HDRMetadataHelperWinTest, DoesntCacheMetadataIfNotAvailble) {
  // Add an empty adapter.
  AddAdapter();
  auto helper = CreateHelper();
  EXPECT_FALSE(helper->GetDisplayMetadata());
}

}  // namespace gl
