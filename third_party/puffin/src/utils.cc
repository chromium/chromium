// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/utils.h"

#include <inttypes.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "puffin/file_stream.h"
#include "puffin/memory_stream.h"
#include "puffin/src/bit_reader.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/logging.h"
#include "puffin/src/puff_writer.h"

using std::set;
using std::string;
using std::vector;

namespace {
// Use memcpy to access the unaligned data of type |T|.
template <typename T>
inline T get_unaligned(const void* address) {
  T result;
  memcpy(&result, address, sizeof(T));
  return result;
}

struct ExtentData {
  puffin::BitExtent extent;
  uint64_t byte_offset;
  uint64_t byte_length;
  const puffin::Buffer& data;

  ExtentData(const puffin::BitExtent& in_extent, const puffin::Buffer& in_data)
      : extent(in_extent), data(in_data) {
    // Round start offset up and end offset down to exclude bits not in this
    // extent. We simply ignore the bits at start and end that's not on byte
    // boundary because as long as the majority of the bytes are the same,
    // bsdiff will be able to reference it.
    byte_offset = (extent.offset + 7) / 8;
    uint64_t byte_end_offset = (extent.offset + extent.length) / 8;
    CHECK(byte_end_offset <= data.size());
    if (byte_end_offset > byte_offset) {
      byte_length = byte_end_offset - byte_offset;
    } else {
      byte_length = 0;
    }
  }

  int Compare(const ExtentData& other) const {
    if (extent.length != other.extent.length) {
      return extent.length < other.extent.length ? -1 : 1;
    }
    return memcmp(data.data() + byte_offset,
                  other.data.data() + other.byte_offset,
                  std::min(byte_length, other.byte_length));
  }
  bool operator<(const ExtentData& other) const { return Compare(other) < 0; }
  bool operator==(const ExtentData& other) const { return Compare(other) == 0; }
};

}  // namespace

namespace puffin {

bool LocateDeflatesInDeflateStream(const uint8_t* data,
                                   uint64_t size,
                                   uint64_t virtual_offset,
                                   vector<BitExtent>* deflates,
                                   uint64_t* compressed_size) {
  Puffer puffer;
  BufferBitReader bit_reader(data, size);
  BufferPuffWriter puff_writer(nullptr, 0);
  vector<BitExtent> sub_deflates;
  TEST_AND_RETURN_FALSE(
      puffer.PuffDeflate(&bit_reader, &puff_writer, &sub_deflates));
  for (const auto& deflate : sub_deflates) {
    deflates->emplace_back(deflate.offset + virtual_offset * 8, deflate.length);
  }
  if (compressed_size) {
    *compressed_size = bit_reader.Offset();
  }
  return true;
}

// This function uses RFC1950 (https://www.ietf.org/rfc/rfc1950.txt) for the
// definition of a zlib stream.  For finding the deflate blocks, we relying on
// the proper size of the zlib stream in |data|. Basically the size of the zlib
// stream should be known before hand. Otherwise we need to parse the stream and
// find the location of compressed blocks using CalculateSizeOfDeflateBlock().
bool LocateDeflatesInZlib(const Buffer& data, vector<BitExtent>* deflates) {
  // A zlib stream has the following format:
  // 0           1     compression method and flag
  // 1           1     flag
  // 2           4     preset dictionary (optional)
  // 2 or 6      n     compressed data
  // n+(2 or 6)  4     Adler-32 checksum
  TEST_AND_RETURN_FALSE(data.size() >= 6 + 4);  // Header + Footer
  uint16_t cmf = data[0];
  auto compression_method = cmf & 0x0F;
  // For deflate compression_method should be 8.
  TEST_AND_RETURN_FALSE(compression_method == 8);

  auto cinfo = (cmf & 0xF0) >> 4;
  // Value greater than 7 is not allowed in deflate.
  TEST_AND_RETURN_FALSE(cinfo <= 7);

  auto flag = data[1];
  TEST_AND_RETURN_FALSE(((cmf << 8) + flag) % 31 == 0);

  uint64_t header_len = 2;
  if (flag & 0x20) {
    header_len += 4;  // 4 bytes for the preset dictionary.
  }

  // 4 is for ADLER32.
  TEST_AND_RETURN_FALSE(LocateDeflatesInDeflateStream(
      data.data() + header_len, data.size() - header_len - 4, header_len,
      deflates, nullptr));
  return true;
}

bool FindDeflateSubBlocks(const UniqueStreamPtr& src,
                          const vector<ByteExtent>& deflates,
                          vector<BitExtent>* subblock_deflates) {
  Puffer puffer;
  Buffer deflate_buffer;
  for (const auto& deflate : deflates) {
    TEST_AND_RETURN_FALSE(src->Seek(deflate.offset));
    // Read from src into deflate_buffer.
    deflate_buffer.resize(deflate.length);
    TEST_AND_RETURN_FALSE(src->Read(deflate_buffer.data(), deflate.length));

    // Find all the subblocks.
    BufferBitReader bit_reader(deflate_buffer.data(), deflate.length);
    // The uncompressed blocks will be ignored since we are passing a null
    // buffered puff writer and a valid deflate locations output array. This
    // should not happen in the puffdiff or anywhere else by default.
    BufferPuffWriter puff_writer(nullptr, 0);
    vector<BitExtent> subblocks;
    TEST_AND_RETURN_FALSE(
        puffer.PuffDeflate(&bit_reader, &puff_writer, &subblocks));
    TEST_AND_RETURN_FALSE(deflate.length == bit_reader.Offset());
    for (const auto& subblock : subblocks) {
      subblock_deflates->emplace_back(subblock.offset + deflate.offset * 8,
                                      subblock.length);
    }
  }
  return true;
}

bool LocateDeflatesInZlibBlocks(const string& file_path,
                                const vector<ByteExtent>& zlibs,
                                vector<BitExtent>* deflates) {
  auto src = FileStream::Open(file_path, true, false);
  TEST_AND_RETURN_FALSE(src);

  Buffer buffer;
  for (const auto& zlib : zlibs) {
    buffer.resize(zlib.length);
    TEST_AND_RETURN_FALSE(src->Seek(zlib.offset));
    TEST_AND_RETURN_FALSE(src->Read(buffer.data(), buffer.size()));
    vector<BitExtent> tmp_deflates;
    TEST_AND_RETURN_FALSE(LocateDeflatesInZlib(buffer, &tmp_deflates));
    for (const auto& deflate : tmp_deflates) {
      deflates->emplace_back(deflate.offset + zlib.offset * 8, deflate.length);
    }
  }
  return true;
}

namespace {
// For more information about gzip format, refer to RFC 1952 located at:
// https://www.ietf.org/rfc/rfc1952.txt
bool IsValidGzipHeader(const uint8_t* header, size_t size) {
  // Each gzip entry has the following format magic header:
  // 0      1     0x1F
  // 1      1     0x8B
  // 2      1     compression method (8 denotes deflate)
  static constexpr uint8_t magic[] = {0x1F, 0x8B, 8};
  return size >= 10 && std::equal(std::begin(magic), std::end(magic), header);
}
}  // namespace

bool LocateDeflatesInGzip(const Buffer& data, vector<BitExtent>* deflates) {
  TEST_AND_RETURN_FALSE(IsValidGzipHeader(data.data(), data.size()));
  uint64_t member_start = 0;
  do {
    // After the magic header, the gzip contains:
    // 3      1     set of flags
    // 4      4     modification time
    // 8      1     extra flags
    // 9      1     operating system

    uint64_t offset = member_start + 10;
    int flag = data[member_start + 3];
    // Extra field
    if (flag & 4) {
      TEST_AND_RETURN_FALSE(offset + 2 <= data.size());
      uint16_t extra_length = data[offset++];
      extra_length |= static_cast<uint16_t>(data[offset++]) << 8;
      TEST_AND_RETURN_FALSE(offset + extra_length <= data.size());
      offset += extra_length;
    }
    // File name field
    if (flag & 8) {
      while (true) {
        TEST_AND_RETURN_FALSE(offset + 1 <= data.size());
        if (data[offset++] == 0) {
          break;
        }
      }
    }
    // File comment field
    if (flag & 16) {
      while (true) {
        TEST_AND_RETURN_FALSE(offset + 1 <= data.size());
        if (data[offset++] == 0) {
          break;
        }
      }
    }
    // CRC16 field
    if (flag & 2) {
      offset += 2;
    }

    uint64_t compressed_size = 0;
    TEST_AND_RETURN_FALSE(LocateDeflatesInDeflateStream(
        data.data() + offset, data.size() - offset, offset, deflates,
        &compressed_size));
    offset += compressed_size;

    // Ignore CRC32 and uncompressed size.
    offset += 8;
    member_start = offset;
  } while (member_start < data.size() &&
           IsValidGzipHeader(&data[member_start], data.size() - member_start));
  return true;
}

// For more information about the zip format, refer to
// https://support.pkware.com/display/PKZIP/APPNOTE
bool LocateDeflatesInZipArchive(const Buffer& data,
                                vector<BitExtent>* deflates) {
  uint64_t pos = 0;
  while (pos + 30 <= data.size()) {
    // TODO(xunchang) add support for big endian system when searching for
    // magic numbers.
    if (get_unaligned<uint32_t>(data.data() + pos) != 0x04034b50) {
      pos++;
      continue;
    }

    // local file header format
    // 0      4     0x04034b50
    // 4      2     minimum version needed to extract
    // 6      2     general purpose bit flag
    // 8      2     compression method
    // 10     4     file last modification date & time
    // 14     4     CRC-32
    // 18     4     compressed size
    // 22     4     uncompressed size
    // 26     2     file name length
    // 28     2     extra field length
    // 30     n     file name
    // 30+n   m     extra field
    auto compression_method = get_unaligned<uint16_t>(data.data() + pos + 8);
    if (compression_method != 8) {  // non-deflate type
      pos += 4;
      continue;
    }

    auto compressed_size = get_unaligned<uint32_t>(data.data() + pos + 18);
    auto file_name_length = get_unaligned<uint16_t>(data.data() + pos + 26);
    auto extra_field_length = get_unaligned<uint16_t>(data.data() + pos + 28);
    uint64_t header_size = 30 + file_name_length + extra_field_length;

    // sanity check
    if (static_cast<uint64_t>(header_size) + compressed_size > data.size() ||
        pos > data.size() - header_size - compressed_size) {
      pos += 4;
      continue;
    }

    vector<BitExtent> tmp_deflates;
    uint64_t offset = pos + header_size;
    uint64_t calculated_compressed_size = 0;
    if (!LocateDeflatesInDeflateStream(
            data.data() + offset, data.size() - offset, offset, &tmp_deflates,
            &calculated_compressed_size)) {
      pos += 4;
      continue;
    }

    deflates->insert(deflates->end(), tmp_deflates.begin(), tmp_deflates.end());
    pos += header_size + calculated_compressed_size;
  }

  return true;
}

bool FindPuffLocations(const UniqueStreamPtr& src,
                       const vector<BitExtent>& deflates,
                       vector<ByteExtent>* puffs,
                       uint64_t* out_puff_size) {
  Puffer puffer;
  Buffer deflate_buffer;

  // Here accumulate the size difference between each corresponding deflate and
  // puff. At the end we add this cummulative size difference to the size of the
  // deflate stream to get the size of the puff stream. We use signed size
  // because puff size could be smaller than deflate size.
  int64_t total_size_difference = 0;
  for (auto deflate = deflates.begin(); deflate != deflates.end(); ++deflate) {
    // Read from src into deflate_buffer.
    auto start_byte = deflate->offset / 8;
    auto end_byte = (deflate->offset + deflate->length + 7) / 8;
    deflate_buffer.resize(end_byte - start_byte);
    TEST_AND_RETURN_FALSE(src->Seek(start_byte));
    TEST_AND_RETURN_FALSE(
        src->Read(deflate_buffer.data(), deflate_buffer.size()));
    // Find the size of the puff.
    BufferBitReader bit_reader(deflate_buffer.data(), deflate_buffer.size());
    uint64_t bits_to_skip = deflate->offset % 8;
    TEST_AND_RETURN_FALSE(bit_reader.CacheBits(bits_to_skip));
    bit_reader.DropBits(bits_to_skip);

    BufferPuffWriter puff_writer(nullptr, 0);
    TEST_AND_RETURN_FALSE(
        puffer.PuffDeflate(&bit_reader, &puff_writer, nullptr));
    TEST_AND_RETURN_FALSE(deflate_buffer.size() == bit_reader.Offset());

    // 1 if a deflate ends at the same byte that the next deflate starts and
    // there is a few bits gap between them. In practice this may never happen,
    // but it is a good idea to support it anyways. If there is a gap, the value
    // of the gap will be saved as an integer byte to the puff stream. The parts
    // of the byte that belogs to the deflates are shifted out.
    int gap = 0;
    if (deflate != deflates.begin()) {
      auto prev_deflate = std::prev(deflate);
      if ((prev_deflate->offset + prev_deflate->length == deflate->offset)
          // If deflates are on byte boundary the gap will not be counted later,
          // so we won't worry about it.
          && (deflate->offset % 8 != 0)) {
        gap = 1;
      }
    }

    start_byte = ((deflate->offset + 7) / 8);
    end_byte = (deflate->offset + deflate->length) / 8;
    int64_t deflate_length_in_bytes = end_byte - start_byte;

    // If there was no gap bits between the current and previous deflates, there
    // will be no extra gap byte, so the offset will be shifted one byte back.
    auto puff_offset = start_byte - gap + total_size_difference;
    auto puff_size = puff_writer.Size();
    // Add the location into puff.
    puffs->emplace_back(puff_offset, puff_size);
    total_size_difference +=
        static_cast<int64_t>(puff_size) - deflate_length_in_bytes - gap;
  }

  uint64_t src_size;
  TEST_AND_RETURN_FALSE(src->GetSize(&src_size));
  auto final_size = static_cast<int64_t>(src_size) + total_size_difference;
  TEST_AND_RETURN_FALSE(final_size >= 0);
  *out_puff_size = final_size;
  return true;
}

void RemoveEqualBitExtents(const Buffer& data1,
                           const Buffer& data2,
                           vector<BitExtent>* extents1,
                           vector<BitExtent>* extents2) {
  set<ExtentData> extent1_set, equal_extents;
  for (const BitExtent& ext : *extents1) {
    extent1_set.emplace(ext, data1);
  }

  auto new_extents2_end = extents2->begin();
  for (const BitExtent& ext : *extents2) {
    ExtentData extent_data(ext, data2);
    if (extent1_set.find(extent_data) != extent1_set.end()) {
      equal_extents.insert(extent_data);
    } else {
      *new_extents2_end++ = ext;
    }
  }
  extents2->erase(new_extents2_end, extents2->end());
  extents1->erase(
      std::remove_if(extents1->begin(), extents1->end(),
                     [&equal_extents, &data1](const BitExtent& ext) {
                       return equal_extents.find(ExtentData(ext, data1)) !=
                              equal_extents.end();
                     }),
      extents1->end());
}

bool RemoveDeflatesWithBadDistanceCaches(const Buffer& data,
                                         vector<BitExtent>* deflates) {
  Puffer puffer(true /* exclude_bad_distance_caches */);
  for (auto def = deflates->begin(); def != deflates->end();) {
    uint64_t offset = def->offset / 8;
    uint64_t length = (def->offset + def->length + 7) / 8 - offset;
    BufferBitReader br(&data[offset], length);
    BufferPuffWriter pw(nullptr, 0);

    // Drop the first few bits in the buffer so we start exactly where the
    // deflate starts.
    uint64_t bits_to_drop = def->offset % 8;
    TEST_AND_RETURN_FALSE(br.CacheBits(bits_to_drop));
    br.DropBits(bits_to_drop);

    vector<BitExtent> defs_out;
    TEST_AND_RETURN_FALSE(puffer.PuffDeflate(&br, &pw, &defs_out));

    TEST_AND_RETURN_FALSE(defs_out.size() <= 1);
    if (defs_out.size() == 0) {
      // This is a deflate we were looking for, remove it.
      def = deflates->erase(def);
    } else {
      ++def;
    }
  }
  return true;
}

}  // namespace puffin
