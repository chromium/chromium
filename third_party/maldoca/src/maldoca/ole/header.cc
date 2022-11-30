// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implementation for the OLEHeader class. See the header.h file for details.

#include "maldoca/ole/header.h"

#include <iomanip>
#include <memory>

#include "absl/strings/string_view.h"
#include "maldoca/ole/endian_reader.h"

namespace maldoca {

static const uint64_t kOLEMagic =
    0xe11ab1a1e011cfd0L;  // Read as little endian.

static bool ParseHeaderInternal(absl::string_view header_input,
                                uint32_t input_length, OLEHeader *header) {
  if (header_input.size() != 76) {
    DLOG(ERROR) << "Can not extract a 76 bytes header, got size: "
                << header_input.size();
    return false;
  }

  uint64_t signature;
  if (!(LittleEndianReader::ConsumeUInt64(&header_input, &signature) &&
        signature == kOLEMagic)) {
    DLOG(ERROR) << "Bytes [0:7]: Incorrect signature: " << std::hex
                << std::setfill('0') << std::setw(16) << signature;
    return false;
  }

  uint64_t clsid1, clsid2;
  if (!(LittleEndianReader::ConsumeUInt64(&header_input, &clsid1) &&
        LittleEndianReader::ConsumeUInt64(&header_input, &clsid2) &&
        clsid1 == 0L && clsid2 == 0L)) {
    DLOG(ERROR) << "Bytes [8:23]: Incorrect CLSID values: " << std::hex
                << std::setfill('0') << std::setw(16) << clsid1 << ", "
                << clsid2 << " (they officialy MUST be zeroes.)";
    return false;
  }

  uint16_t minor_version;
  if (!LittleEndianReader::ConsumeUInt16(&header_input, &minor_version)) {
    DLOG(ERROR) << "Bytes [24:25]: Can not read minor version.";
    return false;
  }

  uint16_t major_version;
  if (!(LittleEndianReader::ConsumeUInt16(&header_input, &major_version) &&
        (major_version == 3 || major_version == 4))) {
    DLOG(ERROR) << "Bytes [26:27]: Unexpected major version: " << major_version;
    return false;
  }

  uint16_t byte_order;
  if (!(LittleEndianReader::ConsumeUInt16(&header_input, &byte_order) &&
        (byte_order == 0xfffe || byte_order == 0xfeff))) {
    DLOG(ERROR) << "Bytes [28:29]: Unexpected byte order: " << std::hex
                << std::setfill('0') << std::setw(4) << byte_order;
    return false;
  }
  if (byte_order == 0xfeff) {
    LOG(ERROR) << "Big endian document are not yet handled.";
    return false;
  }

  // We receive the sector size as its log2. After validation, we
  // compute the sector size and we verify that it matches
  // expectations (against the major_version we've already read.)
  uint16_t log2_sector_size;
  if (!(LittleEndianReader::ConsumeUInt16(&header_input, &log2_sector_size) &&
        (log2_sector_size >= 7 && log2_sector_size <= 12))) {
    DLOG(ERROR) << "Bytes [30:31]: Unexpected log2 sector size: "
                << log2_sector_size;
    return false;
  }
  int sector_size = 2 << (log2_sector_size - 1);
  // Major version 3 means sector size of 512, major version 4 means
  // sector size of 4096.
  if (!((major_version == 3 && sector_size == 512) ||
        (major_version == 4 && sector_size == 4096))) {
    DLOG(ERROR) << "Bytes [30:31]: Sector size not matching major version: "
                << major_version << " -vs- " << sector_size;
  }

  uint16_t log2_minifat_sector_size;
  if (!(LittleEndianReader::ConsumeUInt16(&header_input,
                                          &log2_minifat_sector_size) &&
        log2_minifat_sector_size == 6)) {
    DLOG(ERROR) << "Bytes [32:33]: Unexpected log2 short sector size: "
                << log2_minifat_sector_size;
    return false;
  }
  int minifat_sector_size = 2 << (log2_minifat_sector_size - 1);

  uint16_t reserved1;
  uint32_t reserved2;
  if (!(LittleEndianReader::ConsumeUInt16(&header_input, &reserved1) &&
        LittleEndianReader::ConsumeUInt32(&header_input, &reserved2) &&
        reserved1 == 0 && reserved2 == 0)) {
    DLOG(ERROR) << "Bytes [34:39]: Unexpected reserved values " << std::hex
                << std::setfill('0') << std::setw(4) << reserved1 << ", "
                << std::setw(8) << reserved2;
    return false;
  }

  uint32_t num_sector_sat;  // SAT: sector allocation table
  if (!(LittleEndianReader::ConsumeUInt32(&header_input, &num_sector_sat))) {
    DLOG(ERROR) << "Bytes [40:43]: Can not read SAT sector total";
    return false;
  }
  // num_sector_sat must be zero for 512-byte sectors (which means
  // major version 3)
  if (major_version == 3 && num_sector_sat != 0) {
    DLOG(ERROR) << "Bytes [40:43]: Num sector in SAT " << num_sector_sat
                << " incorrect for major version " << major_version;
    return false;
  }

  uint32_t num_sector_in_fat_chain;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &num_sector_in_fat_chain))) {
    DLOG(ERROR) << "Bytes [44:47]: Can not read the number of sector in the "
                << "FAT chain";
    return false;
  }

  uint32_t directory_first_sector;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &directory_first_sector))) {
    DLOG(ERROR) << "Bytes [48:51]: Can not read the number of the first "
                << "sector in directory chain";
    return false;
  }

  uint32_t transaction_signature;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &transaction_signature))) {
    DLOG(ERROR) << "Bytes [52:55]: Can not read transaction signature";
    return false;
  }
  if (transaction_signature != 0) {
    DLOG(ERROR) << "Transaction signature has a non zero value: " << std::hex
                << std::setfill('0') << std::setw(8) << transaction_signature;
  }

  uint32_t minifat_stream_cutoff;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &minifat_stream_cutoff)) &&
      minifat_stream_cutoff != 4096) {
    DLOG(ERROR) << "Bytes [56:59]: Minimum size of a standard size is "
                << minifat_stream_cutoff;
    return false;
  }

  uint32_t minifat_first_sector;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &minifat_first_sector))) {
    DLOG(ERROR) << "Bytes [60:63]: Can not read the first sector of the mini "
                << "FAT chain";
    return false;
  }

  uint32_t minifat_num_sector;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &minifat_num_sector))) {
    DLOG(ERROR) << "Bytes [64:67]: Can not read the number of sector in the "
                << "mini FAT chain";
    return false;
  }

  uint32_t difat_first_sector;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input,
                                          &difat_first_sector))) {
    DLOG(ERROR) << "Bytes [68:71]: Can not read the first sector of the "
                << "DIFAT chain";
    return false;
  }

  uint32_t difat_num_sector;
  if (!(LittleEndianReader::ConsumeUInt32(&header_input, &difat_num_sector))) {
    DLOG(ERROR) << "Bytes [72:75]: Can not read the number of sector in the "
                << "DIFAT chain";
    return false;
  }

  // At this point, we should have consumed all bytes in the header_input.
  if (!header_input.empty()) {
    DLOG(ERROR) << "Not all bytes from the header_input"
                << " have been consumed: " << header_input.size() << " left";
    return false;
  }

  // Compute the total number of sectors this file is made of. We
  // remove 1 to the intermediate result because the header is one
  // sector but doesn't count as effective file content.
  uint32_t total_num_sector =
      ((input_length + sector_size - 1) / sector_size) - 1;

  // Keep around and/or compute all values that are of interest and
  // return them in the header
  header->Initialize(total_num_sector, sector_size, num_sector_in_fat_chain,
                     minifat_first_sector, minifat_num_sector,
                     minifat_sector_size, minifat_stream_cutoff,
                     difat_first_sector, difat_num_sector,
                     directory_first_sector);

  return true;
}

void OLEHeader::Initialize(
    uint32_t total_num_sector, uint32_t sector_size, uint32_t fat_num_sector,
    uint32_t minifat_first_sector, uint32_t minifat_num_sector,
    uint32_t minifat_sector_size, uint32_t minifat_stream_cutoff,
    uint32_t difat_first_sector, uint32_t difat_num_sector,
    uint32_t directory_first_sector) {
  // Do not allow re-initialization.
  CHECK(!is_initialized_);

  total_num_sector_ = total_num_sector;
  sector_size_ = sector_size;
  fat_num_sector_ = fat_num_sector;
  difat_first_sector_ = difat_first_sector;
  difat_num_sector_ = difat_num_sector;
  directory_first_sector_ = directory_first_sector;
  minifat_first_sector_ = minifat_first_sector;
  minifat_num_sector_ = minifat_num_sector;
  minifat_sector_size_ = minifat_sector_size;
  minifat_stream_cutoff_ = minifat_stream_cutoff;

  is_initialized_ = true;
}

// static
bool OLEHeader::ParseHeader(absl::string_view input, OLEHeader *header) {
  // The header shouldn't have been initialized
  CHECK(!header->IsInitialized());

  // The header input is made of the first 76 bytes of the input. We
  // will consume that input and we should end with nothing left at
  // the end of this method.
  return ParseHeaderInternal(input.substr(0, 76), input.size(), header);
}

void OLEHeader::Log() {
  LOG(INFO) << "is_initialized=" << is_initialized_
            << " total_num_sector=" << total_num_sector_
            << " sector_size=" << sector_size_
            << " difat_num_sector=" << difat_num_sector_
            << " directory_first_sector=" << directory_first_sector_
            << " minifat_first_sector=" << minifat_first_sector_
            << " minifat_num_sector=" << minifat_num_sector_
            << " minifat_sector_size=" << minifat_sector_size_
            << " minifat_stream_cutoff=" << minifat_stream_cutoff_;
}
}  // namespace maldoca
