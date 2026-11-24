/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <7zCrc.h>
#include <Xz.h>
#include <XzCrc64.h>

#include <unwindstack/Log.h>

#include "MemoryXz.h"

namespace unwindstack {

// Statistics (used only for optional debug log messages).
static constexpr bool kLogMemoryXzUsage = false;
std::atomic_size_t MemoryXz::total_used_ = 0;
std::atomic_size_t MemoryXz::total_size_ = 0;
std::atomic_size_t MemoryXz::total_open_ = 0;

MemoryXz::MemoryXz(Memory* memory, uint64_t addr, uint64_t size, const std::string& name)
    : compressed_memory_(memory), compressed_addr_(addr), compressed_size_(size), name_(name) {
  total_open_ += 1;
}

bool MemoryXz::Init() {
  static std::once_flag crc_initialized;
  std::call_once(crc_initialized, []() {
    CrcGenerateTable();
    Crc64GenerateTable();
  });
  if (compressed_size_ >= kMaxCompressedSize) {
    return false;
  }
  if (!ReadBlocks()) {
    return false;
  }

  // All blocks (except the last one) must have the same power-of-2 size.
  if (blocks_.size() > 1) {
    size_t block_size_log2 = __builtin_ctz(blocks_.front().decompressed_size);
    auto correct_size = [=](XzBlock& b) { return b.decompressed_size == (1 << block_size_log2); };
    if (std::all_of(blocks_.begin(), std::prev(blocks_.end()), correct_size) &&
        blocks_.back().decompressed_size <= (1 << block_size_log2)) {
      block_size_log2_ = block_size_log2;
    } else {
      // Inconsistent block-sizes.  Decompress and merge everything now.
      std::unique_ptr<uint8_t[]> data(new uint8_t[size_]);
      size_t offset = 0;
      for (XzBlock& block : blocks_) {
        if (!Decompress(&block)) {
          return false;
        }
        memcpy(data.get() + offset, block.decompressed_data.get(), block.decompressed_size);
        offset += block.decompressed_size;
      }
      blocks_.clear();
      blocks_.push_back(XzBlock{
          .decompressed_data = std::move(data),
          .decompressed_size = size_,
      });
      block_size_log2_ = 31;  // Because 32 bits is too big (shift right by 32 is not allowed).
    }
  }

  return true;
}

MemoryXz::~MemoryXz() {
  total_used_ -= used_;
  total_size_ -= size_;
  total_open_ -= 1;
}

size_t MemoryXz::Read(uint64_t addr, void* buffer, size_t size) {
  if (addr >= size_) {
    return 0;  // Read past the end.
  }
  uint8_t* dst = reinterpret_cast<uint8_t*>(buffer);  // Position in the output buffer.
  for (size_t i = addr >> block_size_log2_; i < blocks_.size(); i++) {
    XzBlock* block = &blocks_[i];
    if (block->decompressed_data == nullptr) {
      if (!Decompress(block)) {
        break;
      }
    }
    size_t offset = (addr - (i << block_size_log2_));  // Start inside the block.
    size_t copy_bytes = std::min<size_t>(size, block->decompressed_size - offset);
    memcpy(dst, block->decompressed_data.get() + offset, copy_bytes);
    dst += copy_bytes;
    addr += copy_bytes;
    size -= copy_bytes;
    if (size == 0) {
      break;
    }
  }
  return dst - reinterpret_cast<uint8_t*>(buffer);
}

bool MemoryXz::ReadBlocks() {
  static ISzAlloc alloc;
  alloc.Alloc = [](ISzAllocPtr, size_t size) { return malloc(size); };
  alloc.Free = [](ISzAllocPtr, void* ptr) { return free(ptr); };

  // Read the compressed data, so we can quickly scan through the headers.
  std::unique_ptr<uint8_t[]> compressed_data(new (std::nothrow) uint8_t[compressed_size_]);
  if (compressed_data.get() == nullptr) {
    return false;
  }
  if (!compressed_memory_->ReadFully(compressed_addr_, compressed_data.get(), compressed_size_)) {
    return false;
  }

  // Implement the required interface for communication
  // (written in C so we can not use virtual methods or member functions).
  struct XzLookInStream : public ILookInStream, public ICompressProgress {
    static SRes LookImpl(const ILookInStream* p, const void** buf, size_t* size) {
      auto* ctx = reinterpret_cast<const XzLookInStream*>(p);
      *buf = ctx->data + ctx->offset;
      *size = std::min(*size, ctx->size - ctx->offset);
      return SZ_OK;
    }
    static SRes SkipImpl(const ILookInStream* p, size_t len) {
      auto* ctx = reinterpret_cast<XzLookInStream*>(const_cast<ILookInStream*>(p));
      ctx->offset += len;
      return SZ_OK;
    }
    static SRes ReadImpl(const ILookInStream* p, void* buf, size_t* size) {
      auto* ctx = reinterpret_cast<const XzLookInStream*>(p);
      *size = std::min(*size, ctx->size - ctx->offset);
      memcpy(buf, ctx->data + ctx->offset, *size);
      return SZ_OK;
    }
    static SRes SeekImpl(const ILookInStream* p, Int64* pos, ESzSeek origin) {
      auto* ctx = reinterpret_cast<XzLookInStream*>(const_cast<ILookInStream*>(p));
      switch (origin) {
        case SZ_SEEK_SET:
          ctx->offset = *pos;
          break;
        case SZ_SEEK_CUR:
          ctx->offset += *pos;
          break;
        case SZ_SEEK_END:
          ctx->offset = ctx->size + *pos;
          break;
      }
      *pos = ctx->offset;
      return SZ_OK;
    }
    static SRes ProgressImpl(const ICompressProgress*, UInt64, UInt64) { return SZ_OK; }
    size_t offset;
    uint8_t* data;
    size_t size;
  };
  XzLookInStream callbacks;
  callbacks.Look = &XzLookInStream::LookImpl;
  callbacks.Skip = &XzLookInStream::SkipImpl;
  callbacks.Read = &XzLookInStream::ReadImpl;
  callbacks.Seek = &XzLookInStream::SeekImpl;
  callbacks.Progress = &XzLookInStream::ProgressImpl;
  callbacks.offset = 0;
  callbacks.data = compressed_data.get();
  callbacks.size = compressed_size_;

  // Iterate over the internal XZ blocks without decompressing them.
  CXzs xzs;
  Xzs_Construct(&xzs);
  Int64 end_offset = compressed_size_;
  if (Xzs_ReadBackward(&xzs, &callbacks, &end_offset, &callbacks, &alloc) == SZ_OK) {
    blocks_.reserve(Xzs_GetNumBlocks(&xzs));
    size_t dst_offset = 0;
    for (int s = xzs.num - 1; s >= 0; s--) {
      const CXzStream& stream = xzs.streams[s];
      size_t src_offset = stream.startOffset + XZ_STREAM_HEADER_SIZE;
      for (size_t b = 0; b < stream.numBlocks; b++) {
        const CXzBlockSizes& block = stream.blocks[b];
        blocks_.push_back(XzBlock{
            .decompressed_data = nullptr,  // Lazy allocation and decompression.
            .decompressed_size = static_cast<uint32_t>(block.unpackSize),
            .compressed_offset = static_cast<uint32_t>(src_offset),
            .compressed_size = static_cast<uint32_t>((block.totalSize + 3) & ~3u),
            .stream_flags = stream.flags,
        });
        dst_offset += blocks_.back().decompressed_size;
        src_offset += blocks_.back().compressed_size;
      }
    }
    size_ = dst_offset;
    total_size_ += dst_offset;
  }
  Xzs_Free(&xzs, &alloc);
  return !blocks_.empty();
}

bool MemoryXz::Decompress(XzBlock* block) {
  static ISzAlloc alloc;
  alloc.Alloc = [](ISzAllocPtr, size_t size) { return malloc(size); };
  alloc.Free = [](ISzAllocPtr, void* ptr) { return free(ptr); };

  // Read the compressed data for this block.
  std::unique_ptr<uint8_t[]> compressed_data(new (std::nothrow) uint8_t[block->compressed_size]);
  if (compressed_data.get() == nullptr) {
    return false;
  }
  if (!compressed_memory_->ReadFully(compressed_addr_ + block->compressed_offset,
                                     compressed_data.get(), block->compressed_size)) {
    return false;
  }

  // Allocate decompressed memory.
  std::unique_ptr<uint8_t[]> decompressed_data(new uint8_t[block->decompressed_size]);
  if (decompressed_data == nullptr) {
    return false;
  }

  // Decompress.
  CXzUnpacker state{};
  XzUnpacker_Construct(&state, &alloc);
  state.streamFlags = block->stream_flags;
  XzUnpacker_PrepareToRandomBlockDecoding(&state);
  size_t decompressed_size = block->decompressed_size;
  size_t compressed_size = block->compressed_size;
  ECoderStatus status;
  XzUnpacker_SetOutBuf(&state, decompressed_data.get(), decompressed_size);
  int return_val =
      XzUnpacker_Code(&state, /*decompressed_data=*/nullptr, &decompressed_size,
                      compressed_data.get(), &compressed_size, true, CODER_FINISH_END, &status);
  XzUnpacker_Free(&state);
  if (return_val != SZ_OK || status != CODER_STATUS_FINISHED_WITH_MARK) {
    Log::Error("Cannot decompress \"%s\"", name_.c_str());
    return false;
  }

  used_ += block->decompressed_size;
  total_used_ += block->decompressed_size;
  if (kLogMemoryXzUsage) {
    Log::Info("decompressed memory: %zi%% of %ziKB (%zi files), %i%% of %iKB (%s)",
              100 * total_used_ / total_size_, total_size_ / 1024, total_open_.load(),
              100 * used_ / size_, size_ / 1024, name_.c_str());
  }

  block->decompressed_data = std::move(decompressed_data);
  return true;
}

}  // namespace unwindstack
