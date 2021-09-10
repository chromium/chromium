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

// Read a FAT from input.
//
// The FAT is just a list of sector indexes - there is no need for an
// object to hold that results and a typedef will add nothing - so a
// FAT is just kept as a vector of uint32. A non instantiable class is
// used to provide a scope to the methods defined to read FATs.
//
// Expected usage for that code:
//
//   OLEHeader header;
//   CHECK(OLEHeader::ParseHeader(input, &header));
//   CHECK(header.IsInitialized());
//   std::vector<uint32_t> fat;
//   CHECK(FAT::Read(input, header, &fat));
//   CHECK(!fat.empty());
//
// TODO(somebody): Dumping FAT to string.

#ifndef MALDOCA_OLE_FAT_H_
#define MALDOCA_OLE_FAT_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "maldoca/ole/header.h"

namespace maldoca {

typedef enum : uint32_t {
  Free = 0xFFFFFFFF,            // (-1) unallocated sector
  EndOfChain = 0xFFFFFFFE,      // (-2) End of stream marker
  FATSectorInFAT = 0xFFFFFFFD,  // (-3) FAT sector in a FAT
  DIFATInFAT = 0xFFFFFFFC,      // (-4) DIFAT sector in FAT
  Maximum = 0xFFFFFFFA,         // (-6) maximum SECT
} SectorConstant;

class FAT {
 public:
  // Disallow copy and assign.
  FAT(const FAT&) = delete;
  FAT& operator=(const FAT&) = delete;
  // Consume all the uint32_t in a string piece that represents a sector
  // and store them into an array.
  static void SectorToArrayOfIndexes(absl::string_view sector,
                                     std::vector<uint32_t> *array);

  // Carve out a sector from input from a give offset and sector size
  // and return that data as a new string piece. Allow the read to
  // conditionally fall short of the number of bytes that were specified
  // to be read.
  static bool ReadSectorAt(absl::string_view input, uint32_t offset,
                           uint32_t sector_size, bool allow_short_sector_read,
                           absl::string_view *sector);

  // Read a fat from input - the input is expected to be the entire
  // stream from which the FAT will be extracted after the space
  // allocated for the header. This requires that the header has already
  // been parsed and the results placed in the header parameter. This
  // function returns true upon success and the FAT will be available as
  // uint32_t values in the fat parameter.
  static bool Read(absl::string_view input, const OLEHeader &header,
                   std::vector<uint32_t> *fat);

 private:
  FAT() = delete;
  ~FAT() = delete;
};

}  // namespace maldoca

#endif  // MALDOCA_OLE_FAT_H_
