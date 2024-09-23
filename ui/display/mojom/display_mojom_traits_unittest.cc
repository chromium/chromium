// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/mojom/display.mojom.h"
#include "ui/display/mojom/display_color_management.mojom.h"
#include "ui/display/mojom/display_color_management_mojom_traits.h"
#include "ui/display/mojom/display_layout_mojom_traits.h"
#include "ui/display/mojom/display_mode_mojom_traits.h"
#include "ui/display/mojom/display_mojom_traits.h"
#include "ui/display/mojom/display_snapshot.mojom.h"
#include "ui/display/mojom/display_snapshot_mojom_traits.h"
#include "ui/display/mojom/gamma_ramp_rgb_entry.mojom.h"
#include "ui/display/mojom/gamma_ramp_rgb_entry_mojom_traits.h"
#include "ui/display/types/display_color_management.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"

namespace display {
namespace {

constexpr int64_t kDisplayId1 = 123;
constexpr int64_t kDisplayId2 = 456;
constexpr int64_t kDisplayId3 = 789;

#define DRM_FORMAT_ARGB8888 0x34325241
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)

void CheckDisplaysEqual(const Display& input, const Display& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.bounds(), output.bounds());
  EXPECT_EQ(input.GetSizeInPixel(), output.GetSizeInPixel());
  EXPECT_EQ(input.work_area(), output.work_area());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
  EXPECT_EQ(input.rotation(), output.rotation());
  EXPECT_EQ(input.touch_support(), output.touch_support());
  EXPECT_EQ(input.accelerometer_support(), output.accelerometer_support());
  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
  EXPECT_EQ(input.color_depth(), output.color_depth());
  EXPECT_EQ(input.depth_per_component(), output.depth_per_component());
  EXPECT_EQ(input.is_monochrome(), output.is_monochrome());
  EXPECT_EQ(input.display_frequency(), output.display_frequency());
  EXPECT_EQ(input.label(), output.label());
}

void CheckDisplayLayoutsEqual(const DisplayLayout& input,
                              const DisplayLayout& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.placement_list, output.placement_list);
  EXPECT_EQ(input.default_unified, output.default_unified);
  EXPECT_EQ(input.primary_id, output.primary_id);
}

void CheckDisplayModesEqual(const DisplayMode* input,
                            const DisplayMode* output) {
  // DisplaySnapshot can have null DisplayModes, so if |input| is null then
  // |output| should be null too.
  if (input == nullptr && output == nullptr)
    return;

  EXPECT_NE(input, output);  // Make sure they aren't the same object.
  EXPECT_EQ(*input, *output);
}

void CheckDisplaySnapShotMojoEqual(const DisplaySnapshot& input,
                                   const DisplaySnapshot& output) {
  // We want to test each component individually to make sure each data member
  // was correctly serialized and deserialized.
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.display_id(), output.display_id());
  EXPECT_EQ(input.port_display_id(), output.port_display_id());
  EXPECT_EQ(input.edid_display_id(), output.edid_display_id());
  EXPECT_EQ(input.origin(), output.origin());
  EXPECT_EQ(input.physical_size(), output.physical_size());
  EXPECT_EQ(input.type(), output.type());
  EXPECT_EQ(input.base_connector_id(), output.base_connector_id());
  EXPECT_EQ(input.path_topology(), output.path_topology());
  EXPECT_EQ(input.is_aspect_preserving_scaling(),
            output.is_aspect_preserving_scaling());
  EXPECT_EQ(input.has_overscan(), output.has_overscan());
  EXPECT_EQ(input.privacy_screen_state(), output.privacy_screen_state());
  EXPECT_EQ(input.has_color_correction_matrix(),
            output.has_color_correction_matrix());
  EXPECT_EQ(input.display_name(), output.display_name());
  EXPECT_EQ(input.sys_path(), output.sys_path());
  EXPECT_EQ(input.product_code(), output.product_code());
  EXPECT_EQ(input.modes().size(), output.modes().size());

  for (size_t i = 0; i < input.modes().size(); i++)
    CheckDisplayModesEqual(input.modes()[i].get(), output.modes()[i].get());

  EXPECT_EQ(input.panel_orientation(), output.panel_orientation());
  EXPECT_EQ(input.edid(), output.edid());

  CheckDisplayModesEqual(input.current_mode(), output.current_mode());
  CheckDisplayModesEqual(input.native_mode(), output.native_mode());

  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
  EXPECT_EQ(input.color_space(), output.color_space());
  EXPECT_EQ(input.color_info().color_space, output.color_info().color_space);
  EXPECT_EQ(input.color_info().edid_primaries,
            output.color_info().edid_primaries);
  EXPECT_EQ(input.color_info().edid_gamma, output.color_info().edid_gamma);
  EXPECT_EQ(input.color_info().hdr_static_metadata,
            output.color_info().hdr_static_metadata);
  EXPECT_EQ(input.color_info().supports_color_temperature_adjustment,
            output.color_info().supports_color_temperature_adjustment);
  EXPECT_EQ(input.color_info().bits_per_channel,
            output.color_info().bits_per_channel);

  EXPECT_EQ(input.bits_per_channel(), output.bits_per_channel());
  EXPECT_EQ(input.hdr_static_metadata(), output.hdr_static_metadata());
  EXPECT_EQ(input.variable_refresh_rate_state(),
            output.variable_refresh_rate_state());
}

// Test StructTrait serialization and deserialization for copyable type. |input|
// will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(const Type& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

// Test StructTrait serialization and deserialization for move only type.
// |input| will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(Type&& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

}  // namespace

TEST(DisplayStructTraitsTest, DefaultDisplayValues) {
  Display input(5);

  Display output;
  SerializeAndDeserialize<mojom::Display>(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST(DisplayStructTraitsTest, SetAllDisplayValues) {
  const gfx::Rect bounds(100, 200, 500, 600);
  const gfx::Rect work_area(150, 250, 400, 500);
  const gfx::Size maximum_cursor_size(64, 64);

  Display input(246345234, bounds);
  input.set_work_area(work_area);
  input.set_device_scale_factor(2.0f);
  input.set_rotation(Display::ROTATE_270);
  input.set_touch_support(Display::TouchSupport::AVAILABLE);
  input.set_accelerometer_support(Display::AccelerometerSupport::UNAVAILABLE);
  input.set_maximum_cursor_size(maximum_cursor_size);
  input.set_color_depth(input.color_depth() + 1);
  input.set_depth_per_component(input.depth_per_component() + 1);
  input.set_is_monochrome(!input.is_monochrome());
  input.set_display_frequency(input.display_frequency() + 1);
  input.set_label("Internal Display");

  Display output;
  SerializeAndDeserialize<mojom::Display>(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST(DisplayStructTraitsTest, DefaultDisplayMode) {
  DisplayMode input({1024, 768}, true, 61.0);

  std::unique_ptr<DisplayMode> output;
  SerializeAndDeserialize<mojom::DisplayMode>(input.Clone(), &output);

  CheckDisplayModesEqual(&input, output.get());
}

TEST(DisplayStructTraitsTest, DisplayPlacementFlushAtTop) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::TOP;
  input.offset = 0;
  input.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement output;
  SerializeAndDeserialize<mojom::DisplayPlacement>(input, &output);

  EXPECT_EQ(input, output);
}

TEST(DisplayStructTraitsTest, DisplayPlacementWithOffset) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::BOTTOM;
  input.offset = -100;
  input.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  DisplayPlacement output;
  SerializeAndDeserialize<mojom::DisplayPlacement>(input, &output);

  EXPECT_EQ(input, output);
}

TEST(DisplayStructTraitsTest, DisplayLayoutTwoExtended) {
  DisplayPlacement placement;
  placement.display_id = kDisplayId1;
  placement.parent_display_id = kDisplayId2;
  placement.position = DisplayPlacement::RIGHT;
  placement.offset = 0;
  placement.offset_reference = DisplayPlacement::TOP_LEFT;

  auto input = std::make_unique<DisplayLayout>();
  input->placement_list.push_back(placement);
  input->primary_id = kDisplayId2;
  input->default_unified = true;

  std::unique_ptr<DisplayLayout> output;
  SerializeAndDeserialize<mojom::DisplayLayout>(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST(DisplayStructTraitsTest, DisplayLayoutThreeExtended) {
  DisplayPlacement placement1;
  placement1.display_id = kDisplayId2;
  placement1.parent_display_id = kDisplayId1;
  placement1.position = DisplayPlacement::LEFT;
  placement1.offset = 0;
  placement1.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement placement2;
  placement2.display_id = kDisplayId3;
  placement2.parent_display_id = kDisplayId1;
  placement2.position = DisplayPlacement::RIGHT;
  placement2.offset = -100;
  placement2.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  auto input = std::make_unique<DisplayLayout>();
  input->placement_list.push_back(placement1);
  input->placement_list.push_back(placement2);
  input->primary_id = kDisplayId1;
  input->default_unified = false;

  std::unique_ptr<DisplayLayout> output;
  SerializeAndDeserialize<mojom::DisplayLayout>(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST(DisplayStructTraitsTest, BasicGammaRampRGBEntry) {
  GammaRampRGBEntry input{259, 81, 16};
  GammaRampRGBEntry output;
  SerializeAndDeserialize<mojom::GammaRampRGBEntry>(input, &output);

  EXPECT_EQ(input.r, output.r);
  EXPECT_EQ(input.g, output.g);
  EXPECT_EQ(input.b, output.b);

  GammaCurve curve_input({input});
  GammaCurve curve_output;
  SerializeAndDeserialize<mojom::GammaCurve>(curve_input, &curve_output);

  curve_input.Evaluate(0.5f, input.r, input.g, input.b);
  curve_output.Evaluate(0.5f, output.r, output.g, output.b);

  EXPECT_EQ(input.r, output.r);
  EXPECT_EQ(input.g, output.g);
  EXPECT_EQ(input.b, output.b);
}

TEST(DisplayStructTraitsTest, ColorCalibrationRoundtrip) {
  uint16_t in_r, in_g, in_b;
  uint16_t out_r, out_g, out_b;

  ColorCalibration input;
  input.srgb_to_linear = GammaCurve({{0, 0, 0}, {10, 20, 30}});
  input.linear_to_device = GammaCurve({{5, 5, 5}, {10, 20, 30}});
  input.srgb_to_device_matrix = SkNamedGamut::kDisplayP3;

  ColorCalibration output;
  SerializeAndDeserialize<mojom::ColorCalibration>(input, &output);

  // Validate `srgb_to_device_matrix`.
  EXPECT_EQ(0, memcmp(&input.srgb_to_device_matrix,
                      &output.srgb_to_device_matrix, sizeof(skcms_Matrix3x3)));

  // Validate `srgb_to_linear`.
  input.srgb_to_linear.Evaluate(0.5f, in_r, in_g, in_b);
  output.srgb_to_linear.Evaluate(0.5f, out_r, out_g, out_b);
  EXPECT_EQ(in_r, out_r);
  EXPECT_EQ(in_g, out_g);
  EXPECT_EQ(in_b, out_b);

  // Validate `linear_to_device`.
  input.linear_to_device.Evaluate(0.5f, in_r, in_g, in_b);
  output.linear_to_device.Evaluate(0.5f, out_r, out_g, out_b);
  EXPECT_EQ(in_r, out_r);
  EXPECT_EQ(in_g, out_g);
  EXPECT_EQ(in_b, out_b);
}

TEST(DisplayStructTraitsTest, ColorTemperatureAdjustmentRoundtrip) {
  ColorTemperatureAdjustment input;
  input.srgb_matrix = SkNamedGamut::kDisplayP3;

  ColorTemperatureAdjustment output;
  SerializeAndDeserialize<mojom::ColorTemperatureAdjustment>(input, &output);

  EXPECT_EQ(0, memcmp(&input.srgb_matrix, &output.srgb_matrix,
                      sizeof(skcms_Matrix3x3)));
}

TEST(DisplayStructTraitsTest, GammaAdjustmentRoundtrip) {
  uint16_t in_r, in_g, in_b;
  uint16_t out_r, out_g, out_b;

  GammaAdjustment input;
  input.curve = GammaCurve({{0, 10, 20}, {10, 20, 30}});
  GammaAdjustment output;

  SerializeAndDeserialize<mojom::GammaAdjustment>(input, &output);

  input.curve.Evaluate(0.5f, in_r, in_g, in_b);
  output.curve.Evaluate(0.5f, out_r, out_g, out_b);
  EXPECT_EQ(in_r, out_r);
  EXPECT_EQ(in_g, out_g);
  EXPECT_EQ(in_b, out_b);
}

// One display mode, current and native mode nullptr.
TEST(DisplayStructTraitsTest, DisplaySnapshotCurrentAndNativeModesNull) {
  // Prepare sample input with random values.
  const int64_t port_display_id = 7;
  const int64_t edid_display_id = 19;
  const uint16_t connector_index = 0x0001;
  const gfx::Point origin(1, 2);
  const gfx::Size physical_size(5, 9);
  const gfx::Size maximum_cursor_size(3, 5);
  const DisplayConnectionType type = DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
  const uint64_t base_connector_id = 1u;
  const std::vector<uint64_t> path_topology{};
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = true;
  const PrivacyScreenState privacy_screen_state = kEnabled;
  const bool has_content_protection_key = false;
  display::DisplaySnapshot::ColorInfo color_info;
  color_info.supports_color_temperature_adjustment = true;
  color_info.color_space = gfx::ColorSpace::CreateREC709();
  color_info.edid_primaries = SkNamedPrimariesExt::kP3;
  color_info.edid_gamma = 1.8;
  color_info.bits_per_channel = 8;
  color_info.hdr_static_metadata.emplace(
      100.0, 80.0, 0.0,
      gfx::HDRStaticMetadata::EotfMask(
          {gfx::HDRStaticMetadata::Eotf::kGammaSdrRange}));
  const std::string display_name("whatever display_name");
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("a/cb");
  const int64_t product_code = 19;
  const int32_t year_of_manufacture = 1776;
  const VariableRefreshRateState variable_refresh_rate_state =
      VariableRefreshRateState::kVrrEnabled;

  const DisplayMode display_mode({13, 11}, true, 40.0f);

  DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = nullptr;
  const DisplayMode* native_mode = nullptr;
  const std::vector<uint8_t> edid = {1};

  DrmFormatsAndModifiers drm_formats_and_modifiers;
  drm_formats_and_modifiers.emplace(
      DRM_FORMAT_ARGB8888, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));

  std::unique_ptr<DisplaySnapshot> input = std::make_unique<DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      origin, physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, color_info, display_name, sys_path,
      std::move(modes), PanelOrientation::kNormal, edid, current_mode,
      native_mode, product_code, year_of_manufacture, maximum_cursor_size,
      variable_refresh_rate_state, std::move(drm_formats_and_modifiers));

  std::unique_ptr<DisplaySnapshot> output;
  SerializeAndDeserialize<mojom::DisplaySnapshot>(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

// One display mode that is the native mode and no current mode.
TEST(DisplayStructTraitsTest, DisplaySnapshotCurrentModeNull) {
  // Prepare sample input with random values.
  const int64_t port_display_id = 6;
  const int64_t edid_display_id = 17;
  const uint16_t connector_index = 0x0101;
  const gfx::Point origin(11, 32);
  const gfx::Size physical_size(55, 49);
  const gfx::Size maximum_cursor_size(13, 95);
  const DisplayConnectionType type = DISPLAY_CONNECTION_TYPE_VGA;
  const uint64_t base_connector_id = 1u;
  const std::vector<uint64_t> path_topology{};
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = true;
  const PrivacyScreenState privacy_screen_state = kEnabled;
  const bool has_content_protection_key = false;
  DisplaySnapshot::ColorInfo color_info;
  color_info.supports_color_temperature_adjustment = true;
  color_info.color_space = gfx::ColorSpace::CreateDisplayP3D65();
  color_info.bits_per_channel = 8u;
  color_info.hdr_static_metadata.emplace(
      100.0, 80.0, 0.0,
      gfx::HDRStaticMetadata::EotfMask(
          {gfx::HDRStaticMetadata::Eotf::kGammaSdrRange}));
  const std::string display_name("whatever display_name");
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("z/b");
  const int64_t product_code = 9;
  const int32_t year_of_manufacture = 1776;
  const VariableRefreshRateState variable_refresh_rate_state =
      VariableRefreshRateState::kVrrEnabled;

  const DisplayMode display_mode({13, 11}, true, 50.0f);

  DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = nullptr;
  const DisplayMode* native_mode = modes[0].get();
  const std::vector<uint8_t> edid = {1};

  DrmFormatsAndModifiers drm_formats_and_modifiers;
  drm_formats_and_modifiers.emplace(
      DRM_FORMAT_ARGB8888, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));

  std::unique_ptr<DisplaySnapshot> input = std::make_unique<DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      origin, physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, color_info, display_name, sys_path,
      std::move(modes), PanelOrientation::kNormal, edid, current_mode,
      native_mode, product_code, year_of_manufacture, maximum_cursor_size,
      variable_refresh_rate_state, std::move(drm_formats_and_modifiers));

  std::unique_ptr<DisplaySnapshot> output;
  SerializeAndDeserialize<mojom::DisplaySnapshot>(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

// Multiple display modes, both native and current mode set.
TEST(DisplayStructTraitsTest, DisplaySnapshotExternal) {
  // Prepare sample input from external display.
  const int64_t port_display_id = 9834293210466051;
  const int64_t edid_display_id = 1428;
  const uint16_t connector_index = 0x0002;
  const gfx::Point origin(0, 1760);
  const gfx::Size physical_size(520, 320);
  const gfx::Size maximum_cursor_size(4, 5);
  const DisplayConnectionType type = DISPLAY_CONNECTION_TYPE_HDMI;
  const uint64_t base_connector_id = 1u;
  const std::vector<uint64_t> path_topology{};
  const bool is_aspect_preserving_scaling = false;
  const bool has_overscan = false;
  const PrivacyScreenState privacy_screen_state = kDisabled;
  const bool has_content_protection_key = true;
  DisplaySnapshot::ColorInfo color_info;
  color_info.supports_color_temperature_adjustment = false;
  const std::string display_name("HP Z24i");
  color_info.color_space = gfx::ColorSpace::CreateSRGB();
  color_info.bits_per_channel = 8u;
  color_info.hdr_static_metadata.emplace(
      100.0, 80.0, 0.0,
      gfx::HDRStaticMetadata::EotfMask(
          {gfx::HDRStaticMetadata::Eotf::kGammaSdrRange}));
  const base::FilePath sys_path = base::FilePath::FromUTF8Unsafe("a/cb");
  const int64_t product_code = 139;
  const int32_t year_of_manufacture = 2018;
  const VariableRefreshRateState variable_refresh_rate_state =
      VariableRefreshRateState::kVrrDisabled;

  const DisplayMode display_mode({1024, 768}, false, 60.0f);
  const DisplayMode display_current_mode({1440, 900}, false, 59.89f);
  const DisplayMode display_native_mode({1920, 1200}, false, 59.89f);

  DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());
  modes.push_back(display_current_mode.Clone());
  modes.push_back(display_native_mode.Clone());

  const DisplayMode* current_mode = modes[1].get();
  const DisplayMode* native_mode = modes[2].get();
  const std::vector<uint8_t> edid = {2, 3, 4, 5};

  DrmFormatsAndModifiers drm_formats_and_modifiers;
  drm_formats_and_modifiers.emplace(
      DRM_FORMAT_ARGB8888, std::vector<uint64_t>({DRM_FORMAT_MOD_INVALID}));

  std::unique_ptr<DisplaySnapshot> input = std::make_unique<DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      origin, physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, color_info, display_name, sys_path,
      std::move(modes), PanelOrientation::kLeftUp, edid, current_mode,
      native_mode, product_code, year_of_manufacture, maximum_cursor_size,
      variable_refresh_rate_state, std::move(drm_formats_and_modifiers));

  std::unique_ptr<DisplaySnapshot> output;
  SerializeAndDeserialize<mojom::DisplaySnapshot>(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

TEST(DisplayStructTraitsTest, DisplaySnapshotInternal) {
  // Prepare sample input from Pixel's internal display.
  const int64_t port_display_id = 13761487533244416;
  const int64_t edid_display_id = 39927;
  const uint16_t connector_index = 0x0103;
  const gfx::Point origin(0, 0);
  const gfx::Size physical_size(270, 180);
  const gfx::Size maximum_cursor_size(64, 64);
  const DisplayConnectionType type = DISPLAY_CONNECTION_TYPE_INTERNAL;
  const uint64_t base_connector_id = 1u;
  const std::vector<uint64_t> path_topology{};
  const bool is_aspect_preserving_scaling = true;
  const bool has_overscan = false;
  const PrivacyScreenState privacy_screen_state = kNotSupported;
  const bool has_content_protection_key = false;
  DisplaySnapshot::ColorInfo color_info;
  color_info.supports_color_temperature_adjustment = false;
  color_info.color_space = gfx::ColorSpace::CreateDisplayP3D65();
  color_info.bits_per_channel = 9u;
  color_info.hdr_static_metadata.emplace(
      200.0, 100.0, 0.0,
      gfx::HDRStaticMetadata::EotfMask({
          gfx::HDRStaticMetadata::Eotf::kGammaSdrRange,
          gfx::HDRStaticMetadata::Eotf::kPq,
      }));
  const std::string display_name("");
  const base::FilePath sys_path;
  const int64_t product_code = 139;
  const int32_t year_of_manufacture = 2018;
  const VariableRefreshRateState variable_refresh_rate_state =
      VariableRefreshRateState::kVrrNotCapable;

  const DisplayMode display_mode({2560, 1700}, false, 95.96f);

  DisplaySnapshot::DisplayModeList modes;
  modes.push_back(display_mode.Clone());

  const DisplayMode* current_mode = modes[0].get();
  const DisplayMode* native_mode = modes[0].get();
  const std::vector<uint8_t> edid = {2, 3};

  const DrmFormatsAndModifiers drm_formats_and_modifiers;

  std::unique_ptr<DisplaySnapshot> input = std::make_unique<DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id, connector_index,
      origin, physical_size, type, base_connector_id, path_topology,
      is_aspect_preserving_scaling, has_overscan, privacy_screen_state,
      has_content_protection_key, color_info, display_name, sys_path,
      std::move(modes), PanelOrientation::kRightUp, edid, current_mode,
      native_mode, product_code, year_of_manufacture, maximum_cursor_size,
      variable_refresh_rate_state, drm_formats_and_modifiers);

  std::unique_ptr<DisplaySnapshot> output;
  SerializeAndDeserialize<mojom::DisplaySnapshot>(input->Clone(), &output);

  CheckDisplaySnapShotMojoEqual(*input, *output);
}

}  // namespace display
