// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/puffin_stream.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "puffin/src/bit_reader.h"
#include "puffin/src/bit_writer.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/include/puffin/stream.h"
#include "puffin/src/logging.h"
#include "puffin/src/puff_reader.h"
#include "puffin/src/puff_writer.h"

using std::shared_ptr;
using std::unique_ptr;
using std::vector;

namespace puffin {

namespace {

bool CheckArgsIntegrity(uint64_t puff_size,
                        const vector<BitExtent>& deflates,
                        const vector<ByteExtent>& puffs) {
  TEST_AND_RETURN_FALSE(puffs.size() == deflates.size());
  // Check if the |puff_size| is actually greater than the last byte of the last
  // puff in |puffs|.
  if (!puffs.empty()) {
    TEST_AND_RETURN_FALSE(puff_size >=
                          puffs.back().offset + puffs.back().length);
  }

  // Check to make sure |puffs| and |deflates| are sorted and non-overlapping.
  auto is_overlap = [](const auto& a, const auto& b) {
    return (a.offset + a.length) > b.offset;
  };
  TEST_AND_RETURN_FALSE(deflates.end() == std::adjacent_find(deflates.begin(),
                                                             deflates.end(),
                                                             is_overlap));
  TEST_AND_RETURN_FALSE(puffs.end() == std::adjacent_find(puffs.begin(),
                                                          puffs.end(),
                                                          is_overlap));
  return true;
}

}  // namespace

UniqueStreamPtr PuffinStream::CreateForPuff(UniqueStreamPtr stream,
                                            shared_ptr<Puffer> puffer,
                                            uint64_t puff_size,
                                            const vector<BitExtent>& deflates,
                                            const vector<ByteExtent>& puffs,
                                            size_t max_cache_size) {
  if (!CheckArgsIntegrity(puff_size, deflates, puffs) || !stream->Seek(0)) {
    stream->Close();
    return nullptr;
  }

  UniqueStreamPtr puffin_stream(new PuffinStream(std::move(stream), puffer,
                                                 nullptr, puff_size, deflates,
                                                 puffs, max_cache_size));
  if (!puffin_stream->Seek(0)) {
    puffin_stream->Close();
    return nullptr;
  }
  return puffin_stream;
}

UniqueStreamPtr PuffinStream::CreateForHuff(UniqueStreamPtr stream,
                                            shared_ptr<Huffer> huffer,
                                            uint64_t puff_size,
                                            const vector<BitExtent>& deflates,
                                            const vector<ByteExtent>& puffs) {
  if (!CheckArgsIntegrity(puff_size, deflates, puffs) || !stream->Seek(0)) {
    stream->Close();
    return nullptr;
  }

  UniqueStreamPtr puffin_stream(new PuffinStream(
      std::move(stream), nullptr, huffer, puff_size, deflates, puffs, 0));
  if (!puffin_stream->Seek(0)) {
    puffin_stream->Close();
    return nullptr;
  }
  return puffin_stream;
}

PuffinStream::PuffinStream(UniqueStreamPtr stream,
                           shared_ptr<Puffer> puffer,
                           shared_ptr<Huffer> huffer,
                           uint64_t puff_size,
                           const vector<BitExtent>& deflates,
                           const vector<ByteExtent>& puffs,
                           size_t max_cache_size)
    : stream_(std::move(stream)),
      puffer_(puffer),
      huffer_(huffer),
      puff_stream_size_(puff_size),
      deflates_(deflates),
      puffs_(puffs),
      is_for_puff_(puffer_ ? true : false),
      max_cache_size_(max_cache_size) {
  // Building upper bounds for faster seek.
  upper_bounds_.reserve(puffs.size());
  for (const auto& puff : puffs) {
    upper_bounds_.emplace_back(puff.offset + puff.length);
  }
  upper_bounds_.emplace_back(puff_stream_size_ + 1);

  // We can pass the size of the deflate stream too, but it is not necessary
  // yet. We cannot get the size of stream from itself, because we might be
  // writing into it and its size is not defined yet.
  uint64_t deflate_stream_size = puff_stream_size_;
  if (!puffs.empty()) {
    deflate_stream_size =
        ((deflates.back().offset + deflates.back().length) / 8) +
        puff_stream_size_ - (puffs.back().offset + puffs.back().length);
  }

  deflates_.emplace_back(deflate_stream_size * 8, 0);
  puffs_.emplace_back(puff_stream_size_, 0);

  // Look for the largest puff and deflate extents and get proper size buffers.
  uint64_t max_puff_length = 0;
  for (const auto& puff : puffs) {
    max_puff_length = std::max(max_puff_length, puff.length);
  }
  puff_buffer_ = std::make_shared<Buffer>(max_puff_length + 1);
  if (max_cache_size_ < max_puff_length) {
    max_cache_size_ = 0;  // It means we are not caching puffs.
  }

  uint64_t max_deflate_length = 0;
  for (const auto& deflate : deflates) {
    max_deflate_length = std::max(max_deflate_length, deflate.length * 8);
  }
  deflate_buffer_ = std::make_unique<Buffer>(max_deflate_length + 2);
}

bool PuffinStream::GetSize(uint64_t* size) {
  *size = puff_stream_size_;
  return true;
}

bool PuffinStream::GetOffset(uint64_t* offset) {
  *offset = puff_pos_ + skip_bytes_;
  return true;
}

bool PuffinStream::Seek(uint64_t offset) {
  TEST_AND_RETURN_FALSE(!closed_);
  if (!is_for_puff_) {
    // For huffing we should not seek, only seek to zero is accepted.
    TEST_AND_RETURN_FALSE(offset == 0);
  }

  TEST_AND_RETURN_FALSE(offset <= puff_stream_size_);

  // We are searching first available puff which either includes the |offset| or
  // it is the next available puff after |offset|.
  auto next_puff_iter =
      std::upper_bound(upper_bounds_.begin(), upper_bounds_.end(), offset);
  TEST_AND_RETURN_FALSE(next_puff_iter != upper_bounds_.end());
  auto next_puff_idx = std::distance(upper_bounds_.begin(), next_puff_iter);
  cur_puff_ = std::next(puffs_.begin(), next_puff_idx);
  cur_deflate_ = std::next(deflates_.begin(), next_puff_idx);

  if (offset < cur_puff_->offset) {
    // between two puffs.
    puff_pos_ = offset;
    auto back_track_bytes = cur_puff_->offset - puff_pos_;
    deflate_bit_pos_ = ((cur_deflate_->offset + 7) / 8 - back_track_bytes) * 8;
    if (cur_puff_ != puffs_.begin()) {
      auto prev_deflate = std::prev(cur_deflate_);
      if (deflate_bit_pos_ < prev_deflate->offset + prev_deflate->length) {
        deflate_bit_pos_ = prev_deflate->offset + prev_deflate->length;
      }
    }
  } else {
    // Inside a puff.
    puff_pos_ = cur_puff_->offset;
    deflate_bit_pos_ = cur_deflate_->offset;
  }
  skip_bytes_ = offset - puff_pos_;
  if (!is_for_puff_ && offset == 0) {
    TEST_AND_RETURN_FALSE(stream_->Seek(0));
    TEST_AND_RETURN_FALSE(SetExtraByte());
  }
  return true;
}

bool PuffinStream::Close() {
  closed_ = true;
  return stream_->Close();
}

bool PuffinStream::Read(void* buffer, size_t count) {
  TEST_AND_RETURN_FALSE(!closed_);
  TEST_AND_RETURN_FALSE(is_for_puff_);
  if (cur_puff_ == puffs_.end()) {
    TEST_AND_RETURN_FALSE(count == 0);
  }
  auto bytes = static_cast<uint8_t*>(buffer);
  uint64_t length = count;
  uint64_t bytes_read = 0;
  while (bytes_read < length) {
    if (puff_pos_ < cur_puff_->offset) {
      // Reading between two deflates. We also read bytes that have at least one
      // bit of a deflate bit stream. The byte which has both deflate and raw
      // data will be shifted or masked off the deflate bits and the remaining
      // value will be saved in the puff stream as an byte integer.
      uint64_t start_byte = (deflate_bit_pos_ / 8);
      uint64_t end_byte = (cur_deflate_->offset + 7) / 8;
      auto bytes_to_read = std::min(length - bytes_read, end_byte - start_byte);
      TEST_AND_RETURN_FALSE(bytes_to_read >= 1);

      TEST_AND_RETURN_FALSE(stream_->Seek(start_byte));
      TEST_AND_RETURN_FALSE(stream_->Read(bytes + bytes_read, bytes_to_read));

      // If true, we read the first byte of the curret deflate. So we have to
      // mask out the deflate bits (which are most significant bits.)
      if ((start_byte + bytes_to_read) * 8 > cur_deflate_->offset) {
        bytes[bytes_read + bytes_to_read - 1] &=
            (1 << (cur_deflate_->offset & 7)) - 1;
      }

      // If true, we read the last byte of the previous deflate and we have to
      // shift it out. The least significat bits belongs to the deflate
      // stream. The order of these last two conditions are important because a
      // byte can contain a finishing deflate and a starting deflate with some
      // bits between them so we have to modify correctly. Keep in mind that in
      // this situation both are modifying the same byte.
      if (start_byte * 8 < deflate_bit_pos_) {
        bytes[bytes_read] >>= deflate_bit_pos_ & 7;
      }

      // Pass |deflate_bit_pos_| for all the read bytes.
      deflate_bit_pos_ -= deflate_bit_pos_ & 7;
      deflate_bit_pos_ += bytes_to_read * 8;
      if (deflate_bit_pos_ > cur_deflate_->offset) {
        // In case it reads into the first byte of the current deflate.
        deflate_bit_pos_ = cur_deflate_->offset;
      }

      bytes_read += bytes_to_read;
      puff_pos_ += bytes_to_read;
      TEST_AND_RETURN_FALSE(puff_pos_ <= cur_puff_->offset);
    } else {
      // Reading the deflate itself. We read all bytes including the first and
      // last byte (which may partially include a deflate bit). Here we keep the
      // |puff_pos_| point to the first byte of the puffed stream and
      // |skip_bytes_| shows how many bytes in the puff we have copied till now.
      auto start_byte = (cur_deflate_->offset / 8);
      auto end_byte = (cur_deflate_->offset + cur_deflate_->length + 7) / 8;
      auto bytes_to_read = end_byte - start_byte;
      // Puff directly to buffer if it has space.
      bool puff_directly_into_buffer =
          max_cache_size_ == 0 && (skip_bytes_ == 0) &&
          (length - bytes_read >= cur_puff_->length);

      auto cur_puff_idx = std::distance(puffs_.begin(), cur_puff_);
      if (max_cache_size_ == 0 ||
          !GetPuffCache(cur_puff_idx, cur_puff_->length, &puff_buffer_)) {
        // Did not find the puff buffer in cache. We have to build it.
        deflate_buffer_->resize(bytes_to_read);
        TEST_AND_RETURN_FALSE(stream_->Seek(start_byte));
        TEST_AND_RETURN_FALSE(
            stream_->Read(deflate_buffer_->data(), bytes_to_read));
        BufferBitReader bit_reader(deflate_buffer_->data(), bytes_to_read);

        BufferPuffWriter puff_writer(puff_directly_into_buffer
                                         ? bytes + bytes_read
                                         : puff_buffer_->data(),
                                     cur_puff_->length);

        // Drop the first unused bits.
        size_t extra_bits_len = cur_deflate_->offset & 7;
        TEST_AND_RETURN_FALSE(bit_reader.CacheBits(extra_bits_len));
        bit_reader.DropBits(extra_bits_len);

        TEST_AND_RETURN_FALSE(
            puffer_->PuffDeflate(&bit_reader, &puff_writer, nullptr));
        TEST_AND_RETURN_FALSE(bytes_to_read == bit_reader.Offset());
        TEST_AND_RETURN_FALSE(cur_puff_->length == puff_writer.Size());
      } else {
        // Just seek to proper location.
        TEST_AND_RETURN_FALSE(stream_->Seek(start_byte + bytes_to_read));
      }
      // Copy from puff buffer to output if needed.
      auto bytes_to_copy =
          std::min(length - bytes_read, cur_puff_->length - skip_bytes_);
      if (!puff_directly_into_buffer) {
        memcpy(bytes + bytes_read, puff_buffer_->data() + skip_bytes_,
               bytes_to_copy);
      }

      skip_bytes_ += bytes_to_copy;
      bytes_read += bytes_to_copy;

      // Move to next puff.
      if (puff_pos_ + skip_bytes_ == cur_puff_->offset + cur_puff_->length) {
        puff_pos_ += skip_bytes_;
        skip_bytes_ = 0;
        deflate_bit_pos_ = cur_deflate_->offset + cur_deflate_->length;
        cur_puff_++;
        cur_deflate_++;
        if (cur_puff_ == puffs_.end()) {
          break;
        }
      }
    }
  }

  TEST_AND_RETURN_FALSE(bytes_read == length);
  return true;
}

bool PuffinStream::Write(const void* buffer, size_t count) {
  TEST_AND_RETURN_FALSE(!closed_);
  TEST_AND_RETURN_FALSE(!is_for_puff_);
  auto bytes = static_cast<const uint8_t*>(buffer);
  uint64_t length = count;
  uint64_t bytes_wrote = 0;
  while (bytes_wrote < length) {
    if (deflate_bit_pos_ < (cur_deflate_->offset & ~7ull)) {
      // Between two puffs or before the first puff. We know that we are
      // starting from the byte boundary because we have already processed the
      // non-deflate bits of the last byte of the last deflate. Here we don't
      // process any byte that has deflate bit.
      TEST_AND_RETURN_FALSE((deflate_bit_pos_ & 7) == 0);
      auto copy_len =
          std::min((cur_deflate_->offset / 8) - (deflate_bit_pos_ / 8),
                   length - bytes_wrote);
      TEST_AND_RETURN_FALSE(stream_->Write(bytes + bytes_wrote, copy_len));
      bytes_wrote += copy_len;
      puff_pos_ += copy_len;
      deflate_bit_pos_ += copy_len * 8;
    } else {
      // We are in a puff. We have to buffer incoming bytes until we reach the
      // size of the current puff so we can huff :). If the last bit of the
      // current deflate does not end in a byte boundary, then we have to read
      // one more byte to fill up the last byte of the deflate stream before
      // doing anything else.

      // |deflate_bit_pos_| now should be in the same byte as
      // |cur_deflate->offset|.
      if (deflate_bit_pos_ < cur_deflate_->offset) {
        last_byte_ |= bytes[bytes_wrote++] << (deflate_bit_pos_ & 7);
        skip_bytes_ = 0;
        deflate_bit_pos_ = cur_deflate_->offset;
        puff_pos_++;
        TEST_AND_RETURN_FALSE(puff_pos_ == cur_puff_->offset);
      }

      auto copy_len = std::min(length - bytes_wrote,
                               cur_puff_->length + extra_byte_ - skip_bytes_);
      TEST_AND_RETURN_FALSE(puff_buffer_->size() >= skip_bytes_ + copy_len);
      memcpy(puff_buffer_->data() + skip_bytes_, bytes + bytes_wrote, copy_len);
      skip_bytes_ += copy_len;
      bytes_wrote += copy_len;

      if (skip_bytes_ == cur_puff_->length + extra_byte_) {
        // |puff_buffer_| is full, now huff into the |deflate_buffer_|.
        auto start_byte = cur_deflate_->offset / 8;
        auto end_byte = (cur_deflate_->offset + cur_deflate_->length + 7) / 8;
        auto bytes_to_write = end_byte - start_byte;

        deflate_buffer_->resize(bytes_to_write);
        BufferBitWriter bit_writer(deflate_buffer_->data(), bytes_to_write);
        BufferPuffReader puff_reader(puff_buffer_->data(), cur_puff_->length);

        // Write last byte if it has any.
        TEST_AND_RETURN_FALSE(
            bit_writer.WriteBits(cur_deflate_->offset & 7, last_byte_));
        last_byte_ = 0;

        TEST_AND_RETURN_FALSE(huffer_->HuffDeflate(&puff_reader, &bit_writer));
        TEST_AND_RETURN_FALSE(bit_writer.Size() == bytes_to_write);
        TEST_AND_RETURN_FALSE(puff_reader.BytesLeft() == 0);

        deflate_bit_pos_ = cur_deflate_->offset + cur_deflate_->length;
        if (extra_byte_ == 1) {
          deflate_buffer_->data()[bytes_to_write - 1] |=
              puff_buffer_->data()[cur_puff_->length] << (deflate_bit_pos_ & 7);
          deflate_bit_pos_ = (deflate_bit_pos_ + 7) & ~7ull;
        } else if ((deflate_bit_pos_ & 7) != 0) {
          // This happens if current and next deflate finish and end on the same
          // byte, then we cannot write into output until we have huffed the
          // next puff buffer, so untill then we cache it into |last_byte_| and
          // we won't write it out.
          last_byte_ = deflate_buffer_->data()[bytes_to_write - 1];
          bytes_to_write--;
        }

        // Write |deflate_buffer_| into output.
        TEST_AND_RETURN_FALSE(
            stream_->Write(deflate_buffer_->data(), bytes_to_write));

        // Move to the next deflate/puff.
        puff_pos_ += skip_bytes_;
        skip_bytes_ = 0;
        cur_puff_++;
        cur_deflate_++;
        if (cur_puff_ == puffs_.end()) {
          break;
        }
        // Find if need an extra byte to cache at the end.
        TEST_AND_RETURN_FALSE(SetExtraByte());
      }
    }
  }

  TEST_AND_RETURN_FALSE(bytes_wrote == length);
  return true;
}

bool PuffinStream::SetExtraByte() {
  TEST_AND_RETURN_FALSE(cur_deflate_ != deflates_.end());
  if ((cur_deflate_ + 1) == deflates_.end()) {
    extra_byte_ = 0;
    return true;
  }
  uint64_t end_bit = cur_deflate_->offset + cur_deflate_->length;
  if ((end_bit & 7) && ((end_bit + 7) & ~7ull) <= (cur_deflate_ + 1)->offset) {
    extra_byte_ = 1;
  } else {
    extra_byte_ = 0;
  }
  return true;
}

bool PuffinStream::GetPuffCache(int puff_id,
                                uint64_t puff_size,
                                shared_ptr<Buffer>* buffer) {
  bool found = false;
  // Search for it.
  std::pair<int, shared_ptr<Buffer>> cache;
  // TODO(*): Find a faster way of doing this? Maybe change the data structure
  // that supports faster search.
  for (auto iter = caches_.begin(); iter != caches_.end(); ++iter) {
    if (iter->first == puff_id) {
      cache = std::move(*iter);
      found = true;
      // Remove it so later we can add it to the begining of the list.
      caches_.erase(iter);
      break;
    }
  }

  // If not found, either create one or get one from the list.
  if (!found) {
    // If |caches_| were full, remove last ones in the list (least used), until
    // we have enough space for the new cache.
    while (!caches_.empty() && cur_cache_size_ + puff_size > max_cache_size_) {
      cache = std::move(caches_.back());
      caches_.pop_back();  // Remove it from the list.
      cur_cache_size_ -= cache.second->capacity();
    }
    // If we have not populated the cache yet, create one.
    if (!cache.second) {
      cache.second = std::make_shared<Buffer>(puff_size);
    }
    cache.second->resize(puff_size);

    constexpr uint64_t kMaxSizeDifference = 20 * 1024;
    if (puff_size + kMaxSizeDifference < cache.second->capacity()) {
      cache.second->shrink_to_fit();
    }

    cur_cache_size_ += cache.second->capacity();
    cache.first = puff_id;
  }

  *buffer = cache.second;
  // By now we have either removed a cache or created new one. Now we have to
  // insert it in the front of the list so it becomes the most recently used
  // one.
  caches_.push_front(std::move(cache));
  return found;
}

}  // namespace puffin
