// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/huffman_table.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "puffin/src/logging.h"

using std::string;
using std::vector;

namespace puffin {

// Permutations of input Huffman code lengths (used only to read code lengths
// necessary for reading Huffman table.)
const uint8_t kPermutations[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                   11, 4,  12, 3, 13, 2, 14, 1, 15};

// The bases of each alphabet which is added to the integer value of extra
// bits that comes after the Huffman code in the input to create the given
// length value. The last element is a guard.
const uint16_t kLengthBases[30] = {
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23,  27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0xFFFF};

// Number of extra bits that comes after the associating Huffman code.
const uint8_t kLengthExtraBits[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                      1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                      4, 4, 4, 4, 5, 5, 5, 5, 0};

// Same as |kLengthBases| but for the distances instead of lengths. The last
// element is a guard.
const uint16_t kDistanceBases[31] = {
    1,    2,    3,    4,    5,    7,     9,     13,    17,    25,   33,
    49,   65,   97,   129,  193,  257,   385,   513,   769,   1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0xFFFF};

// Same as |kLengthExtraBits| but for distances instead of lengths.
const uint8_t kDistanceExtraBits[30] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                        4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                        9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// 288 is the maximum number of needed huffman codes for an alphabet. Fixed
// huffman table needs 288 and dynamic huffman table needs maximum 286.
// 286 = 256 (coding a byte) +
//         1 (coding the end of block symbole) +
//        29 (coding the lengths)
HuffmanTable::HuffmanTable() : codeindexpairs_(288) {}

bool HuffmanTable::InitHuffmanCodes(const Buffer& lens, size_t* max_bits) {
  // Temporary buffers used in |InitHuffmanCodes|.
  uint16_t len_count_[kMaxHuffmanBits + 1] = {0};
  uint16_t next_code_[kMaxHuffmanBits + 1] = {0};

  // 1. Count the number of codes for each length;
  for (auto len : lens) {
    len_count_[len]++;
  }

  for (*max_bits = kMaxHuffmanBits; *max_bits >= 1; (*max_bits)--) {
    if (len_count_[*max_bits] != 0) {
      break;
    }
  }

  // Check for oversubscribed code lengths. (A code with length 'L' cannot have
  // more than 2^L items.
  for (size_t idx = 1; idx <= *max_bits; idx++) {
    if (len_count_[idx] > (1 << idx)) {
      return false;
    }
  }

  // 2. Compute the coding of the first element for each length.
  uint16_t code = 0;
  len_count_[0] = 0;
  for (size_t bits = 1; bits <= kMaxHuffmanBits; bits++) {
    code = (code + len_count_[bits - 1]) << 1;
    next_code_[bits] = code;
  }

  codeindexpairs_.clear();
  // 3. Calculate all the code values.
  for (size_t idx = 0; idx < lens.size(); idx++) {
    auto len = lens[idx];
    if (len == 0) {
      continue;
    }

    // Reverse the code
    CodeIndexPair cip;
    cip.code = 0;
    auto tmp_code = next_code_[len];
    for (size_t r = 0; r < len; r++) {
      cip.code <<= 1;
      cip.code |= tmp_code & 1U;
      tmp_code >>= 1;
    }
    cip.index = idx;
    codeindexpairs_.push_back(cip);
    next_code_[len]++;
  }
  return true;
}

bool HuffmanTable::BuildHuffmanCodes(const Buffer& lens,
                                     vector<uint16_t>* hcodes,
                                     size_t* max_bits) {
  TEST_AND_RETURN_FALSE(InitHuffmanCodes(lens, max_bits));
  // Sort descending based on the bit-length of the code.
  std::sort(codeindexpairs_.begin(), codeindexpairs_.end(),
            [&lens](const CodeIndexPair& a, const CodeIndexPair& b) {
              return lens[a.index] > lens[b.index];
            });

  // Only zero out the part of hcodes which is valuable.
  memset(hcodes->data(), 0, (1 << *max_bits) * sizeof(uint16_t));
  for (const auto& cip : codeindexpairs_) {
    // The MSB bit of the code in hcodes is set if it is a valid code and its
    // code exists in the input Huffman table.
    (*hcodes)[cip.code] = cip.index | 0x8000;
    auto fill_bits = *max_bits - lens[cip.index];
    for (auto idx = 1; idx < (1 << fill_bits); idx++) {
      auto location = (idx << lens[cip.index]) | cip.code;
      if (!((*hcodes)[location] & 0x8000)) {
        (*hcodes)[location] = cip.index | 0x8000;
      }
    }
  }
  return true;
}

bool HuffmanTable::BuildHuffmanReverseCodes(const Buffer& lens,
                                            vector<uint16_t>* rcodes,
                                            size_t* max_bits) {
  TEST_AND_RETURN_FALSE(InitHuffmanCodes(lens, max_bits));
  // Sort ascending based on the index.
  std::sort(codeindexpairs_.begin(), codeindexpairs_.end(),
            [](const CodeIndexPair& a, const CodeIndexPair& b) -> bool {
              return a.index < b.index;
            });

  size_t index = 0;
  for (size_t idx = 0; idx < rcodes->size(); idx++) {
    if (index < codeindexpairs_.size() && idx == codeindexpairs_[index].index) {
      (*rcodes)[idx] = codeindexpairs_[index].code;
      index++;
    } else {
      (*rcodes)[idx] = 0;
    }
  }
  return true;
}

bool HuffmanTable::BuildFixedHuffmanTable() {
  if (!initialized_) {
    // For all the vectors used in this class, we set the size in the
    // constructor and we do not change the size later. This is for optimization
    // purposes. The total size of data in this class is approximately
    // 2KB. Because it is a constructor return values cannot be checked.
    lit_len_lens_.resize(288);
    lit_len_rcodes_.resize(288);
    lit_len_hcodes_.resize(1 << 9);

    distance_lens_.resize(30);
    distance_rcodes_.resize(30);
    distance_hcodes_.resize(1 << 5);

    size_t i = 0;
    while (i < 144) {
      lit_len_lens_[i++] = 8;
    }
    while (i < 256) {
      lit_len_lens_[i++] = 9;
    }
    while (i < 280) {
      lit_len_lens_[i++] = 7;
    }
    while (i < 288) {
      lit_len_lens_[i++] = 8;
    }

    i = 0;
    while (i < 30) {
      distance_lens_[i++] = 5;
    }

    TEST_AND_RETURN_FALSE(
        BuildHuffmanCodes(lit_len_lens_, &lit_len_hcodes_, &lit_len_max_bits_));

    TEST_AND_RETURN_FALSE(BuildHuffmanCodes(distance_lens_, &distance_hcodes_,
                                            &distance_max_bits_));

    TEST_AND_RETURN_FALSE(BuildHuffmanReverseCodes(
        lit_len_lens_, &lit_len_rcodes_, &lit_len_max_bits_));

    TEST_AND_RETURN_FALSE(BuildHuffmanReverseCodes(
        distance_lens_, &distance_rcodes_, &distance_max_bits_));

    initialized_ = true;
  }
  return true;
}

bool HuffmanTable::BuildDynamicHuffmanTable(BitReaderInterface* br,
                                            uint8_t* buffer,
                                            size_t* length) {
  // Initilize only once and reuse.
  if (!initialized_) {
    // Only resizing the arrays needed.
    code_lens_.resize(19);
    code_hcodes_.resize(1 << 7);

    lit_len_lens_.resize(286);
    lit_len_hcodes_.resize(1 << 15);

    distance_lens_.resize(30);
    distance_hcodes_.resize(1 << 15);

    // 286: Maximum number of literal/lengths symbols.
    // 30: Maximum number of distance symbols.
    // The reason we reserve this to the sum of both maximum sizes is that we
    // need to calculate both huffman codes contiguously. See b/72815313.
    tmp_lens_.resize(286 + 30);
    initialized_ = true;
  }

  // Read the header. Reads the first portion of the Huffman data from input and
  // writes it into the puff |buffer|. The first portion includes the size
  // (|num_lit_len|) of the literals/lengths Huffman code length array
  // (|dynamic_lit_len_lens_|), the size (|num_distance|) of distance Huffman
  // code length array (|dynamic_distance_lens_|), and the size (|num_codes|) of
  // Huffman code length array (dynamic_code_lens_) for reading
  // |dynamic_lit_len_lens_| and |dynamic_distance_lens_|. Then it follows by
  // reading |dynamic_code_lens_|.

  TEST_AND_RETURN_FALSE(*length >= 3);
  size_t index = 0;
  TEST_AND_RETURN_FALSE(br->CacheBits(14));
  buffer[index++] = br->ReadBits(5);  // HLIST
  auto num_lit_len = br->ReadBits(5) + 257;
  br->DropBits(5);

  buffer[index++] = br->ReadBits(5);  // HDIST
  auto num_distance = br->ReadBits(5) + 1;
  br->DropBits(5);

  buffer[index++] = br->ReadBits(4);  // HCLEN
  auto num_codes = br->ReadBits(4) + 4;
  br->DropBits(4);

  TEST_AND_RETURN_FALSE(
      CheckHuffmanArrayLengths(num_lit_len, num_distance, num_codes));

  bool checked = false;
  size_t idx = 0;
  TEST_AND_RETURN_FALSE(*length - index >= (num_codes + 1) / 2);
  // Two codes per byte
  for (; idx < num_codes; idx++) {
    TEST_AND_RETURN_FALSE(br->CacheBits(3));
    code_lens_[kPermutations[idx]] = br->ReadBits(3);
    if (checked) {
      buffer[index++] |= br->ReadBits(3);
    } else {
      buffer[index] = br->ReadBits(3) << 4;
    }
    checked = !checked;
    br->DropBits(3);
  }
  // Pad the last byte if odd number of codes.
  if (checked) {
    index++;
  }
  for (; idx < 19; idx++) {
    code_lens_[kPermutations[idx]] = 0;
  }

  TEST_AND_RETURN_FALSE(
      BuildHuffmanCodes(code_lens_, &code_hcodes_, &code_max_bits_));

  // Build literals/lengths and distance Huffman code length arrays.
  auto bytes_available = (*length - index);
  tmp_lens_.clear();
  TEST_AND_RETURN_FALSE(BuildHuffmanCodeLengths(
      br, buffer + index, &bytes_available, code_max_bits_,
      num_lit_len + num_distance, &tmp_lens_));
  index += bytes_available;

  // TODO(ahassani): Optimize this so the memcpy is not needed anymore.
  lit_len_lens_.clear();
  lit_len_lens_.insert(lit_len_lens_.begin(), tmp_lens_.begin(),
                       tmp_lens_.begin() + num_lit_len);

  distance_lens_.clear();
  distance_lens_.insert(distance_lens_.begin(), tmp_lens_.begin() + num_lit_len,
                        tmp_lens_.end());

  TEST_AND_RETURN_FALSE(
      BuildHuffmanCodes(lit_len_lens_, &lit_len_hcodes_, &lit_len_max_bits_));

  // Build distance Huffman codes.
  TEST_AND_RETURN_FALSE(BuildHuffmanCodes(distance_lens_, &distance_hcodes_,
                                          &distance_max_bits_));

  *length = index;
  return true;
}

bool HuffmanTable::BuildHuffmanCodeLengths(BitReaderInterface* br,
                                           uint8_t* buffer,
                                           size_t* length,
                                           size_t max_bits,
                                           size_t num_codes,
                                           Buffer* lens) {
  size_t index = 0;
  lens->clear();
  for (size_t idx = 0; idx < num_codes;) {
    TEST_AND_RETURN_FALSE(br->CacheBits(max_bits));
    auto bits = br->ReadBits(max_bits);
    uint16_t code;
    size_t nbits;
    TEST_AND_RETURN_FALSE(CodeAlphabet(bits, &code, &nbits));
    TEST_AND_RETURN_FALSE(index < *length);
    br->DropBits(nbits);
    if (code < 16) {
      buffer[index++] = code;
      lens->push_back(code);
      idx++;
    } else {
      TEST_AND_RETURN_FALSE(code < 19);
      size_t copy_num = 0;
      uint8_t copy_val;
      switch (code) {
        case 16:
          TEST_AND_RETURN_FALSE(idx != 0);
          TEST_AND_RETURN_FALSE(br->CacheBits(2));
          copy_num = 3 + br->ReadBits(2);
          buffer[index++] = 16 + br->ReadBits(2);  // 3 - 6 times
          copy_val = (*lens)[idx - 1];
          br->DropBits(2);
          break;

        case 17:
          TEST_AND_RETURN_FALSE(br->CacheBits(3));
          copy_num = 3 + br->ReadBits(3);
          buffer[index++] = 20 + br->ReadBits(3);  // 3 - 10 times
          copy_val = 0;
          br->DropBits(3);
          break;

        case 18:
          TEST_AND_RETURN_FALSE(br->CacheBits(7));
          copy_num = 11 + br->ReadBits(7);
          buffer[index++] = 28 + br->ReadBits(7);  // 11 - 138 times
          copy_val = 0;
          br->DropBits(7);
          break;

        default:
          return false;
      }
      idx += copy_num;
      while (copy_num--) {
        lens->push_back(copy_val);
      }
    }
  }
  TEST_AND_RETURN_FALSE(lens->size() == num_codes);
  *length = index;
  return true;
}

bool HuffmanTable::BuildDynamicHuffmanTable(const uint8_t* buffer,
                                            size_t length,
                                            BitWriterInterface* bw) {
  if (!initialized_) {
    // Only resizing the arrays needed.
    code_lens_.resize(19);
    code_rcodes_.resize(19);

    lit_len_lens_.resize(286);
    lit_len_rcodes_.resize(286);

    distance_lens_.resize(30);
    distance_rcodes_.resize(30);

    tmp_lens_.resize(286 + 30);

    initialized_ = true;
  }

  TEST_AND_RETURN_FALSE(length >= 3);
  size_t index = 0;
  // Write the header.
  size_t num_lit_len = buffer[index] + 257;
  TEST_AND_RETURN_FALSE(bw->WriteBits(5, buffer[index++]));

  size_t num_distance = buffer[index] + 1;
  TEST_AND_RETURN_FALSE(bw->WriteBits(5, buffer[index++]));

  size_t num_codes = buffer[index] + 4;
  TEST_AND_RETURN_FALSE(bw->WriteBits(4, buffer[index++]));

  TEST_AND_RETURN_FALSE(
      CheckHuffmanArrayLengths(num_lit_len, num_distance, num_codes));

  TEST_AND_RETURN_FALSE(length - index >= (num_codes + 1) / 2);
  bool checked = false;
  size_t idx = 0;
  for (; idx < num_codes; idx++) {
    uint8_t len;
    if (checked) {
      len = buffer[index++] & 0x0F;
    } else {
      len = buffer[index] >> 4;
    }
    checked = !checked;
    code_lens_[kPermutations[idx]] = len;
    TEST_AND_RETURN_FALSE(bw->WriteBits(3, len));
  }
  if (checked) {
    index++;
  }
  for (; idx < 19; idx++) {
    code_lens_[kPermutations[idx]] = 0;
  }

  TEST_AND_RETURN_FALSE(
      BuildHuffmanReverseCodes(code_lens_, &code_rcodes_, &code_max_bits_));

  // Build literal/lengths and distance Huffman code length arrays.
  auto bytes_available = length - index;
  TEST_AND_RETURN_FALSE(
      BuildHuffmanCodeLengths(buffer + index, &bytes_available, bw,
                              num_lit_len + num_distance, &tmp_lens_));
  index += bytes_available;

  lit_len_lens_.clear();
  lit_len_lens_.insert(lit_len_lens_.begin(), tmp_lens_.begin(),
                       tmp_lens_.begin() + num_lit_len);

  distance_lens_.clear();
  distance_lens_.insert(distance_lens_.begin(), tmp_lens_.begin() + num_lit_len,
                        tmp_lens_.end());

  // Build literal/lengths Huffman reverse codes.
  TEST_AND_RETURN_FALSE(BuildHuffmanReverseCodes(
      lit_len_lens_, &lit_len_rcodes_, &lit_len_max_bits_));

  // Build distance Huffman reverse codes.
  TEST_AND_RETURN_FALSE(BuildHuffmanReverseCodes(
      distance_lens_, &distance_rcodes_, &distance_max_bits_));

  TEST_AND_RETURN_FALSE(length == index);

  return true;
}

bool HuffmanTable::BuildHuffmanCodeLengths(const uint8_t* buffer,
                                           size_t* length,
                                           BitWriterInterface* bw,
                                           size_t num_codes,
                                           Buffer* lens) {
  lens->clear();
  uint16_t hcode;
  size_t nbits;
  size_t index = 0;
  for (size_t idx = 0; idx < num_codes;) {
    TEST_AND_RETURN_FALSE(index < *length);
    auto pcode = buffer[index++];
    TEST_AND_RETURN_FALSE(pcode <= 155);

    auto code = pcode < 16 ? pcode : pcode < 20 ? 16 : pcode < 28 ? 17 : 18;
    TEST_AND_RETURN_FALSE(CodeHuffman(code, &hcode, &nbits));
    TEST_AND_RETURN_FALSE(bw->WriteBits(nbits, hcode));
    if (code < 16) {
      lens->push_back(code);
      idx++;
    } else {
      size_t copy_num = 0;
      uint8_t copy_val;
      switch (code) {
        case 16:
          // Cannot repeat a non-existent last code if idx == 0.
          TEST_AND_RETURN_FALSE(idx != 0);
          TEST_AND_RETURN_FALSE(bw->WriteBits(2, pcode - 16));
          copy_num = 3 + pcode - 16;
          copy_val = (*lens)[idx - 1];
          break;

        case 17:
          TEST_AND_RETURN_FALSE(bw->WriteBits(3, pcode - 20));
          copy_num = 3 + pcode - 20;
          copy_val = 0;
          break;

        case 18:
          TEST_AND_RETURN_FALSE(bw->WriteBits(7, pcode - 28));
          copy_num = 11 + pcode - 28;
          copy_val = 0;
          break;

        default:
          break;
      }
      idx += copy_num;
      while (copy_num--) {
        lens->push_back(copy_val);
      }
    }
  }
  TEST_AND_RETURN_FALSE(lens->size() == num_codes);
  *length = index;
  return true;
}

string BlockTypeToString(BlockType type) {
  switch (type) {
    case BlockType::kUncompressed:
      return "Uncompressed";

    case BlockType::kFixed:
      return "Fixed";

    case BlockType::kDynamic:
      return "Dynamic";

    default:
      return "Unknown";
  }
}

}  // namespace puffin
