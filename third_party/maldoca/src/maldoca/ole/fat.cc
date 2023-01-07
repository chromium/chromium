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

// Read a FAT from a sector. See fat.h for more details.

#include "maldoca/ole/fat.h"

#include "absl/strings/string_view.h"
#include "maldoca/ole/endian_reader.h"

namespace maldoca {

// This is the number of sector indexes that reading the FAT input
// should always yield: (512-76)/4.
static const uint32_t kFATSectorNumIndexes = 109;

static bool ReadSector(absl::string_view input, uint32_t sector_index,
                       uint32_t sector_size, absl::string_view *sector) {
  uint32_t offset = sector_size * (sector_index + 1);
  return FAT::ReadSectorAt(input, offset, sector_size,
                           /* allow_short_sector_read */ false, sector);
}

static void LoadFATFromIndexes(absl::string_view input,
                               const std::vector<uint32_t> &indexes,
                               uint32_t sector_size,
                               std::vector<uint32_t> *fat) {
  for (const auto sector_index : indexes) {
    // Stop when we encounter a sector index indicating an end of
    // chain or a free sector.
    if (sector_index == SectorConstant::EndOfChain ||
        sector_index == SectorConstant::Free) {
      return;
    }
    CHECK_GE(sector_index, 0);
    // Read the FAT sector, convert it to an array of indexes and add
    // that to the final result.
    absl::string_view read_sector;
    if (!ReadSector(input, sector_index, sector_size, &read_sector)) {
      return;
    }
    std::vector<uint32_t> read_fat;
    FAT::SectorToArrayOfIndexes(read_sector, &read_fat);
    fat->insert(fat->end(), read_fat.begin(), read_fat.end());
  }
}

static bool LoadDIFAT(absl::string_view input, const OLEHeader &header,
                      std::vector<uint32_t> *fat) {
  if (header.FATNumSector() <= kFATSectorNumIndexes) {
    DLOG(ERROR) << "No enough sectors to hold a DIFAT";
    return false;
  }
  if (header.DIFATFirstSector() >= header.TotalNumSector()) {
    DLOG(ERROR) << "First DIFAT sector index out of range";
    return false;
  }
  // Compute the index of the next DIFAT sector
  uint32_t next_difat_sector = static_cast<int>(header.SectorSize() / 4) - 1;
  // Re-compute the number of DIFAT sectors and compare it with what was
  // specified in the OLE header.
  uint32_t difat_num_sector =
      ((header.FATNumSector() - kFATSectorNumIndexes + next_difat_sector - 1) /
       next_difat_sector);
  if (difat_num_sector != header.DIFATNumSector()) {
    DLOG(ERROR) << "Can not verify the number of DIFAT sectors";
    return false;
  }
  uint32_t difat_sector_index = header.DIFATFirstSector();
  for (uint32_t index = 0; index < difat_num_sector; index += 1) {
    absl::string_view difat_sector_content;
    if (!ReadSector(input, difat_sector_index, header.SectorSize(),
                    &difat_sector_content)) {
      // ReadSector will have logged everything there's to log.
      return false;
    }
    std::vector<uint32_t> difat;
    FAT::SectorToArrayOfIndexes(difat_sector_content, &difat);
    std::vector<uint32_t> sub_difat(difat.begin(), difat.end() - 1);
    LoadFATFromIndexes(input, difat, header.SectorSize(), fat);
    difat_sector_index = difat[next_difat_sector];
  }
  if (difat_sector_index != SectorConstant::EndOfChain &&
      difat_sector_index != SectorConstant::Free) {
    DLOG(ERROR) << "DIFAT ending incorrectly: " << difat_sector_index;
    return false;
  }
  return true;
}

// static
void FAT::SectorToArrayOfIndexes(absl::string_view sector,
                                 std::vector<uint32_t> *array) {
  CHECK(array != nullptr);
  CHECK_EQ(array->size(), 0);

  uint32_t value;
  while (sector.size() >= sizeof(value)) {
    LittleEndianReader::ConsumeUInt32(&sector, &value);
    array->push_back(value);
  }
  // We should have reached the end of the sector.
  CHECK_EQ(sector.size(), 0);
}

// static
bool FAT::ReadSectorAt(absl::string_view input, uint32_t offset,
                       uint32_t sector_size, bool allow_short_sector_read,
                       absl::string_view *sector) {
  CHECK(sector != nullptr);
  if (offset > input.size()) {
    DLOG(ERROR) << "Can not read sector at offset " << offset;
    return false;
  }
  *sector = absl::ClippedSubstr(input, offset, sector_size);
  // Check that we have the expected sector size - only when we don't
  // allow a read to return less than the expected sector size.
  if (!allow_short_sector_read) {
    if (sector->size() != sector_size) {
      DLOG(ERROR) << "Failed to read " << sector_size << " bytes at offset "
                  << offset;
      return false;
    }
    // Check that we will still be able to read a given number of
    // elements of uint32_t size from the carved out data.
    if (sector->size() % 4) {
      DLOG(ERROR) << "Sector size " << sector->size() << " does not "
                  << "represent a round number of uint32.";
      return false;
    }
  }

  return true;
}

// static
bool FAT::Read(absl::string_view input, const OLEHeader &header,
               std::vector<uint32_t> *fat) {
  CHECK(header.IsInitialized());
  CHECK(fat->empty());

  // The FAT input starts 76 bytes past the beginning of the file
  // (which is also the header) and runs until the end of the header,
  // which spans one sector. We force the size check because depending
  // on the compilation mode, building a StringPiece from a substring
  // might not fail right away.
  // TODO(somebody): Should we be using the sector_size here
  // instead of 512?
  if (input.size() < 512) {
    DLOG(ERROR) << "Input size < 512 bytes. "
                << "Unconventional sector size in use?";
    return false;
  }
  absl::string_view fat_input = absl::ClippedSubstr(input, 76, 512 - 76);

  std::vector<uint32_t> indexes;
  SectorToArrayOfIndexes(fat_input, &indexes);
  // This array should always have exactly 109 entries (436/4 = 109).
  if (indexes.size() != kFATSectorNumIndexes) {
    DLOG(ERROR) << "Could not read the expected number of FAT sectors from "
                << "input. Expected 109, got: " << indexes.size();
    return false;
  }

  LoadFATFromIndexes(input, indexes, header.SectorSize(), fat);

  // If the file is larger that 6.8MB, a DIFAT is present and must be
  // handled. This is indicated by a positive number of DIFAT sectors.
  if (header.DIFATNumSector() && !LoadDIFAT(input, header, fat)) {
    DLOG(ERROR) << "Can not load DIFAT";
    return false;
  }

  // If we have found more FAT than the actual number of sectors in
  // the payload, keep only the relevant sector indexes (it can happen
  // because the FAT is read from fixed-size sectors.)
  if (fat->size() > header.TotalNumSector()) {
    DLOG(WARNING) << "FAT length is " << fat->size() << ", shrinking to "
                  << header.TotalNumSector();
    fat->resize(header.TotalNumSector());
  }
  return true;
}
}  // namespace maldoca
