// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_EXTENT_STREAM_H_
#define SRC_EXTENT_STREAM_H_

#include <memory>
#include <vector>

#include "puffin/src/include/puffin/common.h"
#include "puffin/src/include/puffin/stream.h"

namespace puffin {

// A stream object that allows reading and writing into disk extents. This is
// only used in main.cc for puffin binary to allow puffpatch on a actual rootfs
// and kernel images.
class ExtentStream : public StreamInterface {
 public:
  // Creates a stream only for writing.
  static UniqueStreamPtr CreateForWrite(UniqueStreamPtr stream,
                                        const std::vector<ByteExtent>& extents);
  // Creates a stream only for reading.
  static UniqueStreamPtr CreateForRead(UniqueStreamPtr stream,
                                       const std::vector<ByteExtent>& extents);
  ~ExtentStream() override = default;

  bool GetSize(uint64_t* size) const override;
  bool GetOffset(uint64_t* offset) const override;
  bool Seek(uint64_t offset) override;
  bool Read(void* buffer, size_t length) override;
  bool Write(const void* buffer, size_t length) override;
  bool Close() override;

 private:
  ExtentStream(UniqueStreamPtr stream,
               const std::vector<ByteExtent>& extents,
               bool is_for_write);

  // Since both read and write operations are very similar in this class, this
  // function acts as a common operation that does both write and read based on
  // the nullability of |read_buffer| or |write_buffer|.
  bool DoReadOrWrite(void* read_buffer,
                     const void* write_buffer,
                     size_t length);

  // The underlying stream to read from and write into.
  UniqueStreamPtr stream_;

  std::vector<ByteExtent> extents_;

  // The current |ByteExtent| that is being read from or write into.
  std::vector<ByteExtent>::iterator cur_extent_;

  // The current offset in the current |ByteExtent| |cur_extent_|.
  uint64_t cur_extent_offset_;

  // |True| if the stream is write only. |False| if the stream is read only.
  bool is_for_write_;

  // The size of the stream. It is actually the cumulative size of all the bytes
  // in |extents_|.
  uint64_t size_;

  // The current offset.
  uint64_t offset_;

  // Used for proper and faster seeking.
  std::vector<uint64_t> extents_upper_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ExtentStream);
};

}  // namespace puffin

#endif  // SRC_EXTENT_STREAM_H_
