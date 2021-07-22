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

// Reading OLE streams from input.
//
// A stream is a chunk of bytes that will later on be interpreted as
// an OLE2 object. We don't need to encapsulate this in an object and
// a typedef will add nothing. A non instantiable class is used to
// provide a scope to the methods defined to read OLE streams and
// decompress them.
//
// OLEStream::Read() and OLEStream:ReadDirectoryContent() are used
// once enough of the OLE2 file has been parsed and that we have, for
// instance, obtained a handler on a particular directory:
//
//   CHECK(OLEHeader::ParseHeader(input, &header));
//   CHECK(header.IsInitialized());
//   CHECK(FAT::Read(input, header, &fat));
//   CHECK(!fat.empty());
//   CHECK(OLEDirectoryEntry::ReadDirectory(input, header, fat, &root));
//   CHECK(root.IsInitialized());
//   CHECK(OLEStream::ReadDirectoryContent(
//     input, header,
//     *(root.FindVBAContentRoot()->
//       FindChildByName("vba", DirectoryStorageType::Storage)->
//       FindChildByName("dir", DirectoryStorageType::Stream)),
//     fat, &compressed));
//   CHECK(OLEStream::DecompressStream(compressed, &deflated);

#ifndef MALDOCA_OLE_STREAM_H_
#define MALDOCA_OLE_STREAM_H_

#include "absl/strings/string_view.h"
#include "maldoca/base/logging.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/header.h"

namespace maldoca {

class OLEStream {
 public:
  // Disallow copy and assign.
  OLEStream(const OLEStream&) = delete;
  OLEStream& operator=(const OLEStream&) = delete;

  // Read a string from input and specification. This method can be
  // invoked in two ways:
  //
  // - With the expected stream size unknown or when the expected
  //   stream size is above the Mini FAT stream size cutoff. In this
  //   case, we mandate that root_first_sector and root_stream_size
  //   both be set to -1.
  //
  // - When reading from a MiniStream. We mandate that both
  //   root_first_sector and root_stream_size be set to values greater
  //   than 0.
  //
  // TODO(somebody): This is sub-optimal. We should have two
  // invocations but it's too early to tell whether two separate
  // invocations with less parameters are really feasible.
  static bool Read(absl::string_view input, const OLEHeader& header,
                   uint32_t first_sector, uint32_t first_sector_offset,
                   uint32_t sector_size, uint32_t expected_stream_size,
                   uint32_t expected_stream_size_is_unknown_p,
                   int32_t root_first_sector, int32_t root_stream_size,
                   const std::vector<uint32_t>& fat, std::string* data);

  // Read the data stream attached to a given directory. Return true
  // in case of success.
  static bool ReadDirectoryContent(absl::string_view input,
                                   const OLEHeader& header,
                                   const OLEDirectoryEntry& dir,
                                   const std::vector<uint32_t>& fat,
                                   std::string* data);

  // Read the data stream attached to a given directory. Use 'root' to retrieve
  // the ministream, and then read the data stream from the ministream. This is
  // split from ReadDirectoryContent in order to read directory content of a
  // directory entry that was disconnected from the root (i.e. malformed entries
  // or orphans). Return true in case of success.
  static bool ReadDirectoryContentUsingRoot(absl::string_view input,
                                            const OLEHeader& header,
                                            const OLEDirectoryEntry& root,
                                            const OLEDirectoryEntry& dir,
                                            const std::vector<uint32_t>& fat,
                                            std::string* data);

  // Decompress an OLE stream (the compression algorithm used is RLE.)
  // Return true in case of success.
  static bool DecompressStream(absl::string_view input_string,
                               std::string* output);

 private:
  OLEStream() = delete;
  ~OLEStream() = delete;
};
}  // namespace maldoca

#endif  // MALDOCA_OLE_STREAM_H_
