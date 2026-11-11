// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

#include <color-management-v1-server-protocol.h>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/test/bind.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_wp_color_management_output.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_wp_color_manager.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

namespace {

constexpr int32_t ToWaylandChromaticity(float value) {
  return static_cast<int32_t>(value * 1000000);
}

std::array<int32_t, 8> ToWaylandPrimaries(const SkColorSpacePrimaries& p) {
  return {ToWaylandChromaticity(p.fRX), ToWaylandChromaticity(p.fRY),
          ToWaylandChromaticity(p.fGX), ToWaylandChromaticity(p.fGY),
          ToWaylandChromaticity(p.fBX), ToWaylandChromaticity(p.fBY),
          ToWaylandChromaticity(p.fWX), ToWaylandChromaticity(p.fWY)};
}

}  // namespace

class WaylandWpColorManagerTest : public WaylandTestSimple {
 public:
  WaylandWpColorManagerTest()
      : WaylandTestSimple({.supports_color_management = true}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();
    ASSERT_TRUE(connection_->wp_color_manager());
    ASSERT_TRUE(connection_->wp_color_manager()->ready());
  }

 protected:
  // Creates an image description directly through the color manager, as is
  // done for overlay planes, and returns the parameters the server received.
  wl::TestImageDescriptionParams CreateImageDescription(
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata) {
    bool created = false;
    auto result = connection_->wp_color_manager()->GetImageDescription(
        color_space, hdr_metadata,
        base::BindLambdaForTesting(
            [&](scoped_refptr<WaylandWpImageDescription> description) {
              created = !!description;
            }));
    if (result) {
      created = !!*result;
    }
    SyncDisplay();
    EXPECT_TRUE(created);

    wl::TestImageDescriptionParams params;
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      const auto& descriptions =
          server->wp_color_manager()->created_descriptions();
      ASSERT_FALSE(descriptions.empty());
      params = descriptions.back();
    });
    return params;
  }

  // Sets the image description of the root surface, as is done for the
  // primary plane, and returns the parameters the server received.
  std::optional<wl::TestImageDescriptionParams> SetRootSurfaceImageDescription(
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata) {
    WaylandSurface* surface = window_->root_surface();
    surface->SetImageDescription(color_space, hdr_metadata);
    surface->ApplyPendingState();
    surface->Commit();
    // One roundtrip for the image description to become ready, and one for
    // the resulting set_image_description request.
    SyncDisplay();
    SyncDisplay();

    std::optional<wl::TestImageDescriptionParams> params;
    const uint32_t surface_id = surface->get_surface_id();
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      ASSERT_TRUE(mock_surface);
      params = server->wp_color_manager()->GetSurfaceImageDescription(
          mock_surface->resource());
    });
    return params;
  }
};

// The primary plane with HDR content describes its post-tone-mapping peak:
// the SDR white level scaled by the display's headroom. The compositor must
// receive that peak as max_cll instead of the reference luminance, otherwise
// compositors that clip to the content's peak (e.g. KWin 6.8+) clip all HDR
// highlights to SDR white.
TEST_F(WaylandWpColorManagerTest, PrimaryPlaneWithHdrContent) {
  constexpr float kSdrWhite = 500.f;
  constexpr float kPeak = 1000.f;
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(kSdrWhite);
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = color_space.GetPrimaries(),
          .fMaximumDisplayMasteringLuminance = kPeak,
          .fMinimumDisplayMasteringLuminance = 0.f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = kPeak,
                                          .fMaxFALL = kSdrWhite});

  auto params = SetRootSurfaceImageDescription(color_space, hdr_metadata);
  ASSERT_TRUE(params);
  EXPECT_EQ(params->tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
  EXPECT_EQ(params->primaries_named, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
  ASSERT_TRUE(params->luminances);
  EXPECT_EQ(params->luminances->min, 0u);
  // The max luminance is the content peak, not 10000, even for PQ. Some
  // compositors (e.g. Hyprland) tone map when it exceeds the display peak.
  EXPECT_EQ(params->luminances->max, kPeak);
  EXPECT_EQ(params->luminances->reference, kSdrWhite);
  EXPECT_EQ(params->mastering_display_primaries,
            ToWaylandPrimaries(color_space.GetPrimaries()));
  ASSERT_TRUE(params->mastering_luminance);
  EXPECT_EQ(params->mastering_luminance->min, 0u);
  EXPECT_EQ(params->mastering_luminance->max, kPeak);
  EXPECT_EQ(params->max_cll, kPeak);
  EXPECT_EQ(params->max_fall, kSdrWhite);
}

// The primary plane without HDR content (but in an HDR color space) reports a
// peak of SDR white, so compositors don't tone map SDR content.
TEST_F(WaylandWpColorManagerTest, PrimaryPlaneWithSdrContent) {
  constexpr float kSdrWhite = 203.f;
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(kSdrWhite);
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = color_space.GetPrimaries(),
          .fMaximumDisplayMasteringLuminance = kSdrWhite,
          .fMinimumDisplayMasteringLuminance = 0.f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = kSdrWhite,
                                          .fMaxFALL = kSdrWhite});

  auto params = SetRootSurfaceImageDescription(color_space, hdr_metadata);
  ASSERT_TRUE(params);
  ASSERT_TRUE(params->luminances);
  EXPECT_EQ(params->luminances->max, kSdrWhite);
  EXPECT_EQ(params->luminances->reference, kSdrWhite);
  ASSERT_TRUE(params->mastering_luminance);
  EXPECT_EQ(params->mastering_luminance->max, kSdrWhite);
  EXPECT_EQ(params->max_cll, kSdrWhite);
  EXPECT_EQ(params->max_fall, kSdrWhite);
}

// An overlay (e.g. a video) forwards its stream's MDCV and CLLI metadata.
TEST_F(WaylandWpColorManagerTest, OverlayWithMdcvAndClli) {
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateHDR10();
  const SkColorSpacePrimaries mastering_primaries =
      SkNamedPrimaries::kSMPTE_EG_432_1;
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = mastering_primaries,
          .fMaximumDisplayMasteringLuminance = 4000.f,
          .fMinimumDisplayMasteringLuminance = 0.005f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = 1500.f,
                                          .fMaxFALL = 400.f});

  auto params = CreateImageDescription(color_space, hdr_metadata);
  EXPECT_EQ(params.tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
  EXPECT_EQ(params.primaries_named, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
  ASSERT_TRUE(params.luminances);
  EXPECT_EQ(params.luminances->max, 1500u);
  EXPECT_EQ(params.luminances->reference,
            gfx::ColorSpace::kDefaultSDRWhiteLevel);
  EXPECT_EQ(params.mastering_display_primaries,
            ToWaylandPrimaries(mastering_primaries));
  ASSERT_TRUE(params.mastering_luminance);
  EXPECT_EQ(params.mastering_luminance->min, 50u);
  EXPECT_EQ(params.mastering_luminance->max, 4000u);
  EXPECT_EQ(params.max_cll, 1500u);
  EXPECT_EQ(params.max_fall, 400u);
}

// An HDR overlay without metadata assumes the default HDR10 peak of 1000 nits,
// matching what viz assumes when tone mapping such content.
TEST_F(WaylandWpColorManagerTest, OverlayWithoutMetadata) {
  auto params = CreateImageDescription(gfx::ColorSpace::CreateHDR10(),
                                       gfx::HDRMetadata());
  ASSERT_TRUE(params.luminances);
  EXPECT_EQ(params.luminances->max, 1000u);
  EXPECT_EQ(params.luminances->reference,
            gfx::ColorSpace::kDefaultSDRWhiteLevel);
  EXPECT_FALSE(params.mastering_display_primaries);
  EXPECT_FALSE(params.mastering_luminance);
  EXPECT_EQ(params.max_cll, 1000u);
  EXPECT_EQ(params.max_fall, gfx::ColorSpace::kDefaultSDRWhiteLevel);
}

// A max_cll below the reference luminance is raised to the reference
// luminance, and max_fall never exceeds max_cll.
TEST_F(WaylandWpColorManagerTest, ClliBelowReferenceLuminancePQ) {
  constexpr float kSdrWhite = 300.f;
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(kSdrWhite);
  const gfx::HDRMetadata hdr_metadata(
      skhdr::ContentLightLevelInformation{.fMaxCLL = 150.f, .fMaxFALL = 100.f});

  auto params = CreateImageDescription(color_space, hdr_metadata);
  ASSERT_TRUE(params.luminances);
  EXPECT_EQ(params.luminances->max, kSdrWhite);
  EXPECT_EQ(params.luminances->reference, kSdrWhite);
  EXPECT_EQ(params.max_cll, kSdrWhite);
  EXPECT_EQ(params.max_fall, 100u);
}

// For non-PQ transfer functions, the primary color volume's max luminance must
// not be below the reference luminance. Some compositors (e.g. mutter) raise a
// protocol error if it is.
TEST_F(WaylandWpColorManagerTest, ClliBelowReferenceLuminanceHLG) {
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateHLG();
  const gfx::HDRMetadata hdr_metadata(
      skhdr::ContentLightLevelInformation{.fMaxCLL = 100.f, .fMaxFALL = 400.f});

  auto params = CreateImageDescription(color_space, hdr_metadata);
  EXPECT_EQ(params.tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG);
  ASSERT_TRUE(params.luminances);
  EXPECT_GT(params.luminances->reference, 100u);
  EXPECT_GE(params.luminances->max, params.luminances->reference);
  ASSERT_TRUE(params.max_cll);
  ASSERT_TRUE(params.max_fall);
  EXPECT_EQ(*params.max_cll, params.luminances->reference);
  EXPECT_LE(*params.max_fall, *params.max_cll);
}

// max_cll and max_fall are clamped to the mastering peak. Some compositors
// (e.g. Weston) raise a protocol error otherwise.
TEST_F(WaylandWpColorManagerTest, ClliAboveMasteringMax) {
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = SkNamedPrimaries::kRec2020,
          .fMaximumDisplayMasteringLuminance = 1000.f,
          .fMinimumDisplayMasteringLuminance = 0.f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = 4000.f,
                                          .fMaxFALL = 1200.f});

  auto params =
      CreateImageDescription(gfx::ColorSpace::CreateHDR10(), hdr_metadata);
  ASSERT_TRUE(params.mastering_luminance);
  EXPECT_EQ(params.mastering_luminance->max, 1000u);
  EXPECT_EQ(params.max_cll, 1000u);
  EXPECT_EQ(params.max_fall, 1000u);
  ASSERT_TRUE(params.luminances);
  EXPECT_EQ(params.luminances->max, 1000u);
}

// A mastering peak below the reference luminance is raised to the reference
// luminance, so that max_cll can be both >= the reference luminance and
// <= the mastering peak, and the max luminance stays >= the reference
// luminance.
TEST_F(WaylandWpColorManagerTest, MasteringMaxBelowReferenceLuminance) {
  constexpr uint32_t kSdrWhite = 300;
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(kSdrWhite);
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = SkNamedPrimaries::kRec2020,
          .fMaximumDisplayMasteringLuminance = 150.f,
          .fMinimumDisplayMasteringLuminance = 0.f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = 120.f, .fMaxFALL = 100.f});

  auto params = CreateImageDescription(color_space, hdr_metadata);
  ASSERT_TRUE(params.mastering_luminance);
  EXPECT_EQ(params.mastering_luminance->max, kSdrWhite);
  EXPECT_EQ(params.max_cll, kSdrWhite);
  EXPECT_EQ(params.max_fall, 100u);
  ASSERT_TRUE(params.luminances);
  EXPECT_EQ(params.luminances->max, kSdrWhite);
  EXPECT_EQ(params.luminances->reference, kSdrWhite);
}

class WaylandWpColorManagerNoMasteringTest : public WaylandWpColorManagerTest {
 public:
  WaylandWpColorManagerNoMasteringTest() {
    // Like mutter, don't advertise mastering display metadata support.
    server_.wp_color_manager()->set_supported_features(
        {WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC,
         WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES,
         WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES});
  }
};

// Compositors other than KWin that don't accept mastering display metadata
// (e.g. mutter) are not expected to tone map using client HDR metadata.
TEST_F(WaylandWpColorManagerNoMasteringTest, NoHdrMetadataPassthrough) {
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// Mastering metadata must not be sent if the compositor doesn't support it.
TEST_F(WaylandWpColorManagerNoMasteringTest, NoMasteringRequests) {
  const gfx::HDRMetadata hdr_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = SkNamedPrimaries::kRec2020,
          .fMaximumDisplayMasteringLuminance = 1000.f,
          .fMinimumDisplayMasteringLuminance = 0.f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = 800.f, .fMaxFALL = 200.f});

  auto params =
      CreateImageDescription(gfx::ColorSpace::CreateHDR10(), hdr_metadata);
  EXPECT_FALSE(params.mastering_display_primaries);
  EXPECT_FALSE(params.mastering_luminance);
  EXPECT_EQ(params.max_cll, 800u);
  EXPECT_EQ(params.max_fall, 200u);
}

// Compositors other than KWin that accept mastering display metadata (e.g.
// sway, Jay, Weston) clip HDR content instead of tone mapping it.
TEST_F(WaylandWpColorManagerTest, NonKWinDoesNotToneMapUsingHdrMetadata) {
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// KWin, identified by its org_kde_kwin_appmenu_manager global, tone maps using
// client HDR metadata.
class WaylandWpColorManagerKWinTest : public WaylandTestSimple {
 public:
  WaylandWpColorManagerKWinTest()
      : WaylandTestSimple({.supports_color_management = true,
                           .supports_kde_appmenu = true}) {}
};

TEST_F(WaylandWpColorManagerKWinTest, ToneMapsUsingHdrMetadata) {
  ASSERT_TRUE(connection_->org_kde_kwin_appmenu_manager());
  EXPECT_TRUE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_TRUE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// KWin without mastering display metadata support can only use max_cll.
class WaylandWpColorManagerKWinNoMasteringTest
    : public WaylandWpColorManagerKWinTest {
 public:
  WaylandWpColorManagerKWinNoMasteringTest() {
    server_.wp_color_manager()->set_supported_features(
        {WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC,
         WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES,
         WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES});
  }
};

TEST_F(WaylandWpColorManagerKWinNoMasteringTest, ToneMapsUsingClliOnly) {
  EXPECT_TRUE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// KWin without set_luminances can't relate the content's luminance to its SDR
// white, so it doesn't qualify.
class WaylandWpColorManagerKWinNoLuminancesTest
    : public WaylandWpColorManagerKWinTest {
 public:
  WaylandWpColorManagerKWinNoLuminancesTest() {
    server_.wp_color_manager()->set_supported_features(
        {WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC,
         WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES,
         WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES});
  }
};

TEST_F(WaylandWpColorManagerKWinNoLuminancesTest,
       NoToneMappingUsingHdrMetadata) {
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

using WaylandNoColorManagerTest = WaylandTestSimple;

// Without color management, HDR metadata can't be passed through.
TEST_F(WaylandNoColorManagerTest, NoHdrMetadataPassthrough) {
  ASSERT_FALSE(connection_->wp_color_manager());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// KWin without color management can't pass HDR metadata through either.
class WaylandKWinNoColorManagerTest : public WaylandTestSimple {
 public:
  WaylandKWinNoColorManagerTest()
      : WaylandTestSimple({.supports_kde_appmenu = true}) {}
};

TEST_F(WaylandKWinNoColorManagerTest, NoHdrMetadataPassthrough) {
  ASSERT_TRUE(connection_->org_kde_kwin_appmenu_manager());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingClli());
  EXPECT_FALSE(
      connection_->buffer_manager_host()->CompositorToneMapsUsingMdcv());
}

// SDR color spaces don't get any luminance or HDR metadata, and keep the sRGB
// transfer function when the display itself is described with sRGB (e.g.
// Hyprland's default `cm_sdr_eotf` or Mutter 48-49).
TEST_F(WaylandWpColorManagerTest, SdrColorSpace) {
  auto params =
      CreateImageDescription(gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata());
  EXPECT_EQ(params.tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
  EXPECT_EQ(params.primaries_named, WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
  EXPECT_FALSE(params.luminances);
  EXPECT_FALSE(params.mastering_luminance);
  EXPECT_FALSE(params.max_cll);
  EXPECT_FALSE(params.max_fall);

  auto surface_params = SetRootSurfaceImageDescription(
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata());
  ASSERT_TRUE(surface_params);
  EXPECT_EQ(surface_params->tf_named,
            WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
  EXPECT_EQ(surface_params->primaries_named,
            WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
}

// When a surface moves between an sRGB display and a gamma 2.2 display, its
// sRGB image description updates between sRGB and gamma 2.2 accordingly.
TEST_F(WaylandWpColorManagerTest, PreferredChangedUpdatesSrgbSurfaceTransfer) {
  WaylandSurface* surface = window_->root_surface();
  const uint32_t surface_id = surface->get_surface_id();

  auto params = SetRootSurfaceImageDescription(gfx::ColorSpace::CreateSRGB(),
                                               gfx::HDRMetadata());
  ASSERT_TRUE(params);
  EXPECT_EQ(params->tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->wp_color_manager()->set_display_tf_named(
        WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
    server->wp_color_manager()->SendPreferredChanged();
  });
  // Roundtrips for get_preferred -> get_information -> create -> set.
  for (int i = 0; i < 4; ++i) {
    SyncDisplay();
  }
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(mock_surface);
    params = server->wp_color_manager()->GetSurfaceImageDescription(
        mock_surface->resource());
  });
  ASSERT_TRUE(params);
  EXPECT_EQ(params->tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->wp_color_manager()->set_display_tf_named(
        WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
    server->wp_color_manager()->SendPreferredChanged();
  });
  for (int i = 0; i < 4; ++i) {
    SyncDisplay();
  }
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(mock_surface);
    params = server->wp_color_manager()->GetSurfaceImageDescription(
        mock_surface->resource());
  });
  ASSERT_TRUE(params);
  EXPECT_EQ(params->tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
}

namespace {

// Returns the primary plane's PQ color space and metadata for SDR content at
// `sdr_white` nits, as viz describes it on HDR displays.
std::pair<gfx::ColorSpace, gfx::HDRMetadata> PrimaryPlanePQ(float sdr_white) {
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(sdr_white);
  return {color_space, gfx::HDRMetadata(
                           skhdr::MasteringDisplayColorVolume{
                               .fDisplayPrimaries = color_space.GetPrimaries(),
                               .fMaximumDisplayMasteringLuminance = sdr_white,
                               .fMinimumDisplayMasteringLuminance = 0.f,
                           },
                           skhdr::ContentLightLevelInformation{
                               .fMaxCLL = sdr_white, .fMaxFALL = sdr_white})};
}

}  // namespace

// When the SDR white level changes (e.g. the user changes the brightness of an
// HDR display) and changes back before the new image description is ready,
// the stale description must not replace the current one once it becomes
// ready. Otherwise the buffers are described with the wrong reference
// luminance and look dimmer or brighter (https://crbug.com/471769943).
TEST_F(WaylandWpColorManagerTest, StaleImageDescriptionDoesNotOverrideLatest) {
  const auto [cs_203, md_203] = PrimaryPlanePQ(203.f);
  const auto [cs_150, md_150] = PrimaryPlanePQ(150.f);

  auto params = SetRootSurfaceImageDescription(cs_203, md_203);
  ASSERT_TRUE(params);
  ASSERT_TRUE(params->luminances);
  EXPECT_EQ(params->luminances->reference, 203u);

  WaylandSurface* surface = window_->root_surface();
  // The description for 150 nits must be created asynchronously.
  surface->SetImageDescription(cs_150, md_150);
  surface->ApplyPendingState();
  surface->Commit();
  // The description for 203 nits is cached and is set immediately.
  surface->SetImageDescription(cs_203, md_203);
  surface->ApplyPendingState();
  surface->Commit();
  SyncDisplay();
  SyncDisplay();

  const uint32_t surface_id = surface->get_surface_id();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    ASSERT_TRUE(mock_surface);
    params = server->wp_color_manager()->GetSurfaceImageDescription(
        mock_surface->resource());
  });
  ASSERT_TRUE(params);
  ASSERT_TRUE(params->luminances);
  EXPECT_EQ(params->luminances->reference, 203u);
}

class WaylandWpColorManagerNoSrgbTest : public WaylandWpColorManagerTest {
 public:
  WaylandWpColorManagerNoSrgbTest() {
    // Like KWin 6.6+, don't advertise the deprecated sRGB transfer function.
    server_.wp_color_manager()->set_supported_tfs_named(
        {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22,
         WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ});
  }
};

// Compositors that don't support the sRGB transfer function display sRGB as
// gamma 2.2, so sRGB content must be described that way instead of failing.
TEST_F(WaylandWpColorManagerNoSrgbTest, SrgbDescribedAsGamma22) {
  auto params =
      CreateImageDescription(gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata());
  EXPECT_EQ(params.tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
  EXPECT_EQ(params.primaries_named, WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
}

class WaylandWpColorManagerGamma22DisplayTest
    : public WaylandWpColorManagerTest {
 public:
  WaylandWpColorManagerGamma22DisplayTest() {
    // Like KWin, describe SDR displays with a gamma 2.2 transfer function.
    server_.wp_color_manager()->set_display_tf_named(
        WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
  }
};

// sRGB content must be passed through to SDR gamma 2.2 displays, rather than
// being decoded with the piece-wise sRGB curve and re-encoded as gamma 2.2,
// which lifts dark tones (https://crbug.com/475549558). Even when the
// compositor advertises the sRGB transfer function (e.g. Hyprland v1 or Mutter
// 48-49, which decode it as piece-wise sRGB), surfaces whose preferred SDR
// description was gamma 2.2 must describe sRGB buffers as gamma 2.2.
TEST_F(WaylandWpColorManagerGamma22DisplayTest, SrgbPassedThrough) {
  connection_->wp_color_manager()->OnHdrEnabledChanged(true);
  SyncDisplay();
  WaylandOutput* output =
      connection_->wayland_output_manager()->GetPrimaryOutput();
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->wp_color_management_output());
  const gfx::DisplayColorSpaces* color_spaces =
      output->wp_color_management_output()->GetDisplayColorSpaces();
  ASSERT_TRUE(color_spaces);
  EXPECT_EQ(color_spaces->GetOutputColorSpace(gfx::ContentColorUsage::kSRGB,
                                              /*needs_alpha=*/false),
            gfx::ColorSpace::CreateSRGB());

  auto srgb_params = SetRootSurfaceImageDescription(
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata());
  ASSERT_TRUE(srgb_params);
  EXPECT_EQ(srgb_params->tf_named,
            WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
  EXPECT_EQ(srgb_params->primaries_named, WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);

  auto p3_params = SetRootSurfaceImageDescription(
      gfx::ColorSpace::CreateDisplayP3D65(), gfx::HDRMetadata());
  ASSERT_TRUE(p3_params);
  EXPECT_EQ(p3_params->tf_named, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22);
  EXPECT_EQ(p3_params->primaries_named,
            WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3);
}

}  // namespace ui
