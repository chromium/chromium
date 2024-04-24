// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_UNITTEST_CC_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_UNITTEST_CC_

#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"

#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"

namespace ui {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

Matcher<const CrtcConnectorPair&> PairEq(const CrtcConnectorPair& value) {
  return AllOf(Field(&CrtcConnectorPair::connector_id, Eq(value.connector_id)),
               Field(&CrtcConnectorPair::crtc_id, Eq(value.crtc_id)));
}

FakeDrmDevice::ConnectorProperties& CreateConnectorWithPossibleCrtcs(
    raw_ptr<FakeDrmDevice> drm,
    uint32_t possible_crtcs) {
  FakeDrmDevice::EncoderProperties& encoder = drm->AddEncoder();
  encoder.possible_crtcs = possible_crtcs;
  const uint32_t encoder_id = encoder.id;

  FakeDrmDevice::ConnectorProperties& connector = drm->AddConnector();
  connector.connection = true;
  connector.encoders = std::vector<uint32_t>{encoder_id};
  return connector;
}

}  // namespace

class DrmGpuUtilTest : public testing::Test {
 public:
  DrmGpuUtilTest() {
    fake_device_generator_ = std::make_unique<FakeDrmDeviceGenerator>();
    device_ = fake_device_generator_->CreateDevice(
        base::FilePath("/test/dri/card0"), base::ScopedFD(),
        /*is_primary_device=*/true);
    fake_drm_ = static_cast<FakeDrmDevice*>(device_.get());
  }

  ControllerConfigParams CreateFakeParamWithConnectorId(uint32_t connector_id) {
    return ControllerConfigParams(/*display_id=*/0, device_, /*crtc=*/0,
                                  connector_id, /*origin=*/gfx::Point(0, 0),
                                  /*pmode=*/nullptr);
  }

  std::unique_ptr<DrmDeviceGenerator> fake_device_generator_;
  scoped_refptr<DrmDevice> device_;
  raw_ptr<FakeDrmDevice> fake_drm_;
};

// TODO(b/322831691): Deterministic failure.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_EmptyPossibleCrtcsForConnector \
  DISABLED_EmptyPossibleCrtcsForConnector
#else
#define MAYBE_EmptyPossibleCrtcsForConnector EmptyPossibleCrtcsForConnector
#endif
TEST_F(DrmGpuUtilTest, MAYBE_EmptyPossibleCrtcsForConnector) {
  EXPECT_DEATH_IF_SUPPORTED(GetAllCrtcConnectorPermutations(*fake_drm_, {}),
                            "No connectors specified");
}

TEST_F(DrmGpuUtilTest, EmptyPossibleCrtcs) {
  fake_drm_->ResetStateWithAllProperties();

  uint32_t connector_1 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0).id;
  uint32_t connector_2 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0).id;

  fake_drm_->InitializeState(/*use_atomic*/ true);

  EXPECT_THAT(GetAllCrtcConnectorPermutations(
                  *fake_drm_, {CreateFakeParamWithConnectorId(connector_1),
                               CreateFakeParamWithConnectorId(connector_2)}),
              IsEmpty());
}

TEST_F(DrmGpuUtilTest, ThreeConnectorsSameThreeCrtcs) {
  fake_drm_->ResetStateWithAllProperties();

  // Add 3 CRTCs
  uint32_t crtc_1 = fake_drm_->AddCrtc().id;
  uint32_t crtc_2 = fake_drm_->AddCrtc().id;
  uint32_t crtc_3 = fake_drm_->AddCrtc().id;

  // Add 3 encoders and connectors that can use all 3 CRTCs.
  uint32_t connector_1 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;
  uint32_t connector_2 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;
  uint32_t connector_3 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;

  fake_drm_->InitializeState(/*use_atomic*/ true);

  // 3! = 6 permutations
  //   {{crtc_1, connector_1}, {crtc_2, connector_2}, {crtc_3, connector_3}},
  //   {{crtc_1, connector_1}, {crtc_3, connector_2}, {crtc_2, connector_3}},
  //   {{crtc_2, connector_1}, {crtc_1, connector_2}, {crtc_3, connector_3}},
  //   {{crtc_2, connector_1}, {crtc_3, connector_2}, {crtc_1, connector_3}},
  //   {{crtc_3, connector_1}, {crtc_2, connector_2}, {crtc_1, connector_3}},
  //   {{crtc_3, connector_1}, {crtc_1, connector_2}, {crtc_2, connector_3}}
  EXPECT_THAT(GetAllCrtcConnectorPermutations(
                  *fake_drm_, {CreateFakeParamWithConnectorId(connector_1),
                               CreateFakeParamWithConnectorId(connector_2),
                               CreateFakeParamWithConnectorId(connector_3)}),
              UnorderedElementsAre(
                  UnorderedElementsAre(PairEq({crtc_1, connector_1}),
                                       PairEq({crtc_2, connector_2}),
                                       PairEq({crtc_3, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_1, connector_1}),
                                       PairEq({crtc_3, connector_2}),
                                       PairEq({crtc_2, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_2, connector_1}),
                                       PairEq({crtc_1, connector_2}),
                                       PairEq({crtc_3, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_2, connector_1}),
                                       PairEq({crtc_3, connector_2}),
                                       PairEq({crtc_1, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_3, connector_1}),
                                       PairEq({crtc_2, connector_2}),
                                       PairEq({crtc_1, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_3, connector_1}),
                                       PairEq({crtc_1, connector_2}),
                                       PairEq({crtc_2, connector_3}))));
}

// GetAllCrtcConnectorPermutations() should filter out permutations where not
// all connectors are assigned a CRTC (But the inverse of not all CRTCs being
// assigned connectors is OK).
TEST_F(DrmGpuUtilTest, FilterConnectorWithoutCrtcPermutaitons) {
  fake_drm_->ResetStateWithAllProperties();

  // Add 2 CRTCs
  fake_drm_->AddCrtc();
  fake_drm_->AddCrtc();

  // Add 3 encoders and connectors that can use all 2 CRTCs.
  uint32_t connector_1 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b11).id;
  uint32_t connector_2 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b11).id;
  uint32_t connector_3 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b11).id;

  fake_drm_->InitializeState(/*use_atomic*/ true);

  EXPECT_THAT(GetAllCrtcConnectorPermutations(
                  *fake_drm_, {CreateFakeParamWithConnectorId(connector_1),
                               CreateFakeParamWithConnectorId(connector_2),
                               CreateFakeParamWithConnectorId(connector_3)}),
              IsEmpty());
}

TEST_F(DrmGpuUtilTest, MoreCrtcsThanConnectors) {
  fake_drm_->ResetStateWithAllProperties();

  // Add 3 CRTCs
  uint32_t crtc_1 = fake_drm_->AddCrtc().id;
  uint32_t crtc_2 = fake_drm_->AddCrtc().id;
  uint32_t crtc_3 = fake_drm_->AddCrtc().id;

  // Add 2 encoders and connectors that can use all 3 CRTCs.
  uint32_t connector_1 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;
  uint32_t connector_2 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;

  fake_drm_->InitializeState(/*use_atomic*/ true);

  // 2 * 3 = 6 permutations
  //   {{crtc_1, connector_1}, {crtc_2, connector_2},
  //   {{crtc_1, connector_1}, {crtc_3, connector_2},
  //   {{crtc_2, connector_1}, {crtc_1, connector_2},
  //   {{crtc_2, connector_1}, {crtc_3, connector_2},
  //   {{crtc_3, connector_1}, {crtc_2, connector_2},
  //   {{crtc_3, connector_1}, {crtc_1, connector_2}
  EXPECT_THAT(GetAllCrtcConnectorPermutations(
                  *fake_drm_, {CreateFakeParamWithConnectorId(connector_1),
                               CreateFakeParamWithConnectorId(connector_2)}),
              UnorderedElementsAre(
                  UnorderedElementsAre(PairEq({crtc_1, connector_1}),
                                       PairEq({crtc_2, connector_2})),
                  UnorderedElementsAre(PairEq({crtc_1, connector_1}),
                                       PairEq({crtc_3, connector_2})),
                  UnorderedElementsAre(PairEq({crtc_2, connector_1}),
                                       PairEq({crtc_3, connector_2})),
                  UnorderedElementsAre(PairEq({crtc_2, connector_1}),
                                       PairEq({crtc_1, connector_2})),
                  UnorderedElementsAre(PairEq({crtc_3, connector_1}),
                                       PairEq({crtc_1, connector_2})),
                  UnorderedElementsAre(PairEq({crtc_3, connector_1}),
                                       PairEq({crtc_2, connector_2}))));
}

TEST_F(DrmGpuUtilTest, VaryingPossibleCrtcs) {
  fake_drm_->ResetStateWithAllProperties();

  // Add 3 CRTCs
  uint32_t crtc_1 = fake_drm_->AddCrtc().id;
  uint32_t crtc_2 = fake_drm_->AddCrtc().id;
  uint32_t crtc_3 = fake_drm_->AddCrtc().id;

  // Add 3 encoders and connectors that uses combination of the above 3 CRTCs.
  uint32_t connector_1 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b111).id;
  uint32_t connector_2 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b011).id;
  uint32_t connector_3 =
      CreateConnectorWithPossibleCrtcs(fake_drm_, /*possible_crtcs=*/0b101).id;

  fake_drm_->InitializeState(/*use_atomic*/ true);

  //   {{crtc_1, connector_1}, {crtc_2, connector_2}, {crtc_3, connector_3}},
  //   {{crtc_2, connector_1}, {crtc_1, connector_2}, {crtc_3, connector_3}},
  //   {{crtc_3, connector_1}, {crtc_2, connector_2}, {crtc_1, connector_3}}
  EXPECT_THAT(GetAllCrtcConnectorPermutations(
                  *fake_drm_, {CreateFakeParamWithConnectorId(connector_1),
                               CreateFakeParamWithConnectorId(connector_2),
                               CreateFakeParamWithConnectorId(connector_3)}),
              UnorderedElementsAre(
                  UnorderedElementsAre(PairEq({crtc_1, connector_1}),
                                       PairEq({crtc_2, connector_2}),
                                       PairEq({crtc_3, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_2, connector_1}),
                                       PairEq({crtc_1, connector_2}),
                                       PairEq({crtc_3, connector_3})),
                  UnorderedElementsAre(PairEq({crtc_3, connector_1}),
                                       PairEq({crtc_2, connector_2}),
                                       PairEq({crtc_1, connector_3}))));
}
}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_UTIL_UNITTEST_CC_
