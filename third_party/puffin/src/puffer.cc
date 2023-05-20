// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/puffer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "puffin/src/bit_reader.h"
#include "puffin/src/huffman_table.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/stream.h"
#include "puffin/src/logging.h"
#include "puffin/src/puff_data.h"
#include "puffin/src/puff_writer.h"

using std::string;
using std::vector;

namespace puffin {

Puffer::Puffer(bool exclude_bad_distance_caches)
    : dyn_ht_(new HuffmanTable()),
      fix_ht_(new HuffmanTable()),
      exclude_bad_distance_caches_(exclude_bad_distance_caches) {}

Puffer::Puffer() : Puffer(false) {}

Puffer::~Puffer() = default;

bool Puffer::PuffDeflate(BitReaderInterface* br,
                         PuffWriterInterface* pw,
                         vector<BitExtent>* deflates) const {
  PuffData pd;
  HuffmanTable* cur_ht;
  bool end_loop = false;
  // No bits left to read, return. We try to cache at least eight bits because
  // the minimum length of a deflate bit stream is 8: (fixed huffman table) 3
  // bits header + 5 bits just one len/dist symbol.
  while (!end_loop && br->CacheBits(8)) {
    auto start_bit_offset = br->OffsetInBits();

    TEST_AND_RETURN_FALSE(br->CacheBits(3));
    uint8_t final_bit = br->ReadBits(1);  // BFINAL
    br->DropBits(1);
    uint8_t type = br->ReadBits(2);  // BTYPE
    br->DropBits(2);

    // If it is the final block and we are just looking for deflate locations,
    // we consider this the end of the search.
    if (deflates != nullptr && final_bit) {
      end_loop = true;
    }

    // Header structure
    // +-+-+-+-+-+-+-+-+
    // |F| TP|   SKIP  |
    // +-+-+-+-+-+-+-+-+
    // F -> final_bit
    // TP -> type
    // SKIP -> skipped_bits (only in kUncompressed type)
    auto block_header = (final_bit << 7) | (type << 5);
    switch (static_cast<BlockType>(type)) {
      case BlockType::kUncompressed: {
        auto skipped_bits = br->ReadBoundaryBits();
        br->SkipBoundaryBits();
        TEST_AND_RETURN_FALSE(br->CacheBits(32));
        auto len = br->ReadBits(16);  // LEN
        br->DropBits(16);
        auto nlen = br->ReadBits(16);  // NLEN
        br->DropBits(16);

        if ((len ^ nlen) != 0xFFFF) {
          return false;
        }

        // Put skipped bits into header.
        block_header |= skipped_bits;

        // Insert block header.
        pd.type = PuffData::Type::kBlockMetadata;
        pd.block_metadata[0] = block_header;
        pd.length = 1;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));

        // Insert all the raw literals.
        pd.type = PuffData::Type::kLiterals;
        pd.length = len;
        TEST_AND_RETURN_FALSE(br->GetByteReaderFn(pd.length, &pd.read_fn));
        TEST_AND_RETURN_FALSE(pw->Insert(pd));

        pd.type = PuffData::Type::kEndOfBlock;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));

        // There is no need to insert the location of uncompressed deflates
        // because we do not want the uncompressed blocks when trying to find
        // the bit-addressed location of deflates. They better be ignored.

        // continue the loop. Do not read any literal/length/distance.
        continue;
      }

      case BlockType::kFixed:
        fix_ht_->BuildFixedHuffmanTable();
        cur_ht = fix_ht_.get();
        pd.type = PuffData::Type::kBlockMetadata;
        pd.block_metadata[0] = block_header;
        pd.length = 1;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));
        break;

      case BlockType::kDynamic:
        pd.type = PuffData::Type::kBlockMetadata;
        pd.block_metadata[0] = block_header;
        pd.length = sizeof(pd.block_metadata) - 1;
        TEST_AND_RETURN_FALSE(dyn_ht_->BuildDynamicHuffmanTable(
            br, &pd.block_metadata[1], &pd.length));
        pd.length += 1;  // For the header.
        TEST_AND_RETURN_FALSE(pw->Insert(pd));
        cur_ht = dyn_ht_.get();
        break;

      default:
        return false;
    }

    // If true and the list of output |deflates| is non-null, the current
    // deflate location will be added to that list.
    bool include_deflate = true;

    while (true) {  // Breaks when the end of block is reached.
      auto max_bits = cur_ht->LitLenMaxBits();
      if (!br->CacheBits(max_bits)) {
        // It could be the end of buffer and the bit length of the end_of_block
        // symbol has less than maximum bit length of current Huffman table. So
        // only asking for the size of end of block symbol (256).
        TEST_AND_RETURN_FALSE(cur_ht->EndOfBlockBitLength(&max_bits));
      }
      TEST_AND_RETURN_FALSE(br->CacheBits(max_bits));
      auto bits = br->ReadBits(max_bits);
      uint16_t lit_len_alphabet;
      size_t dropNbits = 0;
      TEST_AND_RETURN_FALSE(
          cur_ht->LitLenAlphabet(bits, &lit_len_alphabet, &dropNbits));
      br->DropBits(dropNbits);
      if (lit_len_alphabet < 256) {
        pd.type = PuffData::Type::kLiteral;
        pd.byte = lit_len_alphabet;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));

      } else if (256 == lit_len_alphabet) {
        pd.type = PuffData::Type::kEndOfBlock;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));
        if (deflates != nullptr && include_deflate) {
          deflates->emplace_back(start_bit_offset,
                                 br->OffsetInBits() - start_bit_offset);
        }
        break;  // Breaks the loop.
      } else {
        TEST_AND_RETURN_FALSE(lit_len_alphabet <= 285);
        // Reading length.
        auto len_code_start = lit_len_alphabet - 257;
        auto extra_bits_len = kLengthExtraBits[len_code_start];
        uint16_t extra_bits_value = 0;
        if (extra_bits_len) {
          TEST_AND_RETURN_FALSE(br->CacheBits(extra_bits_len));
          extra_bits_value = br->ReadBits(extra_bits_len);
          br->DropBits(extra_bits_len);
        }
        auto length = kLengthBases[len_code_start] + extra_bits_value;

        auto bits_to_cache = cur_ht->DistanceMaxBits();
        if (!br->CacheBits(bits_to_cache)) {
          // This is a corner case that is present in the older versions of the
          // Puffin. So we need to catch it and correctly discard this kind of
          // deflate when we encounter it. See crbug.com/915559 for more info.
          bits_to_cache = br->BitsRemaining();
          TEST_AND_RETURN_FALSE(br->CacheBits(bits_to_cache));
          if (exclude_bad_distance_caches_) {
            include_deflate = false;
          }
        }
        auto read_bits = br->ReadBits(bits_to_cache);
        size_t nbits = 0;
        uint16_t distance_alphabet;
        TEST_AND_RETURN_FALSE(
            cur_ht->DistanceAlphabet(read_bits, &distance_alphabet, &nbits));
        br->DropBits(nbits);

        // Reading distance.
        extra_bits_len = kDistanceExtraBits[distance_alphabet];
        extra_bits_value = 0;
        if (extra_bits_len) {
          TEST_AND_RETURN_FALSE(br->CacheBits(extra_bits_len));
          extra_bits_value = br->ReadBits(extra_bits_len);
          br->DropBits(extra_bits_len);
        }

        pd.type = PuffData::Type::kLenDist;
        pd.length = length;
        pd.distance = kDistanceBases[distance_alphabet] + extra_bits_value;
        TEST_AND_RETURN_FALSE(pw->Insert(pd));
      }
    }
  }
  TEST_AND_RETURN_FALSE(pw->Flush());
  return true;
}

}  // namespace puffin
