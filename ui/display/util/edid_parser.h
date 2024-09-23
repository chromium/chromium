// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_EDID_PARSER_H_
#define UI_DISPLAY_UTIL_EDID_PARSER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_static_metadata.h"

namespace display {

// This class parses a EDID (Extended Display Identification Data) binary blob
// passed on constructor, and provides access to the parsed information, plus
// a few utility postprocessings.
class DISPLAY_UTIL_EXPORT EdidParser {
 public:
  explicit EdidParser(std::vector<uint8_t>&& edid_blob,
                      bool is_external = false);

  EdidParser(const EdidParser&) = delete;
  EdidParser& operator=(const EdidParser&) = delete;
  EdidParser(EdidParser&&);
  EdidParser& operator=(EdidParser&&);
  ~EdidParser();

  uint16_t manufacturer_id() const { return manufacturer_id_; }
  uint16_t product_id() const { return product_id_; }
  std::string block_zero_serial_number_hash() const {
    return block_zero_serial_number_hash_.value_or("");
  }
  std::string descriptor_block_serial_number_hash() const {
    return descriptor_block_serial_number_hash_.value_or("");
  }
  gfx::Size max_image_size() const {
    return max_image_size_.value_or(gfx::Size());
  }
  const std::string& display_name() const { return display_name_; }
  const gfx::Size& active_pixel_size() const { return active_pixel_size_; }
  int32_t week_of_manufacture() const {
    return week_of_manufacture_.value_or(0);
  }
  int32_t year_of_manufacture() const { return year_of_manufacture_; }
  bool has_overscan_flag() const { return overscan_flag_.has_value(); }
  bool overscan_flag() const { return overscan_flag_.value(); }
  double gamma() const { return gamma_; }
  int32_t bits_per_channel() const { return bits_per_channel_; }
  const SkColorSpacePrimaries& primaries() const { return primaries_; }
  using PrimaryMatrixPair =
      std::pair<gfx::ColorSpace::PrimaryID, gfx::ColorSpace::MatrixID>;
  const base::flat_set<PrimaryMatrixPair>& supported_color_primary_matrix_ids()
      const {
    return supported_color_primary_matrix_ids_;
  }
  const base::flat_set<gfx::ColorSpace::TransferID>&
  supported_color_transfer_ids() const {
    return supported_color_transfer_ids_;
  }
  const std::optional<gfx::HDRStaticMetadata>& hdr_static_metadata() const {
    return hdr_static_metadata_;
  }
  const std::optional<uint16_t>& vsync_rate_min() const {
    return vsync_rate_min_;
  }
  // Returns a 32-bit identifier for this display |manufacturer_id_| and
  // |product_id_|.
  uint32_t GetProductCode() const;

  // Generates a unique display id out of a mix of |manufacturer_id_|, hashed
  // |display_name_| if available, and |output_index|.
  // Here, uniqueness is heavily based on the connector's index to which the
  // display is attached to.
  int64_t GetIndexBasedDisplayId(uint8_t output_index) const;

  // Generates a unique display ID out of a mix of |manufacturer_id_|,
  // |product_id_|, |display_name_|, |week_of_manufacture_|,
  // |year_of_manufacture_|, |max_image_size_|,
  // |block_zero_serial_number_hash_|, and
  // |descriptor_block_serial_number_hash_|. Note that a hash will be produced
  // regardless of whether or not some (or all) of the fields are
  // missing/empty/default.
  // Here, uniqueness is solely based on a display's EDID and is not guaranteed
  // due to known EDIDs' completeness and correctness issues.
  int64_t GetEdidBasedDisplayId() const;

  // Bitmask of audio formats supported by the display.
  enum : uint32_t {
    kAudioBitstreamPcmLinear = 1u << 0,  // PCM is 'raw' amplitude samples.
    kAudioBitstreamDts = 1u << 1,        // Compressed DTS bitstream.
    kAudioBitstreamDtsHd = 1u << 2,      // Compressed DTS-HD bitstream.
  };
  uint32_t audio_formats() const { return audio_formats_; }

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

  bool is_external_display() const { return is_external_display_; }

  // Returns true if the display is a tiled display and the tile (which all have
  // their own EDID) specified that its content will stretch to fit the entire
  // display across all tiles if the tile is the only tile being transmitted.
  // Returns false if the display is not tiled, if EDID does not have a
  // DisplayID tiled display block, or specifies a different behavior (e.g.
  // clone).
  bool TileCanScaleToFit() const;

  const std::vector<uint8_t>& edid_blob() const { return edid_blob_; }

 private:
  // Parses |edid_blob|, filling up as many as possible fields below.
  void ParseEdid(const std::vector<uint8_t>& edid);

  // We collect optional fields UMAs for external external displays only.
  void ReportEdidOptionalsForExternalDisplay() const;

  // DisplayID in this context refers to the VESA standard for display metadata,
  // not the identifier used throughout ash/ozone.
  void ParseDisplayIdExtension(const std::vector<uint8_t>& edid,
                               size_t extension_offset);

  // Parses Tiled Display Topology data blocks for DisplayID v1.3 and v2.0.
  void ParseTiledDisplayBlock(const std::vector<uint8_t>& edid,
                              size_t block_offset);

  std::vector<uint8_t> edid_blob_;

  // Whether or not this EDID belongs to an external display.
  bool is_external_display_;

  uint16_t manufacturer_id_;
  uint16_t product_id_;
  std::optional<std::string> block_zero_serial_number_hash_;
  std::optional<std::string> descriptor_block_serial_number_hash_;
  std::optional<gfx::Size> max_image_size_;
  std::string display_name_;
  // Active pixel size from the first detailed timing descriptor in the EDID.
  gfx::Size active_pixel_size_;
  // When |week_of_manufacture_| == 0xFF, |year_of_manufacture_| is model year.
  std::optional<int32_t> week_of_manufacture_;
  int32_t year_of_manufacture_;
  std::optional<bool> overscan_flag_;
  double gamma_;
  int bits_per_channel_;
  SkColorSpacePrimaries primaries_;

  base::flat_set<PrimaryMatrixPair> supported_color_primary_matrix_ids_;
  base::flat_set<gfx::ColorSpace::TransferID> supported_color_transfer_ids_;
  std::optional<gfx::HDRStaticMetadata> hdr_static_metadata_;
  std::optional<uint16_t> vsync_rate_min_;

  uint32_t audio_formats_;

  bool tile_can_scale_to_fit_ = false;
};

}  // namespace display

#endif  // UI_DISPLAY_UTIL_EDID_PARSER_H_
