// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/util/edid_parser.h"

#include <stddef.h>

#include <algorithm>
#include <bitset>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace {

constexpr char kParseEdidFailureMetric[] = "Display.ParseEdidFailure";
constexpr char kParseExternalDisplayEdidOptionalsMetric[] =
    "Display.External.ParseEdidOptionals";
constexpr char kBlockZeroSerialNumberTypeMetric[] =
    "Display.External.BlockZeroSerialNumberType";
constexpr char kNumOfSerialNumbersProvidedByExternalDisplay[] =
    "Display.External.NumOfSerialNumbersProvided";
constexpr uint8_t kMaxSerialNumberCount = 2;
constexpr uint8_t kDisplayIdExtensionTag = 0x70;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ParseEdidFailure {
  kNoError = 0,
  kManufacturerId = 1,
  kProductId = 2,
  kYearOfManufacture = 3,
  kBitsPerChannel = 4,
  kGamma = 5,
  kChromaticityCoordinates = 6,
  kDisplayName = 7,
  kExtensions = 8,
  kSerialNumber = 9,
  kWeekOfManufacture = 10,
  kPhysicalSize = 11,
  kMaxValue = kPhysicalSize,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum is used to track the
// availability (or lack thereof) of optional fields during EDID parsing.
enum class ParseEdidOptionals {
  kAllAvailable = 0,
  kBlockZeroSerialNumber = 1,
  kDescriptorBlockSerialNumber = 2,
  kWeekOfManufacture = 3,
  kPhysicalSize = 4,
  kMaxValue = kPhysicalSize,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum is used to track the
// serial number types that can be retrieved from an EDID's block zero.
enum class BlockZeroSerialNumberType {
  kNormal = 0,
  kRepeatingPattern = 1,
  kNoSerialNumber = 2,
  kMaxValue = kNoSerialNumber,
};

BlockZeroSerialNumberType GetSerialNumberType(
    base::span<const uint8_t, 4u> serial_number) {
  uint32_t sum = serial_number[0u];
  bool all_equal = true;
  for (size_t i = 1u; i < serial_number.size(); ++i) {
    sum += serial_number[i];
    if (serial_number[i - 1u] != serial_number[i]) {
      all_equal = false;
    }
  }

  if (sum == 0u) {
    return BlockZeroSerialNumberType::kNoSerialNumber;
  }

  if (all_equal) {
    return BlockZeroSerialNumberType::kRepeatingPattern;
  }

  return BlockZeroSerialNumberType::kNormal;
}
}  // namespace

EdidParser::EdidParser(std::vector<uint8_t>&& edid_blob, bool is_external)
    : edid_blob_(std::move(edid_blob)),
      is_external_display_(is_external),
      manufacturer_id_(0),
      product_id_(0),
      year_of_manufacture_(display::kInvalidYearOfManufacture),
      gamma_(0.0),
      bits_per_channel_(-1),
      primaries_({0}),
      audio_formats_(0) {
  ParseEdid(edid_blob_);
}

EdidParser::EdidParser(EdidParser&& other) = default;
EdidParser& EdidParser::operator=(EdidParser&& other) = default;
EdidParser::~EdidParser() = default;

uint32_t EdidParser::GetProductCode() const {
  return ((static_cast<uint32_t>(manufacturer_id_) << 16) |
          (static_cast<uint32_t>(product_id_)));
}

int64_t EdidParser::GetIndexBasedDisplayId(uint8_t output_index) const {
  // Generates product specific value from product_name instead of product code.
  // See https://crbug.com/240341
  const uint32_t product_code_hash =
      display_name_.empty() ? 0 : base::Hash(display_name_);
  // An ID based on display's index will be assigned later if this call fails.
  return GenerateDisplayID(manufacturer_id_, product_code_hash, output_index);
}

int64_t EdidParser::GetEdidBasedDisplayId() const {
  const std::string string_to_hash =
      base::NumberToString(manufacturer_id_) +
      base::NumberToString(product_id_) + display_name_ +
      base::NumberToString(week_of_manufacture()) +
      base::NumberToString(year_of_manufacture_) + max_image_size().ToString() +
      block_zero_serial_number_hash() + descriptor_block_serial_number_hash();
  return static_cast<int64_t>(base::PersistentHash(string_to_hash));
}

// static
void EdidParser::SplitProductCodeInManufacturerIdAndProductId(
    int64_t product_code,
    uint16_t* manufacturer_id,
    uint16_t* product_id) {
  DCHECK(manufacturer_id);
  DCHECK(product_id);
  // Undo GetProductCode() packing.
  *product_id = product_code & 0xFFFF;
  *manufacturer_id = (product_code >> 16) & 0xFFFF;
}

// static
std::string EdidParser::ManufacturerIdToString(uint16_t manufacturer_id) {
  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 2, Sep 2006, Sec 3.4.1 "ID Manufacturer Name: 2 Bytes". Essentially these
  // are 3 5-bit ASCII characters packed in 2 bytes, where 1 means 'A', etc.
  constexpr uint8_t kFiveBitAsciiMask = 0x1F;
  constexpr char kFiveBitToAsciiOffset = 'A' - 1;
  constexpr size_t kSecondLetterOffset = 5;
  constexpr size_t kFirstLetterOffset = 10;

  char out[4] = {};
  out[2] = (manufacturer_id & kFiveBitAsciiMask) + kFiveBitToAsciiOffset;
  out[1] = ((manufacturer_id >> kSecondLetterOffset) & kFiveBitAsciiMask) +
           kFiveBitToAsciiOffset;
  out[0] = ((manufacturer_id >> kFirstLetterOffset) & kFiveBitAsciiMask) +
           kFiveBitToAsciiOffset;
  return out;
}

// static
std::string EdidParser::ProductIdToString(uint16_t product_id) {
  // From "VESA Enhanced EDID Standard" Release A, Revision 2, Sep 2006, Sec
  // 3.4.2 "ID Product Code: 2 Bytes": "The ID product code field, [...]
  // contains a 2-byte manufacturer assigned product code. [...] The 2 byte
  // number is stored in hex with the least significant byte listed first."
  uint8_t lower_char = (product_id >> 8) & 0xFF;
  uint8_t upper_char = product_id & 0xFF;
  return base::StringPrintf("%02X%02X", upper_char, lower_char);
}

bool EdidParser::TileCanScaleToFit() const {
  return tile_can_scale_to_fit_;
}

void EdidParser::ParseEdid(const std::vector<uint8_t>& edid) {
  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the details of EDID data format.  We use the following data:
  //   bytes 8-9: manufacturer EISA ID, in big-endian
  //   bytes 10-11: manufacturer product code, in little-endian
  constexpr size_t kManufacturerOffset = 8;
  constexpr size_t kManufacturerLength = 2;
  constexpr size_t kProductIdOffset = 10;
  constexpr size_t kProductIdLength = 2;

  if (edid.size() < kManufacturerOffset + kManufacturerLength) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kManufacturerId);
    return;  // Any other fields below are beyond this edid offset.
  }
  // ICC filename is generated based on these ids. We always read this as big
  // endian so that the file name matches bytes 8-11 as they appear in EDID.
  manufacturer_id_ = base::numerics::U16FromBigEndian(
      base::span(edid).subspan<kManufacturerOffset, kManufacturerLength>());

  if (edid.size() < kProductIdOffset + kProductIdLength) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kProductId);
    return;  // Any other fields below are beyond this edid offset.
  }
  // TODO: crbug.com/332745398 - The comment above says this data is in little
  // endian, however there was a mistake in the past which led to us parsing
  // this as big endian, and so we are now maintaining consistency with that. We
  // cannot fix this without disturbing display support, as the product ID is
  // used to produce display IDs and we need these to stay consistent. We'll
  // have to keep parsing it incorrectly until we migrate to EDID-based display
  // IDs. See also (googlers-only) http://b/193019614.
  product_id_ = base::numerics::U16FromBigEndian(
      base::span(edid).subspan<kProductIdOffset, kProductIdLength>());

  //   Bytes 12-15: display serial number, in little-endian (LSB). This field is
  //   optional and its absence is marked by having all bytes set to 0x00.
  //   Values do not represent ASCII characters.
  constexpr size_t kSerialNumberOffset = 12;
  constexpr size_t kSerialNumberLength = 4;

  if (edid.size() < kSerialNumberOffset + kSerialNumberLength) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kSerialNumber);
    return;  // Any other fields below are beyond this edid offset.
  }

  auto serial_number_bytes =
      base::span(edid).subspan<kSerialNumberOffset, kSerialNumberLength>();

  // Report the type of serial number encountered in block zero of external
  // displays: empty (==0), repeating pattern (e.g. 01010101 or 0F0F0F0F),
  // or normal.
  if (is_external_display_) {
    base::UmaHistogramEnumeration(kBlockZeroSerialNumberTypeMetric,
                                  GetSerialNumberType(serial_number_bytes));
  }

  const uint32_t serial_number =
      base::numerics::U32FromLittleEndian(serial_number_bytes);
  if (serial_number) {
    block_zero_serial_number_hash_ =
        base::MD5String(base::NumberToString(serial_number));
  }

  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 2, Sep 2006, Sec 3.4.4 "Week and Year of Manufacture or Model Year: 2
  // Bytes".
  constexpr size_t kWeekOfManufactureOffset = 16;
  constexpr uint32_t kValidWeekValueUpperBound = 0x36;
  constexpr uint32_t kModelYearMarker = 0xFF;

  if (edid.size() < kWeekOfManufactureOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kWeekOfManufacture);
    return;  // Any other fields below are beyond this edid offset.
  }
  {
    const uint8_t byte_data = edid[kWeekOfManufactureOffset];
    // Store the value if it's within the range of 1-54 or equals to 0xFF.
    if ((byte_data > 0x00 && byte_data <= kValidWeekValueUpperBound) ||
        byte_data == kModelYearMarker) {
      week_of_manufacture_ = byte_data;
    }
  }

  constexpr size_t kYearOfManufactureOffset = 17;
  constexpr uint32_t kValidYearValueLowerBound = 0x10;
  constexpr int32_t kYearOffset = 1990;

  if (edid.size() < kYearOfManufactureOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kYearOfManufacture);
    return;  // Any other fields below are beyond this edid offset.
  }
  {
    const uint8_t byte_data = edid[kYearOfManufactureOffset];
    if (byte_data >= kValidYearValueLowerBound) {
      year_of_manufacture_ = byte_data + kYearOffset;
    }
  }

  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 1, Feb 2000, Sec 3.6 "Basic Display Parameters and Features: 5 bytes"
  static constexpr int kBitsPerChannelTable[] = {0, 6, 8, 10, 12, 14, 16, 0};

  constexpr size_t kEDIDRevisionNumberOffset = 19;
  constexpr uint8_t kEDIDRevision4Value = 4;

  constexpr size_t kVideoInputDefinitionOffset = 20;
  constexpr uint8_t kDigitalInfoMask = 0x80;
  constexpr uint8_t kColorBitDepthMask = 0x70;
  constexpr uint8_t kColorBitDepthOffset = 4;

  if (edid.size() < kVideoInputDefinitionOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kBitsPerChannel);
    return;  // Any other fields below are beyond this edid offset.
  }
  if (edid[kEDIDRevisionNumberOffset] >= kEDIDRevision4Value &&
      (edid[kVideoInputDefinitionOffset] & kDigitalInfoMask)) {
    // EDID needs to be revision 4 at least, and kDigitalInfoMask be set for
    // the Video Input Definition entry to describe a digital interface.
    bits_per_channel_ = kBitsPerChannelTable[(
        (edid[kVideoInputDefinitionOffset] & kColorBitDepthMask) >>
        kColorBitDepthOffset)];
  }

  constexpr size_t kEDIDMaxHorizontalImageSizeOffset = 21;
  constexpr size_t kEDIDMaxVerticalImageSizeOffset = 22;

  if (edid.size() < kEDIDMaxVerticalImageSizeOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kPhysicalSize);
    return;  // Any other fields below are beyond this edid offset.
  }
  const gfx::Size max_image_size(edid[kEDIDMaxHorizontalImageSizeOffset],
                                 edid[kEDIDMaxVerticalImageSizeOffset]);
  if (!max_image_size.IsEmpty()) {
    max_image_size_ = max_image_size;
  }

  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 2, Sep 2006, Sec. 3.6.3 "Display Transfer Characteristics (GAMMA ): 1 Byte"
  constexpr size_t kGammaOffset = 23;
  constexpr double kGammaMultiplier = 100.0;
  constexpr double kGammaBias = 100.0;

  if (edid.size() < kGammaOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kGamma);
    return;  // Any other fields below are beyond this edid offset.
  }
  if (edid[kGammaOffset] != 0xFF) {
    // Otherwise the byte at kGammaOffset is 0xFF, gamma is stored elsewhere.
    gamma_ = (edid[kGammaOffset] + kGammaBias) / kGammaMultiplier;
  }

  // Offsets, lengths, positions and masks are taken from [1] (or [2]).
  // [1] http://en.wikipedia.org/wiki/Extended_display_identification_data
  // [2] "VESA Enhanced EDID Standard " Release A, Revision 1, Feb 2000, Sec 3.7
  //  "Phosphor or Filter Chromaticity: 10 bytes"
  constexpr size_t kChromaticityOffset = 25;
  constexpr unsigned int kChromaticityLength = 10;

  constexpr size_t kRedGreenLsbOffset = 25;
  constexpr uint8_t kRedxLsbPosition = 6;
  constexpr uint8_t kRedyLsbPosition = 4;
  constexpr uint8_t kGreenxLsbPosition = 2;
  constexpr uint8_t kGreenyLsbPosition = 0;

  constexpr size_t kBlueWhiteLsbOffset = 26;
  constexpr uint8_t kBluexLsbPosition = 6;
  constexpr uint8_t kBlueyLsbPosition = 4;
  constexpr uint8_t kWhitexLsbPosition = 2;
  constexpr uint8_t kWhiteyLsbPosition = 0;

  // All LSBits parts are 2 bits wide.
  constexpr uint8_t kLsbMask = 0x3;

  constexpr size_t kRedxMsbOffset = 27;
  constexpr size_t kRedyMsbOffset = 28;
  constexpr size_t kGreenxMsbOffset = 29;
  constexpr size_t kGreenyMsbOffset = 30;
  constexpr size_t kBluexMsbOffset = 31;
  constexpr size_t kBlueyMsbOffset = 32;
  constexpr size_t kWhitexMsbOffset = 33;
  constexpr size_t kWhiteyMsbOffset = 34;

  static_assert(
      kChromaticityOffset + kChromaticityLength == kWhiteyMsbOffset + 1,
      "EDID Parameter section length error");

  if (edid.size() < kChromaticityOffset + kChromaticityLength) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kChromaticityCoordinates);
    return;  // Any other fields below are beyond this edid offset.
  }

  const uint8_t red_green_lsbs = edid[kRedGreenLsbOffset];
  const uint8_t blue_white_lsbs = edid[kBlueWhiteLsbOffset];

  // Recompose the 10b values by appropriately mixing the 8 MSBs and the 2 LSBs,
  // then rescale to 1024;
  primaries_.fRX = ((edid[kRedxMsbOffset] << 2) +
                    ((red_green_lsbs >> kRedxLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fRY = ((edid[kRedyMsbOffset] << 2) +
                    ((red_green_lsbs >> kRedyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fGX = ((edid[kGreenxMsbOffset] << 2) +
                    ((red_green_lsbs >> kGreenxLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fGY = ((edid[kGreenyMsbOffset] << 2) +
                    ((red_green_lsbs >> kGreenyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fBX = ((edid[kBluexMsbOffset] << 2) +
                    ((blue_white_lsbs >> kBluexLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fBY = ((edid[kBlueyMsbOffset] << 2) +
                    ((blue_white_lsbs >> kBlueyLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fWX = ((edid[kWhitexMsbOffset] << 2) +
                    ((blue_white_lsbs >> kWhitexLsbPosition) & kLsbMask)) /
                   1024.0f;
  primaries_.fWY = ((edid[kWhiteyMsbOffset] << 2) +
                    ((blue_white_lsbs >> kWhiteyLsbPosition) & kLsbMask)) /
                   1024.0f;
  // TODO(mcasas): Up to two additional White Point coordinates can be provided
  // in a Display Descriptor. Read them if we are not satisfied with |fWX| or
  // |fWy|. https://crbug.com/771345.

  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the details of EDID data format.  We use the following data:
  //   bytes 54-125: four descriptors (18-bytes each) which may contain
  //     the display name.
  constexpr size_t kDescriptorOffset = 54;
  constexpr size_t kNumDescriptors = 4;
  constexpr size_t kDescriptorLength = 18;
  // The specifier types.
  constexpr uint8_t kMonitorNameDescriptor = 0xfc;
  constexpr uint8_t kDisplayRangeLimitsDescriptor = 0xfd;
  constexpr uint8_t kMonitorSerialNumberDescriptor = 0xff;

  display_name_.clear();
  for (size_t i = 0; i < kNumDescriptors; ++i) {
    if (edid.size() < kDescriptorOffset + (i + 1) * kDescriptorLength) {
      break;
    }

    size_t offset = kDescriptorOffset + i * kDescriptorLength;

    // Detailed Timing Descriptor:
    if (edid[offset] != 0 && edid[offset + 1] != 0) {
      constexpr int kMaxResolution = 10080;  // 8k display.

      // EDID may contain multiple DTD. Use the first one, that contains the
      // highest resolution.
      if (active_pixel_size_.IsEmpty()) {
        constexpr size_t kHorizontalPixelLsbOffset = 2;
        constexpr size_t kHorizontalPixelMsbOffset = 4;
        constexpr size_t kVerticalPixelLsbOffset = 5;
        constexpr size_t kVerticalPixelMsbOffset = 7;

        const uint8_t h_lsb = edid[offset + kHorizontalPixelLsbOffset];
        const uint8_t h_msb = edid[offset + kHorizontalPixelMsbOffset];
        int h_pixel = std::min(h_lsb + ((h_msb & 0xF0) << 4), kMaxResolution);

        const uint8_t v_lsb = edid[offset + kVerticalPixelLsbOffset];
        const uint8_t v_msb = edid[offset + kVerticalPixelMsbOffset];
        int v_pixel = std::min(v_lsb + ((v_msb & 0xF0) << 4), kMaxResolution);

        active_pixel_size_.SetSize(h_pixel, v_pixel);
      }
      continue;
    }

    // EDID Other Monitor Descriptors:
    // If the descriptor contains the display name, it has the following
    // structure:
    //   bytes 0-2, 4: \0
    //   byte 3: 0xfc
    //   bytes 5-17: text data, ending with \r, padding with spaces
    // we should check bytes 0-2 and 4, since it may have other values in
    // case that the descriptor contains other type of data.
    if (edid[offset] == 0 && edid[offset + 1] == 0 && edid[offset + 2] == 0 &&
        edid[offset + 3] == kMonitorNameDescriptor && edid[offset + 4] == 0) {
      std::string name(reinterpret_cast<const char*>(&edid[offset + 5]),
                       kDescriptorLength - 5);
      base::TrimWhitespaceASCII(name, base::TRIM_TRAILING, &display_name_);
      continue;
    }

    // If the descriptor contains the display's range limits, it has the
    // following structure:
    //   bytes 0-2: \0
    //   byte 3: 0xfd
    //   byte 4: Offsets for display range limits
    //   bytes 5-17: Display range limits and timing information
    if (edid[offset] == 0 && edid[offset + 1] == 0 && edid[offset + 2] == 0 &&
        edid[offset + 3] == kDisplayRangeLimitsDescriptor) {
      // byte 4: Offsets for display range limits
      const uint8_t rateOffset = edid[offset + 4];
      // bits 7-4: Reserved \0
      if (rateOffset & 0xf0) {
        continue;
      }
      // bit 3: Horizontal max rate offset (not used)
      // bit 2: Horizontal min rate offset (not used)
      // bit 1: Vertical max rate offset (not used)
      // bit 0: Vertical min rate offset
      const uint8_t verticalMinRateOffset = rateOffset & (1 << 0) ? 255 : 0;

      // bytes 5-8: Rate limits
      // Each byte must be within [1, 255].
      if (edid[offset + 5] == 0 || edid[offset + 6] == 0 ||
          edid[offset + 7] == 0 || edid[offset + 8] == 0) {
        continue;
      }
      // byte 5: Min vertical rate in Hz
      vsync_rate_min_ = edid[offset + 5] + verticalMinRateOffset;
      // byte 6: Max vertical rate in Hz (not used)
      // byte 7: Min horizontal rate in kHz (not used)
      // byte 8: Max horizontal rate in kHz (not used)

      // byte 9: Maximum pixel clock rate (not used)
      // byte 10: Extended timing information type (not used)
      // bytes 11-17: Video timing parameters (not used)

      continue;
    }

    // If the descriptor contains the display's product serial number, it has
    // the following structure:
    //   bytes 0-2, 4: \0
    //   byte 3: 0xff
    //   bytes 5-17: text data, ending with \r, padding with spaces
    // we should check bytes 0-2 and 4, since it may have other values in
    // case that the descriptor contains other type of data.
    if (edid[offset] == 0 && edid[offset + 1] == 0 && edid[offset + 2] == 0 &&
        edid[offset + 3] == kMonitorSerialNumberDescriptor &&
        edid[offset + 4] == 0) {
      std::string serial_number_str(
          reinterpret_cast<const char*>(&edid[offset + 5]),
          kDescriptorLength - 5);
      base::TrimWhitespaceASCII(serial_number_str, base::TRIM_TRAILING,
                                &serial_number_str);
      if (!serial_number_str.empty()) {
        descriptor_block_serial_number_hash_ =
            base::MD5String(serial_number_str);
      }
      continue;
    }
  }

  // Verify if the |display_name_| consists of printable characters only.
  // Replace unprintable chars with white space.
  std::replace_if(
      display_name_.begin(), display_name_.end(),
      [](unsigned char c) {
        return !absl::ascii_isascii(c) || !absl::ascii_isprint(c);
      },
      ' ');

  // See http://en.wikipedia.org/wiki/Extended_display_identification_data
  // for the extension format of EDID.  Also see EIA/CEA-861 spec for
  // the format of the extensions and how video capability is encoded.
  //  - byte 0: tag.  should be 02h.
  //  - byte 1: revision.  only cares revision 3 (03h).
  //  - byte 4-: data block.
  constexpr size_t kExtensionBaseOffset = 128;
  constexpr size_t kExtensionSize = 128;
  constexpr size_t kNumExtensionsOffset = 126;
  constexpr size_t kDataBlockOffset = 4;
  constexpr uint8_t kCEAExtensionTag = '\x02';
  constexpr uint8_t kExpectedExtensionRevision = '\x03';
  constexpr uint8_t kAudioTag = 1;
  constexpr uint8_t kExtendedTag = 7;
  constexpr uint8_t kExtendedVideoCapabilityTag = 0;
  constexpr uint8_t kPTOverscanFlagPosition = 4;
  constexpr uint8_t kITOverscanFlagPosition = 2;
  constexpr uint8_t kCEOverscanFlagPosition = 0;
  // See CTA-861-F, particularly Table 56 "Colorimetry Data Block".
  constexpr uint8_t kColorimetryDataBlockCapabilityTag = 0x05;
  constexpr std::pair<gfx::ColorSpace::PrimaryID, gfx::ColorSpace::MatrixID>
      kPrimaryMatrixIDMap[] = {
          // xvYCC601. Standard Definition Colorimetry based on IEC 61966-2-4.
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::SMPTE170M},
          // xvYCC709. High Definition Colorimetry based on IEC 61966-2-4.
          {gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::MatrixID::BT709},
          // sYCC601. Colorimetry based on IEC 61966-2-1/Amendment 1.
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::SMPTE170M},
          // opYCC601. Colorimetry based on IEC 61966-2-5, Annex A.
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::SMPTE170M},
          // opRGB, Colorimetry based on IEC 61966-2-5.
          {gfx::ColorSpace::PrimaryID::SMPTE170M,
           gfx::ColorSpace::MatrixID::RGB},
          // BT2020YCC. Colorimetry based on ITU-R BT.2020 Y’C’BC’R.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // BT2020YCC. Colorimetry based on ITU-R BT.2020 Y’C’BC’R.
          {gfx::ColorSpace::PrimaryID::BT2020,
           gfx::ColorSpace::MatrixID::BT2020_NCL},
          // BT2020RGB. Colorimetry based on ITU-R BT.2020 R’G’B’.
          {gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::MatrixID::RGB},
          // MD0. Metadata bit.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // MD1. Metadata bit.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // MD2. Metadata bit.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // MD3. Metadata bit.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // F44=0.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // F45=0.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // F46=0.
          {gfx::ColorSpace::PrimaryID::INVALID,
           gfx::ColorSpace::MatrixID::INVALID},
          // DCI-P3. Colorimetry based on DCI-P3.
          {gfx::ColorSpace::PrimaryID::P3, gfx::ColorSpace::MatrixID::RGB}};
  // See CEA 861.G-2018, Sec.7.5.13, "HDR Static Metadata Data Block" for these.
  constexpr uint8_t kHDRStaticMetadataCapabilityTag = 0x6;
  constexpr gfx::ColorSpace::TransferID kTransferIDMap[] = {
      gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::TransferID::GAMMA24,
      gfx::ColorSpace::TransferID::PQ,
      // STD B67 is also known as Hybrid-log Gamma (HLG).
      gfx::ColorSpace::TransferID::HLG,
  };
  constexpr uint8_t kHDRStaticMetadataDataBlockLengthMask = 0x1F;

  if (edid.size() < kNumExtensionsOffset + 1) {
    base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                  ParseEdidFailure::kExtensions);
    return;  // Any other fields below are beyond this edid offset.
  }
  const uint8_t num_extensions = edid[kNumExtensionsOffset];

  for (size_t i = 0; i < num_extensions; ++i) {
    // Skip parsing the whole extension if size is not enough.
    if (edid.size() < kExtensionBaseOffset + (i + 1) * kExtensionSize) {
      break;
    }

    const size_t extension_offset = kExtensionBaseOffset + i * kExtensionSize;
    const uint8_t extention_tag = edid[extension_offset];
    const uint8_t revision = edid[extension_offset + 1];

    if (extention_tag == kDisplayIdExtensionTag) {
      ParseDisplayIdExtension(edid, extension_offset);
      continue;
    }
    if (extention_tag != kCEAExtensionTag ||
        revision != kExpectedExtensionRevision) {
      continue;
    }

    const uint8_t timing_descriptors_start = std::min(
        edid[extension_offset + 2], static_cast<unsigned char>(kExtensionSize));

    for (size_t data_offset = extension_offset + kDataBlockOffset;
         data_offset < extension_offset + timing_descriptors_start;) {
      // A data block is encoded as:
      // - byte 1 high 3 bits: tag: '1' for audio, '7' for extended tags.
      // - byte 1 remaining bits: the length of data block after header.
      // - byte 2: the extended tag. E.g. '0' for video capability. Values are
      //   defined by the k...CapabilityTag constants.
      // - byte 3: the capability.
      const uint8_t tag = edid[data_offset] >> 5;
      const uint8_t payload_length = edid[data_offset] & 0x1f;
      if (data_offset + payload_length + 1 > edid.size()) {
        break;
      }

      // Short Audio Descriptors contain passthrough audio support information.
      // Note: Short Audio Descriptors also contain channel count and sampling
      // frequency information as described in:
      // CTA-861-G (2017) section 7.5.2 Audio Data Block.
      if (tag == kAudioTag) {
        constexpr uint8_t kCEAShortAudioDescriptorLength = 3;
        constexpr uint8_t kFormatBitsLPCM = 1;
        constexpr uint8_t kFormatBitsDTS = 7;
        constexpr uint8_t kFormatBitsDTSHD = 11;

        for (int sad_index = 0;
             sad_index + kCEAShortAudioDescriptorLength <= payload_length;
             sad_index += kCEAShortAudioDescriptorLength) {
          switch ((edid[data_offset + 1 + sad_index] >> 3) & 0x1F) {
            case kFormatBitsLPCM:
              audio_formats_ |= kAudioBitstreamPcmLinear;
              break;
            case kFormatBitsDTS:
              audio_formats_ |= kAudioBitstreamDts;
              break;
            case kFormatBitsDTSHD:
              audio_formats_ |= kAudioBitstreamDtsHd;
              break;
          }
        }
        data_offset += payload_length + 1;
        continue;
      }

      if (tag != kExtendedTag || payload_length < 2) {
        data_offset += payload_length + 1;
        continue;
      }

      switch (edid[data_offset + 1]) {
        case kExtendedVideoCapabilityTag:
          // The difference between preferred, IT, and CE video formats doesn't
          // matter. Set the flag to true if any of these flags are true.
          overscan_flag_ =
              (edid[data_offset + 2] & (1 << kPTOverscanFlagPosition)) ||
              (edid[data_offset + 2] & (1 << kITOverscanFlagPosition)) ||
              (edid[data_offset + 2] & (1 << kCEOverscanFlagPosition));
          break;

        case kColorimetryDataBlockCapabilityTag: {
          constexpr size_t kMaxNumColorimetryEntries = 16;
          // The Colorimetry Data Block bitfield is 2 bytes long, the second
          // byte containing the most significant bit (MSB), so it needs to be
          // shifted to the left to create a 16 bit long value that can be
          // passed to the bitset constructor.
          long cdb_bits = edid[data_offset + 2];
          if (edid.size() > data_offset + 3) {
            cdb_bits += edid[data_offset + 3] << 8;
          }
          const std::bitset<kMaxNumColorimetryEntries>
              supported_primaries_bitfield(cdb_bits);
          static_assert(
              kMaxNumColorimetryEntries == std::size(kPrimaryMatrixIDMap),
              "kPrimaryIDMap should describe all possible colorimetry entries");
          for (size_t entry = 0; entry < kMaxNumColorimetryEntries; ++entry) {
            if (supported_primaries_bitfield[entry] &&
                std::get<0>(kPrimaryMatrixIDMap[entry]) !=
                    gfx::ColorSpace::PrimaryID::INVALID &&
                std::get<1>(kPrimaryMatrixIDMap[entry]) !=
                    gfx::ColorSpace::MatrixID::INVALID) {
              supported_color_primary_matrix_ids_.insert(
                  kPrimaryMatrixIDMap[entry]);
            }
          }
          break;
        }

        case kHDRStaticMetadataCapabilityTag: {
          constexpr size_t kMaxNumHDRStaticMetadataEntries = 4;
          const std::bitset<kMaxNumHDRStaticMetadataEntries>
              supported_eotfs_bitfield(edid[data_offset + 2]);
          static_assert(
              kMaxNumHDRStaticMetadataEntries == std::size(kTransferIDMap),
              "kTransferIDMap should describe all possible transfer entries");
          for (size_t entry = 0; entry < kMaxNumHDRStaticMetadataEntries;
               ++entry) {
            if (supported_eotfs_bitfield[entry]) {
              supported_color_transfer_ids_.insert(kTransferIDMap[entry]);
            }
          }
          hdr_static_metadata_ = std::make_optional<gfx::HDRStaticMetadata>({});
          hdr_static_metadata_->supported_eotf_mask =
              base::checked_cast<uint8_t>(supported_eotfs_bitfield.to_ulong());

          // See CEA 861.3-2015, Sec.7.5.13, "HDR Static Metadata Data Block"
          // for details on the following calculations.
          const uint8_t length_of_data_block =
              edid[data_offset] & kHDRStaticMetadataDataBlockLengthMask;
          if (length_of_data_block <= 3) {
            break;
          }
          const uint8_t desired_content_max_luminance = edid[data_offset + 4];
          hdr_static_metadata_->max =
              50.0 * pow(2, desired_content_max_luminance / 32.0);

          if (length_of_data_block <= 4) {
            break;
          }
          const uint8_t desired_content_max_frame_average_luminance =
              edid[data_offset + 5];
          hdr_static_metadata_->max_avg =
              50.0 * pow(2, desired_content_max_frame_average_luminance / 32.0);

          if (length_of_data_block <= 5) {
            break;
          }
          const uint8_t desired_content_min_luminance = edid[data_offset + 6];
          hdr_static_metadata_->min =
              hdr_static_metadata_->max *
              pow(desired_content_min_luminance / 255.0, 2) / 100.0;
          break;
        }
        default:
          break;
      }

      data_offset += payload_length + 1;
    }
  }
  base::UmaHistogramEnumeration(kParseEdidFailureMetric,
                                ParseEdidFailure::kNoError);
  ReportEdidOptionalsForExternalDisplay();
}

// TODO(b/316356595): Move DisplayID parsing into its own class.
// NOTE: Refer to figure Figure 2-1 of VESA DisplayID Standard Version 2.1 for
// how DisplayID Structure v2.0 is laid out as an EDID extension.
void EdidParser::ParseDisplayIdExtension(const std::vector<uint8_t>& edid,
                                         size_t extension_offset) {
  const uint8_t extension_tag = edid[extension_offset];
  if (extension_tag != kDisplayIdExtensionTag) {
    LOG(ERROR) << "Unable to proceed with parsing DisplayID extension as "
                  "extension tag is not for DisplayID (0x70). Actual tag: "
               << extension_tag;
    return;
  }

  // There are two data blocks that describe tiled displays:
  // * DisplayID v1.3 with tag 0x12
  // * DisplayID v2.0 with tag 0x28
  // The v1.3 block is superscede by v2.0. Both of the blocks are laregely
  // identical.
  constexpr uint8_t kTiledDisplayDataBlockTag2_0 = 0x28;
  constexpr uint8_t kTiledDisplayDataBlockTag1_3 = 0x12;

  // Section data block is divided into (block tag, revision #, number of
  // payload bytes, payload), where everything except for the payload is one
  // byte long.
  constexpr size_t kDataBlockNumPayloadBytesOffset = 2;
  constexpr size_t kDataBlockNonPayloadBytes = 3;

  // The EDID-extension section block tag is the first byte
  // (|extension_offset|), followed by 4 bytes of DisplayID extension section
  // header, then the data blocks.
  const size_t displayid_extension_offset = extension_offset + 1;
  const size_t displayid_data_block_base = displayid_extension_offset + 4;
  size_t current_data_block_offset = displayid_data_block_base;

  // The second byte in the extension section header indicates the total number
  // of bytes in the section data block(s). This should always be 121.
  const uint8_t num_bytes_in_section_data_blocks =
      edid[displayid_extension_offset + 1];
  if (num_bytes_in_section_data_blocks != 121) {
    LOG(WARNING) << "Number of bytes in section data block should be 121 "
                    "according to the "
                    "DisplayID spec. Actual # of bytes: "
                 << num_bytes_in_section_data_blocks;
    return;
  }

  const size_t max_offset =
      std::min(edid.size(),
               displayid_data_block_base + num_bytes_in_section_data_blocks);
  while (current_data_block_offset + kDataBlockNumPayloadBytesOffset <
             max_offset
         // If there are no remaining data blocks before the fixed 121 bytes of
         // section data block space runs out, the remaining space is padded
         // with 0. Since there are no data block tag with ID 0, if a data block
         // tag is 0 then the rest of the section is just padding.
         && edid[current_data_block_offset] != 0) {
    const uint8_t current_data_block_tag = edid[current_data_block_offset];
    switch (current_data_block_tag) {
      case kTiledDisplayDataBlockTag1_3:
      case kTiledDisplayDataBlockTag2_0:
        ParseTiledDisplayBlock(edid, current_data_block_offset);
        break;
    }
    // NOTE: Parse other DisplayID blocks here.

    // Increment |current_data_block_offset| to point to the next data block's
    // tag (1st byte of the section data block).
    current_data_block_offset +=
        edid[current_data_block_offset + kDataBlockNumPayloadBytesOffset] +
        kDataBlockNonPayloadBytes;
  }
}

// DisplayID 1.3 and 2.0 tiled display data blocks look identical, at
// least for the current set of fields. Consult both of the specs before
// parsing more fields.
void EdidParser::ParseTiledDisplayBlock(const std::vector<uint8_t>& edid,
                                        size_t block_offset) {
  // See:
  // https://en.wikipedia.org/wiki/DisplayID#0x28_Tiled_display_topology
  // "Tile capabilities" is described in the 4th byte (offset + 3).
  // Bits 2:0 describe "Tile Behavior when It Is the Only Tile Receiving an
  // Image from the Source". With value of 2 indicating that the tile will
  // "Scale to fit the display" when it is the only tile receiving an image from
  // the source.
  constexpr size_t kTileCapabilitiesOffset = 3;
  constexpr uint8_t kSingleTileBehaviorBitmask = 0b111;
  constexpr uint8_t kSingleTileStretchToFit = 0x02;

  if (edid.size() <= block_offset + kTileCapabilitiesOffset) {
    return;
  }

  tile_can_scale_to_fit_ =
      (edid[block_offset + kTileCapabilitiesOffset] &
       kSingleTileBehaviorBitmask) == kSingleTileStretchToFit;
}

void EdidParser::ReportEdidOptionalsForExternalDisplay() const {
  if (!is_external_display_) {
    return;
  }

  bool all_optionals_available = true;

  if (!week_of_manufacture_.has_value()) {
    all_optionals_available = false;
    base::UmaHistogramEnumeration(kParseExternalDisplayEdidOptionalsMetric,
                                  ParseEdidOptionals::kWeekOfManufacture);
  }
  if (!max_image_size_.has_value()) {
    all_optionals_available = false;
    base::UmaHistogramEnumeration(kParseExternalDisplayEdidOptionalsMetric,
                                  ParseEdidOptionals::kPhysicalSize);
  }
  uint8_t serial_number_count = kMaxSerialNumberCount;
  if (!block_zero_serial_number_hash_.has_value()) {
    all_optionals_available = false;
    serial_number_count--;
    base::UmaHistogramEnumeration(kParseExternalDisplayEdidOptionalsMetric,
                                  ParseEdidOptionals::kBlockZeroSerialNumber);
  }
  if (!descriptor_block_serial_number_hash_.has_value()) {
    all_optionals_available = false;
    serial_number_count--;
    base::UmaHistogramEnumeration(
        kParseExternalDisplayEdidOptionalsMetric,
        ParseEdidOptionals::kDescriptorBlockSerialNumber);
  }
  base::UmaHistogramExactLinear(kNumOfSerialNumbersProvidedByExternalDisplay,
                                serial_number_count, kMaxSerialNumberCount);

  if (all_optionals_available) {
    base::UmaHistogramEnumeration(kParseExternalDisplayEdidOptionalsMetric,
                                  ParseEdidOptionals::kAllAvailable);
  }
}

}  // namespace display
