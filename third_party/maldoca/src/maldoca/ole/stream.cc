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

// Reading OLE2 streams. See stream.h for more details.

#include "maldoca/ole/stream.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/digest.h"
#include "maldoca/ole/endian_reader.h"
#include "maldoca/ole/fat.h"
#include "maldoca/ole/oss_utils.h"

namespace maldoca {

// Read a stream from input: starting from a given sector, at a given
// sector offset and for a given sector size. If the size of the
// output isn't known, it is estimated before reading occurs and if
// necessary the output is resized.
static bool ReadStreamInternal(absl::string_view input, uint32_t sector,
                               uint32_t sector_offset, uint32_t sector_size,
                               uint32_t size, bool size_is_unknown_p,
                               const std::vector<uint32_t>& fat,
                               std::string* data) {
  DLOG(INFO) << "input len=" << input.size() << " sector=" << sector
             << " sector_offset=" << sector_offset
             << " sector_size=" << sector_size << " size=" << size
             << " size_is_unknown_p=" << size_is_unknown_p;

  CHECK(data->empty());
  // If it's been specified that the size of the stream to read is
  // unknown, attempt to compute it now using the size of the FAT.
  if (size_is_unknown_p) {
    size = fat.size() * sector_size;
  }

  uint32_t num_sectors = (size + sector_size - 1) / sector_size;
  if (num_sectors > fat.size()) {
    DLOG(ERROR) << "Computed number of sector is bigger than the FAT size.";
    return false;
  }

  // If the size is 0, the first sector index should be a end of
  // stream marker.
  if (size == 0 && sector != SectorConstant::EndOfChain) {
    DLOG(ERROR) << "Sector index is " << sector << " for empty stream";
    return false;
  }

  // We are now going over the number of sectors we have to read from
  for (int current_sector_num = 0; current_sector_num < num_sectors;
       current_sector_num += 1) {
    // Always consider the sector at hand and check first whether
    // we've reached the end of the stream.
    if (sector == SectorConstant::EndOfChain) {
      if (size_is_unknown_p) {
        break;
        // If the size was known, we will end here when the stream is
        // smaller than anticipated.
      } else {
        DLOG(ERROR) << "Sector is " << sector
                    << " before the expected end of the stream";
        return false;
      }
    }
    if (sector >= SectorConstant::Free || sector > fat.size()) {
      DLOG(ERROR) << "Sector index " << sector << " out of range "
                  << fat.size();
      return false;
    }
    // Now that we have validated the sector index, read its content.
    absl::string_view sector_data;
    // Allow short reads when we're at the end of the file (the last
    // sector might not be complete)
    bool allow_short_sector_read = (sector == fat.size() - 1);
    uint32_t offset = sector_offset + sector_size * sector;
    if (!FAT::ReadSectorAt(input, offset, sector_size, allow_short_sector_read,
                           &sector_data)) {
      DLOG(ERROR) << "Can not read sector at offset=" << offset
                  << " sector_size=" << sector_size
                  << " allow_short_sector_read=" << allow_short_sector_read;
      return false;
    }
    absl::StrAppend(data, sector_data);
    // This is the indirect jump to the next sector
    if (sector >= fat.size()) {
      DLOG(ERROR) << "Can not read next sector index " << sector
                  << "from FAT of size " << fat.size();
      return false;
    }
    sector = fat[sector];
  }
  // The last sector we handled should be the end of stream marker
  if (sector != SectorConstant::EndOfChain) {
    DLOG(ERROR) << "Last handled sector index has unexpected value " << sector
                << " (" << SectorConstant::EndOfChain << " expected)";
    return false;
  }
  // >= is used here to avoid getting true in the else branch.
  if (data->size() >= size) {
    data->resize(size);
    // If the size is known, we've read less than expected
  } else if (!size_is_unknown_p) {
    DLOG(ERROR) << data->size() << " bytes read when " << size
                << " were expected";
    return false;
  }
  DLOG(INFO) << "md5=" << Md5HexString(*data);
  return true;
}

// Load a MiniFAT from input, given a root directory stream size.
static bool LoadMiniFAT(absl::string_view input, const OLEHeader& header,
                        uint32_t root_stream_size,
                        const std::vector<uint32_t>& fat,
                        std::vector<uint32_t>* minifat) {
  CHECK(header.IsInitialized());
  uint32_t stream_size = header.MiniFATNumSector() * header.SectorSize();

  uint32_t num_minisectors =
      ((root_stream_size + header.MiniFATSectorSize() - 1) /
       header.MiniFATSectorSize());
  uint32_t used_size = num_minisectors * sizeof(uint32_t);
  // If the ministream is larger than the MiniFat, we just signal the
  // issue but parsing can carry on.
  if (used_size > stream_size) {
    DLOG(WARNING) << "OLE ministream is larger than MiniFAT: " << used_size
                  << " -vs- " << stream_size;
  }

  // First read the stream in its entirety and convert it to FAT.
  std::string data;
  if (!ReadStreamInternal(input, header.MiniFATFirstSector(),
                          header.SectorSize(), header.SectorSize(), stream_size,
                          false, fat, &data)) {
    DLOG(ERROR) << "Error reading stream from input:"
                << " sector=" << header.MiniFATFirstSector()
                << " sector_offset=" << header.SectorSize()
                << " sector_size=" << header.SectorSize()
                << " stream_size=" << stream_size;
    return false;
  }
  FAT::SectorToArrayOfIndexes(absl::string_view(data), minifat);
  // Then shrink its to the expected size to avoid indexes out of the
  // ministream.
  minifat->resize(num_minisectors);
  return true;
}

// static
bool OLEStream::Read(absl::string_view input, const OLEHeader& header,
                     uint32_t first_sector, uint32_t first_sector_offset,
                     uint32_t sector_size, uint32_t expected_stream_size,
                     uint32_t expected_stream_size_is_unknown_p,
                     int32_t root_first_sector, int32_t root_stream_size,
                     const std::vector<uint32_t>& fat, std::string* data) {
  CHECK(header.IsInitialized());
  CHECK(!fat.empty());

  // This section of the read operation doesn't require that we
  // have the root_stream_size and root_first_sector set. To avoid
  // code misuse, we enforce both to be set to -1.
  if (expected_stream_size_is_unknown_p ||
      expected_stream_size >= header.MiniFATStreamCutoff()) {
    CHECK_EQ(root_first_sector, -1)
        << "For this invocation, root_first_sector should be -1: "
        << "root_first_sector=" << root_first_sector;
    CHECK_EQ(root_stream_size, -1)
        << "For this invocation, root_stream_size should be -1: "
        << "root_stream_size=" << root_stream_size;
    return ReadStreamInternal(input, first_sector, first_sector_offset,
                              sector_size, expected_stream_size,
                              expected_stream_size_is_unknown_p, fat, data);
  }
  // We're reading from a mini stream. Load the mini FAT first.
  // First enforce expected parameter values.
  CHECK_GE(root_stream_size, 0)
      << "For this invocation, root_stream_size should be >= 0: "
      << "root_stream_size=" << root_stream_size;
  // root_first_sector might be -2 (EndOfChain)  and it is still valid OLE2
  // document
  if (root_first_sector != SectorConstant::EndOfChain) {
    CHECK_GE(root_first_sector, 0)
        << "For this invocation, root_first_sector should be >= 0: "
        << "root_first_sector=" << root_first_sector;
  }
  std::vector<uint32_t> mini_fat;
  if (!LoadMiniFAT(input, header, root_stream_size, fat, &mini_fat)) {
    DLOG(ERROR) << "Can't read MiniFAT from input";
    return false;
  }
  // From the current FAT, read the mini stream
  std::string mini_stream;
  if (!ReadStreamInternal(input, root_first_sector, header.SectorSize(),
                          header.SectorSize(), root_stream_size, false, fat,
                          &mini_stream)) {
    DLOG(ERROR) << "Error reading stream from input:"
                << " sector=" << root_first_sector
                << " sector_offset=" << header.SectorSize()
                << " sector_size=" << header.SectorSize()
                << " stream_size=" << root_stream_size;
    return false;
  }
  // And read the stream that exists in the mini stream and is
  // structured by the mini FAT.
  if (!ReadStreamInternal(absl::string_view(mini_stream), first_sector, 0,
                          header.MiniFATSectorSize(), expected_stream_size,
                          false, mini_fat, data)) {
    DLOG(ERROR) << "Error reading stream from input:"
                << " sector=" << first_sector << " sector_offset=" << 0
                << " sector_size=" << header.MiniFATSectorSize()
                << " stream_size=" << expected_stream_size;
    return false;
  }
  return true;
}

bool OLEStream::ReadDirectoryContent(absl::string_view input,
                                     const OLEHeader& header,
                                     const OLEDirectoryEntry& dir,
                                     const std::vector<uint32_t>& fat,
                                     std::string* data) {
  // Find the directory root node - we're going to use its first
  // sector and size for that invocation of Read()
  OLEDirectoryEntry* root = dir.FindRoot();
  CHECK(root != nullptr);
  return ReadDirectoryContentUsingRoot(input, header, *root, dir, fat, data);
}

bool OLEStream::ReadDirectoryContentUsingRoot(absl::string_view input,
                                              const OLEHeader& header,
                                              const OLEDirectoryEntry& root,
                                              const OLEDirectoryEntry& dir,
                                              const std::vector<uint32_t>& fat,
                                              std::string* data) {
  CHECK(header.IsInitialized());
  CHECK(dir.IsInitialized());
  CHECK(!fat.empty());
  // Assume that we can't read a stream from a node that isn't of type
  // stream.
  if (dir.EntryType() != DirectoryStorageType::Stream) {
    DLOG(ERROR) << "Directory is of type " << dir.EntryType()
                << ", type DirectoryStorageType::Stream was expected";
    return false;
  }

  // Do not provide valid values for the root first sector and stream
  // size if we can determine that we are above the threshold that
  // would otherwise make us read from the MiniFAT.
  int32_t root_first_sector = root.StreamFirstSectorIndex();
  int32_t root_stream_size = root.StreamSize();
  if (dir.StreamSize() >= header.MiniFATStreamCutoff()) {
    root_first_sector = -1;
    root_stream_size = -1;
  }
  Read(input, header, dir.StreamFirstSectorIndex(), header.SectorSize(),
       header.SectorSize(), dir.StreamSize(), false, root_first_sector,
       root_stream_size, fat, data);
  return true;
}

// Helper function to issue error messages prefix refering to a
// position in the stream and a size. The prefix mentions a byte
// range.
static std::string ByteRangeMessage(uint32_t position, uint32_t size) {
  if (size > 1) {
    return absl::StrFormat("Byte [%d:%d]: ", static_cast<int>(position),
                           static_cast<int>(position + size - 1));
  }
  return absl::StrFormat("Byte [%d]: ", static_cast<int>(position));
}

// static
bool OLEStream::DecompressStream(absl::string_view input_string,
                                 std::string* output) {
  CHECK(!input_string.empty());
  CHECK(output->empty());
  absl::string_view input = input_string;

  // Pointer on the current data
  uint32_t current = 0;
  uint8_t sig_byte;
  if (!(LittleEndianReader::LoadUInt8At(input, current, &sig_byte) &&
        sig_byte == 0x01)) {
    DLOG(ERROR) << ByteRangeMessage(current, sizeof(sig_byte))
                << "Can not read signature byte with value 0x01";
    return false;
  }

  current += sizeof(uint8_t);
  while (current < input.size()) {
    uint16_t header;
    if (!LittleEndianReader::LoadUInt16At(input, current, &header)) {
      DLOG(ERROR) << ByteRangeMessage(current, sizeof(header))
                  << "can not read header";
      return false;
    }
    // The chunks size is computed from the header, adding the three
    // bytes we already read.
    uint16_t chunk_size = 3 + (header & 0x0fff);
    uint16_t chunk_signature = (header >> 12) & 0x07;
    if (chunk_signature != 0x03) {
      DLOG(ERROR) << ByteRangeMessage(current, sizeof(header))
                  << "unexpected chunk_signature value: " << chunk_signature;
      return false;
    }

    uint16_t chunk_flag = (header >> 15) & 0x01;
    if (chunk_flag == 1 && chunk_size > 4098) {
      DLOG(ERROR) << ByteRangeMessage(current, sizeof(header))
                  << "unexpected chunk_size for a chunk_flag of 1: "
                  << chunk_size;
      return false;
    }
    if (chunk_flag == 0 && chunk_size != 4098) {
      DLOG(ERROR) << ByteRangeMessage(current, sizeof(header))
                  << "unexpected chunk_size for a chunk_flag of 0: "
                  << chunk_size;
      return false;
    }

    if (current + chunk_size > input.size()) {
      DLOG(ERROR) << ByteRangeMessage(current, sizeof(header))
                  << "chunk_size addresses data past the input size: "
                  << "end=" << current + chunk_size
                  << ", input size=" << input.size();
      return false;
    }
    uint32_t compressed_end =
        std::min({static_cast<uint32_t>(input.size()), current + chunk_size});

    current += sizeof(uint16_t);

    if (chunk_flag == 0) {
      *output =
          std::string(absl::ClippedSubstr(input, current, current + 4096));
      current += 4096;
    } else {
      // We need to keep around the current size of the output from now on.
      uint32_t decompressed_chunk_start = output->length();
      while (current < compressed_end) {
        uint8_t flag;
        if (!(LittleEndianReader::LoadUInt8At(input, current, &flag))) {
          DLOG(ERROR) << ByteRangeMessage(current, sizeof(flag))
                      << "can not read flag value";
          return false;
        }
        current += sizeof(flag);
        for (uint32_t bit_index = 0; bit_index < 8 * sizeof(uint8_t); bit_index++) {
          if (current >= compressed_end) {
            break;
          }
          bool bit_at_index = static_cast<bool>((1u << bit_index) & flag);
          // If the bit at index is 0, the next byte is just added to
          // the output.
          if (!bit_at_index) {
            uint8_t byte_to_copy;
            if (!(LittleEndianReader::LoadUInt8At(input, current,
                                                  &byte_to_copy))) {
              DLOG(ERROR) << ByteRangeMessage(current, sizeof(byte_to_copy))
                          << "can not read byte to copy";
              return false;
            }
            output->push_back(byte_to_copy);
            current += sizeof(byte_to_copy);
            // Read the next two bytes and use it to compute how much of
            // the data that has been previously produced must be added
            // again to the output
          } else {
            uint16_t short_to_copy;
            if (!(LittleEndianReader::LoadUInt16At(input, current,
                                                   &short_to_copy))) {
              DLOG(ERROR) << ByteRangeMessage(current, sizeof(short_to_copy))
                          << "can not read short to copy";
              return false;
            }
            uint32_t difference = output->length() - decompressed_chunk_start;
            uint32_t different_bit_count =
                std::max(utils::Log2Ceiling(difference), 4);
            uint16_t length_mask = 0xffff >> different_bit_count;
            uint16_t offset_mask = ~length_mask;
            uint16_t length = (short_to_copy & length_mask) + 3;
            uint16_t offset =
                ((short_to_copy & offset_mask) >> (16 - different_bit_count)) +
                1;
            if (offset > output->length()) {
              DLOG(ERROR) << "Invalid back offset " << offset
                          << ", current length: " << output->length();
              return false;
            }
            uint32_t copy_source = output->length() - offset;
            uint32_t copy_source_stop = copy_source + length;
            while (copy_source < copy_source_stop) {
              output->push_back(output->at(copy_source));
              copy_source += 1;
            }
            current += sizeof(uint16_t);
          }
        }
      }
    }
  }
  return true;
}
}  // namespace maldoca
