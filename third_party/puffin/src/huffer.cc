// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/huffer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "puffin/src/bit_writer.h"
#include "puffin/src/huffman_table.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/stream.h"
#include "puffin/src/logging.h"
#include "puffin/src/puff_data.h"
#include "puffin/src/puff_reader.h"

using std::string;

namespace puffin {

Huffer::Huffer() : dyn_ht_(new HuffmanTable()), fix_ht_(new HuffmanTable()) {}

Huffer::~Huffer() = default;

bool Huffer::HuffDeflate(PuffReaderInterface* pr,
                         BitWriterInterface* bw) const {
  PuffData pd;
  HuffmanTable* cur_ht = nullptr;
  // If no bytes left for PuffReader to read, bail out.
  while (pr->BytesLeft() != 0) {
    TEST_AND_RETURN_FALSE(pr->GetNext(&pd));

    // The first data should be a metadata.
    TEST_AND_RETURN_FALSE(pd.type == PuffData::Type::kBlockMetadata);
    auto header = pd.block_metadata[0];
    auto final_bit = (header & 0x80) >> 7;
    auto type = (header & 0x60) >> 5;
    auto skipped_bits = header & 0x1F;

    TEST_AND_RETURN_FALSE(bw->WriteBits(1, final_bit));
    TEST_AND_RETURN_FALSE(bw->WriteBits(2, type));
    switch (static_cast<BlockType>(type)) {
      case BlockType::kUncompressed:
        bw->WriteBoundaryBits(skipped_bits);
        TEST_AND_RETURN_FALSE(pr->GetNext(&pd));
        TEST_AND_RETURN_FALSE(pd.type != PuffData::Type::kLiteral);

        if (pd.type == PuffData::Type::kLiterals) {
          TEST_AND_RETURN_FALSE(bw->WriteBits(16, pd.length));
          TEST_AND_RETURN_FALSE(bw->WriteBits(16, ~pd.length));
          TEST_AND_RETURN_FALSE(bw->WriteBytes(pd.length, pd.read_fn));
          // Reading end of block, but don't write anything.
          TEST_AND_RETURN_FALSE(pr->GetNext(&pd));
          TEST_AND_RETURN_FALSE(pd.type == PuffData::Type::kEndOfBlock);
        } else if (pd.type == PuffData::Type::kEndOfBlock) {
          TEST_AND_RETURN_FALSE(bw->WriteBits(16, 0));
          TEST_AND_RETURN_FALSE(bw->WriteBits(16, ~0));
        } else {
          return false;
        }
        // We have to read a new block.
        continue;

      case BlockType::kFixed:
        fix_ht_->BuildFixedHuffmanTable();
        cur_ht = fix_ht_.get();
        break;

      case BlockType::kDynamic:
        cur_ht = dyn_ht_.get();
        TEST_AND_RETURN_FALSE(dyn_ht_->BuildDynamicHuffmanTable(
            &pd.block_metadata[1], pd.length - 1, bw));
        break;

      default:
        return false;
    }

    // We read literal or distrance/lengths until and end of block or end of
    // stream is reached.
    bool block_ended = false;
    while (!block_ended) {
      TEST_AND_RETURN_FALSE(pr->GetNext(&pd));
      switch (pd.type) {
        case PuffData::Type::kLiteral:
        case PuffData::Type::kLiterals: {
          auto write_literal = [&cur_ht, &bw](uint8_t literal) {
            uint16_t literal_huffman;
            size_t nbits;
            TEST_AND_RETURN_FALSE(
                cur_ht->LitLenHuffman(literal, &literal_huffman, &nbits));
            TEST_AND_RETURN_FALSE(bw->WriteBits(nbits, literal_huffman));
            return true;
          };

          if (pd.type == PuffData::Type::kLiteral) {
            TEST_AND_RETURN_FALSE(write_literal(pd.byte));
          } else {
            auto len = pd.length;
            while (len-- > 0) {
              uint8_t literal;
              pd.read_fn(&literal, 1);
              TEST_AND_RETURN_FALSE(write_literal(literal));
            }
          }
          break;
        }
        case PuffData::Type::kLenDist: {
          auto len = pd.length;
          auto dist = pd.distance;
          TEST_AND_RETURN_FALSE(len >= 3 && len <= 258);

          // Using a binary search here instead of the linear search may be (but
          // not necessarily) faster. Needs experiment to validate.
          size_t index = 0;
          while (len > kLengthBases[index]) {
            index++;
          }
          if (len < kLengthBases[index]) {
            index--;
          }
          auto extra_bits_len = kLengthExtraBits[index];
          uint16_t length_huffman;
          size_t nbits;
          TEST_AND_RETURN_FALSE(
              cur_ht->LitLenHuffman(index + 257, &length_huffman, &nbits));

          TEST_AND_RETURN_FALSE(bw->WriteBits(nbits, length_huffman));

          if (extra_bits_len > 0) {
            TEST_AND_RETURN_FALSE(
                bw->WriteBits(extra_bits_len, len - kLengthBases[index]));
          }

          // Same as above (binary search).
          index = 0;
          while (dist > kDistanceBases[index]) {
            index++;
          }
          if (dist < kDistanceBases[index]) {
            index--;
          }
          extra_bits_len = kDistanceExtraBits[index];
          uint16_t distance_huffman;
          TEST_AND_RETURN_FALSE(
              cur_ht->DistanceHuffman(index, &distance_huffman, &nbits));

          TEST_AND_RETURN_FALSE(bw->WriteBits(nbits, distance_huffman));
          if (extra_bits_len > 0) {
            TEST_AND_RETURN_FALSE(
                bw->WriteBits(extra_bits_len, dist - kDistanceBases[index]));
          }
          break;
        }

        case PuffData::Type::kEndOfBlock: {
          uint16_t eos_huffman;
          size_t nbits;
          TEST_AND_RETURN_FALSE(
              cur_ht->LitLenHuffman(256, &eos_huffman, &nbits));
          TEST_AND_RETURN_FALSE(bw->WriteBits(nbits, eos_huffman));
          block_ended = true;
          break;
        }
        case PuffData::Type::kBlockMetadata:
          return false;

        default:
          return false;
      }
    }
  }

  TEST_AND_RETURN_FALSE(bw->Flush());
  return true;
}

}  // namespace puffin
