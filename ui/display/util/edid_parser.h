// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_EDID_PARSER_H_
#define UI_DISPLAY_UTIL_EDID_PARSER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/util/display_util_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// This class parses a EDID (Extended Display Identification Data) binary blob
// passed on constructor, and provides access to the parsed information, plus
// a few utility postprocessings.
class DISPLAY_UTIL_EXPORT EdidParser {
 public:
  // Reflects CEA 861.G-2018, Sec.7.5.13, "HDR Static Metadata Data Block"
  // A value of 0.0 in any of this fields means that it's not indicated.
  struct Luminance {
    // "Desired Content Max Luminance Data. This is the content’s absolute peak
    // luminance (in cd/m2) (likely only in a small area of the screen) that the
    // display prefers for optimal content rendering."
    double max;
    // "Desired Content Max Frame-average Luminance. This is the content’s max
    // frame-average luminance (in cd/m2) that the display prefers for optimal
    // content rendering."
    double max_avg;
    // "Desired Content Min Luminance. This is the minimum value of the content
    // (in cd/m2) that the display prefers for optimal content rendering."
    double min;
  };

  explicit EdidParser(const std::vector<uint8_t>& edid_blob);
  ~EdidParser();

  uint16_t manufacturer_id() const { return manufacturer_id_; }
  uint16_t product_id() const { return product_id_; }
  const std::string& display_name() const { return display_name_; }
  const gfx::Size& active_pixel_size() const { return active_pixel_size_; }
  int32_t year_of_manufacture() const { return year_of_manufacture_; }
  bool has_overscan_flag() const { return overscan_flag_.has_value(); }
  bool overscan_flag() const { return overscan_flag_.value(); }
  double gamma() const { return gamma_; }
  int32_t bits_per_channel() const { return bits_per_channel_; }
  const SkColorSpacePrimaries& primaries() const { return primaries_; }
  const base::flat_set<gfx::ColorSpace::PrimaryID>&
  supported_color_primary_ids() const {
    return supported_color_primary_ids_;
  }
  const base::flat_set<gfx::ColorSpace::TransferID>&
  supported_color_transfer_ids() const {
    return supported_color_transfer_ids_;
  }
  const Luminance* luminance() const {
    return base::OptionalOrNullptr(luminance_);
  }
  // Returns a 32-bit identifier for this display |manufacturer_id_| and
  // |product_id_|.
  uint32_t GetProductCode() const;

  // Generates a unique display id out of a mix of |manufacturer_id_|, hashed
  // |display_name_| if available, and |output_index|.
  int64_t GetDisplayId(uint8_t output_index) const;

  // Splits the |product_code| (as returned by GetDisplayId()) into its
  // constituents |manufacturer_id| and |product_id|.
  static void SplitProductCodeInManufacturerIdAndProductId(
      int64_t product_code,
      uint16_t* manufacturer_id,
      uint16_t* product_id);
  // Extracts the three letter Manufacturer ID out of |manufacturer_id|.
  static std::string ManufacturerIdToString(uint16_t manufacturer_id);
  // Extracts the 2 Byte Product ID as hex out of |product_id|.
  static std::string ProductIdToString(uint16_t product_id);

 private:
  // Parses |edid_blob|, filling up as many as possible fields below.
  void ParseEdid(const std::vector<uint8_t>& edid);

  uint16_t manufacturer_id_;
  uint16_t product_id_;
  std::string display_name_;
  // Active pixel size from the first detailed timing descriptor in the EDID.
  gfx::Size active_pixel_size_;
  int32_t year_of_manufacture_;
  absl::optional<bool> overscan_flag_;
  double gamma_;
  int bits_per_channel_;
  SkColorSpacePrimaries primaries_;

  base::flat_set<gfx::ColorSpace::PrimaryID> supported_color_primary_ids_;
  base::flat_set<gfx::ColorSpace::TransferID> supported_color_transfer_ids_;
  absl::optional<Luminance> luminance_;

  DISALLOW_COPY_AND_ASSIGN(EdidParser);
};

}  // namespace display

#endif // UI_DISPLAY_UTIL_EDID_PARSER_H_
