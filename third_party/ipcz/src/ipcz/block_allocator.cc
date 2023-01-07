// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/block_allocator.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <thread>

#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/safe_math.h"

namespace ipcz {

namespace {

constexpr int16_t kFrontBlockIndex = 0;

// Helper for legibility of relative/absolute index conversions.
struct ForBaseIndex {
  // Constructs a helper to compute absolute or relative indexes based around
  // the successor to the block at `index`. See documentation on BlockHeader's
  // `next` field.
  explicit constexpr ForBaseIndex(int16_t index) : index_(index) {}

  // Returns an index equivalent to `absolute_index`, but relative to the
  // successor of the block at `index_`.
  constexpr int16_t GetRelativeFromAbsoluteIndex(int16_t absolute_index) const {
    // NOTE: We intentionally ignore overflow. Absolute indices are always
    // validated before use anyway, and it would be redundant in some cases to
    // validate these inputs.
    return absolute_index - index_ - 1;
  }

  // Returns an absolute index which is equivalent to `relative_index` if taken
  // as relative to the successor of the block at `index_`.
  constexpr int16_t GetAbsoluteFromRelativeIndex(int16_t relative_index) const {
    // NOTE: We intentionally ignore overflow. The returned index is only stored
    // and maybe eventually used to reconstruct an absolute index. Absolute
    // indices are always validated before use, and it would be redundant in
    // some cases to validate these inputs.
    return index_ + relative_index + 1;
  }

 private:
  const int16_t index_;
};

}  // namespace

BlockAllocator::BlockAllocator() = default;
BlockAllocator::BlockAllocator(absl::Span<uint8_t> region, uint32_t block_size)
    : region_(region), block_size_(block_size) {
  // Require 8-byte alignment of the region and of block sizes, to ensure that
  // each BlockHeader is itself 8-byte aligned. Also a non-zero block size is
  // obviously a requirement.
  ABSL_ASSERT((reinterpret_cast<uintptr_t>(region_.data()) & 7) == 0);
  ABSL_ASSERT(block_size > 0);
  ABSL_ASSERT((block_size & 7) == 0);

  // BlockHeader uses a signed 16-bit index to reference other blocks, and block
  // 0 must be able to reference any block; so the total number of blocks must
  // not exceed the max value of an int16_t.
  num_blocks_ = checked_cast<int16_t>(region.size() / block_size);
  ABSL_ASSERT(num_blocks_ > 0);
}

BlockAllocator::BlockAllocator(const BlockAllocator&) = default;

BlockAllocator& BlockAllocator::operator=(const BlockAllocator&) = default;

BlockAllocator::~BlockAllocator() = default;

void BlockAllocator::InitializeRegion() const {
  // By zeroing the entire region, every block effectively points to its
  // immediate successor as the next free block. See comments on the `next`
  // field if BlockHeader.
  memset(region_.data(), 0, region_.size());

  // Ensure that the last block points back to the unallocable first block,
  // indicating the end of the free-list.
  free_block_at(last_block_index()).SetNextFreeBlock(kFrontBlockIndex);
}

void* BlockAllocator::Allocate() const {
  BlockHeader front =
      block_header_at(kFrontBlockIndex).load(std::memory_order_relaxed);
  for (;;) {
    const int16_t first_free_block_index =
        ForBaseIndex(kFrontBlockIndex).GetAbsoluteFromRelativeIndex(front.next);
    if (first_free_block_index == kFrontBlockIndex ||
        !is_index_valid(first_free_block_index)) {
      // Note that the front block can never be allocated, so if that's where
      // the head of the free-list points then the free-list is empty. If it
      // otherwise points to an out-of-range index, the allocator is in an
      // invalid state. In either case, we fail the allocation.
      return nullptr;
    }

    // Extract the index of the *next* free block from the header of the first.
    FreeBlock first_free_block = free_block_at(first_free_block_index);

    // SUBTLE: This stack requires a race suppression when running with TSan.
    //
    // Multiple threads may race to load the front block header at roughly the
    // same time and end up with the same `first_free_block`. Only one thread
    // will successfully claim the block via TryUpdateFrontHeader() below, but
    // all of them will still load the maybe-free block's header here.
    //
    // Meanwhile, the winning thread may at any point begin using the block and
    // e.g. issuing non-atomic writes to its memory. This can cause TSan to blow
    // up, as it will detect data races between the winner's writes, and the
    // losing thread(s) issuing this atomic load. The races are legitimate but
    // harmless, because the result of this load is unused unless the subsequent
    // TryUpdateFrontHeader() succeeds, which it won't.
    BlockHeader first_free_block_header =
        first_free_block.header().load(std::memory_order_acquire);
    const int16_t next_free_block_index =
        ForBaseIndex(first_free_block_index)
            .GetAbsoluteFromRelativeIndex(first_free_block_header.next);
    if (!is_index_valid(next_free_block_index)) {
      // Invalid block header, so we cannot proceed.
      return nullptr;
    }

    if (TryUpdateFrontHeader(front, next_free_block_index)) {
      // If we successfully update the front block's header to point at the
      // second free block, we have effectively allocated the first free block
      // by removing it from the free-list. This means we're done and the
      // allocator will not touch the contents of this block (including header)
      // until it's freed.
      return first_free_block.address();
    }

    // Another thread must have modified the front block header since we fetched
    // it above. `front` now has a newly updated copy, so we loop around again
    // to retry allocation.
  }
}

bool BlockAllocator::Free(void* ptr) const {
  // Derive a block index from the given address, relative to the start of this
  // allocator's managed region.
  const int16_t new_free_index =
      (reinterpret_cast<uint8_t*>(ptr) - region_.data()) / block_size_;
  if (new_free_index == kFrontBlockIndex || !is_index_valid(new_free_index)) {
    // The first block cannot be freed, and obviously neither can any block out
    // of range for this allocator.
    return false;
  }

  FreeBlock free_block = free_block_at(new_free_index);
  BlockHeader front =
      block_header_at(kFrontBlockIndex).load(std::memory_order_relaxed);
  do {
    const int16_t first_free_index =
        ForBaseIndex(kFrontBlockIndex).GetAbsoluteFromRelativeIndex(front.next);
    if (!is_index_valid(first_free_index)) {
      // The front block header is in an invalid state, so we cannot proceed.
      return false;
    }

    // The application calling Free() implies that it's done using the block.
    // The allocator is therefore free to overwrite the contents with a new
    // BlockHeader. Write a header which points to the current head of the
    // free-list.
    free_block.SetNextFreeBlock(first_free_index);

    // And now try to update the front block so that this newly freed block
    // becomes the new head of the free-list. Upon success, the block is
    // effectively freed. Upon failure, `front` will have an updated copy of
    // the front block header, so we can loop around and try to insert the freed
    // block again.
  } while (!TryUpdateFrontHeader(front, new_free_index));

  return true;
}

bool BlockAllocator::TryUpdateFrontHeader(BlockHeader& last_known_header,
                                          int16_t first_free_block) const {
  // Note that `version` overflow is acceptable here. The version is only used
  // as a tag to protect against the ABA problem. Because all alloc and free
  // operations are completed via an atomic exchange of the front block header,
  // and because they must always increment the header version at the same time,
  // we effectively avoid one operation trampling the result of another.
  const uint16_t new_version = last_known_header.version + 1;
  const int16_t relative_next =
      ForBaseIndex(kFrontBlockIndex)
          .GetRelativeFromAbsoluteIndex(first_free_block);

  // A weak compare/exchange is used since in practice this will always be
  // called within a tight retry loop.
  return block_header_at(kFrontBlockIndex)
      .compare_exchange_weak(
          last_known_header, {.version = new_version, .next = relative_next},
          std::memory_order_release, std::memory_order_relaxed);
}

BlockAllocator::FreeBlock::FreeBlock(int16_t index, AtomicBlockHeader& header)
    : index_(index), header_(header) {
  ABSL_ASSERT(index > 0);
}

void BlockAllocator::FreeBlock::SetNextFreeBlock(int16_t next_free_block) {
  const int16_t relative_next =
      ForBaseIndex(index_).GetRelativeFromAbsoluteIndex(next_free_block);
  header_.store({.version = 0, .next = relative_next},
                std::memory_order_release);
}

}  // namespace ipcz
