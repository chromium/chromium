// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include <stddef.h>

#include <algorithm>
#include <bitset>

#include "base/hash/hash.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display {

EdidParser::EdidParser(const std::vector<uint8_t>& edid_blob)
    : manufacturer_id_(0),
      product_id_(0),
      year_of_manufacture_(display::kInvalidYearOfManufacture),
      gamma_(0.0),
      bits_per_channel_(-1),
      primaries_({0}) {
  ParseEdid(edid_blob);
}

EdidParser::~EdidParser() = default;

uint32_t EdidParser::GetProductCode() const {
  return ((static_cast<uint32_t>(manufacturer_id_) << 16) |
          (static_cast<uint32_t>(product_id_)));
}

int64_t EdidParser::GetDisplayId(uint8_t output_index) const {
  // Generates product specific value from product_name instead of product code.
  // See https://crbug.com/240341
  const uint32_t product_code_hash =
      display_name_.empty() ? 0 : base::Hash(display_name_);
  // An ID based on display's index will be assigned later if this call fails.
  return GenerateDisplayID(manufacturer_id_, product_code_hash, output_index);
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
    LOG(ERROR) << "Too short EDID data: manufacturer id";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
    return;  // Any other fields below are beyond this edid offset.
  }
  // ICC filename is generated based on these ids. We always read this as big
  // endian so that the file name matches bytes 8-11 as they appear in EDID.
  manufacturer_id_ =
      (edid[kManufacturerOffset] << 8) + edid[kManufacturerOffset + 1];

  if (edid.size() < kProductIdOffset + kProductIdLength) {
    LOG(ERROR) << "Too short EDID data: product id";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
    return;  // Any other fields below are beyond this edid offset.
  }
  product_id_ = (edid[kProductIdOffset] << 8) + edid[kProductIdOffset + 1];

  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 2, Sep 2006, Sec 3.4.4 "Week and Year of Manufacture or Model Year: 2
  // Bytes".
  constexpr size_t kYearOfManufactureOffset = 17;
  constexpr uint32_t kValidValueLowerBound = 0x10;
  constexpr int32_t kYearOffset = 1990;

  if (edid.size() < kYearOfManufactureOffset + 1) {
    LOG(ERROR) << "Too short EDID data: year of manufacture";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
    return;  // Any other fields below are beyond this edid offset.
  }
  const uint8_t byte_data = edid[kYearOfManufactureOffset];
  if (byte_data >= kValidValueLowerBound)
    year_of_manufacture_ = byte_data + kYearOffset;

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
    LOG(ERROR) << "Too short EDID data: bits per channel";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
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

  // Constants are taken from "VESA Enhanced EDID Standard" Release A, Revision
  // 2, Sep 2006, Sec. 3.6.3 "Display Transfer Characteristics (GAMMA ): 1 Byte"
  constexpr size_t kGammaOffset = 23;
  constexpr double kGammaMultiplier = 100.0;
  constexpr double kGammaBias = 100.0;

  if (edid.size() < kGammaOffset + 1) {
    LOG(ERROR) << "Too short EDID data: gamma";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
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
    LOG(ERROR) << "Too short EDID data: chromaticity coordinates";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
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

  display_name_.clear();
  for (size_t i = 0; i < kNumDescriptors; ++i) {
    if (edid.size() < kDescriptorOffset + (i + 1) * kDescriptorLength)
      break;

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
    //   byte 3: descriptor type, defined above.
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
  }

  // Verify if the |display_name_| consists of printable characters only.
  // TODO(oshima|muka): Consider replacing unprintable chars with white space.
  for (const char c : display_name_) {
    if (!isascii(c) || !isprint(c)) {
      display_name_.clear();
      LOG(ERROR) << "invalid EDID: human unreadable char in name";
      // TODO(mcasas): add UMA, https://crbug.com/821393.
    }
  }

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
  constexpr uint8_t kExtendedTag = 7;
  constexpr uint8_t kExtendedVideoCapabilityTag = 0;
  constexpr uint8_t kPTOverscanFlagPosition = 4;
  constexpr uint8_t kITOverscanFlagPosition = 2;
  constexpr uint8_t kCEOverscanFlagPosition = 0;
  // See CTA-861-F, particularly Table 56 "Colorimetry Data Block".
  constexpr uint8_t kColorimetryDataBlockCapabilityTag = 0x05;
  constexpr gfx::ColorSpace::PrimaryID kPrimaryIDMap[] = {
      // xvYCC601. Standard Definition Colorimetry based on IEC 61966-2-4.
      gfx::ColorSpace::PrimaryID::SMPTE170M,
      // xvYCC709. High Definition Colorimetry based on IEC 61966-2-4.
      gfx::ColorSpace::PrimaryID::BT709,
      // sYCC601. Colorimetry based on IEC 61966-2-1/Amendment 1.
      gfx::ColorSpace::PrimaryID::SMPTE170M,
      // opYCC601. Colorimetry based on IEC 61966-2-5, Annex A.
      gfx::ColorSpace::PrimaryID::SMPTE170M,
      // opRGB, Colorimetry based on IEC 61966-2-5.
      gfx::ColorSpace::PrimaryID::SMPTE170M,
      // BT2020RGB. Colorimetry based on ITU-R BT.2020 R’G’B’.
      gfx::ColorSpace::PrimaryID::BT2020,
      // BT2020YCC. Colorimetry based on ITU-R BT.2020 Y’C’BC’R.
      gfx::ColorSpace::PrimaryID::BT2020,
      // BT2020cYCC. Colorimetry based on ITU-R BT.2020 Y’cC’BCC’RC.
      gfx::ColorSpace::PrimaryID::BT2020,
  };
  // See CEA 861.3-2015, "HDR Static Metadata Extensions" for these.
  constexpr uint8_t kHDRStaticMetadataCapabilityTag = 0x6;
  constexpr gfx::ColorSpace::TransferID kTransferIDMap[] = {
      gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::TransferID::GAMMA24,
      gfx::ColorSpace::TransferID::SMPTEST2084,
      // STD B67 is also known as Hybrid-log Gamma (HLG).
      gfx::ColorSpace::TransferID::ARIB_STD_B67,
  };

  if (edid.size() < kNumExtensionsOffset + 1) {
    LOG(ERROR) << "Too short EDID data: extensions";
    // TODO(mcasas): add UMA, https://crbug.com/821393.
    return;  // Any other fields below are beyond this edid offset.
  }
  const uint8_t num_extensions = edid[kNumExtensionsOffset];

  for (size_t i = 0; i < num_extensions; ++i) {
    // Skip parsing the whole extension if size is not enough.
    if (edid.size() < kExtensionBaseOffset + (i + 1) * kExtensionSize)
      break;

    const size_t extension_offset = kExtensionBaseOffset + i * kExtensionSize;
    const uint8_t cea_tag = edid[extension_offset];
    const uint8_t revision = edid[extension_offset + 1];
    if (cea_tag != kCEAExtensionTag || revision != kExpectedExtensionRevision)
      continue;

    const uint8_t timing_descriptors_start = std::min(
        edid[extension_offset + 2], static_cast<unsigned char>(kExtensionSize));

    for (size_t data_offset = extension_offset + kDataBlockOffset;
         data_offset < extension_offset + timing_descriptors_start;) {
      // A data block is encoded as:
      // - byte 1 high 3 bits: tag. '07' for extended tags.
      // - byte 1 remaining bits: the length of data block.
      // - byte 2: the extended tag. E.g. '0' for video capability. Values are
      //   defined by the k...CapabilityTag constants.
      // - byte 3: the capability.
      const uint8_t tag = edid[data_offset] >> 5;
      const uint8_t payload_length = edid[data_offset] & 0x1f;
      if (data_offset + payload_length + 1 > edid.size())
        break;

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
          constexpr size_t kMaxNumColorimetryEntries = 8;
          const std::bitset<kMaxNumColorimetryEntries>
              supported_primaries_bitfield(edid[data_offset + 2]);
          static_assert(
              kMaxNumColorimetryEntries == base::size(kPrimaryIDMap),
              "kPrimaryIDMap should describe all possible colorimetry entries");
          for (size_t i = 0; i < kMaxNumColorimetryEntries; ++i) {
            if (supported_primaries_bitfield[i])
              supported_color_primary_ids_.insert(kPrimaryIDMap[i]);
          }
          break;
        }

        case kHDRStaticMetadataCapabilityTag: {
          constexpr size_t kMaxNumHDRStaticMedatataEntries = 4;
          const std::bitset<kMaxNumHDRStaticMedatataEntries>
              supported_eotfs_bitfield(edid[data_offset + 2]);
          static_assert(
              kMaxNumHDRStaticMedatataEntries == base::size(kTransferIDMap),
              "kTransferIDMap should describe all possible transfer entries");
          for (size_t i = 0; i < kMaxNumHDRStaticMedatataEntries; ++i) {
            if (supported_eotfs_bitfield[i])
              supported_color_transfer_ids_.insert(kTransferIDMap[i]);
          }
          break;
        }
        default:
          break;
      }

      data_offset += payload_length + 1;
    }
  }
}

}  // namespace display
