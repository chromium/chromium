/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Parse and verify a OLE2 document header, also known as the Compound
// File Header.
//
// This implementation takes the route of carefully reading the header
// bytes by bytes, reporting errors or inconsistencies as they are
// found and eventually completing the initialization the instance of
// an OLEHeader object.
//
// That object keeps only the information that is required for the
// rest of the OLE related operations to happen (reading the FAT,
// opening directory streams, etc...)
//
// A class static method is used to parse the OLE2 header in input and
// complete the initialization of an OLEHeader object as it is
// not recommended to perform, in constructors, work that can fail.
//
// TODO(somebody): Check for the duplication of streams in FAT and
// Mini FAT.
//
// Typical usage:
//
//   OLEHeader header;
//   CHECK(!header.IsInitialized());
//   CHECK(OLEHeader::ParseHeader(input, &header));
//   CHECK(header.IsInitialized());

#ifndef MALDOCA_OLE_HEADER_H_
#define MALDOCA_OLE_HEADER_H_

#include "absl/strings/string_view.h"

namespace maldoca {

class OLEHeader {
 public:
  // Parse an OLEHeader from input, filling header as it goes. Return
  // true on success.
  static bool ParseHeader(absl::string_view input, OLEHeader* header);

  OLEHeader()
      : is_initialized_(false),
        total_num_sector_(0),
        sector_size_(0),
        fat_num_sector_(0),
        directory_first_sector_(0),
        difat_first_sector_(0),
        difat_num_sector_(0),
        minifat_first_sector_(0),
        minifat_num_sector_(0),
        minifat_sector_size_(0),
        minifat_stream_cutoff_(0) {}
  virtual ~OLEHeader() {}

  // Disallow copy and assign.
  OLEHeader(const OLEHeader&) = delete;
  OLEHeader& operator=(const OLEHeader&) = delete;

  // Store collected values and mark the instance as fully initialized.
  void Initialize(uint32_t total_num_sector, uint32_t sector_size,
                  uint32_t fat_num_sector, uint32_t minifat_first_sector,
                  uint32_t minifat_num_sector, uint32_t minifat_sector_size,
                  uint32_t minifat_stream_cutoff, uint32_t difat_first_sector,
                  uint32_t difat_num_sector, uint32_t directory_first_sector);

  // For debugging purposes: write the content of an instance in one
  // log line.
  void Log();

  // Getters
  bool IsInitialized() const { return is_initialized_; }
  uint32_t TotalNumSector() const { return total_num_sector_; }
  uint32_t SectorSize() const { return sector_size_; }

  int32_t FATNumSector() const { return fat_num_sector_; }

  uint32_t DirectoryFirstSector() const { return directory_first_sector_; }

  uint32_t DIFATFirstSector() const { return difat_first_sector_; }
  uint32_t DIFATNumSector() const { return difat_num_sector_; }

  uint32_t MiniFATFirstSector() const { return minifat_first_sector_; }
  uint32_t MiniFATNumSector() const { return minifat_num_sector_; }
  uint32_t MiniFATSectorSize() const { return minifat_sector_size_; }
  uint32_t MiniFATStreamCutoff() const { return minifat_stream_cutoff_; }

 private:
  // Global properties
  bool is_initialized_;
  uint32_t total_num_sector_;
  uint32_t sector_size_;

  // FAT properties
  uint32_t fat_num_sector_;

  // Directory properties
  uint32_t directory_first_sector_;

  // DIFAT properties
  uint32_t difat_first_sector_;
  uint32_t difat_num_sector_;

  // Minifat properties
  uint32_t minifat_first_sector_;
  uint32_t minifat_num_sector_;
  uint32_t minifat_sector_size_;
  uint32_t minifat_stream_cutoff_;
};
}  // namespace maldoca

#endif  // MALDOCA_OLE_HEADER_H_
